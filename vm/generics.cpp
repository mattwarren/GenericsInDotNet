// File: generics.cpp
//  
// Copyright (c) 1998 Microsoft Corporation.  All Rights Reserved.
//
// Helper functions for generics prototype
//
#include "common.h"
#include "method.hpp"
#include "field.h"
#include "eeconfig.h"
#include "prettyprintsig.h"
#include "wsperf.h"
#include "generics.h"

// Create a new instantiation of a generic type, by:              
// EITHER creating a completely new, specialized class (mt + EEClass)   
// OR copying the method table representation-compatible class.
//
TypeHandle ClassLoader::NewInstantiation(TypeHandle genericType, TypeHandle* inst, Pending *pending)
{
    _ASSERTE(inst != NULL);
    _ASSERTE(!genericType.IsNull());
    
  // The generic type must be uninstantiated
  _ASSERTE(genericType.GetClass()->IsGenericTypeDefinition());

  Module *pModule = genericType.GetModule();
  EEClass *pGenericClass = genericType.GetClass();

  // Number of type parameters must be non-zero
  DWORD ntypars = genericType.GetNumGenericArgs();
  _ASSERTE(ntypars > 0);

#ifdef _DEBUG
  char instbuff[1000];
  Generics::PrettyInstantiation(instbuff, 1000, ntypars, inst);
  LOG((LF_CLASSLOADER, LL_INFO1000, "GENERICS: New instantiation requested: type %s at inst %s\n", 
    pGenericClass->m_szDebugClassName, instbuff));
#endif

    // Canonicalize the type arguments.  Set nonCanonical if any were not
    // in canonical form, which implies we will have to create a new
    // instantiation by duplicating a canonical method table.
    TypeHandle *repInst = (TypeHandle*) _alloca(ntypars * sizeof(TypeHandle));
    BOOL canonical = true;
    for (DWORD i = 0; i < ntypars; i++)
    {
        TypeHandle th = inst[i];
        TypeHandle repth = th.GetCanonicalFormAsGenericArgument();
        canonical = canonical && (repth == th);
        repInst[i] = repth;
    }
    
    MethodTable *pMT = NULL;
    
    if (canonical)
    {
        // If we're loading the canonical one then do a fresh class load, first of the
        // generic type (it might already be loaded) and then instantiate it.
        EEClass* pClass = NULL;
        HRESULT hr = 
            pModule->GetClassLoader()->LoadTypeHandleFromToken(pModule, pGenericClass->GetCl(), &pClass, NULL, pending, 
                                                               genericType, inst);
        if (SUCCEEDED(hr))
        {
            pMT = pClass->GetMethodTable();
            
#ifdef _DEBUG
            BOOL shared = pClass->IsSharedByGenericInstantiations();
            TypeHandle t = TypeHandle(pMT);
            
            char buff[200];
            t.GetName(buff, 200);
            LOG((LF_CLASSLOADER, LL_INFO1000, "GENERICS: Created %s %s (%x), EEClass %s\n", 
                 (shared ? "shared instantiation" : "fully specialised instantiation"),
                 buff, t, pMT->GetClass()->m_szDebugClassName));
            
            if (shared) 
            {
                _ASSERTE(pClass->GetDictionaryLayout() != NULL);
                LOG((LF_CLASSLOADER, LL_INFO1000, "GENERICS: Created dictionary layout table with %d slots for sharable class %s\n", 
                     pClass->GetDictionaryLayout()->numSlots,
                     pMT->m_szDebugClassName));
            }
#endif
        }
    }
    else 
    {
        // If nonCanonical we need to go and load the canonical one
        TypeHandle canonType = ClassLoader::LoadGenericInstantiation(genericType, repInst, ntypars);
        
        _ASSERTE(!canonType.IsNull());
        if (canonType.IsNull())
            return TypeHandle();
        
        // Now share its EEClass and fabricate a method table
        MethodTable* pOldMT = canonType.AsMethodTable();
        
        EEClass *pClass = pOldMT->GetClass();
        
        // We only need true vtable entries as the rest can be found in the representative method table
        DWORD cbSlots = pClass->GetNumVtableSlots();
        
        // The number of bytes used for GC info
        size_t cbGC = pClass->GetNumGCPointerSeries() ? ((CGCDesc*) pOldMT)->GetSize() : 0;
        
        // Bytes are required for the vtable itself
        size_t cbMT = cbGC + sizeof(MethodTable) + (cbSlots * sizeof(SLOT));
        
        // Also we need space for the duplicated interface map
        DWORD dwNumInterfaces = pOldMT->GetNumInterfaces();
        DWORD cbIMap = dwNumInterfaces * sizeof(InterfaceInfo_t);
        BYTE* pOldIMapMem = (BYTE*) pOldMT->GetInterfaceMap();
#ifdef _DEBUG
        cbIMap += sizeof(InterfaceInfo_t);
        if (pOldIMapMem)
            pOldIMapMem -= sizeof(InterfaceInfo_t);
#endif


    // And finally, we need space for dictionaries...
    DWORD cbPerInst = pClass->GetNumDicts() * sizeof(TypeHandle*);

    // ...and the dictionary for this class
    DWORD cbDict = sizeof(TypeHandle) * ntypars;

    if (pClass->GetDictionaryLayout())
      cbDict += sizeof(void*) * (1+pClass->GetDictionaryLayout()->numSlots);

    // Allocate from the high frequence heap
    WS_PERF_SET_HEAP(HIGH_FREQ_HEAP);
    BYTE* pMemory = (BYTE *) GetHighFrequencyHeap()->AllocMem(cbMT + cbIMap + cbPerInst + cbDict);
    if (pMemory == NULL) 
      return TypeHandle();

    // Head of MethodTable memory 
    pMT = (MethodTable*) (pMemory + cbGC);

    // First do a shallow copy of the entire structure, 
    // except for the interface map and per-inst info (they might have moved)
    memcpy(pMemory, (BYTE*) pOldMT - cbGC, cbMT);

    // Copy interface map across
#ifdef _DEBUG
    if (!pOldIMapMem)
    {
        InterfaceInfo_t *_pIMap_ = (InterfaceInfo_t*)(pMemory + cbMT);
        _pIMap_->m_wStartSlot = 0xCDCD;
        _pIMap_->m_wFlags = 0xCDCD;
        _pIMap_->m_pMethodTable = (MethodTable*)(size_t)INVALID_POINTER_CD;
    }
    else
#endif
    memcpy(pMemory + cbMT, pOldIMapMem, cbIMap);

    // Number of slots only includes vtable slots
    pMT->m_cbSlots = cbSlots;

    // Fill in interface map pointer    
    pMT->m_pIMap = (InterfaceInfo_t *)(pMemory + cbMT);

    if (pOldMT->HasDynamicInterfaceMap()) 
      pMT->m_pIMap = (InterfaceInfo_t*) ((BYTE*) pMT->m_pIMap + sizeof(DWORD));
      
#ifdef _DEBUG
    pMT->m_pIMap++;
#endif

    // Fill in per-inst map pointer
    pMT->m_pPerInstInfo = (TypeHandle**) (pMemory + cbMT + cbIMap);
    
    pMT->m_pPerInstInfo[pClass->GetNumDicts()-1] = (TypeHandle*) (pMemory + cbMT + cbIMap + cbPerInst);    
    TypeHandle *pInst = pMT->GetInstantiation();
    memset(pInst, 0, cbPerInst);
    memcpy(pInst, inst, ntypars * sizeof(TypeHandle));

    // Load exact parent and interface info
    pClass->LoadInstantiatedInfo(pMT, pending, NULL);
    
    // Exposed type is wrong so set it to null
    pMT->m_ExposedClassObject = NULL;

        // Name for debugging
#ifdef _DEBUG
        pMT->m_szDebugClassName = (char*)GetHighFrequencyHeap()->AllocMem(strlen(pGenericClass->m_szDebugClassName) + strlen(instbuff) + 1);
        _ASSERTE(pMT->m_szDebugClassName);   
        strcpy(pMT->m_szDebugClassName, pGenericClass->m_szDebugClassName);
        strcat(pMT->m_szDebugClassName, instbuff); 
#endif
        
#ifdef _DEBUG
        TypeHandle t = TypeHandle(pMT);
        
        char buff[200];
        t.GetName(buff, 200);
        LOG((LF_CLASSLOADER, LL_INFO1000, "GENERICS: Replicated methodtable for instantiation %s (%x) with EEClass %s\n", buff, t, pMT->GetClass()->m_szDebugClassName));
#endif 
    }
    
    return(TypeHandle(pMT));
}


