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
// ===========================================================================
// File: JITinterfaceGen.CPP
//
// ===========================================================================

// This contains generic C versions of some of the routines
// required by JITinterface.cpp. They are modeled after
// X86 specific routines found in JIThelp.asm or JITinterfaceX86.cpp

#include "common.h"
#include "jitinterface.h"
#include "eeconfig.h"
#include "excep.h"
#include "comdelegate.h"
#include "remoting.h" // create context bound and remote class instances
#include "field.h"

#define JIT_LINKTIME_SECURITY

FCDECL1(Object*, JIT_NewFast, CORINFO_CLASS_HANDLE typeHnd_);
FCDECL2(Object*, JITutil_ChkCastBizarre, CORINFO_CLASS_HANDLE type, Object *obj);
FCDECL2(Object*, JITutil_IsInstanceOfBizarre, CORINFO_CLASS_HANDLE type, Object* obj);
FCDECL1(void, JIT_InternalThrow, unsigned exceptNum);

extern "C"
{
    VMHELPDEF hlpFuncTable[];
}



#ifdef MAXALLOC
extern "C" BOOL CheckAllocRequest(size_t n)
{
    return GetGCAllocManager()->CheckRequest(n);
}

extern "C" void UndoAllocRequest()
{
    GetGCAllocManager()->UndoRequest();
}
#endif // MAXALLOC


// Nonguaranteed attempt to allocate small, non-finalizer, non-array object.
// Will return NULL if it finds it must gc, block, or throw an exception
// or anything else that requires a frame to track callee-saved registers.
// It should call try_fast_alloc but the inliner does not do
// a perfect job, so we do it by hand.
#pragma optimize("t", on)
Object * __fastcall JIT_TrialAllocSFastSP(MethodTable *mt)
{
    _ASSERTE(!"Not implemented - JIT_TrialAllocSFastSP (JITinterfaceGen.cpp)");
    return NULL;
}
//    if (! CheckAllocRequest())
//        return NULL;
//    if (++m_GCLock == 0)
//    {
//        size_t size = Align (mt->GetBaseSize());
//        assert (size >= Align (min_obj_size));
//        generation* gen = pGenGCHeap->generation_of (0);
//        BYTE*  result = generation_allocation_pointer (gen);
//        generation_allocation_pointer (gen) += size;
//        if (generation_allocation_pointer (gen) <=
//            generation_allocation_limit (gen))
//        {
//            LeaveAllocLock();
//            ((Object*)result)->SetMethodTable(mt);
//            return (Object*)result;
//        }
//        else
//        {
//            generation_allocation_pointer (gen) -= size;
//            LeaveAllocLock();
//        }
//    }
//    UndoAllocRequest();
//    goto CORINFO_HELP_NEWFAST


Object * __fastcall JIT_TrialAllocSFastMP(MethodTable *mt)
{
    _ASSERTE(!"@UNDONE - JIT_TrialAllocSFastMP (JITinterface.cpp)");
    return NULL;
}


HCIMPL1(int, JIT_Dbl2IntOvf, double val)
{
    __int32 ret = (__int32) val;   // consider inlining the assembly for the cast
    _ASSERTE(!"@UNDONE - JIT_Dbl2IntOvf (JITinterface.cpp)");
    return ret;
}
HCIMPLEND


HCIMPL1(INT64, JIT_Dbl2LngOvf, double val)
{
    __int64 ret = (__int64) val;   // consider inlining the assembly for the cast
    _ASSERTE(!"@UNDONE - JIT_Dbl2LngOvf (JITinterface.cpp)");
    return ret;
}
HCIMPLEND


HCIMPL0(VOID, JIT_StressGC)
{
#ifdef _DEBUG
    HELPER_METHOD_FRAME_BEGIN_0();    // Set up a frame


    g_pGCHeap->GarbageCollect();

SKIP_GC:;

    HELPER_METHOD_FRAME_END();
#endif // _DEBUG
}
HCIMPLEND

/*********************************************************************/
// Initialize the part of the JIT helpers that require very little of
// EE infrastructure to be in place.
/*********************************************************************/
BOOL InitJITHelpers1()
{
    // Init GetThread function
    _ASSERTE(GetThread != NULL);
    hlpFuncTable[CORINFO_HELP_GET_THREAD].pfnHelper = (void *) GetThread;


    return TRUE;
}


/*********************************************************************/
// This is a frameless helper for entering a monitor on a object.
// The object is in ARGUMENT_REG1.  This tries the normal case (no
// blocking or object allocation) in line and calls a framed helper
// for the other cases.
HCIMPL1(void, JIT_MonEnter, Object* or)
{
    THROWSCOMPLUSEXCEPTION();

    if (or == 0)
        COMPlusThrow(kArgumentNullException, L"ArgumentNull_Obj");
    or->EnterObjMonitor();
}
HCIMPLEND

/***********************************************************************/
// This is a frameless helper for trying to enter a monitor on a object.
// The object is in ARGUMENT_REG1 and a timeout in ARGUMENT_REG2. This tries the
// normal case (no object allocation) in line and calls a framed helper for the
// other cases.
HCIMPL2(BOOL, JIT_MonTryEnter, Object* or, __int32 timeout)
{
    THROWSCOMPLUSEXCEPTION();

    if (or == 0)
        COMPlusThrow(kArgumentNullException, L"ArgumentNull_Obj");

    return or->TryEnterObjMonitor(timeout);
}
HCIMPLEND


