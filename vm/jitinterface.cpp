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
// File: JITinterface.CPP
//
// ===========================================================================

//
// JITinterface.CPP has been broken up to make it easier to port
// from platform to platform. The code is now organized as follows:
//
// JITinterface.CPP             - C Code that is the same across processors and platforms
// JITinterfaceGen.cpp          - Generic C versions of routines found in
//                                JIThelp.asm and/or JITinterfaceX86.cpp
// i386\JIThelp.asm             - Assembler code for the x86 platform
// Alpha\JIThelp.s              - Assembler code for the Alpha platform
// i386\JITinterfaceX86.cpp     - C code that is x86 processor specific
// Alpha\JITinterfaceAlpha.cpp  - C code that is Alpha processor specific
//
// New processor platforms that are added need only write custom
// versions of JITinterfaceXXX.cpp (where xxx is the processor id)
// optionally for performance new platforms could add new jithelp versions too.
//

#include "common.h"
#include "jitinterface.h"
#include "codeman.h"
#include "ejitmgr.h"
#include "method.hpp"
#include "class.h"
#include "object.h"
#include "field.h"
#include "stublink.h"
#include "corjit.h"
#include "eeconfig.h"
#include "comstring.h"
#include "excep.h"
#include "log.h"
#include "excep.h"
#include "float.h"      // for isnan
#include "dbginterface.h"
#include "gcscan.h"
#include "security.h"   // to get security method attribute
#include "ndirect.h"
#include "ml.h"
#include "gc.h"
#include "comdelegate.h"
#include "jitperf.h" // to track jit perf
#include "corprof.h"
#include "eeprofinterfaces.h"
#include "remoting.h" // create context bound and remote class instances
#include "perfcounters.h"
#ifdef PROFILING_SUPPORTED
#include "proftoeeinterfaceimpl.h"
#endif // PROFILING_SUPPORTED
#include "tls.h"
#include "ecall.h"
#include "comstringbuffer.h"
#include "comobject.h"
#include "asmconstants.h"
#include "generics.h"

#include "stackprobe.h"



#define StartCAP()
#define StopCAP()

#define JIT_LINKTIME_SECURITY


// **  NOTE  **  NOTE  **  NOTE  **  NOTE  **  NOTE  **  NOTE  **  NOTE  **  NOTE
//
// A word about EEClass vs. MethodTable
// ------------------------------------
//
// At compile-time, we are happy to touch both MethodTable and EEClass.  However,
// at runtime we want to restrict ourselves to the MethodTable.  This is critical
// for common code paths, where we want to keep the EEClass out of our working
// set.  For uncommon code paths, like throwing exceptions or strange Contexts
// issues, it's okay to access the EEClass.
//
// To this end, the TypeHandle (CORINFO_CLASS_HANDLE) abstraction is now based on the
// MethodTable pointer instead of the EEClass pointer.  If you are writing a
// runtime helper that calls GetClass() to access the associated EEClass, please
// stop to wonder if you are making a mistake.
//
// **  NOTE  **  NOTE  **  NOTE  **  NOTE  **  NOTE  **  NOTE  **  NOTE  **  NOTE



/*********************************************************************/

// Compile-time checks that the offsets declared in clr/src/vm/i386/asmconstants.h
// match the actual structure offsets.  If any of these lines generates a
// compile error:
//   An assertion failure results in error C2118: negative subscript.
//    or
//   size of array `__C_ASSERT__' is negative
// Then the offsets in asmconstants.h are out of sync with the C/C++ struct
C_ASSERT(ICodeManager::SHADOW_SP_IN_FILTER == SHADOW_SP_IN_FILTER_ASM);
#ifdef _X86_
C_ASSERT(CORINFO_HELP_INTERNALTHROW == CORINFO_HELP_INTERNALTHROW_ASM);
C_ASSERT(CORINFO_NullReferenceException == CORINFO_NullReferenceException_ASM);
C_ASSERT(CORINFO_IndexOutOfRangeException == CORINFO_IndexOutOfRangeException_ASM);
C_ASSERT(CORINFO_ArrayTypeMismatchException == CORINFO_ArrayTypeMismatchException_ASM);
C_ASSERT(CORINFO_RankException == CORINFO_RankException_ASM);
// No checks for ARRAYOPLOCSIZE and ARRAYOPLOCSIZEFORPOP - there is no C 
// equivalent to compare against.
C_ASSERT(MLHF_THISCALL == MLHF_THISCALL_ASM);
C_ASSERT(MLHF_THISCALLHIDDENARG == MLHF_THISCALLHIDDENARG_ASM);
#else // _X86_
C_ASSERT(METHOD_CALL_PRESTUB_SIZE == METHOD_CALL_PRESTUB_SIZE_ASM);
#endif // _X86_
C_ASSERT(offsetof(Thread, m_Context) == Thread_m_Context);
C_ASSERT(MDEnums::MD_IndexOffset == MD_IndexOffset_ASM);
C_ASSERT(MDEnums::MD_SkewOffset == MD_SkewOffset_ASM);
C_ASSERT(MethodDesc::ALIGNMENT == MethodDesc__ALIGNMENT);
C_ASSERT(offsetof(MethodTable,m_pEEClass) == MethodTable_m_pEEClass);
C_ASSERT(offsetof(MethodTable,m_pInterfaceVTableMap) == MethodTable_m_pInterfaceVTableMap);

class CheckAsmOffsetsCommon {
CPP_ASSERT(1, offsetof(MethodDesc, m_wSlotNumber) == MethodDesc_m_wSlotNumber);
CPP_ASSERT(2, offsetof(EEClass, m_dwInterfaceId) == EEClass_m_dwInterfaceId);
};

void JIT_BadHelper() {
    _ASSERTE(!"Bad Helper");
}


#if defined(ENABLE_PERF_COUNTERS)
LARGE_INTEGER g_lastTimeInJitCompilation;
#endif

// This functions are only called via a jmp from other FCALLs as a
// result they need to do their own stack clean up
FCDECL1(void, JITutil_MonContention, AwareLock* awarelock);
FCDECL2(BOOL, JITutil_MonTryEnter,  Object* obj, __int32 timeOut);

EXTERN_C void __stdcall JIT_EndCatch();               // JITinterfaceX86.cpp/JITinterfaceAlpha.cpp
FCDECL2(INT64, JIT_LMul, INT64 val1, INT64 val2);
FCDECL2(INT64, JIT_ULMul, UINT64 val1, UINT64 val2);
static unsigned __int64 __stdcall JIT_Dbl2ULng(double val);
static double __stdcall JIT_ULng2Dbl(unsigned __int64 val);
FCDECL3(void*, JIT_Ldelema_Ref, PtrArray* array, unsigned idx, CORINFO_CLASS_HANDLE type);
FCDECL1(void, JITutil_MonEnter,  Object* obj);
FCDECL1(void, JITutil_MonExit,  AwareLock* lock);
FCDECL1(void, JITutil_MonEnterStatic,  EEClass* pclass);
FCDECL3(void, JIT_Stelem_Ref, PtrArray* array, unsigned idx, Object* val);
FCDECL2(LPVOID, JIT_Unbox, CORINFO_CLASS_HANDLE type, Object* obj);
FCDECL2(LPVOID, JIT_GetRefAny, CORINFO_CLASS_HANDLE type, TypedByRef typedByRef);
FCDECL1(Object*, JIT_NewFast, CORINFO_CLASS_HANDLE typeHnd_);
FCDECL1(Object*, JIT_NewCrossContext, CORINFO_CLASS_HANDLE typeHnd_);
FCDECL1(void, JIT_InitClass, CORINFO_CLASS_HANDLE typeHnd_);
FCDECL2(Object*, JIT_NewArr1, CORINFO_CLASS_HANDLE typeHnd_, int size);

FCDECL2(Object *, JIT_StrCns, unsigned metaTok, CORINFO_MODULE_HANDLE scopeHnd);
FCDECL2(Object*, JIT_Box, CORINFO_CLASS_HANDLE type, void* data);
FCDECL2(INT64, JIT_LDiv, INT64 divisor, INT64 dividend);
FCDECL2(INT64, JIT_LMod, INT64 divisor, INT64 dividend);
FCDECL2(UINT64, JIT_ULDiv, UINT64 divisor, UINT64 dividend);
FCDECL2(UINT64, JIT_ULMod, UINT64 divisor, UINT64 dividend);
FCDECL2(INT64, JIT_LMulOvf, INT64 val2, INT64 val1);
FCDECL2(INT64, JIT_ULMulOvf, UINT64 val2, UINT64 val1);
FCDECL1(Object*, JIT_NewSpecial, CORINFO_CLASS_HANDLE typeHnd_);
FCDECL1(Object*, JIT_New, CORINFO_CLASS_HANDLE typeHnd_);
FCDECL1(Object*, JIT_NewString, unsigned length);
FCDECL1(void, JIT_RareDisableHelper, Thread* thread);
FCDECL1(int, JIT_Dbl2IntOvf, double val);    // JITinterfaceX86.cpp/JITinterfaceGen.cpp
FCDECL1(INT64, JIT_Dbl2LngOvf, double val);  // JITinterfaceX86.cpp/JITinterfaceGen.cpp
FCDECL1(unsigned, JIT_Dbl2UIntOvf, double val);
FCDECL1(UINT64, JIT_Dbl2ULngOvf, double val);
FCDECL1(void, JIT_InitClass_Framed, MethodTable* pMT);

FCDECL2(Object*, JITutil_ChkCastBizarre, CORINFO_CLASS_HANDLE type, Object *obj);

Object* __fastcall JIT_TrialAllocSFastSP(MethodTable *mt);   // JITinterfaceX86.cpp/JITinterfaceGen.cpp
Object* __fastcall JIT_TrialAllocSFastMP(MethodTable *mt);   // JITinterfaceX86.cpp/JITinterfaceGen.cpp
FCDECL0(VOID, JIT_StressGC);                 // JITinterfaceX86.cpp/JITinterfaceGen.cpp
FCDECL1(Object*, JIT_CheckObj, Object* obj); 
FCDECL0(void, JIT_UserBreakpoint);
Object* __cdecl JIT_NewObj(CORINFO_MODULE_HANDLE classHnd, CORINFO_METHOD_HANDLE constrMeth, ...);
BOOL InitJITHelpers1();

FCDECL2(double, JIT_FloatDiv,  double a, double b);
FCDECL2(double, JIT_FloatMul,  double a, double b);
FCDECL2(double, JIT_DoubleDiv, double a, double b);

extern "C"
{


    Object *  F_CALL_CONV JIT_IsInstanceOf(CORINFO_CLASS_HANDLE type, Object *pObject);  // JITInterfaceX86.cpp, etc.


    Object* F_CALL_CONV JIT_ChkCast(CORINFO_CLASS_HANDLE type, Object *pObject);
    Object* F_CALL_CONV JIT_ChkCastClass(MethodTable *pMT, Object *pObject);
    Object* F_CALL_CONV JIT_IsInstanceOfClass(MethodTable *pMT, Object *pObject);

    void __stdcall JIT_TailCall();                    // JIThelp.asm/JITinterfaceAlpha.cpp
    void __stdcall JIT_MemSet();
    void __stdcall JIT_MemCpy();
    void __stdcall JIT_ProfilerStub(UINT_PTR ProfilingHandle);
}

#ifdef _X86_
extern BYTE JIT_WriteBarrierReg_Buf[8][32];
#endif // defined(_X86_)

/*********************************************************************/

inline Module* GetModule(CORINFO_MODULE_HANDLE scope) {
    return((Module*)scope);
}

inline CORINFO_MODULE_HANDLE GetScopeHandle(Module* module) {
    return(CORINFO_MODULE_HANDLE(module));
}

inline CORINFO_MODULE_HANDLE GetScopeHandle(MethodDesc* method) {
    return GetScopeHandle(method->GetModule());
}


inline HRESULT __stdcall CEEJitInfo::alloc (
        ULONG code_len, unsigned char** ppCode,
        ULONG EHinfo_len, unsigned char** ppEHinfo,
        ULONG GCinfo_len, unsigned char** ppGCinfo
        )
{
    CANNOTTHROWCOMPLUSEXCEPTION();

    return m_jitManager->alloc(code_len, ppCode, EHinfo_len, ppEHinfo,
                               GCinfo_len, ppGCinfo, m_FD);
}

/*********************************************************************/

// This normalizes EE type information into the form
// expected by the JIT.
inline static CorInfoType toJitType(CorElementType eeType, 
                                    TypeHandle typeHnd = TypeHandle() /* optional in */, 
                                    CORINFO_CLASS_HANDLE *clsRet = NULL /* optional out */ ) {

    _ASSERTE(!(eeType == ELEMENT_TYPE_VAR && typeHnd.IsNull()));
    _ASSERTE(!(eeType == ELEMENT_TYPE_MVAR && typeHnd.IsNull()));
    _ASSERTE(eeType != ELEMENT_TYPE_WITH);

    TypeHandle typeHndUpdated = typeHnd;

    if (!typeHnd.IsNull())
    {
        // GenerivVariables are used during the verification of 
        // generic code and for reflection.  
    
        if (typeHnd.IsGenericVariable())
            eeType = ELEMENT_TYPE_VAR;

        CorElementType normType = typeHnd.GetNormCorElementType();
        // If we have a type handle, then it has the better type 
        // in some cases
        if (eeType == ELEMENT_TYPE_VALUETYPE && !CorTypeInfo::IsObjRef(normType))
            eeType = normType;

        // Zap the typeHnd when the type _really_ is a primitive 
        // as far as verification is concerned. Returning a null class 
        // handle means it is is a primitive.  
        //
        // Enums are exactly like primitives, even from a verification standpoint,
        // so we zap the type handle in this case.
        //
        // However RuntimeTypeHandle etc. are reported as E_T_INT (or something like that)
        // but don't count as primitives as far as verification is concerned...
        //
        // To make things stranger, TypedReference returns true for "IsTruePrimitive".
        // However the JIT likes us to report the type handle in that case.
        if ((typeHnd.IsUnsharedMT() && 
             typeHnd.GetClass()->IsTruePrimitive() && 
             typeHnd.GetSigCorElementType() != ELEMENT_TYPE_TYPEDBYREF) 
            || typeHnd.IsEnum())
        {
            typeHndUpdated = TypeHandle(); 
        }
    }

    static const BYTE map[] = {
        CORINFO_TYPE_UNDEF,
        CORINFO_TYPE_VOID,
        CORINFO_TYPE_BOOL,
        CORINFO_TYPE_CHAR,
        CORINFO_TYPE_BYTE,
        CORINFO_TYPE_UBYTE,
        CORINFO_TYPE_SHORT,
        CORINFO_TYPE_USHORT,
        CORINFO_TYPE_INT,
        CORINFO_TYPE_UINT,
        CORINFO_TYPE_LONG,
        CORINFO_TYPE_ULONG,
        CORINFO_TYPE_FLOAT,
        CORINFO_TYPE_DOUBLE,
        CORINFO_TYPE_STRING,
        CORINFO_TYPE_PTR,            // PTR
        CORINFO_TYPE_BYREF,
        CORINFO_TYPE_VALUECLASS,
        CORINFO_TYPE_CLASS,
        CORINFO_TYPE_VAR,            // VAR (type variable)
        CORINFO_TYPE_CLASS,          // ARRAY
        CORINFO_TYPE_CLASS,          // WITH
        CORINFO_TYPE_REFANY,
        CORINFO_TYPE_VALUECLASS,     // VALUEARRAY
        CORINFO_TYPE_INT,            // I
        CORINFO_TYPE_UINT,           // U
        CORINFO_TYPE_DOUBLE,         // R

        // put the correct type when we know our implementation
        CORINFO_TYPE_PTR,            // FNPTR
        CORINFO_TYPE_CLASS,          // OBJECT
        CORINFO_TYPE_CLASS,          // SZARRAY
        CORINFO_TYPE_VAR,            // MVAR

        CORINFO_TYPE_UNDEF,          // CMOD_REQD
        CORINFO_TYPE_UNDEF,          // CMOD_OPT
        CORINFO_TYPE_UNDEF,          // INTERNAL
        };

    _ASSERTE(sizeof(map) == ELEMENT_TYPE_MAX);
    _ASSERTE(eeType < (CorElementType) sizeof(map));
        // spot check of the map
    _ASSERTE((CorInfoType) map[ELEMENT_TYPE_I4] == CORINFO_TYPE_INT);
    _ASSERTE((CorInfoType) map[ELEMENT_TYPE_VALUEARRAY] == CORINFO_TYPE_VALUECLASS);
    _ASSERTE((CorInfoType) map[ELEMENT_TYPE_PTR] == CORINFO_TYPE_PTR);
    _ASSERTE((CorInfoType) map[ELEMENT_TYPE_TYPEDBYREF] == CORINFO_TYPE_REFANY);
    _ASSERTE((CorInfoType) map[ELEMENT_TYPE_R] == CORINFO_TYPE_DOUBLE);

    CorInfoType res = ((CorInfoType) map[eeType]);

    if (clsRet)
        *clsRet = CORINFO_CLASS_HANDLE(typeHndUpdated.AsPtr());
    return res;
}


inline static CorInfoType toJitType(TypeHandle typeHnd, CORINFO_CLASS_HANDLE *clsRet = NULL)
{
    return toJitType(typeHnd.GetNormCorElementType(), typeHnd, clsRet);
}

/*********************************************************************/
//@GENERICS: 
// The method handle is used to instantiate method and class type parameters
// It's also used to determine whether an extra dictionary parameter is required
/*********************************************************************/
static HRESULT ConvToJitSig(PCCOR_SIGNATURE sig, CORINFO_MODULE_HANDLE scopeHnd, mdToken token, CORINFO_SIG_INFO* sigRet, CORINFO_METHOD_HANDLE context = NULL, bool localSig=false, TypeHandle owner = TypeHandle())
{    
    THROWSCOMPLUSEXCEPTION();

    TypeHandle* classInst = NULL;
    TypeHandle* methodInst = NULL;

    if (context)
    {
      MethodDesc* md = (MethodDesc*)context;
      if (md->GetMethodTable()->IsArray())
          classInst = md->GetMethodTable()->GetClassOrArrayInstantiation();
      else
          // Use class instantiation of any provided owner
          classInst = md->GetClassInstantiation(owner);

      methodInst = md->GetMethodInstantiation();
    }

    SigPointer ptr(sig);
    _ASSERTE(CORINFO_CALLCONV_DEFAULT == (CorInfoCallConv) IMAGE_CEE_CS_CALLCONV_DEFAULT);
    _ASSERTE(CORINFO_CALLCONV_VARARG == (CorInfoCallConv) IMAGE_CEE_CS_CALLCONV_VARARG);
    _ASSERTE(CORINFO_CALLCONV_MASK == (CorInfoCallConv) IMAGE_CEE_CS_CALLCONV_MASK);
    _ASSERTE(CORINFO_CALLCONV_HASTHIS == (CorInfoCallConv) IMAGE_CEE_CS_CALLCONV_HASTHIS);

    TypeHandle typeHnd = TypeHandle();

    sigRet->sig = (CORINFO_SIG_HANDLE) sig;
    sigRet->retTypeClass = 0;
    sigRet->retTypeSigClass = 0;
    sigRet->scope = scopeHnd;
    sigRet->token = token;
    sigRet->classInst = (CORINFO_CLASS_HANDLE *) classInst;
    sigRet->methodInst = (CORINFO_CLASS_HANDLE *) methodInst;

    if (!localSig) {
        _ASSERTE(sig != 0);
        Module* module = GetModule(scopeHnd);
        sigRet->callConv = (CorInfoCallConv) ptr.GetCallingConvInfo();
	// Skip number of type arguments
        if (sigRet->callConv & IMAGE_CEE_CS_CALLCONV_GENERIC)
          ptr.GetData();
        sigRet->numArgs = (unsigned short) ptr.GetData();

        CorElementType type = ptr.PeekElemType();
        if (!CorTypeInfo::IsPrimitiveType(type)) {
            OBJECTREF Throwable = NULL;
            GCPROTECT_BEGIN(Throwable);
            typeHnd = ptr.GetTypeHandle(module, &Throwable, FALSE, FALSE, FALSE, (TypeHandle*) classInst, (TypeHandle*) methodInst);
            if (typeHnd.IsNull()) {
                if (Throwable == NULL)
                    COMPlusThrow(kTypeLoadException);
                else
                    COMPlusThrow(Throwable);
            }
            GCPROTECT_END();

	        type = typeHnd.GetSigCorElementType();
 
        }
        sigRet->retType = toJitType(type, typeHnd, &sigRet->retTypeClass);
        sigRet->retTypeSigClass = CORINFO_CLASS_HANDLE(typeHnd.AsPtr());
 
        ptr.Skip();     // must to a skip so we skip any class tokens associated with the return type
        _ASSERTE(sigRet->retType < CORINFO_TYPE_COUNT);

        // I pass a SigPointer as a CORINFO_ARG_LIST_HANDLE and back
        _ASSERTE(sizeof(SigPointer) == sizeof(BYTE*));
        sigRet->args =  *((CORINFO_ARG_LIST_HANDLE*) (&ptr));

        // Force a load of value type arguments.  This is necessary because the
        // JIT doens't ask about argument types before making a call, but
        // the garbage collector will need to have them loaded so that
        // it can walk the stack during collection.
        //
        for(unsigned i=0; i < sigRet->numArgs; i++) {
            unsigned type = ptr.Normalize(module, (TypeHandle*) classInst, (TypeHandle*) methodInst);
            if (type == ELEMENT_TYPE_VALUETYPE) {
                OBJECTREF throwable = NULL;
                GCPROTECT_BEGIN(throwable);

                TypeHandle typeHnd = ptr.GetTypeHandle(module, &throwable, FALSE, FALSE, FALSE, (TypeHandle*) classInst, (TypeHandle*) methodInst);
                if (typeHnd.IsNull()) {
                    _ASSERTE(throwable != NULL);
                    COMPlusThrow(throwable);
                }
                _ASSERTE(typeHnd.GetClass()->IsValueClass());
                GCPROTECT_END();
            }
            ptr.Skip();
        }

    } else {
        sigRet->callConv = CORINFO_CALLCONV_DEFAULT;
        sigRet->retType = CORINFO_TYPE_VOID;

        sigRet->numArgs = 0;
        if (sig != 0) {
            BAD_FORMAT_ASSERT(*sig == IMAGE_CEE_CS_CALLCONV_LOCAL_SIG);
            if (*sig != IMAGE_CEE_CS_CALLCONV_LOCAL_SIG)
                COMPlusThrowHR(COR_E_BADIMAGEFORMAT);
            ptr.GetData();
            sigRet->numArgs = (unsigned short) ptr.GetData();
        }

        // I pass a SigPointer as a CORINFO_ARG_LIST_HANDLE and back
        _ASSERTE(sizeof(SigPointer) == sizeof(BYTE*));
        sigRet->args =  *((CORINFO_ARG_LIST_HANDLE*) (&ptr));
    }

    return S_OK;
}

/*********************************************************************/
CORINFO_GENERIC_HANDLE __stdcall CEEInfo::findToken (
            CORINFO_MODULE_HANDLE  scopeHnd,
            unsigned      metaTOK,
            CORINFO_METHOD_HANDLE context,
            CORINFO_CLASS_HANDLE& tokenType     // field, method, etc. 
            )
{
    REQUIRES_4K_STACK;
    CORINFO_GENERIC_HANDLE ret = NULL;

    COOPERATIVE_TRANSITION_BEGIN();

    THROWSCOMPLUSEXCEPTION();

    mdToken     tokType = TypeFromToken(metaTOK);
    InitStaticTypeHandles();

    switch (tokType)
    {
    case mdtTypeRef:
    case mdtTypeDef:
    case mdtTypeSpec:
        ret = CORINFO_GENERIC_HANDLE(findClass(scopeHnd, metaTOK, context));
        tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeTypeHandle);
        break;

    case mdtMethodSpec:
        ret = CORINFO_GENERIC_HANDLE(findMethod(scopeHnd, metaTOK, context));
        tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeMethodHandle);
        break;

    case mdtMemberRef:
        {
            // OK, we have to look at the metadata to see if it's a field or method

            Module *pModule = (Module*)scopeHnd;

            PCCOR_SIGNATURE pSig;
            ULONG cSig;
            pModule->GetMDImport()->GetNameAndSigOfMemberRef(metaTOK, &pSig, &cSig);

        if (isCallConv(MetaSig::GetCallingConventionInfo(pModule, pSig), IMAGE_CEE_CS_CALLCONV_FIELD))
            {
                ret = CORINFO_GENERIC_HANDLE(findField(scopeHnd, metaTOK, context));
                tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeFieldHandle);
            }
            else
            {
                ret = CORINFO_GENERIC_HANDLE(findMethod(scopeHnd, metaTOK, context));
                tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeMethodHandle);
            }
        }
        break;

    case mdtMethodDef:
        ret = CORINFO_GENERIC_HANDLE(findMethod(scopeHnd, metaTOK, context));
        tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeMethodHandle);
        break;

    case mdtFieldDef:
        ret = CORINFO_GENERIC_HANDLE(findField(scopeHnd, metaTOK, context));
        tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeFieldHandle);
        break;

    default:
        BAD_FORMAT_ASSERT(!"Error, bad token type");
        COMPlusThrowHR(COR_E_BADIMAGEFORMAT);
    }

    COOPERATIVE_TRANSITION_END();
    return ret;
}
/*********************************************************************/
const char * __stdcall CEEInfo::findNameOfToken (
            CORINFO_MODULE_HANDLE       scopeHnd,
            mdToken                     metaTOK)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();  

    Module* module = GetModule(scopeHnd);
    return findNameOfToken(module, metaTOK);
}
/*********************************************************************/
BOOL __stdcall CEEInfo::canSkipVerification(CORINFO_MODULE_HANDLE moduleHnd)
{
    BOOL ret;
    COOPERATIVE_TRANSITION_BEGIN();

    THROWSCOMPLUSEXCEPTION();
    ret = Security::CanSkipVerification(GetModule(moduleHnd));

    if (ret)
        GetAppDomain()->OnLinktimeCanSkipVerificationCheck(GetModule(moduleHnd));

    COOPERATIVE_TRANSITION_END();
    return ret;
}
/*********************************************************************/
// Checks if the given metadata token is valid
BOOL __stdcall CEEInfo::isValidToken (
        CORINFO_MODULE_HANDLE       module,
        mdToken                     metaTOK)
{
    return (GetModule(module))->GetMDImport()->IsValidToken(metaTOK);
}

/*********************************************************************/
// Checks if the given metadata token is valid StringRef
BOOL __stdcall CEEInfo::isValidStringRef (
        CORINFO_MODULE_HANDLE       module,
        mdToken                     metaTOK)
{
    return (GetModule(module))->IsValidStringRef(metaTOK);
}

/* static */
const char * __stdcall CEEInfo::findNameOfToken (Module* module,
                                                 mdToken metaTOK)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();  

    PCCOR_SIGNATURE sig = NULL;
    DWORD           cSig;

    mdToken tokType = TypeFromToken(metaTOK);
    if (tokType == mdtMemberRef)
        return (char *) module->GetMDImport()->GetNameAndSigOfMemberRef((mdMemberRef)metaTOK, &sig, &cSig);
    else if (tokType == mdtMethodDef)
        return (char*) module->GetMDImport()->GetNameOfMethodDef(metaTOK);

#ifdef _DEBUG
        static char buff[64];
        sprintf(buff, "Meta Token %#x", metaTOK);
    return buff;
#else // !_DEBUG
    return "<UNKNOWN>";
#endif // _DEBUG
}

/*********************************************************************/
CORINFO_CLASS_HANDLE __stdcall CEEInfo::findClass (
            CORINFO_MODULE_HANDLE  scopeHnd,
            unsigned      metaTOK,
            CORINFO_METHOD_HANDLE context)
{
    REQUIRES_4K_STACK;
    CORINFO_CLASS_HANDLE ret;

    COOPERATIVE_TRANSITION_BEGIN();
    THROWSCOMPLUSEXCEPTION();

    if (TypeFromToken(metaTOK) != mdtTypeDef && 
        TypeFromToken(metaTOK) != mdtTypeRef &&
        TypeFromToken(metaTOK) != mdtTypeSpec &&
        TypeFromToken(metaTOK) != mdtModuleRef)
    {
        BAD_FORMAT_ASSERT(!"Error, bad class token");
        COMPlusThrowHR(COR_E_BADIMAGEFORMAT);
    }

    Module* module = GetModule(scopeHnd);
    TypeHandle* classInst = NULL;
    TypeHandle* methodInst = NULL;
    if (context)
    {
      MethodDesc* md = (MethodDesc*) context;
      classInst = md->GetClassInstantiation();
      methodInst = md->GetMethodInstantiation();
    }

    OBJECTREF throwable = NULL;
    GCPROTECT_BEGIN(throwable);

    TypeHandle clsHnd;
    if (TypeFromToken(metaTOK) == mdtModuleRef)
    { Module * thatModule;
      HRESULT hr = module->GetAssembly()->FindModuleByModuleRef(module->GetMDImport(),
								metaTOK,
								mdModuleRefNil,
								&thatModule,
								&throwable);
      if (SUCCEEDED(hr))
        { clsHnd = TypeHandle(thatModule->GetMethodTable());}
    }
    else 
    { NameHandle name(module, metaTOK);
      clsHnd = module->GetClassLoader()->LoadTypeHandle(&name, &throwable, classInst, methodInst);
    };

    ret = CORINFO_CLASS_HANDLE(clsHnd.AsPtr());

     // make certain m_BaseSize is aligned properly
    _ASSERTE(clsHnd.IsNull() || !clsHnd.IsUnsharedMT() ||
             clsHnd.GetMethodTable()->GetBaseSize() % sizeof(void*) == 0);

#ifdef JIT_LINKTIME_SECURITY


    if (throwable != NULL)
        COMPlusThrow(throwable);

#endif // JIT_LINKTIME_SECURITY
    GCPROTECT_END();
    COOPERATIVE_TRANSITION_END();
    return ret;
}

// Modified to do the right thing for MethodSpecs too
// Given a field or method token metaTOK return its parent token
unsigned __stdcall CEEInfo::getMemberParent(CORINFO_MODULE_HANDLE scopeHnd, unsigned metaTOK)
{
    COOPERATIVE_TRANSITION_BEGIN();

    Module* module = GetModule(scopeHnd);

    OBJECTREF pThrowable = NULL;
    GCPROTECT_BEGIN(pThrowable);


    // Extended to extract parent class of a method spec
    if (TypeFromToken(metaTOK) == mdtMethodSpec) 
    {
      PCCOR_SIGNATURE pSig;
      ULONG cSig;
      mdMemberRef GenericMemberRef;
      module->GetMDImport()->GetMethodSpecProps(metaTOK, &GenericMemberRef, &pSig, &cSig);
      _ASSERTE(TypeFromToken(GenericMemberRef) == mdtMethodDef || TypeFromToken(GenericMemberRef) == mdtMemberRef);
      metaTOK = GenericMemberRef;
    }


    // Extended to extract parent class of a member-ref, method-def or field-def

    if (TypeFromToken(metaTOK) == mdtMemberRef)
    {
      metaTOK = module->GetMDImport()->GetParentOfMemberRef(metaTOK);
    }
    else if (TypeFromToken(metaTOK) == mdtMethodDef || TypeFromToken(metaTOK) == mdtFieldDef)
    {
      module->GetMDImport()->GetParentToken(metaTOK,&metaTOK);
    }

    _ASSERTE(TypeFromToken(metaTOK) == mdtTypeDef ||
             TypeFromToken(metaTOK) == mdtTypeRef ||
             TypeFromToken(metaTOK) == mdtTypeSpec ||
             TypeFromToken(metaTOK) == mdtModuleRef 
	        );

    GCPROTECT_END();
    COOPERATIVE_TRANSITION_END();

    return metaTOK;
}


/*********************************************************************/
CORINFO_FIELD_HANDLE __stdcall CEEInfo::findField (
            CORINFO_MODULE_HANDLE  scopeHnd,
            unsigned      metaTOK,
            CORINFO_METHOD_HANDLE  context
                        )
{
    REQUIRES_4K_STACK;

    FieldDesc* fieldDesc = 0;
    MethodDesc* method = (MethodDesc*) context;
    TypeHandle* classInst = NULL;
    TypeHandle* methodInst = NULL;
    if (context)
    {
      classInst = method->GetClassInstantiation();
      methodInst = method->GetMethodInstantiation();
    }

    COOPERATIVE_TRANSITION_BEGIN();
    THROWSCOMPLUSEXCEPTION();

    if (TypeFromToken(metaTOK) != mdtFieldDef && 
        TypeFromToken(metaTOK) != mdtMemberRef)
    {
        BAD_FORMAT_ASSERT(!"Error, bad field token");
        COMPlusThrowHR(COR_E_BADIMAGEFORMAT);
    }

    Module* module = GetModule(scopeHnd);

    START_NON_JIT_PERF();
    OBJECTREF throwable = NULL;
    GCPROTECT_BEGIN(throwable);
    HRESULT res = EEClass::GetFieldDescFromMemberRef(module, (mdMemberRef) metaTOK, &fieldDesc, &throwable, classInst, methodInst);

    if (SUCCEEDED(res))
    {
        if (method != NULL)
        {
            // Check private/protected etc. access
            if (ClassLoader::CanAccessField(method,
                                            fieldDesc) == FALSE)
            {
                CreateFieldExceptionObject(kFieldAccessException, fieldDesc, &throwable);
                fieldDesc = NULL;
            }

        }
    }

    STOP_NON_JIT_PERF();

#ifdef JIT_LINKTIME_SECURITY


    if (throwable != NULL && fieldDesc == NULL)
        COMPlusThrow(throwable);

#endif // JIT_LINKTIME_SECURITY

    GCPROTECT_END();
    COOPERATIVE_TRANSITION_END();
    return(CORINFO_FIELD_HANDLE(fieldDesc));
}

/*********************************************************************/
void* __stdcall CEEInfo::findPtr(CORINFO_MODULE_HANDLE  scopeHnd, unsigned ptrTOK)
{
    // Currently not used
    _ASSERTE(!"If this is used, the callees need to worry about endianness");
    return NULL;
}

CORINFO_METHOD_HANDLE __stdcall CEEInfo::findMethod(CORINFO_MODULE_HANDLE  scopeHnd, unsigned annotatedMetaTOK, CORINFO_METHOD_HANDLE context)
{
    REQUIRES_8K_STACK;

    MethodDesc* funcDesc = 0;
    HRESULT res = S_OK;

    unsigned metaTOK = annotatedMetaTOK & ~CORINFO_ANNOT_MASK;

    COOPERATIVE_TRANSITION_BEGIN();
    THROWSCOMPLUSEXCEPTION();     

    if (TypeFromToken(metaTOK) != mdtMethodDef && 
        TypeFromToken(metaTOK) != mdtMethodSpec &&
        TypeFromToken(metaTOK) != mdtMemberRef)
    {
        BAD_FORMAT_ASSERT(!"Error, bad method token");
        COMPlusThrowHR(COR_E_BADIMAGEFORMAT);
    }

    Module* module = GetModule(scopeHnd);
    MethodDesc* method = (MethodDesc*)context;

    START_NON_JIT_PERF();

    TypeHandle* classInst = NULL;
    TypeHandle* methodInst = NULL;
    if (context)
    {
      classInst = method->GetClassInstantiation();
      methodInst = method->GetMethodInstantiation();
    }

    OBJECTREF throwable = NULL;
    GCPROTECT_BEGIN(throwable);

    res = EEClass::GetMethodDescFromMemberRef(module, (mdMemberRef) metaTOK,(MethodDesc **) &funcDesc, &throwable, classInst, methodInst,
      (annotatedMetaTOK & CORINFO_ANNOT_ALLOWINSTPARAM) != 0);
    if (SUCCEEDED(res))
    {
        if (method != NULL)
        {
            if (ClassLoader::CanAccessMethod(method,
                                             funcDesc) == FALSE)
            {
#ifdef _DEBUG
                if(method && funcDesc) {
                    LOG((LF_CLASSLOADER,
                         LL_INFO10,
                         "\tERROR: Method %s:%s does not have access to %s:%s\n",
                         method->m_pszDebugClassName,
                         method->m_pszDebugMethodName,
                         funcDesc->m_pszDebugClassName,
                         funcDesc->m_pszDebugMethodName));
                }
#endif


                CreateMethodExceptionObject(kMethodAccessException, funcDesc, &throwable);
                funcDesc = NULL;
                goto exit;
            }

#ifdef JIT_LINKTIME_SECURITY
            if (funcDesc->RequiresLinktimeCheck() &&
                (Security::LinktimeCheckMethod(method->GetAssembly(), funcDesc, &throwable) == FALSE))
            {
                _ASSERTE(Security::IsSecurityOn());

                funcDesc = NULL;
                goto exit;
            }
#endif // JIT_LINKTIME_SECURITY
        }
        // we can ignore the result, if it fails, methodDesc == 0
        goto exit;
    }
    else
    {


    }
exit:
    STOP_NON_JIT_PERF();


#ifdef JIT_LINKTIME_SECURITY


    if (throwable != NULL && funcDesc == NULL)
        COMPlusThrow(throwable);

#endif // JIT_LINKTIME_SECURITY


    GCPROTECT_END();
    COOPERATIVE_TRANSITION_END();
    return CORINFO_METHOD_HANDLE(funcDesc);
}

/*********************************************************************/
void __stdcall CEEInfo::findCallSiteSig (
        CORINFO_MODULE_HANDLE       scopeHnd,
        unsigned                    sigMethTok,
        CORINFO_METHOD_HANDLE       context,
        CORINFO_SIG_INFO *          sigRet)
{
    REQUIRES_4K_STACK;

    COOPERATIVE_TRANSITION_BEGIN();
	THROWSCOMPLUSEXCEPTION();

    Module* module = GetModule(scopeHnd);
    LPCUTF8         szName;
    PCCOR_SIGNATURE sig = NULL;
    DWORD           cSig;

    if (TypeFromToken(sigMethTok) == mdtMemberRef)
    {
        szName = module->GetMDImport()->GetNameAndSigOfMemberRef(sigMethTok, &sig, &cSig);

			// Defs have already been checked by the loader for validity
			// However refs need to be checked.
		if (!Security::LazyCanSkipVerification(module)) 
		{
				// Can pass 0 for the flags, since it is only used for defs.  
			HRESULT hr = validateTokenSig(sigMethTok, sig, cSig, 0, module->GetMDImport());
			if (FAILED(hr) && !Security::CanSkipVerification(module))
				COMPlusThrow(kInvalidProgramException);
		}
    }
    else if (TypeFromToken(sigMethTok) == mdtMethodDef)
    {
        sig = module->GetMDImport()->GetSigOfMethodDef(sigMethTok, &cSig);

    }

#ifdef _DEBUG
    HRESULT hr =
#endif
    ConvToJitSig(sig, scopeHnd, sigMethTok, sigRet, context, false);
    _ASSERTE(SUCCEEDED(hr));

    COOPERATIVE_TRANSITION_END();
}

