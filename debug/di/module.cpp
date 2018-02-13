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
//*****************************************************************************
// File: module.cpp
//
//*****************************************************************************
#include "stdafx.h"

// We have an assert in ceemain.cpp that validates this assumption
#define FIELD_OFFSET_NEW_ENC_DB          0x07FFFFFB

#ifdef UNDEFINE_RIGHT_SIDE_ONLY
#undef RIGHT_SIDE_ONLY
#endif //UNDEFINE_RIGHT_SIDE_ONLY

#include "winbase.h"
#include "corpriv.h"

STDAPI ReOpenMetaDataWithMemory(
				void        *pUnk,
				LPCVOID     pData,
				ULONG       cbData);


/* ------------------------------------------------------------------------- *
 * Module class
 * ------------------------------------------------------------------------- */


// Put this here to avoid dragging in EnC.cpp

HRESULT CordbModule::GetEditAndContinueSnapshot(
    ICorDebugEditAndContinueSnapshot **ppEditAndContinueSnapshot)
{
    return CORDBG_E_INPROC_NOT_IMPL;
}

MetadataPointerCache  CordbModule::m_metadataPointerCache;

CordbModule::CordbModule(CordbProcess *process, CordbAssembly *pAssembly,
                         REMOTE_PTR debuggerModuleToken, void* pMetadataStart, 
                         ULONG nMetadataSize, REMOTE_PTR PEBaseAddress, 
                         ULONG nPESize, BOOL fDynamic, BOOL fInMemory,
                         const WCHAR *szName,
                         CordbAppDomain *pAppDomain,
                         BOOL fInproc)
    : CordbBase((ULONG)debuggerModuleToken, enumCordbModule),
    m_process(process),
    m_pAssembly(pAssembly),
    m_pAppDomain(pAppDomain),
    m_classes(11),
    m_functions(101),
    m_debuggerModuleToken(debuggerModuleToken),
    m_pIMImport(NULL),
    m_pMetadataStart(pMetadataStart),
    m_nMetadataSize(nMetadataSize),
    m_pMetadataCopy(NULL),
    m_PEBaseAddress(PEBaseAddress),
    m_nPESize(nPESize),
    m_fDynamic(fDynamic),
    m_fInMemory(fInMemory),
    m_szModuleName(NULL),
    m_pClass(NULL),
    m_fInproc(fInproc)
{
    _ASSERTE(m_debuggerModuleToken != NULL);
    // Make a copy of the name. 
    m_szModuleName = new WCHAR[wcslen(szName) + 1];
    if (m_szModuleName)
        wcscpy(m_szModuleName, szName);

    {
        DWORD dwErr;
        dwErr = process->GetID(&m_dwProcessId);
        _ASSERTE(!FAILED(dwErr));
    }
    m_metadataPointerCache.AddRef();
}

/*
    A list of which resources owned by this object are accounted for.

UNKNOWN:
        void*            m_pMetadataStartToBe;        
        void*            m_pMetadataStart; 
HANDLED:
        CordbProcess*    m_process; // Assigned w/o AddRef() 
        CordbAssembly*   m_pAssembly; // Assigned w/o AddRef() 
        CordbAppDomain*  m_pAppDomain; // Assigned w/o AddRef() 
        CordbHashTable   m_classes; // Neutered
        CordbHashTable   m_functions; // Neutered
        IMetaDataImport *m_pIMImport; // Released in ~CordbModule
        BYTE*            m_pMetadataCopy; // Deleted by m_metadataPointerCache when no other modules use it
        WCHAR*           m_szModuleName; // Deleted in ~CordbModule
        CordbClass*      m_pClass; // Released in ~CordbModule
*/

CordbModule::~CordbModule()
{
#ifdef RIGHT_SIDE_ONLY
    // We don't want to release this inproc, b/c we got it from
    // GetImporter(), which just gave us a copy of the pointer that
    // it owns.
    if (m_pIMImport)
        m_pIMImport->Release();
#endif //RIGHT_SIDE_ONLY

    if (m_pClass)
        m_pClass->Release();

    if (m_pMetadataCopy && !m_fInproc)
    {
        if (!m_fDynamic)
        {
            CordbModule::m_metadataPointerCache.ReleaseCachePointer(m_dwProcessId, m_pMetadataCopy, m_pMetadataStart, m_nMetadataSize);
        }
        else
        {
            delete[] m_pMetadataCopy;
        }
        m_pMetadataCopy = NULL;
        m_nMetadataSize = 0;
    }

    if (m_szModuleName != NULL)
        delete [] m_szModuleName;

    m_metadataPointerCache.Release();
}

// Neutered by CordbAppDomain
void CordbModule::Neuter()
{
    AddRef();
    {
        // m_process, m_pAppDomain, m_pAssembly assigned w/o AddRef()
        NeuterAndClearHashtable(&m_classes);
        NeuterAndClearHashtable(&m_functions);

        CordbBase::Neuter();
    }        
    Release();
}

HRESULT CordbModule::ConvertToNewMetaDataInMemory(BYTE *pMD, DWORD cb)
{
    if (pMD == NULL || cb == 0)
        return E_INVALIDARG;
    
    //Save what we've got
    BYTE *rgbMetadataCopyOld = m_pMetadataCopy;
    DWORD cbOld = m_nMetadataSize;

    // Try the new stuff.
    m_pMetadataCopy = pMD;
    m_nMetadataSize = cb;
    

    HRESULT hr = ReInit(true);

    if (!FAILED(hr))
    {
        if (rgbMetadataCopyOld)
        {
            delete[] rgbMetadataCopyOld;            
        }
    }
    else
    {
        // Presumably, the old MD is still there...
        m_pMetadataCopy = rgbMetadataCopyOld;
        m_nMetadataSize = cbOld;
    }

    return hr;
}

HRESULT CordbModule::Init(void)
{
    return ReInit(false);
}

// Note that if we're reopening the metadata, then this must be a dynamic
// module & we've already dragged the metadata over from the left side, so
// don't go get it again.
//
// CordbHashTableEnum::GetBase simulates the work done here by 
// simply getting an IMetaDataImporter interface from the runtime Module* -
// if more work gets done in the future, change that as well.
HRESULT CordbModule::ReInit(bool fReopen)
{
    HRESULT hr = S_OK;
    BOOL succ = true;
    //
    // Allocate enough memory for the metadata for this module and copy
    // it over from the remote process.
    //
    if (m_nMetadataSize == 0)
        return S_OK;
    
    // For inproc, simply use the already present metadata.
    if (!fReopen && !m_fInproc) 
    {
        DWORD dwErr;
        if (!m_fDynamic)
        {
            dwErr = CordbModule::m_metadataPointerCache.AddRefCachePointer(GetProcess()->m_handle, m_dwProcessId, m_pMetadataStart, m_nMetadataSize, &m_pMetadataCopy);
            if (FAILED(dwErr))
            {
                succ = false;
            }
        }
        else
        {
            dwErr = CordbModule::m_metadataPointerCache.CopyRemoteMetadata(GetProcess()->m_handle, m_pMetadataStart, m_nMetadataSize, &m_pMetadataCopy);
            if (FAILED(dwErr))
            {
                succ = false;
            }
        }
    }
    
    // else it's already local, so don't get it again (it's invalid
    //  by now, anyways)

    if (succ || fReopen)
    {
        //
        // Open the metadata scope in Read/Write mode.
        //
        IMetaDataDispenserEx *pDisp;
        hr = m_process->m_cordb->m_pMetaDispenser->QueryInterface(
                                                    IID_IMetaDataDispenserEx,
                                                    (void**)&pDisp);
        if( FAILED(hr) )
            return hr;
         
        if (fReopen)
        {   
            LOG((LF_CORDB,LL_INFO100000, "CM::RI: converting to new metadata\n"));
            IMetaDataImport *pIMImport = NULL;
            hr = pDisp->OpenScopeOnMemory(m_pMetadataCopy,
                                          m_nMetadataSize,
                                          0,
                                          IID_IMetaDataImport,
                                          (IUnknown**)&pIMImport);
            if (FAILED(hr))
            {
                pDisp->Release();
                return hr;
            }

	    hr = ReOpenMetaDataWithMemory(m_pIMImport,
					  m_pMetadataCopy,
					  m_nMetadataSize);
            pDisp->Release();
            pIMImport->Release();

            return hr;
        }

        // Save the old mode for restoration
        VARIANT valueOld;
        hr = pDisp->GetOption(MetaDataSetUpdate, &valueOld);
        if (FAILED(hr))
            return hr;

        // Set R/W mode so that we can update the metadata when
        // we do EnC operations.
        VARIANT valueRW;
        V_VT(&valueRW) = VT_UI4;
        V_I4(&valueRW) = MDUpdateFull;
        
        hr = pDisp->SetOption(MetaDataSetUpdate, &valueRW);
        if (FAILED(hr))
        {
            pDisp->Release();
            return hr;
        }

        hr = pDisp->OpenScopeOnMemory(m_pMetadataCopy,
                                      m_nMetadataSize,
                                      0,
                                      IID_IMetaDataImport,
                                      (IUnknown**)&m_pIMImport);
        if (FAILED(hr))
        {
            pDisp->Release();
            return hr;
        }
        
        // Restore the old setting
        hr = pDisp->SetOption(MetaDataSetUpdate, &valueOld);
        pDisp->Release();
        
        if (FAILED(hr))
            return hr;
    }
    else
    {
        hr = HRESULT_FROM_WIN32(GetLastError());

        if (m_pMetadataCopy)
        {
            if (!m_fDynamic)
            {
                CordbModule::m_metadataPointerCache.ReleaseCachePointer(m_dwProcessId, m_pMetadataCopy, m_pMetadataStart, m_nMetadataSize);
            }
            else
            {
                delete[] m_pMetadataCopy;
            }
            m_pMetadataCopy = NULL;
            m_nMetadataSize = 0;
        }
        return hr;
    }
    
    return hr;
}