// Pretty print an instantiation (reflection-style syntax) into a buffer, terminated with null
void Generics::PrettyInstantiation(char *buf, DWORD max, DWORD numTyPars, TypeHandle *inst)
{
  // Assume that the buffer exists
  _ASSERTE(buf != NULL);

  if (max < 5) 
  {
    *buf = 0;
    return;
  }

  char *p = buf;
  if (inst != NULL)
  {
    max -= 2;
    *p++ = '[';
    for (unsigned i = 0; i < numTyPars; i++)
    {
      if (i > 0) 
      {
        if (max <= 1) return;
        *p++ = ','; max--;
      }
      if (inst[i].IsNull())
      {
        *p++ = '?';
        max--;
      }
      else
      {
        int len = inst[i].GetName(p, max);
        max -= len; 
        p += len;
      }
      if (max <= 0) return;
    }    
    *p++ = ']';
  }
  *p = 0;
}  

// Given the exact type owning a method (which might be inherited and could
// be shared between compatible instantiations), return the exact type in
// which this method is "declared" i.e. where its implementation lives
TypeHandle Generics::GetMethodDeclaringType(TypeHandle owner, MethodDesc *pMD, OBJECTREF *pThrowable)
{
  EEClass *pClass = pMD->GetClass();
  if (!pClass->HasInstantiation())
    return TypeHandle(pClass->GetMethodTable());
  else
  { 
    TypeHandle genericType = pClass->GetGenericTypeDefinition();
    _ASSERTE(!genericType.IsNull());
  
    TypeHandle *inst = pMD->GetClassInstantiation(owner);
    _ASSERTE(inst != NULL);

    return ClassLoader::LoadGenericInstantiation(genericType, inst, pClass->GetNumGenericArgs(), pThrowable);
  }
}