/*********************************************************************/
// This is a frameless helper for exiting a monitor on a object.
// The object is in ARGUMENT_REG1.  This tries the normal case (no
// blocking or object allocation) in line and calls a framed helper
// for the other cases.
HCIMPL1(void, JIT_MonExit, Object* or)
{
    //
    // @TODO: implement for ia64
    //
    THROWSCOMPLUSEXCEPTION();

    if (or == 0)
        COMPlusThrow(kArgumentNullException, L"ArgumentNull_Obj");

    or->LeaveObjMonitor();
}
HCIMPLEND


/*********************************************************************/
// This is a frameless helper for entering a static monitor on a class.
// The methoddesc is in ARGUMENT_REG1.  This tries the normal case (no
// blocking or object allocation) in line and calls a framed helper
// for the other cases.
// Note we are changing the methoddesc parameter to a pointer to the
// AwareLock.
HCIMPL1(void, JIT_MonEnterStatic, AwareLock *lock)
{
    THROWSCOMPLUSEXCEPTION();

    _ASSERTE(lock);
    // no need to check for proxies, which is asserted inside the syncblock anyway
    lock->Enter();
}
HCIMPLEND


/*********************************************************************/
// A frameless helper for exiting a static monitor on a class.
// The methoddesc is in ARGUMENT_REG1.  This tries the normal case (no
// blocking or object allocation) in line and calls a framed helper
// for the other cases.
// Note we are changing the methoddesc parameter to a pointer to the
// AwareLock.
HCIMPL1(void, JIT_MonExitStatic, AwareLock *lock)
{
    THROWSCOMPLUSEXCEPTION();

    _ASSERTE(lock);
    // no need to check for proxies, which is asserted inside the syncblock anyway
    lock->Leave();
}
HCIMPLEND



FCIMPL1(StringObject*, FastAllocateStringGeneric, DWORD cchString)
{
    STRINGREF result;
    HELPER_METHOD_FRAME_BEGIN_RET_0();    // Set up a frame
    result = AllocateString(cchString+1);
    result->SetStringLength(cchString);
    HELPER_METHOD_FRAME_END();
    return((StringObject*) OBJECTREFToObject(result));
}
FCIMPLEND

extern "C" Object* __stdcall JIT_IsInstanceOfClassHelper(MethodTable* pMT, Object* pObject, BOOL bThrow)
{
    //
    // make certain common cases are already taken care of
    //
    _ASSERTE(NULL != pObject);
    _ASSERTE(pObject->GetMethodTable() != pMT);

    //
    // search for parent-class relationship
    //
    MethodTable* pMT1 = pMT;
    MethodTable* pMT2 = pObject->GetMethodTable();

    while (pMT2)
    {
        pMT2 = pMT1->GetParentMethodTable();
        if (pMT1 == pMT2)
        {
            return pObject;
        }
    }

    //
    // check to see if it's a proxy
    //
    if (pObject->GetMethodTable()->m_wFlags & (MethodTable::enum_CtxProxyMask | MethodTable::enum_TransparentProxy))
    {
        return JIT_IsInstanceOfClassWorker(ObjectToOBJECTREF(pObject), pMT, bThrow);
    }
    
    if (bThrow)
    {
        JIT_InternalThrow(CORINFO_InvalidCastException);
    }

    return NULL;
}

extern "C" Object* F_CALL_CONV JIT_IsInstanceOfClass(MethodTable* pMT, Object* pObject)
{
    //
    // handle common cases first
    //
    if (NULL == pObject)
    {
        return NULL;
    }

    if (pObject->GetMethodTable() == pMT)
    {
        return pObject;
    }

    return JIT_IsInstanceOfClassHelper(pMT, pObject, FALSE);
}


extern "C" Object* F_CALL_CONV JIT_ChkCastClass(MethodTable* pMT, Object* pObject)
{
    //
    // casts pObject to type pMT
    //

    if (NULL == pObject)
    {
        return NULL;
    }

    if (pObject->GetMethodTable() == pMT)
    {
        return pObject;
    }

    return JIT_IsInstanceOfClassHelper(pMT, pObject, TRUE);
}


extern "C" Object* F_CALL_CONV JIT_ChkCast(CORINFO_CLASS_HANDLE type, Object* pObject)
{
    if (NULL == pObject)
    {
        return pObject;
    }

    MethodTable*        pTypeMT     = (MethodTable*)type;
    MethodTable*        pObjMT      = pObject->GetMethodTable();
    
    for (int i = 0; i < pObjMT->m_wNumInterface; i++)
    {
        if (pObjMT->m_pIMap[i].m_pMethodTable == pTypeMT)
        {
            return pObject;
        }
    }

    return JITutil_ChkCastBizarre(type, pObject);
}

extern "C" Object* JIT_IsInstanceOf(CORINFO_CLASS_HANDLE type, Object* pObject)
{

    if (NULL == pObject)
    {
        return NULL;
    }

    MethodTable*        pMT         = pObject->GetMethodTable();
    WORD                wItfCnt     = pMT->m_wNumInterface;
    InterfaceInfo_t*    pItfInfo    = pMT->m_pIMap;

    while (wItfCnt)
    {
        if ((MethodTable*)type == pItfInfo->m_pMethodTable)
        {
            return pObject;
        }
        wItfCnt--;
        pItfInfo++;
    }

    return JITutil_IsInstanceOfBizarre(type, pObject);
}