/*********************************************************************/
void __stdcall CEEInfo::findSig (
        CORINFO_MODULE_HANDLE  scopeHnd,
        unsigned               sigTok,
        CORINFO_METHOD_HANDLE  context,
        CORINFO_SIG_INFO *     sigRet)
{
    REQUIRES_4K_STACK;

    COOPERATIVE_TRANSITION_BEGIN();

    START_NON_JIT_PERF();

    Module* module = GetModule(scopeHnd);
    PCCOR_SIGNATURE sig = NULL;
    DWORD cSig = 0;

    // We need to resolve this stand alone sig
    sig = module->GetMDImport()->GetSigFromToken((mdSignature)sigTok,
                                                &cSig);
    STOP_NON_JIT_PERF();
#ifdef _DEBUG
    HRESULT hr =
#endif
    ConvToJitSig(sig, scopeHnd, sigTok, sigRet,context,false);
    _ASSERTE(SUCCEEDED(hr));
    COOPERATIVE_TRANSITION_END();
}

/*********************************************************************/
DWORD __stdcall CEEInfo::getModuleAttribs (CORINFO_MODULE_HANDLE scopeHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    return(0);
}

/*********************************************************************/
unsigned __stdcall CEEInfo::getClassSize (CORINFO_CLASS_HANDLE clsHnd)
{
    REQUIRES_4K_STACK;

    unsigned ret;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (isValueArray(clsHnd)) {
        ValueArrayDescr* valArr = asValueArray(clsHnd);
        ret = valArr->sig.SizeOf(valArr->module);
    } else {
        TypeHandle VMClsHnd(clsHnd);
        ret = VMClsHnd.GetSize();
    }
    return ret;
}

/*********************************************************************/
unsigned __stdcall CEEInfo::getClassGClayout (CORINFO_CLASS_HANDLE clsHnd, BYTE* gcPtrs)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();
    unsigned ret;

    if (isValueArray(clsHnd)) {
        ValueArrayDescr* valArr = asValueArray(clsHnd);

        SigPointer elemType;
        ULONG count;
        valArr->sig.GetSDArrayElementProps(&elemType, &count);
        CorElementType type = elemType.PeekElemType();

        switch(GetElementTypeInfo(type)->m_gc) {
            case TYPE_GC_NONE: {
                unsigned numSlots = (elemType.SizeOf(valArr->module) * count + sizeof(void*)-1) / sizeof(void*);
                memset(gcPtrs, TYPE_GC_NONE, numSlots);
                ret = 0;
                }
                break;
            case TYPE_GC_REF:
                memset(gcPtrs, TYPE_GC_REF, count);
                ret = count;
                break;
            case TYPE_GC_OTHER:
            case TYPE_GC_BYREF:
            default:
                _ASSERTE(!"ILLEGAL");
                ret = 0;
                break;
        }
    }
    else {
        TypeHandle VMClsHnd(clsHnd);

        if (g_Mscorlib.IsClass(VMClsHnd.AsMethodTable(), CLASS__TYPED_REFERENCE))
        {
            gcPtrs[0] = TYPE_GC_BYREF;
            gcPtrs[1] = TYPE_GC_NONE;
            ret = 1;
        }
        else
        {
            EEClass* cls = VMClsHnd.GetClass();

            // Sanity test for class
            _ASSERTE(cls);
            _ASSERTE(cls->GetMethodTable()->GetClass() == cls);
            _ASSERTE(cls->IsValueClass());
            _ASSERTE(sizeof(BYTE) == 1);
            
            // assume no GC pointers at first
            ret = 0;
            memset(gcPtrs, TYPE_GC_NONE, 
                   (VMClsHnd.GetSize() + sizeof(void*) -1)/ sizeof(void*));

            // walk the GC descriptors, turning on the correct bits
            MethodTable *   pByValueMT = cls->GetMethodTable();
            if (cls->GetNumGCPointerSeries() > 0)
            {
                CGCDescSeries * pByValueSeries = ((CGCDesc*) pByValueMT)->GetLowestSeries();
                for (int i = 0; i < cls->GetNumGCPointerSeries(); i++)
                {
                    // Get offset into the value class of the first pointer field (includes a +Object)
                    size_t cbSeriesSize = pByValueSeries->GetSeriesSize() + pByValueMT->GetBaseSize();
                    size_t cbOffset = pByValueSeries->GetSeriesOffset() - sizeof(Object);

                    _ASSERTE (cbOffset % sizeof(void*) == 0);
                    _ASSERTE (cbSeriesSize % sizeof(void*) == 0);
                    
                    ret += (unsigned) (cbSeriesSize / sizeof(void*));
                    memset(&gcPtrs[cbOffset/sizeof(void*)], TYPE_GC_REF, cbSeriesSize / sizeof(void*));
                    pByValueSeries++;
                }
            }
        }
    }
    STOP_NON_JIT_PERF();
    return(ret);
}
/*********************************************************************/
unsigned __stdcall CEEInfo::getClassNumInstanceFields (CORINFO_CLASS_HANDLE clsHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    TypeHandle VMClsHnd(clsHnd);
    EEClass* cls = VMClsHnd.GetClass();
    _ASSERTE(cls);
    return cls->GetNumInstanceFields();

}

/*********************************************************************/
// Turn the given fieldDsc into a FieldNum
unsigned __stdcall CEEInfo::getFieldNumber(CORINFO_FIELD_HANDLE fldHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    FieldDesc* fldDsc = (FieldDesc*) fldHnd;
    EEClass *   cls = fldDsc->GetEnclosingClass();
    DWORD       fldNum = 0;

    FieldDescIterator fdIterator(cls, FieldDescIterator::ALL_FIELDS);
    FieldDesc* pFD;

    while ((pFD = fdIterator.Next()) != NULL)
    {
        if (pFD == fldDsc)
            return (long) fldNum;

        if (pFD->IsStatic() == FALSE)
            fldNum++;
    }

    return (unsigned) -1;
}

/*********************************************************************/
// return the enclosing class of the field
CORINFO_CLASS_HANDLE __stdcall CEEInfo::getEnclosingClass (
                    CORINFO_FIELD_HANDLE    field )
{
    return ((CORINFO_CLASS_HANDLE) TypeHandle(((FieldDesc*)field)->GetEnclosingClass()->GetMethodTable()).AsPtr());
}

/*********************************************************************/
// Check Visibility rules.
// For Protected (family access) members, type of the instance is also
// considered when checking visibility rules.
BOOL __stdcall CEEInfo::canAccessField(
                    CORINFO_METHOD_HANDLE   context,
                    CORINFO_FIELD_HANDLE    target,
                    CORINFO_CLASS_HANDLE    instance)
{
    FieldDesc *pFD =  (FieldDesc*) target;
    EEClass   *pCls = pFD->GetEnclosingClass();

    return ClassLoader::CanAccess(
            ((MethodDesc*)context)->GetClass(),
            ((MethodDesc*)context)->GetModule()->GetAssembly(),
            pCls,
            pCls->GetAssembly(),
            TypeHandle(instance).GetClass(),
            pFD->GetAttributes());
}

/*********************************************************************/
/* get the class for the single dimentional array of 'clsHnd' elements */

CORINFO_CLASS_HANDLE __stdcall CEEInfo::getSDArrayForClass(CORINFO_CLASS_HANDLE clsHnd)
{
    REQUIRES_12K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    TypeHandle elemType(clsHnd);
    TypeHandle ret = elemType.GetModule()->GetClassLoader()->FindArrayForElem(elemType, ELEMENT_TYPE_SZARRAY);
    return((CORINFO_CLASS_HANDLE) ret.AsPtr());
}

CorInfoType __stdcall CEEInfo::asCorInfoType (CORINFO_CLASS_HANDLE clsHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    TypeHandle VMClsHnd(clsHnd);
    return toJitType(VMClsHnd);
}

//@GENERICS:
// Given a module scope (scopeHnd), a method handle (context) and an annotated metadata token (annotatedMetaTOK),
// attempt to load the handle (type, field or method) associated with the token.
// If this is not possible at compile-time (because the method code is shared and the token contains type parameters)
// then indicate how the handle should be looked up at run-time.
// 
// Type tokens can be combined with CORINFO_ANNOT_MASK flags to obtain array type handles and method entry points
// These are required by newarr and invariant.newarray instructions which take a token for the *element* type of the array
// and by call/ldftn instructions.
//
// See corinfo.h for more details
//
CORINFO_GENERICHANDLE_RESULT __stdcall CEEInfo::embedGenericHandle(
            CORINFO_MODULE_HANDLE  scopeHnd,
            unsigned      annotatedMetaTOK,
            CORINFO_METHOD_HANDLE context,
            void** ppIndirection,
            CORINFO_CLASS_HANDLE& tokenType)
{
    REQUIRES_4K_STACK;
    CORINFO_GENERICHANDLE_RESULT result;
    result.handle = NULL;

    COOPERATIVE_TRANSITION_BEGIN();

    // Class or method type parameters present in token
    VarKind varKind = hasNoVars;

    // Strip off the annotation
    mdToken     metaTOK = annotatedMetaTOK & ~CORINFO_ANNOT_MASK;
    unsigned annotation = annotatedMetaTOK & CORINFO_ANNOT_MASK;
    mdToken     tokType = TypeFromToken(metaTOK);

    // Where does the token live?
    Module* pModule = GetModule(scopeHnd);
    IMDInternalImport *pInternalImport = pModule->GetMDImport();

    // Is the context shared code (wrt either method or class type parameters)?
    MethodDesc *pContextMD = (MethodDesc*)context;
    BOOL contextShared = pContextMD->IsSharedByGenericInstantiations() != 0;

    InitStaticTypeHandles();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    switch (tokType)
    {
    case mdtTypeRef:
    case mdtTypeDef:
    case mdtTypeSpec:
        tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeTypeHandle);
        if (tokType == mdtTypeSpec && contextShared)
        {
            PCCOR_SIGNATURE pSig;
            ULONG cSig;
            pInternalImport->GetTypeSpecFromToken(metaTOK, &pSig, &cSig);
            SigPointer psig(pSig);
            varKind = psig.IsPolyType(pContextMD);
        }

        if (varKind == hasNoVars)
        {
            result.kind = CORINFO_LOOKUP_CONST;
            CORINFO_CLASS_HANDLE clsHnd = findClass(scopeHnd, metaTOK, context);

            switch (annotation)
            {
            case CORINFO_ANNOT_ARRAY :    
                clsHnd = getSDArrayForClass(clsHnd);
                break;
            }

            result.handle = CORINFO_GENERIC_HANDLE(clsHnd);
            goto Finish;
        }       
        break;

    case mdtMemberRef:
    {
        // OK, we have to look at the metadata to see if it's a field or method
        PCCOR_SIGNATURE pSig;
        ULONG cSig;
        pModule->GetMDImport()->GetNameAndSigOfMemberRef(metaTOK, &pSig, &cSig);

        // Fields are always resolved at compile-time (rethink this if we introduce per-instantiation static fields)
        if (isCallConv(MetaSig::GetCallingConventionInfo(pModule, pSig), IMAGE_CEE_CS_CALLCONV_FIELD))
        {
            tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeFieldHandle);
            result.kind = CORINFO_LOOKUP_CONST;
            result.handle = CORINFO_GENERIC_HANDLE(findField(scopeHnd, metaTOK, context));
            goto Finish;
        }
        else
        {
            tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeMethodHandle);

            // If the context is shared then we'd better check for type/method type parameters
            if (contextShared)
            {
                unsigned parentTok = pInternalImport->GetParentOfMemberRef(metaTOK);
                if (TypeFromToken(parentTok) == mdtTypeSpec)
                {
                    pInternalImport->GetTypeSpecFromToken(parentTok, &pSig, &cSig);
                    SigPointer psig(pSig);
                    varKind = psig.IsPolyType(pContextMD); 
                }
            }

            MethodDesc *resultMD = (MethodDesc*)(findMethod(scopeHnd,
                                                            annotatedMetaTOK & ~CORINFO_ANNOT_ENTRYPOINT,
                                                            context));

            // If both context and target are shared then we can get away with a static look up
            if (varKind == hasNoVars)
            {
                result.kind = CORINFO_LOOKUP_CONST;
                if (annotation & CORINFO_ANNOT_ENTRYPOINT)
                {
                    void* addr = (void*) (resultMD->GetAddrOfCodeForLdFtn());
                    result.handle = CORINFO_GENERIC_HANDLE(addr);
                }
                else 
                    result.handle = CORINFO_GENERIC_HANDLE(resultMD);
                goto Finish;
            }
        }
        break;
    }

    case mdtMethodDef:
    {
        tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeMethodHandle);
        result.kind = CORINFO_LOOKUP_CONST;
        MethodDesc *resultMD = (MethodDesc*)((findMethod(scopeHnd, annotatedMetaTOK & ~CORINFO_ANNOT_ENTRYPOINT, context)));

        if (annotation & CORINFO_ANNOT_ENTRYPOINT)
        {
            void* addr = (void*) (resultMD->GetAddrOfCodeForLdFtn());
            result.handle = CORINFO_GENERIC_HANDLE(addr);
        }
        else 
            result.handle = CORINFO_GENERIC_HANDLE(resultMD);
        goto Finish;
    }

    case mdtMethodSpec:
      {
        if (!g_pConfig->IsGenericsEnabled())
          COMPlusThrowHR(COR_E_BADIMAGEFORMAT);

        tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeMethodHandle);
        
        // If any of the context is shared then we'd better check for type/method type parameters
        if (contextShared)
        {
          PCCOR_SIGNATURE pSig;
          ULONG cSig;
          mdMemberRef MemberRef;

          // Get the member def/ref and instantiation signature
          pInternalImport->GetMethodSpecProps(metaTOK, &MemberRef, &pSig, &cSig);

          if (TypeFromToken(MemberRef) == mdtMemberRef)
          {
            PCCOR_SIGNATURE pParentSig;
            ULONG cParentSig;
            unsigned parentTok = pInternalImport->GetParentOfMemberRef(MemberRef);
            if (TypeFromToken(parentTok) == mdtTypeSpec)
            {
              pInternalImport->GetTypeSpecFromToken(parentTok, &pParentSig, &cParentSig);
              SigPointer psig(pParentSig);
              varKind = psig.IsPolyType(pContextMD); 
            }
          }

          _ASSERT(*pSig == IMAGE_CEE_CS_CALLCONV_INSTANTIATION);
          pSig++;

          SigPointer sp(pSig);
          DWORD ntypars = sp.GetData();

          for (DWORD i = 0; i < ntypars; i++)
          {
            varKind = (VarKind) (sp.IsPolyType(pContextMD) | varKind);
            sp.SkipExactlyOne();
          }
        }

        MethodDesc *resultMD = (MethodDesc*)((findMethod(scopeHnd, annotatedMetaTOK & ~CORINFO_ANNOT_ENTRYPOINT, context)));

        if (varKind == hasNoVars)
        {
          result.kind = CORINFO_LOOKUP_CONST;
          if (annotation & CORINFO_ANNOT_ENTRYPOINT)
          {
            void* addr = (void*) (resultMD->GetAddrOfCodeForLdFtn());
            result.handle = CORINFO_GENERIC_HANDLE(addr);
          }
          else 
            result.handle = CORINFO_GENERIC_HANDLE(resultMD);
          goto Finish;
        }
      }
      break;

    case mdtFieldDef:
        tokenType = CORINFO_CLASS_HANDLE(s_pRuntimeFieldHandle);
        result.kind = CORINFO_LOOKUP_CONST;
        result.handle = CORINFO_GENERIC_HANDLE(findField(scopeHnd, metaTOK, context));
        goto Finish;

    default:
        COMPlusThrowHR(COR_E_BADIMAGEFORMAT);
        _ASSERTE(!"Error bad token type");
    }


    // If we've got this far we need to do the lookup at run-time
    _ASSERTE(varKind != hasNoVars);
    _ASSERTE(contextShared);

    // If we've got a free method type parameter then we must look in the method desc arg
    if ((varKind & hasMethodVar) != 0)
        result.kind = CORINFO_LOOKUP_METHODPARAM;

    // Otherwise we must just have class type variables
    else {
      _ASSERTE(varKind == hasClassVar);

      // If we've got a vtable extra argument, go through that
      if (pContextMD->GetMethodTable()->IsValueClass())
        result.kind = CORINFO_LOOKUP_CLASSPARAM;
      
      // If we've got an object, go through its vtable
      else if (!pContextMD->IsStatic())
        result.kind = CORINFO_LOOKUP_THISOBJ;

      // Otherwise go through the method-desc argument
      else {
        result.kind = CORINFO_LOOKUP_METHODPARAM;
      }
    }

    // Unless we decide otherwise, just do the lookup via a helper function
    result.indirections = CORINFO_USEHELPER;

    // It's a method dictionary lookup
    if (result.kind == CORINFO_LOOKUP_METHODPARAM)
    {
        // Special cases: 
        // (1) Naked method type variable: look up directly in instantiation hanging off runtime md
        // (2) Reference to method-spec of current method (e.g. a recursive call) i.e. currentmeth<!0,...,!(n-1)>
        if (tokType == mdtTypeSpec && annotation == 0)
        {
            PCCOR_SIGNATURE pSig;
            ULONG cSig;
            pInternalImport->GetTypeSpecFromToken(metaTOK, &pSig, &cSig);
            SigPointer sigptr = SigPointer(pSig);
            CorElementType type = sigptr.GetElemType();
            if (type == ELEMENT_TYPE_MVAR)
            {
                result.indirections = 2;
                result.testForNull = 0;
                result.offsets[0] = offsetof(InstantiatedMethodDesc, m_pPerInstInfo);
                result.offsets[1] = (WORD)sizeof(TypeHandle) * sigptr.GetData();
            }
        }
        else if (tokType == mdtMethodSpec && annotation == 0)
        {
          PCCOR_SIGNATURE pSig;
          ULONG cSig;
          mdMemberRef MemberRef;

          // Get the member def/ref and instantiation signature
          pInternalImport->GetMethodSpecProps(metaTOK, &MemberRef, &pSig, &cSig);

          // It's the context itself (i.e. a recursive call)
          if (TypeFromToken(MemberRef) == mdtMethodDef && MemberRef == pContextMD->GetMemberDef())
          {
            // Now just check that the instantiation is (!!0, ..., !!(n-1))
            _ASSERT(*pSig == IMAGE_CEE_CS_CALLCONV_INSTANTIATION);
            pSig++;

            SigPointer sigptr(pSig);
            DWORD ntypars = sigptr.GetData();

            for (unsigned i = 0; i < ntypars; i++)
            {
              CorElementType type = sigptr.GetElemType();
              if (type != ELEMENT_TYPE_MVAR)
                goto NoSpecialCase1;
              if (sigptr.GetData() != i)
                goto NoSpecialCase1;
            }

            // Just use the method descriptor that was passed in!
            result.indirections = 0;
            result.testForNull = 0;
          }
        }

        NoSpecialCase1:
        // No special-casing has been applied
        if (result.indirections == CORINFO_USEHELPER)
        {
          _ASSERTE(pContextMD != NULL);
          _ASSERTE(pContextMD->GetMethodInstantiation() != NULL);
          result.indirections = ((InstantiatedMethodDesc*) pContextMD)->FindDictionaryToken(annotatedMetaTOK, result.offsets + 1);
          if (result.indirections != CORINFO_USEHELPER)
          {
            _ASSERTE(result.indirections+1 <= CORINFO_MAXINDIRECTIONS);

            result.testForNull = 1;

            // Indirect through dictionary table pointer in InstantiatedMethodDesc
            result.offsets[0] = offsetof(InstantiatedMethodDesc, m_pPerInstInfo);

            // ...and we've added an indirection
            result.indirections += 1;
          }
        }
    }

    // It's a class dictionary lookup (CORINFO_LOOKUP_CLASSPARAM or CORINFO_LOOKUP_THISOBJ)
    else
    {
        EEClass *pClass = pContextMD->GetClass();
        _ASSERTE(pClass->GetNumGenericArgs() > 0);

        // Special cases: 
        // (1) Naked class type variable: look up directly in instantiation hanging off vtable
        // (2) C<!0,...,!(n-1)> where C is the context's class and C is sealed: just return vtable ptr
        if (tokType == mdtTypeSpec && annotation == 0)
        {
           PCCOR_SIGNATURE pSig;
           ULONG cSig;
           pInternalImport->GetTypeSpecFromToken(metaTOK, &pSig, &cSig);
           SigPointer sigptr = SigPointer(pSig);
           CorElementType type = sigptr.GetElemType();
           if (type == ELEMENT_TYPE_VAR)
           {
             result.indirections = 3;
             result.testForNull = 0;
             result.offsets[0] = offsetof(MethodTable, m_pPerInstInfo);
             result.offsets[1] = (WORD)sizeof(TypeHandle*) * (pClass->GetNumDicts()-1);
             result.offsets[2] = (WORD)sizeof(TypeHandle) * sigptr.GetData();
           }
           else if (type == ELEMENT_TYPE_WITH && IsTdSealed(pClass->GetAttrClass()))
           {
                if (sigptr.GetElemType() == ELEMENT_TYPE_CLASS)
                {
                    mdTypeRef typeref = sigptr.GetToken();          
                    if (TypeFromToken(typeref) == mdtTypeDef && typeref == pClass->GetCl())
                    {
                        unsigned ntypars = sigptr.GetData();
                        for (unsigned i = 0; i < ntypars; i++)
                        {
                            type = sigptr.GetElemType();
                            if (type != ELEMENT_TYPE_VAR)
                                goto NoSpecialCase2;
                            if (sigptr.GetData() != i)
                                goto NoSpecialCase2;
                        }

                        // Just use the vtable pointer itself!
                        result.indirections = 0;
                        result.testForNull = 0;
                    }
                }
           }
        }
        NoSpecialCase2:

        // No special-casing has been applied
        if (result.indirections == CORINFO_USEHELPER)
        {
            result.indirections = Generics::FindClassDictionaryToken(pClass, annotatedMetaTOK, result.offsets + 2);
            if (result.indirections != CORINFO_USEHELPER)
            {
                _ASSERTE(result.indirections+2 <= CORINFO_MAXINDIRECTIONS);

                result.testForNull = 1;

                // Indirect through dictionary table pointer in vtable
                result.offsets[0] = offsetof(MethodTable, m_pPerInstInfo);

                // Next indirect through the dictionary appropriate to this instantiated type
                result.offsets[1] = (WORD)sizeof(TypeHandle*) * (pClass->GetNumDicts()-1);

                // ...and we've added a couple of indirections
                result.indirections += 2;
            }
        }
    }

Finish:

    result.compileTimeHandle = result.handle;

    COOPERATIVE_TRANSITION_END();

    return result;
}

BYTE* __stdcall CEEInfo::unpackMethodInst(CORINFO_MODULE_HANDLE scopeHnd, unsigned metaTOK, unsigned *memberRef)
{
    *memberRef = mdTokenNil;

    Module* pModule = GetModule(scopeHnd);
    IMDInternalImport *pInternalImport = pModule->GetMDImport();

    if (TypeFromToken(metaTOK) == mdtMethodSpec)
    {
      PCCOR_SIGNATURE pSig;
      ULONG cSig;

      // Get the member def/ref and instantiation signature
      pInternalImport->GetMethodSpecProps(metaTOK, memberRef, &pSig, &cSig);

      _ASSERT(*pSig == IMAGE_CEE_CS_CALLCONV_INSTANTIATION);
      pSig++;

      return (BYTE*) pSig;
    }
    else
      return NULL;
}


/*********************************************************************/
const char* __stdcall CEEInfo::getClassName (CORINFO_CLASS_HANDLE clsHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    _ASSERTE(!isValueArray(clsHnd));
    TypeHandle VMClsHnd(clsHnd);
    EEClass* cls = VMClsHnd.GetClass();

        // Sanity test for class
    _ASSERTE(cls);
    _ASSERTE(cls->GetMethodTable()->GetClass() == cls);

#ifdef _DEBUG
    cls->_GetFullyQualifiedNameForClass(clsNameBuff, MAX_CLASSNAME_LENGTH);
    return(clsNameBuff);
#else 
    // since this is for diagnostic purposes only,
    // give up on the namespace, as we don't have a buffer to concat it
    // also note this won't show array class names.
    LPCUTF8 nameSpace;
    return cls-> GetFullyQualifiedNameInfo(&nameSpace);
#endif
}

/*********************************************************************/
CORINFO_MODULE_HANDLE __stdcall CEEInfo::getClassModule(CORINFO_CLASS_HANDLE clsHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    TypeHandle     VMClsHnd(clsHnd);

    return(CORINFO_MODULE_HANDLE(VMClsHnd.GetModule()));
}

/*********************************************************************/
DWORD __stdcall CEEInfo::getClassAttribs (CORINFO_CLASS_HANDLE clsHnd, CORINFO_METHOD_HANDLE context)
{
    REQUIRES_4K_STACK;

    DWORD ret =0;
    BEGINCANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();

    _ASSERTE(clsHnd);
    _ASSERTE(!isValueArray(clsHnd));

    TypeHandle     VMClsHnd(clsHnd);

    // For now, type variables simply report "variable".  
    if (VMClsHnd.IsGenericVariable())
    {
        ret |= CORINFO_FLG_GENERIC_TYPE_VARIABLE;
    }
    else
    {
        MethodTable    *pMT = VMClsHnd.GetMethodTable();
        if (pMT != NULL)
        {
            EEClass        *cls = pMT->GetClass();
            _ASSERTE(cls->GetMethodTable()->GetClass() == cls);
            
            // Get the calling method
            MethodDesc* method = (MethodDesc*) context;
            
            if (pMT->IsArray())
                ret |= CORINFO_FLG_ARRAY;
            
            // Set the INITIALIZED bit if the class is initialized 
            if (pMT->IsClassInited())
                ret |= CORINFO_FLG_INITIALIZED;
            else if (pMT->IsShared() && !method->GetMethodTable()->IsShared())
            {
                // If we are accessing from unshared to shared code, we can check the
                // state of the current domain as well.
                
                COMPLUS_TRY 
                    {
                        DomainLocalBlock *pBlock = GetAppDomain()->GetDomainLocalBlock();
                        if (pBlock->IsClassInitialized(pMT->GetSharedClassIndex()))
                            ret |= CORINFO_FLG_INITIALIZED;
                    }
                COMPLUS_CATCH
                    {
                        // just eat the exception and assume class not inited
                    }
                COMPLUS_END_CATCH;
            }
            
            // Set the NEEDS_INIT bit if we require the JIT to insert cctor logic before
            // access.  This is currently used only for static field accesses and method inlining.
            // Note that this is set independently of the INITIALIZED bit above.
            
            if (pMT != method->GetMethodTable())
            {
                if (pMT->IsShared())
                {
                    // For shared classes, the inited bit is only set when we never need
                    // class initialzation (no cctor or static handles)
                    
                    if (!pMT->IsClassInited())
                    {
                        // 
                        // For the shared->shared access, it turns out we never have to do any initialization
                        // in this case.  There are 2 cases:
                        // 
                        // BeforeFieldInit: in this case, no init is required on method calls, and 
                        //  the shared helper will perform the required init on static field accesses.
                        // Exact:  in this case we do need an init on method calls, but this will be
                        //  provided by a prolog of the method itself.
                        
                        if (!method->GetMethodTable()->IsShared())
                        {
                            // Unshared->shared access.  The difference in this case (from above) is 
                            // that we don't use the shared helper.  Thus we need the JIT to provide
                            // a class construction call.  
                            
                            ret |= CORINFO_FLG_NEEDS_INIT;
                        }
                    }
                }
                else
                {
                    // For accesses to unshared classes (which are by necessity from other unshared classes),
                    // we need initialization iff we have a class constructor.
                    
                    if (pMT->HasClassConstructor())
                        ret |= CORINFO_FLG_NEEDS_INIT;
                }
            }
            
            if (cls->IsInterface())
                ret |= CORINFO_FLG_INTERFACE;
            
            if (cls->HasVarSizedInstances())
                ret |= CORINFO_FLG_VAROBJSIZE;
            
            if (cls->IsValueClass()) 
            {
                ret |= CORINFO_FLG_VALUECLASS;
                
                if (cls->ContainsStackPtr())
                    ret |= CORINFO_FLG_CONTAINS_STACK_PTR;
            }
            
            if (cls->IsContextful())
                ret |= CORINFO_FLG_CONTEXTFUL;
            
            if (cls->IsMarshaledByRef())
                ret |= CORINFO_FLG_MARSHAL_BYREF;
            
            if (cls == g_pObjectClass->GetClass())
                ret |= CORINFO_FLG_OBJECT;
            
            if (pMT->ContainsPointers())
                ret |= CORINFO_FLG_CONTAINS_GC_PTR;
            
            if (cls->IsAnyDelegateClass())
                ret |= CORINFO_FLG_DELEGATE;
        }
    }

    STOP_NON_JIT_PERF();

    ENDCANNOTTHROWCOMPLUSEXCEPTION();

    return(ret);
}

/*********************************************************************/
BOOL __stdcall CEEInfo::initClass (CORINFO_CLASS_HANDLE clsHnd,
                                   CORINFO_METHOD_HANDLE context,
                                   BOOL speculative)
{
    REQUIRES_4K_STACK;


    MethodTable    *pMT = TypeHandle(clsHnd).GetMethodTable();
    MethodDesc *method = (MethodDesc*) context;

    BOOL result = FALSE;

    // Eagerly run the .cctor for the classes that are not value classes and
    // that have the BeforeFieldInit bit set.  (But don't run it if the 
    // caller is shared as it cannot help in the general case.)
    // 
    //
    //
    // In the future, this routine should decide situationally as well as 
    // whether it's legal to run the constructor.  (But to do that, we need
    // to know what field or method we are accessing, not just the class.)

    if (!method->GetClass()->IsShared() && 
        (IsTdBeforeFieldInit(pMT->GetClass()->GetAttrClass())
         || pMT->GetModule()->GetAssembly() == SystemDomain::SystemAssembly()))
    {
        COOPERATIVE_TRANSITION_BEGIN();
        THROWSCOMPLUSEXCEPTION();

        result = TRUE;

        if (!speculative)
        {
            //
            // Run the .cctor
            //

            OBJECTREF throwable = NULL;
            GCPROTECT_BEGIN(throwable);
            if (!pMT->CheckRunClassInit(&throwable))
            {
                // Return FALSE, so the JIT will embed the check and get the
                // exception at execution time.
                result = FALSE;
            }
            GCPROTECT_END();

            // 
            // If we encountered a deadlock while running the class init, our method
            // may have already been jitted by now. If this is true, abort the jit
            // by throwing an exception. (Note that we are relying on the JIT catching
            // and handling this exception, so it doesn't really need a message.)
            //

            if (method->IsJitted())
                COMPlusThrow(kSynchronizationLockException);
        }
            
        COOPERATIVE_TRANSITION_END();
    }

    return result;
}

BOOL __stdcall CEEInfo::loadClass (CORINFO_CLASS_HANDLE clsHnd,
                                   CORINFO_METHOD_HANDLE context,
                                   BOOL speculative)
{
    REQUIRES_4K_STACK;
    
    TypeHandle th = TypeHandle(clsHnd);
    
    //@GENERICSVER: (generic verification time) type variables do not need to be loaded 
    if (th.IsGenericVariable()) 
       return TRUE;

    MethodTable    *pMT = th.GetMethodTable();

    //
    // It is always safe to restore a class at jit time, as it happens 
    // once per process and has no interesting side effects.
    //

    if (!speculative)
        pMT->CheckRestore();

    return TRUE;
}

/*********************************************************************/
CORINFO_CLASS_HANDLE __stdcall CEEInfo::getBuiltinClass(CorInfoClassId classId)
{
    CORINFO_CLASS_HANDLE result = (CORINFO_CLASS_HANDLE) 0;

    COOPERATIVE_TRANSITION_BEGIN();


    switch (classId)
    {
    case CLASSID_SYSTEM_OBJECT:
        result = CORINFO_CLASS_HANDLE(g_pObjectClass);
        break;
    case CLASSID_TYPED_BYREF:
        InitStaticTypeHandles();
        result = CORINFO_CLASS_HANDLE(s_pTypedReference);
        break;
    case CLASSID_TYPE_HANDLE:
        InitStaticTypeHandles();
        result = CORINFO_CLASS_HANDLE(s_pRuntimeTypeHandle);
        break;
    case CLASSID_FIELD_HANDLE:
        InitStaticTypeHandles();
        result = CORINFO_CLASS_HANDLE(s_pRuntimeFieldHandle);
        break;
    case CLASSID_METHOD_HANDLE:
        InitStaticTypeHandles();
        result = CORINFO_CLASS_HANDLE(s_pRuntimeMethodHandle);
        break;
    case CLASSID_ARGUMENT_HANDLE:
        InitStaticTypeHandles();
        result = CORINFO_CLASS_HANDLE(s_pRuntimeArgumentHandle);
        break;
    case CLASSID_STRING:
        result = CORINFO_CLASS_HANDLE(g_pStringClass);
        break;
    default:
        _ASSERTE(!"NYI: unknown classId");
        break;
    }

    COOPERATIVE_TRANSITION_END();

    return result;
}



/*********************************************************************/
CorInfoType __stdcall CEEInfo::getTypeForPrimitiveValueClass(
        CORINFO_CLASS_HANDLE cls)
{
    TypeHandle th(cls);

    CorInfoType result = CORINFO_TYPE_UNDEF;

    COOPERATIVE_TRANSITION_BEGIN();

    switch (th.GetNormCorElementType())
    {
    case ELEMENT_TYPE_I1:
    case ELEMENT_TYPE_U1:
    case ELEMENT_TYPE_BOOLEAN:
        result = toJitType(ELEMENT_TYPE_I1);
        break;

    case ELEMENT_TYPE_I2:
    case ELEMENT_TYPE_U2:
    case ELEMENT_TYPE_CHAR:
        result = toJitType(ELEMENT_TYPE_I2);
        break;

    case ELEMENT_TYPE_I4:
    case ELEMENT_TYPE_U4:
        result = toJitType(ELEMENT_TYPE_I4);
        break;

    case ELEMENT_TYPE_I8:
    case ELEMENT_TYPE_U8:
        result = toJitType(ELEMENT_TYPE_I8);
        break;

    case ELEMENT_TYPE_I:

        // RuntimeTypeHandle, RuntimeMethodHandle, RuntimeArgHandle etc
        // Are disguised as ELEMENT_TYPE_I. Catch it here.
        InitStaticTypeHandles();

        if ((th == TypeHandle(s_pRuntimeTypeHandle)) ||
            (th == TypeHandle(s_pRuntimeFieldHandle)) ||
            (th == TypeHandle(s_pRuntimeMethodHandle)) ||
            (th == TypeHandle(s_pRuntimeArgumentHandle)))
        {
            result = CORINFO_TYPE_UNDEF;
            break;
        }

        // FALL THROUGH

    case ELEMENT_TYPE_U:
        result = toJitType(ELEMENT_TYPE_I);
        break;

    case ELEMENT_TYPE_R4:
        result = toJitType(ELEMENT_TYPE_R4);
        break;

    case ELEMENT_TYPE_R:
    case ELEMENT_TYPE_R8:
        result = toJitType(ELEMENT_TYPE_R8);
        break;

    case ELEMENT_TYPE_VOID:
        result = toJitType(ELEMENT_TYPE_VOID);
        break;

    default:
        break;
    }

    COOPERATIVE_TRANSITION_END();

    return result;
}

/*********************************************************************/
// TRUE if child is a subtype of parent
// if parent is an interface, then does child implement / extend parent
BOOL __stdcall CEEInfo::canCast(
        CORINFO_CLASS_HANDLE        child, 
        CORINFO_CLASS_HANDLE        parent)
{
    return ((TypeHandle)child).CanCastTo((TypeHandle)parent);
}

/*********************************************************************/
// returns is the intersection of cls1 and cls2.
CORINFO_CLASS_HANDLE __stdcall CEEInfo::mergeClasses(
        CORINFO_CLASS_HANDLE        cls1, 
        CORINFO_CLASS_HANDLE        cls2)
{
    return CORINFO_CLASS_HANDLE(TypeHandle::MergeTypeHandlesToCommonParent(
            TypeHandle(cls1), TypeHandle(cls2)).AsPtr());
}

/*********************************************************************/
// Given a class handle, returns the Parent type.
// For COMObjectType, it returns Class Handle of System.Object.
// Returns 0 if System.Object is passed in.
CORINFO_CLASS_HANDLE __stdcall CEEInfo::getParentType(
            CORINFO_CLASS_HANDLE    cls)
{
    _ASSERTE(!isValueArray(cls));

    TypeHandle th(cls);

    _ASSERTE(!th.IsNull());


    return CORINFO_CLASS_HANDLE(th.GetParent().AsPtr());
}


/*********************************************************************/
// Returns the CorInfoType of the "child type". If the child type is
// not a primitive type, *clsRet will be set.
// Given an Array of Type Foo, returns Foo.
// Given BYREF Foo, returns Foo
CorInfoType __stdcall CEEInfo::getChildType (
        CORINFO_CLASS_HANDLE       clsHnd,
        CORINFO_CLASS_HANDLE       *clsRet
        )
{
    REQUIRES_16K_STACK;

    CorInfoType ret = CORINFO_TYPE_UNDEF;
    *clsRet = 0;
    TypeHandle  retType = TypeHandle();

    COOPERATIVE_TRANSITION_BEGIN();
    THROWSCOMPLUSEXCEPTION();

    OBJECTREF throwable = NULL;
    GCPROTECT_BEGIN(throwable);

    BOOL fErr = FALSE;

    if (isValueArray(clsHnd)) {

        ValueArrayDescr* valArr = asValueArray(clsHnd);

        CorElementType type;
        SigPointer ptr(valArr->sig);    // make a copy

        for(;;) {
            type = ptr.PeekElemType();
            if (type != ELEMENT_TYPE_PINNED)
                break;
            ptr.GetElemType();
        }
    
        if (type == ELEMENT_TYPE_VALUEARRAY) {

            ValueArrayDescr* elem = allocDescr(ptr, valArr->module);
            // set low order bits to mark as a value class array descriptor
            *clsRet = markAsValueArray(elem);
            ret = toJitType(ELEMENT_TYPE_VALUEARRAY);

        } else {

            retType = ptr.GetTypeHandle(valArr->module, &throwable);

            if (retType.IsNull())
                fErr = TRUE;
        }

    } else  {

        TypeHandle th(clsHnd);

        _ASSERTE(!th.IsNull());

        // BYREF, ARRAY types 
        if (th.IsTypeDesc())
            retType = th.AsTypeDesc()->GetTypeParam();
        else {
            EEClass* eeClass = th.GetClass();
            if (eeClass->IsArrayClass())
                retType = ((ArrayClass*) eeClass)->GetElementTypeHandle();
        }

    }

    if (fErr) {
        // If don't have a throwable, find out who didn't create one,
        // and fix it.
        _ASSERTE(throwable!=NULL);
        COMPlusThrow(throwable);
    }
    GCPROTECT_END();
    COOPERATIVE_TRANSITION_END();

    if (!retType.IsNull()) {
        CorElementType type = retType.GetNormCorElementType();
        ret = toJitType(type,retType, clsRet);

    }

    return ret;
}

/*********************************************************************/
// Check Visibility rules.
BOOL __stdcall CEEInfo::canAccessType(
        CORINFO_METHOD_HANDLE       context,
        CORINFO_CLASS_HANDLE        target) 
{
    _ASSERTE(!isValueArray(target));

    return ClassLoader::CanAccessClass(
            ((MethodDesc*)context)->GetClass(),
            ((MethodDesc*)context)->GetModule()->GetAssembly(),
            TypeHandle(target).GetClass(),
            TypeHandle(target).GetClass()->GetAssembly());
}

