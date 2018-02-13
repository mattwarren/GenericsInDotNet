// ==++==
// 
//   
//    Copyright (c) 2002 Microsoft Corporation.  All rights reserved.
//   
//    The use and distribution terms for this software are contained in the file
//    named license.txt, which can be found in the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by the
//    terms of this license.
//   
//    This file contains modifications of the base SSCLI software to support generic
//    type definitions and generic methods,  THese modifications are for research
//    purposes.  They do not commit Microsoft to the future support of these or
//    any similar changes to the SSCLI or the .NET product.  -- 31st October, 2002.
//   
//    You must not remove this notice, or any other, from this software.
//   
// 
// ==--==
// File: genmeth.cpp
//
// Most functionality for generic methods is put here
//

#include "common.h"
#include "method.hpp"
#include "field.h"
#include "eeconfig.h"
#include "prettyprintsig.h"
#include "wsperf.h"
#include "perfcounters.h"
#include "crst.h"

//@GENERICS: outline of scheme.
//
// Generic method descriptors are there to cache metadata information only; their prestubs
// should *not* be entered and their IL RVAs should never be replaced by a code pointer.
//
// Method descriptors for *instantiated* generic methods are allocated on demand and are chained
// onto the chunk list in the enclosing type. 
//
// For non-shared instantiations, entering the prestub for such a method descriptor causes the method to 
// be JIT-compiled, specialized to that instantiation.
// For shared instantiations, entering the prestub generates a piece of stub code that passes the
// method descriptor as an extra argument and then jumps to code shared between compatible instantiations.
// This code has its only method descriptor, which we call a *representative* instantiated method descriptor,
// that's also allocated in the linked list of chunks chained off the representative class type.
//
// Example:
// class C<T> { public void m<S>(S x, T y) { ... } }
// 
// Suppose that code sharing is turned on for classes and methods and for reference instantiations.
// Upon compiling calls to C<string>.m<string>, C<string>.m<Type>, C<Type>.m<string> and C<Type>.m<Type>
// we end up with a chain off the representative class C<object> as shown below:
//
// EEClass(C)
//   ...
//   m_pChunks -> chunks for ordinary methods ->
//     chunk(mt = C<string>): md(method = m, inst = <string>) ->
//     chunk(mt = C<string>): md(method = m, inst = <Type>) ->
//     chunk(mt = C<Type>): md(method = m, inst = <string>) ->
//     chunk(mt = C<Type>): md(method = m, inst = <Type>).
//
// When, say, C<Type>.m<string> is first invoked this chain gets extended to give
//
//   m_pChunks -> chunks for ordinary methods ->
//     chunk(mt = C<string>): md(method = m, inst = <string>) ->
//     chunk(mt = C<string>): md(method = m, inst = <Type>) ->
//     chunk(mt = C<Type>): md(method = m, inst = <string>, code="pass me as extra arg. jump to rmd(C<string>.m<string>)") ->
//     chunk(mt = C<Type>): md(method = m, inst = <Type>) ->
//     chunk(mt = C<string>): rmd(method = m, inst = <string>).
// 
// where rmd is a "representative" method descriptor. Then when the prestub for this rmd is entered 
// code is finally JIT-compiled:
//
//
//   m_pChunks -> chunks for ordinary methods ->
//     chunk(mt = C<string>): md(method = m, inst = <string>) ->
//     chunk(mt = C<string>): md(method = m, inst = <Type>) ->
//     chunk(mt = C<Type>): md(method = m, inst = <string>, code="pass me as extra arg. jump to rmd(C<string>.m<string>)") ->
//     chunk(mt = C<Type>): md(method = m, inst = <Type>) ->
//     chunk(mt = C<string>): rmd(method = m, inst = <string>, code=shared code for C<string>.m<string> and compatible insts, expecting extra arg)
//


char* FormatSig(MethodDesc* pMD);


// Count the number of slots in a dictionary layout
static DWORD CountDictionarySlots(DictionaryLayout *pDictLayout)
{
  DWORD numSlots = 0;
  DictionaryLayout *pD = pDictLayout;
  while (pD)
  {
    _ASSERTE(!pD->hasSpillPointer);
    // Last bucket might have empty entries
    if (pD->next == NULL)
    {
      for (DWORD i = 0; i < pD->numSlots; i++)
        if (pD->slots[i] != 0) 
          numSlots++;
    }
    else
      numSlots += pD->numSlots;
    pD = pD->next;
  }
  return numSlots;
}