// Return the exact type handle for a 
TypeHandle Generics::GetFrameOwner(CrawlFrame* pCf)
{
  MethodDesc* pFunc = pCf->GetFunction();
  if (!pFunc->GetClass()->IsSharedByGenericInstantiations())
    return TypeHandle(pFunc->GetMethodTable());
  else
  {    
    return TypeHandle();
  }
}

// Given the shared EEClass for an instantiated type, search its dictionary layout
// for a particular (annotated) token.
// If there's no space in the current layout, grow the layout and the actual dictionaries
// of all compatible types.
// Return a sequence of offsets into dictionaries
WORD Generics::FindClassDictionaryToken(EEClass *pClass, unsigned token, WORD *offsets)
{
  DictionaryLayout *dictLayout = pClass->GetDictionaryLayout();
  _ASSERTE(dictLayout != NULL);

  WORD nindirections = 0;

  // First bucket also contains type parameters
  WORD slot = pClass->GetNumGenericArgs();

  DictionaryLayout *lastDictLayout = NULL;

  while (dictLayout)
  {
    for (DWORD i = 0; i < dictLayout->numSlots; i++)
    {
      // We've found it
      if (dictLayout->slots[i] == token)
      {
        offsets[nindirections++] = (slot) * sizeof(void*);
	return nindirections;
      }

      // If we hit an empty slot then there's no more so use it
      else if (dictLayout->slots[i] == mdTokenNil)
      {
        dictLayout->slots[i] = token;
        offsets[nindirections++] = slot * sizeof(void*);
	
#ifdef _DEBUG
        char slotstr[100];
	*slotstr = 0;
        for (DWORD j = 0; j < nindirections; j++)
	  sprintf(slotstr + strlen(slotstr), "%d ", offsets[j]);
        LOG((LF_JIT, LL_INFO1000, "GENERICS: Allocated slot at position %s to token %x in class dictionary for %s\n", slotstr, token, pClass->m_szDebugClassName));
#endif
        return nindirections;
      }
      slot++;
    }

    // Next pointer is stored at end
    if (dictLayout->hasSpillPointer)
    {
      offsets[nindirections++] = slot * sizeof(void*);
      slot = 0;
    }

    lastDictLayout = dictLayout;
    dictLayout = dictLayout->next;
  }


  return static_cast<WORD>(-1);
}