/*********************************************************************/
// Check Visibility rules.
BOOL __stdcall CEEInfo::isSDArray(CORINFO_CLASS_HANDLE  cls)
{
    TypeHandle th(cls);

    _ASSERTE(!th.IsNull());

    if (!th.IsArray())
        return FALSE;

    if (th == TypeHandle(g_pArrayClass))
        return FALSE;

    return (th.AsArray()->GetRank() == 1);
}


/*********************************************************************/
// Static helpers
/*********************************************************************/
MethodTable * CEEInfo::s_pRuntimeTypeHandle;
MethodTable * CEEInfo::s_pRuntimeMethodHandle;
MethodTable * CEEInfo::s_pRuntimeFieldHandle;
MethodTable * CEEInfo::s_pRuntimeArgumentHandle;
MethodTable * CEEInfo::s_pTypedReference;

/* static */
void CEEInfo::InitStaticTypeHandles()
{
    static bool fInited = false;

    if (fInited)
        return;

    if (s_pRuntimeTypeHandle == NULL)
        s_pRuntimeTypeHandle = g_Mscorlib.FetchClass(CLASS__TYPE_HANDLE);

    if (s_pRuntimeFieldHandle == NULL)
        s_pRuntimeFieldHandle = g_Mscorlib.FetchClass(CLASS__FIELD_HANDLE);

    if (s_pRuntimeMethodHandle == NULL)
        s_pRuntimeMethodHandle = g_Mscorlib.FetchClass(CLASS__METHOD_HANDLE);

    if (s_pRuntimeArgumentHandle == NULL)
        s_pRuntimeArgumentHandle = g_Mscorlib.FetchClass(CLASS__ARGUMENT_HANDLE);

    if (s_pTypedReference == NULL)
        s_pTypedReference = g_Mscorlib.FetchClass(CLASS__TYPED_REFERENCE);

    fInited = true;
}

/*********************************************************************
 * This method returns TRUE only for primitive types like I1, I2.. R8 etc
 * Returns FALSE if the types are OBJREF, VALUECLASS etc.
 */
/* static */ 
BOOL CEEInfo::CanCast(CorElementType el1, CorElementType el2)
{
    if (el1 == el2)
        return CorIsPrimitiveType(el1);

    switch (el1)
    {
    case ELEMENT_TYPE_I1:
    case ELEMENT_TYPE_U1:
    case ELEMENT_TYPE_BOOLEAN:
        return (el2 == ELEMENT_TYPE_I1 || 
                el2 == ELEMENT_TYPE_U1 || 
                el2 == ELEMENT_TYPE_BOOLEAN);

    case ELEMENT_TYPE_I2:
    case ELEMENT_TYPE_U2:
    case ELEMENT_TYPE_CHAR:
        return (el2 == ELEMENT_TYPE_I2 || 
                el2 == ELEMENT_TYPE_U2 || 
                el2 == ELEMENT_TYPE_CHAR);

    case ELEMENT_TYPE_I4:
    case ELEMENT_TYPE_U4:
        return (el2 == ELEMENT_TYPE_I4 ||
                el2 == ELEMENT_TYPE_U4);

    case ELEMENT_TYPE_I8:
    case ELEMENT_TYPE_U8:
        return (el2 == ELEMENT_TYPE_I8 ||
                el2 == ELEMENT_TYPE_U8);

    case ELEMENT_TYPE_I:
    case ELEMENT_TYPE_U:
        return  (el2 == ELEMENT_TYPE_I || 
                 el2 == ELEMENT_TYPE_U);

    case ELEMENT_TYPE_R4:
        return (el2 == ELEMENT_TYPE_R4);

    case ELEMENT_TYPE_R8:
    case ELEMENT_TYPE_R:
        return (el2 == ELEMENT_TYPE_R8 ||
                el2 == ELEMENT_TYPE_R);

    default:
        break;
    }

    return FALSE;
}

/*********************************************************************/
/* static */
BOOL CEEInfo::CompatibleMethodSig(MethodDesc *pMethDescA, MethodDesc *pMethDescB)
{
    _ASSERTE(pMethDescA != NULL && pMethDescB != NULL);

    if (pMethDescA == pMethDescB)
        return TRUE;

    // Disallow virtual methods
    if (pMethDescA->IsVirtual() || pMethDescB->IsVirtual())
        return FALSE;

    Module *pModA, *pModB; 
    PCCOR_SIGNATURE pSigA, pSigB; 
    DWORD dwSigA, dwSigB;

    pMethDescA->GetSig(&pSigA, &dwSigA);
    pMethDescB->GetSig(&pSigB, &dwSigB);

    pModA = pMethDescA->GetModule();
    pModB = pMethDescB->GetModule();

    MetaSig SigA(pMethDescA);
    MetaSig SigB(pMethDescB);

    // check everyting CompareMethodSigs() does not check
    if (SigA.GetCallingConventionInfo() != SigB.GetCallingConventionInfo())
        return FALSE;

    if (SigA.NumFixedArgs() != SigB.NumFixedArgs())
        return FALSE;

    MethodTable *pMTA, *pMTB;

    pMTA = pMethDescA->GetMethodTable();
    pMTB = pMethDescB->GetMethodTable();

    if (pMTA != pMTB)
    {
        return FALSE;
    }

    return MetaSig::CompareMethodSigs(pSigA, dwSigA, pModA, NULL, pSigB, dwSigB, pModB, NULL);
}

/*********************************************************************/
void* __stdcall CEEInfo::getInterfaceID (CORINFO_CLASS_HANDLE  clsHnd,
                                         void **ppIndirection)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    _ASSERTE(!isValueArray(clsHnd));
    TypeHandle  VMClsHnd(clsHnd);

        // Sanity test for class
    _ASSERTE(VMClsHnd.AsClass()->GetMethodTable()->GetClass() == VMClsHnd.AsClass());

        // The Interface ID is defined to be the pointer to the MethodTable
        // for the interface to be looked up
    return VMClsHnd.AsMethodTable();
}


/***********************************************************************/
unsigned __stdcall CEEInfo::getInterfaceTableOffset (CORINFO_CLASS_HANDLE       clsHnd,
                                                     void **ppIndirection)
{
    REQUIRES_4K_STACK;

    THROWSCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    _ASSERTE(!isValueArray(clsHnd));
    TypeHandle  VMClsHnd(clsHnd);

        // Sanity test for class
    _ASSERTE(VMClsHnd.AsClass()->GetMethodTable()->GetClass() == VMClsHnd.AsClass());

     // Make sure we are restored so our interface ID is valid
     _ASSERTE(VMClsHnd.AsClass()->IsRestored());

    _ASSERTE(VMClsHnd.AsClass()->IsInterface());

    // Make sure we are restored so our interface ID is valid
    _ASSERTE(VMClsHnd.AsClass()->IsRestored());

    unsigned result = VMClsHnd.AsClass()->GetInterfaceId(); // Can throw.

    //
    // There is a case where we load a system domain com interface from one
    // app domain, then use a zap file in another app domain which tries to call on
    // that interface on a com wrapper.  I believe this is the only
    // place where we can hook in to map the interface from the system
    // domain into the current domain.
    //

    if (VMClsHnd.AsMethodTable()->GetModule()->GetDomain() == SystemDomain::System())
    {
        EEClass::MapInterfaceFromSystem(SystemDomain::GetCurrentDomain(),
                                        VMClsHnd.AsMethodTable());
    }

    return result;
}

/***********************************************************************/
unsigned __stdcall CEEInfo::getClassDomainID (CORINFO_CLASS_HANDLE clsHnd,
                                              void **ppIndirection)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    _ASSERTE(!isValueArray(clsHnd));
    TypeHandle  VMClsHnd(clsHnd);

        // Sanity test for class
    _ASSERTE(VMClsHnd.AsClass()->GetMethodTable()->GetClass() == VMClsHnd.AsClass());

    unsigned result = (unsigned)VMClsHnd.AsMethodTable()->GetSharedClassIndex();

    return result;
}

/***********************************************************************/
CorInfoHelpFunc CEEInfo::getNewHelper (CORINFO_CLASS_HANDLE newClsHnd, CORINFO_METHOD_HANDLE context)
{
    REQUIRES_4K_STACK;

    MethodDesc * method;

    _ASSERTE(!isValueArray(newClsHnd));
    TypeHandle  VMClsHnd(newClsHnd);

	COOPERATIVE_TRANSITION_BEGIN();
    THROWSCOMPLUSEXCEPTION();     
        // Sanity test for class
    _ASSERTE(VMClsHnd.AsClass()->GetMethodTable()->GetClass() == VMClsHnd.AsClass());
	if(VMClsHnd.AsClass()->IsAbstract())
	{
		// Disabling this assert to allow automated tests for illegal il
        //_ASSERTE(!"Instantiation of an abstract class");
		COMPlusThrow(kNullReferenceException,L"InvalidOperation_BadTypeAttributesNotAbstract");
	}
	COOPERATIVE_TRANSITION_END();

        method = (MethodDesc*) context;

    MethodTable * pMT    = VMClsHnd.AsMethodTable();


    if(CRemotingServices::IsRemoteActivationRequired(VMClsHnd))
    {
        return CORINFO_HELP_NEW_CROSSCONTEXT;
    }

    // We shouldn't get here with a COM object (they're all potentially
    // remotable, so they're covered by the case above).
    _ASSERTE(!pMT->IsComObjectType());

    if (GCHeap::IsLargeObject(pMT) ||
        pMT->HasFinalizer())
    {
#ifdef _DEBUG
        //printf("NEWFAST:   %s\n", pMT->GetClass()->m_szDebugClassName);
#endif // _DEBUG
        return CORINFO_HELP_NEWFAST;
    }

#ifdef _DEBUG
        //printf("NEWSFAST:  %s\n", pMT->GetClass()->m_szDebugClassName);
#endif // _DEBUG

        // don't call the super-optimized one since that does not check
        // for GCStress
#ifdef STRESS_HEAP
    if (g_pConfig->GetGCStressLevel() & EEConfig::GCSTRESS_ALLOC)
        return CORINFO_HELP_NEWFAST;
#endif // STRESS_HEAP

#ifdef _LOGALLOC
#ifdef LOGGING
    // Super fast version doesn't do logging
    if (LoggingOn(LF_GCALLOC, LL_INFO10))
        return CORINFO_HELP_NEWFAST;
#endif
#endif

#ifdef PROFILING_SUPPORTED
    // Don't use the SFAST allocator when tracking object allocations,
    // so we don't have to instrument it.
    if (CORProfilerTrackAllocationsEnabled())
        return CORINFO_HELP_NEWFAST;
#endif // PROFILING_SUPPORTED

    if (VMClsHnd.AsClass()->IsAlign8Candidate())
    {
        return CORINFO_HELP_NEWSFAST_ALIGN8;
    }

    return CORINFO_HELP_NEWSFAST;

}

/***********************************************************************/
//<REVIEW>GENERICS: this only works for shared generic code because all the
// helpers are actually the same. If they were different then things might
// break because the same helper would end up getting used for different but
// representation-compatible arrays (e.g. one with a default constructor
// and one without)</REVIEW>
CorInfoHelpFunc CEEInfo::getNewArrHelper (CORINFO_CLASS_HANDLE arrayClsHnd,
                                          CORINFO_METHOD_HANDLE context)
{
    REQUIRES_4K_STACK;

    TypeHandle arrayType(arrayClsHnd);
    ArrayTypeDesc* arrayTypeDesc = arrayType.AsArray();
    _ASSERTE(arrayTypeDesc->GetNormCorElementType() == ELEMENT_TYPE_SZARRAY);

    // This is if we're asked for newarr !0 when verifying generic code
    // Of course ideally you wouldn't even be generating code when
    // simply doing verification (we run the JIT importer in import-only 
    // mode), but importing does more than one would like so we try to be
    // tolerant when asked for non-sensical helpers.  
    if(arrayTypeDesc->GetTypeParam().IsGenericVariable())
        return CORINFO_HELP_NEWARR_1_OBJ;

    // This is the normal case
    if(arrayTypeDesc->GetMethodTable()->HasSharedMethodTable()) {
                // It is an array of object refs
        _ASSERTE(CorTypeInfo::IsObjRef(arrayTypeDesc->GetTypeParam().GetNormCorElementType()));
        return CORINFO_HELP_NEWARR_1_OBJ;
    }
    else {
        _ASSERTE(!CorTypeInfo::IsObjRef(arrayTypeDesc->GetTypeParam().GetNormCorElementType()));

        // Make certain we called the class constructor
        if (!m_pOverride->initClass(CORINFO_CLASS_HANDLE(arrayTypeDesc->GetTypeParam().AsPtr()), 
                                    context, FALSE))
            return CORINFO_HELP_NEWARR_1_DIRECT;

        // If the elemen type has a default constructor, do it the slow way
        if (arrayTypeDesc->GetElementCtor())
            return CORINFO_HELP_NEWARR_1_DIRECT;

        if (arrayTypeDesc->GetTypeParam().GetSize() > LARGE_ELEMENT_SIZE)
            return CORINFO_HELP_NEWARR_1_DIRECT;

        // Fast version doesn't do logging
        if (LoggingOn(LF_GCALLOC, LL_INFO10))
            return (CORINFO_HELP_NEWARR_1_DIRECT);

#ifdef PROFILING_SUPPORTED
        // If we're profiling object allocations then we should always use the slow path
        if (CORProfilerTrackAllocationsEnabled())
            return (CORINFO_HELP_NEWARR_1_DIRECT);
#endif // PROFILING_SUPPORTED

        // See if we want to double align the object
        if (arrayTypeDesc->GetTypeParam().GetNormCorElementType() == ELEMENT_TYPE_R8) {
            return CORINFO_HELP_NEWARR_1_ALIGN8;
        }

        // Yea, we can do it the fast way!
        return CORINFO_HELP_NEWARR_1_VC;
    }
}

/***********************************************************************/
CorInfoHelpFunc CEEInfo::getIsInstanceOfHelper (CORINFO_CLASS_HANDLE IsInstClsHnd)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();
    _ASSERTE(!isValueArray(IsInstClsHnd));
    TypeHandle  clsHnd(IsInstClsHnd);

    // Sanity test for class
    _ASSERTE(!clsHnd.IsUnsharedMT() || clsHnd.HasInstantiation() || 
            clsHnd.AsClass()->GetMethodTable()->GetClass() == clsHnd.AsClass());

    // If it is a class, but not a interface, use an optimized handler.
    if (clsHnd.IsUnsharedMT() && !clsHnd.AsMethodTable()->GetClass()->IsInterface())
        return CORINFO_HELP_ISINSTANCEOFCLASS;

    // Array or Interface.
    return CORINFO_HELP_ISINSTANCEOF;
}

/***********************************************************************/
CorInfoHelpFunc CEEInfo::getChkCastHelper (CORINFO_CLASS_HANDLE IsInstClsHnd)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    _ASSERTE(!isValueArray(IsInstClsHnd));
    TypeHandle  clsHnd(IsInstClsHnd);

    // Sanity test for class
    _ASSERTE(!clsHnd.IsUnsharedMT() || clsHnd.HasInstantiation() || 
            clsHnd.AsClass()->GetMethodTable()->GetClass() == clsHnd.AsClass());

    // If it is a class, but not a interface, use an optimized handler.
    if (clsHnd.IsUnsharedMT() && !clsHnd.AsMethodTable()->GetClass()->IsInterface())
        return CORINFO_HELP_CHKCASTCLASS;

    // Array or Interface.
    return CORINFO_HELP_CHKCAST;
}
/***********************************************************************/
// This returns the typedesc from a method token. 
CORINFO_CLASS_HANDLE __stdcall CEEInfo::findMethodClass(CORINFO_MODULE_HANDLE scopeHnd, 
                                                        mdToken methodTOK,
                                                        CORINFO_METHOD_HANDLE context)
{
    REQUIRES_8K_STACK;
    TypeHandle type;
    COOPERATIVE_TRANSITION_BEGIN();

    Module* module = GetModule(scopeHnd);
    IMDInternalImport *pInternalImport = module->GetMDImport();
        
    _ASSERTE(TypeFromToken(methodTOK) == mdtMemberRef);

    mdTypeDef typeSpec;
#ifdef _DEBUG
    HRESULT hr =
#endif
    pInternalImport->GetParentToken(methodTOK, &typeSpec);
    _ASSERTE(SUCCEEDED(hr));

    TypeHandle* classInst = NULL;
    TypeHandle* methodInst = NULL;
    if (context)
    {
      MethodDesc* md = (MethodDesc*) context;
      classInst = md->GetClassInstantiation();
      methodInst = md->GetMethodInstantiation();
    }

    NameHandle typeNameHnd(module, typeSpec);
    type = module->GetClassLoader()->LoadTypeHandle(&typeNameHnd, NULL, classInst, methodInst);

    COOPERATIVE_TRANSITION_END();
    return(CORINFO_CLASS_HANDLE(type.AsPtr()));
}


/***********************************************************************/
        // registers a vararg sig & returns a class-specific cookie for it.
CORINFO_VARARGS_HANDLE __stdcall CEEInfo::getVarArgsHandle(CORINFO_SIG_INFO *sig,
                                                           void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    Module* module = GetModule(sig->scope);

    return CORINFO_VARARGS_HANDLE(module->GetVASigCookie((PCCOR_SIGNATURE)sig->sig));
}

/***********************************************************************/
unsigned __stdcall CEEInfo::getMethodHash (CORINFO_METHOD_HANDLE ftnHnd)
{
    MethodDesc* ftn = (MethodDesc*) ftnHnd;
        // Form a hash for the method.  We choose the following

        // start with something that represents a DLL uniquely (we
        // use imageBase for this.  Since this is always on a 64K 
        // boundry, get rid of the lower bits.  
    size_t hash = size_t(ftn->GetModule()->GetILBase()) >> 16;

        // Make the module fit in roughly 4 bits and make it so that
        // mscorlib.dll has hash = 0;
    hash = (hash + 1) % 17;     

        // 4K of method tokens per module
    size_t methodId = RidFromToken(ftn->GetMemberDef());
    hash =  methodId + (hash << 12);

        // Currently the hash should not be greater than a bit over 64K
        // you can start there to mean 'everything'.  
    return (unsigned)hash;
}

/***********************************************************************/
const char* __stdcall CEEInfo::getMethodName (CORINFO_METHOD_HANDLE ftnHnd, const char** scopeName)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    MethodDesc* ftn = (MethodDesc*) ftnHnd;

    if (scopeName != 0)
    {
        TypeHandle t = TypeHandle(ftn->GetMethodTable());
        *scopeName = "";
        if (!t.IsNull()) 
        {
#ifdef _DEBUG
            t.GetName(clsNameBuff, MAX_CLASSNAME_LENGTH);
            *scopeName = clsNameBuff;
#else 
            // since this is for diagnostic purposes only,
            // give up on the namespace, as we don't have a buffer to concat it
            // also note this won't show array class names.         
            LPCUTF8 nameSpace;
            *scopeName= t.GetClass()->GetFullyQualifiedNameInfo(&nameSpace);
#endif
        }
    }
    return(ftn->GetName());
}

/*********************************************************************/
DWORD __stdcall CEEInfo::getMethodAttribs (CORINFO_METHOD_HANDLE ftnHnd, CORINFO_METHOD_HANDLE callerHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

/*
        returns method attribute flags (defined in corhdr.h)

        NOTE: This doesn't return certain method flags
        (mdAssem, mdFamANDAssem, mdFamORAssem, mdPrivateScope)
*/
    START_NON_JIT_PERF();

    MethodDesc* ftn = (MethodDesc*) ftnHnd;
    MethodDesc* caller = (MethodDesc*) callerHnd;


    DWORD ret;
    DWORD ret0;

    ret0 = ftn->GetAttrs();
    ret  = 0;

    if (IsMdPublic(ret0))
        ret |= CORINFO_FLG_PUBLIC;
    if (IsMdPrivate(ret0))
        ret |= CORINFO_FLG_PRIVATE;
    if (IsMdFamily(ret0))
        ret |= CORINFO_FLG_PROTECTED;
    if (IsMdStatic(ret0))
        ret |= CORINFO_FLG_STATIC;
    if (IsMdFinal(ret0))
        ret |= CORINFO_FLG_FINAL;
    if (ftn->IsSynchronized()) 
        ret |= CORINFO_FLG_SYNCH;
    if (ftn->CouldBeFCall())
        ret |= CORINFO_FLG_NOGCCHECK | CORINFO_FLG_INTRINSIC;
    if (IsMdVirtual(ret0))
        ret |= CORINFO_FLG_VIRTUAL;
    if (IsMdAbstract(ret0))
        ret |= CORINFO_FLG_ABSTRACT;
    if (IsMdInstanceInitializer(ret0, ftn->GetName()) ||
        IsMdClassConstructor(   ret0, ftn->GetName()))
        ret |= CORINFO_FLG_CONSTRUCTOR;
    
    //
    // See if we need to embed a .cctor call at the head of the
    // method body.
    //
    
    EEClass* pClass = ftn->GetClass();
    if (pClass->IsShared() 

        // Only if the class has a .cctor or static handles
        // (note for shared classes, this is exactly the case when the ClassInited bit is set)
        && (!pClass->GetMethodTable()->IsClassInited())

        // Only if strict init is required - otherwise we can wait for
        // field accesses to run .cctor
        && !IsTdBeforeFieldInit(pClass->GetAttrClass())

        // Run .cctor on statics & constructors
        && (IsMdInstanceInitializer(ret0, ftn->GetName()) || IsMdStatic(ret0))

        // Except don't class construct on .cctor - it would be circular
        && !IsMdClassConstructor(ret0, ftn->GetName())

        // if same class, it must have been run already.
        // Note that jit has both methods the same if asking whether to emit cctor
        // for a given method's code (as opposed to inlining codegen).
        && ((caller == ftn) || caller->GetClass() != pClass)

        // System assembly always waits for field accesses
        && (ftn->GetModule()->GetAssembly() != SystemDomain::SystemAssembly())

        )       
        ret |= CORINFO_FLG_RUN_CCTOR;

    // method may not have the final bit, but the class might
    if (IsMdFinal(ret0) == 0)
    {
        DWORD dwAttrClass = pClass->GetAttrClass();

        if (IsTdSealed(dwAttrClass))
            ret |= CORINFO_FLG_FINAL;
    }

    if (ftn->IsEnCNewVirtual())
    {
        ret |= CORINFO_FLG_EnC;
    }

    if (ftn->IsSharedByGenericInstantiations())
    {
        ret |= CORINFO_FLG_SHAREDINST;
    }

    if (ftn->IsNDirect())
    {
        if (!Security::IsSecurityOn() || (ftn->GetSecurityFlags() == 0))
            ret |= CORINFO_FLG_UNCHECKEDPINVOKE;
    }

    // Turn off inlining for the object class as contextful and
    // marshalbyref classes derive from it and we need to intercept
    // calls on such classes for remoting (we can make an exception for constructors of object
    if (ftn->IsNotInline() || ((pClass == g_pObjectClass->GetClass()) && !ftn->IsCtor()))
    {
        /* Function marked as not inlineable */
        ret |= CORINFO_FLG_DONT_INLINE;
    }

    if (!ftn->IsECall())
    {
        // now I have too many positives (too many EBP frames)

        MethodDesc* method = ftn;
        if (Security::HasREQ_SOAttribute(method->GetMDImport(), method->GetMemberDef()) == S_OK)
        {
            ret |= CORINFO_FLG_SECURITYCHECK;
        }

    }

    if (pClass->IsSingleDelegateClass() && ((DelegateEEClass*)pClass)->m_pInvokeMethod == ftn)
    {
        ret |= CORINFO_FLG_DELEGATE_INVOKE;
    }

    STOP_NON_JIT_PERF();

    return ret;
}

static void *GetClassSync(MethodTable *pMT)
{
    void *ret;
    COOPERATIVE_TRANSITION_BEGIN();

    OBJECTREF ref = NULL;

    ref = pMT->GetExposedClassObject();
    if (ref == NULL)
        ret = NULL;
    else
        ret = (void*)ref->GetSyncBlock()->GetMonitor();

    COOPERATIVE_TRANSITION_END();
    return ret;
}

/*********************************************************************/
void* __stdcall CEEInfo::getMethodSync(CORINFO_METHOD_HANDLE ftnHnd,
                                       void **ppIndirection)
{
    REQUIRES_4K_STACK;

    void *ret;

    COOPERATIVE_TRANSITION_BEGIN();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    ret = GetClassSync(((MethodDesc*)ftnHnd)->GetMethodTable());

    COOPERATIVE_TRANSITION_END();
    return ret;
}

/*********************************************************************/
void __stdcall CEEInfo::setMethodAttribs (
        CORINFO_METHOD_HANDLE ftnHnd,
        DWORD          attribs)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    MethodDesc* ftn = (MethodDesc*) ftnHnd;

    if (attribs & CORINFO_FLG_DONT_INLINE)
        ftn->SetNotInline(true);

    //_ASSERTE(!"NYI");
}

/*********************************************************************/
void * __stdcall    CEEInfo::getMethodEntryPoint(CORINFO_METHOD_HANDLE ftnHnd,
                                                 CORINFO_ACCESS_FLAGS  flags,
                                                 void **ppIndirection)
{
    REQUIRES_4K_STACK;

    void * addrOfCode = NULL;
    COOPERATIVE_TRANSITION_BEGIN();
    THROWSCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    MethodDesc * func = (MethodDesc*)ftnHnd;
    addrOfCode = (void *)func->GetAddrOfCodeForLdFtn();
    _ASSERTE(addrOfCode);

    STOP_NON_JIT_PERF();
    COOPERATIVE_TRANSITION_END();
    return(addrOfCode);
}

/*********************************************************************/
bool __stdcall      CEEInfo::getMethodInfo (CORINFO_METHOD_HANDLE     ftnHnd,
                                               CORINFO_METHOD_INFO * methInfo)
{
    bool ret;
    REQUIRES_16K_STACK;
    COOPERATIVE_TRANSITION_BEGIN();
	THROWSCOMPLUSEXCEPTION();

    MethodDesc * ftn = (MethodDesc*) ftnHnd;
    HRESULT hr;

    /* Return false if not IL or has no code */
    if (!ftn->IsIL() || !ftn->GetRVA()) {
        ret = false;
        goto exit;
    }
    else {
        /* Get the IL header */
        /*                                                                      
                                                            */
        Module* pModule = ftn->GetModule();
        bool verify = !Security::LazyCanSkipVerification(pModule);
        COR_ILMETHOD_DECODER header(ftn->GetILHeader(), ftn->GetMDImport(), verify);

        if(verify && header.Code)
        {
            IMAGE_DATA_DIRECTORY dir;
            dir.VirtualAddress = VAL32(ftn->GetRVA());
            dir.Size = VAL32(header.GetCodeSize() + (header.EH ? header.EH->DataSize() : 0));
            if (pModule->IsPEFile() &&
                FAILED(pModule->GetPEFile()->VerifyDirectory(&dir,IMAGE_SCN_MEM_WRITE))) header.Code = 0;
        }
        BAD_FORMAT_ASSERT(header.Code != 0);
        if (header.Code == 0)
            COMPlusThrowHR(COR_E_BADIMAGEFORMAT);
        
        /* Grab information from the IL header */

        methInfo->ftn             = CORINFO_METHOD_HANDLE(ftn);
        methInfo->scope           = GetScopeHandle(ftn);
        methInfo->ILCode          = const_cast<BYTE*>(header.Code);
        methInfo->ILCodeSize      = header.GetCodeSize();
        methInfo->maxStack        = header.GetMaxStack();
        methInfo->EHcount         = header.EHCount();

        _ASSERTE(CORINFO_OPT_INIT_LOCALS == (CorInfoOptions) CorILMethod_InitLocals);
        methInfo->options         = (CorInfoOptions) header.GetFlags();

        /* Fetch the method signature */
	// Type parameters in the signature should be instantiated according to the class/method/array instantiation of ftnHnd
        hr = ConvToJitSig(ftn->GetSig(), GetScopeHandle(ftn), mdTokenNil, &methInfo->args, ftnHnd, false);
        _ASSERTE(SUCCEEDED(hr));

        //@GENERICS:
        // Shared generic methods and shared methods on generic structs take an extra argument representing their instantiation
        if (ftn->HasInstParam())
          methInfo->args.callConv = (CorInfoCallConv) (methInfo->args.callConv | CORINFO_CALLCONV_PARAMTYPE);

        _ASSERTE( (IsMdStatic(ftn->GetAttrs()) == 0) == ((methInfo->args.callConv & CORINFO_CALLCONV_HASTHIS) != 0));

        /* And its local variables */
	// Type parameters in the signature should be instantiated according to the class/method/array instantiation of ftnHnd
        hr = ConvToJitSig(header.LocalVarSig, GetScopeHandle(ftn), mdTokenNil, &methInfo->locals, ftnHnd, true);
        _ASSERTE(SUCCEEDED(hr));

        LOG((LF_JIT, LL_INFO100000, "Getting method info (possible inline) %s::%s%s\n",
            ftn->m_pszDebugMethodName, ftn->m_pszDebugClassName, ftn->m_pszDebugMethodSignature));

        ret = true;
    }

exit:
    COOPERATIVE_TRANSITION_END();
    return ret;
}

/*************************************************************
 * Check if the caller and calle are in the same assembly
 * i.e. do not inline across assemblies
 *************************************************************/

CorInfoInline __stdcall      CEEInfo::canInline (CORINFO_METHOD_HANDLE hCaller,
                                        CORINFO_METHOD_HANDLE hCallee,
                                        CORINFO_ACCESS_FLAGS  flags)
{
    CorInfoInline res = INLINE_FAIL;

    REQUIRES_16K_STACK;
    COOPERATIVE_TRANSITION_BEGIN();
    THROWSCOMPLUSEXCEPTION();

    // Returns TRUE: if caller and callee are from the same assembly or the callee
    //               is part of the system assembly.
    //
    // If the caller and callee are from the same security and visibility scope
    // then the callee can be inlined into the caller. If they are not from the
    // same visibility scope then the callee cannot safely be inlined. For example,
    // if method b in class B in assembly BB calls method a in class A in assembly
    // AA it is not always safe to inline method a into method b. If method a makes
    // calles to methods in assembly AA that are private they would fail when made
    // from b. For security, if the callee does not have the same set of permissions
    // as the caller then subsequent security checks will have lost the set of permissions
    // granted method a. This can lead to security holes when permissions for a are
    // less then the granted permissions for b.


    MethodDesc* pCaller = (MethodDesc*) hCaller;
    MethodDesc* pCallee = (MethodDesc*) hCallee;

    _ASSERTE(pCaller->GetModule());
    _ASSERTE(pCaller->GetModule()->GetClassLoader());

    _ASSERTE(pCallee->GetModule());
    _ASSERTE(pCallee->GetModule()->GetClassLoader());


    if(pCallee->GetModule()->GetClassLoader() == SystemDomain::Loader())
        res = INLINE_PASS;
    else if(pCallee->GetModule()->GetClassLoader() != pCaller->GetModule()->GetClassLoader())
        res = INLINE_RESPECT_BOUNDARY;
    else
        res = INLINE_PASS;

#ifdef PROFILING_SUPPORTED
    if (IsProfilerPresent())
    {
        // If the profiler has set a mask preventing inlining, always return
        // false to the jit.
        if (CORProfilerDisableInlining())
            res = INLINE_FAIL;

        // If the profiler wishes to be notified of JIT events and the result from
        // the above tests will cause a function to be inlined, we need to tell the
        // profiler that this inlining is going to take place, and give them a
        // chance to prevent it.
        if (CORProfilerTrackJITInfo() && res != INLINE_FAIL)
        {
            BOOL fShouldInline;

            HRESULT hr = g_profControlBlock.pProfInterface->JITInlining(
                (ThreadID)GetThread(),
                (FunctionID)pCaller,
                (FunctionID)pCallee,
                &fShouldInline);

            if (SUCCEEDED(hr) && !fShouldInline)
                res = INLINE_FAIL;
        }
    }
#endif // PROFILING_SUPPORTED

	//  Inliner does not do code verification.
	//  Never inline anything that is not verifiable / bad code.
	//  Return false here if the jit fails or if jit emits verification 
	//  exception. Trusted code could ignore this step.

	/* Allow trusted code, but don't do a complete policy resolve here. */
	if (!dontInline(res) && !pCallee->IsVerified() && !Security::LazyCanSkipVerification(pCaller->GetModule()))
	{
		/* JIT will set the CORINFO_FLG_DONT_INLINE flag */ 
		/* Jit this method and check if this is a safe method */

        COR_ILMETHOD_DECODER header(pCallee->GetILHeader(), pCallee->GetMDImport(), true /*VERIFY*/);
		if(header.Code)
		{
			IMAGE_DATA_DIRECTORY dir;
			dir.VirtualAddress = VAL32(pCallee->GetRVA());
			dir.Size = VAL32(header.GetCodeSize() + (header.EH ? header.EH->DataSize() : 0));
			if (pCallee->GetModule()->IsPEFile() &&
				FAILED(pCallee->GetModule()->GetPEFile()->VerifyDirectory(&dir,IMAGE_SCN_MEM_WRITE))) header.Code = 0;
		}
        BAD_FORMAT_ASSERT(header.Code != 0);
        if (header.Code == 0)
            COMPlusThrowHR(COR_E_BADIMAGEFORMAT);

		BOOL dummy;
		JITFunction(pCallee, &header, &dummy, CORJIT_FLG_IMPORT_ONLY);

		if (pCallee->IsNotInline() || !pCallee->IsVerified())
			res = INLINE_NEVER;
	}

    COOPERATIVE_TRANSITION_END();
	return (res);
}

/*************************************************************
 * Check if a method to be compiled is an instantiation
 * of generic code that has already been verified.  Three
 * possible return values (see corinfo.h)
 *************************************************************/
CorInfoInstantiationVerification __stdcall  CEEInfo::isInstantiationOfVerifiedGeneric(CORINFO_METHOD_HANDLE hMethod)
{
    CorInfoInstantiationVerification res;

    REQUIRES_16K_STACK;
    COOPERATIVE_TRANSITION_BEGIN();

    LOG((LF_JIT, LL_INFO1000, "GENERICSVER: entering isInstantiationOfVerifiedGeneric\n"));

    MethodDesc* pMethod = (MethodDesc*)hMethod;

    if (!(pMethod->HasClassInstantiation() || pMethod->HasMethodInstantiation()))
    {
        res = INSTVER_NOT_INSTANTIATION;
    }
    else
    {
        MethodTable *pMT;
        if (pMethod->HasClassInstantiation())
        {
            EEClass *pClass = pMethod->GetClass();
            TypeHandle th = ClassLoader::LoadGenericInstantiation(
                                                                  pMethod->GetMethodTable()->GetGenericTypeDefinition(),
                                                                  pClass->GetMethodTable()->GetInstantiation(),
                                                                  pClass->GetNumGenericArgs()
                                                                  );
            pMT = th.GetMethodTable();
        } 
        else 
        {
            pMT = pMethod->GetMethodTable();
        }
        
        MethodDesc *pGenMethod;
        if (pMethod->HasMethodInstantiation())
        { InstantiatedMethodDesc *pIMD = (InstantiatedMethodDesc *) pMT->GetMethodDescForSlot(pMethod->GetSlot());
        pGenMethod = InstantiatedMethodDesc::CreateGenericInstantiation(pMT,pIMD,pIMD->GetTypicalMethodInstantiation(),pIMD->GetNumGenericMethodArgs());
        }
        else 
        {  
            pGenMethod = pMT->GetMethodDescForSlot(pMethod->GetSlot());
        }
        
        // We use physical equality to detect whether pMethod is the typical instantiation, pGenMethod, just created
        if (pGenMethod == pMethod)
        {
            res = INSTVER_NOT_INSTANTIATION;
        }
        else
        {
            bool passedOrSkippedVerification = false;
            _ASSERTE(pGenMethod->GetModule());
            _ASSERTE(pGenMethod->GetModule()->GetClassLoader());
            
            if (pGenMethod->IsVerified())
            {
                passedOrSkippedVerification = !pGenMethod->IsNotInline();
                LOG((LF_JIT, LL_INFO1000, "GENERICSVER: skipping verification of generic method, IsVerified = %d, IsNotInline = %d \n", pGenMethod->IsVerified(),pGenMethod->IsNotInline()));
            }
            else if (Security::LazyCanSkipVerification(pGenMethod->GetModule()))
            {
                LOG((LF_JIT, LL_INFO1000, "GENERICSVER: skipping verification of generic method, IsVerified = %d, IsNotInline = %d \n", pGenMethod->IsVerified(),pGenMethod->IsNotInline()));
                passedOrSkippedVerification = true;
            }
            else
            {
                COR_ILMETHOD_DECODER header(pGenMethod->GetILHeader(), pGenMethod->GetMDImport(), true /*VERIFY*/);
                if(header.Code)
                {
                    IMAGE_DATA_DIRECTORY dir;
                    dir.VirtualAddress = pGenMethod->GetRVA();
                    dir.Size = header.CodeSize + (header.EH ? header.EH->DataSize() : 0);
                    if (pGenMethod->GetModule()->IsPEFile() &&
                        FAILED(pGenMethod->GetModule()->GetPEFile()->VerifyDirectory(&dir,IMAGE_SCN_MEM_WRITE))) header.Code = 0;
                }
                BAD_FORMAT_ASSERT(header.Code != 0);
                if (header.Code == 0)
                    COMPlusThrowHR(COR_E_BADIMAGEFORMAT);
                
#ifdef _X86_
                
                LOG((LF_JIT, LL_INFO1000, "GENERICSVER: verifying generic method\n"));
                
                BOOL dummy;
                JITFunction(pGenMethod, &header, &dummy, CORJIT_FLG_IMPORT_ONLY);
                
                passedOrSkippedVerification = !pGenMethod->IsNotInline();
                LOG((LF_JIT, LL_INFO1000, "GENERICSVER: generic method IsVerified = %d, IsNotInline = %d \n",pGenMethod->IsVerified(),pGenMethod->IsNotInline()));
                
#endif
            }
            res = 
                passedOrSkippedVerification
                ? INSTVER_GENERIC_PASSED_OR_SKIPPED_VERIFICATION 
                : INSTVER_GENERIC_FAILED_VERIFICATION;
        }
    }
    COOPERATIVE_TRANSITION_END();
    return (res );

}


/*************************************************************
 * Similar to above, but perform check for tail call
 * eligibility. The callee can be passed as NULL if not known
 * (calli and callvirt).
 *************************************************************/

bool __stdcall      CEEInfo::canTailCall (CORINFO_METHOD_HANDLE hCaller,
                                          CORINFO_METHOD_HANDLE hCallee,
                                          CORINFO_ACCESS_FLAGS  flags)
{
    bool ret;

    // See comments in canInline above.

    MethodDesc* pCaller = (MethodDesc*) hCaller;
    MethodDesc* pCallee = (MethodDesc*) hCallee;

    _ASSERTE(pCaller->GetModule());
    _ASSERTE(pCaller->GetModule()->GetClassLoader());

    _ASSERTE((pCallee == NULL) || pCallee->GetModule());
    _ASSERTE((pCallee == NULL) || pCallee->GetModule()->GetClassLoader());

    // If the callee is not known (callvirt, calli) or the caller and callee are
    // in different assemblies, we cannot allow the tailcall (since we'd
    // optimize away what might be the only stack frame for a given assembly,
    // skipping a security check).
    if(pCallee == NULL ||
       pCallee->GetModule()->GetClassLoader() != pCaller->GetModule()->GetClassLoader())
    {

        // We'll allow code with SkipVerification permission to tailcall into
        // another assembly anyway. It's the responsiblity of such code to make
        // sure that they're not opening up a security hole by calling untrusted
        // code in such cases.
        return !Security::IsSecurityOn() || 
			   pCaller->GetModule()->GetSecurityDescriptor()->CanSkipVerification() != 0;
    }
    else
        ret = TRUE;

    return ret;
}