// Pretty-printing for a method desc
static void PrettyMethodDesc(MethodDesc* pMD, char* buff, DWORD len)
{
    TypeHandle t = TypeHandle(pMD->GetMethodTable());
    mdMethodDef methodDef = pMD->GetMemberDef();
    t.GetName(buff, len);
    DWORD rem = len - strlen(buff);
    if (rem > 2)
    { 
        strcat(buff, "::");
        rem -= 2;
    }
    
    const char *mname = t.GetClass()->GetMDImport()->GetNameOfMethodDef(methodDef);
    DWORD mlen = (DWORD) strlen(mname);
    if (rem > mlen)
    {
        strcat(buff, mname);
        rem -= mlen;
    }
    TypeHandle *inst = pMD->GetMethodInstantiation();
    if (rem > 0 && inst != NULL ) 
    {
        Generics::PrettyInstantiation(buff+strlen(buff), rem, pMD->GetNumGenericMethodArgs(), inst);
    }
    rem = len - strlen(buff);
    if (pMD->IsSharedByGenericInstantiations()) 
    {
        const char *s = 
            pMD->IsSharedByGenericMethodInstantiations() 
            ? (pMD->GetMethodTable()->GetClass()->IsSharedByGenericInstantiations() 
               ? "{class-method-shared}" 
               : "method-shared")
            : "class-shared";
        DWORD slen = (DWORD) strlen(s);
        if (rem > slen) 
        {
            strcat(buff, s);
            rem -= slen;
        }
    }
}



// Given a generic method descriptor and an instantiation, create a new instantiated method
// descriptor and chain it into the list attached to the generic method descriptor
//
// pMT is the owner method table
// pGenericMD is the generic method descriptor (owner must not be instantiated)
// pSharedMD is the corresponding shared instantiated md for stubs
// ntypars/typars is the instantiation
// shared=TRUE if you want a shared instantiated md whose code expects an extra argument
//
// The result is put in ppMD 
//
// Thread safety is guaranteed through 
// - a critical section ensuring that only one thread attempts to instantiate at any one time
// - self-consistency of the linked list of method chunks (pointer from EEClass is updated last of all, atomically)
// - on entering the critical section we check to see if another thread has meanwhile created the md we're seeking