HRESULT CordbModule::QueryInterface(REFIID id, void **pInterface)
{
    if (id == IID_ICorDebugModule)
        *pInterface = (ICorDebugModule*)this;
    else if (id == IID_IUnknown)
        *pInterface = (IUnknown*)(ICorDebugModule*)this;
    else
    {
        *pInterface = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

HRESULT CordbModule::GetProcess(ICorDebugProcess **ppProcess)
{
    VALIDATE_POINTER_TO_OBJECT(ppProcess, ICorDebugProcess **);
    
    *ppProcess = (ICorDebugProcess*)m_process;
    (*ppProcess)->AddRef();

    return S_OK;
}

HRESULT CordbModule::GetBaseAddress(CORDB_ADDRESS *pAddress)
{
    VALIDATE_POINTER_TO_OBJECT(pAddress, CORDB_ADDRESS *);
    
    *pAddress = PTR_TO_CORDB_ADDRESS(m_PEBaseAddress);
    return S_OK;
}

HRESULT CordbModule::GetAssembly(ICorDebugAssembly **ppAssembly)
{
    VALIDATE_POINTER_TO_OBJECT(ppAssembly, ICorDebugAssembly **);

#ifndef RIGHT_SIDE_ONLY
    // There exists a chance that the assembly wasn't available when we
    // got the module the first time (eg, ModuleLoadFinished before
    // AssemblyLoadFinished).  If the module's assembly is now available,
    // attach it to the module.
    if (m_pAssembly == NULL)
    {
        // try and go get it.
        DebuggerModule *dm = (DebuggerModule *)m_debuggerModuleToken;
        Assembly *as = dm->m_pRuntimeModule->GetAssembly();
        if (as != NULL)
        {
            CordbAssembly *ca = (CordbAssembly*)GetAppDomain()
                ->m_assemblies.GetBase((ULONG)as);

            _ASSERTE(ca != NULL);
            m_pAssembly = ca;
        }
    }
#endif //RIGHT_SIDE_ONLY

    *ppAssembly = (ICorDebugAssembly *)m_pAssembly;
    if ((*ppAssembly) != NULL)
        (*ppAssembly)->AddRef();

    return S_OK;
}

HRESULT CordbModule::GetName(ULONG32 cchName, ULONG32 *pcchName, WCHAR szName[])
{
    VALIDATE_POINTER_TO_OBJECT_ARRAY_OR_NULL(szName, WCHAR, cchName, true, true);
    VALIDATE_POINTER_TO_OBJECT_OR_NULL(pcchName, ULONG32);

    const WCHAR *szTempName = m_szModuleName;

    // In case we didn't get the name (most likely out of memory on ctor).
    if (!szTempName)
        szTempName = L"<unknown>";

    // true length of the name, with null
    SIZE_T iTrueLen = wcslen(szTempName) + 1;

    // Do a safe buffer copy including null if there is room.
    if (szName != NULL)
    {
        // Figure out the length that can actually be copied
        SIZE_T iCopyLen = min(cchName, iTrueLen);
    
        wcsncpy(szName, szTempName, iCopyLen);

        // Force a null no matter what, and return the count if desired.
        szName[iCopyLen - 1] = 0;
    }
    
    // Always provide the true string length, so the caller can know if they
    // provided an insufficient buffer.  The length includes the null char.
    if (pcchName)
        *pcchName = iTrueLen;

    return S_OK;
}

HRESULT CordbModule::EnableJITDebugging(BOOL bTrackJITInfo, BOOL bAllowJitOpts)
{
#ifndef RIGHT_SIDE_ONLY
    return CORDBG_E_INPROC_NOT_IMPL;
#else
    CordbProcess *pProcess = GetProcess();
    CORDBCheckProcessStateOKAndSync(pProcess, GetAppDomain());
    
    DebuggerIPCEvent event;
    pProcess->InitIPCEvent(&event, 
                           DB_IPCE_CHANGE_JIT_DEBUG_INFO, 
                           true,
                           (void *)(GetAppDomain()->m_id));
                           
    event.JitDebugInfo.debuggerModuleToken = m_debuggerModuleToken;
    event.JitDebugInfo.fTrackInfo = bTrackJITInfo;
    event.JitDebugInfo.fAllowJitOpts = bAllowJitOpts;
    
    // Note: two-way event here...
    HRESULT hr = pProcess->m_cordb->SendIPCEvent(pProcess, 
                                                 &event,
                                                 sizeof(DebuggerIPCEvent));

    if (!SUCCEEDED(hr))
        return hr;

    _ASSERTE(event.type == DB_IPCE_CHANGE_JIT_INFO_RESULT);
    
    return event.hr;
#endif //RIGHT_SIDE_ONLY    
}

HRESULT CordbModule::EnableClassLoadCallbacks(BOOL bClassLoadCallbacks)
{
#ifndef RIGHT_SIDE_ONLY
    return CORDBG_E_INPROC_NOT_IMPL;
#else
    // You must receive ClassLoad callbacks for dynamic modules so that we can keep the metadata up-to-date on the Right
    // Side. Therefore, we refuse to turn them off for all dynamic modules (they were forced on when the module was
    // loaded on the Left Side.)
    if (m_fDynamic && !bClassLoadCallbacks)
        return E_INVALIDARG;
    
    // Send a Set Class Load Flag event to the left side. There is no need to wait for a response, and this can be
    // called whether or not the process is synchronized.
    CordbProcess *pProcess = GetProcess();
    
    DebuggerIPCEvent event;
    pProcess->InitIPCEvent(&event, 
                           DB_IPCE_SET_CLASS_LOAD_FLAG, 
                           false,
                           (void *)(GetAppDomain()->m_id));
    event.SetClassLoad.debuggerModuleToken = m_debuggerModuleToken;
    event.SetClassLoad.flag = (bClassLoadCallbacks == TRUE);

    HRESULT hr = pProcess->m_cordb->SendIPCEvent(pProcess, &event,
                                                 sizeof(DebuggerIPCEvent));
    
    return hr;
#endif //RIGHT_SIDE_ONLY    
}

HRESULT CordbModule::GetFunctionFromToken(mdMethodDef token,
                                          ICorDebugFunction **ppFunction)
{
    if (token == mdMethodDefNil)
        return E_INVALIDARG;
        
    VALIDATE_POINTER_TO_OBJECT(ppFunction, ICorDebugFunction **);
    
    HRESULT hr = S_OK;

    INPROC_LOCK();
    
    // If we already have a CordbFunction for this token, then we'll
    // take since we know it has to be valid.
    CordbFunction *f = (CordbFunction *)m_functions.GetBase(token);

    if (f == NULL)
    {
        // Validate the token.
        if (!m_pIMImport->IsValidToken(token))
        {
            hr = E_INVALIDARG;
            goto LExit;
        }

        f = new CordbFunction(this, token, 0);
            
        if (f == NULL)
        {
            hr = E_OUTOFMEMORY;
            goto LExit;
        }

        hr = m_functions.AddBase(f);
        
        if (FAILED(hr))
        {
            delete f;
            goto LExit;
        }
    }
    
    *ppFunction = (ICorDebugFunction*)f;
    (*ppFunction)->AddRef();
    
LExit:
    INPROC_UNLOCK();
    return hr;
}

HRESULT CordbModule::GetFunctionFromRVA(CORDB_ADDRESS rva,
                                        ICorDebugFunction **ppFunction)
{
    VALIDATE_POINTER_TO_OBJECT(ppFunction, ICorDebugFunction **);
    
    return E_NOTIMPL;
}

HRESULT CordbModule::LookupClassByToken(mdTypeDef token, 
                                        CordbClass **ppClass)
{
    *ppClass = NULL;
    
    if ((token == mdTypeDefNil) || (TypeFromToken(token) != mdtTypeDef))
        return E_INVALIDARG;
    
    CordbClass *c = (CordbClass *)m_classes.GetBase(token);

    if (c == NULL)
    {
        // Validate the token.
        if (!m_pIMImport->IsValidToken(token))
            return E_INVALIDARG;
        
        c = new CordbClass(this, token);

        if (c == NULL)
            return E_OUTOFMEMORY;
        
        HRESULT res = m_classes.AddBase(c);

        if (FAILED(res))
        {
            delete c;
            return (res);
        }
    }

    *ppClass = c;

    return S_OK;
}

HRESULT CordbModule::LookupClassByName(LPWSTR fullClassName,
                                       CordbClass **ppClass)
{
    WCHAR fullName[MAX_CLASSNAME_LENGTH + 1];
    wcscpy(fullName, fullClassName);

    *ppClass = NULL;

    // Find the TypeDef for this class, if it exists.
    mdTypeDef token = mdTokenNil;
    WCHAR *pStart = fullName;
    HRESULT hr;

    do
    {
        WCHAR *pEnd = wcschr(pStart, NESTED_SEPARATOR_WCHAR);
        if (pEnd)
            *pEnd++ = L'\0';

        hr = m_pIMImport->FindTypeDefByName(pStart,
                                            token,
                                            &token);
        pStart = pEnd;

    } while (pStart && SUCCEEDED(hr));

    if (FAILED(hr))
        return hr;

    // Now that we have the token, simply call the normal lookup...
    return LookupClassByToken(token, ppClass);
}

HRESULT CordbModule::GetClassFromToken(mdTypeDef token, 
                                       ICorDebugClass **ppClass)
{
    CordbClass *c;

    VALIDATE_POINTER_TO_OBJECT(ppClass, ICorDebugClass **);
    
    // Validate the token.
    if (!m_pIMImport->IsValidToken(token))
        return E_INVALIDARG;
        
    INPROC_LOCK();    
    
    HRESULT hr = LookupClassByToken(token, &c);

    if (SUCCEEDED(hr))
    {
        *ppClass = (ICorDebugClass*)c;
        (*ppClass)->AddRef();
    }
    
    INPROC_UNLOCK();
    
    return hr;
}

HRESULT CordbModule::CreateBreakpoint(ICorDebugModuleBreakpoint **ppBreakpoint)
{
#ifndef RIGHT_SIDE_ONLY
    return CORDBG_E_INPROC_NOT_IMPL;
#else
    VALIDATE_POINTER_TO_OBJECT(ppBreakpoint, ICorDebugModuleBreakpoint **);

    return E_NOTIMPL;
#endif //RIGHT_SIDE_ONLY    
}

//
// Return the token for the Module table entry for this object.  The token
// may then be passed to the meta data import api's.
//
HRESULT CordbModule::GetToken(mdModule *pToken)
{
    VALIDATE_POINTER_TO_OBJECT(pToken, mdModule *);
    HRESULT hr = S_OK;

    INPROC_LOCK();

    _ASSERTE(m_pIMImport);
    hr = (m_pIMImport->GetModuleFromScope(pToken));

    INPROC_UNLOCK();
    
    return hr;
}


//
// Return a meta data interface pointer that can be used to examine the
// meta data for this module.
HRESULT CordbModule::GetMetaDataInterface(REFIID riid, IUnknown **ppObj)
{
    VALIDATE_POINTER_TO_OBJECT(ppObj, IUnknown **);
    HRESULT hr = S_OK;

    INPROC_LOCK();
    
    // QI the importer that we already have and return the result.
    hr = m_pIMImport->QueryInterface(riid, (void**)ppObj);

    INPROC_UNLOCK();

    return hr;
}

//
// LookupFunction finds an existing CordbFunction in the given module.
// If the function doesn't exist, it returns NULL.
//
CordbFunction* CordbModule::LookupFunction(mdMethodDef funcMetadataToken)
{
    return (CordbFunction *)m_functions.GetBase(funcMetadataToken);
}

HRESULT CordbModule::IsDynamic(BOOL *pDynamic)
{
    VALIDATE_POINTER_TO_OBJECT(pDynamic, BOOL *);

    (*pDynamic) = m_fDynamic;

    return S_OK;
}

HRESULT CordbModule::IsInMemory(BOOL *pInMemory)
{
    VALIDATE_POINTER_TO_OBJECT(pInMemory, BOOL *);

    (*pInMemory) = m_fInMemory;

    return S_OK;
}

HRESULT CordbModule::GetGlobalVariableValue(mdFieldDef fieldDef,
                                            ICorDebugValue **ppValue)
{
    VALIDATE_POINTER_TO_OBJECT(ppValue, ICorDebugValue **);

    HRESULT hr = S_OK;

    INPROC_LOCK();

    if (m_pClass == NULL)
    {
        hr = LookupClassByToken(COR_GLOBAL_PARENT_TOKEN,
                                &m_pClass);
        if (FAILED(hr))
            goto LExit;
        _ASSERTE( m_pClass != NULL);
    }        
    
    hr = m_pClass->GetStaticFieldValue(fieldDef, NULL, ppValue);
                                       
LExit:

    INPROC_UNLOCK();
    return hr;
}



//
// CreateFunction creates a new function from the given information and
// adds it to the module.
//
HRESULT CordbModule::CreateFunction(mdMethodDef funcMetadataToken,
                                    SIZE_T funcRVA,
                                    CordbFunction** ppFunction)
{
    // Create a new function object.
    CordbFunction* pFunction = new CordbFunction(this,funcMetadataToken, funcRVA);

    if (pFunction == NULL)
        return E_OUTOFMEMORY;

    // Add the function to the Module's hash of all functions.
    HRESULT hr = m_functions.AddBase(pFunction);
        
    if (SUCCEEDED(hr))
        *ppFunction = pFunction;
    else
        delete pFunction;

    return hr;
}



HRESULT CordbModule::LookupOrCreateClass(mdTypeDef classMetadataToken,CordbClass** ppClass)
{
    HRESULT hr = S_OK;
    *ppClass = LookupClass(classMetadataToken);
    if (*ppClass == NULL)
    {
        hr = CreateClass(classMetadataToken,ppClass);
        if (!SUCCEEDED(hr))
            return hr;
    }
    return hr;
}

//
// LookupClass finds an existing CordbClass in the given module.
// If the class doesn't exist, it returns NULL.
//
CordbClass* CordbModule::LookupClass(mdTypeDef classMetadataToken)
{
    return (CordbClass *)m_classes.GetBase(classMetadataToken);
}

//
// CreateClass creates a new class from the given information and
// adds it to the module.
//
HRESULT CordbModule::CreateClass(mdTypeDef classMetadataToken,
                                 CordbClass** ppClass)
{
    CordbClass* pClass =
        new CordbClass(this, classMetadataToken);

    if (pClass == NULL)
        return E_OUTOFMEMORY;

    HRESULT hr = m_classes.AddBase(pClass);

    if (SUCCEEDED(hr))
        *ppClass = pClass;
    else
        delete pClass;

    if (classMetadataToken == COR_GLOBAL_PARENT_TOKEN)
    {
        _ASSERTE( m_pClass == NULL ); //redundant create
        m_pClass = pClass;
        m_pClass->AddRef();
    }

    return hr;
}

HRESULT CordbModule::ResolveTypeRef(mdTypeRef token,
                                    CordbClass **ppClass)
{
    *ppClass = NULL;
    
    if ((token == mdTypeRefNil) || (TypeFromToken(token) != mdtTypeRef))
        return E_INVALIDARG;
    
    // Get the necessary properties of the typeref from this module.
    WCHAR typeName[MAX_CLASSNAME_LENGTH + 1];
    WCHAR fullName[MAX_CLASSNAME_LENGTH + 1];
    HRESULT hr;

    WCHAR *pName = typeName + MAX_CLASSNAME_LENGTH + 1;
    WCHAR cSep = L'\0';
    ULONG fullNameLen;

    do
    {
        if (pName <= typeName)
            hr = E_FAIL;       // buffer too small
        else
            hr = m_pIMImport->GetTypeRefProps(token,
                                          &token,
                                          fullName,
                                          MAX_CLASSNAME_LENGTH,
                                          &fullNameLen);
        if (SUCCEEDED(hr))
        {
            *(--pName) = cSep;
            cSep = NESTED_SEPARATOR_WCHAR;

            fullNameLen--;          // don't count null terminator
            pName -= fullNameLen;

            if (pName < typeName)
                hr = E_FAIL;       // buffer too small
            else
                memcpy(pName, fullName, fullNameLen*sizeof(fullName[0]));
         }

    }
    while (TypeFromToken(token) == mdtTypeRef && SUCCEEDED(hr));

    if (FAILED(hr))
        return hr;

    return GetAppDomain()->ResolveClassByName(pName, ppClass);
}

HRESULT CordbModule::ResolveTypeRefOrDef(mdToken token,
                                    CordbClass **ppClass)
{
    
    if ((token == mdTypeRefNil) || 
		(TypeFromToken(token) != mdtTypeRef && TypeFromToken(token) != mdtTypeDef))
        return E_INVALIDARG;
    
	if (TypeFromToken(token)==mdtTypeRef)
	{
		return ( ResolveTypeRef(token, ppClass) );
	}
	else
	{
		return ( LookupClassByToken(token, ppClass) );
	}

}

//
// Copy the metadata from the in-memory cached copy to the output stream given.
// This was done in lieu of using an accessor to return the pointer to the cached
// data, which would not have been thread safe during updates.
//
HRESULT CordbModule::SaveMetaDataCopyToStream(IStream *pIStream)
{
    ULONG       cbWritten;              // Junk variable for output.
    HRESULT     hr;

    // Caller must have the stream ready for input at current location.  Simply
    // write from our copy of the current metadata to the stream.  Expectations
    // are that the data can be written and all of it was, which we assert.
    _ASSERTE(pIStream);
    hr = pIStream->Write(m_pMetadataCopy, m_nMetadataSize, &cbWritten);
    _ASSERTE(FAILED(hr) || cbWritten == m_nMetadataSize);
    return (hr);
}

//
// GetSize returns the size of the module.
//
HRESULT CordbModule::GetSize(ULONG32 *pcBytes)
{
    VALIDATE_POINTER_TO_OBJECT(pcBytes, ULONG32 *);

    *pcBytes = m_nPESize;

    return S_OK;
}

CordbAssembly *CordbModule::GetCordbAssembly(void)
{
#ifndef RIGHT_SIDE_ONLY
    // There exists a chance that the assembly wasn't available when we
    // got the module the first time (eg, ModuleLoadFinished before
    // AssemblyLoadFinished).  If the module's assembly is now available,
    // attach it to the module.
    if (m_pAssembly == NULL)
    {
        // try and go get it.
        DebuggerModule *dm = (DebuggerModule *)m_debuggerModuleToken;
        Assembly *as = dm->m_pRuntimeModule->GetAssembly();
        if (as != NULL)
        {
            CordbAssembly *ca = (CordbAssembly*)GetAppDomain()
                ->m_assemblies.GetBase((ULONG)as);
    
            _ASSERTE(ca != NULL);
            m_pAssembly = ca;
        }
    }
#endif //RIGHT_SIDE_ONLY

    return m_pAssembly;
}


/* ------------------------------------------------------------------------- *
 * Class class
 * ------------------------------------------------------------------------- */

CordbClass::CordbClass(CordbModule *m, mdTypeDef classMetadataToken)
  : CordbBase(classMetadataToken, enumCordbClass),
    m_loadEventSent(FALSE),
    m_hasBeenUnloaded(false),
    m_classtypes(2),
    m_module(m),
    m_token(classMetadataToken),
    m_typarCount(0),
    m_EnCCounterLastSyncClass(0),
    m_continueCounterLastSync(0),
    m_isValueClass(false),
    m_instanceVarCount(0),
    m_staticVarCount(0),
    m_staticVarBase(NULL),
    m_fields(NULL),
    m_objectSize(0)  
{
}


/*
    A list of which resources owned by this object are accounted for.

    UNKNOWN:
        CordbSyncBlockFieldTable m_syncBlockFieldsStatic; 
    HANDLED:
        CordbModule*            m_module; // Assigned w/o AddRef()
        DebuggerIPCE_FieldData *m_fields; // Deleted in ~CordbClass
*/

CordbClass::~CordbClass()
{
    if(m_fields)
        delete [] m_fields;
}

// Neutered by CordbModule
void CordbClass::Neuter()
{
    AddRef();
    {   
		// GENERICS: Constructed types include pointers across to other types - reduce
		// the reference counts on these....
        NeuterAndClearHashtable(&m_classtypes);
        CordbBase::Neuter();
    }
    Release();
}    





HRESULT CordbClass::QueryInterface(REFIID id, void **pInterface)
{
    if (id == IID_ICorDebugClass)
        *pInterface = (ICorDebugClass*)this;
    else if (id == IID_IUnknown)
        *pInterface = (IUnknown*)(ICorDebugClass*)this;
    else
    {
        *pInterface = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

HRESULT CordbClass::GetStaticFieldValue(mdFieldDef fieldDef,
                                        ICorDebugFrame *pFrame,
                                        ICorDebugValue **ppValue)
{
    VALIDATE_POINTER_TO_OBJECT(ppValue, ICorDebugValue **);

#ifdef RIGHT_SIDE_ONLY
    CORDBRequireProcessStateOKAndSync(GetProcess(), GetAppDomain());
#else 
    // For the Virtual Right Side (In-proc debugging), we'll always be synched, but not neccessarily b/c we've gotten a
    // synch message.
    CORDBRequireProcessStateOK(GetProcess());
#endif    

    HRESULT          hr = S_OK;
    *ppValue = NULL;
    BOOL             fSyncBlockField = FALSE;

    // Used below for faking out CreateValueByType
    static CorElementType elementTypeClass = ELEMENT_TYPE_CLASS;

    INPROC_LOCK();
    
    // Validate the token.
    if (!GetModule()->m_pIMImport->IsValidToken(fieldDef))
    {
        hr = E_INVALIDARG;
        goto LExit;
    }

    // Make sure we have enough info about the class. Also re-query if the static var base is still NULL.
    hr = Init(m_staticVarBase == NULL);

    if (!SUCCEEDED(hr))
        goto LExit;

    // Lookup the field given its metadata token.
    DebuggerIPCE_FieldData *pFieldData;

    hr = GetFieldInfo(fieldDef, &pFieldData);

    if (hr == CORDBG_E_ENC_HANGING_FIELD)
    {
        hr = GetSyncBlockField(fieldDef, 
                               &pFieldData,
                               NULL);
            
        if (SUCCEEDED(hr))
            fSyncBlockField = TRUE;
    }
    
    if (!SUCCEEDED(hr))
        goto LExit;

    if (!pFieldData->fldIsStatic)
    {
        hr = CORDBG_E_FIELD_NOT_STATIC;
        goto LExit;
    }
    
    REMOTE_PTR pRmtStaticValue;

    if (!pFieldData->fldIsTLS && !pFieldData->fldIsContextStatic)
    {
        // We'd better have the static area initialized on the Left Side.
        if (m_staticVarBase == NULL)
        {
            hr = CORDBG_E_STATIC_VAR_NOT_AVAILABLE;
            goto LExit;
        }
    
        // For normal old static variables (including ones that are relative to the app domain... that's handled on the
        // Left Side through manipulation of m_staticVarBase) the address of the variable is m_staticVarBase + the
        // variable's offset.
        pRmtStaticValue = (BYTE*)m_staticVarBase + pFieldData->fldOffset;
    }
    else
    {
        if (fSyncBlockField)
        {
            _ASSERTE(!pFieldData->fldIsContextStatic);
            pRmtStaticValue = (REMOTE_PTR)pFieldData->fldOffset;
        }
        else
        {
            // What thread are we working on here.
            if (pFrame == NULL)
            {
                hr = E_INVALIDARG;
                goto LExit;
            }
            
            ICorDebugChain *pChain = NULL;

            hr = pFrame->GetChain(&pChain);

            if (FAILED(hr))
                goto LExit;

            CordbChain *c = (CordbChain*)pChain;
            CordbThread *t = c->m_thread;

            // Send an event to the Left Side to find out the address of this field for the given thread.
            DebuggerIPCEvent event;
            GetProcess()->InitIPCEvent(&event, DB_IPCE_GET_SPECIAL_STATIC, true, (void *)(m_module->GetAppDomain()->m_id));
            event.GetSpecialStatic.fldDebuggerToken = pFieldData->fldDebuggerToken;
            event.GetSpecialStatic.debuggerThreadToken = t->m_debuggerThreadToken;

            // Note: two-way event here...
            hr = GetProcess()->m_cordb->SendIPCEvent(GetProcess(), &event, sizeof(DebuggerIPCEvent));

            if (FAILED(hr))
                goto LExit;

            _ASSERTE(event.type == DB_IPCE_GET_SPECIAL_STATIC_RESULT);

            pRmtStaticValue = (BYTE*)event.GetSpecialStaticResult.fldAddress;
        }
        
        if (pRmtStaticValue == NULL)
        {
            hr = CORDBG_E_STATIC_VAR_NOT_AVAILABLE;
            goto LExit;
        }
    }

  {
    CordbType *type;
	hr = CordbType::SigToType(GetModule(), pFieldData->fldFullSig, Instantiation(), &type);
	if (FAILED(hr))
		goto LExit;

    type = type->SkipFunkyModifiers();

	CorElementType et = type->m_elementType;

    // If this is a static that is non-primitive, then we have to do an extra level of indirection.
    if (!pFieldData->fldIsTLS &&
        !pFieldData->fldIsContextStatic &&
        !fSyncBlockField &&               // EnC-added fields don't need the extra de-ref.
        !pFieldData->fldIsPrimitive &&    // Classes that are really primitives don't need the extra de-ref.
        ((et == ELEMENT_TYPE_CLASS)    || 
         (et == ELEMENT_TYPE_OBJECT)   ||
         (et == ELEMENT_TYPE_SZARRAY)  || 
         (et == ELEMENT_TYPE_ARRAY)    ||
         (et == ELEMENT_TYPE_STRING)   ||
         (et == ELEMENT_TYPE_VALUETYPE && !pFieldData->fldIsRVA)))
    {

        REMOTE_PTR pRealRmtStaticValue = NULL;
        
        BOOL succ = ReadProcessMemoryI(GetProcess()->m_handle,
                                       pRmtStaticValue,
                                       &pRealRmtStaticValue,
                                       sizeof(pRealRmtStaticValue),
                                       NULL);
        
        if (!succ)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto LExit;
        }

        if (pRealRmtStaticValue == NULL)
        {
            hr = CORDBG_E_STATIC_VAR_NOT_AVAILABLE;
            goto LExit;
        }

        pRmtStaticValue = pRealRmtStaticValue;
    }
    
    // Static value classes are stored as handles so that GC can deal with them properly.  Thus, we need to follow the
    // handle like an objectref.  Do this by forcing CreateValueByType to think this is an objectref. Note: we don't do
    // this for value classes that have an RVA, since they're layed out at the RVA with no handle.
    if (et == ELEMENT_TYPE_VALUETYPE &&
        !pFieldData->fldIsRVA &&
        !pFieldData->fldIsPrimitive &&
        !pFieldData->fldIsTLS &&
        !pFieldData->fldIsContextStatic)
    {
        hr = CordbType::MkNullaryType(GetAppDomain(), elementTypeClass, &type);
        if (FAILED(hr))
			goto LExit;
    }
    
    ICorDebugValue *pValue;
    hr = CordbValue::CreateValueByType(GetAppDomain(),
                                       type,
                                       pRmtStaticValue, NULL,
                                       false,
                                       NULL,
                                       NULL,
                                       &pValue);

    if (SUCCEEDED(hr))
        *ppValue = pValue;
  }

LExit:
    INPROC_UNLOCK();

    hr = CordbClass::PostProcessUnavailableHRESULT(hr, GetModule()->m_pIMImport, fieldDef);
    
    return hr;
}


HRESULT CordbClass::GetType(ULONG32 nTypeArgs, ICorDebugType **ppTypeArgs, ICorDebugType **pType)
{
	bool isVC;
	HRESULT hr = IsValueClass(&isVC);
	if (FAILED(hr))
		return hr;
	hr = CordbType::MkConstructedType(GetAppDomain(), isVC ? ELEMENT_TYPE_VALUETYPE : ELEMENT_TYPE_CLASS, this, Instantiation(nTypeArgs,(CordbType **) ppTypeArgs), (CordbType **) pType);
	if (FAILED(hr))
		return hr;
	_ASSERTE(*pType);
	if (*pType)
		(*pType)->AddRef();
	return S_OK;
}


HRESULT CordbClass::PostProcessUnavailableHRESULT(HRESULT hr, 
                                       IMetaDataImport *pImport,
                                       mdFieldDef fieldDef)
{                                       
    if (hr == CORDBG_E_FIELD_NOT_AVAILABLE)
    {
        DWORD dwFieldAttr;
        hr = pImport->GetFieldProps(
            fieldDef,
            NULL,
            NULL,
            0,
            NULL,
            &dwFieldAttr,
            NULL,
            0,
            NULL,
            NULL,
            0);

        if (IsFdLiteral(dwFieldAttr))
        {
            hr = CORDBG_E_VARIABLE_IS_ACTUALLY_LITERAL;
        }
    }

    return hr;
}

HRESULT CordbClass::GetModule(ICorDebugModule **ppModule)
{
    VALIDATE_POINTER_TO_OBJECT(ppModule, ICorDebugModule **);
    
    *ppModule = (ICorDebugModule*) m_module;
    (*ppModule)->AddRef();

    return S_OK;
}

HRESULT CordbClass::GetToken(mdTypeDef *pTypeDef)
{
    VALIDATE_POINTER_TO_OBJECT(pTypeDef, mdTypeDef *);
    
    *pTypeDef = m_token;

    return S_OK;
}

HRESULT CordbClass::IsValueClass(bool *pIsValueClass)
{
#ifdef RIGHT_SIDE_ONLY
    CORDBRequireProcessStateOKAndSync(GetProcess(), GetAppDomain());
#else 
    // For the Virtual Right Side (In-proc debugging), we'll
    // always be synched, but not neccessarily b/c we've
    // gotten a synch message.
    CORDBRequireProcessStateOK(GetProcess());
#endif    

    HRESULT hr = S_OK;
    *pIsValueClass = false;
    
    hr = Init(FALSE);

    if (!SUCCEEDED(hr))
        return hr;

    *pIsValueClass = m_isValueClass;

    return hr;
}

HRESULT CordbClass::GetThisType(const Instantiation &inst, CordbType **pRes)
{
    HRESULT hr = S_OK;

    hr = Init(FALSE);
	if (!SUCCEEDED(hr))
		return hr;
    if (m_isValueClass)
    {
     	CordbType *ty;
		hr = CordbType::MkConstructedType(GetAppDomain(),ELEMENT_TYPE_VALUETYPE,this, inst, &ty);
		if (!SUCCEEDED(hr))
			return hr;
		hr = CordbType::MkUnaryType(GetAppDomain(),ELEMENT_TYPE_BYREF,0, ty, pRes);
		if (!SUCCEEDED(hr))
			return hr;
	}
	else {
		hr = CordbType::MkConstructedType(GetAppDomain(),ELEMENT_TYPE_CLASS,this,inst, pRes);
		if (!SUCCEEDED(hr))
			return hr;
	}

    return hr;
}

HRESULT CordbClass::Init(BOOL fForceInit)
{
    // If we've done a continue since we last time we got hanging static fields,
    // we should clear our our cache, since everything may have moved.
    if (m_continueCounterLastSync < GetProcess()->m_continueCounter)
    {
        m_syncBlockFieldsStatic.Clear();
        m_continueCounterLastSync = GetProcess()->m_continueCounter;
    }
    
    // We don't have to reinit if the EnC version is up-to-date &
    // we haven't been told to do the init regardless.
    if (m_EnCCounterLastSyncClass >= GetProcess()->m_EnCCounter
        && !fForceInit)
        return S_OK;
        
    bool wait = true;
    bool fFirstEvent = true;
    unsigned int fieldIndex = 0;
    unsigned int totalFieldCount = 0;
    DebuggerIPCEvent *retEvent = NULL;
    
    CORDBSyncFromWin32StopIfStopped(GetProcess());

    INPROC_LOCK();
    
    HRESULT hr = S_OK;
    
    // We've got a remote address that points to the EEClass.
    // We need to send to the left side to get real information about
    // the class, including its instance and static variables.
    CordbProcess *pProcess = GetProcess();
    
    DebuggerIPCEvent event;
    pProcess->InitIPCEvent(&event, 
                           DB_IPCE_GET_CLASS_INFO, 
                           false,
                           (void *)(m_module->GetAppDomain()->m_id));
    event.GetClassInfo.metadataToken = m_token;
    event.GetClassInfo.debuggerModuleToken = m_module->m_debuggerModuleToken;
	event.GetClassInfo.typeHandle = NULL;

    hr = pProcess->m_cordb->SendIPCEvent(pProcess, &event,
                                         sizeof(DebuggerIPCEvent));

    // Stop now if we can't even send the event.
    if (!SUCCEEDED(hr))
        goto exit;

    // Wait for events to return from the RC. We expect at least one
    // class info result event.
    retEvent = (DebuggerIPCEvent *) _alloca(CorDBIPC_BUFFER_SIZE);

    while (wait)
    {
#ifdef RIGHT_SIDE_ONLY
        hr = pProcess->m_cordb->WaitForIPCEventFromProcess(pProcess, 
                                                    m_module->GetAppDomain(),
                                                    retEvent);
#else 
        if (fFirstEvent)
            hr = pProcess->m_cordb->GetFirstContinuationEvent(pProcess,retEvent);
        else
            hr = pProcess->m_cordb->GetNextContinuationEvent(pProcess,retEvent);
#endif //RIGHT_SIDE_ONLY    

        if (!SUCCEEDED(hr))
            goto exit;
        
        _ASSERTE(retEvent->type == DB_IPCE_GET_CLASS_INFO_RESULT);

        // If this is the first event back from the RC, then create the
        // array to hold the field.
        if (fFirstEvent)
        {
            fFirstEvent = false;

#ifdef _DEBUG
            // Shouldn't ever loose fields!
            totalFieldCount = m_instanceVarCount + m_staticVarCount;
            _ASSERTE(retEvent->GetClassInfoResult.instanceVarCount +
                     retEvent->GetClassInfoResult.staticVarCount >=
                     totalFieldCount);
#endif
            
            m_isValueClass = retEvent->GetClassInfoResult.isValueClass;
            m_typarCount = retEvent->GetClassInfoResult.typarCount;
  	        // If type is a generic  type then use the size in the instantiated type
			m_objectSize = (m_typarCount != 0) ? 0xbadbad : retEvent->GetClassInfoResult.objectSize;
            m_staticVarBase = retEvent->GetClassInfoResult.staticVarBase;
            m_instanceVarCount = retEvent->GetClassInfoResult.instanceVarCount;
            m_staticVarCount = retEvent->GetClassInfoResult.staticVarCount;

            totalFieldCount = m_instanceVarCount + m_staticVarCount;

            // Since we don't keep pointers to the m_fields elements, 
            // just toss it & get a new one.
            if (m_fields != NULL)
            {
                delete m_fields;
                m_fields = NULL;
            }
            
            if (totalFieldCount > 0)
            {
                m_fields = new DebuggerIPCE_FieldData[totalFieldCount];

                if (m_fields == NULL)
                {
                    hr = E_OUTOFMEMORY;
                    goto exit;
                }
            }
        }

        DebuggerIPCE_FieldData *currentFieldData =
            &(retEvent->GetClassInfoResult.fieldData);

        for (unsigned int i = 0; i < retEvent->GetClassInfoResult.fieldCount;
             i++)
        {
            m_fields[fieldIndex] = *currentFieldData;
            
            _ASSERTE(m_fields[fieldIndex].fldOffset != FIELD_OFFSET_NEW_ENC_DB);
            
            currentFieldData++;
            fieldIndex++;
        }

        if (fieldIndex >= totalFieldCount)
            wait = false;
    }

    // Remember the most recently acquired version of this class
    m_EnCCounterLastSyncClass = GetProcess()->m_EnCCounter;

exit:    

#ifndef RIGHT_SIDE_ONLY    
    GetProcess()->ClearContinuationEvents();
#endif
    
    INPROC_UNLOCK();
    
    return hr;
}




/* static */ HRESULT CordbClass::GetFieldSig(CordbModule *module, mdFieldDef fldToken, DebuggerIPCE_FieldData *pFieldData)
{
    HRESULT hr = S_OK;
    
    // Go to the metadata for all fields: previously the left-side tranferred over
	// single-byte signatures as part of the field info.  Since the left-side
	// goes to the metadata anyway, and we already fetch plenty of other metadata, 
	// I don't believe that fetching it here instead of transferring it over
	// is going to slow things down at all, and
	// in any case will not be where the primary optimizations lie...

        ULONG size;
        IfFailRet( module->m_pIMImport->GetFieldProps(fldToken, NULL, NULL, 0, NULL, NULL,
                                                     &(pFieldData->fldFullSig),
                                                     &size,
                                                     NULL, NULL, NULL) );
        // Point past the calling convention
#ifdef _DEBUG
        CorCallingConvention conv = (CorCallingConvention)
#endif
        CorSigUncompressData(pFieldData->fldFullSig);
        _ASSERTE(conv == IMAGE_CEE_CS_CALLCONV_FIELD);

    return hr;
}

// ****** DON'T CALL THIS WITHOUT FIRST CALLING object->IsValid !!!!!!! ******
// object is NULL if this is being called from GetStaticFieldValue
HRESULT CordbClass::GetSyncBlockField(mdFieldDef fldToken, 
                                      DebuggerIPCE_FieldData **ppFieldData,
                                      CordbObjectValue *object)
{
    HRESULT hr = S_OK;
    _ASSERTE(object == NULL || object->m_fIsValid); 
            // What we really want to assert is that
            // IsValid has been called, if this is for an instance value

	if (m_typarCount > 0) 
	{
		_ASSERTE(!"sync block field not yet implemented on constructed types!");
		return E_FAIL;
	}

    BOOL fStatic = (object == NULL);

    // Static stuff should _NOT_ be cleared, since they stick around.  Thus
    // the separate tables.

    // We must get new copies each time we call continue b/c we get the
    // actual Object ptr from the left side, which can move during a GC.
    
    DebuggerIPCE_FieldData *pInfo = NULL;
    if (!fStatic)
    {
        pInfo = object->m_syncBlockFieldsInstance.GetFieldInfo(fldToken);

        // We've found a previously located entry
        if (pInfo != NULL)
        {
            (*ppFieldData) = pInfo;
            return S_OK;
        }
    }
    else
    {
        pInfo = m_syncBlockFieldsStatic.GetFieldInfo(fldToken);

        // We've found a previously located entry
        if (pInfo != NULL)
        {
            (*ppFieldData) = pInfo;
            return S_OK;
        }
    }
    
    // We're not going to be able to get the instance-specific field
    // if we can't get the instance.
    if (!fStatic && object->m_info.objRefBad)
        return CORDBG_E_ENC_HANGING_FIELD;

    // Go get this particular field.
    DebuggerIPCEvent event;
    CordbProcess *process = GetModule()->GetProcess();
    _ASSERTE(process != NULL);

    process->InitIPCEvent(&event, 
                          DB_IPCE_GET_SYNC_BLOCK_FIELD, 
                          true, // two-way event
                          (void *)m_module->GetAppDomain()->m_id);
                          
	event.GetSyncBlockField.objectTypeData.debuggerModuleToken = (void *)GetModule()->m_id;
	hr = GetToken(&(event.GetSyncBlockField.objectTypeData.metadataToken));
    _ASSERTE(!FAILED(hr));
    event.GetSyncBlockField.fldToken = fldToken;

    if (fStatic)
    {
        event.GetSyncBlockField.staticVarBase = m_staticVarBase; // in case it's static.
        
        event.GetSyncBlockField.pObject = NULL;
		event.GetSyncBlockField.objectTypeData.elementType = ELEMENT_TYPE_MAX;
        event.GetSyncBlockField.offsetToVars = NULL;
    }
    else
    {
        _ASSERTE(object != NULL);
    
        event.GetSyncBlockField.pObject = (void *)object->m_id;
        event.GetSyncBlockField.objectTypeData.elementType = object->m_info.objTypeData.elementType;
        event.GetSyncBlockField.offsetToVars = object->m_info.objOffsetToVars;
        
        event.GetSyncBlockField.staticVarBase = NULL;
    }
    
    // Note: two-way event here...
    hr = process->m_cordb->SendIPCEvent(process, 
                                        &event,
                                        sizeof(DebuggerIPCEvent));

    // Stop now if we can't even send the event.
    if (!SUCCEEDED(hr))
        return hr;

    _ASSERTE(event.type == DB_IPCE_GET_SYNC_BLOCK_FIELD_RESULT);

    if (!SUCCEEDED(event.hr))
        return event.hr;

    _ASSERTE(pInfo == NULL);

    _ASSERTE( fStatic == event.GetSyncBlockFieldResult.fStatic );
    
    // Save the results for later.
    if(fStatic)
    {
        m_syncBlockFieldsStatic.AddFieldInfo(&(event.GetSyncBlockFieldResult.fieldData));
        pInfo = m_syncBlockFieldsStatic.GetFieldInfo(fldToken);

        // We've found a previously located entry.esove
        if (pInfo != NULL)
        {
            (*ppFieldData) = pInfo;
        }
    }
    else
    {
        object->m_syncBlockFieldsInstance.AddFieldInfo(&(event.GetSyncBlockFieldResult.fieldData));
        pInfo = object->m_syncBlockFieldsInstance.GetFieldInfo(fldToken);

        // We've found a previously located entry.esove
        if (pInfo != NULL)
        {
            (*ppFieldData) = pInfo;
        }
    }

    if (pInfo != NULL)
    {
        // It's important to do this here, once we've got the final memory blob for pInfo
        hr = GetFieldSig(GetModule(), fldToken, pInfo);
        return hr;
    }
    else
        return CORDBG_E_ENC_HANGING_FIELD;
}


HRESULT CordbClass::GetFieldInfo(mdFieldDef fldToken, DebuggerIPCE_FieldData **ppFieldData)
{
    HRESULT hr = S_OK;

    *ppFieldData = NULL;
    
    hr = Init(FALSE);

    if (!SUCCEEDED(hr))
        return hr;

	return SearchFieldInfo(GetModule(), m_instanceVarCount + m_staticVarCount, m_fields, m_token, fldToken, ppFieldData);
}


/* static */ HRESULT CordbClass::SearchFieldInfo(CordbModule *module, unsigned int cData, DebuggerIPCE_FieldData *data, mdTypeDef classToken, mdFieldDef fldToken, DebuggerIPCE_FieldData **ppFieldData)
{
	unsigned int i;

	HRESULT hr = S_OK;
    for (i = 0; i < cData; i++)
    {
        if (data[i].fldMetadataToken == fldToken)
        {
            if (!data[i].fldEnCAvailable)
            {
                return CORDBG_E_ENC_HANGING_FIELD; // caller should get instance-specific info.
            }
        
            if (data[i].fldFullSig == NULL)
            {
                hr = GetFieldSig(module, fldToken, &data[i]);
                if (FAILED(hr))
                    return hr;
            }

            *ppFieldData = &(data[i]);
            return S_OK;
        }
    }

    // Hmmm... we didn't find the field on this class. See if the field really belongs to this class or not.
    mdTypeDef classTok;
    
    hr = module->m_pIMImport->GetFieldProps(fldToken, &classTok, NULL, 0, NULL, NULL, NULL, 0, NULL, NULL, NULL);

    if (FAILED(hr))
        return hr;

    if (classTok == (mdTypeDef) classToken)
    {
        // Well, the field belongs in this class. The assumption is that the Runtime optimized the field away.
        return CORDBG_E_FIELD_NOT_AVAILABLE;
    }

    // Well, the field doesn't even belong to this class...
    return E_INVALIDARG;
}

/* ------------------------------------------------------------------------- *
 * Function class
 * ------------------------------------------------------------------------- */

CordbFunction::CordbFunction(CordbModule *m,
                             mdMethodDef funcMetadataToken,
                             SIZE_T funcRVA)
  : CordbBase(funcMetadataToken, enumCordbFunction),
    m_module(m),
    m_class(NULL),
    m_jitinfos(1),
    m_token(funcMetadataToken),
    m_functionRVA(funcRVA),
    m_nVersionLastNativeInfo(0),
    m_methodSig(NULL),
    m_argCount(0),
    m_isStatic(false),
    m_localsSig(NULL),
    m_localVarCount(0),
    m_localVarSigToken(mdSignatureNil),
    m_encCounterLastSynch(0),
    m_nVersionMostRecentEnC(0), 
    m_isNativeImpl(false)
{
}



/*
    A list of which resources owned by this object are accounted for.

    UNKNOWN:
        PCCOR_SIGNATURE          m_methodSig;
        PCCOR_SIGNATURE          m_localsSig;
        ICorJitInfo::NativeVarInfo *m_nativeInfo;           
        
    HANDLED:
        CordbModule             *m_module; // Assigned w/o AddRef()
        CordbClass              *m_class; // Assigned w/o AddRef()
*/

CordbFunction::~CordbFunction()
{
    if ( m_rgilCode.Table() != NULL)
        for (int i =0; i < m_rgilCode.Count();i++)
        {
            CordbCode * pCordbCode = m_rgilCode.Table()[i];
            pCordbCode->Release();
        }

}

// Neutered by CordbModule
void CordbFunction::Neuter()
{
    AddRef();
    {
        // Neuter any/all native CordbCode objects
        if ( m_rgilCode.Table() != NULL)
        {
            for (int i =0; i < m_rgilCode.Count();i++)
            {
                CordbCode * pCordbCode = m_rgilCode.Table()[i];
                pCordbCode->Neuter();
            }
        }
		NeuterAndClearHashtable(&m_jitinfos);
        
        CordbBase::Neuter();
    }
    Release();
}


HRESULT CordbFunction::LookupOrCreateFromFuncData(CordbProcess *pProcess, CordbAppDomain *pAppDomain, DebuggerIPCE_FuncData *data, CordbFunction **ppRes) 
{
	CordbModule* pFunctionModule = pAppDomain->LookupModule(data->funcDebuggerModuleToken);
	_ASSERTE(pFunctionModule != NULL);

	// Does this function already exist?
	CordbFunction *pFunction = NULL;
        
	HRESULT hr = S_OK;

	pFunction = pFunctionModule->LookupFunction(data->funcMetadataToken);

	if (pFunction == NULL)
	{
		// New function. Go ahead and create it.
		hr = pFunctionModule->CreateFunction(data->funcMetadataToken,
			data->funcRVA,
			&pFunction);

		_ASSERTE( SUCCEEDED(hr) || !"FAILURE" );
		if (!SUCCEEDED(hr))
			return hr;

		pFunction->SetLocalVarToken(data->localVarSigToken);
	}

	_ASSERTE(pFunction != NULL);

	// Does this function have a class?
	if ((pFunction->m_class == NULL) && (data->classMetadataToken != mdTypeDefNil))
	{
		// No. Go ahead and create the class.
		CordbModule* pClassModule = pAppDomain->LookupModule(data->funcDebuggerModuleToken);
		_ASSERTE(pClassModule != NULL);

		// Does the class already exist?
		CordbClass *pClass;
		hr = pClassModule->LookupOrCreateClass(data->classMetadataToken,
			&pClass);
		_ASSERTE(SUCCEEDED(hr) || !"FAILURE");

		if (!SUCCEEDED(hr)) 
			return hr;

		_ASSERTE(pClass != NULL);
		pFunction->m_class = pClass;
	}
	*ppRes = pFunction;
	return hr;

}

HRESULT CordbFunction::QueryInterface(REFIID id, void **pInterface)
{
    if (id == IID_ICorDebugFunction)
        *pInterface = (ICorDebugFunction*)this;
    else if (id == IID_IUnknown)
        *pInterface = (IUnknown*)(ICorDebugFunction*)this;
    else
    {
        *pInterface = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

// if nVersion == const int DMI_VERSION_MOST_RECENTLY_JITTED,
// get the highest-numbered version.  Otherwise,
// get the version asked for.
CordbCode *UnorderedCodeArrayGet( UnorderedCodeArray *pThis, SIZE_T nVersion )
{
#ifdef LOGGING
    if (nVersion == (SIZE_T) DMI_VERSION_MOST_RECENTLY_JITTED)
        LOG((LF_CORDB,LL_EVERYTHING,"Looking for DMI_VERSION_MOST_"
            "RECENTLY_JITTED\n"));
    else
        LOG((LF_CORDB,LL_EVERYTHING,"Looking for ver 0x%x\n", nVersion));
#endif //LOGGING
        
    if (pThis->Table() != NULL)
    {
        CordbCode *pCode = *pThis->Table();
        CordbCode *pCodeMax = pCode;
        USHORT cCode;
        USHORT i;
        for(i = 0,cCode=pThis->Count(); i <cCode; i++)
        {
            pCode = (pThis->Table())[i];
            if (nVersion == (SIZE_T) DMI_VERSION_MOST_RECENTLY_JITTED )
            {
                if (pCode->m_nVersion > pCodeMax->m_nVersion)
                {   
                    pCodeMax = pCode;
                }
            }
            else if (pCode->m_nVersion == nVersion)
            {
                LOG((LF_CORDB,LL_EVERYTHING,"Found ver 0x%x\n", nVersion));
                return pCode;
            }
        }

        if (nVersion == (SIZE_T) DMI_VERSION_MOST_RECENTLY_JITTED )
        {
#ifdef LOGGING
            if (pCodeMax != NULL )
                LOG((LF_CORDB,LL_INFO10000,"Found 0x%x, ver 0x%x as "
                    "most recent\n",pCodeMax,pCodeMax->m_nVersion));
#endif //LOGGING
            return pCodeMax;
        }
    }

    return NULL;
}

HRESULT UnorderedCodeArrayAdd( UnorderedCodeArray *pThis, CordbCode *pCode )
{
    CordbCode **ppCodeNew =pThis->Append();

    if (NULL == ppCodeNew)
        return E_OUTOFMEMORY;

    *ppCodeNew = pCode;
    
    // This ref is freed whenever the code array we are storing is freed.
    pCode->AddRef();
    return S_OK;
}


HRESULT CordbFunction::GetModule(ICorDebugModule **ppModule)
{
    VALIDATE_POINTER_TO_OBJECT(ppModule, ICorDebugModule **);

    HRESULT hr = UpdateToMostRecentEnCVersion();
    if (FAILED(hr))
        return hr;
    
    *ppModule = (ICorDebugModule*) m_module;
    (*ppModule)->AddRef();

    return hr;
}

HRESULT CordbFunction::GetClass(ICorDebugClass **ppClass)
{
    VALIDATE_POINTER_TO_OBJECT(ppClass, ICorDebugClass **);
    
    *ppClass = NULL;
    
    INPROC_LOCK();

    HRESULT hr = UpdateToMostRecentEnCVersion();
    if (FAILED(hr))
        return hr;
    
    if (m_class == NULL)
    {
        // We're not looking for any particular version, just
        // the class info.  This seems like the best version to request
        hr = Populate(DMI_VERSION_MOST_RECENTLY_JITTED);

        if (FAILED(hr))
            goto LExit;
    }

    *ppClass = (ICorDebugClass*) m_class;

LExit:
    INPROC_UNLOCK();

    if (FAILED(hr))
        return hr;

    if (*ppClass)
    {
        (*ppClass)->AddRef();
        return S_OK;
    }
    else
        return S_FALSE;
}

HRESULT CordbFunction::GetToken(mdMethodDef *pMemberDef)
{
    VALIDATE_POINTER_TO_OBJECT(pMemberDef, mdMethodDef *);

    HRESULT hr = UpdateToMostRecentEnCVersion();

    if (FAILED(hr))
        return hr;
    
    *pMemberDef = m_token;
    return S_OK;
}

HRESULT CordbFunction::GetILCode(ICorDebugCode **ppCode)
{
    VALIDATE_POINTER_TO_OBJECT(ppCode, ICorDebugCode **);

    INPROC_LOCK();

    HRESULT hr = UpdateToMostRecentEnCVersion();
    if (FAILED(hr))
        return hr;
    
    CordbCode *pCode = NULL;
    hr = GetCodeByVersion(TRUE, DMI_VERSION_MOST_RECENTLY_JITTED, &pCode);
    *ppCode = (ICorDebugCode*)pCode;

    INPROC_UNLOCK();
    
    return hr;
}

// <REVIEW> GENERICS: this gets a pretty much random version of the native code when the
// function is a generic method that gets JITted more than once.  The really correct thing
// to do is have an enumerator that allows the user to visit all of the native code's
// corresponding to a single IL method. But many V1 BVTs and the VS debugger currently
// expect to be able to get native code for non-generic methods, so we'll implement
// this by just choosing an appropriate looking native code blob.</REVIEW>
HRESULT CordbFunction::GetNativeCode(ICorDebugCode **ppCode)
{
    VALIDATE_POINTER_TO_OBJECT(ppCode, ICorDebugCode **);

    INPROC_LOCK();

    HRESULT hr = UpdateToMostRecentEnCVersion();
    if (FAILED(hr))
        return hr;

	CordbCode *pCode = NULL;
	HASHFIND srch;
    for (CordbJITInfo *ji = (CordbJITInfo *)m_jitinfos.FindFirst(&srch); 
		  ji != NULL;
		  (ji = (CordbJITInfo *)m_jitinfos.FindNext(&srch)))
	{ 
        if (pCode == NULL) 
		{
			pCode = ji->m_nativecode;
			continue;
		}
		if (ji->m_nativecode->m_nVersion > pCode->m_nVersion)
			pCode = ji->m_nativecode;
	}

    if (pCode == NULL)
	{
        hr = CORDBG_E_CODE_NOT_AVAILABLE;
	}
	else 
	{
        pCode->AddRef();
        *ppCode = pCode;
		hr = S_OK;
    }

    INPROC_UNLOCK();
    
    return hr;
}

HRESULT CordbFunction::GetCodeByVersion(BOOL fGetIfNotPresent,
                                        SIZE_T nVer, CordbCode **ppCode)
{
    VALIDATE_POINTER_TO_OBJECT(ppCode, ICorDebugCode **);

    _ASSERTE(*ppCode == NULL && "Common source of errors is getting addref'd copy here and never Release()ing it");
    *ppCode = NULL;

    // Its okay to do this if the process is not sync'd.
    CORDBRequireProcessStateOK(GetProcess());

    HRESULT hr = S_OK;
    CordbCode *pCode = NULL;

    LOG((LF_CORDB, LL_EVERYTHING, "Asked to find code ver 0x%x\n", nVer));

    if ((pCode = UnorderedCodeArrayGet(&m_rgilCode, nVer)) == NULL &&
        fGetIfNotPresent)
        hr = Populate(nVer);

    if (SUCCEEDED(hr) && pCode == NULL)
        pCode=UnorderedCodeArrayGet(&m_rgilCode, nVer);

    if (pCode != NULL)
    {
        pCode->AddRef();
        *ppCode = pCode;
    }
    
    return hr;
}

HRESULT CordbFunction::CreateBreakpoint(ICorDebugFunctionBreakpoint **ppBreakpoint)
{
#ifndef RIGHT_SIDE_ONLY
    return CORDBG_E_INPROC_NOT_IMPL;
#else
    HRESULT hr = S_OK;

    VALIDATE_POINTER_TO_OBJECT(ppBreakpoint, ICorDebugFunctionBreakpoint **);

    hr = UpdateToMostRecentEnCVersion();
    if (FAILED(hr))
        return hr;

    ICorDebugCode *pCode = NULL;

    // Use the IL code so that we stop after the prolog
    hr = GetILCode(&pCode);
    
    if (FAILED(hr))
        goto LError;

    hr = pCode->CreateBreakpoint(0, ppBreakpoint);

LError:
    if (pCode != NULL)
        pCode->Release();

    return hr;
#endif //RIGHT_SIDE_ONLY    
}

HRESULT CordbFunction::GetLocalVarSigToken(mdSignature *pmdSig)
{
    VALIDATE_POINTER_TO_OBJECT(pmdSig, mdSignature *);
    
#ifdef RIGHT_SIDE_ONLY
    CORDBRequireProcessStateOKAndSync(GetProcess(), GetAppDomain());
#else 
    // For the Virtual Right Side (In-proc debugging), we'll
    // always be synched, but not neccessarily b/c we've
    // gotten a synch message.
    CORDBRequireProcessStateOK(GetProcess());
#endif    
    HRESULT hr = UpdateToMostRecentEnCVersion();
    if (FAILED(hr))
        return hr;

    *pmdSig = m_localVarSigToken;

    return S_OK;
}


HRESULT CordbFunction::GetCurrentVersionNumber(ULONG32 *pnCurrentVersion)
{
    VALIDATE_POINTER_TO_OBJECT(pnCurrentVersion, ULONG32 *);

    INPROC_LOCK();

    HRESULT hr = UpdateToMostRecentEnCVersion();
    if (FAILED(hr))
        return hr;

    CordbCode *pCode = NULL;
    hr = GetCodeByVersion(TRUE, DMI_VERSION_MOST_RECENTLY_EnCED, &pCode);
    
    if (FAILED(hr))
        goto LError;

    (*pnCurrentVersion) = INTERNAL_TO_EXTERNAL_VERSION(m_nVersionMostRecentEnC);
    _ASSERTE((*pnCurrentVersion) >= USER_VISIBLE_FIRST_VALID_VERSION_NUMBER);
    
LError:
    if (pCode != NULL)
        pCode->Release();

    INPROC_UNLOCK();

    return hr;
}


HRESULT CordbFunction::CreateCode(REMOTE_PTR startAddress,
                                  SIZE_T size, CordbCode** ppCode,
                                  SIZE_T nVersion, void* ilCodeVersionToken)
{
    _ASSERTE(ppCode != NULL);

    *ppCode = NULL;
    
    CordbCode* pCode = new CordbCode(this, startAddress, size,
                                     nVersion, ilCodeVersionToken);

    if (pCode == NULL)
        return E_OUTOFMEMORY;

    HRESULT hr = S_OK;
    
    hr = UnorderedCodeArrayAdd( &m_rgilCode, pCode);

    if (FAILED(hr))
    {
        delete pCode;
        return hr;
    }

    pCode->AddRef();
    *ppCode = pCode;

    return S_OK;
}

HRESULT CordbFunction::Populate( SIZE_T nVersion)
{
    HRESULT hr = S_OK;
    CordbProcess* pProcess = m_module->m_process;

    _ASSERTE(m_token != mdMethodDefNil);

    // Bail now if we've already discovered that this function is implemented natively as part of the Runtime.
    if (m_isNativeImpl)
        return CORDBG_E_FUNCTION_NOT_IL;

    // Figure out if this function is implemented as a native part of the Runtime. If it is, then this ICorDebugFunction
    // is just a container for certian Right Side bits of info, i.e., module, class, token, etc.
    DWORD attrs;
    DWORD implAttrs;
    ULONG ulRVA;
	BOOL	isDynamic;

    IfFailRet( GetModule()->m_pIMImport->GetMethodProps(m_token, NULL, NULL, 0, NULL,
                                     &attrs, NULL, NULL, &ulRVA, &implAttrs) );
	IfFailRet( GetModule()->IsDynamic(&isDynamic) );

	// A method has associated IL if it's RVA is non-zero unless it is a dynamic module
    if (IsMiNative(implAttrs) || (isDynamic == FALSE && ulRVA == 0))
    {
        m_isNativeImpl = true;
        return CORDBG_E_FUNCTION_NOT_IL;
    }

    // Make sure the Left Side is running free before trying to send an event to it.
    CORDBSyncFromWin32StopIfStopped(pProcess);

    // Send the get function data event to the RC.
    DebuggerIPCEvent event;
    pProcess->InitIPCEvent(&event, DB_IPCE_GET_FUNCTION_DATA, true, (void *)(m_module->GetAppDomain()->m_id));
    event.GetFunctionData.funcMetadataToken = m_token;
    event.GetFunctionData.funcDebuggerModuleToken = m_module->m_debuggerModuleToken;
    event.GetFunctionData.nVersion = nVersion;

    _ASSERTE(m_module->m_debuggerModuleToken != NULL);

    // Note: two-way event here...
    hr = pProcess->m_cordb->SendIPCEvent(pProcess, &event, sizeof(DebuggerIPCEvent));

    // Stop now if we can't even send the event.
    if (!SUCCEEDED(hr))
        return hr;

    if (!SUCCEEDED(event.hr))
        return event.hr;

    _ASSERTE(event.type == DB_IPCE_GET_FUNCTION_DATA_RESULT);

    // Cache the most recently EnC'ed version number
    m_nVersionMostRecentEnC = event.FunctionData.basicData.nVersionMostRecentEnC;

    // Fill in the proper function data.
    m_functionRVA = event.FunctionData.basicData.funcRVA;
    
    CordbCode *pCodeTemp = NULL;

    // Should we make or fill in some class data for this function?
    if ((m_class == NULL) && (event.FunctionData.basicData.classMetadataToken != mdTypeDefNil))
    {
        CordbAssembly *pAssembly = m_module->GetCordbAssembly();
        CordbModule* pClassModule = pAssembly->m_pAppDomain->LookupModule(event.FunctionData.basicData.funcDebuggerModuleToken);
        _ASSERTE(pClassModule != NULL);

        CordbClass *pClass;
        hr = pClassModule->LookupOrCreateClass(event.FunctionData.basicData.classMetadataToken, &pClass);

        if (!SUCCEEDED(hr))
            goto exit;
                
        _ASSERTE(pClass != NULL);
        m_class = pClass;
    }

    // Do we need to make any code objects for this function?
    LOG((LF_CORDB,LL_INFO10000,"R:CF::Pop: looking for IL code, version 0x%x\n", event.FunctionData.basicData.ilnVersion));
        
    if ((UnorderedCodeArrayGet(&m_rgilCode, event.FunctionData.basicData.ilnVersion) == NULL) &&
        (event.FunctionData.basicData.ilStartAddress != 0))
    {
        LOG((LF_CORDB,LL_INFO10000,"R:CF::Pop: not found, creating...\n"));
        _ASSERTE((unsigned)DMI_VERSION_INVALID != event.FunctionData.basicData.ilnVersion);
        
        hr = CreateCode(event.FunctionData.basicData.ilStartAddress,
                        event.FunctionData.basicData.ilSize,
                        &pCodeTemp, event.FunctionData.basicData.ilnVersion,
                        event.FunctionData.basicData.CodeVersionToken);

        if (!SUCCEEDED(hr))
            goto exit;
    }
    
    SetLocalVarToken(event.FunctionData.basicData.localVarSigToken);

	// <REVIEW>To make sure we don't break V1 behaviour, e.g. GetNativeCode,
	// we allow for CordbFunction's to be associated
	// with at least one JIT blob.  We could implement an iterator to go and fetch all the
	// native code blobs and then just choose one, but we may as well keep it simple for the
	// moment and just return one specified here.</REVIEW>
    
	if (event.FunctionData.possibleNativeData.nativeCodeVersionToken != NULL)
	{
		CordbJITInfo *pInfo = NULL;
		hr = CordbJITInfo::LookupOrCreateFromJITData(this, &event.FunctionData.basicData, &event.FunctionData.possibleNativeData, &pInfo);
		if (!SUCCEEDED(hr))
			goto exit;
	}

exit:
    if (pCodeTemp)
        pCodeTemp->Release();

    return hr;
}


HRESULT CordbJITInfo::LookupOrCreateFromJITData(CordbFunction *pFunction, DebuggerIPCE_FuncData *currentFuncData, DebuggerIPCE_JITFuncData* currentJITFuncData, CordbJITInfo **ppRes)
{
	_ASSERTE(ppRes != NULL);
	HRESULT hr = S_OK;

    *ppRes = (CordbJITInfo *)pFunction->m_jitinfos.GetBase((UINT_PTR) currentJITFuncData->nativeCodeVersionToken);

	if (*ppRes == NULL)
	{
		LOG((LF_CORDB,LL_INFO10000,"R:CT::RSCreating code w/ ver:0x%x, token:0x%x\n",
			currentFuncData->ilnVersion,
			currentJITFuncData->nativeCodeVersionToken));

		CordbCode *pCode = 
			new CordbCode(pFunction, currentJITFuncData->nativeStartAddressPtr, 
			currentJITFuncData->nativeSize,
			currentFuncData->ilnVersion, 
			currentFuncData->CodeVersionToken,
			currentJITFuncData->nativeCodeVersionToken,
			currentJITFuncData->ilToNativeMapAddr, 
			currentJITFuncData->ilToNativeMapSize);

		if (pCode == NULL) 
		{
			hr = E_OUTOFMEMORY;
			return hr;
		}

		*ppRes = new CordbJITInfo(pFunction, pCode);

		if (*ppRes == NULL) 
		{
			delete pCode;
			hr = E_OUTOFMEMORY;
			return hr;
		}
		hr = pFunction->m_jitinfos.AddBase(*ppRes);
		if (FAILED(hr)) 
		  {
		    delete pCode;
		    delete *ppRes;
		    return hr;
		  }

	}
	_ASSERTE(*ppRes != NULL);

	return hr;
}

//
// LoadNativeInfo loads from the left side any native variable info
// from the JIT.
//
HRESULT CordbJITInfo::LoadNativeInfo(void)
{
    HRESULT hr = S_OK;

    // Then, if we've either never done this before (no info), or we have, but the version number has increased, we
    // should try and get a newer version of our JIT info.
    if(m_nativeInfoValid)
        return S_OK;

    // You can't do this if the function is implemented as part of the Runtime.
    if (m_function->m_isNativeImpl)
        return CORDBG_E_FUNCTION_NOT_IL;

    DebuggerIPCEvent *retEvent = NULL;
    bool wait = true;
#ifndef RIGHT_SIDE_ONLY
    bool fFirstEvent = true;
#endif

    // We might be here b/c we've done some EnCs, but we also may have pitched some code, so don't overwrite this until
    // we're sure we've got a good replacement.
    unsigned int argumentCount = 0;
    unsigned int nativeInfoCount = 0;
    unsigned int nativeInfoCountTotal = 0;
    ICorJitInfo::NativeVarInfo *nativeInfo = NULL;
    
    CORDBSyncFromWin32StopIfStopped(GetProcess());

    INPROC_LOCK();

    // We've got a remote address that points to the EEClass.  We need to send to the left side to get real information
    // about the class, including its instance and static variables.
    CordbProcess *pProcess = GetProcess();

    DebuggerIPCEvent event;
    pProcess->InitIPCEvent(&event, DB_IPCE_GET_JIT_INFO, false, (void *)(GetAppDomain()->m_id));
    event.GetJITInfo.nativeCodeVersionToken = m_nativecode->m_nativeCodeVersionToken;

    hr = pProcess->m_cordb->SendIPCEvent(pProcess, &event, sizeof(DebuggerIPCEvent));

    // Stop now if we can't even send the event.
    if (!SUCCEEDED(hr))
        goto exit;

    // Wait for events to return from the RC. We expect at least one jit info result event.
    retEvent = (DebuggerIPCEvent *) _alloca(CorDBIPC_BUFFER_SIZE);
    
    while (wait)
    {
        unsigned int currentInfoCount = 0;

#ifdef RIGHT_SIDE_ONLY
        hr = pProcess->m_cordb->WaitForIPCEventFromProcess(pProcess, GetAppDomain(), retEvent);
#else
        if (fFirstEvent)
        {
            hr = pProcess->m_cordb->GetFirstContinuationEvent(pProcess,retEvent);
            fFirstEvent = false;
        }
        else
        {
            hr = pProcess->m_cordb->GetNextContinuationEvent(pProcess,retEvent);
        }
#endif //RIGHT_SIDE_ONLY
        
        if (!SUCCEEDED(hr))
            goto exit;
        
        _ASSERTE(retEvent->type == DB_IPCE_GET_JIT_INFO_RESULT);

        // If this is the first event back from the RC, then create the array to hold the data.
        if ((retEvent->GetJITInfoResult.totalNativeInfos > 0) && (nativeInfo == NULL))
        {
            argumentCount = retEvent->GetJITInfoResult.argumentCount;
            nativeInfoCountTotal = retEvent->GetJITInfoResult.totalNativeInfos;

            nativeInfo = new ICorJitInfo::NativeVarInfo[nativeInfoCountTotal];

            if (nativeInfo == NULL)
            {
                hr = E_OUTOFMEMORY;
                goto exit;
            }
        }

        ICorJitInfo::NativeVarInfo *currentNativeInfo = &(retEvent->GetJITInfoResult.nativeInfo);

        while (currentInfoCount++ < retEvent->GetJITInfoResult.nativeInfoCount)
        {
            nativeInfo[nativeInfoCount] = *currentNativeInfo;
            
            currentNativeInfo++;
            nativeInfoCount++;
        }

        if (nativeInfoCount >= nativeInfoCountTotal)
            wait = false;
    }

    if (m_nativeInfo != NULL)
    {
        delete [] m_nativeInfo;
        m_nativeInfo = NULL;
    }
    
    m_nativeInfo = nativeInfo;
    m_argumentCount = argumentCount;
    m_nativeInfoCount = nativeInfoCount;
    m_nativeInfoValid = true;
    
exit:

#ifndef RIGHT_SIDE_ONLY    
    GetProcess()->ClearContinuationEvents();
#endif    

    INPROC_UNLOCK();

    return hr;
}


HRESULT CordbFunction::LoadSig( void )
{
    HRESULT hr = S_OK;

    INPROC_LOCK();

    if (m_methodSig == NULL)
    {
        DWORD methodAttr = 0;
        ULONG sigBlobSize = 0;
        
        hr = GetModule()->m_pIMImport->GetMethodProps(
                               m_token, NULL, NULL, 0, NULL,            
                               &methodAttr, &m_methodSig, &sigBlobSize,     
                               NULL, NULL);

        if (FAILED(hr))
            goto exit;
        
        // Run past the calling convetion, then get the
        // arg count, and return type   
        ULONG cb = 0;
        cb += _skipMethodSignatureHeader(m_methodSig, &m_argCount);

        m_methodSig = &m_methodSig[cb];
        m_methodSigSize = sigBlobSize - cb;

        // If this function is not static, then we've got one extra arg.
        m_isStatic = (methodAttr & mdStatic) != 0;

        if (!m_isStatic)
            m_argCount++;
    }

exit:
    INPROC_UNLOCK();

    return hr;
}

//
// Figures out if an EnC has happened since the last time we were updated, and
// if so, updates all the fields of this CordbFunction so that everything
// is up-to-date.
//
HRESULT CordbFunction::UpdateToMostRecentEnCVersion(void)
{
    HRESULT hr = S_OK;

#ifdef RIGHT_SIDE_ONLY
    if (m_isNativeImpl)
        m_encCounterLastSynch = m_module->GetProcess()->m_EnCCounter;

    if (m_encCounterLastSynch < m_module->GetProcess()->m_EnCCounter)
    {
        hr = Populate(DMI_VERSION_MOST_RECENTLY_EnCED);

        if (FAILED(hr) && hr != CORDBG_E_FUNCTION_NOT_IL)
            return hr;

        // These 'signatures' are actually sub-signatures whose memory is owned
        // by someone else.  We don't delete them in the Dtor, so don't 
        // delete them here, either.
        // Get rid of these so that Load(LocalVar)Sig will re-get them.
        m_methodSig = NULL;
        m_localsSig = NULL;
        
        hr = LoadSig();
        if (FAILED(hr))
            return hr;

        if (!m_isNativeImpl)
        {
            hr = LoadLocalVarSig();
            if (FAILED(hr))
                return hr;
        }

        m_encCounterLastSynch = m_module->GetProcess()->m_EnCCounter;
    }
#endif

    return hr;
}

//
// Given an IL argument number, return its type.
//
HRESULT CordbFunction::GetArgumentType(DWORD dwIndex,
									   const Instantiation &inst,
                                      CordbType **res)
{
    HRESULT hr = S_OK;
    ULONG cb;

    // Load the method's signature if necessary.
    if (m_methodSig == NULL)
    {
        hr = LoadSig();
        if( !SUCCEEDED( hr ) )
            return hr;
    }

    // Check the index
    if (dwIndex >= m_argCount)
        return E_INVALIDARG;

    if (!m_isStatic)
        if (dwIndex == 0)
        {
            // Return the signature for the 'this' pointer for the
            // class this method is in.
            return m_class->GetThisType(inst, res);
        }
        else
            dwIndex--;
    
    cb = 0;
    
    // Run the signature and find the required argument.
    for (unsigned int i = 0; i < dwIndex; i++)
        cb += _skipTypeInSignature(&m_methodSig[cb]);

    //Get rid of funky modifiers
    cb += _skipFunkyModifiersInSignature(&m_methodSig[cb]);

    hr = CordbType::SigToType(m_module, &m_methodSig[cb], inst, res);
    
    return hr;
}

//
// Set the info needed to build a local var signature for this function.
//
void CordbFunction::SetLocalVarToken(mdSignature localVarSigToken)
{
    m_localVarSigToken = localVarSigToken;
}


#include "corpriv.h"

//
// LoadLocalVarSig loads the local variable signature from the token
// passed over from the Left Side.
//
HRESULT CordbFunction::LoadLocalVarSig(void)
{
    HRESULT hr = S_OK;
    
    INPROC_LOCK();

    if ((m_localsSig == NULL) && (m_localVarSigToken != mdSignatureNil))
    {
        hr = GetModule()->m_pIMImport->GetSigFromToken(m_localVarSigToken,
                                                       &m_localsSig,
                                                       &m_localsSigSize);

        if (FAILED(hr))
            goto Exit;

        _ASSERTE(*m_localsSig == IMAGE_CEE_CS_CALLCONV_LOCAL_SIG);
        m_localsSig++;
        --m_localsSigSize;

        // Snagg the count of locals in the sig.
        m_localVarCount = CorSigUncompressData(m_localsSig);
    }

Exit:
    INPROC_UNLOCK();
    
    return hr;
}

//
// Given an IL variable number, return its type.
//
HRESULT CordbFunction::GetLocalVariableType(DWORD dwIndex,
                                            const Instantiation &inst,
											CordbType **res)
{
    HRESULT hr = S_OK;
    ULONG cb;

    // Load the method's signature if necessary.
    if (m_localsSig == NULL)
    {
        hr = Populate(DMI_VERSION_MOST_RECENTLY_JITTED);

        if (FAILED(hr))
            return hr;
        
        hr = LoadLocalVarSig();

        if (FAILED(hr))
            return hr;
    }

    // Check the index
    if (dwIndex >= m_localVarCount)
        return E_INVALIDARG;

    cb = 0;
    
    // Run the signature and find the required argument.
    for (unsigned int i = 0; i < dwIndex; i++)
        cb += _skipTypeInSignature(&m_localsSig[cb]);

    cb += _skipFunkyModifiersInSignature(&m_localsSig[cb]);

    hr = CordbType::SigToType(m_module, &m_localsSig[cb], inst, res);

    return hr;
}

HRESULT CordbFunction::LookupJITInfo( void *nativeCodeVersionToken, CordbJITInfo **ppInfo )
{
    *ppInfo = (CordbJITInfo *)m_jitinfos.GetBase((UINT_PTR) nativeCodeVersionToken);
	return S_OK;
}



/* ------------------------------------------------------------------------- *
 * FunctionAsNative class: one is created for each realisation (i.e. blob of
 * native code, e.g. there may be many because of code-generation for
 * generic methods or for other reasons) of each version of each function.
 * ------------------------------------------------------------------------- */

CordbJITInfo::CordbJITInfo(CordbFunction *f, CordbCode *code)
  : CordbBase((UINT_PTR) code->m_nativeCodeVersionToken, enumCordbJITInfo),
    m_function(f),
    m_nativecode(code),
    m_nativeInfoCount(0),
    m_nativeInfo(NULL),
    m_nativeInfoValid(0),
    m_argumentCount(0)
{
    if (m_nativecode)
        m_nativecode->AddRef();
}


/*
    A list of which resources owened by this object are accounted for.

    UNKNOWN:
        
    HANDLED:
	   m_nativeInfo  Local array - freed in ~CordbJITInfo
	   m_nativecode   Owned by this object.  Neutered in Neuter()
*/

CordbJITInfo::~CordbJITInfo()
{
    if (m_nativeInfo != NULL)
        delete [] m_nativeInfo;
}

// Neutered by CordbFunction
void CordbJITInfo::Neuter()
{
    AddRef();
    {
        CordbBase::Neuter();
        m_nativecode->Neuter();
        m_nativecode->Release();
    }
    Release();
}

HRESULT CordbJITInfo::QueryInterface(REFIID id, void **pInterface)
{
    if (id == IID_IUnknown)
        *pInterface = (IUnknown*)(ICorDebugFunction*)this;
    else
    {
        *pInterface = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

//
// Given an IL local variable number and a native IP offset, return the
// location of the variable in jitted code.
//
HRESULT CordbJITInfo::ILVariableToNative(DWORD dwIndex,
                                          SIZE_T ip,
                                          ICorJitInfo::NativeVarInfo **ppNativeInfo)
{
    _ASSERTE(m_nativeInfoValid);
    
    return FindNativeInfoInILVariableArray(dwIndex,
                                           ip,
                                           ppNativeInfo,
                                           m_nativeInfoCount,
                                           m_nativeInfo);
}


/* ------------------------------------------------------------------------- *
 * Code class
 * ------------------------------------------------------------------------- */

// To make IL code:
CordbCode::CordbCode(CordbFunction *m, REMOTE_PTR startAddress,
                     SIZE_T size, SIZE_T nVersion, void *CodeVersionToken)
  : CordbBase(0, enumCordbCode),
    m_function(m),
    m_isIL(true), 
    m_address(startAddress),
    m_size(size),
    m_nVersion(nVersion),
    m_rgbCode(NULL),
    m_continueCounterLastSync(0),
    m_ilCodeVersionToken(CodeVersionToken),
    m_nativeCodeVersionToken(NULL),
    m_ilToNativeMapAddr(NULL),
    m_ilToNativeMapSize(0)
{
}

// To make native code:
CordbCode::CordbCode(CordbFunction *m, REMOTE_PTR startAddress,
                     SIZE_T size, SIZE_T nVersion, void *ilCodeVersionToken, void *nativeCodeVersionToken,
                     REMOTE_PTR ilToNativeMapAddr, SIZE_T ilToNativeMapSize)
  : CordbBase(0, enumCordbCode),
    m_function(m),
    m_isIL(false), 
    m_address(startAddress),
    m_size(size),
    m_nVersion(nVersion),
    m_ilCodeVersionToken(ilCodeVersionToken),
    m_nativeCodeVersionToken(nativeCodeVersionToken),
    m_ilToNativeMapAddr(ilToNativeMapAddr),
    m_ilToNativeMapSize(ilToNativeMapSize)
{
}

CordbCode::~CordbCode()
{
    if (m_rgbCode != NULL)
        delete [] m_rgbCode;
}

// Neutered by CordbFunction
void CordbCode::Neuter()
{
    AddRef();
    {
        CordbBase::Neuter();
    }
    Release();
}

HRESULT CordbCode::QueryInterface(REFIID id, void **pInterface)
{
    if (id == IID_ICorDebugCode)
        *pInterface = (ICorDebugCode*)this;
    else if (id == IID_IUnknown)
        *pInterface = (IUnknown*)(ICorDebugCode*)this;
    else
    {
        *pInterface = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

HRESULT CordbCode::IsIL(BOOL *pbIL)
{
    VALIDATE_POINTER_TO_OBJECT(pbIL, BOOL *);
    
    *pbIL = m_isIL;

    return S_OK;
}


HRESULT CordbCode::GetFunction(ICorDebugFunction **ppFunction)
{
    VALIDATE_POINTER_TO_OBJECT(ppFunction, ICorDebugFunction **);
    
    *ppFunction = (ICorDebugFunction*) m_function;
    (*ppFunction)->AddRef();

    return S_OK;
}

HRESULT CordbCode::GetAddress(CORDB_ADDRESS *pStart)
{
    VALIDATE_POINTER_TO_OBJECT(pStart, CORDB_ADDRESS *);
    
    // Native can be pitched, and so we have to actually
    // grab the address from the left side, whereas the
    // IL code address doesn't change.
    if (m_isIL )
    {
        *pStart = PTR_TO_CORDB_ADDRESS(m_address);
    }
    else
    {

        _ASSERTE( this != NULL );
        _ASSERTE( this->m_function != NULL );
        _ASSERTE( this->m_function->m_module != NULL );
        _ASSERTE( this->m_function->m_module->m_process != NULL );

        if (m_address != NULL)
        {
            DWORD dwRead = 0;
            if ( 0 == ReadProcessMemoryI( m_function->m_module->m_process->m_handle,
                    m_address, pStart, sizeof(CORDB_ADDRESS),&dwRead))
            {
                *pStart = NULL;
                return HRESULT_FROM_WIN32(GetLastError());
            }
        }

        // If the address was zero'd out on the left side, then
        // the code has been pitched & isn't available.
        if ((*pStart == NULL) || (m_address == NULL))
        {
            return CORDBG_E_CODE_NOT_AVAILABLE;
        }
    }
    return S_OK;
}

HRESULT CordbCode::GetSize(ULONG32 *pcBytes)
{
    VALIDATE_POINTER_TO_OBJECT(pcBytes, ULONG32 *);
    
    *pcBytes = m_size;
    return S_OK;
}

HRESULT CordbCode::CreateBreakpoint(ULONG32 offset, 
                                    ICorDebugFunctionBreakpoint **ppBreakpoint)
{
#ifndef RIGHT_SIDE_ONLY
    return CORDBG_E_INPROC_NOT_IMPL;
#else
    VALIDATE_POINTER_TO_OBJECT(ppBreakpoint, ICorDebugFunctionBreakpoint **);
    
    CordbFunctionBreakpoint *bp = new CordbFunctionBreakpoint(this, offset);

    if (bp == NULL)
        return E_OUTOFMEMORY;

    HRESULT hr = bp->Activate(TRUE);
    if (SUCCEEDED(hr))
    {
        *ppBreakpoint = (ICorDebugFunctionBreakpoint*) bp;
        bp->AddRef();
        return S_OK;
    }
    else
    {
        delete bp;
        return hr;
    }
#endif //RIGHT_SIDE_ONLY    
}

HRESULT CordbCode::GetCode(ULONG32 startOffset, 
                           ULONG32 endOffset,
                           ULONG32 cBufferAlloc,
                           BYTE buffer[],
                           ULONG32 *pcBufferSize)
{
    VALIDATE_POINTER_TO_OBJECT_ARRAY(buffer, BYTE, cBufferAlloc, true, true);
    VALIDATE_POINTER_TO_OBJECT(pcBufferSize, ULONG32 *);
    
    LOG((LF_CORDB,LL_EVERYTHING, "CC::GC: for token:0x%x\n", m_function->m_token));

    CORDBSyncFromWin32StopIfStopped(GetProcess());
#ifdef RIGHT_SIDE_ONLY
    CORDBRequireProcessStateOKAndSync(GetProcess(), GetAppDomain());
#else 
    // For the Virtual Right Side (In-proc debugging), we'll
    // always be synched, but not neccessarily b/c we've
    // gotten a synch message.
    CORDBRequireProcessStateOK(GetProcess());
#endif    
    INPROC_LOCK();

    HRESULT hr = S_OK;
    *pcBufferSize = 0;

    //
    // Check ranges.
    //

    if (cBufferAlloc < endOffset - startOffset)
        endOffset = startOffset + cBufferAlloc;

    if (endOffset > m_size)
        endOffset = m_size;

    if (startOffset > m_size)
        startOffset = m_size;

    if (m_rgbCode == NULL || 
        m_continueCounterLastSync < GetProcess()->m_continueCounter)
    {
        BYTE *rgbCodeOrCodeSnippet;
        ULONG32 start;
        ULONG32 end;
        ULONG cAlloc;

        if (m_continueCounterLastSync < GetProcess()->m_continueCounter &&
            m_rgbCode != NULL )
        {
            delete [] m_rgbCode;
        }
        
        m_rgbCode = new BYTE[m_size];
        if (m_rgbCode == NULL)
        {
            rgbCodeOrCodeSnippet = buffer;
            start = startOffset;
            end = endOffset;
            cAlloc = cBufferAlloc;
        }
        else
        {
            rgbCodeOrCodeSnippet = m_rgbCode;
            start = 0;
            end = m_size;
            cAlloc = m_size;
        }

        DebuggerIPCEvent *event = 
          (DebuggerIPCEvent *) _alloca(CorDBIPC_BUFFER_SIZE);

        //
        // Send event to get code.
        // !!! This assumes that we're currently synchronized.  
        //
        GetProcess()->InitIPCEvent(event,
                                   DB_IPCE_GET_CODE, 
                                   false,
                                   (void *)(GetAppDomain()->m_id));
        event->GetCodeData.funcMetadataToken = m_function->m_token;
        event->GetCodeData.funcDebuggerModuleToken =
            m_function->m_module->m_debuggerModuleToken;
        event->GetCodeData.il = (m_isIL != 0);
        event->GetCodeData.start = start;
        event->GetCodeData.end = end;
        event->GetCodeData.ilCodeVersionToken = m_ilCodeVersionToken;
        event->GetCodeData.nativeCodeVersionToken = m_nativeCodeVersionToken;

        hr = GetProcess()->SendIPCEvent(event, CorDBIPC_BUFFER_SIZE);

        if FAILED(hr)
            goto LExit;

        //
        // Keep getting result events until we get the last bit of code.
        //
#ifndef RIGHT_SIDE_ONLY
        bool fFirstLoop = true;
#endif
        do
        {
#ifdef RIGHT_SIDE_ONLY

            hr = GetProcess()->m_cordb->WaitForIPCEventFromProcess(
                    GetProcess(), 
                    GetAppDomain(), 
                    event);
            
#else

            if (fFirstLoop)
            {
                hr = GetProcess()->m_cordb->GetFirstContinuationEvent(
                        GetProcess(), 
                        event);
                fFirstLoop = false;
            }
            else
            {
                hr = GetProcess()->m_cordb->GetNextContinuationEvent(
                        GetProcess(), 
                        event);
            }
            
#endif //RIGHT_SIDE_ONLY
            if(FAILED(hr))
                goto LExit;


            _ASSERTE(event->type == DB_IPCE_GET_CODE_RESULT);

            memcpy(rgbCodeOrCodeSnippet + event->GetCodeData.start - start, 
                   &event->GetCodeData.code, 
                   event->GetCodeData.end - event->GetCodeData.start);

        } while (event->GetCodeData.end < end);

        // We sluiced the code into the caller's buffer, so tell the caller
        // how much space is used.
        if (rgbCodeOrCodeSnippet == buffer)
            *pcBufferSize = endOffset - startOffset;
        
        m_continueCounterLastSync = GetProcess()->m_continueCounter;
    }

    // if we just got the code, we'll have to copy it over
    if (*pcBufferSize == 0 && m_rgbCode != NULL)
    {
        memcpy(buffer, 
               m_rgbCode+startOffset, 
               endOffset - startOffset);
        *pcBufferSize = endOffset - startOffset;
    }

LExit:

#ifndef RIGHT_SIDE_ONLY    
    GetProcess()->ClearContinuationEvents();
#endif    

    INPROC_UNLOCK();

    return hr;
}

#include "dbgipcevents.h"
HRESULT CordbCode::GetVersionNumber( ULONG32 *nVersion)
{
    VALIDATE_POINTER_TO_OBJECT(nVersion, ULONG32 *);
    
    LOG((LF_CORDB,LL_INFO10000,"R:CC:GVN:Returning 0x%x "
        "as version\n",m_nVersion));
    
    *nVersion = INTERNAL_TO_EXTERNAL_VERSION(m_nVersion);
    _ASSERTE((*nVersion) >= USER_VISIBLE_FIRST_VALID_VERSION_NUMBER);
    return S_OK;
}

HRESULT CordbCode::GetILToNativeMapping(ULONG32 cMap,
                                        ULONG32 *pcMap,
                                        COR_DEBUG_IL_TO_NATIVE_MAP map[])
{
    VALIDATE_POINTER_TO_OBJECT_OR_NULL(pcMap, ULONG32 *);
    VALIDATE_POINTER_TO_OBJECT_ARRAY_OR_NULL(map, COR_DEBUG_IL_TO_NATIVE_MAP *,cMap,true,true);

    // Gotta have a map address to return a map.
    if (m_ilToNativeMapAddr == NULL)
        return CORDBG_E_NON_NATIVE_FRAME;
    
    HRESULT hr = S_OK;
    DebuggerILToNativeMap *mapInt = NULL;

    mapInt = new DebuggerILToNativeMap[cMap];
    
    if (mapInt == NULL)
        return E_OUTOFMEMORY;
    
    // If they gave us space to copy into...
    if (map != NULL)
    {
        // Only copy as much as either they gave us or we have to copy.
        SIZE_T cnt = min(cMap, m_ilToNativeMapSize);

        if (cnt > 0)
        {
            // Read the map right out of the Left Side.
            BOOL succ = ReadProcessMemory(GetProcess()->m_handle,
                                          m_ilToNativeMapAddr,
                                          mapInt,
                                          cnt *
                                          sizeof(DebuggerILToNativeMap),
                                          NULL);

            if (!succ)
                hr = HRESULT_FROM_WIN32(GetLastError());
        }

        // Remember that we need to translate between our internal DebuggerILToNativeMap and the external
        // COR_DEBUG_IL_TO_NATIVE_MAP!
        if (SUCCEEDED(hr))
            ExportILToNativeMap(cMap, map, mapInt, m_size);
    }
    
    if (pcMap)
        *pcMap = m_ilToNativeMapSize;

    if (mapInt != NULL)
        delete [] mapInt;

    return hr;
}

HRESULT CordbCode::GetEnCRemapSequencePoints(ULONG32 cMap, ULONG32 *pcMap, ULONG32 offsets[])
{
    VALIDATE_POINTER_TO_OBJECT_OR_NULL(pcMap, ULONG32*);
    VALIDATE_POINTER_TO_OBJECT_ARRAY_OR_NULL(offsets, ULONG32*, cMap, true, true);

    // Gotta have a map address to return a map.
    if (m_ilToNativeMapAddr == NULL)
        return CORDBG_E_NON_NATIVE_FRAME;
    
    _ASSERTE(m_ilToNativeMapSize > 0);
    
    HRESULT hr = S_OK;
    DebuggerILToNativeMap *mapInt = NULL;

    // We need space for the entire map from the Left Side. We really should be caching this...
    mapInt = new DebuggerILToNativeMap[m_ilToNativeMapSize];
    
    if (mapInt == NULL)
        return E_OUTOFMEMORY;
    
    // Read the map right out of the Left Side.
    BOOL succ = ReadProcessMemory(GetProcess()->m_handle,
                                  m_ilToNativeMapAddr,
                                  mapInt,
                                  m_ilToNativeMapSize * sizeof(DebuggerILToNativeMap),
                                  NULL);

    if (!succ)
        hr = HRESULT_FROM_WIN32(GetLastError());

    // We'll count up how many entries there are as we go.
    ULONG32 cnt = 0;
            
    if (SUCCEEDED(hr))
    {
        for (ULONG32 iMap = 0; iMap < m_ilToNativeMapSize; iMap++)
        {
            SIZE_T offset = mapInt[iMap].ilOffset;
            ICorDebugInfo::SourceTypes src = mapInt[iMap].source;

            // We only set EnC remap breakpoints at valid, stack empty IL offsets.
            if ((offset != (SIZE_T) ICorDebugInfo::PROLOG) &&
                (offset != (SIZE_T) ICorDebugInfo::EPILOG) &&
                (offset != (SIZE_T) ICorDebugInfo::NO_MAPPING) &&
                (src & ICorDebugInfo::STACK_EMPTY))
            {
                // If they gave us space to copy into...
                if ((offsets != NULL) && (cnt < cMap))
                    offsets[cnt] = offset;

                // We've got another one, so count it.
                cnt++;
            }
        }
    }
    
    if (pcMap)
        *pcMap = cnt;

    if (mapInt != NULL)
        delete [] mapInt;

    return hr;
}


// GENERICS: code for this class placed at end to minimize DIFF
/* ------------------------------------------------------------------------- *
 * Class for types
 // GENERICS: For the moment                                      
                                              we keep a hash table in the
// type constructor (e.g. "Dict" in "Dict<String,String>") that uses
// the pointers of CordbType's themselves as unique ids for single type applications.
// Thus the representation for  "Dict<class String,class Foo, class Foo* >" goes as follows:
//    1. Assume the type Foo is represented by CordbClass *5678x
//    1b. Assume the hashtable m_sharedtypes in the AppDomain maps E_T_STRING to the CordbType *0ABCx
//       Assume the hashtable m_classtypes in type Foo (i.e. CordbClass *5678x) maps E_T_CLASS to the CordbTypeeter *0DEFx
//       Assume the hashtable m_spinetypes in the type Foo maps E_T_PTR to the CordbTypeeter *0647x
//    2. The hash table m_classtypes in "Dict" maps "0ABCx" to a new CordbClass
//       representing Dict<String> (a single type application)
//    3. The hash table m_classtypes in this new CordbClass maps "0DEFx" to a
//        new CordbClass representing Dict<class String,class Foo>
//    3. The hash table m_classtypes in this new CordbClass maps "0647" to a
//        new CordbClass representing Dict<class String,class Foo, class Foo*>
//
// This                                       lets us reuse the existing hash table scheme to build
// up constructed types. 
//
// UPDATE: actually we also hack the rank into this....                  
* ------------------------------------------------------------------------- */

// Combine E_T_s and rank together to get an id for the m_sharedtypes table
#define CORDBTYPE_ID(elementType,rank) ((unsigned int) elementType * (rank + 1) + 1)

CordbType::CordbType(CordbAppDomain *appdomain, CorElementType et, unsigned int rank)
: CordbBase( CORDBTYPE_ID(et,rank) , enumCordbType),
  m_elementType(et),
  m_appdomain(appdomain),
  m_class(NULL),
  m_rank(rank),
  m_spinetypes(2),
  m_objectSize(0),
  m_typeHandle(NULL),
  m_instancefields(0),
  m_EnCCounterLastSync(0)
{
}


CordbType::CordbType(CordbAppDomain *appdomain, CorElementType et, CordbClass *cls)
: CordbBase( et, enumCordbType),
  m_elementType(et),
  m_appdomain(appdomain),
  m_class(cls),
  m_rank(0),
  m_spinetypes(2),
  m_objectSize(0),
  m_typeHandle(NULL),
  m_instancefields(0),
  m_EnCCounterLastSync(0)
{
}


CordbType::CordbType(CordbType *tycon, CordbType *tyarg)
: CordbBase( (unsigned int) tyarg, enumCordbType), 
  m_elementType(tycon->m_elementType), 
  m_appdomain(tycon->m_appdomain), 
  m_class(tycon->m_class),
  m_rank(tycon->m_rank),
  m_spinetypes(2),
  m_objectSize(0),
  m_typeHandle(NULL),
  m_instancefields(0),
  m_EnCCounterLastSync(0)
  // tyarg is added as part of instantiation -see below...
{
}


ULONG STDMETHODCALLTYPE CordbType::AddRef()
{
	// This AddRef/Release pair creates a very weak ref-counted reference to the class for this
	// type.  This avoids a circularity in ref-counted references between 
	// classes and types - if we had a circularity the objects would never get
	// collected at all...
	if (m_class)
		m_class->AddRef();
	return (BaseAddRef());
}
ULONG STDMETHODCALLTYPE CordbType::Release()
{
	if (m_class)
		m_class->Release();
	return (BaseRelease());
}

/*
    A list of which resources owened by this object are accounted for.

    HANDLED:
        CordbClass *m_class;  Weakly referenced by increasing count directly in AddRef() and Release()
        Instantiation   m_inst; // Internal pointers to CordbClass released in CordbClass::Neuter
        CordbHashTable   m_spinetypes; // Neutered
        CordbHashTable   m_fields; // Deleted in ~CordbType
*/

CordbType::~CordbType()
{
	if(m_inst.m_ppInst)
        delete [] m_inst.m_ppInst;
    if(m_instancefields)
        delete [] m_instancefields;
}

// Neutered by CordbModule
void CordbType::Neuter()
{
    AddRef();
    {   
		for (unsigned int i = 0; i < m_inst.m_cInst; i++) {
			m_inst.m_ppInst[i]->Release();
		}
        NeuterAndClearHashtable(&m_spinetypes);
        CordbBase::Neuter();
    }
    Release();
}    

HRESULT CordbType::QueryInterface(REFIID id, void **pInterface)
{
    if (id == IID_ICorDebugType)
        *pInterface = (ICorDebugClass*)this;
    else if (id == IID_IUnknown)
        *pInterface = (IUnknown*)(ICorDebugType*)this;
    else
    {
        *pInterface = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}


// CordbType's are effectively a full representation of
// structured types.  They are hashed via a combination of their constituent
// elements (e.g. CordbClass's or CordbType's) and the element type that is used to
// combine the elements, or if they have no elements then via 
// the element type alone.  The following  is used to create all CordbTypes.
//
// 
HRESULT CordbType::MkNullaryType(CordbAppDomain *appdomain, CorElementType et, CordbType **pRes)
{
	_ASSERTE(appdomain != NULL);
	_ASSERTE(pRes != NULL);

	// Some points in the code create types via element types that are clearly objects but where
	// no further information is given.  This is always done shen creating a CordbValue, prior
	// to actually going over to the EE to discover what kind of value it is.  In all these 
	// cases we can just use the type for "Object" - the code for dereferencing the value
	// will update the type correctly once it has been determined.  We don't do this for ELEMENT_TYPE_STRING
	// as that is actually a NullaryType and at other places in the code we will want exactly that type!
	if (et == ELEMENT_TYPE_CLASS || 
		et == ELEMENT_TYPE_SZARRAY || 
		et == ELEMENT_TYPE_ARRAY)
	    et = ELEMENT_TYPE_OBJECT;

	switch (et) 
	{
	case ELEMENT_TYPE_VOID:
	case ELEMENT_TYPE_FNPTR: // this one is included because we need a "seed" type to uniquely hash FNPTR types, i.e. the nullary FNPTR type is used as the type constructor for all function pointer types, when combined with an approproiate instantiation.
	case ELEMENT_TYPE_BOOLEAN:
	case ELEMENT_TYPE_CHAR:
	case ELEMENT_TYPE_I1:
	case ELEMENT_TYPE_U1:
	case ELEMENT_TYPE_I2:
	case ELEMENT_TYPE_U2:
	case ELEMENT_TYPE_I4:
	case ELEMENT_TYPE_U4:
	case ELEMENT_TYPE_I8:
	case ELEMENT_TYPE_U8:
	case ELEMENT_TYPE_R4:
	case ELEMENT_TYPE_R8:
	case ELEMENT_TYPE_STRING:
	case ELEMENT_TYPE_OBJECT:
	case ELEMENT_TYPE_TYPEDBYREF:
	case ELEMENT_TYPE_I:
	case ELEMENT_TYPE_U:
	case ELEMENT_TYPE_R:
		*pRes = (CordbType *)appdomain->m_sharedtypes.GetBase(CORDBTYPE_ID(et,0));
		if (*pRes == NULL)
		{
			CordbType *pGP = new CordbType(appdomain, et, (unsigned int) 0);
			if (pGP == NULL)
				return E_OUTOFMEMORY;
			HRESULT hr = appdomain->m_sharedtypes.AddBase(pGP);
			if (SUCCEEDED(hr))
				*pRes = pGP;
			else {
				_ASSERTE(0);
				delete pGP;
			}

			return hr;
		}
		return S_OK;
	default:
		_ASSERTE(!"unexpected element type!");
		return E_FAIL;
	}

}

HRESULT CordbType::MkUnaryType(CordbAppDomain *appdomain, CorElementType et, ULONG rank, CordbType *tyarg, CordbType **pRes)
{
	_ASSERTE(appdomain != NULL);
	_ASSERTE(pRes != NULL);
	switch (et) 
	{

	case ELEMENT_TYPE_PTR:
	case ELEMENT_TYPE_BYREF:
	case ELEMENT_TYPE_CMOD_REQD:
	case ELEMENT_TYPE_CMOD_OPT:
	case ELEMENT_TYPE_MODIFIER:
	case ELEMENT_TYPE_PINNED:
		_ASSERTE(rank == 0);
		goto unary;

	case ELEMENT_TYPE_SZARRAY:
		_ASSERTE(rank == 1);
		goto unary;

	case ELEMENT_TYPE_ARRAY:
unary:
		{
			CordbType *tycon = (CordbType *)appdomain->m_sharedtypes.GetBase(CORDBTYPE_ID(et,rank));
			if (tycon == NULL)
			{
				tycon = new CordbType(appdomain, et, rank);
				if (tycon == NULL)
					return E_OUTOFMEMORY;
				HRESULT hr = appdomain->m_sharedtypes.AddBase(tycon);
				if (FAILED(hr))
				{
					_ASSERTE(0);
					delete tycon;
					return hr;
				}
			}
			Instantiation inst(1, &tyarg);
			return MkTyAppType(appdomain, tycon, inst, pRes);

		}
	case ELEMENT_TYPE_VALUEARRAY:
		_ASSERTE(!"unimplemented!");
		return E_FAIL;
	default:
		_ASSERTE(!"unexpected element type!");
		return E_FAIL;
	}

}

HRESULT CordbType::MkTyAppType(CordbAppDomain *appdomain, CordbType *tycon, const Instantiation &inst, CordbType **pRes)
{
	CordbType *c = tycon;
	for (unsigned int i = 0; i<inst.m_cClassTyPars; i++) {
		CordbType *c2 = (CordbType *)c->m_spinetypes.GetBase((unsigned int) (inst.m_ppInst[i]));

		if (c2 == NULL)
		{
			c2 = new CordbType(c, inst.m_ppInst[i]);

			if (c2 == NULL)
				return E_OUTOFMEMORY;

			HRESULT hr = c->m_spinetypes.AddBase(c2);

			if (FAILED(hr))
			{
				_ASSERTE(0);
				delete c2;
				return (hr);
			}
			c2->m_inst.m_cInst = i+1;
			c2->m_inst.m_cClassTyPars = i+1;
			c2->m_inst.m_ppInst = new CordbType *[i+1];
			if (c2->m_inst.m_ppInst == NULL) {
				delete c2;
				return E_OUTOFMEMORY;
			}
			for (unsigned int j = 0; j<i+1; j++) {
				// Constructed types include pointers across to other types - increase
				// the reference counts on these.... 
				inst.m_ppInst[j]->AddRef();
				c2->m_inst.m_ppInst[j] = inst.m_ppInst[j];
			}
		}
		c = c2;
	}
	*pRes = c;
	return S_OK;
}

HRESULT CordbType::MkConstructedType(CordbAppDomain *appdomain, CorElementType et, CordbClass *tycon, const Instantiation &inst, CordbType **pRes)
{
	_ASSERTE(appdomain != NULL);
	_ASSERTE(pRes != NULL);
	switch (et) 
	{
	case ELEMENT_TYPE_CLASS:
	case ELEMENT_TYPE_VALUETYPE:
		{
			CordbType *tyconty = NULL;

			tyconty = (CordbType *)(tycon->m_classtypes.GetBase(et));
			if (tyconty == NULL)
			{
				tyconty = new CordbType(appdomain, et, tycon);
				if (tyconty == NULL)
					return E_OUTOFMEMORY;
				HRESULT hr = tycon->m_classtypes.AddBase(tyconty);
				if (!SUCCEEDED(hr)) {
					delete tyconty;
					return hr;
				}
			}
			_ASSERTE(tyconty != NULL);

			return CordbType::MkTyAppType(appdomain, tyconty, inst, pRes);
		}
	default:
		_ASSERTE(inst.m_cInst == 0);
		return MkNullaryType(appdomain, et, pRes);

	}
}    


HRESULT CordbType::MkNaryType(CordbAppDomain *appdomain, CorElementType et, const Instantiation &inst, CordbType **pRes)
{
	CordbType *tycon;
	_ASSERTE(et == ELEMENT_TYPE_FNPTR);
	HRESULT hr = MkNullaryType(appdomain, et, &tycon);
	if (!SUCCEEDED(hr)) {
		return hr;
	}
	return CordbType::MkTyAppType(appdomain, tycon, inst, pRes);
}    



HRESULT CordbType::GetType(CorElementType *pType)
{
	*pType = m_elementType;
	return S_OK;
}

HRESULT CordbType::GetClass(ICorDebugClass **pClass)
{
	_ASSERTE(m_class);
	*pClass = m_class;
	if (*pClass)
		(*pClass)->AddRef();
	return S_OK;
}

HRESULT CordbType::GetRank(ULONG32 *pnRank)
{
    VALIDATE_POINTER_TO_OBJECT(pnRank, ULONG32 *);
    
    if (m_elementType != ELEMENT_TYPE_SZARRAY &&
        m_elementType != ELEMENT_TYPE_ARRAY)
        return E_INVALIDARG;

    *pnRank = (ULONG32) m_rank;

    return S_OK;
}
HRESULT CordbType::GetFirstTypeParameter(ICorDebugType **pType)
{
	_ASSERTE(m_inst.m_ppInst != NULL);
	_ASSERTE(m_inst.m_ppInst[0] != NULL);
	*pType = m_inst.m_ppInst[0];
	if (*pType)
		(*pType)->AddRef();
	return S_OK;
}


HRESULT CordbType::MkNonGenericType(CordbAppDomain *appdomain, CorElementType et, CordbClass *cl,CordbType **pRes)
{
	return CordbType::MkConstructedType(appdomain, et, cl, Instantiation(), pRes);
}

HRESULT CordbType::MkNaturalNonGenericType(CordbAppDomain *appdomain, CordbClass *cl, CordbType **ppType)
{ 
	bool isVC;
	HRESULT hr = cl->IsValueClass(&isVC);
	if (FAILED(hr))
	{
		_ASSERTE(0);
		return hr;
	}
	return MkNonGenericType(appdomain, isVC ? ELEMENT_TYPE_VALUETYPE : ELEMENT_TYPE_CLASS, cl, ppType); 
}

CordbType *CordbType::SkipFunkyModifiers() 
{
	switch (m_elementType) 
	{
	case ELEMENT_TYPE_CMOD_REQD:
	case ELEMENT_TYPE_CMOD_OPT:
	case ELEMENT_TYPE_MODIFIER:
	case ELEMENT_TYPE_PINNED:
		{
			CordbType *next;
			this->DestUnaryType(&next);
			return next->SkipFunkyModifiers();
		}
	default:
		return this;
	}
}


void
CordbType::DestUnaryType(CordbType **pRes) 
{
    _ASSERTE(m_elementType == ELEMENT_TYPE_PTR 
		|| m_elementType == ELEMENT_TYPE_BYREF
		|| m_elementType == ELEMENT_TYPE_ARRAY 
		|| m_elementType == ELEMENT_TYPE_SZARRAY 
		|| m_elementType == ELEMENT_TYPE_CMOD_REQD
		|| m_elementType == ELEMENT_TYPE_CMOD_OPT
		|| m_elementType == ELEMENT_TYPE_MODIFIER
		|| m_elementType == ELEMENT_TYPE_PINNED);
    _ASSERTE(m_elementType != ELEMENT_TYPE_VAR); 
    _ASSERTE(m_elementType != ELEMENT_TYPE_MVAR);
	_ASSERTE(m_inst.m_cInst == 1);
	_ASSERTE(m_inst.m_ppInst != NULL);
	*pRes = m_inst.m_ppInst[0];
}


void
CordbType::DestConstructedType(CordbClass **cls, Instantiation *inst) 
{
    ASSERT(m_elementType == ELEMENT_TYPE_VALUETYPE || m_elementType == ELEMENT_TYPE_CLASS);
	*cls = m_class;
	*inst = m_inst;
}

void
CordbType::DestNaryType(Instantiation *inst) 
{
    ASSERT(m_elementType == ELEMENT_TYPE_FNPTR);
	*inst = m_inst;
}


HRESULT
CordbType::SigToType(CordbModule *module, PCCOR_SIGNATURE pvSigBlob, const Instantiation &inst, CordbType **pRes)
{

	PCCOR_SIGNATURE blob = pvSigBlob;
	CorElementType elementType = CorSigUncompressElementType(blob);
	HRESULT hr;
	switch (elementType) 
	{
	case ELEMENT_TYPE_VAR: 
	case ELEMENT_TYPE_MVAR: 
		{
			ULONG tyvar_num;
			tyvar_num = CorSigUncompressData(blob);
			_ASSERTE (tyvar_num < (elementType == ELEMENT_TYPE_VAR ? inst.m_cClassTyPars : inst.m_cInst - inst.m_cClassTyPars));
			_ASSERTE (inst.m_ppInst != NULL);
			*pRes = (elementType == ELEMENT_TYPE_VAR ? inst.m_ppInst[tyvar_num] : inst.m_ppInst[tyvar_num + inst.m_cClassTyPars]);
			return S_OK;
		}
	case ELEMENT_TYPE_WITH:
		{
			// ignore "WITH", look at next ELEMENT_TYPE to get CLASS or VALUE
			elementType = CorSigUncompressElementType(blob);
			mdToken tycon = CorSigUncompressToken(blob);
			ULONG argCnt = CorSigUncompressData(blob); // Get number of parameters
			CordbType **ppInst = (CordbType **) _alloca(sizeof(CordbType *) * argCnt);
			for (unsigned int i = 0; i<argCnt;i++) {
				IfFailRet( CordbType::SigToType(module, blob, inst, &ppInst[i]) );
				blob += _skipTypeInSignature(blob);        // Skip the parameter
			}
			CordbClass *tyconcc;
  		    IfFailRet( module->ResolveTypeRefOrDef(tycon,&tyconcc));

			Instantiation tyinst(argCnt,ppInst);
			return CordbType::MkConstructedType(module->GetAppDomain(), elementType, tyconcc, tyinst, pRes);
		}
	case ELEMENT_TYPE_CLASS:
	case ELEMENT_TYPE_VALUETYPE:
		{
			mdToken tycon = CorSigUncompressToken(blob);
			CordbClass *tyconcc;
  		    IfFailRet( module->ResolveTypeRefOrDef(tycon,&tyconcc));

			return CordbType::MkNonGenericType(module->GetAppDomain(), elementType, tyconcc, pRes);
		}
	case ELEMENT_TYPE_CMOD_REQD:
	case ELEMENT_TYPE_CMOD_OPT:
		{    
			CorSigUncompressToken(blob);
			CordbType *tyarg;
			IfFailRet( CordbType::SigToType(module, blob, inst, &tyarg) );

			*pRes = tyarg; // Throw away CMOD on all CordbTypes...
			return S_OK;
		}

	case ELEMENT_TYPE_ARRAY:
		{
			CordbType *tyarg;
			IfFailRet( CordbType::SigToType(module, blob, inst, &tyarg) );
			blob += _skipTypeInSignature(blob);        // Skip the embedded type

			ULONG rank = CorSigUncompressData(blob);
			return CordbType::MkUnaryType(module->GetAppDomain(), elementType, rank, tyarg, pRes);
		}
	case ELEMENT_TYPE_SZARRAY:
		{
			CordbType *tyarg;
			IfFailRet( CordbType::SigToType(module, blob, inst, &tyarg) );
			return CordbType::MkUnaryType(module->GetAppDomain(), elementType, 1, tyarg, pRes);
		}

	case ELEMENT_TYPE_PTR:
	case ELEMENT_TYPE_BYREF:
	case ELEMENT_TYPE_MODIFIER:
	case ELEMENT_TYPE_PINNED:
		{
			CordbType *tyarg;
			IfFailRet( CordbType::SigToType(module, blob, inst, &tyarg) );
			return CordbType::MkUnaryType(module->GetAppDomain(),elementType, 0, tyarg, pRes);
		}

	case ELEMENT_TYPE_FNPTR:
		{
			ULONG argCnt = CorSigUncompressData(blob); // Get number of parameters
			CordbType **ppInst = (CordbType **) _alloca(sizeof(CordbType *) * argCnt);
			for (unsigned int i = 0; i<argCnt;i++) {
				IfFailRet( CordbType::SigToType(module, blob, inst, &ppInst[i]) );
				blob += _skipTypeInSignature(blob);        // Skip the parameter
			}
			Instantiation tyinst(argCnt,ppInst);
			return CordbType::MkNaryType(module->GetAppDomain(), elementType, tyinst, pRes);
		}

	case ELEMENT_TYPE_VALUEARRAY:
		_ASSERTE(!"unimplemented!");
		return E_FAIL;
	case ELEMENT_TYPE_VOID:
	case ELEMENT_TYPE_BOOLEAN:
	case ELEMENT_TYPE_CHAR:
	case ELEMENT_TYPE_I1:
	case ELEMENT_TYPE_U1:
	case ELEMENT_TYPE_I2:
	case ELEMENT_TYPE_U2:
	case ELEMENT_TYPE_I4:
	case ELEMENT_TYPE_U4:
	case ELEMENT_TYPE_I8:
	case ELEMENT_TYPE_U8:
	case ELEMENT_TYPE_R4:
	case ELEMENT_TYPE_R8:
	case ELEMENT_TYPE_STRING:
	case ELEMENT_TYPE_TYPEDBYREF:
	case ELEMENT_TYPE_OBJECT:
	case ELEMENT_TYPE_I:
	case ELEMENT_TYPE_U:
	case ELEMENT_TYPE_R:
		return CordbType::MkNullaryType(module->GetAppDomain(), elementType, pRes);
	default:
		_ASSERTE(!"unexpected element type!");
		return E_FAIL;

	}
}

HRESULT CordbType::TypeDataToType(CordbAppDomain *pAppDomain, DebuggerIPCE_BasicTypeData *data, CordbType **pRes) 
{
	HRESULT hr = S_OK;
	CorElementType et = data->elementType;
	switch (et) 
	{
	case ELEMENT_TYPE_ARRAY:
	case ELEMENT_TYPE_VALUEARRAY:
	case ELEMENT_TYPE_SZARRAY:
	case ELEMENT_TYPE_PTR:
	case ELEMENT_TYPE_BYREF:
		// For these element types the "Basic" type data only contains the type handle.
		// So we fetch some more data, and the go onto the "Expanded" case...
		{
			INPROC_LOCK();
			DebuggerIPCEvent event;
			pAppDomain->GetProcess()->InitIPCEvent(&event, 
				DB_IPCE_GET_EXPANDED_TYPE_INFO, 
				true,
				(void *)(pAppDomain->m_id));
			event.ExpandType.typeHandle = data->typeHandle;

			// two-way event
			hr = pAppDomain->GetProcess()->m_cordb->SendIPCEvent(pAppDomain->GetProcess(), &event,
				sizeof(DebuggerIPCEvent));

			if (!SUCCEEDED(hr))
			{
				_ASSERTE(0);
				goto exitevent;
			}

                        if (!SUCCEEDED(event.hr))
                                goto exitevent;

			_ASSERTE(event.type == DB_IPCE_GET_EXPANDED_TYPE_INFO_RESULT);
			hr = CordbType::TypeDataToType(pAppDomain,&event.ExpandTypeResult, pRes);
			if (!SUCCEEDED(hr))
			{
				_ASSERTE(0);
				goto exitevent;
			}
exitevent:
			INPROC_UNLOCK();
			return hr;
		}

	case ELEMENT_TYPE_FNPTR:
		{
			DebuggerIPCE_ExpandedTypeData e;
			e.elementType = et;
			e.NaryTypeData.typeHandle = data->typeHandle;
			return CordbType::TypeDataToType(pAppDomain, &e, pRes);
		}
	default:
		// For all other element types the "Basic" view of a type 
		// contains the same information as the "expanded"
		// view, so just reuse the code for the Expanded view...
		DebuggerIPCE_ExpandedTypeData e;
		e.elementType = et;
		e.ClassTypeData.metadataToken = data->metadataToken;
		e.ClassTypeData.debuggerModuleToken = data->debuggerModuleToken;
		e.ClassTypeData.typeHandle = data->typeHandle;
		return CordbType::TypeDataToType(pAppDomain, &e, pRes);
	}
}


// GENERICS: A subtle change in the approach I have adopted is that
// for the type E_T_STRING m_class is now NULL.  This means that
// the behaviour revealed to clients also changes slightly: GetClass (which
// could be deprecated anyway...) now returns NULL for the type STRING
// and for all objects of type STRING.  
//
// I do actually return the class token back from the left-side when
// getting the type of an object, so in this case we could use this 
// to lookup the CordbClass for System.String.  Or we could go and resolve
// the type ourselves on the right-side.  However neither seems very
// appealing: there is no reason per-se that we need to "know" the class
// for E_T_STRING on the right-side at all....
//
HRESULT CordbType::TypeDataToType(CordbAppDomain *pAppDomain, DebuggerIPCE_ExpandedTypeData *data, CordbType **pRes) 
{
    CorElementType et = data->elementType;
    HRESULT hr;
    switch (et) 
    {
    case ELEMENT_TYPE_OBJECT:
        // This is just in case we have "OBJECT" but actually have a more specific class....
        if (data->ClassTypeData.metadataToken != mdTokenNil) {
            et = ELEMENT_TYPE_CLASS;
            goto ETClass;
        }
        else 
            goto ETObject;


    case ELEMENT_TYPE_VOID:
    case ELEMENT_TYPE_BOOLEAN:
    case ELEMENT_TYPE_CHAR:
    case ELEMENT_TYPE_I1:
    case ELEMENT_TYPE_U1:
    case ELEMENT_TYPE_I2:
    case ELEMENT_TYPE_U2:
    case ELEMENT_TYPE_I4:
    case ELEMENT_TYPE_U4:
    case ELEMENT_TYPE_I8:
    case ELEMENT_TYPE_U8:
    case ELEMENT_TYPE_R4:
    case ELEMENT_TYPE_R8:
    case ELEMENT_TYPE_STRING:
    case ELEMENT_TYPE_TYPEDBYREF:
    case ELEMENT_TYPE_I:
    case ELEMENT_TYPE_U:
    case ELEMENT_TYPE_R:
ETObject:
        IfFailRet (CordbType::MkNullaryType(pAppDomain, et, pRes));
        break;

	case ELEMENT_TYPE_CLASS:
	case ELEMENT_TYPE_VALUETYPE:
ETClass:
        {
            if (data->ClassTypeData.metadataToken == mdTokenNil) {
                et = ELEMENT_TYPE_OBJECT;
                goto ETObject;
            }
            CordbModule* pClassModule = pAppDomain->LookupModule(data->ClassTypeData.debuggerModuleToken);
#ifdef RIGHT_SIDE_ONLY
            _ASSERTE(pClassModule != NULL);
#else
            // This case happens if inproc debugging is used from a ModuleLoadFinished
            // callback for a module that hasn't been bound to an assembly yet.
            if (pClassModule == NULL)
                return E_FAIL;
#endif

            CordbClass *tycon;
            IfFailRet (pClassModule->LookupOrCreateClass(data->ClassTypeData.metadataToken,&tycon));
            if (data->ClassTypeData.typeHandle) 
            {
                IfFailRet (CordbType::InstantiateFromTypeHandle(pAppDomain, data->ClassTypeData.typeHandle, et, tycon, pRes));  
                // Set the type handle regardless of how we found
                // the type.  For example if type was already 
                // constructed without the type handle still set
                // it here.
                if (*pRes) 
                    (*pRes)->m_typeHandle = data->ClassTypeData.typeHandle;
                break;
            } 
            else 
            {
                IfFailRet (CordbType::MkNonGenericType(pAppDomain, et,tycon,pRes));
                break;
            }
        }

    case ELEMENT_TYPE_ARRAY:
    case ELEMENT_TYPE_SZARRAY:
        {
            CordbType *argty;
            IfFailRet (CordbType::TypeDataToType(pAppDomain, &(data->ArrayTypeData.arrayTypeArg), &argty));
            IfFailRet (CordbType::MkUnaryType(pAppDomain, et, data->ArrayTypeData.arrayRank, argty, pRes));
            break;
        }

    case ELEMENT_TYPE_PTR:
    case ELEMENT_TYPE_BYREF:
        {
            CordbType *argty;
            IfFailRet (CordbType::TypeDataToType(pAppDomain, &(data->UnaryTypeData.unaryTypeArg), &argty));
            IfFailRet (CordbType::MkUnaryType(pAppDomain, et, 0, argty, pRes));
            break;
        }

    case ELEMENT_TYPE_FNPTR:
        {
            IfFailRet (CordbType::InstantiateFromTypeHandle(pAppDomain, data->NaryTypeData.typeHandle, et, NULL, pRes));  
            if (*pRes) 
              (*pRes)->m_typeHandle = data->NaryTypeData.typeHandle;
            break;
        }

    default:
        _ASSERTE(!"unexpected element type!");
        return E_FAIL;

    }
    return S_OK;
}

HRESULT CordbType::InstantiateFromTypeHandle(CordbAppDomain *appdomain, REMOTE_PTR typeHandle, CorElementType et, CordbClass *tycon, CordbType **pRes) 
{
    HRESULT hr;
    DebuggerIPCEvent *retEvent;
    unsigned int typarCount;
    CordbType **ppInst;
    DebuggerIPCE_ExpandedTypeData *currentTyParData;


    CORDBSyncFromWin32StopIfNecessary(appdomain->GetProcess());

    INPROC_LOCK();

    // We've got a remote address that points to a type handle.
    // We need to send to the left side to get real information about
    // the type handle, including the type parameters.
    DebuggerIPCEvent event;
    // GENERICS: Collect up the class type parameters
	appdomain->GetProcess()->InitIPCEvent(&event, 
        DB_IPCE_GET_TYPE_HANDLE_PARAMS, 
        false,
        (void *)(appdomain->m_id));
    event.GetTypeHandleParams.typeHandle = typeHandle;
    
	hr = appdomain->GetProcess()->m_cordb->SendIPCEvent(appdomain->GetProcess(), &event,
        sizeof(DebuggerIPCEvent));
    
    // Stop now if we can't even send the event.
    if (!SUCCEEDED(hr))
        goto exit;
    
    // Wait for events to return from the RC. We expect at least one
    // class info result event.                                                           
    retEvent = (DebuggerIPCEvent *) _alloca(CorDBIPC_BUFFER_SIZE);
            
#ifdef RIGHT_SIDE_ONLY
    hr = appdomain->GetProcess()->m_cordb->WaitForIPCEventFromProcess(appdomain->GetProcess(), 
		appdomain,
                retEvent);
#else 
    hr = appdomain->GetProcess()->m_cordb->GetFirstContinuationEvent(appdomain->GetProcess(),retEvent);
#endif //RIGHT_SIDE_ONLY    
            
    if (!SUCCEEDED(hr))
        goto exit;
            
    _ASSERTE(retEvent->type == DB_IPCE_GET_TYPE_HANDLE_PARAMS_RESULT);
            
    typarCount = retEvent->GetTypeHandleParamsResult.typarCount;
            
    currentTyParData = &(retEvent->GetTypeHandleParamsResult.tyParData[0]);
            
    ppInst = (CordbType **) _alloca(sizeof(CordbType *) * typarCount);

    for (unsigned int i = 0; i < typarCount;i++)
    {
		hr = CordbType::TypeDataToType(appdomain, currentTyParData, &ppInst[i]);
        if (!SUCCEEDED(hr))
            goto exit;
        currentTyParData++;
    }

	{
		Instantiation inst(typarCount, ppInst);
		if (et == ELEMENT_TYPE_FNPTR) 
		{
			hr = CordbType::MkNaryType(appdomain, et,inst, pRes);
		}
		else 
		{
			hr = CordbType::MkConstructedType(appdomain, et,tycon,inst, pRes);
		}
	}

exit:    

#ifndef RIGHT_SIDE_ONLY    
    appdomain->GetProcess()->ClearContinuationEvents();
#endif
    
    INPROC_UNLOCK();
    
    return hr;
}


HRESULT CordbType::Init(BOOL fForceInit)
{

	HRESULT hr = S_OK;

    // We don't have to reinit if the EnC version is up-to-date &
    // we haven't been told to do the init regardless.
    if (m_EnCCounterLastSync >= GetProcess()->m_EnCCounter
        && !fForceInit)
        return S_OK;
        

	// Step 1. initialize the type constructor (if one exists)
	// and the (class) type parameters.... 
	if (m_elementType == ELEMENT_TYPE_CLASS || m_elementType == ELEMENT_TYPE_VALUETYPE) {
		_ASSERTE(m_class != NULL);
		IfFailRet( m_class->Init(fForceInit) );
		// That's all that's needed for simple classes and value types, the normal case.
		if (m_class->m_typarCount == 0) 
		{
			return hr; // no clean-up required
		}
	}

	for (unsigned int i = 0; i<m_inst.m_cClassTyPars; i++)
	{
		_ASSERTE(m_inst.m_ppInst != NULL);
		_ASSERTE(m_inst.m_ppInst[i] != NULL);
		IfFailRet( m_inst.m_ppInst[i]->Init(fForceInit) );
	}

    // Step 2. Try to fetch the type handle if necessary (only 
    // for instantiated class types, pointer types etc.)
    // We do this by preparing an event specifying the type and 
    // then fetching the type handle from the left-side.  This
    // will not always succeed, as getting the type handle is the 
    // equivalent of doing a FuncEval, i.e. the instantiation may
    // not have been created.  But we try anyway to reduce the number of
    // failures.  Also, the attempt will fail if too many type parameters
    // are specified.
    //
    // Note that in the normal case we will have the type handle from the EE
    // anyway, e.g. if the CordbType was created when reporting the type
    // of an actual object.
     CordbProcess *pProcess = GetProcess();
     INPROC_LOCK();
     if (m_typeHandle == NULL &&
         (m_elementType == ELEMENT_TYPE_ARRAY ||
          m_elementType == ELEMENT_TYPE_SZARRAY ||
          m_elementType == ELEMENT_TYPE_BYREF ||
          m_elementType == ELEMENT_TYPE_PTR ||
          m_elementType == ELEMENT_TYPE_FNPTR ||
          (m_elementType == ELEMENT_TYPE_CLASS && m_class->m_typarCount > 0)))
      {

#ifdef RIGHT_SIDE_ONLY
          CORDBRequireProcessStateOKAndSync(GetProcess(), GetAppDomain());
#else 
          CORDBRequireProcessStateOK(GetProcess());
#endif    

          DebuggerIPCEvent event;
          pProcess->InitIPCEvent(&event, 
                                 DB_IPCE_GET_TYPE_HANDLE, 
                                 true,
                                 (void *)(m_appdomain->m_id));
          
          TypeToTypeData(&(event.GetTypeHandle.typeData));
           
          if (m_inst.m_cClassTyPars > CORDB_MAX_TYPARS) 
          {
              hr = E_FAIL;
              goto exit1;
          }
          event.GetTypeHandle.typarCount = m_inst.m_cClassTyPars;
          for (unsigned int i = 0; i<m_inst.m_cClassTyPars; i++)
          {
              _ASSERTE(m_inst.m_ppInst != NULL);
              _ASSERTE(m_inst.m_ppInst[i] != NULL);
              m_inst.m_ppInst[i]->TypeToTypeData(&event.GetTypeHandle.tyParData[i]);
          }
           
          hr = pProcess->m_cordb->SendIPCEvent(pProcess, 
                                               &event,
                                               sizeof(DebuggerIPCEvent));
           
          if (!SUCCEEDED(hr) || !SUCCEEDED(event.hr))
              goto exit1;
           
          _ASSERTE(event.type == DB_IPCE_GET_TYPE_HANDLE_RESULT);
          _ASSERTE(event.GetTypeHandleResult.typeHandle != NULL);
          m_typeHandle = event.GetTypeHandleResult.typeHandle;
          

exit1:    
          if (FAILED(hr))
              goto exit;
      }

	//
	if ((m_elementType == ELEMENT_TYPE_VALUETYPE || m_elementType == ELEMENT_TYPE_CLASS) && m_class->m_typarCount > 0)
	{
		_ASSERTE(m_typeHandle);

	    DebuggerIPCEvent event;
		pProcess->InitIPCEvent(&event, 
			DB_IPCE_GET_CLASS_INFO, 
			false,
			(void *)(m_appdomain->m_id));
		event.GetClassInfo.metadataToken = 0;
		event.GetClassInfo.debuggerModuleToken = NULL;
		event.GetClassInfo.typeHandle = m_typeHandle;

		// two-way event
        bool wait = true;
		bool fFirstEvent = true;
		unsigned int fieldIndex = 0;
		unsigned int totalFieldCount = 0;
        DebuggerIPCEvent *retEvent;

		hr = pProcess->m_cordb->SendIPCEvent(pProcess, &event,
			sizeof(DebuggerIPCEvent));

		// Stop now if we can't even send the event.
		if (!SUCCEEDED(hr))
			goto exit2;

		// Wait for events to return from the RC. We expect at least one
		// class info result event.
		retEvent = (DebuggerIPCEvent *) _alloca(CorDBIPC_BUFFER_SIZE);

		while (wait)
		{
#ifdef RIGHT_SIDE_ONLY
			hr = pProcess->m_cordb->WaitForIPCEventFromProcess(pProcess, 
				GetAppDomain(),
				retEvent);
#else 

			if (fFirstEvent)
				hr = pProcess->m_cordb->GetFirstContinuationEvent(pProcess,retEvent);
			else
				hr = pProcess->m_cordb->GetNextContinuationEvent(pProcess,retEvent);
#endif //RIGHT_SIDE_ONLY    

			if (!SUCCEEDED(hr))
				goto exit2;

			_ASSERTE(retEvent->type == DB_IPCE_GET_CLASS_INFO_RESULT);

			// If this is the first event back from the RC, then create the
			// array to hold the field.
			if (fFirstEvent)
			{
				fFirstEvent = false;

				m_objectSize = retEvent->GetClassInfoResult.objectSize;
				// Shouldn't ever loose fields, and should only get back information on 
				// instance fields.  This should be _exactly_ the number of fields reported
				// by the parent class, as that will have been updated to take into account
				// EnC fields when it was initialized above.
				_ASSERTE(retEvent->GetClassInfoResult.instanceVarCount == m_class->m_instanceVarCount);
				totalFieldCount = retEvent->GetClassInfoResult.instanceVarCount;
				// Since we don't keep pointers to the m_instancefields elements, 
				// just toss it & get a new one.
				if (m_instancefields != NULL)
				{
					delete m_instancefields;
					m_instancefields = NULL;
				}
				if (totalFieldCount > 0)
				{
					m_instancefields = new DebuggerIPCE_FieldData[totalFieldCount];

					if (m_instancefields == NULL)
					{
						hr = E_OUTOFMEMORY;
						goto exit;
					}
				}
			}

			DebuggerIPCE_FieldData *currentFieldData =
				&(retEvent->GetClassInfoResult.fieldData);

			for (unsigned int i = 0; i < retEvent->GetClassInfoResult.fieldCount;
				i++)
			{
				m_instancefields[fieldIndex] = *currentFieldData;

				_ASSERTE(m_instancefields[fieldIndex].fldOffset != FIELD_OFFSET_NEW_ENC_DB);

				currentFieldData++;
				fieldIndex++;
			}
		
			if (fieldIndex >= totalFieldCount)
				wait = false;
		}

		// Remember the most recently acquired version of this class
		m_EnCCounterLastSync = GetProcess()->m_EnCCounter;

exit2:    
#ifndef RIGHT_SIDE_ONLY    
		GetProcess()->ClearContinuationEvents();
#endif

		if (FAILED(hr))
			goto exit;

	}

exit:
	INPROC_UNLOCK();
	return hr;

}


HRESULT
CordbType::GetObjectSize(ULONG32 *pObjectSize)
{
    switch (m_elementType)
	{
	case ELEMENT_TYPE_VALUETYPE:
		{
			HRESULT hr = S_OK;
			*pObjectSize = 0;

			hr = Init(FALSE);

			if (!SUCCEEDED(hr))
				return hr;

			*pObjectSize = (ULONG) ((m_class->m_typarCount == 0) ? m_class->m_objectSize : this->m_objectSize);

			return hr;
		}
	default:
		*pObjectSize = _sizeOfElementInstance((PCCOR_SIGNATURE) &m_elementType);
		return S_OK;
	}

}

void CordbType::TypeToTypeData(DebuggerIPCE_BasicTypeData *data)
{
	data->elementType = m_elementType;
	switch (m_elementType) 
	{
	case ELEMENT_TYPE_ARRAY:
	case ELEMENT_TYPE_VALUEARRAY:
	case ELEMENT_TYPE_SZARRAY:
	case ELEMENT_TYPE_BYREF:
	case ELEMENT_TYPE_PTR:
		_ASSERTE(m_typeHandle != NULL);
		data->metadataToken = mdTokenNil;
		data->debuggerModuleToken = NULL;
		data->typeHandle = m_typeHandle; 
		break;

	case ELEMENT_TYPE_VALUETYPE:
	case ELEMENT_TYPE_CLASS:
		_ASSERTE(m_class != NULL);
		data->metadataToken = m_class->m_token;
		data->debuggerModuleToken = m_class->GetModule()->m_debuggerModuleToken;
		data->typeHandle = m_typeHandle; //normally null
		break;
	default:
		data->metadataToken = mdTokenNil;
		data->debuggerModuleToken = NULL;
		data->typeHandle = NULL; 
		break;
	}
}

// Nb. CordbType::Init need NOT have been called before this...
// Also, this does not write the type arguments.  How this is done depends
// depends on where this is called from.
void CordbType::TypeToTypeData(DebuggerIPCE_ExpandedTypeData *data)
{

	switch (m_elementType) 
	{
	case ELEMENT_TYPE_ARRAY:
	case ELEMENT_TYPE_SZARRAY:

		data->ArrayTypeData.arrayRank = m_rank;
		data->elementType = m_elementType;
		break;

	case ELEMENT_TYPE_BYREF:
	case ELEMENT_TYPE_PTR:
	case ELEMENT_TYPE_FNPTR:

		data->elementType = m_elementType;
		break;

	case ELEMENT_TYPE_CLASS:
		{
			data->elementType = m_elementType;
			data->ClassTypeData.metadataToken = m_class->m_token;
			data->ClassTypeData.debuggerModuleToken = m_class->m_module->m_debuggerModuleToken;
			data->ClassTypeData.typeHandle = NULL;

			break;
		}
	case ELEMENT_TYPE_VALUEARRAY:
		_ASSERTE(!"unimplemented!");
	case ELEMENT_TYPE_END:
		_ASSERTE(!"bad element type!");

	default:
		data->elementType = m_elementType;
		break;
	}
}

HRESULT CordbType::EnumerateTypeParameters(ICorDebugTypeEnum **ppTypeParameterEnum)
{
    VALIDATE_POINTER_TO_OBJECT(ppTypeParameterEnum, ICorDebugTypeEnum **);

#ifdef RIGHT_SIDE_ONLY
    CORDBRequireProcessStateOKAndSync(m_appdomain->GetProcess(), GetAppDomain());
#else 
    // For the Virtual Right Side (In-proc debugging), we'll always be synched, but not neccessarily b/c we've gotten a
    // synch message.
    CORDBRequireProcessStateOK(m_appdomain->GetProcess());
#endif    

	ICorDebugTypeEnum *icdTPE = new CordbTypeEnum(this->m_inst.m_cInst, this->m_inst.m_ppInst);
    if ( icdTPE == NULL )
    {
        (*ppTypeParameterEnum) = NULL;
        return E_OUTOFMEMORY;
    }
    
    (*ppTypeParameterEnum) = icdTPE;
    icdTPE->AddRef();
    return S_OK;
}

HRESULT CordbType::GetBase(ICorDebugType **ppType)
{
	HRESULT hr;
    VALIDATE_POINTER_TO_OBJECT(ppType, ICorDebugType **);

	if (m_elementType != ELEMENT_TYPE_CLASS && m_elementType != ELEMENT_TYPE_VALUETYPE)
		return E_INVALIDARG;

	CordbType *res = NULL;

	_ASSERTE(m_class != NULL);

	// Get the supertype from metadata for m_class
	mdToken extends;
	_ASSERTE(m_class->GetModule()->m_pIMImport != NULL);
	IfFailRet (m_class->GetModule()->m_pIMImport->GetTypeDefProps(m_class->m_token, NULL, 0, NULL, NULL, &extends));

	if (extends == mdTypeDefNil || extends == mdTypeRefNil || extends == mdTokenNil)
	{
		res = NULL;
	}
	else if (TypeFromToken(extends) == mdtTypeSpec)
	{
        PCCOR_SIGNATURE sig;
		ULONG sigsz;
   	    // Get the signature for the constructed supertype...
        IfFailRet (m_class->GetModule()->m_pIMImport->GetTypeSpecFromToken(extends,&sig,&sigsz));
		_ASSERTE(sig);

     	// Instantiate the signature of the supertype using the type instantiation for
		// the current type....
		IfFailRet( SigToType(m_class->GetModule(), sig, m_inst, &res) );
	}
	else if (TypeFromToken(extends) == mdtTypeRef || TypeFromToken(extends) == mdtTypeDef)
	{
		CordbClass *superclass;
		IfFailRet( m_class->GetModule()->ResolveTypeRefOrDef(extends,&superclass));
		_ASSERTE(superclass != NULL);
		IfFailRet( MkNaturalNonGenericType(m_appdomain, superclass, &res) );
	}
	else 
	{
		res = NULL;
		_ASSERTE(!"unexpected token!");
	}

    (*ppType) = res;
    if (*ppType)
		res->AddRef();
    return S_OK;
}

HRESULT CordbType::GetFieldInfo(mdFieldDef fldToken, DebuggerIPCE_FieldData **ppFieldData)
{
    HRESULT hr = S_OK;

	if (m_elementType != ELEMENT_TYPE_CLASS && m_elementType != ELEMENT_TYPE_VALUETYPE)
		return E_INVALIDARG;

	*ppFieldData = NULL;
    
    hr = Init(FALSE);

    if (!SUCCEEDED(hr))
        return hr;

	if (m_class->m_typarCount > 0) 
	{
		hr = CordbClass::SearchFieldInfo(m_class->GetModule(), m_class->m_instanceVarCount, m_instancefields, m_class->m_token, fldToken, ppFieldData);
	}
	else
	{
		hr = CORDBG_E_FIELD_NOT_AVAILABLE;
	}
	if (hr == CORDBG_E_FIELD_NOT_AVAILABLE)
		return m_class->GetFieldInfo(fldToken, ppFieldData); // this is for static fields an non-generic types....
	else
		return hr;
}