/*********************************************************************/
// get individual exception handler
void __stdcall CEEInfo::getEHinfo(
            CORINFO_METHOD_HANDLE ftnHnd,
            unsigned      EHnumber,
            CORINFO_EH_CLAUSE* clause)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();

    MethodDesc * ftn          = (MethodDesc*) ftnHnd;

	COR_ILMETHOD_DECODER header(ftn->GetILHeader(), ftn->GetMDImport());

     _ASSERTE(header.EH);
    _ASSERTE(EHnumber < header.EH->EHCount());

    COR_ILMETHOD_SECT_EH_CLAUSE_FAT ehClause;

    const COR_ILMETHOD_SECT_EH_CLAUSE_FAT* ehInfo;
    ehInfo = header.EH->EHClause(EHnumber, &ehClause);

    clause->Flags = (CORINFO_EH_CLAUSE_FLAGS)ehInfo->GetFlags();
    clause->TryOffset = ehInfo->GetTryOffset();
    clause->TryLength = ehInfo->GetTryLength();
    clause->HandlerOffset = ehInfo->GetHandlerOffset();
    clause->HandlerLength = ehInfo->GetHandlerLength();
    clause->ClassToken = ehInfo->GetClassToken();
    clause->FilterOffset = ehInfo->GetFilterOffset();

    STOP_NON_JIT_PERF();
}

/*********************************************************************/
void __stdcall CEEInfo::getMethodSig (
            CORINFO_METHOD_HANDLE ftnHnd,
            CORINFO_SIG_INFO* sigRet,
            CORINFO_CLASS_HANDLE owner
            )
{
    REQUIRES_8K_STACK;

    COOPERATIVE_TRANSITION_BEGIN();

    START_NON_JIT_PERF();

    MethodDesc* ftn = (MethodDesc*) ftnHnd;
    PCCOR_SIGNATURE sig = ftn->GetSig();

    STOP_NON_JIT_PERF();

#ifdef _DEBUG
    HRESULT hr =
#endif
    ConvToJitSig(sig, GetScopeHandle(ftn), mdTokenNil, sigRet, ftnHnd, false, (TypeHandle) owner);
    _ASSERTE(SUCCEEDED(hr));

    //@GENERICS:
    // Shared generic methods and shared methods on generic structs take an extra argument representing their instantiation
    if (ftn->HasInstParam())
      sigRet->callConv = (CorInfoCallConv) (sigRet->callConv | CORINFO_CALLCONV_PARAMTYPE);


        // We want the calling convention bit to be consistant with the method attribute bit
    _ASSERTE( (IsMdStatic(ftn->GetAttrs()) == 0) == ((sigRet->callConv & CORINFO_CALLCONV_HASTHIS) != 0) );

    COOPERATIVE_TRANSITION_END();
}


/***********************************************************************/
// For a method desc in a typical instantiation of a generic class, 
// this will return the typical instantiation of the generic class, 
// but only provided type variables are never shared.
// The JIT verifier relies on this behaviour to extract the typical class from an instantiated method's typical method handle.
CORINFO_CLASS_HANDLE __stdcall CEEInfo::getMethodClass (CORINFO_METHOD_HANDLE methodHnd)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();

    MethodDesc* method = (MethodDesc*)methodHnd;
    TypeHandle th = TypeHandle(method->GetMethodTable());
    CORINFO_CLASS_HANDLE clsHnd = CORINFO_CLASS_HANDLE(th.AsPtr());

    STOP_NON_JIT_PERF();
    return clsHnd;
}

/***********************************************************************/
CORINFO_MODULE_HANDLE __stdcall CEEInfo::getMethodModule (CORINFO_METHOD_HANDLE methodHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();

    MethodDesc* method = (MethodDesc*) methodHnd;
        CORINFO_MODULE_HANDLE moduleHnd = (CORINFO_MODULE_HANDLE) method->GetModule();

    STOP_NON_JIT_PERF();
    return moduleHnd;
}

/*********************************************************************/
CorInfoIntrinsics __stdcall CEEInfo::getIntrinsicID(CORINFO_METHOD_HANDLE methodHnd)
{
    REQUIRES_4K_STACK;

    CorInfoIntrinsics ret;
    COOPERATIVE_TRANSITION_BEGIN();

    MethodDesc* method = (MethodDesc*) methodHnd;

    if (method->GetClass()->IsArrayClass())
    {
        ArrayECallMethodDesc * arrMethod = (ArrayECallMethodDesc *)method;
        ret = CorInfoIntrinsics(arrMethod->m_intrinsicID);
    }
    else
    {
        ret = ECall::IntrinsicID(method);
    }

    COOPERATIVE_TRANSITION_END();
    return ret;
}

/*********************************************************************/
unsigned __stdcall CEEInfo::getMethodVTableOffset (CORINFO_METHOD_HANDLE methodHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();
    unsigned methodVtabOffset;

    MethodDesc* method = (MethodDesc*) methodHnd;

    //@GENERICS: shouldn't be doing this for instantiated methods as they live elsewhere
    _ASSERTE(!method->HasMethodInstantiation());

    const DWORD methTabOffset = MethodTable::GetOffsetOfVtable();
    _ASSERTE(methTabOffset < 256);  // a rough sanity check

        // better be in the vtable
    _ASSERTE(method->GetSlot() < method->GetClass()->GetNumVtableSlots());

    if (method->GetClass()->IsInterface())
        methodVtabOffset = method->GetSlot()*sizeof(void*);
    else
        methodVtabOffset = method->GetSlot()*sizeof(void*) + methTabOffset;

    STOP_NON_JIT_PERF();
    return methodVtabOffset;
}

/*********************************************************************/
void** __stdcall CEEInfo::AllocHintPointer(CORINFO_METHOD_HANDLE methodHnd,
                                           void **ppIndirection)
{
    REQUIRES_4K_STACK;
    _ASSERTE(!"Old style interface invoke no longer supported.");

    return NULL;
}

/*********************************************************************/
void* __stdcall CEEInfo::getMethodPointer(CORINFO_METHOD_HANDLE ftnHnd,
                                          CORINFO_ACCESS_FLAGS  flags,
                                          void **ppIndirection)
{
    REQUIRES_4K_STACK;

    void *ret;

    COOPERATIVE_TRANSITION_BEGIN();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    MethodDesc* ftn = (MethodDesc*) ftnHnd;

    // Strings and Arrays are variable sized objects and so they
    // do not have an instance allocated before this method gets called.
    // We defer to the non-thunking path in this case.
    // Static methods by definition do not have an object for the method
    // called, hence it takes the non-thunking path too.
    // Value classes do not expect calls through method table pointer
    // and hence take the non-thunking path
    // Non contextful and agile (ie not marshal by ref) classes also
    // do not require the thunking path
    // Contextful and marshalbyref classes derive from Object class
    // so we need to take the thunking path for methods on Object class
    // for remoting
    //
    EEClass* pClass = ftn->GetClass();
    LPVOID pvCode = NULL;

    // Check for calling a virtual method on marshalbyref class or object class
    // non virtually
    //
    //
    if(ftn->IsRemotingIntercepted2()) {

        // Contextful classes imply marshal by ref but not vice versa
        _ASSERTE(!pClass->IsContextful() || pClass->IsMarshaledByRef());

        // This call will find or create the thunk and store it in
        // a hash table
         pvCode = (LPVOID)CRemotingServices::GetNonVirtualThunkForVirtualMethod(ftn); // throws
        _ASSERTE(NULL != pvCode);
    } 
    else 
    {
        if (ftn->HasMethodInstantiation())
        {
          pvCode = ((InstantiatedMethodDesc*) ftn)->GetCodeAddr();
        }
        else
          pvCode = pClass->GetMethodSlot(ftn);
    } 

    _ASSERTE(pvCode);
    ret = pvCode;

    COOPERATIVE_TRANSITION_END();
    return ret;

}

/*********************************************************************/
void* __stdcall CEEInfo::getFunctionEntryPoint(
                                CORINFO_METHOD_HANDLE ftnHnd,
                                InfoAccessType       *pAccessType,
                                CORINFO_ACCESS_FLAGS  flags)
{
    REQUIRES_4K_STACK;
    //COOPERATIVE_TRANSITION_BEGIN();   // Not needed -- getMethodPointer is guarded.
    _ASSERTE(*pAccessType == IAT_VALUE || *pAccessType == IAT_PVALUE || *pAccessType == IAT_PPVALUE);

    /* If the method's prestub has already been executed, we can call
       the function directly.
     */

    MethodDesc * ftn = (MethodDesc*) ftnHnd;
    if (!ftn->PointAtPreStub() && !ftn->MayBeRemotingIntercepted() && *pAccessType == IAT_VALUE
        )
    {
                *pAccessType = IAT_VALUE;

                //@GENERICS: extract address from method descriptor itself; why isn't this done normally anyhow?
                if (ftn->HasMethodInstantiation())
                        return (void*) (ftn->GetUnsafeAddrofCode());
                return *(ftn->GetClass()->GetMethodSlot(ftn));
    }

    void *pAddr;
    void * addr = getMethodPointer(ftnHnd, flags, &pAddr);    // throws
    _ASSERTE((!addr) != (!pAddr));

    // Double-indirections not needed by EE
    _ASSERTE(addr);

    *pAccessType = IAT_PVALUE;
    //COOPERATIVE_TRANSITION_END();
    return addr;
}

/*********************************************************************/
void* __stdcall CEEInfo::getFunctionFixedEntryPoint(CORINFO_METHOD_HANDLE ftn,
                                            InfoAccessType       *pAccessType,
                                            CORINFO_ACCESS_FLAGS  flags)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();
    _ASSERTE(*pAccessType == IAT_VALUE || *pAccessType == IAT_PVALUE || *pAccessType == IAT_PPVALUE);

    void *pAddr;
    void * addr = getMethodEntryPoint(ftn, flags, &pAddr);
    _ASSERTE((!addr) != (!pAddr));
    _ASSERTE(addr); // The EE doesnt need the JIT to use an indirection

    *pAccessType = IAT_VALUE;
    return addr;
}

/*********************************************************************/
CorInfoCallCategory __stdcall CEEInfo::getMethodCallCategory(CORINFO_METHOD_HANDLE      ftnHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    MethodDesc *ftn = (MethodDesc*)ftnHnd;

    DWORD attribs = ftn->GetAttrs();

    if (IsMdPrivate(attribs)
        || IsMdStatic(attribs)
        || IsMdFinal(attribs)
        || IsMdInstanceInitializer(attribs, ftn->GetName())
        || IsMdClassConstructor(attribs, ftn->GetName()))
    {
        return CORINFO_CallCategoryPointer;
    }
    else
    {
        return CORINFO_CallCategoryVTableOffset;
    }

}





/*********************************************************************/
BOOL __stdcall CEEInfo::canPutField(
        CORINFO_METHOD_HANDLE methodHnd,
        CORINFO_FIELD_HANDLE fieldHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

     return(1);
}

/*********************************************************************/
const char* __stdcall CEEInfo::getFieldName (CORINFO_FIELD_HANDLE fieldHnd, const char** scopeName)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    FieldDesc* field = (FieldDesc*) fieldHnd;
    if (scopeName != 0)
    {
        TypeHandle t = TypeHandle(field->GetMethodTableOfEnclosingClass());
        *scopeName = "";
        if (!t.IsNull())
        {
#ifdef _DEBUG
            t.GetName(clsNameBuff, MAX_CLASSNAME_LENGTH);
            *scopeName = clsNameBuff;
#else 
            // since this is for diagnostic purposes only,
            // give up on the namespace, as we don't have a buffer to concat it
            // also note this won't show array class names.
            LPCUTF8 nameSpace;
            *scopeName= t.GetClass()->GetFullyQualifiedNameInfo(&nameSpace);
#endif
        }
    }
    return(field->GetName());
}


/*********************************************************************/
DWORD __stdcall CEEInfo::getFieldAttribs (CORINFO_FIELD_HANDLE  fieldHnd, 
                                          CORINFO_METHOD_HANDLE context,
                                          CORINFO_ACCESS_FLAGS  flags)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

/*
        returns field attribute flags (defined in corhdr.h)

        NOTE: This doesn't return certain field flags
        (fdAssembly, fdFamANDAssem, fdFamORAssem, fdPrivateScope)
*/
    START_NON_JIT_PERF();

    FieldDesc* field = (FieldDesc*) fieldHnd;
    DWORD ret;

    DWORD ret0 = field->GetAttributes();
    ret = 0;

    if (IsFdPublic(ret0))
        ret |= CORINFO_FLG_PUBLIC;
    if (IsFdPrivate(ret0))
        ret |= CORINFO_FLG_PRIVATE;
    if (IsFdFamily(ret0))
        ret |= CORINFO_FLG_PROTECTED;
    if (IsFdStatic(ret0))
        ret |= CORINFO_FLG_STATIC;
    if (IsFdInitOnly(ret0))
        ret |= CORINFO_FLG_FINAL;

    if (IsFdHasFieldRVA(ret0))
    {
        ret |= CORINFO_FLG_UNMANAGED;

    }

    /*                  
           */

    /*                  
           */

    if (field->IsEnCNew())
        ret |= CORINFO_FLG_EnC;

    if (field->IsStatic())
    {
        // static field reference

        if (field->IsThreadStatic() || field->IsContextStatic())
        {
            ret |= CORINFO_FLG_HELPER;
        }
        else
        {
            if (!field->IsSpecialStatic())   // Special means RVA, context or thread local 
			{
				if (field->GetFieldType() == ELEMENT_TYPE_VALUETYPE)
					ret |= CORINFO_FLG_STATIC_IN_HEAP;

				// Only use a helper to access static fields inside shared assemblies.
				MethodDesc* contextMethod = (MethodDesc*) context;
				if (contextMethod->GetMethodTable()->IsShared())
				{
					_ASSERTE(field->GetMethodTableOfEnclosingClass()->IsShared());
					ret |= CORINFO_FLG_SHARED_HELPER;
				}
			}
		}
    }
    else
    {
        // instance field reference

#if CHECK_APP_DOMAIN_LEAKS
        if (field->IsDangerousAppDomainAgileField()
            && CorTypeInfo::IsObjRef(field->GetFieldType()))
        {
            //
            // In a checked field, we use a helper to enforce the app domain
            // agile invariant.
            //
            ret |= CORINFO_FLG_HELPER;
        }
        else
#endif
        {
            EEClass * fldCls = field->GetEnclosingClass();

            if (fldCls->IsContextful())           // is contextful class (i,e. a proxy)
            {
                // If the caller is states that we have a 'this reference'
                // and he is also willing to unwrap it himself then
                // we won't require a helper call.
                if (!(flags & CORINFO_ACCESS_THIS  )  ||
                    !(flags & CORINFO_ACCESS_UNWRAP))
                {
                    // Normally a helper call is required.
                    ret |= CORINFO_FLG_HELPER;
                }
            }
            else if (fldCls->IsMarshaledByRef())  //  is marshaled by ref class
            {
                // If the caller is states that we have a 'this reference'
                // we won't require a helper call.
                if (!(flags & CORINFO_ACCESS_THIS))
                {
                    // Normally a helper call is required.
                    ret |= CORINFO_FLG_HELPER;
                }
            }
        }
    }


    STOP_NON_JIT_PERF();

    return(ret);
}

/*********************************************************************/
CORINFO_CLASS_HANDLE __stdcall CEEInfo::getFieldClass (CORINFO_FIELD_HANDLE fieldHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    FieldDesc* field = (FieldDesc*) fieldHnd;
    return(CORINFO_CLASS_HANDLE(TypeHandle(field->GetMethodTableOfEnclosingClass()).AsPtr()));
}

/*********************************************************************/
CorInfoType __stdcall CEEInfo::getFieldType (CORINFO_FIELD_HANDLE fieldHnd, CORINFO_CLASS_HANDLE* structType, CORINFO_CLASS_HANDLE owner)
{
    REQUIRES_8K_STACK;

    *structType = 0;
    CorElementType type;

    CorInfoType ret;
    TypeHandle clsHnd = TypeHandle();
    COOPERATIVE_TRANSITION_BEGIN();
    THROWSCOMPLUSEXCEPTION();

    FieldDesc* field = (FieldDesc*) fieldHnd;
    type = field->GetFieldType();

    _ASSERTE(type != ELEMENT_TYPE_BYREF);

    // For verifying code involving generics, use the class instantiation
    // of the optional owner (to provide exact, not representative, 
    // type information)
    TypeHandle* classInst = field->GetExactClassInstantiation((TypeHandle) owner);

    if (!CorTypeInfo::IsPrimitiveType(type))
    {
        PCCOR_SIGNATURE sig;
        DWORD sigCount;
        field->GetSig(&sig, &sigCount);
#ifdef _DEBUG
        CorCallingConvention conv =
#endif
        (CorCallingConvention) CorSigUncompressCallingConv(sig);
        _ASSERTE(isCallConv(conv, IMAGE_CEE_CS_CALLCONV_FIELD));

        SigPointer ptr(sig);

        OBJECTREF Throwable = NULL;
        GCPROTECT_BEGIN(Throwable);
        clsHnd = ptr.GetTypeHandle(field->GetModule(), &Throwable, FALSE, FALSE, FALSE, classInst);

        if (clsHnd.IsNull()) {
            if (Throwable == NULL)
                COMPlusThrow(kTypeLoadException);
            else
                COMPlusThrow(Throwable);
        }
        GCPROTECT_END();
        type = clsHnd.GetSigCorElementType();
    }

    ret = toJitType(type, clsHnd, structType);

    COOPERATIVE_TRANSITION_END();

    return(ret);
}

/*********************************************************************/
unsigned __stdcall CEEInfo::getFieldOffset (CORINFO_FIELD_HANDLE fieldHnd)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    FieldDesc* field = (FieldDesc*) fieldHnd;

    // GetOffset() does not include the size of Object
    unsigned ret = field->GetOffset();

    // only the small types are not DWORD aligned.
    // FIX put this back _ASSERTE(field->GetFieldType() < ELEMENT_TYPE_I4 || (ret & 3) == 0);


    // So if it is not a value class, add the Object into it
    if (field->IsStatic())
    {
        if (field->GetMethodTableOfEnclosingClass()->IsShared()) 
        {
            // helper returns base of DomainLocalClass, skip stuff before statics 
            ret += (unsigned)DomainLocalClass::GetOffsetOfStatics();
        }
        else 
        {
            Module* pModule = field->GetModule();
            BYTE* ptr = pModule->ResolveILRVA(field->GetOffset(), field->IsRVA());
            if (pModule->IsPEFile() && pModule->GetPEFile()->IsTLSAddress(ptr))
                ret = (unsigned)(size_t)(ptr - pModule->GetPEFile()->GetTLSDirectory()->StartAddressOfRawData);
            else {
            }
        }
    }
    else if (!field->GetEnclosingClass()->IsValueClass())
        ret += sizeof(Object);

    return(ret);
}

/*********************************************************************/
void* __stdcall CEEInfo::getFieldAddress(CORINFO_FIELD_HANDLE fieldHnd,
                                         void **ppIndirection)
{
    REQUIRES_4K_STACK;

    void *result;

    COOPERATIVE_TRANSITION_BEGIN();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    FieldDesc* field = (FieldDesc*) fieldHnd;
    void *base;

    if (field->GetMethodTableOfEnclosingClass()->IsShared())
    {

        // Allocate space for the local class if necessary, but don't trigger
        // class construction.
        DomainLocalBlock *pLocalBlock = GetAppDomain()->GetDomainLocalBlock();
        DomainLocalClass *pLocalClass = pLocalBlock->PopulateClass(field->GetMethodTableOfEnclosingClass());

        base = pLocalClass->GetStaticSpace();
    }
    else
        base = field->GetUnsharedBase();

    result = field->GetStaticAddressHandle(base);

    COOPERATIVE_TRANSITION_END();
    return result;

}

/*********************************************************************/
CorInfoHelpFunc __stdcall CEEInfo::getFieldHelper(CORINFO_FIELD_HANDLE fieldHnd, enum CorInfoFieldAccess kind)
{
    REQUIRES_4K_STACK;

    CANNOTTHROWCOMPLUSEXCEPTION();

    FieldDesc* field = (FieldDesc*) fieldHnd;
    CorElementType type = field->GetFieldType();

    if (kind == CORINFO_ADDRESS) {
        if (field->IsStatic()) {
            return(CORINFO_HELP_GETSTATICFIELDADDR);
        }
        else 
            return(CORINFO_HELP_GETFIELDADDR);
    }

    CorInfoHelpFunc ret;
    if (CorTypeInfo::IsObjRef(type))
        ret = CORINFO_HELP_GETFIELDOBJ;
    else if (CorTypeInfo::Size(type) <= 4)
        ret = CORINFO_HELP_GETFIELD32;
    else if (CorTypeInfo::Size(type) == 8)
        ret = CORINFO_HELP_GETFIELD64;
    else
    {
        _ASSERTE(type == ELEMENT_TYPE_VALUETYPE);
        ret = CORINFO_HELP_GETFIELDSTRUCT;
    }

    _ASSERTE(kind == CORINFO_GET || kind == CORINFO_SET);
    _ASSERTE(!field->IsStatic());       // Static fields always accessed through address
    _ASSERTE(CORINFO_GET == 0);
    _ASSERTE((int) CORINFO_HELP_SETFIELD32 == (int) CORINFO_HELP_GETFIELD32 + (int) CORINFO_SET);
    _ASSERTE((int) CORINFO_HELP_SETFIELD64 == (int) CORINFO_HELP_GETFIELD64 + (int) CORINFO_SET);
    _ASSERTE((int) CORINFO_HELP_SETFIELDOBJ == (int) CORINFO_HELP_GETFIELDOBJ + (int) CORINFO_SET);
    _ASSERTE((int) CORINFO_HELP_SETFIELDSTRUCT == (int) CORINFO_HELP_GETFIELDSTRUCT + (int) CORINFO_SET);

    return (CorInfoHelpFunc) (ret + kind);
}

/*********************************************************************/
CorInfoFieldCategory __stdcall CEEInfo::getFieldCategory (CORINFO_FIELD_HANDLE fieldHnd)
{
    REQUIRES_4K_STACK;
    return CORINFO_FIELDCATEGORY_NORMAL;
}

DWORD __stdcall CEEInfo::getFieldThreadLocalStoreID(CORINFO_FIELD_HANDLE fieldHnd, void **ppIndirection)
{
    _ASSERTE(false);
    return 0;
}

/*********************************************************************/
unsigned __stdcall CEEInfo::getIndirectionOffset ()
{
    REQUIRES_4K_STACK;
    return 0;//DEPRECATED                                 
}

void *CEEInfo::allocateArray(SIZE_T cBytes)
{
#ifdef DEBUGGING_SUPPORTED
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    return g_pDebugInterface->allocateArray(cBytes);
#else // !DEBUGGING_SUPPORTED
    return NULL;
#endif // !DEBUGGING_SUPPORTED
}

void CEEInfo::freeArray(void *array)
{
#ifdef DEBUGGING_SUPPORTED
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    g_pDebugInterface->freeArray(array);
#endif // DEBUGGING_SUPPORTED
}

void CEEInfo::getBoundaries(CORINFO_METHOD_HANDLE ftn,
                               unsigned int *cILOffsets, DWORD **pILOffsets,
                               ICorDebugInfo::BoundaryTypes *implicitBoundaries)
{
#ifdef DEBUGGING_SUPPORTED
    REQUIRES_N4K_STACK(16); // this is big, since we can't throw 
    CANNOTTHROWCOMPLUSEXCEPTION();

#ifdef _DEBUG
    DWORD dwDebugBits = ((MethodDesc*)ftn)->GetModule()->GetDebuggerInfoBits();
#ifdef PROFILING_SUPPORTED
    _ASSERTE(CORDebuggerTrackJITInfo(dwDebugBits) ||
             SystemDomain::GetCurrentDomain()->IsCompilationDomain() ||
             g_pConfig->GenDebugInfo() || CORProfilerJITMapEnabled());
#else
    _ASSERTE(CORDebuggerTrackJITInfo(dwDebugBits) ||
             SystemDomain::GetCurrentDomain()->IsCompilationDomain() ||
             g_pConfig->GenDebugInfo());
#endif

#endif

    g_pDebugInterface->getBoundaries(ftn, cILOffsets, pILOffsets,
                                     implicitBoundaries);
#endif // DEBUGGING_SUPPORTED
}

void CEEInfo::setBoundaries(CORINFO_METHOD_HANDLE ftn, ULONG32 cMap,
                               OffsetMapping *pMap)
{
#ifdef DEBUGGING_SUPPORTED
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    DWORD dwDebugBits = ((MethodDesc*)ftn)->GetModule()->GetDebuggerInfoBits();

    if (CORDebuggerTrackJITInfo(dwDebugBits) ||
        SystemDomain::GetCurrentDomain()->IsCompilationDomain()
#ifdef PROFILING_SUPPORTED
        || CORProfilerJITMapEnabled()
#endif
        )
    {
        g_pDebugInterface->setBoundaries(ftn, cMap, pMap);
    }
#endif // DEBUGGING_SUPPORTED
}

void CEEInfo::getVars(CORINFO_METHOD_HANDLE ftn, ULONG32 *cVars, ILVarInfo **vars,
                         bool *extendOthers)
{
#ifdef DEBUGGING_SUPPORTED
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

#ifdef _DEBUG
    DWORD dwDebugBits = ((MethodDesc*)ftn)->GetModule()->GetDebuggerInfoBits();
#ifdef PROFILING_SUPPORTED
    _ASSERTE(CORDebuggerTrackJITInfo(dwDebugBits) ||
             SystemDomain::GetCurrentDomain()->IsCompilationDomain() ||
             g_pConfig->GenDebugInfo() || CORProfilerJITMapEnabled());
#else
    _ASSERTE(CORDebuggerTrackJITInfo(dwDebugBits) ||
             SystemDomain::GetCurrentDomain()->IsCompilationDomain() ||
             g_pConfig->GenDebugInfo());
#endif
#endif

    g_pDebugInterface->getVars(ftn, cVars, vars, extendOthers);
#endif // DEBUGGING_SUPPORTED
}

void CEEInfo::setVars(CORINFO_METHOD_HANDLE ftn, ULONG32 cVars, NativeVarInfo *vars)
{
#ifdef DEBUGGING_SUPPORTED
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    DWORD dwDebugBits = ((MethodDesc*)ftn)->GetModule()->GetDebuggerInfoBits();
    if (CORDebuggerTrackJITInfo(dwDebugBits)
        || SystemDomain::GetCurrentDomain()->IsCompilationDomain()
#ifdef PROFILING_SUPPORTED
        || CORProfilerJITMapEnabled()
#endif
        )
        g_pDebugInterface->setVars(ftn, cVars, vars);
#endif // DEBUGGING_SUPPORTED
}

/*********************************************************************/
CORINFO_ARG_LIST_HANDLE __stdcall CEEInfo::getArgNext (
        CORINFO_ARG_LIST_HANDLE args
        )
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    SigPointer ptr(((unsigned __int8*) args));
    ptr.Skip();

        // I pass a SigPointer as a void* and back
    _ASSERTE(sizeof(SigPointer) == sizeof(BYTE*));
    return(*((CORINFO_ARG_LIST_HANDLE*) &ptr));
}

/*********************************************************************/
CorInfoTypeWithMod __stdcall CEEInfo::getArgType (
        CORINFO_SIG_INFO*       sig,
        CORINFO_ARG_LIST_HANDLE    args,
        CORINFO_CLASS_HANDLE       *vcTypeRet
        )
{
    CorInfoTypeWithMod ret = CorInfoTypeWithMod(CORINFO_TYPE_UNDEF);
    REQUIRES_8K_STACK;
    COOPERATIVE_TRANSITION_BEGIN();
    THROWSCOMPLUSEXCEPTION();     

   _ASSERTE((BYTE*) sig->sig <= (BYTE*) sig->args);
   _ASSERTE((BYTE*) sig->args <= (BYTE*) args && (BYTE*) args < &((BYTE*) sig->args)[0x10000*5]);
    INDEBUG(*vcTypeRet = CORINFO_CLASS_HANDLE((size_t)INVALID_POINTER_CC));

    SigPointer ptr((unsigned __int8*) args);
    CorElementType type = ptr.PeekElemType();
    while (type == ELEMENT_TYPE_PINNED) {
        type = ptr.GetElemType();
        ret = CORINFO_TYPE_MOD_PINNED;
    }

    TypeHandle typeHnd = TypeHandle();

    switch (type)
    {
      case ELEMENT_TYPE_VAR :
      case ELEMENT_TYPE_MVAR :
      case ELEMENT_TYPE_VALUETYPE :
      case ELEMENT_TYPE_TYPEDBYREF :
      case ELEMENT_TYPE_WITH :
        {
        OBJECTREF Throwable = NULL;
        GCPROTECT_BEGIN(Throwable);
        typeHnd = ptr.GetTypeHandle(GetModule(sig->scope), &Throwable, TRUE, FALSE, FALSE, (TypeHandle*) sig->classInst, (TypeHandle*) sig->methodInst);
        if (typeHnd.IsNull()) {
            if (Throwable == NULL)
                COMPlusThrow(kTypeLoadException);
            else
                COMPlusThrow(Throwable);
        }
        GCPROTECT_END();
        }

        type = typeHnd.GetNormCorElementType();
        break;

    default:
        break;
    }

    ret = CorInfoTypeWithMod(ret | toJitType(type, typeHnd, vcTypeRet));

    COOPERATIVE_TRANSITION_END();
    return(ret);
}

/*********************************************************************/
CORINFO_CLASS_HANDLE __stdcall CEEInfo::getArgClass (
        CORINFO_SIG_INFO*       sig,
    CORINFO_ARG_LIST_HANDLE    args
    )
{
    REQUIRES_16K_STACK;
    CORINFO_CLASS_HANDLE ret = NULL;

    COOPERATIVE_TRANSITION_BEGIN();
    THROWSCOMPLUSEXCEPTION();


    OBJECTREF throwable = NULL;
    GCPROTECT_BEGIN(throwable);

    // make certain we dont have a completely wacked out sig pointer
    _ASSERTE((BYTE*) sig->sig <= (BYTE*) sig->args);
    _ASSERTE((BYTE*) sig->args <= (BYTE*) args && (BYTE*) args < &((BYTE*) sig->args)[0x10000*5]);

    Module* module = GetModule(sig->scope);
    SigPointer ptr((unsigned __int8*) args);

    CorElementType type;
    for(;;) {
        type = ptr.PeekElemType();
        if (type != ELEMENT_TYPE_PINNED)
            break;
        ptr.GetElemType();
    }

    if (!CorTypeInfo::IsPrimitiveType(type)) {
        ret = CORINFO_CLASS_HANDLE(ptr.GetTypeHandle(module, &throwable, FALSE, FALSE, FALSE, (TypeHandle*) sig->classInst,
          (TypeHandle*) sig->methodInst).AsPtr());

        if (!ret) {
            // If don't have a throwable, find out who didn't create one,
            // and fix it.
            _ASSERTE(throwable!=NULL);
            COMPlusThrow(throwable);
        }
    }

    GCPROTECT_END();
    COOPERATIVE_TRANSITION_END();
    return ret;
}

/*********************************************************************/

    // return the unmanaged calling convention for a PInvoke
CorInfoUnmanagedCallConv __stdcall CEEInfo::getUnmanagedCallConv(CORINFO_METHOD_HANDLE method)
{
    CorInfoUnmanagedCallConv conv = CORINFO_UNMANAGED_CALLCONV_UNKNOWN;
    BEGIN_REQUIRES_4K_STACK;

    BEGINCANNOTTHROWCOMPLUSEXCEPTION();

    MethodDesc* ftn = (MethodDesc*) method;
    _ASSERTE(ftn->IsNDirect());

    CorPinvokeMap            pmconv;

    COMPLUS_TRY
    {
        CorNativeLinkType linkType;
        CorNativeLinkFlags linkFlags;
        LPCUTF8 pLibName = NULL;
        LPCUTF8 pEntrypointName = NULL;

        CalculatePinvokeMapInfo(ftn,
                                &linkType,
                                &linkFlags,
                                &pmconv,
                                &pLibName,
                                &pEntrypointName);
        // We've already asserts that this ftn is an NDirect so the above call
        // must either succeed or throw.

        switch (pmconv) {
            case pmCallConvStdcall:
                conv = CORINFO_UNMANAGED_CALLCONV_STDCALL;
                break;
            case pmCallConvCdecl:
                conv = CORINFO_UNMANAGED_CALLCONV_C;
                break;
            default:
                conv = CORINFO_UNMANAGED_CALLCONV_UNKNOWN;
        }



    }
    COMPLUS_CATCH
    {
        conv = CORINFO_UNMANAGED_CALLCONV_UNKNOWN;
    }
    COMPLUS_END_CATCH

    ENDCANNOTTHROWCOMPLUSEXCEPTION();


    END_CHECK_STACK;
    return conv;
}

/*********************************************************************/
BOOL __stdcall CEEInfo::pInvokeMarshalingRequired(CORINFO_METHOD_HANDLE method, CORINFO_SIG_INFO* callSiteSig)
{
    REQUIRES_8K_STACK;
    BOOL ret;
    COOPERATIVE_TRANSITION_BEGIN();
    START_NON_JIT_PERF();

    if (method != 0) 
    {
        MethodDesc* ftn = (MethodDesc*) method;
        _ASSERTE(ftn->IsNDirect());
        NDirectMethodDesc *pMD = (NDirectMethodDesc*)ftn;

        NDirectMethodDesc::MarshCategory marshCatSnap = pMD->GetMarshCategory(); 
        if (marshCatSnap != pMD->kUnknown)
        {
            ret = marshCatSnap != pMD->kNoMarsh;
        }
        else
        {
            CorNativeLinkType type;
            CorNativeLinkFlags flags;
            LPCUTF8 szLibName = NULL;
            LPCUTF8 szEntrypointName = NULL;
            Stub  *pTempMLStub = NULL;
            CorPinvokeMap unmgdCallConv;

            CalculatePinvokeMapInfo(pMD, &type, &flags, &unmgdCallConv, &szLibName, &szEntrypointName);

            PCCOR_SIGNATURE pMetaSig;
            DWORD       cbMetaSig;
            pMD->GetSig(&pMetaSig, &cbMetaSig);

            NDirectMethodDesc::MarshCategory marshCat;
            OBJECTREF throwable = NULL;

            GCPROTECT_BEGIN(throwable);
            pTempMLStub = CreateNDirectMLStub(pMetaSig, pMD->GetModule(), pMD->GetMemberDef(), type, flags, unmgdCallConv, &throwable, FALSE
#ifdef CUSTOMER_CHECKED_BUILD
                                              ,pMD
#endif
                                              );
            GCPROTECT_END();

            if (!pTempMLStub)
            {
                marshCat = pMD->kYesMarsh;
            }
            else
            {
                pMD->ndirect.m_cbDstBufSize = ((MLHeader*)(pTempMLStub->GetEntryPoint()))->m_cbDstBuffer;

                MLHeader *header = (MLHeader*)(pTempMLStub->GetEntryPoint());

                marshCat = pMD->kNoMarsh;

                if (header->m_Flags != 0)
                {
                    // Has a floating point result
                    marshCat = pMD->kYesMarsh;
                }
                else
                {
                    const MLCode *pMLCode = header->GetMLCode();
                    MLCode mlcode;
                    while (ML_END != (mlcode = *(pMLCode++)))
                    {
                        if (!(mlcode == ML_COPY4 || mlcode == ML_COPYI4 || mlcode == ML_COPYU4 || mlcode == ML_INTERRUPT))
                        {
                            if (mlcode == ML_BUMPSRC)
                            {
                                pMLCode += 2;
                            }
                            else
                            {
                                marshCat = pMD->kYesMarsh;
                                break;
                            }
                        }
                    }
                }

                pTempMLStub->DecRef();

            }
            _ASSERTE(marshCat != pMD->kUnknown);
            pMD->ProbabilisticallyUpdateMarshCategory(marshCat);
            ret = marshCat != pMD->kNoMarsh;

                // make certain call site signature does not require marshelling
            if (!ret && *pMetaSig == IMAGE_CEE_CS_CALLCONV_VARARG)
                goto CHECK_SIG;
        }
    }
    else {
    CHECK_SIG:
            // Check to make certain that the signature only contains types that marshel trivially
        ret = FALSE;

        SigPointer ptr((PCCOR_SIGNATURE) callSiteSig->sig);
        ptr.GetCallingConvInfo();
        unsigned numArgs = ptr.GetData() + 1;   // +1 for return type
        do {
            SigPointer arg = ptr;
            CorElementType type = arg.GetElemType();
            if (type == ELEMENT_TYPE_PTR) 
            {
                if (arg.HasCustomModifier((Module*) callSiteSig->scope, "Microsoft.VisualC.NeedsCopyConstructorModifier", ELEMENT_TYPE_CMOD_REQD))
                {
                    ret = TRUE;
                    break;
                }
            } 
            else if (!CorTypeInfo::IsPrimitiveType(type))
            {
                ret = TRUE;
                break;
            }
           ptr.Skip();
            --numArgs;
        } while (numArgs != 0);
    }

    STOP_NON_JIT_PERF();

    COOPERATIVE_TRANSITION_END();
    return ret;
}

// Generate a cookie based on the signature that would needs to be passed
// to CORINFO_HELP_PINVOKE_CALLI
LPVOID CEEInfo::GetCookieForPInvokeCalliSig(CORINFO_SIG_INFO* szMetaSig,
                                            void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    return getVarArgsHandle(szMetaSig, ppIndirection);
}

/*********************************************************************/
// checks if two method sigs are compatible.
BOOL __stdcall CEEInfo::compatibleMethodSig(
            CORINFO_METHOD_HANDLE        child, 
            CORINFO_METHOD_HANDLE        parent)
{
    return CompatibleMethodSig((MethodDesc*)child, (MethodDesc*)parent);
}

/*********************************************************************/
// Check Visibility rules.
// For Protected (family access) members, type of the instance is also
// considered when checking visibility rules.
BOOL __stdcall CEEInfo::canAccessMethod(
        CORINFO_METHOD_HANDLE       context,
        CORINFO_METHOD_HANDLE       target,
        CORINFO_CLASS_HANDLE        instance) 
{
    return ClassLoader::CanAccess(
            ((MethodDesc*)context)->GetClass(),
            ((MethodDesc*)context)->GetModule()->GetAssembly(),
            ((MethodDesc*)target)->GetClass(),
            ((MethodDesc*)target)->GetModule()->GetAssembly(),
            TypeHandle(instance).GetClass(),
            ((MethodDesc*)target)->GetAttrs());
}