HRESULT InstantiatedMethodDesc::NewInstantiatedMethodDesc(MethodTable *pExactMT, 
                                                          MethodDesc* pGenericMDescInRepMT, 
                                                          InstantiatedMethodDesc* pSharedMDescForStub, 
                                                          InstantiatedMethodDesc** ppMD, 
                                                          DWORD ntypars, 
                                                          TypeHandle *typars, 
                                                          BOOL getSharedNotStub)
{
    _ASSERTE(ppMD != NULL);
    _ASSERTE(pGenericMDescInRepMT->IsGenericMethodDefinition());
    _ASSERTE(ntypars == pGenericMDescInRepMT->GetNumGenericMethodArgs());
    _ASSERTE(typars != NULL);
    
    // All instantiated method descs live off the RepMT for the
    // instantiated class they live in.
    MethodTable *pRepMT = pExactMT->GetCanonicalMethodTable();
    
    _ASSERTE(pGenericMDescInRepMT->GetMethodTable() == pRepMT);
    
    if (getSharedNotStub)
    {
        _ASSERTE(pSharedMDescForStub == NULL);
        _ASSERTE(pExactMT->IsCanonicalMethodTable());
        _ASSERTE(pRepMT == pExactMT);
        _ASSERTE(TypeHandle::CheckInstantiationIsCanonical(ntypars,typars));
        _ASSERTE(pExactMT->GetClass()->IsSharedByGenericInstantiations() || TypeHandle::IsSharableInstantiation(ntypars, typars));
        
    }
    EEClass* pEEClass = pGenericMDescInRepMT->GetClass();
    mdMethodDef methodDef = pGenericMDescInRepMT->GetMemberDef();

  DWORD nslots = 0;

  pEEClass->LockChunks();

  // Check whether another thread beat us to it!
  //<NICE>GENERICS: don't search the whole list again, just the newly-created portion</NICE>
  InstantiatedMethodDesc *pNewMD = FindInstantiatedMethodDesc(pExactMT, pGenericMDescInRepMT, ntypars, typars, getSharedNotStub);
  BOOL alreadyThere = (pNewMD != NULL);
  if (!alreadyThere)
  {
     // Allocate the dictionary/type-instantiation if shared
    if (pSharedMDescForStub)
    {
       DictionaryLayout* pDL = pSharedMDescForStub->GetDictionaryLayout();
       nslots = CountDictionarySlots(pDL);
    }
    TypeHandle *pInstOrPerInstInfo = (TypeHandle *)pEEClass->GetClassLoader()->GetHighFrequencyHeap()->AllocMem(sizeof(TypeHandle) * (ntypars + nslots));

    // Allocate space for the instantiation (and the dictionary if the layout is known).
    if (pInstOrPerInstInfo == NULL) 
       return E_OUTOFMEMORY;
    memcpy(pInstOrPerInstInfo, typars, sizeof(TypeHandle) * ntypars);
    if (nslots)
       memset(pInstOrPerInstInfo + ntypars, 0, sizeof(TypeHandle) * nslots);

    // Create a new singleton chunk for the new instantiated method descriptor
    // Notice that we've passed in the method table pointer; this gets used in some of the subsequent setup methods for method descs
    // but the chunk isn't chained into the chunk list until the very last moment to protect concurrent readers
    MethodDescChunk *pChunk = 
      MethodDescChunk::CreateChunk(pEEClass->GetClassLoader()->GetHighFrequencyHeap(), 1, mcInstantiated, ::GetTokenRange(methodDef), pExactMT);
    if (pChunk == NULL)
      return E_OUTOFMEMORY;

    // Now get the single method descriptor in the new chunk
    pNewMD = (InstantiatedMethodDesc*) pChunk->GetFirstMethodDesc();
    memset(pNewMD, 0, sizeof(InstantiatedMethodDesc));

    // Set the method desc's classification and chunk index.
    pNewMD->SetGenericMethodDesc(pGenericMDescInRepMT);
    pNewMD->SetClassification(mcInstantiated);
    pNewMD->SetChunkIndex(0, mcInstantiated);
  
    emitStubCall(pNewMD, (BYTE*) (ThePreStub()->GetEntryPoint()));
    pNewMD->SetAddrofCode(pNewMD->GetPreStubAddr());

    // Initialize the MD the way it needs to be
    if (pSharedMDescForStub)
    {
      pNewMD->SetupInstantiatingStub(pSharedMDescForStub,pInstOrPerInstInfo);
    }
    else if (getSharedNotStub)
    {
      pNewMD->SetupSharedMethodInstantiation(pInstOrPerInstInfo);
    }
    else 
    {
      pNewMD->SetupUnsharedMethodInstantiation(pInstOrPerInstInfo);
    }
    // Check that whichever field holds the inst. got setup correctly
    _ASSERTE(pNewMD->GetMethodInstantiation() == pInstOrPerInstInfo);

    // Final action is atomic: set the head pointer in the EEClass
    pRepMT->GetClass()->AddChunk(pChunk);
  }

#ifdef _DEBUG
  {
    char buff[300]; // Should be enough for most apps
    TypeHandle *inst = pNewMD->GetMethodInstantiation();
    strcpy(buff, pEEClass->GetMDImport()->GetNameOfMethodDef(methodDef));
    Generics::PrettyInstantiation(buff + strlen(buff), 300U-strlen(buff), ntypars, inst);
    pNewMD->m_pszDebugMethodName = new char[strlen(buff)+1];
    strcpy((char*)pNewMD->m_pszDebugMethodName, buff);
    pNewMD->m_pszDebugClassName  = pExactMT->m_szDebugClassName;
    pNewMD->m_pDebugEEClass      = pExactMT->GetClass();
    pNewMD->m_pDebugMethodTable  = pExactMT;
    pNewMD->m_pszDebugMethodSignature = FormatSig(pNewMD);
    const char* verb = alreadyThere ? "Another thread created" : "Created";
    if (pSharedMDescForStub)
      LOG((LF_CLASSLOADER, LL_INFO1000, "GENERICS: %s instantiating-stub method desc for %s with %d dictionary slots\n",  
      verb, pNewMD->m_pszDebugMethodName, nslots));
    else
      LOG((LF_CLASSLOADER, LL_INFO1000, "GENERICS: %s instantiated method desc for %s\n", verb, 
      pNewMD->m_pszDebugMethodName));
  }
#endif

  pEEClass->UnlockChunks();

  *ppMD = pNewMD;

  _ASSERTE(getSharedNotStub == pNewMD->IsSharedByGenericInstantiations());
  _ASSERTE(pNewMD->HasMethodInstantiation());

  return S_OK;
}