/*********************************************************************/
// Given an object type, method ptr, and Delegate ctor, check if
// the object and method signature are Compatible with the Invoke method
// of the delegate.
//
// objCls should be NULL if the target object is NULL
//@GENERIVSVER: deprecated
BOOL __stdcall CEEInfo::isCompatibleDelegate(
            CORINFO_CLASS_HANDLE        objCls,
            CORINFO_METHOD_HANDLE       method,
            CORINFO_METHOD_HANDLE       delegateCtor)
{
    _ASSERTE(method != NULL);
    _ASSERTE(delegateCtor != NULL);

    BOOL result = FALSE;

    BEGIN_REQUIRES_4K_STACK;

    BEGINCANNOTTHROWCOMPLUSEXCEPTION();

    EEClass* pClsDelegate = ((MethodDesc*)delegateCtor)->GetClass();

#ifdef _DEBUG
    _ASSERTE(pClsDelegate->IsAnyDelegateClass());
#endif

    MethodDesc* pMDFtn = (MethodDesc*) method;
    TypeHandle inst(objCls);

    COMPLUS_TRY
    {
        result = COMDelegate::ValidateCtor(pMDFtn, pClsDelegate, 
                inst.IsNull() ? NULL : inst.GetClass());
    }
    COMPLUS_CATCH
    {
    }
    COMPLUS_END_CATCH

    ENDCANNOTTHROWCOMPLUSEXCEPTION();

    END_CHECK_STACK;

    return result;
}


/*********************************************************************/
// Given an object type, method parent, method ptr, and Delegate ctor, check if
// the object and method signature are Compatible with the Invoke method
// of the delegate.
//
// objCls should be NULL if the target object is NULL
//@GENERICSVER: new
BOOL __stdcall CEEInfo::isCompatibleDelegate(
            CORINFO_CLASS_HANDLE        objCls,
            CORINFO_CLASS_HANDLE        methodParentCls,
            CORINFO_METHOD_HANDLE       method,
            CORINFO_CLASS_HANDLE        delegateCls,
            CORINFO_MODULE_HANDLE       moduleHnd,
            unsigned        methodMemberRef,
            unsigned        delegateConstructorMemberRef)
{
    _ASSERTE(method != NULL);
    _ASSERTE(delegateCls != NULL);

    BOOL result = FALSE;

    BEGIN_REQUIRES_4K_STACK;

    BEGINCANNOTTHROWCOMPLUSEXCEPTION();

    TypeHandle delegateClsHnd = (TypeHandle) delegateCls;

#ifdef _DEBUG
    _ASSERTE(delegateClsHnd.GetClass()->IsAnyDelegateClass());
#endif

    TypeHandle methodParentHnd = (TypeHandle) (methodParentCls);
    MethodDesc* pMDFtn = (MethodDesc*) method;
    TypeHandle objClsHnd(objCls);
    Module* module = GetModule(moduleHnd);

    COMPLUS_TRY
    {
      result = COMDelegate::ValidateCtor(
          objClsHnd,
          methodParentHnd,
          pMDFtn,
          delegateClsHnd,
          module, 
          methodMemberRef, 
          delegateConstructorMemberRef);
    }
    COMPLUS_CATCH
    {
    }
    COMPLUS_END_CATCH

    ENDCANNOTTHROWCOMPLUSEXCEPTION();

    END_CHECK_STACK;

    return result;
}

/*********************************************************************/
    // return the unmanaged target *if method has already been prelinked.*
void* __stdcall CEEInfo::getPInvokeUnmanagedTarget(CORINFO_METHOD_HANDLE method,
                                                    void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    MethodDesc* ftn = (MethodDesc*) method;
    _ASSERTE(ftn->IsNDirect());
    NDirectMethodDesc *pMD = (NDirectMethodDesc*)ftn;

    LPVOID pTarget = pMD->ndirect.m_pNDirectTarget;
    if (pTarget == pMD->ndirect.m_ImportThunkGlue)
    {
        return NULL;
    }
    else
    {
        return pTarget;
    }
}

/*********************************************************************/
    // return address of fixup area for late-bound N/Direct calls.
void* __stdcall CEEInfo::getAddressOfPInvokeFixup(CORINFO_METHOD_HANDLE method,
                                                   void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    MethodDesc* ftn = (MethodDesc*) method;
    _ASSERTE(ftn->IsNDirect());
    NDirectMethodDesc *pMD = (NDirectMethodDesc*)ftn;

    return (LPVOID)&(pMD->ndirect.m_pNDirectTarget);
}


/*********************************************************************/
    // Gets a method handle that can be used to correlate profiling data.
    // This is the IP of a native method, or the address of the descriptor struct
    // for IL.  Always guaranteed to be unique per process, and not to move. */
CORINFO_PROFILING_HANDLE __stdcall CEEInfo::GetProfilingHandle(CORINFO_METHOD_HANDLE method,
                                                               BOOL *pbHookFunction,
                                                               void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    MethodDesc* ftn = (MethodDesc*) method;
    CORINFO_PROFILING_HANDLE    handle = 0;

    extern FunctionIDMapper *g_pFuncIDMapper;
    handle = (CORINFO_PROFILING_HANDLE) g_pFuncIDMapper((FunctionID) ftn, pbHookFunction);

    return (handle);
}



/*********************************************************************/
    // Return details about EE internal data structures
void __stdcall CEEInfo::getEEInfo(CORINFO_EE_INFO *pEEInfoOut)
{
    REQUIRES_4K_STACK;
    COOPERATIVE_TRANSITION_BEGIN();

    START_NON_JIT_PERF();

    ZeroMemory(pEEInfoOut, sizeof(CORINFO_EE_INFO));


    // Offsets into the method table.
    pEEInfoOut->offsetOfInterfaceTable = offsetof(MethodTable, m_pInterfaceVTableMap);

    // Delegate offsets
    pEEInfoOut->offsetOfDelegateInstance    = COMDelegate::GetOR()->GetOffset()        + sizeof(Object);
    pEEInfoOut->offsetOfDelegateFirstTarget = COMDelegate::GetMethodPtr()->GetOffset() + sizeof(Object);

    // Remoting offsets
    pEEInfoOut->offsetOfTransparentProxyRP = g_Mscorlib.GetFieldOffset(FIELD__TRANSPARENT_PROXY__RP);
    pEEInfoOut->offsetOfRealProxyServer    = g_Mscorlib.GetFieldOffset(FIELD__REAL_PROXY__SERVER);


    pEEInfoOut->osType  = CORINFO_PAL;
    pEEInfoOut->noDirectTLS = 1;


    STOP_NON_JIT_PERF();

    COOPERATIVE_TRANSITION_END();
}

    // Return details about EE internal data structures
DWORD __stdcall CEEInfo::getThreadTLSIndex(void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    return GetThreadTLSIndex();
}

const void * __stdcall CEEInfo::getInlinedCallFrameVptr(void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    return NULL;
}

LONG * __stdcall CEEInfo::getAddrOfCaptureThreadGlobal(void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    return &g_TrapReturningThreads;
}

HRESULT __stdcall CEEInfo::GetErrorHRESULT()
{
    REQUIRES_4K_STACK;
    HRESULT hr;

    CANNOTTHROWCOMPLUSEXCEPTION();

    Thread *pCurThread = GetThread();
    BOOL toggleGC = !pCurThread->PreemptiveGCDisabled();

    if (toggleGC)
        pCurThread->DisablePreemptiveGC();

    OBJECTREF throwable = GetThread()->LastThrownObject();

    if (throwable == NULL)
        hr = S_OK;
    else
        hr = SecurityHelper::MapToHR(throwable);

    if (toggleGC)
        pCurThread->EnablePreemptiveGC();

    return hr;
}

CORINFO_CLASS_HANDLE __stdcall CEEInfo::GetErrorClass()
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    OBJECTREF throwable = GetThread()->LastThrownObject();

    if (throwable == NULL)
        return NULL;

    return (CORINFO_CLASS_HANDLE) TypeHandle(throwable->GetMethodTable()).AsPtr();
}



ULONG __stdcall CEEInfo::GetErrorMessage(LPWSTR buffer, ULONG bufferLength)
{
    ULONG result = 0;
    BEGIN_REQUIRES_4K_STACK;

    BEGINCANNOTTHROWCOMPLUSEXCEPTION();

    OBJECTREF throwable = GetThread()->LastThrownObject();

    if (throwable != NULL)
    {
        COMPLUS_TRY
          {
              result = GetExceptionMessage(throwable, buffer, bufferLength);
          }
        COMPLUS_CATCH
          {
          }
        COMPLUS_END_CATCH
    }

    ENDCANNOTTHROWCOMPLUSEXCEPTION();


    END_CHECK_STACK;
    return result;
}

int __stdcall CEEInfo::FilterException(struct _EXCEPTION_POINTERS *pExceptionPointers) {

    REQUIRES_4K_STACK;
    unsigned code = pExceptionPointers->ExceptionRecord->ExceptionCode;


#ifdef _DEBUG
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        static int hit = 0;
        if (hit++ == 0) {
            _ASSERTE(!"Access violation while Jitting!");
        }
        return(EXCEPTION_CONTINUE_SEARCH);
    }
#endif
    // No one should be catching breakpoint
    if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP)
        return(EXCEPTION_CONTINUE_SEARCH);

    if (code != EXCEPTION_COMPLUS)
        return(EXCEPTION_EXECUTE_HANDLER);

    // Don't catch ThreadStop exceptions

    int result;
    Thread *pCurThread = GetThread();
    BOOL toggleGC = !pCurThread->PreemptiveGCDisabled();

    if (toggleGC)
        pCurThread->DisablePreemptiveGC();

    OBJECTREF throwable = GetThread()->LastThrownObject();
    _ASSERTE(throwable != 0);

    GCPROTECT_BEGIN(throwable);

    if (IsUncatchable(&throwable))
        result = EXCEPTION_CONTINUE_SEARCH;
    else
        result = EXCEPTION_EXECUTE_HANDLER;

    GCPROTECT_END();

    if (toggleGC)
        pCurThread->EnablePreemptiveGC();

    return result;
}

CORINFO_MODULE_HANDLE __stdcall CEEInfo::embedModuleHandle(CORINFO_MODULE_HANDLE handle,
                                                             void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    return handle;
}

CORINFO_CLASS_HANDLE __stdcall CEEInfo::embedClassHandle(CORINFO_CLASS_HANDLE handle,
                                                           void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    return handle;
}

CORINFO_FIELD_HANDLE __stdcall CEEInfo::embedFieldHandle(CORINFO_FIELD_HANDLE handle,
                                                           void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    return handle;
}

CORINFO_METHOD_HANDLE __stdcall CEEInfo::embedMethodHandle(CORINFO_METHOD_HANDLE handle,
                                                             void **ppIndirection)
{
    REQUIRES_4K_STACK;
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    return handle;
}

/*********************************************************************/
HCIMPL2(CORINFO_MethodPtr, JIT_EnCResolveVirtual, void * obj, CORINFO_METHOD_HANDLE method)
{
    return NULL;
}
HCIMPLEND


/*********************************************************************/

//@GENERICS:
// Resolve a generic virtual method at run-time
// classHnd is the actual run-time type of the object on which the call is made
// methodHnd is the exact instantiated method descriptor corresponding to the static method signature (i.e. might be for a superclass of classHnd)
HCIMPL2(CORINFO_MethodPtr, JIT_GenericVirtual, CORINFO_METHOD_HANDLE methodHnd, CORINFO_CLASS_HANDLE classHnd)
{
  // The address of the method that's returned.
  CORINFO_MethodPtr   addr = NULL;

  HELPER_METHOD_FRAME_BEGIN_RET_NOPOLL();    // Set up a frame
  
  // Method table of target (might be instantiated)
  MethodTable *pTargetMT = TypeHandle(classHnd).GetMethodTable();

  // This is the method descriptor from which we obtain generic descriptor
  // and method instantiation 
  InstantiatedMethodDesc* pMD = (InstantiatedMethodDesc*) methodHnd;

  // Generic method descriptor
  MethodDesc* pGenericMD = pMD->StripMethodInstantiation();

  MethodDesc *pTargetGenericMD;

  if (pMD->IsInterface())
  {
    // It's an interface call
    pTargetGenericMD = pTargetMT->GetMethodDescForInterfaceMethod(pGenericMD);
  }
  else
  {
    // It's an ordinary virtual call
    pTargetGenericMD = pTargetMT->GetMethodDescForSlot(pGenericMD->GetSlot());   
  }

  // The method definition may be higher up the inheritance hierarchy.
  // Make sure we add the instantiation to the correct class.
  pTargetMT = pTargetGenericMD->GetMethodTable();

  MethodDesc *pResultMD = InstantiatedMethodDesc::CreateGenericInstantiation(pTargetMT, pTargetGenericMD, pMD->GetMethodInstantiation());
  _ASSERTE(pResultMD->HasMethodInstantiation());

  addr = (CORINFO_MethodPtr) pResultMD->GetUnsafeAddrofCode();
  _ASSERTE(addr);

  HELPER_METHOD_FRAME_END();
  return addr;
}
HCIMPLEND

/*********************************************************************/
// Returns the address of the field in the object (This is an interior
// pointer and the caller has to use it appropriately). obj can be
// either a reference or a byref
HCIMPL2(void*, JIT_GetFieldAddr, Object *obj, FieldDesc* pFD)
{
	if (GetThread()->CatchAtSafePoint())  {
		HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_1(Frame::FRAME_ATTR_RETURN_INTERIOR, obj);
		CommonTripThread(); 
		HELPER_METHOD_FRAME_END();
	}

    void * fldAddr = NULL;
    _ASSERTE(pFD->GetMethodTableOfEnclosingClass()->GetClass()->GetMethodTable() == pFD->GetMethodTableOfEnclosingClass());

    // If obj is a byref, it will never be null.
    if (obj == NULL)
        FCThrow(kNullReferenceException);

    OBJECTREF objRef = ObjectToOBJECTREF(obj);
    if(objRef->GetMethodTable()->IsTransparentProxyType())
    {
        objRef = CRemotingServices::GetObjectFromProxy(objRef, FALSE);
        if (objRef->GetMethodTable()->IsTransparentProxyType())
            FCThrow(kInvalidOperationException);
    }

    {
        fldAddr = pFD->GetAddress(OBJECTREFToObject(objRef));
        _ASSERTE(objRef->GetMethodTable()->IsMarshaledByRef() || pFD->IsDangerousAppDomainAgileField());
    }

    return fldAddr;
}
HCIMPLEND







/*********************************************************************/

int CEEJitInfo::doAssert(const char* szFile, int iLine, const char* szExpr)
{
#ifdef _DEBUG
    return(_DbgBreakCheck(szFile, iLine, szExpr));
#else
    return(true);   // break into debugger
#endif
}

BOOL __cdecl CEEJitInfo::logMsg(unsigned level, const char* fmt, va_list args) {
#ifdef LOGGING
    CANNOTTHROWCOMPLUSEXCEPTION();

    _ASSERTE(GetThread()->PreemptiveGCDisabled());  // can be used with FJIT codeLog to quickly locallize the problem

    if (LoggingOn(LF_JIT, level)) 
    {
        LogSpewValist(LF_JIT, level, (char*) fmt, args);
        return(true);
    }
#endif // LOGGING
    return false;
}

static OBJECTHANDLE __stdcall ConstructStringLiteral(CORINFO_MODULE_HANDLE scopeHnd, mdToken metaTok)
{
    THROWSCOMPLUSEXCEPTION();

    _ASSERTE(TypeFromToken(metaTok) == mdtString);

    Module* module = GetModule(scopeHnd);

    return module->ResolveStringRef(metaTok, module->GetAssembly()->Parent());
}

LPVOID __stdcall CEEInfo::constructStringLiteral(CORINFO_MODULE_HANDLE scopeHnd,
                                                 mdToken metaTok,
                                                 void **ppIndirection)
{
    REQUIRES_4K_STACK;
    LPVOID result;

    COOPERATIVE_TRANSITION_BEGIN();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    result = (LPVOID)ConstructStringLiteral(scopeHnd, metaTok); // throws

    COOPERATIVE_TRANSITION_END();
    return result;
}


/*********************************************************************/

HRESULT __stdcall CEEJitInfo::allocMem (
            ULONG               codeSize,
            ULONG               roDataSize,
            ULONG               rwDataSize,
            void **             codeBlock,
            void **             roDataBlock,
            void **             rwDataBlock
            )
{

    size_t  roDataExtra = 0;
    size_t  rwDataExtra = 0;

     CANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();

#ifdef _X86_

    // As per Intel Optimization Manual the cache line size is 32 bytes
#define CACHE_LINE_SIZE 32
#endif

    if (roDataSize > 0)
    {
        // Make sure that the codeSize a multiple of 4
        // This will insure that the first byte of
        // the roDataSize will be 4 byte aligned.
        codeSize = (ULONG) ALIGN_UP(codeSize, CODE_SIZE_ALIGN);

        if (roDataSize >= 8)
        {
            // allocates an extra 4 bytes so that we can
            // double word align the roData section.
            roDataExtra = 4;
        }
    }
    if (rwDataSize > 0)
    {
        // Make sure that the codeSize a multiple of 4
        codeSize   = (ULONG) ALIGN_UP(codeSize, CODE_SIZE_ALIGN);
        // Make sure that the roDataSize a multiple of 4
        roDataSize = (ULONG) ALIGN_UP(roDataSize, 4);
        // Make sure that the rwDataSize a full cache line
        rwDataSize = (ULONG) ALIGN_UP(rwDataSize, CACHE_LINE_SIZE);

        // We also need to make sure that the rwData section
        // starts on a new cache line
        rwDataExtra = (CACHE_LINE_SIZE - 4);
    }

    size_t totalSize = codeSize + roDataSize + roDataExtra
                                + rwDataSize + rwDataExtra;


    _ASSERTE(m_CodeHeader == 0);
    m_CodeHeader = m_jitManager->allocCode(m_FD,totalSize);

    if (!m_CodeHeader)
    {
        STOP_NON_JIT_PERF();
        return(E_FAIL);
    }

    BYTE* start = ((EEJitManager*)m_jitManager)->JitToken2StartAddress((METHODTOKEN)m_CodeHeader);
    BYTE* current = start;

    *codeBlock = start;
    current += codeSize;


    /* Do we need to 8-byte align the roData section? */
    if (roDataSize >= 8)
    {
        _ASSERTE( ALIGN_UP((size_t) current, 8) - (size_t) current <= roDataExtra );
        current = (BYTE*) ALIGN_UP((size_t) current, 8);
    }

    *roDataBlock = current;
    current += roDataSize;

    /* Do we need to cache line align the rwData section? */
    if (rwDataSize > 0)
    {
        _ASSERTE( ALIGN_UP((size_t) current, CACHE_LINE_SIZE) - (size_t) current <= rwDataExtra );
        current = (BYTE*) ALIGN_UP((size_t) current, CACHE_LINE_SIZE);
    }

    *rwDataBlock = current;
    current += rwDataSize;

    _ASSERTE(((size_t) (current - start)) <= totalSize);

    STOP_NON_JIT_PERF();
    return(S_OK);
}

/*********************************************************************/
HRESULT __stdcall CEEJitInfo::allocGCInfo (ULONG size, void ** block)
{
    CANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();

    _ASSERTE(m_CodeHeader != 0);
    _ASSERTE(m_CodeHeader->phdrJitGCInfo == 0);
    *block = m_jitManager->allocGCInfo(m_CodeHeader,(DWORD)size);
    if (!*block)
    {
        STOP_NON_JIT_PERF();
        return(E_FAIL);
    }

    _ASSERTE(m_CodeHeader->phdrJitGCInfo != 0 && *block == m_CodeHeader->phdrJitGCInfo);

    STOP_NON_JIT_PERF();
    return S_OK;
}

/*********************************************************************/
HRESULT __stdcall CEEJitInfo::setEHcount (
        unsigned      cEH)
{
    CANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();

    _ASSERTE(cEH != 0);
    _ASSERTE(m_CodeHeader != 0);
    _ASSERTE(m_CodeHeader->phdrJitEHInfo == 0);
    EE_ILEXCEPTION* ret = m_jitManager->allocEHInfo(m_CodeHeader,cEH);
    if (!ret)
    {
        STOP_NON_JIT_PERF();
        return(E_FAIL);
    }
    _ASSERTE(m_CodeHeader->phdrJitEHInfo != 0 && m_CodeHeader->phdrJitEHInfo->EHCount() == cEH);

    STOP_NON_JIT_PERF();
    return(S_OK);
}

/*********************************************************************/
void __stdcall CEEJitInfo::setEHinfo (
        unsigned      EHnumber,
        const CORINFO_EH_CLAUSE* clause)
{
    CANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();
    _ASSERTE(m_CodeHeader->phdrJitEHInfo != 0 && EHnumber < m_CodeHeader->phdrJitEHInfo->EHCount());

    EE_ILEXCEPTION_CLAUSE* pEHClause = m_CodeHeader->phdrJitEHInfo->EHClause(EHnumber);

    pEHClause->TryEndPC       = clause->TryLength;
    pEHClause->HandlerEndPC   = clause->HandlerLength;
    pEHClause->TryStartPC     = clause->TryOffset;
    pEHClause->HandlerStartPC = clause->HandlerOffset;
    pEHClause->ClassToken     = clause->ClassToken;
    pEHClause->Flags          = (CorExceptionFlag)clause->Flags;

    LOG((LF_EH, LL_INFO100000, "Setting EH clause #%d for %s::%s\n", EHnumber, m_FD->m_pszDebugClassName, m_FD->m_pszDebugMethodName));
    LOG((LF_EH, LL_INFO100000, "    Flags         : 0x%08lx  ->  0x%08lx\n",            clause->Flags,         pEHClause->Flags));
    LOG((LF_EH, LL_INFO100000, "    TryOffset     : 0x%08lx  ->  0x%08lx (startpc)\n",  clause->TryOffset,     pEHClause->TryStartPC));
    LOG((LF_EH, LL_INFO100000, "    TryLength     : 0x%08lx  ->  0x%08lx (endpc)\n",    clause->TryLength,     pEHClause->TryEndPC));
    LOG((LF_EH, LL_INFO100000, "    HandlerOffset : 0x%08lx  ->  0x%08lx\n",            clause->HandlerOffset, pEHClause->HandlerStartPC));
    LOG((LF_EH, LL_INFO100000, "    HandlerLength : 0x%08lx  ->  0x%08lx\n",            clause->HandlerLength, pEHClause->HandlerEndPC));
    LOG((LF_EH, LL_INFO100000, "    ClassToken    : 0x%08lx  ->  0x%08lx\n",            clause->ClassToken,    pEHClause->exnType));
    LOG((LF_EH, LL_INFO100000, "    FilterOffset  : 0x%08lx  ->  0x%08lx\n",            clause->FilterOffset,  pEHClause->FilterOffset));


    STOP_NON_JIT_PERF();
}

/*********************************************************************/
// get individual exception handler
void __stdcall CEEJitInfo::getEHinfo(
            CORINFO_METHOD_HANDLE ftn,                      /* IN  */
            unsigned      EHnumber,                 /* IN */
            CORINFO_EH_CLAUSE* clause)                  /* OUT */
{
    CANNOTTHROWCOMPLUSEXCEPTION();

    START_NON_JIT_PERF();

    _ASSERTE(ftn == CORINFO_METHOD_HANDLE(m_FD));  // For now only support if the method being jitted
    _ASSERTE(m_ILHeader->EH);
    _ASSERTE(EHnumber < m_ILHeader->EH->EHCount());

    COR_ILMETHOD_SECT_EH_CLAUSE_FAT ehClause;

    const COR_ILMETHOD_SECT_EH_CLAUSE_FAT* ehInfo;
    ehInfo = m_ILHeader->EH->EHClause(EHnumber, &ehClause);
    clause->Flags = (CORINFO_EH_CLAUSE_FLAGS)ehInfo->GetFlags();
    clause->TryOffset = ehInfo->GetTryOffset();
    clause->TryLength = ehInfo->GetTryLength();
    clause->HandlerOffset = ehInfo->GetHandlerOffset();
    clause->HandlerLength = ehInfo->GetHandlerLength();
    clause->ClassToken = ehInfo->GetClassToken();
    clause->FilterOffset = ehInfo->GetFilterOffset();

    STOP_NON_JIT_PERF();
}

CorInfoHelpFunc CEEJitInfo::getNewHelper (CORINFO_CLASS_HANDLE newClsHnd, CORINFO_METHOD_HANDLE context)
{
    THROWSCOMPLUSEXCEPTION();     

    return CEEInfo::getNewHelper (newClsHnd, context == NULL ? CORINFO_METHOD_HANDLE(m_FD) : context);
}

ICorJitInfo* CEEJitInfo::getNewFuncletInfo()
{
    CEEJitInfo* pInfoRet;
    
    pInfoRet = new CEEJitInfo(m_FD, NULL, m_jitManager, m_isStub);

    return pInfoRet;
}

void CEEJitInfo::releaseFuncletInfo(ICorJitInfo* pInfo)
{
    delete pInfo;
}




extern int DumpCurrentStack();

/*********************************************************************/

 
CorJitResult CallCompileMethodWithSEHWrapper(IJitManager *jitMgr,
                                ICorJitInfo *comp,
                                struct CORINFO_METHOD_INFO *info,
                                unsigned flags,
                                BYTE **nativeEntry,
                                ULONG *nativeSizeOfCode,
                                bool debuggerTrackJITInfo,
                                MethodDesc *ftn)
{
    CorJitResult res = CORJIT_INTERNALERROR;

    // debuggerTrackJITInfo is only to be used to determine whether or not
    // to communicate with the debugger, NOT with how to generate code - use
    // flags for code gen, instead.

    PAL_TRY
    {
#ifdef DEBUGGING_SUPPORTED
        if (debuggerTrackJITInfo || CORDebuggerAttached())
            g_pDebugInterface->JITBeginning(ftn, debuggerTrackJITInfo);
#endif

        res = jitMgr->m_jit->compileMethod(comp, info, flags,
            nativeEntry, nativeSizeOfCode);


    }
    PAL_FINALLY
    {
#ifdef DEBUGGING_SUPPORTED
        if (res == CORJIT_OK)
        {
            // Notify the debugger that we have successfully jitted the function

            if (debuggerTrackJITInfo || CORDebuggerAttached())
                g_pDebugInterface->JITComplete(ftn, 
                                               jitMgr->GetNativeEntry(*nativeEntry), 
                                               *nativeSizeOfCode,
                                               debuggerTrackJITInfo);
            if (CORDebuggerAttached())
                g_pDebugInterface->FunctionStubInitialized(ftn,
                                                           jitMgr->GetNativeEntry(*nativeEntry));
        }
        else if (ftn->IsJitted())
        {
            // This is the case where we aborted the jit because of a deadlock cycle
            // in initClass.  Nothing to do here (don't need to notify the debugger
            // because the function has already been successfully jitted)
        }
        else
        {
            if (debuggerTrackJITInfo)
            {
                LOG((LF_CORDB,LL_INFO10000, "FUDW: compileMethod threw an exception, and"
                     " FilterUndoDebuggerWork is backing out the DebuggerJitInfo! ("
                     "JITInterface.cpp"));
                g_pDebugInterface->JITComplete(ftn, 0, 0, debuggerTrackJITInfo);
            }
        }
#endif // DEBUGGING_SUPPORTED
    }
    PAL_ENDTRY

    return res;
}


/*********************************************************************/


CorInfoTypeWithMod __stdcall CEEJitInfo::getArgType(
            CORINFO_SIG_INFO*           sig,
            CORINFO_ARG_LIST_HANDLE     args,
            CORINFO_CLASS_HANDLE*       vcTypeRet)
{
    CorInfoTypeWithMod type;
    
    type = CEEInfo::getArgType(sig, args, vcTypeRet);

    if (m_isStub)
    {
        CorInfoType         typeBase    = strip(type);
        int                 fPinned     = type & CORINFO_TYPE_MOD_PINNED;

        if (typeBase == CORINFO_TYPE_CLASS)
        {
            type = (CorInfoTypeWithMod)(CORINFO_TYPE_PTR | fPinned);
        }
    }

    return type;
}


CORINFO_METHOD_HANDLE __stdcall CEEJitInfo::findMethod(CORINFO_MODULE_HANDLE  scopeHnd, unsigned metaTOK, CORINFO_METHOD_HANDLE context)
{
    CORINFO_METHOD_HANDLE methHnd = NULL;

    if (m_isStub)
    {
        methHnd = context;
    }
    else
    {
        methHnd = CEEInfo::findMethod(scopeHnd, metaTOK, context);
    }
    return methHnd;
}


/*********************************************************************/

Stub* JITFunction(MethodDesc* ftn, COR_ILMETHOD_DECODER* ILHeader, BOOL *fEJit, DWORD flags)
{
    Stub *ret = 0;

    THROWSCOMPLUSEXCEPTION();

    REQUIRES_16K_STACK;

    COOPERATIVE_TRANSITION_BEGIN();

    IJitManager *jitMgr = NULL;

    *fEJit = FALSE;
    // Has the user configured us to not JIT the method?
    if (g_pConfig->ShouldEJitMethod(ftn))
    {
        // For the given flags see if we have a
        jitMgr = ExecutionManager::GetJitForType(miManaged_IL_EJIT);
        if (!jitMgr || !jitMgr->m_jit)
	  COMPlusThrow(kExecutionEngineException); // Don't want to throw InvalidProgram from here
        *fEJit = TRUE;

    }
    else
        COMPlusThrow(kExecutionEngineException); // Don't want to throw InvalidProgram from here

    {


#ifdef _DEBUG
    // This is here so we can see the name and class easily in the debugger

    LPCUTF8 cls  = ftn->GetClass() ? ftn->GetClass()->m_szDebugClassName
                                   : "GlobalFunction";
    LPCUTF8 name = ftn->GetName();


    // Set these up for the LOG() calls
    bool           isOptIl = FALSE;

#ifdef _DEBUG
    char instbuff[200]; // Should be enough for most apps
    Generics::PrettyInstantiation(instbuff, 200, ftn->GetNumGenericMethodArgs(), ftn->GetMethodInstantiation());
#endif

    LOG((LF_JIT, LL_INFO1000, "{ Jitting method (%p) %s::%s%s  %s\n", ftn, cls,name, instbuff, ftn->m_pszDebugMethodSignature));

#endif // _DEBUG

    bool debuggerTrackJITInfo = false;

#ifdef DEBUGGING_SUPPORTED
    DWORD dwDebugBits = ftn->GetModule()->GetDebuggerInfoBits();
    debuggerTrackJITInfo = CORDebuggerTrackJITInfo(dwDebugBits)
#ifdef PROFILING_SUPPORTED
                           || CORProfilerJITMapEnabled()
#endif
                          ;
#endif // DEBUGGING_SUPPORTED

    CORINFO_METHOD_INFO methodInfo;
    methodInfo.ftn = CORINFO_METHOD_HANDLE(ftn);
    methodInfo.scope = GetScopeHandle(ftn);
    methodInfo.ILCode = const_cast<BYTE*>(ILHeader->Code);
    methodInfo.ILCodeSize = ILHeader->GetCodeSize();
    methodInfo.maxStack = ILHeader->GetMaxStack();
    methodInfo.EHcount = ILHeader->EHCount();
    _ASSERTE(CORINFO_OPT_INIT_LOCALS == (CorInfoOptions) CorILMethod_InitLocals);
    methodInfo.options = (CorInfoOptions) ILHeader->GetFlags();

    // If it's generic then we can only enter through an instantiated md (unless we're just verifying it)
    _ASSERTE((flags & CORJIT_FLG_IMPORT_ONLY) != 0 || !ftn->IsGenericMethodDefinition());

    // If it's an instance method then it must not be entered from a generic class
    _ASSERTE((flags & CORJIT_FLG_IMPORT_ONLY) != 0 || ftn->IsStatic() || ftn->GetNumGenericClassArgs() == 0 || ftn->HasClassInstantiation());

    // If it's a static method then it must not be entered through an instantiated type 
    // (in fact, static md's shouldn't be present in vtables of instantiated types)
    _ASSERTE(!ftn->IsStatic() || !ftn->GetClass()->HasInstantiation());

    // fetch the method signature
    HRESULT hr = ConvToJitSig(ftn->GetSig(), GetScopeHandle(ftn), mdTokenNil, &methodInfo.args, (CORINFO_METHOD_HANDLE) ftn, false);
    _ASSERTE(SUCCEEDED(hr));

    //@GENERICS:
    // Shared generic methods and shared methods on generic structs take an extra argument representing their instantiation
    if (ftn->HasInstParam())
      methodInfo.args.callConv = (CorInfoCallConv) (methodInfo.args.callConv | CORINFO_CALLCONV_PARAMTYPE);

            // method attributes and signature are consistant
    _ASSERTE( (IsMdStatic(ftn->GetAttrs()) == 0) == ((methodInfo.args.callConv & CORINFO_CALLCONV_HASTHIS) != 0) );

    // And its local variables
    hr = ConvToJitSig(ILHeader->LocalVarSig, GetScopeHandle(ftn), mdTokenNil, &methodInfo.locals, (CORINFO_METHOD_HANDLE) ftn, true);
    _ASSERTE(SUCCEEDED(hr));

    bool            isStub = ((flags & CORJIT_FLG_IL_STUB) != 0);
    CEEJitInfo      jitInfo(ftn, ILHeader, jitMgr, isStub);
    CEEJitInfo*     pJitInfo = &jitInfo;

    SLOT nativeEntry;
    ULONG sizeOfCode;
    CorJitResult res;

#ifdef DEBUGGING_SUPPORTED
    if (debuggerTrackJITInfo 
#ifdef PROFILING_SUPPORTED
        || CORProfilerDisableOptimizations()
#endif
       )
    {
        flags |= CORJIT_FLG_DEBUG_INFO;

        if (!CORDebuggerAllowJITOpts(dwDebugBits) 
#ifdef PROFILING_SUPPORTED
            || CORProfilerDisableOptimizations()
#endif
            )
            flags |= CORJIT_FLG_DEBUG_OPT;

    }
#endif // DEBUGGING_SUPPORTED

#ifdef PROFILING_SUPPORTED
    if (CORProfilerTrackEnterLeave())
        flags |= CORJIT_FLG_PROF_ENTERLEAVE;

    if (CORProfilerTrackTransitions())
        flags |= CORJIT_FLG_PROF_NO_PINVOKE_INLINE;

    if (CORProfilerInprocEnabled())
        flags |= CORJIT_FLG_PROF_INPROC_ACTIVE;
#endif // PROFILING_SUPPORTED

    /*      
           */

    if (g_pConfig->GenLooseExceptOrder())
        flags |= CORJIT_FLG_LOOSE_EXCEPT_ORDER;

    // Set optimization flags

    unsigned optType = g_pConfig->GenOptimizeType();
    assert(optType <= OPT_RANDOM);

    if (optType == OPT_RANDOM)
        optType = methodInfo.ILCodeSize % OPT_RANDOM;

#ifdef _DEBUG
    if (g_pConfig->GenDebugInfo())
    {
        flags |= CORJIT_FLG_DEBUG_INFO;
        debuggerTrackJITInfo = true;
    }
    
    if (g_pConfig->GenDebuggableCode())
        flags |= CORJIT_FLG_DEBUG_OPT;
#endif

    const static unsigned optTypeFlags[] =
    {
        0,                      // OPT_BLENDED
        CORJIT_FLG_SIZE_OPT,    // OPT_CODE_SIZE
        CORJIT_FLG_SPEED_OPT    // OPT_CODE_SPEED
    };

    assert(optType < OPT_RANDOM);
    assert((sizeof(optTypeFlags)/sizeof(optTypeFlags[0])) == OPT_RANDOM);
    flags |= optTypeFlags[optType];
    flags |= g_pConfig->GetCpuFlag();
    flags |= g_pConfig->GetCpuCapabilities();

#ifdef _DEBUG
    if (g_pConfig->IsJitVerificationDisabled())
        flags |= CORJIT_FLG_SKIP_VERIFICATION;
#endif
    if (Security::LazyCanSkipVerification(ftn->GetModule()))
        flags |= CORJIT_FLG_SKIP_VERIFICATION;


    {
        /* There is a double indirection to call compilemethod  - can we
           improve this with the new structure? */

#if defined(ENABLE_PERF_COUNTERS)
        START_JIT_PERF();
#endif
        StartCAP();

#if defined(ENABLE_PERF_COUNTERS)
        LARGE_INTEGER CycleStart;
        QueryPerformanceCounter (&CycleStart);
#endif // ENABLE_PERF_COUNTERS

        CommonTripThread();         // Indicate we are at a GC safe point

        // Note on debuggerTrackInfo arg: if we're only importing (ie, verifying/
        // checking to make sure we could JIT, but not actually generating code (
        // eg, for inlining), then DON'T TELL THE DEBUGGER about this.
        res = CallCompileMethodWithSEHWrapper(jitMgr, 
                                              pJitInfo, 
                                              &methodInfo, 
                                              flags,
                                              &nativeEntry, 
                                              &sizeOfCode,
                                              (flags & CORJIT_FLG_IMPORT_ONLY)?false:debuggerTrackJITInfo,
                                              (MethodDesc*)ftn);

#if defined(ENABLE_PERF_COUNTERS)
        LARGE_INTEGER CycleStop;
        QueryPerformanceCounter(&CycleStop);

        GetPrivatePerfCounters().m_Jit.timeInJit = (CycleStop.QuadPart - CycleStart.QuadPart);
        GetGlobalPerfCounters().m_Jit.timeInJit = (CycleStop.QuadPart - CycleStart.QuadPart);

        GetPrivatePerfCounters().m_Jit.timeInJitBase = (CycleStop.QuadPart - g_lastTimeInJitCompilation.QuadPart);
        GetGlobalPerfCounters().m_Jit.timeInJitBase = (CycleStop.QuadPart - g_lastTimeInJitCompilation.QuadPart);
        g_lastTimeInJitCompilation = CycleStop;
                
        GetPrivatePerfCounters().m_Jit.cMethodsJitted++;
        GetGlobalPerfCounters().m_Jit.cMethodsJitted++;

        GetPrivatePerfCounters().m_Jit.cbILJitted+=methodInfo.ILCodeSize;
        GetGlobalPerfCounters().m_Jit.cbILJitted+=methodInfo.ILCodeSize;
#endif // ENABLE_PERF_COUNTERS

        StopCAP();
#if defined(ENABLE_PERF_COUNTERS)
        STOP_JIT_PERF();
#endif

    }
    LOG((LF_JIT, LL_INFO1000, "Done Jitting method %s::%s  %s }\n",cls,name, ftn->m_pszDebugMethodSignature));

	if (flags & CORJIT_FLG_IMPORT_ONLY) {
		if (SUCCEEDED(res))
			ftn->SetIsVerified(TRUE);		// We only cared about the success and the side effect of setting 'Not Inline'
		goto done;
	}
    if (!SUCCEEDED(res))
    {
      COUNTER_ONLY(GetPrivatePerfCounters().m_Jit.cJitFailures++);
      COUNTER_ONLY(GetGlobalPerfCounters().m_Jit.cJitFailures++);

      // Throw the appropriate exception depending on the reason for failure
      if (res == CORJIT_OUTOFMEM)
            COMPlusThrowOM();
      else if (res == CORJIT_BADCODE)
            goto exit;
      else // (res == CORJIT_INTERNALERROR )
            COMPlusThrow(kExecutionEngineException);
    }

    LOG((LF_JIT, LL_INFO1000,
        "%s Jitted Entry %08x method %s::%s %s\n",
        (isOptIl ? "OptJit" : (*fEJit? "Ejit" : "Jit")), nativeEntry,ftn->m_pszDebugClassName, ftn->m_pszDebugMethodName, ftn->m_pszDebugMethodSignature));

#ifdef VTUNE_STATS
    extern LPCUTF8 NameForMethodDesc(UINT_PTR pMD);
    extern MethodDesc* IP2MD(ULONG_PTR IP);
    extern LPCUTF8 ClassNameForMethodDesc(UINT_PTR pMD);
    MethodDesc* meth = IP2MD ((ULONG_PTR)nativeEntry);

    WCHAR dumpMethods[3];

    if (WszGetEnvironmentVariable(L"DUMP_METHODS", dumpMethods, 2))
    {
        printf("VtuneStats %s::%s 0x%x\n", ClassNameForMethodDesc((UINT_PTR)meth), NameForMethodDesc((UINT_PTR)meth), nativeEntry);
    }
#endif // VTUNE_STATS

    ret = (Stub*)nativeEntry;
    }
exit:
    if (!ret)
        COMPlusThrow(kInvalidProgramException);

done:
    COOPERATIVE_TRANSITION_END();
    return ret;
}