// Given a generic method descriptor, search for a particular instantiation in its chain of instantiations
// Return NULL if not found.
// pGenericMDescInRepMT is the generic method descriptor but
// may belong to the shared representative EEClass of a generic type instance.
InstantiatedMethodDesc* 
InstantiatedMethodDesc::FindInstantiatedMethodDesc(MethodTable *pExactOrRepMT, 
                                                   MethodDesc *pGenericMDescInRepMT, 
                                                   DWORD ntypars, 
                                                   TypeHandle *typars, 
                                                   BOOL getSharedNotStub)
{
  _ASSERTE(pGenericMDescInRepMT->IsGenericMethodDefinition());
  _ASSERTE(ntypars == pGenericMDescInRepMT->GetNumGenericMethodArgs());
  _ASSERTE(typars != NULL);

  if (getSharedNotStub)
  {
    _ASSERTE(pExactOrRepMT->IsCanonicalMethodTable());
    _ASSERTE(TypeHandle::CheckInstantiationIsCanonical(ntypars,typars));
    _ASSERTE(pExactOrRepMT->GetClass()->IsSharedByGenericInstantiations() || 
        TypeHandle::IsSharableInstantiation(ntypars, typars));
    _ASSERTE(pExactOrRepMT->IsCanonicalMethodTable());
  }
    
  MethodDescChunk *pChunk = pGenericMDescInRepMT->GetClass()->GetChunks();

  while (pChunk)
  {
    if ((pChunk->GetKind() & mdcClassification) == mcInstantiated)
    {
      // This assert no longer holds because generic method desc now also have kind mcInstantiated
      // _ASSERTE(pChunk->GetCount() == 1);
      InstantiatedMethodDesc *pInstMD = (InstantiatedMethodDesc*) (pChunk->GetFirstMethodDesc());

      if (pInstMD->GetMemberDef() != pGenericMDescInRepMT->GetMemberDef()
      ||  pInstMD->GetMethodTable() != pExactOrRepMT
      ||  pInstMD->IsSharedByGenericInstantiations() != getSharedNotStub)
        goto NextChunk;

      // We're expecting it to be instantiated
      _ASSERTE(pInstMD->HasMethodInstantiation() || pInstMD->IsGenericMethodDefinition());

      // What's more, we expect its parent to be the generic MD that was passed in
      _ASSERTE(pInstMD->StripMethodInstantiation() == pGenericMDescInRepMT);

      // Now do the comparison
      TypeHandle *inst = pInstMD->GetMethodInstantiation();
      _ASSERTE(inst != NULL);

      for (DWORD i = 0; i < ntypars; i++)
      {
        if (inst[i] != typars[i]) 
          goto NextChunk;
      }

      // We found it!
      return pInstMD;
    }
  NextChunk:
    pChunk = pChunk->GetNextChunk();
  }

  return NULL;
}

// Given a generic method descriptor, find (or create) an instantiated method descriptor at the instantiation typars.
// The generic MD may lie in an instantiated generic class type, i.e. the generic parameters.
InstantiatedMethodDesc* InstantiatedMethodDesc::CreateGenericInstantiation(MethodTable *pMT, MethodDesc* pGenericMDescInRepMT, TypeHandle *typars, BOOL allowInstParam)
{
  _ASSERTE(typars != NULL);
  _ASSERTE(pGenericMDescInRepMT->IsGenericMethodDefinition());

  DWORD ntypars = pGenericMDescInRepMT->GetNumGenericMethodArgs();

  // Are either the generic type arguments or the generic method arguments shared?
  BOOL sharedInst =
    pMT->GetClass()->IsSharedByGenericInstantiations()
    || TypeHandle::IsSharableInstantiation(ntypars,typars);

#ifdef _DEBUG

  BOOL sharedClassInst = pMT->GetClass()->IsSharedByGenericInstantiations();
  BOOL sharedMethInst = TypeHandle::IsSharableInstantiation(ntypars,typars);  
  char inststring[300];
  char mdstring[200];
  Generics::PrettyInstantiation(inststring, 300, ntypars, typars);
  PrettyMethodDesc(pGenericMDescInRepMT, mdstring, 300);
  LOG((LF_CLASSLOADER, LL_INFO1000, "GENERICS: Instantiated method desc requested for generic method %s at instantiation %s, sharedClassInst = %d, sharedMethInst = %d\n", mdstring, inststring, (int) sharedClassInst, (int) sharedMethInst));
#endif

  BOOL getSharedNotStub = allowInstParam && sharedInst;
  BOOL needStub = !allowInstParam && sharedInst;

  // If getSharedNotStub == true, we are looking for a representative MethodDesc.
  // If we also have domain neutral class instances, this MethodDesc will point to
  // the representative MethodTable. (MethodDescs for the actual method 
  // instances point to the MT of the actual class instance).
    
  TypeHandle *repInst = NULL;
  if (sharedInst)
  { 
    // Canonicalize the type arguments.  
    repInst = (TypeHandle*) _alloca(ntypars * sizeof(TypeHandle));
    for (DWORD i = 0; i < ntypars; i++)
    {
      repInst[i] = typars[i].GetCanonicalFormAsGenericArgument();
    }
  }
  MethodTable *pRepMT = pMT->GetCanonicalMethodTable();
  MethodTable *pExactOrRepMT = getSharedNotStub ? pRepMT : pMT;
  TypeHandle *exactInst = getSharedNotStub ? repInst : typars;

  // See if we've already got the instantiated method desc for this one
  InstantiatedMethodDesc* pInstMD = FindInstantiatedMethodDesc(pExactOrRepMT, pGenericMDescInRepMT, ntypars, exactInst, getSharedNotStub);

  // No - so create one.  Go fetch the shared one first if not creating it.
  if (pInstMD == NULL)
  {
    InstantiatedMethodDesc* pSharedMDescForStub = NULL;

    if (needStub)
    {
      pSharedMDescForStub = CreateGenericInstantiation(pRepMT, pGenericMDescInRepMT, repInst, TRUE);
      if (!pSharedMDescForStub)
        return NULL;
    }

    NewInstantiatedMethodDesc(pExactOrRepMT, pGenericMDescInRepMT, pSharedMDescForStub, &pInstMD, ntypars, exactInst, getSharedNotStub);
  }
    
  _ASSERTE(pInstMD == NULL || pInstMD->HasMethodInstantiation() || pInstMD->IsGenericMethodDefinition());
  _ASSERTE(pInstMD == NULL || pInstMD->GetMethodTable() == pExactOrRepMT);
  _ASSERTE(pInstMD == NULL || !allowInstParam || pInstMD->GetMethodTable() == pMT);

  return pInstMD;
}

    
HRESULT InstantiatedMethodDesc::AllocateMethodDict(DictionaryLayout *pDictLayout)
{  
  _ASSERTE(!IsSharedByGenericInstantiations());
  _ASSERTE(!IsGenericMethodDefinition());

  DWORD numSlots = CountDictionarySlots(pDictLayout);

  // No need for expansion if there are no slots
  if (numSlots != 0) 
  {
    DWORD ntypars = GetNumGenericMethodArgs();

    // Create space for the instantiation
    TypeHandle *pPerInstInfo = (TypeHandle *) GetClass()->GetClassLoader()->GetHighFrequencyHeap()->AllocMem(sizeof(TypeHandle) * (ntypars + numSlots));
    if (pPerInstInfo == NULL) 
      return E_OUTOFMEMORY;

    memcpy(pPerInstInfo, GetMethodInstantiation(), sizeof(TypeHandle) * ntypars);
    memset(pPerInstInfo + ntypars, 0, numSlots*sizeof(TypeHandle));
    ModifyPerInstInfo(pPerInstInfo);

#ifdef _DEBUG
    char buff[300];
    PrettyMethodDesc(this, buff, 300);
    LOG((LF_CLASSLOADER, LL_INFO1000, "GENERICS: Method dictionary allocated for %s with %d slots\n", buff, numSlots));
#endif
  }

  return S_OK;
}