/*********************************************************************/
HCIMPL2(Object *, JIT_StrCns, unsigned metaTok, CORINFO_MODULE_HANDLE scopeHnd)
{
    OBJECTHANDLE hndStr;

    HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_NOPOLL(Frame::FRAME_ATTR_RETURNOBJ);
    HELPER_METHOD_POLL();

    // Retrieve the handle to the COM+ string object.
    hndStr = ConstructStringLiteral(scopeHnd, metaTok);
    HELPER_METHOD_FRAME_END();

    // Don't use ObjectFromHandle; this isn't a real handle
    return *(Object**)hndStr;
}
HCIMPLEND

/*********************************************************************/

HCIMPL2(Object *, JITutil_IsInstanceOfBizarre, CORINFO_CLASS_HANDLE type, Object* obj)
{
    THROWSCOMPLUSEXCEPTION();

        // Null is an instance of any type
    //if (obj == NULL)
    //    return 0;
    _ASSERTE(obj != NULL);  // this check is done in the ASM helper

    TypeHandle clsHnd(type);
    /*@GENERICS: removed this assertion as we might have an ordinary class type in the case of unbox.any !0
    _ASSERTE(!clsHnd.IsUnsharedMT() ||
             (clsHnd.IsUnsharedMT() && clsHnd.IsInterface()));
    */

    VALIDATEOBJECTREF(obj);
    MethodTable *pMT = obj->GetMethodTable();

    // any method table with an instance has been restored.
    _ASSERTE(pMT->GetClass()->IsRestored());

    // Since classes are handled in another helper, this can't happen.
    //@GENERICS: yes it can now because of unbox.any !0 (see above)
    //    _ASSERTE(clsHnd != TypeHandle(pMT));

    if (pMT->IsThunking())
    {
        
        // Check whether the type represented by the proxy can be
        // cast to the given type
        HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_1(Frame::FRAME_ATTR_RETURNOBJ, obj);
        clsHnd.CheckRestore();
        if (!CRemotingServices::CheckCast(ObjectToOBJECTREF(obj), clsHnd)) {
            obj = 0;
        }
        HELPER_METHOD_FRAME_END();
        return obj;
    }

    // If it is a class we can handle it all right here.
    // We have already checked for proxy. Code below can cause a GC, better
    // create a frame.
    if (clsHnd.IsUnsharedMT())
    {
        /*@GENERICS: removed this assertion as we might have an ordinary class type in the case of unbox.any !0
        // Since non-interface classes are in another helper, this must be an interface or instantiated type.
        _ASSERTE(clsHnd.IsInterface());
	*/

        if (!clsHnd.IsInterface())
	{
              HELPER_METHOD_FRAME_BEGIN_RET_1(obj);
              TypeHandle objHnd = TypeHandle(pMT);
              while (!objHnd.IsNull())
              {
                if (objHnd == clsHnd) break;
                objHnd = objHnd.GetParent();
              }
              if (objHnd.IsNull())
                obj = 0;
              HELPER_METHOD_FRAME_END();
	}

        {
            // If the interface has not been restored then erect a helper frame to handle
            // exceptions and restore it.
            if (!clsHnd.AsMethodTable()->GetClass()->IsRestored())
            {
                HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_1(Frame::FRAME_ATTR_RETURNOBJ, obj);
                clsHnd.CheckRestore();
                HELPER_METHOD_FRAME_END();
                // Standard object type, no frame needed.
                if (!pMT->GetClass()->SupportsInterface(OBJECTREF(obj), clsHnd.AsMethodTable()))
                    obj = 0;
            }
            else
                obj = 0;

        }
    } 
    else {
        // We know that the clsHnd is an array so check the object.  If it is not an array return false.
        if (pMT->IsArray()) {
            ArrayBase* arrayObj = (ArrayBase*) obj;
            // Check if both are SZARRAY and their typehandles match
            if (pMT->GetNormCorElementType() != ELEMENT_TYPE_SZARRAY ||
                clsHnd.GetNormCorElementType() != ELEMENT_TYPE_SZARRAY ||
                clsHnd.AsTypeDesc()->GetTypeParam() != arrayObj->GetElementTypeHandle())
            {
                ArrayTypeDesc objType(pMT, arrayObj->GetElementTypeHandle());
                HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_1(Frame::FRAME_ATTR_RETURNOBJ, obj);
                if (!objType.CanCastTo(clsHnd.AsArray()))
                    obj = 0;
                HELPER_METHOD_FRAME_END();
            }
        }
        else 
            obj = 0;
    }
    FC_GC_POLL_AND_RETURN_OBJREF(obj);
}
HCIMPLEND

// This is the failure & bizarre portion of JIT_ChkCast.  If the fast-path ASM case
// fails, we fall back here.

HCIMPL2(Object *, JITutil_ChkCastBizarre, CORINFO_CLASS_HANDLE type, Object *obj)
{
        // Null is an instance of any type
    if (obj == NULL)
        return obj;

    // ChkCast is like isInstance trhow if null
    ENDFORBIDGC();
    obj = JITutil_IsInstanceOfBizarre(type, obj);
    if (obj != NULL)
        return(obj);

    FCThrow(kInvalidCastException);
}
HCIMPLEND

// @GENERICS:
// Given a token (annotatedMetaTOK) representing a type (typeDef/Ref/Spec) or method (memberRef/methodSpec), 
// look up the corresponding handle (TypeHandle or MethodDesc) at runtime.
//
// Postprocess the handle according to the top two bits of the token (CORINFO_ANNOT_MASK):
//   CORINFO_ANNOT_ARRAY        return a handle for an SZARRAY whose element type is given by the token
//   CORINFO_ANNOT_ENTRYPOINT   return a code pointer instead of a MethodDesc (typically its prestub)
//   CORINFO_ANNOT_ALLOWINSTPARAM return a MethodDesc or code pointer to shared code for generic methods or methods in generic structs 
//                              (which might require an extra parameter)
//
// The instantiation of class and method type parameters is provided by the methodHnd context; if classHnd
// is non-null then use this for class type parameters instead as methodHnd only provides a *representative*
// instantiation.
//
// If slotPtr is non-null then store then handle there too.
HCIMPL4(CORINFO_GENERIC_HANDLE, JIT_RuntimeHandle,  CORINFO_METHOD_HANDLE  methodHnd, unsigned annotatedMetaTOK,
  CORINFO_GENERIC_HANDLE* slotPtr, CORINFO_CLASS_HANDLE classHnd)
{
    // Cheap test to see if slot is already filled in
    // This allows the caller to omit the null test itself
    // to give a saving in code size at the expense of a helper call every time
    if (slotPtr != NULL && *slotPtr != NULL)
        return *slotPtr;

    // Could throw an exception e.g. in the class loader
    THROWSCOMPLUSEXCEPTION();

    // Result is a generic handle (in fact, a CORINFO_CLASS_HANDLE, CORINFO_METHOD_HANDLE, or a code pointer)
    CORINFO_GENERIC_HANDLE result = NULL;

    // Set up a frame
    HELPER_METHOD_FRAME_BEGIN_RET_NOPOLL(); 

    // Method context (this is the code that's requesting the handle)
    MethodDesc* context = (MethodDesc*)(methodHnd);

    // If this is an instantiated method then get the method instantiation (it will be exact)
    TypeHandle *methodInst = context->GetMethodInstantiation();

    // Get the class instantiation, using the exact handle passed in if necessary.  Note the
    // the method may live in some parent class.
    TypeHandle *classInst = context->GetClassInstantiation(TypeHandle(classHnd));

    // Strip off annotation
    unsigned metaTOK = annotatedMetaTOK & ~CORINFO_ANNOT_MASK;
    unsigned annotation = annotatedMetaTOK & CORINFO_ANNOT_MASK;

    // This is where the token lives
    Module* module = context->GetModule();
   
    OBJECTREF pThrowable = NULL;
    GCPROTECT_BEGIN(pThrowable);       

    // We're expecting a type or method, as nothing else is needed at run-time
    switch (TypeFromToken(metaTOK))
    {   
      case mdtTypeSpec :
      {
        NameHandle nh(module, metaTOK);
        TypeHandle clsHnd = module->GetClassLoader()->LoadTypeHandle(&nh, &pThrowable, classInst, methodInst);
 
        if (clsHnd.IsNull())
          COMPlusThrow(pThrowable);
        
        switch (annotation)
        {
          case CORINFO_ANNOT_ARRAY :
            clsHnd = module->GetClassLoader()->FindArrayForElem(clsHnd, ELEMENT_TYPE_SZARRAY);
            if (clsHnd.IsNull())
             COMPlusThrow(pThrowable);
            break;
        }

#ifdef _DEBUG
        char buff[200];
        clsHnd.GetName(buff, 200);
        LOG((LF_JIT, LL_INFO10000,
          "GENERICS: Assigned type 0x%x = %s to slot %p for typeSpec token 0x%x\n", clsHnd.AsPtr(), buff, slotPtr, annotatedMetaTOK));
        if (slotPtr && *slotPtr) 
        {
          TypeHandle clsHnd2 = TypeHandle(*slotPtr);
          clsHnd2.GetName(buff, 200);
          LOG((LF_JIT, LL_INFO10000, "GENERICS: slot was already assigned 0x%x = %s\n", clsHnd2.AsPtr(), buff));
      _ASSERTE(clsHnd == clsHnd2);
    }
#endif    
 
        result = CORINFO_GENERIC_HANDLE(clsHnd.AsPtr());

        break;
      }

      case mdtMemberRef :
      case mdtMethodSpec :
      {
        MethodDesc* md;
        HRESULT res = EEClass::GetMethodDescFromMemberRef(module, (mdMemberRef) metaTOK, &md, &pThrowable, classInst, methodInst,
                              (annotation & CORINFO_ANNOT_ALLOWINSTPARAM) != 0);
        if (SUCCEEDED(res) && md != NULL)
        {
          if (annotation & CORINFO_ANNOT_ENTRYPOINT)
          {
            void* addr = (void*) (md->GetAddrOfCodeForLdFtn());
            result = CORINFO_GENERIC_HANDLE(addr);
          }
          else 
            result = CORINFO_GENERIC_HANDLE(md);

#ifdef _DEBUG       
          LOG((LF_JIT, LL_INFO10000,
            "GENERICS: Assigned 0x%x to slot %p for method-ref token 0x%x\n", result, slotPtr, annotatedMetaTOK));
          if (slotPtr && *slotPtr)
          {
            LOG((LF_JIT, LL_INFO10000, "GENERICS: slot was already assigned 0x%x\n", *slotPtr));
          }
#endif     
        }
        else
          COMPlusThrow(pThrowable);        
        break;
      }

      default :
        _ASSERTE(!"Invalid token type");
    }

    // Update the slot
    // No harm is done if another thread has already updated it but we expect it to be the same!
    if (slotPtr) 
    {
#ifdef _DEBUG
      if (*slotPtr)
      {
    //  _ASSERTE(*slotPtr == result);
      }
#endif
      *slotPtr = result;
    }

    GCPROTECT_END();
    HELPER_METHOD_FRAME_END();

    // Return the handle
    return result;
}
HCIMPLEND

/*********************************************************************/
// putting the framed helpers first
/*********************************************************************/
// A helper for JIT_MonEnter that is on the callee side of an ecall
// frame and handles cases that might allocate,  or block.
HCIMPL1(void, JITutil_MonEnter,  Object* obj)
{
    HELPER_METHOD_FRAME_BEGIN_NOPOLL();
    THROWSCOMPLUSEXCEPTION();
    obj->EnterObjMonitor();
    HELPER_METHOD_FRAME_END_POLL();
}
HCIMPLEND

/*********************************************************************/
// A helper for JIT_MonEnterStatic that is on the callee side of an ecall
// frame and handles cases that might allocate, throw or block.
HCIMPL1(void, JITutil_MonEnterStatic,  EEClass* pclass)
{
    HELPER_METHOD_FRAME_BEGIN_NOPOLL();
    pclass->GetMethodTable()->GetExposedClassObject()->EnterObjMonitor();
    HELPER_METHOD_FRAME_END_POLL();
}
HCIMPLEND

/*********************************************************************/
// A helper for JIT_MonTryEnter that is on the callee side of an ecall
// frame and handles cases that might allocate, throw or block.
HCIMPL2(BOOL, JITutil_MonTryEnter,  Object* obj, __int32 timeOut)
{
    BOOL ret;
    HELPER_METHOD_FRAME_BEGIN_RET_NOPOLL();
    THROWSCOMPLUSEXCEPTION();
    OBJECTREF objRef = ObjectToOBJECTREF(obj);
    ret = objRef->TryEnterObjMonitor(timeOut);
    HELPER_METHOD_FRAME_END_POLL();
    return(ret);
}
HCIMPLEND

/*********************************************************************/
// A helper for JIT_MonEnter that is on the callee side of an ecall
// frame and handles the contention case.
HCIMPL1(void, JITutil_MonContention, AwareLock* lock)
{
    HELPER_METHOD_FRAME_BEGIN_NOPOLL();
    THROWSCOMPLUSEXCEPTION();
    lock->Contention();
    HELPER_METHOD_FRAME_END_POLL();
}
HCIMPLEND

/*********************************************************************/
// A helper for JIT_MonExit and JIT_MonExitStatic that is on the
// callee side of an ecall frame and handles cases that might allocate,
// throw or block.
HCIMPL1(void, JITutil_MonExit,  AwareLock* lock)
{
    HELPER_METHOD_FRAME_BEGIN_NOPOLL();
    THROWSCOMPLUSEXCEPTION();
    lock->Signal();
    HELPER_METHOD_FRAME_END_POLL();
}
HCIMPLEND

/*********************************************************************/
// In general, we want to use COMPlusThrow to throw exceptions.  However, 
// the JIT_Throw helper is a special case.  Here, we're called from
// managed code.  We have a guarantee that the first FS:0 handler
// is our COMPlusFrameHandler.  We could call COMPlusThrow(), which pushes
// another handler, but there is a significant (10% on JGFExceptionBench) 
// performance gain if we avoid this by calling RaiseTheException() 
// directly.
//
extern VOID RaiseTheException(OBJECTREF pThroable, BOOL rethrow);

/*************************************************************/

HCIMPL1(void, JIT_Throw,  Object* obj)
{
    THROWSCOMPLUSEXCEPTION();

    
    /* Make no assumptions about the current machine state */
    ResetCurrentContext();

    FC_GC_POLL_NOT_NEEDED();    // throws always open up for GC
    HELPER_METHOD_FRAME_BEGIN_ATTRIB_NOPOLL(Frame::FRAME_ATTR_EXCEPTION);    // Set up a frame


    VALIDATEOBJECTREF(obj);

#if defined(_DEBUG) && defined(_X86_)
    __helperframe.InsureInit();
    g_ExceptionEIP = __helperframe.GetReturnAddress();
#endif // defined(_DEBUG) && defined(_X86_)

    if (obj == 0)
        COMPlusThrow(kNullReferenceException);
    else
        RaiseTheException(ObjectToOBJECTREF(obj), FALSE);

    HELPER_METHOD_FRAME_END();
}
HCIMPLEND

/*************************************************************/

HCIMPL0(void, JIT_Rethrow)
{
    THROWSCOMPLUSEXCEPTION();

    FC_GC_POLL_NOT_NEEDED();    // throws always open up for GC
    HELPER_METHOD_FRAME_BEGIN_ATTRIB_NOPOLL(Frame::FRAME_ATTR_EXCEPTION);    // Set up a frame

    OBJECTREF throwable = GETTHROWABLE();
    if (throwable != NULL)
        RaiseTheException(throwable, TRUE);
    else
        // This can only be the result of bad IL (or some internal EE failure).
        RealCOMPlusThrow(kInvalidProgramException, (UINT)IDS_EE_RETHROW_NOT_ALLOWED);

    HELPER_METHOD_FRAME_END();
}
HCIMPLEND

/*********************************************************************/
/* throw an exception in jitted code where the return address does
   not accurately reflect what the correct 'try' block.  Instead
   'EHindex' represents the try block.  It is 0 if there is not try
    blocks, or a 1 based index of the try block we are currently in */


void OutOfLineException(
            unsigned EHindex, 
            RuntimeExceptionKind excep
            , HelperMethodFrame* curFrame
        ) 
{

    THROWSCOMPLUSEXCEPTION();

    /* Make no assumptions about the current machine state */
    ResetCurrentContext();

    // Indicate that the frame caused and exception
    unsigned attribs = Frame::FRAME_ATTR_EXCEPTION;


    if (false &&
        EHindex > 0)
    {
        curFrame->InsureInit();
        --EHindex;
        /* Get the JitManager for the method */
        SLOT          retAddr = (SLOT)curFrame->GetReturnAddress();
        IJitManager*    pEEJM = ExecutionManager::FindJitMan(retAddr);
        _ASSERTE(pEEJM != 0);

        /* Now get the exception table of the method */

        METHODTOKEN     methodToken;
        DWORD           relOffset;
        pEEJM->JitCode2MethodTokenAndOffset(retAddr,&methodToken,&relOffset);
        SLOT            startAddr = retAddr - relOffset;
        _ASSERTE(methodToken);
        EH_CLAUSE_ENUMERATOR EnumState;
#ifdef _DEBUG
        unsigned        EHCount =
#endif
        pEEJM->InitializeEHEnumeration(methodToken,&EnumState);
        _ASSERTE(EHindex < EHCount);

        /* Find the matching clause */

        EE_ILEXCEPTION_CLAUSE EHClause, *EHClausePtr;
        do {
            EHClausePtr = pEEJM->GetNextEHClause(methodToken,&EnumState,&EHClause);
        } while (EHindex-- > 0);

        // Set the return address to somewhere in the exception handler
        // We better not ever return!
        BYTE* startAddrOfClause = startAddr + EHClausePtr->TryStartPC;
        curFrame->SetReturnAddress(startAddrOfClause);
        _ASSERTE(curFrame->GetReturnAddress() == startAddrOfClause);

            // tell the stack crawlerthat we are out of line
        attribs |= Frame::FRAME_ATTR_OUT_OF_LINE;
    }

    curFrame->SetFrameAttribs(attribs);

    COMPlusThrow(excep);
    _ASSERTE(!"COMPlusThrow returned!");
}

/*********************************************************************/
HCIMPL1(void, JIT_RngChkFail,  unsigned tryIndex)
{
    HELPER_METHOD_FRAME_BEGIN_NOPOLL();    // Set up a frame

    // tryIndex is 0 if no exception handlers
    // otherwise it is the index (1 based), of the
    // most deeply nested handler that contains the throw
    OutOfLineException(tryIndex, kIndexOutOfRangeException, &__helperframe);    // __helperframe created by HELPER_METHOD_FRAME
    HELPER_METHOD_FRAME_END_POLL();
}
HCIMPLEND

/*********************************************************************/
HCIMPL1(void, JIT_Overflow,  unsigned tryIndex)
{
    HELPER_METHOD_FRAME_BEGIN_NOPOLL();    // Set up a frame

    // tryIndex is 0 if no exception handlers
    // otherwise it is the index (1 based), of the
    // most deeply nested handler that contains the throw
    OutOfLineException(tryIndex, kOverflowException, &__helperframe);   // __helperframe created by HELPER_METHOD_FRAME
    HELPER_METHOD_FRAME_END_POLL();
}
HCIMPLEND

/*********************************************************************/
HCIMPL1(void, JIT_Verification,  int ilOffset)
{
    THROWSCOMPLUSEXCEPTION();

    FC_GC_POLL_NOT_NEEDED();    // throws always open up for GC
    HELPER_METHOD_FRAME_BEGIN_ATTRIB_NOPOLL(Frame::FRAME_ATTR_EXCEPTION);    // Set up a frame


    //WCHAR str[30];
    //swprintf(str,L"At or near IL offset %x\0",ilOffset);
    COMPlusThrow(kVerificationException);
    
    HELPER_METHOD_FRAME_END();
}
HCIMPLEND

/*********************************************************************/
HCIMPL1(void, JIT_SecurityUnmanagedCodeException, CORINFO_CLASS_HANDLE typeHnd_)
{
    THROWSCOMPLUSEXCEPTION();

    FC_GC_POLL_NOT_NEEDED();    // throws always open up for GC
    HELPER_METHOD_FRAME_BEGIN_ATTRIB_NOPOLL(Frame::FRAME_ATTR_EXCEPTION);    // Set up a frame


    Security::ThrowSecurityException(g_SecurityPermissionClassName, SPFLAGSUNMANAGEDCODE);

    HELPER_METHOD_FRAME_END();
}
HCIMPLEND

/*********************************************************************/
// JIT_UserBreakpoint
// Called by the JIT whenever a cee_break instruction should be executed.
// This ensures that enough info will be pushed onto the stack so that
// we can continue from the exception w/o having special code elsewhere.
// Body of function is written by debugger team
// Args: None
//
// Note: this code is duplicated in the ecall in VM\DebugDebugger:Break,
// so propogate changes to there

HCIMPL0(void, JIT_UserBreakpoint)
{
    HELPER_METHOD_FRAME_BEGIN_NOPOLL();    // Set up a frame

#ifdef DEBUGGING_SUPPORTED
    DebuggerExitFrame __def;

    g_pDebugInterface->SendUserBreakpoint(GetThread());

    __def.Pop();
#else
    _ASSERTE("JIT_UserBreakpoint called, but debugging support is not available in this build.");
#endif // DEBUGGING_SUPPORTED

    HELPER_METHOD_FRAME_END_POLL();
}
HCIMPLEND

static const RuntimeExceptionKind map[CORINFO_Exception_Count] =
{
    kNullReferenceException,
    kDivideByZeroException,
    kInvalidCastException,
    kIndexOutOfRangeException,
    kOverflowException,
    kSynchronizationLockException,
    kArrayTypeMismatchException,
    kRankException,
    kArgumentNullException,
    kArgumentException,
};


/*********************************************************************/
HCIMPL1(void, JIT_InternalThrow, unsigned exceptNum)
{

        // spot check of the array above
    _ASSERTE(map[CORINFO_NullReferenceException] == kNullReferenceException);
    _ASSERTE(map[CORINFO_DivideByZeroException] == kDivideByZeroException);
    _ASSERTE(map[CORINFO_IndexOutOfRangeException] == kIndexOutOfRangeException);
    _ASSERTE(map[CORINFO_OverflowException] == kOverflowException);
    _ASSERTE(map[CORINFO_SynchronizationLockException] == kSynchronizationLockException);
    _ASSERTE(map[CORINFO_ArrayTypeMismatchException] == kArrayTypeMismatchException);
    _ASSERTE(map[CORINFO_RankException] == kRankException);
    _ASSERTE(map[CORINFO_ArgumentNullException] == kArgumentNullException);
    _ASSERTE(map[CORINFO_ArgumentException] == kArgumentException);

    _ASSERTE(exceptNum < CORINFO_Exception_Count);
    _ASSERTE(map[exceptNum] != 0);

        // Most JIT helpers can ONLY be called from jitted code, and thus the
        // exact depth is not needed.  However, Throw is tail called from stubs, 
        // which might have been called for CallDescr, so we need the exact depth
    FC_GC_POLL_NOT_NEEDED();    // throws always open up for GC
    HELPER_METHOD_FRAME_BEGIN_ATTRIB_NOPOLL(Frame::FRAME_ATTR_EXACT_DEPTH);
    THROWSCOMPLUSEXCEPTION();
    COMPlusThrow(map[exceptNum]);
    HELPER_METHOD_FRAME_END();
}
HCIMPLEND

HCIMPL1(void, JIT_InternalThrowFromHelper, unsigned exceptNum)
{
        // spot check of the array above
    _ASSERTE(map[CORINFO_NullReferenceException] == kNullReferenceException);
    _ASSERTE(map[CORINFO_IndexOutOfRangeException] == kIndexOutOfRangeException);
    _ASSERTE(map[CORINFO_OverflowException] == kOverflowException);

    _ASSERTE(exceptNum < CORINFO_Exception_Count);
    _ASSERTE(map[exceptNum] != 0);

    FC_GC_POLL_NOT_NEEDED();    // throws always open up for GC
    HELPER_METHOD_FRAME_BEGIN_ATTRIB_NOPOLL(Frame::FRAME_ATTR_CAPTURE_DEPTH_2|Frame::FRAME_ATTR_EXACT_DEPTH);
    THROWSCOMPLUSEXCEPTION();
    COMPlusThrow(map[exceptNum]);
    HELPER_METHOD_FRAME_END();
}
HCIMPLEND

/*********************************************************************/

HCIMPL1(void*, JIT_GetStaticFieldAddr, FieldDesc *pFD)
{
    THROWSCOMPLUSEXCEPTION();

    _ASSERTE(pFD->IsThreadStatic() || pFD->IsContextStatic() || 
             pFD->GetMethodTableOfEnclosingClass()->IsShared());

    void *addr = NULL;

        // When we care at all about the speed of context and thread local statics
        // we shoudl avoid constructing the frame when possible
    OBJECTREF throwable = 0;
    HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_1(Frame::FRAME_ATTR_RETURN_INTERIOR, throwable);

    MethodTable *pMT = pFD->GetMethodTableOfEnclosingClass();
    pMT->CheckRestore();

    DomainLocalClass *pLocalClass;
    if (!pMT->CheckRunClassInit(&throwable, &pLocalClass))
        COMPlusThrow(throwable);

    if(pFD->IsThreadStatic())
    {
        addr = Thread::GetStaticFieldAddress(pFD);
    }
    else if (pFD->IsContextStatic())
    {
        addr = Context::GetStaticFieldAddress(pFD);
    }
    else 
    {
        _ASSERTE(pMT->IsShared());

        void *base = pFD->GetSharedBase(pLocalClass);
        addr = pFD->GetStaticAddress(base);
    }

    HELPER_METHOD_FRAME_END();
    return addr;
}
HCIMPLEND

/*********************************************************************/
HCIMPL1(void *, JIT_GetThreadFieldAddr_Primitive, FieldDesc *pFD)
    
    MethodTable* pMT = pFD->GetMethodTableOfEnclosingClass();
    _ASSERTE(pFD->IsThreadStatic());
    _ASSERTE(CorTypeInfo::IsPrimitiveType(pFD->GetFieldType()));
    _ASSERTE(pFD->GetOffset() <= FIELD_OFFSET_LAST_REAL_OFFSET);

    Thread *pThread = NULL;
    STATIC_DATA *pData = NULL;
    WORD wClassOffset = 0;
    BYTE *dataBits = NULL;

    if (!pMT->IsRestoredAndClassInited())
        goto SLOW;

    pThread = GetThread();
    pData = pMT->IsShared() ? pThread->GetSharedStaticData() : pThread->GetUnsharedStaticData();
    if (pData == 0)
        goto SLOW;

    wClassOffset = pMT->GetClass()->GetThreadStaticOffset();
    if (wClassOffset >= pData->cElem)
        goto SLOW;

    dataBits = (BYTE*) pData->dataPtr[wClassOffset];
    if (dataBits == 0)
        goto SLOW;

    return &dataBits[pFD->GetOffsetUnsafe()];

SLOW:
    ENDFORBIDGC();
    return(JIT_GetStaticFieldAddr(pFD));
HCIMPLEND


/*********************************************************************/
HCIMPL1(void *, JIT_GetThreadFieldAddr_Objref, FieldDesc *pFD)

    MethodTable* pMT = pFD->GetMethodTableOfEnclosingClass();
    _ASSERTE(pFD->IsThreadStatic());
    _ASSERTE(CorTypeInfo::IsObjRef(pFD->GetFieldType()));
    _ASSERTE(pFD->GetOffset() <= FIELD_OFFSET_LAST_REAL_OFFSET);

    Thread *pThread = NULL;
    STATIC_DATA *pData = NULL;
    WORD wClassOffset = 0;
    BYTE *dataBits = NULL;
    Object **handle = NULL;

    if (!pMT->IsRestoredAndClassInited())
        goto SLOW;

    pThread = GetThread();
    pData = pMT->IsShared() ? pThread->GetSharedStaticData() : pThread->GetUnsharedStaticData();
    if (pData == 0)
        goto SLOW;

    wClassOffset = pMT->GetClass()->GetThreadStaticOffset();
    if (wClassOffset >= pData->cElem)
        goto SLOW;

    dataBits = (BYTE*) pData->dataPtr[wClassOffset];
    if (dataBits == 0)
        goto SLOW;

    handle = *((Object***) &dataBits[pFD->GetOffsetUnsafe()]);
    if (handle == 0)
        goto SLOW;
    return handle;

SLOW:
    ENDFORBIDGC();
    return(JIT_GetStaticFieldAddr(pFD));

HCIMPLEND

#ifndef _X86_
void JIT_ProfilerStub(UINT_PTR ProfilingHandle)
{
    return;
}
#endif // !_X86_


/*********************************************************************/
HCIMPL1(void *, JIT_GetContextFieldAddr_Primitive, FieldDesc *pFD)
    
    MethodTable* pMT = pFD->GetMethodTableOfEnclosingClass();
    _ASSERTE(pFD->IsContextStatic());
    _ASSERTE(CorTypeInfo::IsPrimitiveType(pFD->GetFieldType()));
    _ASSERTE(pFD->GetOffset() <= FIELD_OFFSET_LAST_REAL_OFFSET);

    Context *pCtx = NULL;
    STATIC_DATA *pData = NULL;
    WORD wClassOffset = 0;
    BYTE *dataBits = NULL;

    if (!pMT->IsRestoredAndClassInited())
        goto SLOW;

    pCtx = GetCurrentContext();
    _ASSERTE(pCtx);
    pData = pMT->IsShared() ? pCtx->GetSharedStaticData() : pCtx->GetUnsharedStaticData();
    if (pData == 0)
        goto SLOW;

    wClassOffset = pMT->GetClass()->GetContextStaticOffset();
    if (wClassOffset >= pData->cElem)
        goto SLOW;

    dataBits = (BYTE*) pData->dataPtr[wClassOffset];
    if (dataBits == 0)
        goto SLOW;

    return &dataBits[pFD->GetOffsetUnsafe()];

SLOW:
    ENDFORBIDGC();
    return(JIT_GetStaticFieldAddr(pFD));
HCIMPLEND

/*********************************************************************/
HCIMPL1(void *, JIT_GetContextFieldAddr_Objref, FieldDesc *pFD)
    
    MethodTable* pMT = pFD->GetMethodTableOfEnclosingClass();
    _ASSERTE(pFD->IsContextStatic());
    _ASSERTE(CorTypeInfo::IsObjRef(pFD->GetFieldType()));
    _ASSERTE(pFD->GetOffset() <= FIELD_OFFSET_LAST_REAL_OFFSET);

    Context *pCtx = NULL;
    STATIC_DATA *pData = NULL;
    WORD wClassOffset = 0;
    BYTE *dataBits = NULL;
    Object **handle = NULL;

    if (!pMT->IsRestoredAndClassInited())
        goto SLOW;

    pCtx = GetCurrentContext();
    _ASSERTE(pCtx);
    pData = pMT->IsShared() ? pCtx->GetSharedStaticData() : pCtx->GetUnsharedStaticData();
    if (pData == 0)
        goto SLOW;

    wClassOffset = pMT->GetClass()->GetContextStaticOffset();
    if (wClassOffset >= pData->cElem)
        goto SLOW;

    dataBits = (BYTE*) pData->dataPtr[wClassOffset];
    if (dataBits == 0)
        goto SLOW;

    handle = *((Object***) &dataBits[pFD->GetOffsetUnsafe()]);
    if (handle == 0)
        goto SLOW;

    return handle;

SLOW:
    ENDFORBIDGC();
    return(JIT_GetStaticFieldAddr(pFD));
HCIMPLEND

#ifdef _MSC_VER
#pragma optimize("", on)
#endif

/*********************************************************************/
// This is a helper routine used by JIT_GetField32 below
__int32 __stdcall JIT_GetField32Worker(OBJECTREF objRef, FieldDesc *pFD)
{
    THROWSCOMPLUSEXCEPTION();

    switch (pFD->GetFieldType())
    {
        case ELEMENT_TYPE_I1:
            return (INT8)(pFD->GetValue8(objRef));
        case ELEMENT_TYPE_BOOLEAN:
        case ELEMENT_TYPE_U1:
            return (UINT8)(pFD->GetValue8(objRef));
        case ELEMENT_TYPE_I2:
            return (INT16)(pFD->GetValue16(objRef));
        case ELEMENT_TYPE_CHAR:
        case ELEMENT_TYPE_U2:
            return (UINT16)(pFD->GetValue16(objRef));
        case ELEMENT_TYPE_I4: // can fallthru
        case ELEMENT_TYPE_U4:
        IN_WIN32(case ELEMENT_TYPE_PTR:)
        IN_WIN32(case ELEMENT_TYPE_I:)
        IN_WIN32(case ELEMENT_TYPE_U:)
            return pFD->GetValue32(objRef);

        case ELEMENT_TYPE_R4:
            INT32 value;
            value = pFD->GetValue32(objRef);
            setFPReturn(4, value);
            return value;

        default:
            _ASSERTE(!"Bad Type");
			// as the assert above implies, this should never happen, however if it does
			// it is acceptable to return 0, because the system is not in an inconsistant state.
			// Throwing is hard  because we are called from both framed and unframed contexts
			return 0;
    }
}

/*********************************************************************/
HCIMPL2(__int32, JIT_GetField32, Object *obj, FieldDesc *pFD)
{
    THROWSCOMPLUSEXCEPTION();
    // This is an instance field helper
    _ASSERTE(!pFD->IsStatic());

    if (obj == NULL)
        FCThrow(kNullReferenceException);

    OBJECTREF objRef = ObjectToOBJECTREF(obj);

    INT32 value = 0;

    // basic sanity checks.  Are we pointing at a valid FD and objRef?
    _ASSERTE(pFD->GetMethodTableOfEnclosingClass()->GetClass()->GetMethodTable() == pFD->GetMethodTableOfEnclosingClass());

    // Try an unwrap operation to check that we are not being called in the
    // same context as the server. If that is the case then unwrap will return
    // the server object.
    if(objRef->GetMethodTable()->IsTransparentProxyType())
    {
        objRef = CRemotingServices::GetObjectFromProxy(objRef, TRUE);
        // This is a cross context field access. Setup a frame as we will
        // transition to managed code later.
        HELPER_METHOD_FRAME_BEGIN_RET_NOPOLL();        // Set up a frame
        value = JIT_GetField32Worker(objRef, pFD);
        HELPER_METHOD_FRAME_END();                     // Tear down the frame
    }
    else
    {
        value = JIT_GetField32Worker(objRef, pFD);
    }

    FC_GC_POLL_RET();
    return value;
}
HCIMPLEND

/*********************************************************************/
// This is a helper routine used by JIT_GetField64 below
__int64 __stdcall JIT_GetField64Worker(OBJECTREF objRef, FieldDesc *pFD)
{
    INT64 value = pFD->GetValue64(objRef);
    if (ELEMENT_TYPE_R8 == pFD->GetFieldType())
    {
        setFPReturn(8, value);
    }
    return value;
}

/*********************************************************************/
HCIMPL2(__int64, JIT_GetField64, Object *obj, FieldDesc *pFD)
{
    THROWSCOMPLUSEXCEPTION();
    // This is an instance field helper
    _ASSERTE(!pFD->IsStatic());

    if (obj == NULL)
        FCThrow(kNullReferenceException);

    OBJECTREF objRef = ObjectToOBJECTREF(obj);
    INT64 value = 0;

    _ASSERTE(pFD->GetMethodTableOfEnclosingClass()->GetClass()->GetMethodTable() == pFD->GetMethodTableOfEnclosingClass());
    // Try an unwrap operation to check that we are not being called in the
    // same context as the server. If that is the case then unwrap will return
    // the server object.
    if(objRef->GetMethodTable()->IsTransparentProxyType())
    {
        objRef = CRemotingServices::GetObjectFromProxy(objRef, TRUE);
        // This is a cross context field access. Setup a frame as we will
        // transition to managed code later.
        HELPER_METHOD_FRAME_BEGIN_RET_NOPOLL();        // Set up a frame

        value = JIT_GetField64Worker(objRef, pFD);

        HELPER_METHOD_FRAME_END();              // Tear down the frame
    }
    else
    {
        value = JIT_GetField64Worker(objRef, pFD);
    }
    FC_GC_POLL_RET();
    return value;
}
HCIMPLEND

/*********************************************************************/
// This is a helper routine used by JIT_SetField32 below
static void __stdcall JIT_SetField32Worker(OBJECTREF objRef, FieldDesc *pFD, __int32 value)
{
    THROWSCOMPLUSEXCEPTION();

    switch (pFD->GetFieldType())
    {
        case ELEMENT_TYPE_I1:
        case ELEMENT_TYPE_U1:
        case ELEMENT_TYPE_BOOLEAN:
            pFD->SetValue8(objRef, value);
            break;

        case ELEMENT_TYPE_I2:
        case ELEMENT_TYPE_U2:
        case ELEMENT_TYPE_CHAR:
            pFD->SetValue16(objRef, value);
            break;

        case ELEMENT_TYPE_I4: // can fallthru
        case ELEMENT_TYPE_U4:
        case ELEMENT_TYPE_R4:
        IN_WIN32(case ELEMENT_TYPE_PTR:)
        IN_WIN32(case ELEMENT_TYPE_I:)
        IN_WIN32(case ELEMENT_TYPE_U:)
            pFD->SetValue32(objRef, value);
            break;

        default:
            _ASSERTE(!"Bad Type");
            // as the assert above implies, this should never happen, however if it does
            // it is acceptable to do nothing, because the system is not in an inconsistant state.
            // Throwing is hard because we are called from both framed and unframed contexts
    }
}

/*********************************************************************/
HCIMPL3(VOID, JIT_SetField32, Object *obj, FieldDesc *pFD, __int32 value)
{
    THROWSCOMPLUSEXCEPTION();
    // This is an instance field helper
    _ASSERTE(!pFD->IsStatic());

    if (obj == NULL)
        FCThrowVoid(kNullReferenceException);

    OBJECTREF objRef = ObjectToOBJECTREF(obj);

    // basic sanity checks.  Are we pointing at a valid FD and objRef?
    _ASSERTE(pFD->GetMethodTableOfEnclosingClass()->GetClass()->GetMethodTable() == pFD->GetMethodTableOfEnclosingClass());

    // Try an unwrap operation to check that we are not being called in the
    // same context as the server. If that is the case then unwrap will return
    // the server object.
    if(objRef->GetMethodTable()->IsTransparentProxyType())
    {
        objRef = CRemotingServices::GetObjectFromProxy(objRef, TRUE);
        // This is a cross context field access. Setup a frame as we will
        // transition to managed code later.
        HELPER_METHOD_FRAME_BEGIN_NOPOLL();        // Set up a frame

        JIT_SetField32Worker(objRef, pFD, value);

        HELPER_METHOD_FRAME_END();          // Tear down the frame
    }
    else
    {
        JIT_SetField32Worker(objRef, pFD, value);
    }

    FC_GC_POLL();
}
HCIMPLEND