// Create all the dictionaries of all the methods in the same
// equivalence class as the given method.  This is done
// after we have jitted the shared code and thus have computed
// the dictionary layout ford all the shared code, which is
// the same for all the generic instances of the method.
//
// Generic instances may (indeed typically will) have been created before the dictionary
// layout is computed: these will be stubs which have not yet been invoked
// or which are waiting on another thread.  The dictionaries (i.e. the m_pPerInstInfo
// fields in the method descriptor stubs) need to be created once the 
// dictionary layout is known.
//
// The dictionary layout is known once
// for all after the method has been JITted.  Note this is
// different than the case for generic type dictionaries because
// with generic types more slots in the dictionaries may be 
// discovered as further methods get jitted.
//
HRESULT InstantiatedMethodDesc::AllocateMethodDicts()
{
  _ASSERTE(HasMethodInstantiation());
  _ASSERTE(IsSharedByGenericInstantiations());
  TypeHandle *inst = GetMethodInstantiation();
  _ASSERTE(TypeHandle::CheckInstantiationIsCanonical(GetNumGenericMethodArgs(),inst));

  DictionaryLayout *pDictLayout = GetDictionaryLayout();

#ifdef _DEBUG
  char buff[300];
  PrettyMethodDesc(this, buff, 300);
#endif
  HRESULT hr = S_OK;

  // We don't need dictionaries!
  if (pDictLayout == NULL)
  {
    LOG((LF_CLASSLOADER, LL_INFO1000, "GENERICS: No method dictionaries required for instantiations compatible with %s\n", buff));
    return hr;
  }

  MethodTable *pRepMT = GetMethodTable()->GetCanonicalMethodTable();
  MethodDescChunk *pChunk = pRepMT->GetClass()->GetChunks();

  LOG((LF_CLASSLOADER, LL_INFO1000, "GENERICS: About to allocate method dictionaries for instantiations compatible with %s\n", buff));
  while (pChunk)
  {
    if ((pChunk->GetKind() & mdcClassification) == mcInstantiated)
    {
      // This assert no longer holds because generic method descs now also have kind mcInstantiated.
      // _ASSERTE(pChunk->GetCount() == 1);
      InstantiatedMethodDesc *pInstMD = (InstantiatedMethodDesc*) (pChunk->GetFirstMethodDesc());

      _ASSERTE(pInstMD->GetMethodTable()->GetCanonicalMethodTable() == pRepMT);

      // We are looking for the ones that whose shared code is the shared MD that we've just JITted
      if (pInstMD->GetMemberDef() != GetMemberDef()
          || !pInstMD->IsInstantiatingStub())
          goto NextChunk;
            
      _ASSERTE(pInstMD != this);

      // We're expecting it to be instantiated
      _ASSERTE(pInstMD->HasMethodInstantiation());

      // Now do the comparison
      TypeHandle *inst2 = pInstMD->GetMethodInstantiation();
      _ASSERTE(inst2 != NULL);

      for (DWORD i = 0; i < pInstMD->GetNumGenericMethodArgs(); i++)
      {
        if (inst[i] != inst2[i].GetCanonicalFormAsGenericArgument())
          goto NextChunk;
      }
  
      // We found one, now allocate the dictionary and continue.
      IfFailRet(pInstMD->AllocateMethodDict(pDictLayout));      
    }
  NextChunk:
    pChunk = pChunk->GetNextChunk();
  }

  return hr;
}
  