/*********************************************************************/
HCIMPL3(VOID, JIT_SetField64, Object *obj, FieldDesc *pFD, __int64 value)
{
    THROWSCOMPLUSEXCEPTION();
    // This is an instance field helper
    _ASSERTE(!pFD->IsStatic());

    if (obj == NULL)
        FCThrowVoid(kNullReferenceException);

    OBJECTREF objRef = ObjectToOBJECTREF(obj);

    // basic sanity checks.  Are we pointing at a valid FD and objRef?
    _ASSERTE(pFD->GetMethodTableOfEnclosingClass()->GetClass()->GetMethodTable() == pFD->GetMethodTableOfEnclosingClass());

    // Try an unwrap operation to check that we are not being called in the
    // same context as the server. If that is the case then unwrap will return
    // the server object.
    if(objRef->GetMethodTable()->IsTransparentProxyType())
    {
        objRef = CRemotingServices::GetObjectFromProxy(objRef, TRUE);
        // This is a cross context field access. Setup a frame as we will
        // transition to managed code later.
        HELPER_METHOD_FRAME_BEGIN_NOPOLL();        // Set up a frame

        pFD->SetValue64(objRef, value);

        HELPER_METHOD_FRAME_END();          // Tear down the frame
    }
    else
    {
        *((__int64 *)pFD->GetAddress(OBJECTREFToObject(objRef))) = value;
    }
    FC_GC_POLL();
}
HCIMPLEND

/*********************************************************************/
HCIMPL2(Object*, JIT_GetFieldObj, Object *obj, FieldDesc *pFD)
{
    // This is an instance field helper
    _ASSERTE(!pFD->IsStatic());

    // Assert that we are called only for objects
    _ASSERTE(!pFD->IsPrimitive() && !pFD->IsByValue());

    if (obj == NULL)
        FCThrow(kNullReferenceException);

    OBJECTREF newobj = NULL;

#if CHECK_APP_DOMAIN_LEAKS
    if (pFD->GetMethodTableOfEnclosingClass()->IsValueClass())
    {
        // This case should only occur for dangerous fields
        _ASSERTE(pFD->IsDangerousAppDomainAgileField());
        newobj = *(OBJECTREF*)pFD->GetAddress((void*)obj); 
    }
    else
#endif
    {
        OBJECTREF objRef = ObjectToOBJECTREF(obj);

    // We should use this helper to get field values for marshalbyref types
    // or proxy types
        _ASSERTE(objRef->GetClass()->IsMarshaledByRef() || objRef->IsThunking() 
                 || pFD->IsDangerousAppDomainAgileField());

        // Try an unwrap operation to check that we are not being called in the
        // same context as the server. If that is the case then unwrap will return
        // the server object.
        if(objRef->GetMethodTable()->IsTransparentProxyType())
        {
            objRef = CRemotingServices::GetObjectFromProxy(objRef, TRUE);
            // This is a cross context field access. Setup a frame as we will
            // transition to managed code later.
            HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_NOPOLL(Frame::FRAME_ATTR_RETURNOBJ);    // Set up a frame

            newobj = pFD->GetRefValue(objRef);
            HELPER_METHOD_FRAME_END();          // Tear down the frame
        }
        else
        {
            newobj = ObjectToOBJECTREF(*((Object**) pFD->GetAddress(OBJECTREFToObject(objRef))));
        }
    }

    FC_GC_POLL_AND_RETURN_OBJREF(OBJECTREFToObject(newobj));
}
HCIMPLEND

/*********************************************************************/
HCIMPL3(VOID, JIT_SetFieldObj, Object *obj, FieldDesc *pFD, Object *value)
{
    // This is an instance field helper
    _ASSERTE(!pFD->IsStatic());

    // Assert that we are called only for objects
    _ASSERTE(!pFD->IsPrimitive() && !pFD->IsByValue());

    if (obj == NULL)
        FCThrowVoid(kNullReferenceException);

#if CHECK_APP_DOMAIN_LEAKS
    if (pFD->GetMethodTableOfEnclosingClass()->IsValueClass())
    {
        // This case should only occur for dangerous fields
        _ASSERTE(pFD->IsDangerousAppDomainAgileField());
        SetObjectReference((OBJECTREF*) pFD->GetAddress((void*)obj), 
                           ObjectToOBJECTREF(value), GetAppDomain());
    }
    else
#endif
    {
        OBJECTREF objRef = ObjectToOBJECTREF(obj);

       // We should use this helper to get field values for marshalbyref types
        // or proxy types
        _ASSERTE(objRef->GetClass()->IsMarshaledByRef() || objRef->IsThunking()
                 || pFD->IsDangerousAppDomainAgileField());

        // Try an unwrap operation to check that we are not being called in the
        // same context as the server. If that is the case then unwrap will return
        // the server object.
        if(objRef->GetMethodTable()->IsTransparentProxyType())
        {
            objRef = CRemotingServices::GetObjectFromProxy(objRef, TRUE);

            // This is a cross context field access. Setup a frame as we will
            // transition to managed code later.
            HELPER_METHOD_FRAME_BEGIN_NOPOLL();        // Set up a frame

            pFD->SetRefValue(objRef, ObjectToOBJECTREF(value));

            HELPER_METHOD_FRAME_END();          // Tear down the frame
        }
        else
        {
            pFD->SetRefValue(objRef, ObjectToOBJECTREF(value));
        }
    }
    FC_GC_POLL();
}
HCIMPLEND


/*********************************************************************/
HCIMPL3(VOID, JIT_GetFieldStruct, LPVOID retBuff, Object *obj, FieldDesc *pFD)
{
    // This is an instance field helper
    _ASSERTE(!pFD->IsStatic());

    // Assert that we are not called for objects or primitive types
    _ASSERTE(!pFD->IsPrimitive());

    if (obj == NULL)
        FCThrowVoid(kNullReferenceException);

    OBJECTREF objRef = ObjectToOBJECTREF(obj);

    // We should use this helper to get field values for marshalbyref types
    // or proxy types
    _ASSERTE(objRef->GetClass()->IsMarshaledByRef() || objRef->IsThunking());


    // This may be a  cross context field access. Setup a frame as we will
    // transition to managed code later
    //

    HELPER_METHOD_FRAME_BEGIN_NOPOLL();        // Set up a frame

    // Try an unwrap operation to check that we are not being called in the
    // same context as the server. If that is the case then unwrap will return
    // the server object.
    if(objRef->GetMethodTable()->IsTransparentProxyType())
        objRef = CRemotingServices::GetObjectFromProxy(objRef, TRUE);

    CRemotingServices::FieldAccessor(pFD, objRef, retBuff, TRUE);

    HELPER_METHOD_FRAME_END_POLL();          // Tear down the frame
}
HCIMPLEND

/*********************************************************************/
HCIMPL3(VOID, JIT_SetFieldStruct, Object *obj, FieldDesc *pFD, LPVOID valuePtr)
{
    // Assert that we are not called for objects or primitive types
    _ASSERTE(!pFD->IsPrimitive());

    if (obj == NULL)
        FCThrowVoid(kNullReferenceException);

    OBJECTREF objRef = ObjectToOBJECTREF(obj);

    // We should use this helper to get field values for marshalbyref types
    // or proxy types
    _ASSERTE(objRef->GetClass()->IsMarshaledByRef() || objRef->IsThunking()
             || pFD->IsDangerousAppDomainAgileField());

#ifdef _DEBUG
    if (pFD->IsDangerousAppDomainAgileField())
    {
        //
        // Verify that the object we are assigning to is also agile
        //

        if (objRef->IsAppDomainAgile())
        {
            // !!! validate that all dangerous fields of valuePtr are domain agile
        }
    }
#endif


    // This may be a  cross context field access. Setup a frame as we will
    // transition to managed code later
    //

    HELPER_METHOD_FRAME_BEGIN_NOPOLL();        // Set up a frame

    // Try an unwrap operation to check that we are not being called in the
    // same context as the server. If that is the case then unwrap will return
    // the server object.
    BEGINFORBIDGC();
    if(objRef->GetMethodTable()->IsTransparentProxyType())
        objRef = CRemotingServices::GetObjectFromProxy(objRef, TRUE);
    ENDFORBIDGC();

    CRemotingServices::FieldAccessor(pFD, objRef, valuePtr, FALSE);

    HELPER_METHOD_FRAME_END_POLL();          // Tear down the frame
}
HCIMPLEND

/*********************************************************************/
HCIMPL0(VOID, JIT_PollGC)
{
    FC_GC_POLL_NOT_NEEDED();

    Thread  *thread = GetThread();
    if (thread->CatchAtSafePoint())    // Does someone wants this thread stopped?
    {
        HELPER_METHOD_FRAME_BEGIN_NOPOLL();    // Set up a frame
#ifdef _DEBUG
        BOOL GCOnTransition = FALSE;
        if (g_pConfig->FastGCStressLevel()) {
            GCOnTransition = GC_ON_TRANSITIONS (FALSE);
        }
#endif
        CommonTripThread();         // Indicate we are at a GC safe point
#ifdef _DEBUG
        if (g_pConfig->FastGCStressLevel()) {
            GC_ON_TRANSITIONS (GCOnTransition);
        }
#endif
        HELPER_METHOD_FRAME_END();
    }
}
HCIMPLEND


/*********************************************************************/
/* we don't use HCIMPL macros because we don't want the overhead even in debug mode */


Object* F_CALL_CONV JIT_CheckObj(Object* obj)
{
    if (obj != 0) {
        MethodTable* pMT = obj->GetMethodTable();
        if (pMT->GetClass()->GetMethodTable() != pMT) {
            _ASSERTE(!"Bad Method Table");
            DebugBreak();
        }
    }
    return obj;
}

#ifdef _MSC_VER
#pragma optimize("",on)
#endif

/*********************************************************************/

//

#ifdef _DEBUG
#define JITHELPER(code, pfnHelper) { code, (LPVOID)pfnHelper, 0, 0 },
#else // !_DEBUG
#define JITHELPER(code, pfnHelper) { (LPVOID)pfnHelper, 0, 0 },
#endif // _DEBUG

VMHELPDEF hlpFuncTable[] =
{
    // pfnHelper is set to NULL if it is a stubbed helper.
    // It will be set in InitJITHelpers2

    JITHELPER(CORINFO_HELP_UNDEF,               JIT_BadHelper               )

    // Arithmetic
    JITHELPER(CORINFO_HELP_LMUL,                JIT_LMul                    )
    JITHELPER(CORINFO_HELP_LMUL_OVF,            JIT_LMulOvf                 )
    JITHELPER(CORINFO_HELP_ULMUL_OVF,           JIT_ULMulOvf                )
    JITHELPER(CORINFO_HELP_LDIV,                JIT_LDiv                    )
    JITHELPER(CORINFO_HELP_LMOD,                JIT_LMod                    )
    JITHELPER(CORINFO_HELP_ULDIV,               JIT_ULDiv                   )
    JITHELPER(CORINFO_HELP_ULMOD,               JIT_ULMod                   )
    JITHELPER(CORINFO_HELP_ULNG2DBL,            JIT_ULng2Dbl                )
    JITHELPER(CORINFO_HELP_DBL2INT,             JIT_Dbl2Lng                 ) // use long version
    JITHELPER(CORINFO_HELP_DBL2INT_OVF,         JIT_Dbl2IntOvf              )
    JITHELPER(CORINFO_HELP_DBL2LNG,             JIT_Dbl2Lng                 )
    JITHELPER(CORINFO_HELP_DBL2LNG_OVF,         JIT_Dbl2LngOvf              )
    JITHELPER(CORINFO_HELP_DBL2UINT,            JIT_Dbl2Lng                 ) // use long version
    JITHELPER(CORINFO_HELP_DBL2UINT_OVF,        JIT_Dbl2UIntOvf             )
    JITHELPER(CORINFO_HELP_DBL2ULNG,            JIT_Dbl2ULng                )
    JITHELPER(CORINFO_HELP_DBL2ULNG_OVF,        JIT_Dbl2ULngOvf             )
    JITHELPER(CORINFO_HELP_FLTREM,              JIT_FltRem                  )
    JITHELPER(CORINFO_HELP_DBLREM,              JIT_DblRem                  )

    // Allocating a new object
    JITHELPER(CORINFO_HELP_NEW_DIRECT,          JIT_New                     )
    JITHELPER(CORINFO_HELP_NEW_CROSSCONTEXT,    JIT_NewCrossContext         )
    JITHELPER(CORINFO_HELP_NEWFAST,             JIT_NewFast                 )
    JITHELPER(CORINFO_HELP_NEWSFAST,            JIT_NewFast                 )
    JITHELPER(CORINFO_HELP_NEWSFAST_ALIGN8,     JIT_NewFast                 )
    JITHELPER(CORINFO_HELP_NEW_SPECIALDIRECT,   JIT_NewSpecial              )
    JITHELPER(CORINFO_HELP_NEWOBJ,              JIT_NewObj                  )
    JITHELPER(CORINFO_HELP_NEWARR_1_DIRECT,     JIT_NewArr1                 )
    JITHELPER(CORINFO_HELP_NEWARR_1_OBJ,        JIT_NewArr1                 )
    JITHELPER(CORINFO_HELP_NEWARR_1_VC,         JIT_NewArr1                 )
    JITHELPER(CORINFO_HELP_NEWARR_1_ALIGN8,     JIT_NewArr1                 )
    JITHELPER(CORINFO_HELP_STRCNS,              JIT_StrCns                  )

    // Object model
    JITHELPER(CORINFO_HELP_INITCLASS,           JIT_InitClass               )
    JITHELPER(CORINFO_HELP_ISINSTANCEOF,        JIT_IsInstanceOf            )
    JITHELPER(CORINFO_HELP_ISINSTANCEOFCLASS,   JIT_IsInstanceOfClass       )
    JITHELPER(CORINFO_HELP_CHKCAST,             JIT_ChkCast                 )
    JITHELPER(CORINFO_HELP_CHKCASTCLASS,        JIT_ChkCastClass            )
    JITHELPER(CORINFO_HELP_BOX,                 JIT_Box                     )
    JITHELPER(CORINFO_HELP_UNBOX,               JIT_Unbox                   )
    JITHELPER(CORINFO_HELP_GETREFANY,           JIT_GetRefAny               )
    JITHELPER(CORINFO_HELP_EnC_RESOLVEVIRTUAL,  JIT_EnCResolveVirtual       )
    JITHELPER(CORINFO_HELP_ARRADDR_ST,          JIT_Stelem_Ref              )
    JITHELPER(CORINFO_HELP_LDELEMA_REF,         JIT_Ldelema_Ref             )

    // Exceptions
    JITHELPER(CORINFO_HELP_THROW,               JIT_Throw                   )
    JITHELPER(CORINFO_HELP_RETHROW,             JIT_Rethrow                 )
    JITHELPER(CORINFO_HELP_USER_BREAKPOINT,     JIT_UserBreakpoint          )
    JITHELPER(CORINFO_HELP_RNGCHKFAIL,          JIT_RngChkFail              )
    JITHELPER(CORINFO_HELP_OVERFLOW,            JIT_Overflow                )
    JITHELPER(CORINFO_HELP_INTERNALTHROW,       JIT_InternalThrow           )
    JITHELPER(CORINFO_HELP_INTERNALTHROW_FROM_HELPER, JIT_InternalThrowFromHelper )
    JITHELPER(CORINFO_HELP_VERIFICATION,        JIT_Verification            )
    JITHELPER(CORINFO_HELP_ENDCATCH,            JIT_EndCatch                )

    // Synchronization
    JITHELPER(CORINFO_HELP_MON_ENTER,           JIT_MonEnter                )
    JITHELPER(CORINFO_HELP_MON_EXIT,            JIT_MonExit                 )
    JITHELPER(CORINFO_HELP_MON_ENTER_STATIC,    JIT_MonEnterStatic          )
    JITHELPER(CORINFO_HELP_MON_EXIT_STATIC,     JIT_MonExitStatic           )

    // GC support
    JITHELPER(CORINFO_HELP_STOP_FOR_GC,         JIT_RareDisableHelper       )
    JITHELPER(CORINFO_HELP_POLL_GC,             JIT_PollGC                  )
    JITHELPER(CORINFO_HELP_STRESS_GC,           JIT_StressGC                )
    JITHELPER(CORINFO_HELP_CHECK_OBJ,           JIT_CheckObj                )

    // GC Write barrier support
    JITHELPER(CORINFO_HELP_ASSIGN_REF,          JIT_WriteBarrier            )
    JITHELPER(CORINFO_HELP_CHECKED_ASSIGN_REF,  JIT_CheckedWriteBarrier     )


    JITHELPER(CORINFO_HELP_ASSIGN_STRUCT,       JIT_StructWriteBarrier      )


    // Accessing fields
    JITHELPER(CORINFO_HELP_GETFIELD32,          JIT_GetField32              )
    JITHELPER(CORINFO_HELP_SETFIELD32,          JIT_SetField32              )
    JITHELPER(CORINFO_HELP_GETFIELD64,          JIT_GetField64              )
    JITHELPER(CORINFO_HELP_SETFIELD64,          JIT_SetField64              )
    JITHELPER(CORINFO_HELP_GETFIELDOBJ,         JIT_GetFieldObj             )
    JITHELPER(CORINFO_HELP_SETFIELDOBJ,         JIT_SetFieldObj             )
    JITHELPER(CORINFO_HELP_GETFIELDSTRUCT,      JIT_GetFieldStruct          )
    JITHELPER(CORINFO_HELP_SETFIELDSTRUCT,      JIT_SetFieldStruct          )
    JITHELPER(CORINFO_HELP_GETFIELDADDR,        JIT_GetFieldAddr            )

    JITHELPER(CORINFO_HELP_GETSTATICFIELDADDR,  JIT_GetStaticFieldAddr      )

    JITHELPER(CORINFO_HELP_GETSHAREDSTATICBASE, NULL                        )

    /* Profiling enter/leave probe addresses */
    JITHELPER(CORINFO_HELP_PROF_FCN_CALL,       NULL                        )
    JITHELPER(CORINFO_HELP_PROF_FCN_RET,        NULL                        )
    JITHELPER(CORINFO_HELP_PROF_FCN_ENTER,      JIT_ProfilerStub            )
    JITHELPER(CORINFO_HELP_PROF_FCN_LEAVE,      JIT_ProfilerStub            )
    JITHELPER(CORINFO_HELP_PROF_FCN_TAILCALL,   JIT_ProfilerStub            )

    // Miscellaneous
    JITHELPER(CORINFO_HELP_PINVOKE_CALLI,       NULL                        )
    JITHELPER(CORINFO_HELP_TAILCALL,            JIT_TailCall                )

    JITHELPER(CORINFO_HELP_GET_THREAD_FIELD_ADDR_PRIMITIVE,     JIT_GetThreadFieldAddr_Primitive )
    JITHELPER(CORINFO_HELP_GET_THREAD_FIELD_ADDR_OBJREF,        JIT_GetThreadFieldAddr_Objref    )

    JITHELPER(CORINFO_HELP_GET_CONTEXT_FIELD_ADDR_PRIMITIVE,    JIT_GetContextFieldAddr_Primitive )
    JITHELPER(CORINFO_HELP_GET_CONTEXT_FIELD_ADDR_OBJREF,       JIT_GetContextFieldAddr_Objref    )

    JITHELPER(CORINFO_HELP_NOTANUMBER,          NULL                        )

    JITHELPER(CORINFO_HELP_SEC_UNMGDCODE_EXCPT, JIT_SecurityUnmanagedCodeException )

    JITHELPER(CORINFO_HELP_GET_THREAD,          NULL                        )

    JITHELPER(CORINFO_HELP_MEMSET,              NULL                        )
    JITHELPER(CORINFO_HELP_MEMCPY,              NULL                        )

    // Generics
    JITHELPER(CORINFO_HELP_RUNTIMEHANDLE,       JIT_RuntimeHandle           )
    JITHELPER(CORINFO_HELP_GENERICVIRTUAL,      JIT_GenericVirtual          )

#if !CPU_HAS_FP_SUPPORT || defined(_IA64_)
    //
    // Some architectures need helpers for FP support
    //
    // Keep these at the end of the enum
    //
    JITHELPER(CORINFO_HELP_R4_NEG,              NULL                        )
    JITHELPER(CORINFO_HELP_R8_NEG,              NULL                        )

    JITHELPER(CORINFO_HELP_R4_ADD,              NULL                        )
    JITHELPER(CORINFO_HELP_R8_ADD,              NULL                        )
    JITHELPER(CORINFO_HELP_R4_SUB,              NULL                        )
    JITHELPER(CORINFO_HELP_R8_SUB,              NULL                        )
    JITHELPER(CORINFO_HELP_R4_MUL,              JIT_FloatMul                )
    JITHELPER(CORINFO_HELP_R8_MUL,              NULL                        )
    JITHELPER(CORINFO_HELP_R4_DIV,              JIT_FloatDiv                )
    JITHELPER(CORINFO_HELP_R8_DIV,              JIT_DoubleDiv               )

    JITHELPER(CORINFO_HELP_R4_EQ,               NULL                        )
    JITHELPER(CORINFO_HELP_R8_EQ,               NULL                        )
    JITHELPER(CORINFO_HELP_R4_NE,               NULL                        )
    JITHELPER(CORINFO_HELP_R8_NE,               NULL                        )
    JITHELPER(CORINFO_HELP_R4_LT,               NULL                        )
    JITHELPER(CORINFO_HELP_R8_LT,               NULL                        )
    JITHELPER(CORINFO_HELP_R4_LE,               NULL                        )
    JITHELPER(CORINFO_HELP_R8_LE,               NULL                        )
    JITHELPER(CORINFO_HELP_R4_GE,               NULL                        )
    JITHELPER(CORINFO_HELP_R8_GE,               NULL                        )
    JITHELPER(CORINFO_HELP_R4_GT,               NULL                        )
    JITHELPER(CORINFO_HELP_R8_GT,               NULL                        )

    JITHELPER(CORINFO_HELP_R8_TO_I4,            NULL                        )
    JITHELPER(CORINFO_HELP_R8_TO_I8,            NULL                        )
    JITHELPER(CORINFO_HELP_R8_TO_R4,            NULL                        )

    JITHELPER(CORINFO_HELP_R4_TO_I4,            NULL                        )
    JITHELPER(CORINFO_HELP_R4_TO_I8,            NULL                        )
    JITHELPER(CORINFO_HELP_R4_TO_R8,            NULL                        )

    JITHELPER(CORINFO_HELP_I4_TO_R4,            NULL                        )
    JITHELPER(CORINFO_HELP_I4_TO_R8,            NULL                        )
    JITHELPER(CORINFO_HELP_I8_TO_R4,            NULL                        )
    JITHELPER(CORINFO_HELP_I8_TO_R8,            NULL                        )
    JITHELPER(CORINFO_HELP_U4_TO_R4,            NULL                        )
    JITHELPER(CORINFO_HELP_U4_TO_R8,            NULL                        )
    JITHELPER(CORINFO_HELP_U8_TO_R4,            NULL                        )
    JITHELPER(CORINFO_HELP_U8_TO_R8,            NULL                        )
#endif // CPU_HAS_FP_SUPPORT
};

VMHELPDEF utilFuncTable[] =
{
    JITHELPER((CorInfoHelpFunc)JIT_UTIL_MON_ENTER,          JITutil_MonEnter        )
    JITHELPER((CorInfoHelpFunc)JIT_UTIL_MON_TRY_ENTER,      JITutil_MonTryEnter     )
    JITHELPER((CorInfoHelpFunc)JIT_UTIL_MON_ENTER_STATIC,   JITutil_MonEnterStatic  )
    JITHELPER((CorInfoHelpFunc)JIT_UTIL_MON_EXIT,           JITutil_MonExit         )
    JITHELPER((CorInfoHelpFunc)JIT_UTIL_MON_CONTENTION,     JITutil_MonContention   )
    JITHELPER((CorInfoHelpFunc)JIT_UTIL_CHKCASTBIZARRE,     JITutil_ChkCastBizarre  )
    JITHELPER((CorInfoHelpFunc)JIT_UTIL_ISINSTANCEBIZARRE,  JITutil_IsInstanceOfBizarre )
};

#undef JITHELPER

#ifdef PROFILING_SUPPORTED
HRESULT ProfToEEInterfaceImpl::SetEnterLeaveFunctionHooksForJit(FunctionEnter *pFuncEnter,
                                                                FunctionLeave *pFuncLeave,
                                                                FunctionTailcall *pFuncTailcall)
{
    if (pFuncEnter)
        hlpFuncTable[CORINFO_HELP_PROF_FCN_ENTER].pfnHelper = (void *) pFuncEnter;

    if (pFuncLeave)
        hlpFuncTable[CORINFO_HELP_PROF_FCN_LEAVE].pfnHelper = (void *) pFuncLeave;

    if (pFuncTailcall)
        hlpFuncTable[CORINFO_HELP_PROF_FCN_TAILCALL].pfnHelper = (void *) pFuncTailcall;

    return (S_OK);
}
#endif // PROFILING_SUPPORTED

BOOL ObjIsInstanceOf(Object *pElement, TypeHandle toTypeHnd)
{
    _ASSERTE(pElement != NULL);

    BOOL       fCast = FALSE;
    TypeHandle ElemTypeHnd = pElement->GetTypeHandle();

    // Some of the pathways through here can trigger a GC.  But it is required that
    // those callers which don't involve odd cases like COM and remoting do not.
    //
    // There is code in JIT_Stelem_Ref that relies on this.

    // Start by doing a quick static cast check to see if the type information captured in
    // the metadata indicates that the cast is legal.
    if (!ElemTypeHnd.GetMethodTable()->IsThunking())
    {
        fCast = ElemTypeHnd.CanCastTo(toTypeHnd);
        if (fCast)
            return(fCast);
    }

    // If we are trying to store a proxy in the array we need to delegate to remoting
    // services which will determine whether the proxy and the array element
    // type are compatible.
    if(ElemTypeHnd.GetMethodTable()->IsThunking())
    {
        _ASSERTE(CRemotingServices::IsTransparentProxy(pElement));
        fCast = CRemotingServices::CheckCast(ObjectToOBJECTREF(pElement), toTypeHnd);
        if (fCast)
            return(fCast);
    }


    return(fCast);
}

#ifdef _MSC_VER
#pragma optimize("",on)
#endif

HRESULT LoadHelperTable(Module *currentModule,
                        void **table,
                        SIZE_T tableSize)
{
    void **value = table;
    void **valueEnd = (void **) (((BYTE*)table) + tableSize);

    _ASSERTE(valueEnd <= value + CORINFO_HELP_COUNT);

    //
    // Fill in helpers
    //

    VMHELPDEF *hlpFunc = hlpFuncTable;

    while (value < valueEnd)
        *value++ = hlpFunc++->pfnHelper;

    return S_OK;
}

/*********************************************************************/
// Initialize the part of the JIT helpers that require much of the
// EE infrastructure to be in place.
/*********************************************************************/
extern ECFunc  gStringBufferFuncs[];

BOOL InitJITHelpers2()
{

#ifdef _X86_
#ifdef PROFILING_SUPPORTED
    if (!CORProfilerTrackAllocationsEnabled())
#endif
    {
        JIT_TrialAlloc::Flags flags = JIT_TrialAlloc::ALIGN8;
        if (g_SystemInfo.dwNumberOfProcessors != 1)
            flags = JIT_TrialAlloc::Flags(flags | JIT_TrialAlloc::MP_ALLOCATOR);
        hlpFuncTable[CORINFO_HELP_NEWARR_1_ALIGN8].pfnHelper
          = JIT_TrialAlloc::GenAllocArray(flags);
    }
#endif // _X86_

    _ASSERTE(gStringBufferFuncs[0].m_pImplementation == COMStringBuffer::GetCurrentThread 
        || !"Please keep InternalGetCurrentThread first");
    _ASSERTE(GetThread != NULL);
    gStringBufferFuncs[0].m_pImplementation = (LPVOID) GetThread;

#if defined(ENABLE_PERF_COUNTERS)
    g_lastTimeInJitCompilation.QuadPart = 0;
#endif

    return TRUE;
}

/*********************************************************************/
void* __stdcall CEEInfo::getHelperFtn (CorInfoHelpFunc ftnNum,
                                       void **ppIndirection)
{
    CANNOTTHROWCOMPLUSEXCEPTION();

    if (ppIndirection != NULL)
        *ppIndirection = NULL;

    return(getJitHelper(ftnNum));
}

/*********************************************************************/
void* getJitHelper(CorInfoHelpFunc ftnNum)
{
    _ASSERTE((unsigned) ftnNum < sizeof(hlpFuncTable) / sizeof(VMHELPDEF));
    _ASSERTE(hlpFuncTable[ftnNum].code == ftnNum);
    _ASSERTE(hlpFuncTable[ftnNum].pfnHelper);
    return(hlpFuncTable[ftnNum].pfnHelper);
}

/*********************************************************************/
HCIMPL2(INT64, JIT_LMul, INT64 val2, INT64 val1)
{
    // Special case 0
    if ((val1 == 0) || (val2 == 0))
    {
        _ASSERT(0 == (val1 * val2));
        return 0;
    }

    return (val1 * val2);
}
HCIMPLEND

/*********************************************************************/
HCIMPL2(INT64, JIT_ULMul, UINT64 val2, UINT64 val1)
{
    // Special case 0
    if ((val1 == 0) || (val2 == 0))
    {
        _ASSERT(0 == (val1 * val2));
        return 0;
    }

    return (val1 * val2);
}
HCIMPLEND

/*********************************************************************/
HCIMPL2(INT64, JIT_LMulOvf, INT64 val2, INT64 val1)
{
    THROWSCOMPLUSEXCEPTION();

    __int64 ret = val1 * val2;

    unsigned __int64 uval1 = (val1 < 0) ? -val1 : val1;
    unsigned __int64 uval2 = (val2 < 0) ? -val2 : val2;

        // compute result as unsigned
    __int64 uret  = uval1 * uval2;

        // Get the upper 32 bits of the numbers
    unsigned __int64 uval1High = (unsigned) (uval1 >> 32);
    unsigned __int64 uval2High = (unsigned) (uval2 >> 32);

        // Compute the 'middle' bits of the long multiplication
    unsigned __int64 uvalMid =
        (uval1High * (unsigned) uval2 + uval2High * (unsigned) uval1) +
        ((((unsigned __int64) (unsigned) uval1) * (unsigned) uval2) >> 32);

        // See if any bits after bit 63 are set
    if (uval1High * uval2High != 0 || (uvalMid >> 32) != 0)
        goto THROW;

        // have we spilled into the sign bit?
    if (uret < 0) {
            // MIN_INT is a special case, we did not overflow the sign as
            // long as the signs of the original args are different
        if (!((UINT64)uret == UI64(0x8000000000000000) && (val1 < 0) != (val2 < 0)))
            goto THROW;
    }
    return(ret);

THROW:
    FCThrow(kOverflowException);
}
HCIMPLEND

#ifdef _MSC_VER
#pragma optimize("t", on)
#endif

/*********************************************************************/
HCIMPL2(INT64, JIT_ULMulOvf, UINT64 val2, UINT64 val1)
{
        // Get the upper 32 bits of the numbers
    unsigned __int64 val1High = (unsigned) (val1 >> 32);
    unsigned __int64 val2High = (unsigned) (val2 >> 32);

        // Compute the 'middle' bits of the long multiplication
    unsigned __int64 valMid =
        (val1High * (unsigned) val2 + val2High * (unsigned) val1) +
        ((((unsigned __int64) (unsigned) val1) * (unsigned) val2) >> 32);

        // See if any bits after bit 63 are set
    if (val1High * val2High != 0 || (valMid >> 32) != 0) {
        FCThrow(kOverflowException);
    }

    return(val1 * val2);
}
HCIMPLEND

/*********************************************************************/
HCIMPL2(INT64, JIT_LDiv, INT64 divisor, INT64 dividend)
{
    RuntimeExceptionKind ehKind;
    if (divisor != 0)
    {
        if (divisor != -1) 
        {
            // Check for -ive or +ive numbers in the range -2**31 to 2**31
            if (((int)dividend == dividend) && ((int)divisor == divisor))
                return((int)dividend / (int)divisor);
            // For all other combinations fallback to int64 div.
            else
                return(dividend / divisor);
        }
        else 
        {
            if ((UINT64) dividend == UI64(0x8000000000000000))
            {
                ehKind = kOverflowException;
                goto ThrowExcep;
            }
            return -dividend;
        }
    }
    else
    {
        ehKind = kDivideByZeroException;
        goto ThrowExcep;
    }

ThrowExcep:
    FCThrow(ehKind);
}
HCIMPLEND

/*********************************************************************/
HCIMPL2(INT64, JIT_LMod, INT64 divisor, INT64 dividend)
{
    RuntimeExceptionKind ehKind;
    if (divisor != 0)
    {
        if (divisor != -1) 
        {
            // Check for -ive or +ive numbers in the range -2**31 to 2**31
            if (((int)dividend == dividend) && ((int)divisor == divisor))
                return((int)dividend % (int)divisor);
            // For all other combinations fallback to int64 div.
            else
                return(dividend % divisor);
        }
        else 
        {
            if ((UINT64) dividend == UI64(0x8000000000000000))
            {
                ehKind = kOverflowException;
                goto ThrowExcep;
            }
            return 0;
        }
    }
    else
    {
        ehKind = kDivideByZeroException;
        goto ThrowExcep;
    }

ThrowExcep:
    FCThrow(ehKind);
}
HCIMPLEND

/*********************************************************************/
HCIMPL2(UINT64, JIT_ULDiv, UINT64 divisor, UINT64 dividend)
{

    if (divisor != 0)
    {
        if (((dividend & UI64(0xFFFFFFFF00000000)) == 0) && ((divisor & UI64(0xFFFFFFFF00000000)) == 0))
            return((unsigned int)dividend / (unsigned int)divisor);
        else
            return(dividend / divisor);
    }
    else
    {
        FCThrow(kDivideByZeroException);
    }
}
HCIMPLEND

/*********************************************************************/
HCIMPL2(UINT64, JIT_ULMod, UINT64 divisor, UINT64 dividend)
{

    if (divisor != 0)
    {
        if (((dividend & UI64(0xFFFFFFFF00000000)) == 0) && ((divisor & UI64(0xFFFFFFFF00000000)) == 0))
            return((unsigned int)dividend % (unsigned int)divisor);
        else
            return(dividend % divisor);
    }
    else
    {
        FCThrow(kDivideByZeroException);
    }
}
HCIMPLEND

#ifdef _MSC_VER
#pragma optimize("", on)
#endif

/*********************************************************************/
//
static double __stdcall JIT_ULng2Dbl(unsigned __int64 val)
{
    double conv = (double) ((__int64) val);
    if (conv < 0)
        conv += (4294967296.0 * 4294967296.0);  // add 2^64
    _ASSERTE(conv >= 0);
    return(conv);
}


/*********************************************************************/
HCIMPL1(unsigned, JIT_Dbl2UIntOvf, double val)
{
		// Note that this expression also works properly for val = NaN case
    if (val > -1.0 && val < I64(4294967296))
		return((unsigned) JIT_Dbl2Lng(val));

        FCThrow(kOverflowException);
    }
HCIMPLEND

/*********************************************************************/
static unsigned __int64 __stdcall JIT_Dbl2ULng(double val)
{
    const double two63  = 2147483648.0 * 4294967296.0;
    if (val < two63)
        return JIT_Dbl2Lng(val);

        // subtract 0x8000000000000000, do the convert then add it back again
    return (JIT_Dbl2Lng(val - two63) + I64(0x8000000000000000));
}

/*********************************************************************/
HCIMPL1(UINT64, JIT_Dbl2ULngOvf, double val)
{
    const double two64  = 4294967296.0 * 4294967296.0;
		// Note that this expression also works properly for val = NaN case
	if (val > -1.0 && val < two64) {
    UINT64 ret = JIT_Dbl2ULng(val);
#ifdef _DEBUG
    // since no overflow can occur, the value always has to be within 1
    double roundTripVal = JIT_ULng2Dbl(ret);
    _ASSERTE(val - 1.0 <= roundTripVal && roundTripVal <= val + 1.0); 
#endif
		return ret;
	}

    FCThrow(kOverflowException);
}
HCIMPLEND

Object* __cdecl JIT_NewObj(CORINFO_MODULE_HANDLE classHnd, CORINFO_METHOD_HANDLE constrMeth, ...)
{
    THROWSCOMPLUSEXCEPTION();

    INT32* fwdArgList;

    HCIMPL_PROLOG(JIT_NewObj);
    OBJECTREF    ret = 0;
    HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_1(Frame::FRAME_ATTR_RETURNOBJ, ret);    // Set up a frame
    THROWSCOMPLUSEXCEPTION();

    MethodDesc *pMethod = (MethodDesc*) constrMeth;
    Module *pModule = pMethod->GetModule();

    _ASSERTE(!pMethod->IsStatic());

    // Should be using one of the fast new helpers, if you aren't an array
    _ASSERTE(pMethod->GetMethodTable()->IsArray());

    // No instantiation info passed in; number of args is independent of insts
    unsigned dwNumArgs = MetaSig::NumFixedArgs(pModule, pMethod->GetSig());
    _ASSERTE(dwNumArgs > 0);

    va_list dims;
    va_start(dims, constrMeth);

    // create an array where fwdArgList[0] == arg[0] ...
#ifdef _X86_
    fwdArgList = (INT32*) dims;

    {
    // reverse the order
    INT32* p = fwdArgList;
    INT32* q = fwdArgList + (dwNumArgs-1);
    while (p < q)
    {
        INT32 t = *p; *p = *q; *q = t;
        p++; q--;
    }
    }
#elif defined(_PPC_) || defined(_SPARC_)
    fwdArgList = (INT32*) dims;
#else
    fwdArgList = (INT32*) _alloca(dwNumArgs*sizeof(INT32));

    {
    unsigned i;
    for (i = 0; i <dwNumArgs; i++)
    {
        fwdArgList[i] = va_arg(dims, INT32);
    }
    }
#endif

    va_end(dims);
    ret = AllocateArrayEx((TypeHandle) classHnd, fwdArgList, dwNumArgs);

    // exit: ;
    HELPER_METHOD_FRAME_END();
    return (OBJECTREFToObject(ret));
}

/*************************************************************/
HCIMPL2(LPVOID, JIT_GetRefAny, CORINFO_CLASS_HANDLE type, TypedByRef typedByRef)
{
    TypeHandle clsHnd(type);

    if (clsHnd != typedByRef.type) {
        FCThrow(kInvalidCastException);
    }

    return(typedByRef.data);
}
HCIMPLEND

/*************************************************************/
// For an inlined N/Direct call (and possibly for other places that need this service)
// we have noticed that the returning thread should trap for one reason or another.
// ECall sets up the frame.

HCIMPL1(void, JIT_RareDisableHelper, Thread* thread)
{
		// We do this here (before we set up a frame), because the following scenario
		// We are in the process of doing an inlined pinvoke.  Since we are in preemtive
		// mode, the thread is allowed to continue.  The thread continues and gets a context
		// switch just after it has cleared the preemptive mode bit but before it gets
		// to this helper.    When we do our stack crawl now, we think this thread is 
		// in cooperative mode (and believed that it was suspended in the SuspendEE), so
		// we do a getthreadcontext (on the unsuspended thread!) and get an EIP in jitted code.
		// and proceed.   Assume the crawl of jitted frames is proceeding on the other thread
		// when this thread wakes up and sets up a frame.   Eventually the other thread
		// runs out of jitted frames and sees the frame we just established.  This causes
		// and assert in the stack crawling code.  If this assert is ignored, however, we
		// will end up scanning the jitted frames twice, which will lead to GC holes 
		//

	ENDFORBIDGC();
    thread->RareDisablePreemptiveGC();
	BEGINFORBIDGC();

    FC_GC_POLL_NOT_NEEDED();
    HELPER_METHOD_FRAME_BEGIN_NOPOLL();    // Set up a frame
    THROWSCOMPLUSEXCEPTION();
    thread->HandleThreadAbort();
    HELPER_METHOD_FRAME_END();
}
HCIMPLEND


/*************************************************************/
/* the uncommon case for the helper below (allowing enums to be unboxed
   as their underlying type */

LPVOID __fastcall JIT_Unbox_Helper(CORINFO_CLASS_HANDLE type, Object* obj)
{
    TypeHandle typeHnd(type);

    CorElementType type1 = typeHnd.GetNormCorElementType();

    // we allow enums and their primtive type to be interchangable

    MethodTable* pMT2 = obj->GetMethodTable();
    CorElementType type2 = pMT2->GetNormCorElementType();
    if (type1 == type2)  {
	    EEClass* pClass1 = typeHnd.GetClass();
	    EEClass* pClass2 = pMT2->GetClass();
        if (pClass1 && (pClass1->IsEnum() || pClass1->IsTruePrimitive()) &&
            (pClass2->IsEnum() || pClass2->IsTruePrimitive())) {
            _ASSERTE(CorTypeInfo::IsPrimitiveType(type1));
            return(obj->GetData());
        }
    }

    return(0);
}

/*************************************************************/
HCIMPL2(LPVOID, JIT_Unbox, CORINFO_CLASS_HANDLE type, Object* obj)
{
    TypeHandle typeHnd(type);
    VALIDATEOBJECTREF(obj);
    _ASSERTE(typeHnd.IsUnsharedMT());       // value classes are always unshared
    _ASSERTE(typeHnd.AsClass()->GetMethodTable()->GetClass() == typeHnd.AsClass());

        // This has been tuned so that branch predictions are good
        // (fall through for forward branches) for the common case
    RuntimeExceptionKind except;
    if (obj != 0) {
        if (obj->GetMethodTable() == typeHnd.AsMethodTable())
            return(obj->GetData());
        else {
                // Stuff the uncommon case into a helper so that
                // its register needs don't cause spills that effect
                // the common case above.
            LPVOID ret = JIT_Unbox_Helper(type, obj);
            if (ret != 0)
                return(ret);
        }
        except = kInvalidCastException;
    }
    else
        except = kNullReferenceException;

    FCThrow(except);
}
HCIMPLEND

/*************************************************************/
#ifdef _MSC_VER
#pragma optimize("t", on)
#endif

/* returns '&array[idx], after doing all the proper checks */

HCIMPL3(void*, JIT_Ldelema_Ref, PtrArray* array, unsigned idx, CORINFO_CLASS_HANDLE type)
{
    RuntimeExceptionKind except;
       // This has been carefully arranged to insure that in the common
        // case the branches are predicted properly (fall through).
        // and that we dont spill registers unnecessarily etc.
    if (array != 0)
        if (idx < array->GetNumComponents())
            if (array->GetElementTypeHandle() == TypeHandle(type))
                return(&array->m_Array[idx]);
            else
                except = kArrayTypeMismatchException;
        else
            except = kIndexOutOfRangeException;
    else
        except = kNullReferenceException;

    FCThrow(except);
}
HCIMPLEND

#ifdef _MSC_VER
#pragma optimize("", on )                              /* put optimization back */
#endif

/*************************************************************/
HCIMPL2(Object*, JIT_Box, CORINFO_CLASS_HANDLE type, void* unboxedData)
{
    THROWSCOMPLUSEXCEPTION();

    TypeHandle clsHnd(type);

    _ASSERTE(clsHnd.IsUnsharedMT());  // we never use this helper for arrays
        // Sanity test for class
    _ASSERTE(clsHnd.AsClass()->GetMethodTable()->GetClass() == clsHnd.AsClass());
    MethodTable *pMT = clsHnd.AsMethodTable();

    OBJECTREF newobj;
    HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_NOPOLL(Frame::FRAME_ATTR_RETURNOBJ);    // Set up a frame
    GCPROTECT_BEGININTERIOR(unboxedData);
    HELPER_METHOD_POLL();

    pMT->CheckRestore();

    // You can only box things that inherit from valuetype or Enum.
    if (!CanBoxToObject(pMT))
        COMPlusThrow(kInvalidCastException, L"Arg_ObjObj");

#ifdef _DEBUG
    if (g_pConfig->FastGCStressLevel()) {
        GetThread()->DisableStressHeap();
    }
#endif

    newobj = FastAllocateObject(pMT);

    CopyValueClass(newobj->GetData(), unboxedData, pMT, newobj->GetAppDomain());
    GCPROTECT_END();
    HELPER_METHOD_FRAME_END();

    return(OBJECTREFToObject(newobj));
}
HCIMPLEND

/*************************************************************/
HCIMPL1(Object*, JIT_New, CORINFO_CLASS_HANDLE typeHnd_)
{
    TypeHandle typeHnd(typeHnd_);

    OBJECTREF newobj;
    HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_NOPOLL(Frame::FRAME_ATTR_RETURNOBJ);    // Set up a frame
    HELPER_METHOD_POLL();

    THROWSCOMPLUSEXCEPTION();
    _ASSERTE(typeHnd.IsUnsharedMT());                                   // we never use this helper for arrays
    MethodTable *pMT = typeHnd.AsMethodTable();
    pMT->CheckRestore();

#ifdef _DEBUG
    if (g_pConfig->FastGCStressLevel()) {
        GetThread()->DisableStressHeap();
    }
#endif

    newobj = AllocateObject(pMT);

    HELPER_METHOD_FRAME_END();
    return(OBJECTREFToObject(newobj));
}
HCIMPLEND

/*************************************************************/
HCIMPL1(Object*, JIT_NewString, unsigned length)
{
    STRINGREF newStr;
    HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_NOPOLL(Frame::FRAME_ATTR_RETURNOBJ);    // Set up a frame
    HELPER_METHOD_POLL();

    THROWSCOMPLUSEXCEPTION();

#ifdef _DEBUG
    if (g_pConfig->FastGCStressLevel()) {
        GetThread()->DisableStressHeap();
    }
#endif

    newStr = AllocateString(length+1);

    HELPER_METHOD_FRAME_END();
    return(OBJECTREFToObject(newStr));
}
HCIMPLEND

/*************************************************************/
HCIMPL1(Object*, JIT_NewSpecial, CORINFO_CLASS_HANDLE typeHnd_)
{
    TypeHandle typeHnd(typeHnd_);

    OBJECTREF newobj;
    HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_NOPOLL(Frame::FRAME_ATTR_RETURNOBJ);    // Set up a frame
    HELPER_METHOD_POLL();

    THROWSCOMPLUSEXCEPTION();
    _ASSERTE(typeHnd.IsUnsharedMT());                                   // we never use this helper for arrays
    MethodTable *pMT = typeHnd.AsMethodTable();
    pMT->CheckRestore();

#ifdef _DEBUG
    if (g_pConfig->FastGCStressLevel()) {
        GetThread()->DisableStressHeap();
    }
#endif

    newobj = AllocateObjectSpecial(pMT);

    HELPER_METHOD_FRAME_END();
    return(OBJECTREFToObject(newobj));
}
HCIMPLEND

/*************************************************************/
HCIMPL1(Object*, JIT_NewFast, CORINFO_CLASS_HANDLE typeHnd_)
{
    TypeHandle typeHnd(typeHnd_);

    OBJECTREF newobj;
    HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_NOPOLL(Frame::FRAME_ATTR_RETURNOBJ);    // Set up a frame
    HELPER_METHOD_POLL();

    THROWSCOMPLUSEXCEPTION();
    _ASSERTE(typeHnd.IsUnsharedMT());                                   // we never use this helper for arrays
    MethodTable *pMT = typeHnd.AsMethodTable();

    // Don't bother to restore the method table; assume that the prestub of the
    // constructor will do that check.

#ifdef _DEBUG
    if (g_pConfig->FastGCStressLevel()) {
        GetThread()->DisableStressHeap();
    }
#endif

    //
    // HACK HACK
    //
    _ASSERTE(!(pMT->IsComObjectType()));
    //
    // END HACK
    //
    {
        newobj = FastAllocateObject(pMT);
    }

    HELPER_METHOD_FRAME_END();
    return(OBJECTREFToObject(newobj));
}
HCIMPLEND

/*************************************************************/
HCIMPL2(Object*, JIT_NewArr1, CORINFO_CLASS_HANDLE typeHnd_, INT32 size)
{
    THROWSCOMPLUSEXCEPTION();

    TypeHandle typeHnd(typeHnd_);

    OBJECTREF newArray;
    HELPER_METHOD_FRAME_BEGIN_RET_ATTRIB_NOPOLL(Frame::FRAME_ATTR_RETURNOBJ);    // Set up a frame
    HELPER_METHOD_POLL();

    THROWSCOMPLUSEXCEPTION();
    _ASSERTE(typeHnd.GetNormCorElementType() == ELEMENT_TYPE_SZARRAY);
    ArrayTypeDesc* pArrayClassRef = typeHnd.AsArray();

    if (size < 0)
        COMPlusThrow(kOverflowException);

        // is this a primitive type?
    if (CorTypeInfo::IsPrimitiveType(pArrayClassRef->GetElementTypeHandle().GetSigCorElementType())) {
#ifdef _DEBUG
        if (g_pConfig->FastGCStressLevel()) {
            GetThread()->DisableStressHeap();
        }
#endif

        newArray = FastAllocatePrimitiveArray(pArrayClassRef->GetMethodTable(), size);
    }
    else
    {
        // call class init if necessary
        OBJECTREF Throwable;
        if (pArrayClassRef->GetMethodTable()->CheckRunClassInit(&Throwable) == FALSE)
            COMPlusThrow(Throwable);
#ifdef _DEBUG
        if (g_pConfig->FastGCStressLevel()) {
            GetThread()->DisableStressHeap();
        }
#endif
        newArray = AllocateArrayEx(typeHnd, &size, 1);
    }

    HELPER_METHOD_FRAME_END();
    return(OBJECTREFToObject(newArray));
}
HCIMPLEND

#ifdef _MSC_VER
#pragma optimize("t", on)
#endif

/*********************************************************************/
/* Erect a frame, call the class initializer, and tear the frame down
   Must ONLY be called directly from a FCALL or HCALL, and the
   epilog from it must be simple (can be insured by */

HCIMPL1(void, JIT_InitClass_Framed, MethodTable* pMT)
    THROWSCOMPLUSEXCEPTION();

    OBJECTREF throwable = 0;
    HELPER_METHOD_FRAME_BEGIN_1(throwable); 
    pMT->CheckRestore();
    if (pMT->CheckRunClassInit(&throwable) == FALSE)
        COMPlusThrow(throwable);
    HELPER_METHOD_FRAME_END(); 
HCIMPLEND

/*************************************************************/
HCIMPL1(void, JIT_InitClass, CORINFO_CLASS_HANDLE typeHnd_)
    //
    // Fast check to see if our method table bits are set
    //

    TypeHandle typeHnd(typeHnd_);
    MethodTable *pMT = typeHnd.AsMethodTable();
    if (pMT->IsRestoredAndClassInited())
        return;

    // 
    // Slower check for shared code case as well
    //

    if (pMT->IsShared() && pMT->IsRestored())
    {
        SIZE_T index = pMT->GetSharedClassIndex();
        DomainLocalBlock *pLocalBlock = GetAppDomain()->GetDomainLocalBlock();
        if (pLocalBlock->IsClassInitialized(index))
            return;
    }

    ENDFORBIDGC();
    //  
    // Don't worry about speed so much now as we've got a .cctor to run.
    //
    JIT_InitClass_Framed(pMT);
HCIMPLEND


HCIMPL2(double, JIT_FloatDiv, double a, double b)
    return b / a;
HCIMPLEND

HCIMPL2(double, JIT_DoubleDiv, double a, double b)
    return b / a;
HCIMPLEND

HCIMPL2(double, JIT_FloatMul, double a, double b)
    return b * a;
HCIMPLEND

//*****************************************************************************
EECodeInfo::EECodeInfo(METHODTOKEN token, IJitManager * pJM)
: m_methodToken(token), m_pMD(pJM->JitTokenToMethodDesc(token)), m_pJM(pJM)
{
}


EECodeInfo::EECodeInfo(METHODTOKEN token, IJitManager * pJM, MethodDesc *pMD)
: m_methodToken(token), m_pMD(pMD), m_pJM(pJM)
{
}

BYTE EECodeInfo::s_ceeInfoMemory[sizeof(CEEInfo)];

BOOL EECodeInfo::Init()
{
    // construct static CEEInfo
    new (s_ceeInfoMemory) CEEInfo();
    return TRUE;
}

const char* __stdcall EECodeInfo::getMethodName(const char **moduleName /* OUT */ )
{
    return GetCEEInfo()->getMethodName((CORINFO_METHOD_HANDLE)m_pMD, moduleName);
}

DWORD       __stdcall EECodeInfo::getMethodAttribs()
{
    return GetCEEInfo()->getMethodAttribs((CORINFO_METHOD_HANDLE)m_pMD,(CORINFO_METHOD_HANDLE)m_pMD);
}

DWORD       __stdcall EECodeInfo::getClassAttribs()
{
    CORINFO_CLASS_HANDLE clsHnd = GetCEEInfo()->getMethodClass((CORINFO_METHOD_HANDLE)m_pMD);
    return GetCEEInfo()->getClassAttribs(clsHnd,(CORINFO_METHOD_HANDLE)m_pMD);
}

void        __stdcall EECodeInfo::getMethodSig(CORINFO_SIG_INFO *sig /* OUT */ )
{
    GetCEEInfo()->getMethodSig((CORINFO_METHOD_HANDLE)m_pMD, sig);
}

LPVOID      __stdcall EECodeInfo::getStartAddress()
{
    CANNOTTHROWCOMPLUSEXCEPTION();

    return m_pJM->JitToken2StartAddress(m_methodToken);
}

// This is a helper used by IsInstanceOfClass.  This is only called when
// the instance has already been determined to be a proxy.  Also, the EEClass
// must refer to a class (not an interface or array).
Object* __stdcall JIT_IsInstanceOfClassWorker(OBJECTREF objref, MethodTable *pTargetMT, BOOL bThrow)
{
    HCIMPL_PROLOG(JIT_IsInstanceOfClassWorker);   // just make certain we don't do any GC's in here

    MethodTable *pMT = objref->GetMethodTable();
    _ASSERTE(pMT->IsThunking());
    _ASSERTE(!pTargetMT->IsInterface());

    // Check whether the type represented by the proxy can be
    // cast to the given type;
    HELPER_METHOD_FRAME_BEGIN_RET_1(objref);
    pMT->GetClass()->CheckRestore();
    if (!CRemotingServices::CheckCast(objref, pTargetMT))
	objref = 0;
    HELPER_METHOD_FRAME_END();
    if (objref == 0 && bThrow)
        FCThrow(kInvalidCastException);
    return OBJECTREFToObject(objref);
}


// Helper used by JIT_NewCrossContext
Object* __stdcall JIT_NewCrossContextHelper(CORINFO_CLASS_HANDLE typeHnd_)
{
    HCIMPL_PROLOG(JIT_NewCrossContextHelper);   // just make certain we don't do any GC's in here
    TypeHandle typeHnd(typeHnd_);

    OBJECTREF newobj;
    HELPER_METHOD_FRAME_BEGIN_RET_0();    // Set up a frame
    __helperframe.SetFrameAttribs(Frame::FRAME_ATTR_RETURNOBJ);

    THROWSCOMPLUSEXCEPTION();
    _ASSERTE(typeHnd.IsUnsharedMT());                                   // we never use this helper for arrays
    MethodTable *pMT = typeHnd.AsMethodTable();
    pMT->CheckRestore();

    // Remoting services determines if the current context is appropriate
    // for activation. If the current context is OK then it creates an object
    // else it creates a proxy.
    newobj = CRemotingServices::CreateProxyOrObject(pMT);

    HELPER_METHOD_FRAME_END();
    return(OBJECTREFToObject(newobj));
}

// Helper used by JIT_NewCrossContext
Object * __stdcall AllocObjectWrapper(MethodTable *pMT)
{
#ifdef PROFILING_SUPPORTED
    _ASSERTE(!CORProfilerTrackAllocationsEnabled());
#endif
    HCIMPL_PROLOG(AllocObjectWrapper);
    OBJECTREF newObj;
    HELPER_METHOD_FRAME_BEGIN_RET_0();    // Set up a frame
    __helperframe.SetFrameAttribs(Frame::FRAME_ATTR_RETURNOBJ);
    newObj = FastAllocateObject(pMT);
    HELPER_METHOD_FRAME_END();
    return OBJECTREFToObject(newObj);
}

/****************************************************************************/
/* assigns 'val to 'array[idx], after doing all the proper checks */


HCIMPL3(void, JIT_Stelem_Ref, PtrArray* array, unsigned idx, Object *val)
    if (!array) {
        FCThrowVoid(kNullReferenceException);
    }
    if (idx >= array->GetNumComponents()) {
        FCThrowVoid(kIndexOutOfRangeException);
    }

    if (val) {
        TypeHandle Arrayth = array->GetElementTypeHandle();
        MethodTable *ArrayMT = Arrayth.AsMethodTable();
        MethodTable *valMT = NULL;
#if CHECK_APP_DOMAIN_LEAKS
        // If the instance is agile or check agile
        if (Arrayth.IsUnsharedMT()) {
            EEClass *pEEClass = ArrayMT->GetClass();
            if (pEEClass->IsAppDomainAgile() || pEEClass->IsCheckAppDomainAgile()) {
                // Jump to the generic case so we can do an app domain check
                goto IsComObject;
            }
        }
#endif

        valMT = val->GetMethodTable();
        if (valMT != ArrayMT && ArrayMT != g_pObjectClass) {

            // Not exact match and not assigning to Array of objects
            // See if object being assigned is a com object, if it is we 
            // have to erect a frame (ug)
            if (valMT->IsCtxProxyType() ||
                valMT->IsComObjectType() ||
                valMT->IsArray() ||
                valMT->IsTransparentProxyType()) {

                HELPER_METHOD_FRAME_BEGIN_2(val, array);

                // This is equivalent to ArrayStoreCheck(&val, &array);
#ifdef STRESS_HEAP
                // Force a GC on every jit if the stress level is high enough
                if (g_pConfig->GetGCStressLevel() != 0
#ifdef _DEBUG
                    && !g_pConfig->FastGCStressLevel()
#endif
                   )
                    g_pGCHeap->StressHeap();
#endif

#if CHECK_APP_DOMAIN_LEAKS
                if (g_pConfig->AppDomainLeaks())
                    val->AssignAppDomain(array->GetAppDomain());
#endif

                if (!ObjIsInstanceOf(val, array->GetElementTypeHandle())) {
                    COMPlusThrow(kArrayTypeMismatchException);
                }

                HELPER_METHOD_FRAME_END();

            } else {
#if CHECK_APP_DOMAIN_LEAKS
IsComObject:
#endif
                BOOL b;

                BEGINFORBIDGC();
                b = ObjIsInstanceOf(val, Arrayth);
                ENDFORBIDGC();
                if (!b) {
                    FCThrowVoid(kArrayTypeMismatchException);
                }
            }
        }
    }

    SetObjectReferenceUnchecked(&array->m_Array[idx], ObjectToOBJECTREF(val));
HCIMPLEND


HCIMPL2(Object*, JIT_IsInstanceOf, CORINFO_CLASS_HANDLE type, Object *Obj)
    if (Obj == NULL) {
        return Obj;
    }
    unsigned Num = Obj->GetMethodTable()->GetNumInterfaces();
    if (Num) {
        InterfaceInfo_t *pInterface = Obj->GetMethodTable()->GetInterfaceMap();
        InterfaceInfo_t *pInterfaceEnd = pInterface + Num;
        TypeHandle th(type);
        MethodTable *pMT = th.AsMethodTable();
        while (pInterface < pInterfaceEnd) {
            if (pInterface->m_pMethodTable == pMT) {
                // found
                return Obj;
            }
            pInterface++;
        }
    }

    ENDFORBIDGC();
    return JITutil_IsInstanceOfBizarre(type, Obj);
HCIMPLEND

Object* F_CALL_CONV JIT_IsInstanceOfClassHelper(MethodTable* pMT, Object* pObject, BOOL bThrow)
{
    HCIMPL_PROLOG(JIT_IsInstanceOfClassHelper); // jit make certain we don't do any GC's in here

    //
    // make certain common cases are already taken care of
    //
    _ASSERTE(NULL != pObject);
    _ASSERTE(pObject->GetMethodTable() != pMT);

    //
    // search for parent-class relatikonship
    //
    EEClass* pCls1 = pMT->GetClass();
    EEClass* pCls2 = pObject->GetMethodTable()->GetClass();

    while (pCls2)
    {
        pCls2 = pCls2->GetParentClass();
        if (pCls1 == pCls2)
        {
            return pObject;
        }
    }

    //
    // check to see if it's a proxy
    //
    if (pObject->GetMethodTable()->IsCtxProxyType() || pObject->GetMethodTable()->IsTransparentProxyType())
    {
        ENDFORBIDGC();
        return JIT_IsInstanceOfClassWorker(ObjectToOBJECTREF(pObject), pMT, bThrow);
    }

    if (bThrow)
    {
        FCThrow(kInvalidCastException);
    }

    return NULL;
}

HCIMPL2(Object*, JIT_IsInstanceOfClass, MethodTable *pMT, Object *pObject)
    if (pObject == NULL) {
        return NULL;
    }
    if (pMT == pObject->GetMethodTable()) {
        return pObject;
    }

    ENDFORBIDGC();
    return JIT_IsInstanceOfClassHelper(pMT, pObject, FALSE);
HCIMPLEND


HCIMPL2(Object*, JIT_ChkCastClass, MethodTable* pMT, Object* pObject)
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

    ENDFORBIDGC();
    return JIT_IsInstanceOfClassHelper(pMT, pObject, TRUE);
HCIMPLEND

HCIMPL2(Object*, JIT_ChkCast, CORINFO_CLASS_HANDLE type, Object* pObject)
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

    ENDFORBIDGC();
    return JITutil_ChkCastBizarre(type, pObject);
HCIMPLEND

#if !_X86_
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
#endif  // !_X86_

/*********************************************************************/
// This is a frameless helper for entering a monitor on a object.
// This tries the normal case (no blocking or object allocation) in 
// line and calls a framed helper for the other cases.
FCIMPL1(void, JIT_MonEnter, Object *pObject)
    if (pObject == NULL) {
        FCThrowVoid(kArgumentNullException);
    }

#ifdef MON_DEBUG
    //       CtxProxy does not exist anymore.
    if (pObject->GetMethodTable()->IsCtxProxyType() ||
        pObject->GetMethodTable()->GetClass()->IsValueClass()) {
        // ProxyInst
        // ValueClass
        // Can't synchronize on value classes or proxy
        FCThrowVoid(kArgumentException);
    }
#endif // MON_DEBUG

    // Get the sync block index.
    DWORD SyncBlockIndex;
    ObjHeader *pObjHeader;
    AwareLock *pMonitor = NULL;
    SyncBlock *pSyncBlock = NULL;
    Thread *pThread = NULL;

    pObjHeader = (ObjHeader *)((PBYTE)pObject - sizeof(ObjHeader));
    SyncBlockIndex = pObjHeader->GetBits();
    if ( SyncBlockIndex & ~MASK_SYNCBLOCKINDEX) {
        // if there's an appdomain stored in the m_SyncBlockValue, we
        // definately need to create a syncblock
        if ( SyncBlockIndex & BIT_SBLK_IS_APPDOMAIN_IDX) {
            // CreateSyncBlk
            goto SLOW;
        }

        // Otherwise, just and out the top bits and grab the syncblock index
        SyncBlockIndex &= MASK_SYNCBLOCKINDEX;
    }

    // Get the sync block pointer.
    pSyncBlock = g_pSyncTable[SyncBlockIndex].m_SyncBlock;
    // Check if the sync block has been allocated.
    if (!pSyncBlock) {
        // CreateSyncBlock
        goto SLOW;
    }

    // Get a pointer to the lock object.
    pMonitor = pSyncBlock->QuickGetMonitor();

    // Attempt to acquire the lock.
    do {
        if (pMonitor->m_MonitorHeld) {
            // HaveWaiters
            // It's possible to get here with waiters but no lock held, but in this
            // case a signal is about to be fired which will wake up a waiter. So
            // for fairness sake we should wait too.
            // Check first for recursive lock attempts on the same thread.

            Thread *pThread = GetThread();

            // Is mutex already owned by current thread?
            if (pMonitor->m_HoldingThread != pThread) {
                // PrepareToWait
                // Note, on multiprocessor, here is a good place to spin
                // before calling JIT_UTIL_MON_CONTENTION
                goto SLOW;
            }
            // Yes, bump our use count.
            pMonitor->m_Recursion++;
#if defined(MON_DEBUG) && defined(TRACK_SYNC)
            #error "Need to fetch the current function's return address"
            EnterSyncHelper(ReturnAddress, pMonitor);
#endif
            return;
        }
        // Common case, lock isn't held and there are no waiters. Attempt to
        // gain ownership ourselves.
    } while (FastInterlockCompareExchange(&pMonitor->m_MonitorHeld, 1, 0));

    // Success. Save the thread object in the lock and increment the use count.
    pThread = GetThread();
    pMonitor->m_HoldingThread = pThread;
    pThread->m_dwLockCount++;
    pMonitor->m_Recursion++;

#if defined(MON_DEBUG) && defined(TRACK_SYNC)
    #error "Need to fetch the current function's return address"
    EnterSyncHelper(ReturnAddress, pMonitor);
#endif
    return;

SLOW:
    HELPER_METHOD_FRAME_BEGIN_NOPOLL();
    THROWSCOMPLUSEXCEPTION();
    if (pMonitor)
        pMonitor->Contention();
    else
        pObject->EnterObjMonitor();
    HELPER_METHOD_FRAME_END_POLL();
    return;
FCIMPLEND

/***********************************************************************/
// This is a frameless helper for trying to enter a monitor on a object.
// This tries the normal case (no object allocation) in line and calls a 
// framed helper for the other cases.
FCIMPL2(BOOL, JIT_MonTryEnter, Object *pObject, __int32 timeout)
    if (pObject == NULL) {
        FCThrow(kArgumentNullException);
    }
    if (timeout < -1) {
        FCThrow(kArgumentException);
    }

#ifdef MON_DEBUG
    if (pObject->GetMethodTable()->IsCtxProxyType() ||
        pObject->GetMethodTable()->GetClass()->IsValueClass()) {
        // ProxyInst
        // ValueClass
        // Can't synchronize on value classes or proxy
        FCThrow(kArgumentException);
    }
#endif //MON_DEBUG

    // Get the sync block index.
    DWORD SyncBlockIndex;
    ObjHeader *pObjHeader;
    SyncBlock *pSyncBlock = NULL;
    AwareLock *pMonitor = NULL;
    Thread *pThread = NULL;

    pObjHeader = (ObjHeader *)((PBYTE)pObject - sizeof(ObjHeader));
    SyncBlockIndex = pObjHeader->GetBits();
    if ( SyncBlockIndex & ~MASK_SYNCBLOCKINDEX) {
        // if there's an appdomain stored in the m_SyncBlockValue, we
        // definately need to create a syncblock
        if ( SyncBlockIndex & BIT_SBLK_IS_APPDOMAIN_IDX) {
            // CreateSyncBlk
            goto SLOW;
        }

        // Otherwise, just and out the top bits and grab the syncblock index
        SyncBlockIndex &= MASK_SYNCBLOCKINDEX;
    }

    // Get the sync block pointer.
    pSyncBlock = g_pSyncTable[SyncBlockIndex].m_SyncBlock;
    // Check if the sync block has been allocated.
    if (!pSyncBlock) {
        // CreateSyncBlock
        goto SLOW;
    }

    // Get a pointer to the lock object.
    pMonitor = pSyncBlock->QuickGetMonitor();

    // Attempt to acquire the lock.
    do {
        if (pMonitor->m_MonitorHeld) {
        // HaveWaiters
        // It's possible to get here with waiters but no lock held, but in this
        // case a signal is about to be fired which will wake up a waiter. So
        // for fairness sake we should wait too.
        // Check first for recursive lock attempts on the same thread.

            Thread *pThread = GetThread();

            // Is mutex already owned by current thread?
            if (pMonitor->m_HoldingThread != pThread) {
                // WouldBlock
                // We would need to block to enter the section. Return failure if
                // timeout is zero, else call the framed helper to do the blocking
                // form of TryEnter.
                if (timeout ==0) {
                    return FALSE;
                }
                // Block
                goto SLOW;
            }
            // Yes, bump our use count.
            pMonitor->m_Recursion++;
#if defined(MON_DEBUG) && defined(TRACK_SYNC)
            #error "Need to fetch the current function's return address"
            EnterSyncHelper(ReturnAddress, pMonitor);
#endif
            return TRUE;
        }
        // Common case, lock isn't held and there are no waiters. Attempt to
        // gain ownership ourselves.
    } while (FastInterlockCompareExchange(&pMonitor->m_MonitorHeld, 1, 0));

    // Success. Save the thread object in the lock and increment the use count.
    pThread = GetThread();
    pMonitor->m_HoldingThread = pThread;
    pThread->m_dwLockCount++;
    pMonitor->m_Recursion++;

#if defined(MON_DEBUG) && defined(TRACK_SYNC)
    #error "Need to fetch the current function's return address"
    EnterSyncHelper(ReturnAddress, pMonitor);
#endif
    return TRUE;

SLOW:
    BOOL ret;
    HELPER_METHOD_FRAME_BEGIN_RET_NOPOLL();
    THROWSCOMPLUSEXCEPTION();
    OBJECTREF objRef = ObjectToOBJECTREF(pObject);
    ret = objRef->TryEnterObjMonitor(timeout);
    HELPER_METHOD_FRAME_END_POLL();
    return(ret);
FCIMPLEND

/*********************************************************************/
// This is a frameless helper for exiting a monitor on a object.
// This tries the normal case (no blocking or object allocation) in line 
// and calls a framed helper for the other cases.
FCIMPL1(void, JIT_MonExit, Object *pObject)
    if (pObject == NULL) {
        // NullInst
        FCThrowVoid(kArgumentNullException);
    }

    // Get the sync block index and use it to compute the sync block pointer
    DWORD SyncBlockIndex;
    ObjHeader *pObjHeader;

    pObjHeader = (ObjHeader *)((PBYTE)pObject - sizeof(ObjHeader));
    SyncBlockIndex = pObjHeader->GetBits() & MASK_SYNCBLOCKINDEX;
    SyncBlock *pSyncBlock = g_pSyncTable[SyncBlockIndex].m_SyncBlock; 

    // Was there a sync block?
    if (!pSyncBlock) {
        // LockError
        FCThrowVoid(kSynchronizationLockException);
    }

    // Get a pointer to the lock object.
    AwareLock *pMonitor = pSyncBlock->QuickGetMonitor();

    Thread *pThread = GetThread();

    // Check if the lock is held
    if (pMonitor->m_HoldingThread != pThread) {
        // JustLeave
        // There's a strange case where we are waiting to enter a contentious region when
        // a Thread.Interrupt occurs.  The finally protecting the leave will attempt to
        // remove us from a region we never entered.  We don't have to worry about leaving
        // the wrong entry for a recursive case, because recursive cases can never be
        // contentious, so the Thread.Interrupt will never be serviced at that spot.
        return;
    }

#if defined(MON_DEBUG) && defined(TRACK_SYNC)
    #error "Need to fetch the current function's return address"
    LeaveSyncHelper(pMonitor, ReturnAddress);
#endif

    // Reduce our recursion count.
    if ((--pMonitor->m_Recursion) == 0) {
        // LastRecursion
        LONG MonitorHeld;

        pThread->m_dwLockCount--;
        pMonitor->m_HoldingThread = NULL;

        MonitorHeld = (LONG)FastInterlockDecrement(&pMonitor->m_MonitorHeld);
        if (MonitorHeld != -1 && MonitorHeld != 0) {
            // MustSignal
            HELPER_METHOD_FRAME_BEGIN_NOPOLL();
            THROWSCOMPLUSEXCEPTION();
            pMonitor->Signal();
            HELPER_METHOD_FRAME_END_POLL();
        }
    }
    // JustLeave
FCIMPLEND

FCIMPL1(LPVOID, ObjectNative::FastGetClass, Object* vThisRef)
    if (vThisRef == NULL) {
        FCThrow(kNullReferenceException);
    }
    MethodTable* pMT = vThisRef->GetMethodTable();
    EEClass* pClass = pMT->GetClass();

    if (pClass->IsArrayClass() || pClass->IsContextful() || pClass->IsMarshaledByRef()) {
        return NULL;
    }
    // Get the type object
    return OBJECTREFToObject(pMT->GetExistingExposedClassObject());
FCIMPLEND

/*********************************************************************/
// This is a frameless helper for entering a static monitor on a class.
// This tries the normal case (no blocking or object allocation) in 
// line and calls a framed helper for the other cases.
HCIMPL1(VOID, JIT_MonEnterStatic, AwareLock *lock)
    Thread *pThread = GetThread();

    // Attempt to acquire the lock.
    do {
        LONG Held = lock->m_MonitorHeld;
        if (Held) {
            // HaveWaiters
            // It's possible to get here with waiters but no lock held, but in this
            // case a signal is about to be fired which will wake up a waiter. So
            // for fairness sake we should wait too.
            // Check first for recursive lock attempts on the same thread.

            // Is mutex already owned by current thread?
            if (lock->m_HoldingThread != pThread) {
                // We're going to have to wait.  Increment wait count.
                // PrepareToWait
                goto SLOW;
            }
            // Yes, bump our use count.
            lock->m_Recursion++;
#if defined(MON_DEBUG) && defined(TRACK_SYNC)
            #error "Need to fetch the current function's return address"
            EnterSyncHelper(ReturnAddress, lock);
#endif
            return;
        }
        // Common case, lock isn't held and there are no waiters. Attempt to
        // gain ownership ourselves.
    } while (FastInterlockCompareExchange(&lock->m_MonitorHeld, 1, 0));

    // Success. Save the thread object in the lock and increment the use count.
    lock->m_HoldingThread = pThread;
    lock->m_Recursion++;
    pThread->m_dwLockCount++;

#if defined(MON_DEBUG) && defined(TRACK_SYNC)
    #error "Need to fetch the current function's return address"
    EnterSyncHelper(ReturnAddress, lock);
#endif
    return;

SLOW:
    HELPER_METHOD_FRAME_BEGIN_NOPOLL();
    THROWSCOMPLUSEXCEPTION();
    lock->Contention();
    HELPER_METHOD_FRAME_END_POLL();
    return;
HCIMPLEND

/*********************************************************************/
// This is a frameless helper for exiting a monitor on a object.
// This tries the normal case (no blocking or object allocation) in line 
// and calls a framed helper for the other cases.
HCIMPL1(void, JIT_MonExitStatic, AwareLock *lock)

    Thread *pThread = GetThread();

    // Check if the lock is held
    if (lock->m_HoldingThread != pThread) {
        // JustLeave
        // There's a strange case where we are waiting to enter a contentious region when
        // a Thread.Interrupt occurs.  The finally protecting the leave will attempt to
        // remove us from a region we never entered.  We don't have to worry about leaving
        // the wrong entry for a recursive case, because recursive cases can never be
        // contentious, so the Thread.Interrupt will never be serviced at that spot.
        return;
    }

#if defined(MON_DEBUG) && defined(TRACK_SYNC)
    #error "Need to fetch the current function's return address"
    LeaveSyncHelper(lock, ReturnAddress);
#endif

    // Reduce our recursion count.
    if ((--lock->m_Recursion) == 0) {
        // LastRecursion
        LONG NewHeld;

        pThread->m_dwLockCount--;
        lock->m_HoldingThread = NULL;

        NewHeld = FastInterlockDecrement(&lock->m_MonitorHeld);
        if (NewHeld != -1 && NewHeld != 0) {
            // MustSignal
            HELPER_METHOD_FRAME_BEGIN_NOPOLL();
            THROWSCOMPLUSEXCEPTION();
            lock->Signal();
            HELPER_METHOD_FRAME_END_POLL();
        }
    }
    // JustLeave
HCIMPLEND

HCIMPL1(Object*, JIT_NewCrossContext, CORINFO_CLASS_HANDLE typeHnd_)

    // !!!!!!!!!!!!!WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // NOTE: We treat the type handle as a method table
    // If the semantics of the type handle change then we will fail here.
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    MethodTable *pMT = (MethodTable *)typeHnd_;
    ManagedActivationType ActType;
    ActType = CRemotingServices::RequiresManagedActivation(pMT);
    ENDFORBIDGC();
    if (ActType) {
        // SpecialOrXCtxHelper
        return JIT_NewCrossContextHelper(typeHnd_);
    }

#ifdef _DEBUG
    if (LoggingOn(LF_GCALLOC, LL_INFO10)) {
        // AllocWithLogHelper
        return AllocObjectWrapper(pMT);
    }
#endif //_DEBUG

    // if the object doesn't have a finalizer and the size is small, jump to super fast asm helper
    if (pMT->CannotUseSuperFastHelper()) {
        // FastHelper
        return JIT_NewFast(typeHnd_);
    }

    typedef Object * (F_CALL_CONV *pfnNewSFast)(CORINFO_CLASS_HANDLE);
    pfnNewSFast pfn = (pfnNewSFast)hlpFuncTable[CORINFO_HELP_NEWSFAST].pfnHelper;
    return (*pfn)(typeHnd_);
HCIMPLEND

#if !defined(_X86_)
/*********************************************************************/
double __stdcall JIT_DblRem(double divisor, double dividend)
{
    //
    // From the ECMA standard:
    //
    // If [divisor] is zero or [dividend] is infinity
    //   the result is NaN.
 
    if (divisor==0 || !_finite(dividend))
    {
        UINT64 NaN = UI64(0x7fff000000000000);
        return *(double *)(&NaN);
    }
    // else...
    return(fmod(dividend,divisor));
}

float __stdcall JIT_FltRem(float divisor, float dividend)
{
    return JIT_DblRem(divisor, dividend);
}

//*********************************************************************/
// JIT_Dbl2Lng
//
//Purpose:
//   converts a double to a long truncating toward zero (C semantics)
//
//	uses stdcall calling conventions 
//
//   note that changing the rounding mode is very expensive.  This
//   routine basically does the truncation sematics without changing
//   the rounding mode, resulting in a win.
//
__int64 __stdcall JIT_Dbl2Lng(double val)
{
    return((__int64) val);
}

int __stdcall JIT_Dbl2IntOvf(double val)
{
    _ASSERTE(false && "This function is not used in ROTOR");
    return((int) val);
}

__int64 __stdcall JIT_Dbl2LngOvf(double val)
{
    _ASSERTE(false && "This function is not used in ROTOR");
    return((__int64) val);
}

#endif