// Given the shared MethodDesc for an instantiated method, search its dictionary layout for a particular (annotated) token.
// If there's no space in the current layout, grow the layout.
// Return a sequence of offsets into dictionaries
// IMPORTANT: dictionaries themselves will not be created until the layout is fully know (when the method has been JIT-compiled).
WORD InstantiatedMethodDesc::FindDictionaryToken(unsigned token, WORD *offsets)
{
  _ASSERTE(HasMethodInstantiation());
  _ASSERTE(IsSharedByGenericInstantiations());
  TypeHandle *inst = GetMethodInstantiation();
  _ASSERTE(TypeHandle::CheckInstantiationIsCanonical(GetNumGenericMethodArgs(),inst));

  DictionaryLayout *dictLayout = GetDictionaryLayout();

  if (dictLayout == NULL)
  {
    DWORD nslots = 4;  

    dictLayout = (DictionaryLayout*) GetClass()->GetClassLoader()->GetLowFrequencyHeap()->AllocMem(sizeof(DictionaryLayout) + sizeof(void*) * (nslots-1));
    if (dictLayout == NULL)
      return static_cast<WORD>(-1);

    // When bucket spills we'll allocate another layout structure
    dictLayout->next = NULL;

    // This is the number of slots excluding the type parameters and spill pointer
    dictLayout->numSlots = nslots;
	  
    // As we determine the layout all at once when the method is JIT-compiled, there are no spill tables required
    dictLayout->hasSpillPointer = FALSE;

    memset(dictLayout->slots, 0, sizeof(void*) * nslots);
    ModifyDictionaryLayout(dictLayout);

#ifdef _DEBUG
    char buff[300];
    PrettyMethodDesc(this, buff, 300);
    LOG((LF_JIT, LL_INFO1000, "GENERICS: Created new dictionary layout with %d slots for %s\n", nslots, buff));
#endif
  }

  _ASSERTE(dictLayout != NULL);

  WORD nindirections = 0;

  // First bucket also contains type parameters
  WORD slot = GetNumGenericMethodArgs();

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
        char buff[300];
        PrettyMethodDesc(this, buff, 300);
        LOG((LF_JIT, LL_INFO1000, "GENERICS: Allocated slot at position %s to token %x in method dictionary for %s\n", slotstr, token, buff));
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

  // We didn't find it so we need to grow the dictionary
  // The new bucket will contain twice as many entries as the last one
  DWORD nslots = lastDictLayout->numSlots*2;
  dictLayout = (DictionaryLayout*) GetClass()->GetClassLoader()->GetLowFrequencyHeap()->AllocMem(sizeof(DictionaryLayout) + sizeof(void*) * (nslots-1));
  if (dictLayout == NULL)
    return static_cast<WORD>(-1);

  dictLayout->next = NULL;
  dictLayout->numSlots = nslots;
  dictLayout->hasSpillPointer = FALSE;

  memset(dictLayout->slots, 0, sizeof(void*) * nslots);
  dictLayout->slots[0] = token;

  lastDictLayout->next = dictLayout;

  offsets[0] = (slot) * sizeof(void*);

#ifdef _DEBUG
  char buff[300];
  PrettyMethodDesc(this, buff, 300);
  LOG((LF_JIT, LL_INFO1000, "GENERICS: Extended dictionary layout with %d new slots for %s\n", nslots, buff));
  LOG((LF_JIT, LL_INFO1000, "GENERICS: Allocated slot at position %d to token %x in method dictionary for %s\n", offsets[0], token, buff));
#endif
  return 1;
}


// Set the typical (ie. formal) instantiation 
HRESULT InstantiatedMethodDesc::InitTypicalMethodInstantiation(LoaderHeap *pHeap, DWORD numTyPars)
{
    _ASSERTE (numTyPars > 0);

    //allocate space for and initialize the typical instantiation
    //we share the typical instantiation among all instantiations by placing it in the generic method desc
    LOG((LF_JIT, LL_INFO1000, "GENERICSVER: Initializing typical method instantiation with type handles\n")); 
    TypeHandle *inst = (TypeHandle *) pHeap->AllocMem(numTyPars * sizeof(TypeHandle));
    if (inst == NULL) 
        return E_OUTOFMEMORY;
	for(unsigned int i = 0; i < numTyPars; i++)
    { 
        void *mem = pHeap->AllocMem(sizeof(TypeVarTypeDesc));
        inst[i] = TypeHandle(new (mem) TypeVarTypeDesc(this,i)); 
    }; 

    ModifyTypicalMethodInstantiation(inst);
    LOG((LF_JIT, LL_INFO1000, "GENERICSVER: Initialized typical  method instantiation with %d type handles\n",numTyPars)); 
    
	return S_OK;
}
