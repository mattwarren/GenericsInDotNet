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
//
// method.hpp
//
#ifndef _METHOD_H
#define _METHOD_H

#include "cor.h"
#include "util.hpp"
#include "clsload.hpp"
#include "codeman.h"
#include "class.h"
#include "siginfo.hpp"
#include "declsec.h"
#include "methodimpl.h"
#include <stddef.h>
#include <member-offset-info.h>

class Stub;
class ECallMethodDesc;
class LivePointerInfo;
class FieldDesc;
class NDirect;
class MethodDescChunk;
struct LayoutRawFieldInfo;
struct MLHeader;
class InstantiatedMethodDesc;


//=============================================================
// Splits methoddef token into a 1-byte and 2-byte piece for
// storage inside a methoddesc.
//=============================================================
FORCEINLINE BYTE GetTokenRange(mdToken tok)
{
    return (BYTE)(tok>>16);
}

FORCEINLINE VOID SplitToken(mdToken tok, BYTE *ptokrange, UINT16 *ptokremainder)
{
    *ptokrange = (BYTE)(tok>>16);                  
    *ptokremainder = (UINT16)(tok & 0x0000ffff);
}

FORCEINLINE mdToken MergeToken(BYTE tokrange, UINT16 tokremainder)
{
    return (tokrange << 16) | tokremainder;
}

#define mdUnstoredMethodBits ((DWORD)(nmdReserved1|nmdReserved2))

// The MethodDesc is a union of several types. The following
// 3-bit field determines which type it is. Note that JIT'ed/non-JIT'ed
// is not represented here because this isn't known until the
// method is executed for the first time. Because any thread could
// change this bit, it has to be done in a place where access is
// synchronized.


//#define mdMethodClassificationMask (mdReserved1|mdReserved2|mdReserved3);


// **** NOTE: if you add any new flags, make sure you add them to ClearFlagsOnUpdate
// so that when a method is replaced its relevant flags are updated

// Used in MethodDesc
enum MethodClassification
{
    mcIL        = 0, // IL
    mcECall     = 1, // ECall
    mcNDirect   = 2, // N/Direct
    mcEEImpl    = 3, // special method; implementation provided by EE
    mcArray     = 4, // Array ECall
    mcInstantiated = 5, // Generic methods and their instantiations, including descriptors 
                        // for both shared and unshared code (see InstantiatedMethodDesc)


    mcCount,
};

// All flags in the MethodDesc now reside in a single 16-bit field. In the
// enumeration below, multi-bit fields are represented as a mask together with a
// shift to move the value to bit 0.

// *** NOTE ***
// mdcClassification, mdcNoPrestub, and mdcMethodDesc need to be adjacent and located starting
// at bit 0 for the logic in GetMethodTable to work.
// *** NOTE ***

// @GENERICS: 
// Unfortunately there's no space for a "this is a generic method" flag
// Hence we need to go via the method token and EnumTyPars, which may be slow.
// Probably this doesn't matter because it's only used in asserts and by reflection.
//
// Likewise there's no space for a flag for "this method desc is shared between 
// multiple instantiations", so we instead test the method table owner.
// Could use an illegal combo e.g. mdcStatic | mdcEncNewVirtual as shared-code non-generic methods
// in instantiated types are always non-static and static methods cannot be E&C.
//
enum MethodDescClassification
{
    
    // Method is IL, ECall etc., see MethodClassification above.
    mdcClassification                   = 0x0007,
    mdcClassificationShift              = 0,

    // Method is a body for a method impl (can be used to implement several declarations)
    mdcMethodImpl                       = 0x0008,

    // Method is static
    mdcStatic                           = 0x0010,

    // Is this method elligible for inlining?
    // For ECalls this bit means 'is and FCall' (the native code is not shared)
    mdcInlineEligibility                = 0x0020,

    // Temporary Security Interception.
    // Methods can now be intercepted by security. An intercepted method behaves
    // like it was an interpreted method. The Prestub at the top of the method desc
    // is replaced by an interception stub. Therefore, no back patching will occur.
    // We picked this approach to minimize the number variations given IL and native
    // code with edit and continue. E&C will need to find the real intercepted method
    // and if it is intercepted change the real stub. If E&C is enabled then there
    // is no back patching and needs to fix the pre-stub.
    mdcIntercepted                      = 0x0040,

    // Method requires linktime security checks.
    mdcRequiresLinktimeCheck            = 0x0080,

    // Method requires inheritance security checks.
    // If this bit is set, then this method demands inheritance permissions
    // or a method that this method overrides demands inheritance permissions
    // or both.
    mdcRequiresInheritanceCheck         = 0x0100,

    // The method that this method overrides requires an inheritance security check.
    // This bit is used as an optimization to avoid looking up overridden methods
    // during the inheritance check.
    mdcParentRequiresInheritanceCheck   = 0x0200,

    // Duplicate method.
    mdcDuplicate                        = 0x0400,


    // Has this method been verified?
    mdcVerifiedState                    = 0x1000,

    // Does the method have nonzero security flags?
    mdcHasSecurity                      = 0x2000,

    // Is the method synchronized
    mdcSynchronized                     = 0x4000,

    // IsPrejitted is set if the method has prejitted code
    //
    mdcPrejitted                        = 0x8000

};

extern const unsigned g_ClassificationSizeTable[];

//
// Note: no one is allowed to use this mask outside of method.hpp and method.cpp. Don't make this public! Keep this
// version in sync with the version in the top of method.cpp.
//
#define METHOD_IS_IL_FLAG   0xC0000000

// This is the maximum allowed RVA for a method.
#define METHOD_MAX_RVA      ~0xC0000000

//
// Support of 4gb address space for Unix. The code address is stored shifted in m_CodeOrIL.
// This is safe on the RISC CPUs because of they require the code address to be alligned 
// on 4 byte boundary.
//
#if defined(_PPC_) || defined(_SPARC_)
#define METHOD_IS_IL_SHIFT  2
#else
#define METHOD_IS_IL_SHIFT  0
#endif

//
// For PREPAD_IS_CALLABLE (ie. on x86)
//
//  <3 byte pad> <5 byte call or jmp> <MethodDesc>
//  |                               | |
//   \      METHOD_PREPAD          /   \  MethodDesc fields
//
// The size of this structure needs to be a multiple of 8-bytes
// in 32-bit and a multiple of 16-bytes in 64-bit.  The reason
// 64-bit requires such a large alignment size is that all bundles
// must be 16-byte aligned and we have two bundles that precede
// this class.
//
// The following members guarantee this alignment:
//
//  m_DebugAlignPad
//  m_Align1
//  m_Align2
// 
// If the layout of this struct changes, these need to be 
// revisited to make sure the alignment is maintained.
//
// @GENERICS: 
// Method descriptors for methods belonging to instantiated types may be shared between compatible instantiations
// Hence for reflection and elsewhere where exact types are important it's necessary to pair a method desc 
// with the exact owning type handle.
//
// See genmeth.cpp for details of generic method descriptors.
class MethodDesc
{
// Make EEClass::BuildMethodTable() a friend function. This allows us to not provide any
// Seters.  Only BuildMethodTable should modify entrys in the methoddesc and by making
// all members private this restriction is enforced!
    friend HRESULT EEClass::BuildMethodTable(Module *pModule,
                                             mdToken cl,
                                             BuildingInterfaceInfo_t *pBuildingInterfaceList,
                                             const LayoutRawFieldInfo *pLayoutRawFieldInfos,
                                             OBJECTREF *pThrowable,
                                             MethodTable *pParentMethodTable,
                                             TypeHandle *inst,
                                             PCCOR_SIGNATURE parentInst,
                                             Pending *pending);
    friend class EEClass;
    friend class ArrayClass;
    friend class NDirect;
    friend class MethodDescChunk;
    friend class InstantiatedMethodDesc;
    friend class MDEnums;
    friend class MethodImpl;
    friend struct MEMBER_OFFSET_INFO(MethodDesc);
    friend HRESULT InitializeMiniDumpBlock();
    friend class CheckAsmOffsetsCommon;

public:
    enum
    {
        ALIGNMENT_SHIFT = 3,

        ALIGNMENT       = (1<<ALIGNMENT_SHIFT),
        ALIGNMENT_MASK  = (ALIGNMENT-1)
    };


#ifdef _DEBUG

    // These are set only for MethodDescs but every time I want to use the debugger
    // to examine these fields, the code has the silly thing stored in a MethodDesc*.
    // So...
    LPCUTF8         m_pszDebugMethodName;
    LPUTF8          m_pszDebugClassName;
    LPUTF8          m_pszDebugMethodSignature;
    EEClass        *m_pDebugEEClass;
    MethodTable    *m_pDebugMethodTable;

#ifdef STRESS_HEAP
    class GCCoverageInfo* m_GcCover;
#else // !STRESS_HEAP
    DWORD_PTR       m_DebugAlignPad; // 32-bit: fixes 8-byte align, 64-bit: fixes 16-byte align
#endif // !STRESS_HEAP
#endif // !_DEBUG

    inline BYTE* GetCallablePreStubAddr()
    {
        return ((BYTE *) this) - METHOD_CALL_PRESTUB_SIZE;
    }

    inline BYTE* GetLocationOfPreStub()
    {
        //
        // This is the same as the now deprecated GetPreStubAddr.  However,
        // due to changes to the way prestubs work on IA64, we need to
        // separate out uses of the old GetPreStubAddr into two cases:
        //   1) We want a callable address that goes to the right code
        //      (this may not be the actual jitted code, so it serves
        //      as an indirected call)  These uses should be calling 
        //      GetCallablePreStubAddr.
        //   2) We want to update the prestub area and need to write bytes
        //      into it.  Here we need to know where the bytes live and 
        //      don't want to call anything.  This use should call 
        //      GetLocationOfPreStub
        //
        return ((BYTE *) this) - METHOD_CALL_PRESTUB_SIZE;
    }

    // return the address of the stub
    inline BYTE* GetPreStubAddr()
    {
        return ((BYTE *) this) - METHOD_CALL_PRESTUB_SIZE;
    }

    // return the address of the stub
    static MethodDesc *GetMethodDescFromStubAddr(BYTE *addr)
    {
        return (MethodDesc *) (addr + METHOD_CALL_PRESTUB_SIZE);
    }

    DWORD GetAttrs();
    
    DWORD GetImplAttrs();
    
    // This function can lie if a method impl was used to implement
    // more then one method on this class. Use GetName(int) to indiciate
    // which slot you are interested in.
    LPCUTF8 GetName();

    LPCUTF8 GetName(USHORT slot);
    
    BOOL IsCompressedIL();
    
    FORCEINLINE LPCUTF8 GetNameOnNonArrayClass()
    {
        return (GetMDImport()->GetNameOfMethodDef(GetMemberDef()));
    }

    // Non-zero for InstantiatedMethodDescs and MethodDescs for generic methods
    DWORD GetNumGenericMethodArgs();

    // Return the number of class type parameters that are in scope for this method
    // For static methods and methods living in uninstantiated classes this will be zero
    DWORD GetNumGenericClassArgs()
    {
      if (GetMethodTable()->GetClass()->IsGenericTypeDefinition())
	return 0;
      else
	return GetMethodTable()->GetNumGenericArgs();
    }

    // True if this is a method descriptor for an *uninstantiated* generic method
    // WARNING: this may need to go to metadata to count the generic method
    // parameters in the case of an mcComInterop method.
    BOOL IsGenericMethodDefinition();

    DWORD IsStaticInitMethod()
    {
        return IsMdClassConstructor(GetAttrs(), GetName());
    }
    
    inline BOOL IsMethodImpl()
    {
        return mdcMethodImpl & m_wFlags;
    }

    inline void SetMethodImpl(BOOL bIsMethodImpl)
    {
        m_wFlags = (WORD)((m_wFlags & ~mdcMethodImpl) |
                          (bIsMethodImpl ? mdcMethodImpl : 0));
    }

    inline DWORD IsStatic()
    {
        // This bit caches the following check:
        _ASSERTE(((m_wFlags & mdcStatic) != 0) == (IsMdStatic(GetAttrs()) != 0));

        return (m_wFlags & mdcStatic) != 0;
    }
    
    inline void SetStatic()
    {
        m_wFlags |= mdcStatic;
    }
    
    inline BOOL IsIL()
    {
        return mcIL == GetClassification()  || mcInstantiated == GetClassification();
    }
    // True if the method descriptor is an instantiation of a generic method.
    // Returns false for all other methods including the
    // primary uninstantiated descriptor for a generic method (despite the fact
    // that GetInstantiation returns something in that case)
    inline BOOL HasMethodInstantiation();

    // True if the method descriptor is either an instantiation of 
    // a generic method or is an instance method in an instantiated class (or both).
    BOOL HasClassOrMethodInstantiation() 
    {
	return (HasClassInstantiation() || HasMethodInstantiation());
    }

    inline BOOL HasClassInstantiation()
    {
        return GetMethodTable()->HasInstantiation();
    }
    
    // Return the instantiation for an instantiated generic method
    // Return NULL if not an instantiated method
    // To get the (representative) instantiation of the declaring class use GetMethodTable()->GetInstantiation()
    TypeHandle *GetMethodInstantiation();

    // Return the method descriptor with no class or method instantiations, e.g.
    // C<int>.m<string> -> C.m.  Does not modify any objects.
    // This is the identity function on non-instantiated method descs
    // in non-instantiated classes
    MethodDesc* StripClassAndMethodInstantiation();

    // Return the method descriptor with no method instantiations, e.g.
    // C<int>.m<string> -> C<int>.m
    // D.m<string> -> D.m    Does not modify any objects.
    // This is the identity function on non-instantiated method descs
    MethodDesc* StripMethodInstantiation();


    // Return the instantiation of a method's enclosing class
    // Return NULL if the enclosing class is not instantiated
    // If the method code is shared then this might be a *representative* instantiation
    // In this case, to get the exact instantiation pass in the exact type handle of the owner
    TypeHandle *GetClassInstantiation(TypeHandle owner = TypeHandle());
    
    // Is the code shared between multiple instantiations of class or method?
    // If so, then when compiling the code we might need to look up tokens in the class or method dictionary
    BOOL IsSharedByGenericInstantiations(); // shared code of any kind
    BOOL IsSharedByGenericMethodInstantiations(); // shared due to method instantiation

    // Does this method require an extra argument for instantiation information?
    // This is the case for shared-code instantiated methods and also for instance methods in shared-code instantiated structs
    BOOL HasInstParam(); 

    inline DWORD IsECall()
    {
        return mcECall == GetClassification()
            || mcArray == GetClassification();
    }
    
    inline DWORD IsArray()
    {
        return mcArray == GetClassification();
    }
    
    inline DWORD IsEEImpl()
    {
        return mcEEImpl == GetClassification();
    }
    
    inline DWORD IsNDirect()
    {
        return mcNDirect == GetClassification();
    }
    
    inline DWORD IsInterface()
    {
        return GetMethodTable()->IsInterface();
    }

     // hardcoded to return FALSE to improve code readibility
    inline DWORD IsComPlusCall()
    {
        return FALSE;
    }

    
    inline DWORD IsIntercepted()
    {
        return m_wFlags & mdcIntercepted;
    }

    // If the method is in an Edit and Contine (EnC) module, then
    // we DON'T want to backpatch this, ever.  We MUST always call
    // through the MethodDesc's PrestubAddr (5 bytes before the MethodDesc)
    // so that we can restore those bytes to "call prestub" if we want
    // to update the method.
    // 
    inline DWORD IsEnCMethod()
    {
        return GetModule()->IsEditAndContinue();
    }

    inline BOOL IsNotInline()
    {
        return (m_wFlags & mdcInlineEligibility);
    }
    
    // check if this methoddesc needs to be intercepted 
    // by the context code
    BOOL IsRemotingIntercepted()
    {
        EEClass *pClass = GetClass();
        return !IsVtableMethod() && !IsStatic() && (pClass->IsMarshaledByRef() || 
                                                    pClass->GetMethodTable() == g_pObjectClass);
    }

    // check if this needs to be intercepted by the context code
    // without using the methoddesc.
    // Happens if a virtual function is called non-virtually
    BOOL IsRemotingIntercepted2()
    {
        EEClass * pClass = GetClass();
        return IsVtableMethod() &&
            (pClass->IsMarshaledByRef() || pClass->GetMethodTable() == g_pObjectClass);
    }

    BOOL MayBeRemotingIntercepted()
    {
        EEClass * pClass = GetClass();
        return !IsStatic() &&
            (pClass->IsMarshaledByRef() || pClass->GetMethodTable() == g_pObjectClass);
    }


    // Does it represent a one way method call with no out/return parameters?
    inline BOOL IsOneWay()
    {
        return (S_OK == GetMDImport()->GetCustomAttributeByName(GetMemberDef(),
                                                                "System.Runtime.Remoting.Messaging.OneWayAttribute",
                                                                NULL,
                                                                NULL));

    }

    // If TRUE, the method is an FCALL, however might return
    // FALSE for FCALLs too if the FCALL has not been called
    // at least once.  (prestub has looked it up)
    inline BOOL MustBeFCall()
    {
        return (IsECall() && (m_wFlags & mdcInlineEligibility));
    }
    
    inline void SetNotInline(BOOL bEligibility)
    {
        if (IsECall()) return;      // we reuse the bit for ECall
        m_wFlags = (WORD)((m_wFlags & ~mdcInlineEligibility) |
                          (bEligibility ? mdcInlineEligibility : 0));
    }
    
    inline void SetFCall(BOOL fCall)
    {
        _ASSERTE(IsECall());        // we reuse the bit for ECall
        if (fCall)
            m_wFlags |= mdcInlineEligibility;
        else 
            m_wFlags &= ~mdcInlineEligibility;
    }
    
    
    inline BOOL IsVerified()
    {
        return m_wFlags & mdcVerifiedState;
    }
    
    inline void SetIsVerified(BOOL bIsVerified)
    {
        m_wFlags = (WORD)((m_wFlags & ~mdcVerifiedState) |
                          (bIsVerified ? mdcVerifiedState : 0));
    }

    BOOL CouldBeFCall();
    BOOL PointAtPreStub();

    inline void ClearFlagsOnUpdate()
    {
        SetIsVerified(FALSE);
        SetNotInline(FALSE);
    }
    
    BOOL IsVarArg();
    
    // Is this a special stub used to do one or both of the following:
    // (a) unbox a value class prior to calling an instance method
    // (b) pass an extra instantiation-info parameter to a generic method or instance method in a generic value class
    DWORD IsSpecialStub();	// (a) or (b)
    BOOL IsInstantiatingStub();	// (b) only

    DWORD SetIntercepted(BOOL set);
    
    BOOL IsVoid();
    

    //================================================================
    // The following two methods are used to facilitate the use of
    // metadata instead of calldescrs in a separate section
    
    void SetClassification(DWORD dwClassification)
    {
        _ASSERTE(dwClassification < mcCount);
        m_wFlags  = (WORD)((m_wFlags & ~mdcClassification) |
                           (dwClassification << mdcClassificationShift));
    }

    BOOL IsJitted()
    {
        if (HasMethodInstantiation())
	{
          return (m_CodeOrIL != (size_t) GetPreStubAddr());
	}
        else
          return !((m_CodeOrIL & METHOD_IS_IL_FLAG) == METHOD_IS_IL_FLAG);
    }
    
    // IL RVA stored in same field as code address, but high bit set to
    // discriminate.
    ULONG GetRVA();

    void SetMethodTakesBoxedThis()
    {
        m_CodeOrIL = ~0;
    }

    void SetRVA(ULONG rva)
    {
        _ASSERTE(rva <= METHOD_MAX_RVA);
        ClearPrejitted();
        m_CodeOrIL = rva | METHOD_IS_IL_FLAG;
    }

    inline DWORD HasNativeRVA()
    {
        return IsPrejitted() && !IsJitted();
    }
    
    inline DWORD_PTR GetNativeRVA()
    {
        _ASSERTE(HasNativeRVA());
        return (DWORD)(m_CodeOrIL & ~METHOD_IS_IL_FLAG);
    }

    //==================================================================
    
    COR_ILMETHOD* GetILHeader();
    
    LivePointerInfo* GetLivePointerInfo( const BYTE *IP );
    
    BOOL HasStoredSig()
    {
        return IsArray() || IsECall() || IsEEImpl();
    }

    PCCOR_SIGNATURE GetSig();
    
    void GetSig(PCCOR_SIGNATURE *ppSig, DWORD *pcSig);
    
    IMDInternalImport* GetMDImport()
    {
        return GetModule()->GetMDImport();
    }
        
    IMetaDataEmit* GetEmitter()
    {
        return GetModule()->GetEmitter();
    }
    
    IMetaDataImport* GetImporter()
    {
        return GetModule()->GetImporter();
    }
    
    IMetaDataHelper* GetHelper()
    {
        return GetModule()->GetHelper();
    }
    
    
    inline DWORD IsCtor()
    {
        return IsMdInstanceInitializer(GetAttrs(), GetName());
    }
    
    inline DWORD IsStaticOrPrivate()
    {
        DWORD attr = GetAttrs();
        return IsMdStatic(attr) || IsMdPrivate(attr);
    }
    
    inline DWORD IsFinal()
    {
        return IsMdFinal(GetAttrs());
    }

    inline void SetSynchronized()
    {
        m_wFlags |= mdcSynchronized;
    }
    
    inline DWORD IsSynchronized()
    {
        return (m_wFlags & mdcSynchronized) != 0;
    }
    
    inline void ClearPrejitted()
    {
        m_wFlags &= ~mdcPrejitted;
    }
        
    inline DWORD IsPrejitted()
    {
        return false;
    }
    
    // does not necessarily return TRUE if private
    inline DWORD IsPrivate()
    {
        return IsMdPrivate(GetAttrs());
    }

    inline DWORD IsPublic()
    {
        return IsMdPublic(GetAttrs());
    }

    inline DWORD IsProtected()
    {
        return IsMdFamily(GetAttrs());
    }

    inline DWORD IsVtableMethod()
    {
        return GetSlot() < GetClass()->GetNumVtableSlots();
    }

    // does not necessarily return TRUE if virtual
    inline DWORD IsVirtual()
    {
        return IsMdVirtual(GetAttrs());
    }

    // does not necessarily return TRUE if abstract
    inline DWORD IsAbstract()
    {
        return IsMdAbstract(GetAttrs());
    }

    // duplicate methods
    inline BOOL  IsDuplicate()
    {
        return m_wFlags & mdcDuplicate;
    }

    void SetDuplicate()
    {
        // method table is not setup yet
        //_ASSERTE(!GetClass()->IsInterface());
        m_wFlags |= mdcDuplicate;
    }

    // Sparse methods are those that belong to an interface whose VTable
    // representation is sparse.
    inline BOOL IsSparse()
    {
        return GetMethodTable()->IsSparse();
    }

    inline BOOL IsEnCNewVirtual()
    {
	return FALSE;
    }


    void SetSlot(WORD slot)
    {
        m_wSlotNumber = slot;
    }

    inline DWORD DontVirtualize()
    {
        return !IsVtableMethod();

        /*
        // DO: I commented this out.  I believe
        //  this is all code that tries to guess virtualness
        //  instead of looking at the virtual flag.
        if (IsMdRTSpecialName(attrs) ||
            IsMdStatic(attrs) ||
            IsMdPrivate(attrs) ||
            IsMdFinal(attrs))
            return TRUE;

        if (IsInterface())
            return TRUE;

        if (IsTdSealed(GetClass()->GetAttrClass()))
            return TRUE;

        return FALSE;
        */
    }

    inline EEClass* GetClass()
    {
        return GetMethodTable()->GetClass();
    }

    inline SLOT *GetVtable()
    {
        return GetMethodTable()->GetVtable();
    }

    inline MethodTable* GetMethodTable();   

    inline WORD GetSlot()
    {
        return m_wSlotNumber;
    }


    inline DWORD RequiresLinktimeCheck()
    {
        return m_wFlags & mdcRequiresLinktimeCheck;
    }

    inline DWORD RequiresInheritanceCheck()
    {
        return m_wFlags & mdcRequiresInheritanceCheck;
    }

    inline DWORD ParentRequiresInheritanceCheck()
    {
        return m_wFlags & mdcParentRequiresInheritanceCheck;
    }

    void SetRequiresLinktimeCheck()
    {
        m_wFlags |= mdcRequiresLinktimeCheck;
    }

    void SetRequiresInheritanceCheck()
    {
        m_wFlags |= mdcRequiresInheritanceCheck;
    }

    void SetParentRequiresInheritanceCheck()
    {
        m_wFlags |= mdcParentRequiresInheritanceCheck;
    }

    // fThrowException is used to prevent Verifier from
    // throwin an exception on error
    // fForceVerify is to be used by tools that need to
    // force verifier to verify code even if the code is fully trusted.
    HRESULT Verify(COR_ILMETHOD_DECODER* ILHeader, 
                   BOOL fThrowException, 
                   BOOL fForceVerify);

        BOOL InterlockedReplaceStub(Stub** ppStub, Stub *pNewStub);

        mdMethodDef GetMemberDef();

#ifdef _DEBUG
     private:
            void SMDDebugCheck(mdMethodDef mb);
#endif
     public:

        void SetMemberDef(mdMethodDef mb);

    // Set the offset of this method desc in a chunk table (which allows us
    // to work back to the method table/module pointer stored at the head of
    // the table.
    void SetChunkIndex(DWORD index, int flags);

    // There are two overloads of GetAddrofCode.  If you have a static, or a
    // function, or you are (somehow!) guaranteed not to be thunking, use the
    // simple version.
    //
    // If this is an instance method, you should typically use the complex
    // overload so that we can do "virtual methods" correctly, including any
    // handling of context proxies and other thunking layers.
    //
    // Only call GetUnsafeAddrofCode() if you know what you are doing.  If
    // you can guarantee that no virtualization is necessary, or if you can
    // guarantee that it has already happened.  For instance, the frame of a
    // stackwalk has obviously been virtualized as much as it will be.
    const BYTE* GetAddrofCode();
    const BYTE* GetAddrofCode(OBJECTREF orThis);
    const BYTE* GetAddrofCodeNonVirtual();

    const BYTE* GetMethodEntryPoint();

    const BYTE* GetUnsafeAddrofCode();
    const BYTE* GetNativeAddrofCode();

    // Yet another version of GetAddrofXXXCode()!
    // Please don't use this unless you have read the note in the 
    // implementation below
    const BYTE* GetAddrofJittedCode();

    // This returns either the actual address of the code if the method has already
    // been jitted (best perf) or the address of a stub 
    const BYTE* GetAddrOfCodeForLdFtn();
    
    // Yet another method for getting the address of the jitted function. This method
    // is equivalent to GetNativeAddrofCode except that it works correctly with FJIT, which
    // stores an address of a stub (JittedMethodInfo) in m_CodeOrIL. If the code manager
    // is FJIT the function will follow the stub and return the actual address of the method.
    // The stub is part of the code pitching support in FJIT.   
    const BYTE* GetFunctionAddress();

    void SetAddrofCode(BYTE* newAddr)
    {
        m_CodeOrIL = (DWORD_PTR)newAddr >> METHOD_IS_IL_SHIFT;
        _ASSERTE(IsJitted());
    }

    // does this function return an object reference?
    enum RETURNTYPE {RETOBJ, RETBYREF, RETVALTYPE, RETNONOBJ};
    RETURNTYPE ReturnsObject(
#ifdef _DEBUG
    bool supportStringConstructors = false
#endif    
        );



    Module *GetModule();

    Assembly *GetAssembly() { return GetModule()->GetAssembly(); }

        // Returns the # of bytes of stack required to build a call-stack
        // using the internal linearized calling convention. Includes
        // "this" pointer.
    UINT SizeOfVirtualFixedArgStack();


    // Returns the # of bytes of stack required to build a call-stack
    // using the actual calling convention used by the method. Includes
    // "this" pointer.
    UINT SizeOfActualFixedArgStack();


    // Returns the # of bytes to pop after a call. Not necessary the
    // same as SizeOfActualFixedArgStack()!
    UINT CbStackPop();

    void SetHasSecurity()
    {
        m_wFlags |= mdcHasSecurity;
    }

    BOOL HasSecurity()
    {
        return (m_wFlags & mdcHasSecurity) != 0;
    }

    DWORD GetSecurityFlags();
    DWORD GetSecurityFlags(IMDInternalImport *pInternalImport, mdToken tkMethod, mdToken tkClass, DWORD *dwClassDeclFlags, DWORD *dwClassNullDeclFlags, DWORD *dwMethDeclFlags, DWORD *dwMethNullDeclFlags);

    void destruct();
    //--------------------------------------------------------------------
    // Invoke a method. Arguments are packaged up in right->left order
    // which each array element corresponding to one argument.
    //
    // Can throw a COM+ exception.
    //
    // All the appropriate "virtual" semantics (include thunking like context
    // proxies) occurs inside Call.
    //
    // Call should never be called on interface MethodDesc's. The exception
    // to this rule is when calling on a COM object. In that case the call
    // needs to go through an interface MD and CallOnInterface is there
    // for that.
    //--------------------------------------------------------------------

    //
    // NOTE on Call methods
    //  MethodDesc::Call uses a virtual portable calling convention
    //  Arguments are put left-to-right in the ARG_SLOT array, in the following order:
    //    - this pointer (if any)
    //    - return buffer address (if signature.HasRetBuffArg())
    //    - all other fixed arguments (left-to-right)
    //  Vararg is not supported yet.
    //
    //  The args that fit in an ARG_SLOT are inline. The ones that don't fit in an ARG_SLOT are allocated somewhere else
    //      (usually on the stack) and a pointer to that area is put in the corresponding ARG_SLOT
    // ARG_SLOT is guaranteed to be big enough to fit all basic types and pointer types. Basically, one has
    //      to check only for aggregate value-types and 80-bit floating point values or greater.
    //
    // Not all usages of MethodDesc::CallXXX have been ported to the new convention. The end goal is to port them all and get
    //      rid of the non-portable BYTE* version.
    //    

    
//    ARG_SLOT Call               (PTRARRAYREF args);
    ARG_SLOT Call               (const ARG_SLOT *pArguments);
    ARG_SLOT Call               (const ARG_SLOT *pArguments, BinderMethodID mscorlibID);
    ARG_SLOT Call               (const ARG_SLOT *pArguments, MetaSig* sig);

    ARG_SLOT CallOnInterface(const ARG_SLOT *pArguments);
    ARG_SLOT CallOnInterface(const ARG_SLOT *pArguments, MetaSig* sig);

    INT64 CallDebugHelper    (const ARG_SLOT *pArguments, MetaSig* sig);

    ARG_SLOT CallTransparentProxy(const ARG_SLOT *pArguments);

private:
    ARG_SLOT CallDescr(const BYTE *pTarget, Module *pModule, PCCOR_SIGNATURE pSig, BOOL fIsStatic, const ARG_SLOT *pArguments);
    ARG_SLOT CallDescr(const BYTE *pTarget, Module *pModule, MetaSig* pMetaSig, BOOL fIsStatic, const ARG_SLOT *pArguments);

    const BYTE* GetCallTarget(const ARG_SLOT* pArguments);

public:
    MethodImpl *GetMethodImpl();


    const BYTE *DoPrestub(MethodTable *pDispatchingMT);

    // *********************************************************************
    // *********************************************************************
    // If you think you want to change m_CodeOrIL to be public, think again.
    // Don't do that. Leave it alone and use the darn accessor!!
    // *********************************************************************
    // *********************************************************************
protected:
    // Stores either a native code address or an IL RVA (the high bit is set to
    // indicate IL). If an IL RVA is held, native address is assumed to be the
    // prestub address.
    DWORD_PTR      m_CodeOrIL;

private:
    // Returns the slot number of this MethodDesc in the vtable array.
    WORD           m_wSlotNumber;

    // Flags.
    WORD           m_wFlags;

#ifndef TOKEN_IN_PREPAD
    // Lower three bytes are method def token, upper byte is a combination of
    // offset (in method descs) from a pointer to the method table or module and
    // a flag bit (upper bit) that's 0 for a method and 1 for a global function.
    // The value of the type flag is chosen carefully so that GetMethodTable can
    // ignore it and remain fast, pushing the extra effort on the lesser used
    // GetModule for global functions.
    DWORD          m_dwToken;
#ifdef _X86_
    DWORD          m_dwAlign2;      // assures 8-byte alignment of MethodDesc on x86
#endif
#endif

public:
    StubCallInstrs *GetStubCallInstrs()
    {
        return (StubCallInstrs*)( ((BYTE*)this) - METHOD_PREPAD );
    }


    // Checks if a given function pointer is the start of this method.
    // Note that if the ip points to something like the prestub,
    // this function could give a false positive. Hence, only use
    // this function for checking valid ip's with multiple methods.
    // Don't pass in random garbage.
    BOOL IsTarget(LPVOID ip)
    {
        if (ip == (LPVOID)GetCallablePreStubAddr())
        {
            return TRUE;
        }
        if (ip == (LPVOID)getStubAddr(this))
        {
            return TRUE;
        }
        return FALSE;
    }

private:

    inline DWORD GetClassification()
    {
        return (m_wFlags & mdcClassification) >> mdcClassificationShift;
    }

    Stub *GetStub();
};

class MethodDescChunk
{
    friend struct MEMBER_OFFSET_INFO(MethodDescChunk);
    friend HRESULT InitializeMiniDumpBlock();

public:
    static ULONG32 GetMaxMethodDescs(int flags)
    {
        SIZE_T mdSize = g_ClassificationSizeTable[flags & (mdcClassification|mdcMethodImpl)];
        _ASSERTE((mdSize & ((1 << MethodDesc::ALIGNMENT_SHIFT) - 1)) == 0);
        return (ULONG32)(127 / (mdSize >> MethodDesc::ALIGNMENT_SHIFT));
    }

    static ULONG32 GetChunkCount(ULONG32 methodDescCount, int flags)
    {
        return (methodDescCount + (GetMaxMethodDescs(flags)-1)) / GetMaxMethodDescs(flags);
    }

    static MethodDescChunk *CreateChunk(LoaderHeap *pHeap, DWORD methodDescCount, int flags, BYTE tokRange, MethodTable *initialMT = NULL);

    static MethodDescChunk *RecoverChunk(MethodDesc *methodDesc);

    void SetMethodTable(MethodTable *pMT);

    MethodTable *GetMethodTable()
    {
        return m_methodTable;
    }
        
    ULONG32 GetCount()
    {
        return m_count+1;
    }

    int GetKind()
    {
        return m_kind;
    }

    static ULONG32 GetMethodDescSize(int kind)
    {
        return g_ClassificationSizeTable[kind];
    }

    ULONG32 GetMethodDescSize()
    {
        return GetMethodDescSize(m_kind);
    }

    BYTE GetTokRange()
    {
        return m_tokrange;
    }

    ULONG32 Sizeof() 
    { 
        return (ULONG32)(sizeof(MethodDescChunk) + GetMethodDescSize() * GetCount());
    }

    MethodDesc *GetFirstMethodDesc()
    {
        return (MethodDesc *) (((BYTE*)(this + 1)) + METHOD_PREPAD);
    }

    MethodDesc *GetMethodDescAt(int n)
    {
        return (MethodDesc *) (((BYTE*)(this + 1)) + METHOD_PREPAD + GetMethodDescSize()*n);
    }

    MethodDescChunk *GetNextChunk()
    { 
        return m_next; 
    }

    void SetNextChunk(MethodDescChunk *chunk)
    { 
        m_next = chunk; 
    }


private:

        // This must be at the beginning for the asm routines to work.
    MethodTable *m_methodTable;

    MethodDescChunk     *m_next;
    USHORT               m_count;
    BYTE                 m_kind;
    BYTE                 m_tokrange;  

    //
    // IA64: this struct needs to be a multiple of 16 bytes so that
    //       the code that follows is 16-byte aligned.  
    //
    BYTE                 m_alignpad[METHOD_DESC_CHUNK_ALIGNPAD_BYTES];

        // If the method descs do not have prestubs, there is a gap of METHOD_PREPAD here
        // to make address arithmetic easier.

        // Followed by array of method descs...


};


class MDEnums
{
    public:
        enum {
#if !defined(__GNUC__)
            // GCC doesn't generate the correct constant here.
#ifdef TOKEN_IN_PREPAD
            MD_IndexOffset = (offsetof(StubCallInstrs, m_chunkIndex)) - METHOD_PREPAD,
#else
            MD_IndexOffset = offsetof(MethodDesc, m_dwToken) + 3,
#endif
#endif  // !defined(__GNUC__)
            MD_SkewOffset = - (int)(METHOD_PREPAD + sizeof(MethodDescChunk))
        };

#if defined(__GNUC__)
#ifdef TOKEN_IN_PREPAD
        static const int MD_IndexOffset = (offsetof(StubCallInstrs, m_chunkIndex)) - METHOD_PREPAD;
#else
        static const int MD_IndexOffset = offsetof(MethodDesc, m_dwToken) + 3;
#endif
#endif  // defined(__GNUC__)
};


inline /*static*/ MethodDescChunk *MethodDescChunk::RecoverChunk(MethodDesc *methodDesc)
{
    return (MethodDescChunk*)((BYTE*)methodDesc + (*((char*)methodDesc + MDEnums::MD_IndexOffset) * MethodDesc::ALIGNMENT) + MDEnums::MD_SkewOffset);
}



// convert arbitrary IP location in jitted code to a MethodDesc
MethodDesc* IP2MethodDesc(const BYTE* IP);

// convert an entry point into a MethodDesc
MethodDesc* Entry2MethodDesc(const BYTE* entryPoint, MethodTable *pMT);   


// There are two overloads of GetAddrofCode.  If you have a static, or a
// function, or you are (somehow!) guaranteed not to be thunking, use the
// simple version.
//
// If this is an instance method, you should typically use the complex
// overload so that we can do "virtual methods" correctly, including any
// handling of context proxies and other thunking layers, but more typically
// just adjusting for the inheritance hierarchy of overrides.
//
// Only call GetUnsafeAddrofCode() if you know what you are doing.  If
// you can guarantee that no virtualization is necessary, or if you can
// guarantee that it has already happened.  For instance, the frame of a
// stackwalk has obviously been virtualized as much as it will be.
inline const BYTE* MethodDesc::GetAddrofCode()
{

    // !! This logic is also duplicated in ASM code emitted by
    // EmitUMEntryThunkCall (nexport.cpp.) Don't change this
    // without updating that code!

    _ASSERTE(!IsGenericMethodDefinition());
    _ASSERTE(DontVirtualize() || !GetMethodTable()->IsThunking());
    
    return !IsJitted() ? GetPreStubAddr() : GetAddrofJittedCode();
}

//
// This method is to be used when you are trying to call a managed method.
// It is equivalent to GetAddrofCode on x86, but on other platforms,
// the entry point is defined as a pointer to the prestub area of the MethodDesc.
// The reason for this is that we cannot put code in the prestub area and, 
// as a result, cannot recover the callee's MethodDesc for JITing without
// passing it in a hidden way (r9 on IA64).
//
inline const BYTE* MethodDesc::GetMethodEntryPoint()
{
#ifdef _X86_
    return GetAddrofCode();
#else // _X86_
    _ASSERTE(DontVirtualize() || !GetMethodTable()->IsThunking());
    return GetLocationOfPreStub();
#endif // _X86_
}

inline const BYTE* MethodDesc::GetAddrofCode(OBJECTREF orThis)
{
    _ASSERTE(!DontVirtualize());
    _ASSERTE(!IsGenericMethodDefinition());

    //@GENERICS: this is essentially asking for vtable dispatch off an instantiated method, something that's
    // only handled by a JIT helper function right now
    _ASSERTE(!HasMethodInstantiation());

    // Deliberately use GetMethodTable -- not GetTrueMethodTable
    BYTE *addr = (BYTE *)*(orThis->GetClass()->GetMethodSlot(this));

    return addr;
}

inline const BYTE* MethodDesc::GetAddrofCodeNonVirtual()
{
    _ASSERTE(DontVirtualize());

    _ASSERTE(!IsGenericMethodDefinition());

    //@GENERICS: This method won't work for 
    // instantiated method descs as it goes through the vtable
    _ASSERTE(!HasMethodInstantiation());

    _ASSERTE(GetSlot() < (int)GetMethodTable()->GetTotalSlots());
    BYTE *addr = (BYTE *)*(GetClass()->GetMethodSlot(this));

    return addr;
}

inline const BYTE* MethodDesc::GetUnsafeAddrofCode()
{
    // See comments above.  Only call this if you are sure it is safe
    _ASSERTE(!GetMethodTable()->IsThunking());

    if(IsRemotingIntercepted() || IsIntercepted() || IsComPlusCall() || IsNDirect() || IsEnCMethod() || !IsJitted())
    {
        return GetCallablePreStubAddr();
    }
    else
    {
        return GetAddrofJittedCode();
    }
}

inline const BYTE* MethodDesc::GetAddrofJittedCode()
{
    // ***************************************************
    // 
    // NOTE: This function is for the exclusive use of 
    // the PreStubWorker (MakeJITWorker)
    // Please do not call it from other places in the runtime... 
    // unless you *really* know what you are doing! 
    // 
    // It should always return m_CodeOrIL which is jitted native code!
    // 
    // ***************************************************
    _ASSERTE(!IsGenericMethodDefinition());
    _ASSERTE(IsJitted());
    return (BYTE*)(m_CodeOrIL << METHOD_IS_IL_SHIFT);
}


inline const BYTE* MethodDesc::GetNativeAddrofCode()
{
    // See comments above.  Only call this if you are sure it is safe

    // Commented this out - the debugger must call this routine to get the
    // native address of __TransparentProxy::.ctor during prejit.
    // _ASSERTE(!GetMethodTable()->IsThunking());

    const BYTE *addrOfCode = (const BYTE*)(m_CodeOrIL << METHOD_IS_IL_SHIFT);

    Module *pModule = GetModule();

    if (pModule->SupportsUpdateableMethods() && IsJitted())
        UpdateableMethodStubManager::CheckIsStub(addrOfCode, &addrOfCode);


    return addrOfCode;
}
// This function follows the jump <MethodAddress> stub (which is part of JittedMethodInfo) that FJIT uses
// to support code pitching 
inline const BYTE* MethodDesc::GetFunctionAddress()
{
    const BYTE* adr = GetNativeAddrofCode(); 
    IJitManager* pEEJM = ExecutionManager::FindJitMan((SLOT)adr);

    if (pEEJM == NULL)
        return NULL;

    if (pEEJM->IsStub(adr))
        return pEEJM->FollowStub(adr);
    else
        return adr;
}



//-----------------------------------------------------------------------
//  class MethodImpl
// 
//  There are method impl subclasses of the method desc classes. This 
//  was done to minimize the effect of MethodImpls which are very rare.
//  
//-----------------------------------------------------------------------

class MI_MethodDesc : public MethodDesc
{
    friend class MethodImpl;

    MethodImpl m_Overrides;

public:
    MethodImpl* GetImplData()
    {
        return & m_Overrides;
    }
};

class StoredSigMethodDesc : public MethodDesc
{
  public:
    // Put the sig RVA in here - this allows us to avoid
    // touching the method desc table when mscorlib is prejitted.
    //

    PCCOR_SIGNATURE m_pSig;
    DWORD           m_cSig;
};

//-----------------------------------------------------------------------
// Operations specific to ECall methods. We use a derived class to get
// the compiler involved in enforcing proper method type usage.
// DO NOT ADD FIELDS TO THIS CLASS.
//-----------------------------------------------------------------------

class ECallMethodDesc : public StoredSigMethodDesc
{
    public:

        LPVOID GetECallTarget()
        {
            _ASSERTE(IsECall());
            return (LPVOID)GetAddrofJittedCode();
        }

    static BYTE GetECallTargetOffset()
    {
        size_t ofs = offsetof(ECallMethodDesc, m_CodeOrIL);
        _ASSERTE(FitsInI1(ofs));
        return (BYTE)ofs;
    }

};

class MI_ECallMethodDesc : public ECallMethodDesc
{
    friend class MethodImpl;

    MethodImpl m_Overrides;

  public:
    MethodImpl* GetImplData()
    {
        return & m_Overrides;
    }
};

class ArrayECallMethodDesc : public ECallMethodDesc
{
public:         // should probably be private:
    WORD        m_wAttrs;           // method attributes
    BYTE        m_intrinsicID;
    BYTE        m_unused;
    LPCUTF8     m_pszArrayClassMethodName;
};

class MI_ArrayECallMethodDesc : public ArrayECallMethodDesc
{
    friend class MethodImpl;

    MethodImpl m_Overrides;

  public:
    MethodImpl* GetImplData()
    {
        return & m_Overrides;
    }
};

//-----------------------------------------------------------------------
// Operations specific to NDirect methods. We use a derived class to get
// the compiler involved in enforcing proper method type usage.
// DO NOT ADD FIELDS TO THIS CLASS.
//-----------------------------------------------------------------------
class NDirectMethodDesc : public MethodDesc
{
public:
    struct temp1
    {
        // Initially points to m_ImportThunkGlue (which has an embedded call
        // to link the method.
        //
        // After linking, points to the actual unmanaged target.
        //
        // The JIT generates an indirect call through this location in some cases.
        LPVOID      m_pNDirectTarget;
        MLHeader    *m_pMLHeader;        // If not ASM'ized, points to
                                         //  marshaling code and info.

        // Embeds a "call NDirectImportThunk" instruction. m_pNDirectTarget
        // initially points to this "call" instruction.
        BYTE        m_ImportThunkGlue[METHOD_CALL_PRESTUB_SIZE];

        // Various attributes needed at runtime
        BYTE        m_flags;

        // Size of outgoing arguments (on stack)
        WORD        m_cbDstBufSize;

        LPCUTF8     m_szLibName;
        LPCUTF8     m_szEntrypointName;
        
//#ifdef _IA64_
//        BYTE        m_ArgDeltas[16];
//#endif
    } ndirect;

    enum MarshCategory
    {
        kUnknown = 0,       // Not yet cached. !! DO NOT CHANGE THIS VALUE FROM 0!!!
        kNoMarsh = 1,       // All params & retvals are equivalent to 32-bit ints
        kYesMarsh = 2,      // Some nontrivial marshaling is required
    };

    enum SubClassification
    {
        kLateBound = 0,     // standard [sysimport] stuff
    };

    enum Flags
    {
        // There are some very subtle race issues dealing with
        // setting and resetting these bits safely. Pay attention!
        //
        // There are three groups of flag bits here each which gets initialized
        // at different times. Because they all share the same byte, they must
        // follow some very careful rules to avoid losing settings due to multiple
        // threads racing.

        // Group 1: The JIT group.
        //
        //   Remembers the result of a previous JIT inlining candidacy test.
        //   This caching is done to avoid redundant work.
        //
        //   The candidacy test can run concurrently in multiple threads.
        //   It can also run concurrently with the prestub!
        //
        //   If two candicacy tests race, there is no problem as they
        //   use InterlockedCompareExchange to avoid collision, and the
        //   two tests are guaranteed to come up with the same result.
        //
        //   If a candidacy test races with the prestub, the candidacy test
        //   promises not to stomp on the prestub's work; even if it means
        //   failing to cache the test result. The prestub, however,
        //   may stomp on the test result, setting it back to kUnknown.
        //   This is ok - all it means is the JIT will have to do the
        //   test again the next time it hits this method.
        kMarshCategoryMask          = 0x03,
        kMarshCategoryShift = 0,



        // Group 2: The ctor group.
        //
        //   This group is set during MethodDesc construction. No race issues
        //   here since they are initialized before the MD is ever published
        //   and never change after that.
        kSubClassificationMask      = 0x04,
        kSubClassificationShift = 2,

        // Group 3: The prestub group
        //
        //   This group is initialized during the prestub. Once the prestub
        //   completes, these bits never change. It's possible
        //   for multiple threads to race in the prestub or with the JIT.
        //
        //   To avoid races, the prestub composes all the prestub bits in
        //   a local variable: then uses a single write to "publish" the
        //   finished bits in one go.
        //
        //   If the JIT inlining test races with a prestub, there is a chance
        //   the JIT's update may get "lost". However, all this means is that
        //   the JIT group will remain as kUnknown which is ok: all it means
        //   is that the JIT will do some redundant work.
        kNativeLinkTypeMask         = 0x08,

        kNativeLinkFlagsMask        = 0x30,
        kNativeLinkFlagsShift = 4,

        kVarArgsMask                = 0x40,

        kStdCallMask                = 0x80,
    };

    // Retrieves the current known state of the marshcategory.
    // Note that due to known (and accepted) races, the marshcat
    // can spontaneously revert from Yes/No to unknown. It cannot, however,
    // change from Yes to No or No to Yes.
    MarshCategory GetMarshCategory()
    { 
        return (MarshCategory) 
          ((ndirect.m_flags & kMarshCategoryMask)>>kMarshCategoryShift); 
    }


    // Called during MD construction to initialize to kUnknown. Do not
    // call after the MD has been made available to other threads
    // as you may stomp other bits due to races.
    void InitMarshCategory()
    {
        ndirect.m_flags = 
          (ndirect.m_flags&~kMarshCategoryMask) | (kUnknown<<kMarshCategoryShift);
    }

    // Called from the JIT inline test (and ONLY that.)
    //
    // Attempts to store a kNoMarsh or kYesMarsh in the marshcategory field.
    // Due to the need to avoid races with the prestub, there is a
    // small but nonzero chance that this routine may silently fail
    // and leave the marshcategory as "unknown." This is ok since
    // all it means is that the JIT may have to do repeat some work
    // the next time it JIT's a callsite to this NDirect.
    //
    // This routine is guaranteed not to disturb bits being set
    // simultaneously by the prestub.
    void ProbabilisticallyUpdateMarshCategory(MarshCategory value);



    // Called from the prestub (and ONLY the prestub.)
    //
    // Sets all the prestub bits in one go. Will preserve existing
    // SubClassification bits. Will preserve existing MarshCat bits, BUT
    // if some other thread tries to update MarshCat at the same time,
    // its update will probably be lost. This is a known race that the
    // JIT inline is designed to compensate for for.
    void PublishPrestubFlags(BYTE prestubflags)
    {
        _ASSERTE( 0 == (prestubflags & (kMarshCategoryMask | kSubClassificationMask)) );

        // We'll merge the new prestub flags in with the nonprestub flags.
        // The subclassification masks are set once and for all at MethodDesc construction, so
        //    no race issues there.
        // There is a potential race where the MarshCategory may get reset
        //    back to unknown. However, this is an expected race and only
        //    results in some redundant work in the JIT inliner.
        ndirect.m_flags = ( prestubflags | ( ndirect.m_flags & (kMarshCategoryMask | kSubClassificationMask) ) ); 
    }

    SubClassification GetSubClassification()
    { 
        return (SubClassification) 
          ((ndirect.m_flags & kSubClassificationMask)>>kSubClassificationShift); 
    }

    // Called during MD construction. Do not
    // call after the MD has been made available to other threads
    // as you may stomp other bits due to races.
    void InitSubClassification(SubClassification value)
    { 
        ndirect.m_flags = 
          (ndirect.m_flags&~kSubClassificationMask) | (value<<kSubClassificationShift);

        _ASSERTE(GetSubClassification() == value);
    }

    CorNativeLinkType GetNativeLinkType()
    { 
        return (ndirect.m_flags&kNativeLinkTypeMask) ? nltAnsi : nltUnicode;
    }


    static void SetNativeLinkTypeBits(BYTE *pflags, CorNativeLinkType value)
    {                                
        _ASSERTE(value == nltAnsi || value == nltUnicode);

        if (value == nltAnsi)
            (*pflags) |= kNativeLinkTypeMask;
        else
            (*pflags) &= ~kNativeLinkTypeMask;
    }


    CorNativeLinkFlags GetNativeLinkFlags()
    { 
        return (CorNativeLinkFlags) 
          ((ndirect.m_flags & kNativeLinkFlagsMask)>>kNativeLinkFlagsShift); 
    }


    static void SetNativeLinkFlagsBits(BYTE *pflags, CorNativeLinkFlags value)
    {
        (*pflags) = 
          ((*pflags)&~kNativeLinkFlagsMask) | (value<<kNativeLinkFlagsShift);
    }

    BOOL IsVarArgs()
    { 
        return (ndirect.m_flags&kVarArgsMask) != 0;
    }


    static void SetVarArgsBits(BYTE *pflags, BOOL value)
    {
        if (value)
            (*pflags) |= kVarArgsMask;
        else
            (*pflags) &= ~kVarArgsMask;
    }

    BOOL IsStdCall()
    { 
        return (ndirect.m_flags&kStdCallMask) != 0;
    }

     
    static void SetStdCallBits(BYTE *pflags, BOOL value)
    { 
        if (value)
            (*pflags) |= kStdCallMask;
        else
            (*pflags) &= ~kStdCallMask;

    }

    public:

        LPVOID GetNDirectTarget()
        {
            _ASSERTE(IsNDirect());
            return ndirect.m_pNDirectTarget;
        }

        VOID SetNDirectTarget(LPVOID pTarget)
        {
            _ASSERTE(IsNDirect());
            ndirect.m_pNDirectTarget = pTarget;
        }


        MLHeader *GetMLHeader()
        {
            _ASSERTE(IsNDirect());
            return ndirect.m_pMLHeader;
        }

        MLHeader **GetAddrOfMLHeaderField()
        {
            _ASSERTE(IsNDirect());
            return &ndirect.m_pMLHeader;
        }

        static UINT32 GetOffsetofMLHeaderField()
        {
            return (UINT32)offsetof(NDirectMethodDesc, ndirect.m_pMLHeader);
        }

        BOOL InterlockedReplaceMLHeader(MLHeader *pMLHeader, MLHeader *pMLOldHeader);

        static UINT32 GetOffsetofNDirectTarget()
        {
            size_t ofs = offsetof(NDirectMethodDesc, ndirect.m_pNDirectTarget);
            _ASSERTE(ofs == (UINT32)ofs);
            return (UINT32)ofs;
        }
};

class MI_NDirectMethodDesc : public NDirectMethodDesc
{
    friend class MethodImpl;

    MethodImpl m_Overrides;

  public:
    MethodImpl* GetImplData()
    {
        return & m_Overrides;
    }
};


//-----------------------------------------------------------------------
// Operations specific to EEImplCall methods. We use a derived class to get
// the compiler involved in enforcing proper method type usage.
//
// For now, the only EE impl is the delegate Invoke method. If we
// add other EE impl types in the future, may need a discriminator
// field here.
//-----------------------------------------------------------------------
class EEImplMethodDesc : public StoredSigMethodDesc
{
};

class MI_EEImplMethodDesc : public StoredSigMethodDesc
{
    friend class MethodImpl;

    MethodImpl m_Overrides;

  public:
    MethodImpl* GetImplData()
    {
        return & m_Overrides;
    }
};

//-----------------------------------------------------------------------
// InstantiatedMethodDescs are used for generics and 
// come in four flavours, discriminated by the
// low order bits of the first field:
//
//  00 --> GenericMethodDefinition 
//  01 --> UnsharedGenericMethodInstantiation 
//  10 --> SharedGenericMethodInstantiation 
//  11 --> InstantiatingStub 
//
// A SharedGenericMethodInstantiation descriptor extends MethodDesc  
// with a pointer to dictionary layout and a representative instantiation.
//
// A GenericMethodDefition descriptor extends MethodDesc with
// TypeDescs for the type variables for the method.  These together
// form a "typical instantiation", also used for verifying the method.
//
// An InstantiatingStub extends MethodDesc with per-instantiation info, 
// the first few elements of which are the type
// handles for the method instantiation, along with a shared method descriptor.
// Instantiating stubs are used as
// extra type-context parameters. When used as an entry, instantiating stubs pass an
// instantiation dictionary on to the shared method.
// These entries are required to implement ldftn instructions 
// on instantiations of shared generic methods, as the 
// InstantiatingStub's pointer does not expect a dictionary argument; 
// instead, it passes itself on to the shared code as the dictionary. 
//
// An UnsharedGenericMethodInstantiation contains just an instantiation.
// These are fully-specialized wrt method and class type parameters.
// These satisfy (!IsGenericMethodDefinition() && !IsSharedByGenericMethodInstantiations() && !IsInstantiatingStub())  
//
// Note that plain MethodDescs may represent shared code w.r.t. class type
// parameters (see MethodDesc::IsSharedByGenericInstantiations()).
//-----------------------------------------------------------------------

class InstantiatedMethodDesc : public MethodDesc
{
public:    
    static InstantiatedMethodDesc* CreateGenericInstantiation(MethodTable *pMT, MethodDesc* pGenericMD, TypeHandle *typars, BOOL allowInstParam = FALSE);

    WORD FindDictionaryToken(unsigned token, WORD *offsets);

    HRESULT AllocateMethodDicts();

    void SetGenericMethodDesc(MethodDesc* pMD)
    {
      SetMemberDef(pMD->GetMemberDef());
      m_wFlags = pMD->m_wFlags;
      SetSlot(pMD->GetSlot());
    }

    // All varieties of InstantiatedMethodDesc's support this method.
    TypeHandle* GetMethodInstantiation()
    {
        if (IsGenericMethodDefinition())
            return GetTypicalMethodInstantiation();
        else if (IsSharedByGenericMethodInstantiations())
            return m_pPerInstInfo;
        else
            return m_pMethInst;
    }

    BOOL IsGenericMethodDefinition()
    {
        return((m_asInt & KindMask) == GenericMethodDefinition); 
    }

    SLOT *GetCodeAddr()
    {
        return (SLOT*) &m_CodeOrIL;
    }

    BOOL IsSharedByGenericMethodInstantiations()
    {
        return((m_asInt & KindMask) == SharedGenericMethodInstantiation); 
    }

    BOOL IsInstantiatingStub()
    {
        return((m_asInt & KindMask) == InstantiatingStub); 
    }

    // Initialize the typical (ie. formal) instantiation used for verification purposes
    HRESULT InitTypicalMethodInstantiation(LoaderHeap *pHeap, DWORD numTyPars); 

    // Get the typical (ie. formal) instantiation 
    TypeHandle* GetTypicalMethodInstantiation()
    {
        _ASSERTE(IsGenericMethodDefinition());
        return  m_pTypicalInstantiation; 
    }

    // Get the dictionary layout, if one has been allocated
    DictionaryLayout* GetDictionaryLayout()
    {
        if (IsInstantiatingStub())
            return GetSharedMethodDesc()->GetDictionaryLayout();
        else if (IsSharedByGenericInstantiations())
            return (DictionaryLayout *) ((INT_PTR) (m_asInt & ~KindMask)); 
        else
            return NULL;
    }

    InstantiatedMethodDesc* GetSharedMethodDesc()
    {
        _ASSERTE(IsInstantiatingStub());
        return (InstantiatedMethodDesc *) ((INT_PTR) (m_asInt & ~KindMask)); 
    }

    // Set the instantiation in the per-inst section (this is actually a dictionary)
    void SetupSharedMethodInstantiation(TypeHandle *pPerInstInfo)
    {
        // Initially the dictionary layout is empty
        m_asInt = (INT_PTR) SharedGenericMethodInstantiation; 
        m_pPerInstInfo = pPerInstInfo; 
        _ASSERTE(IsSharedByGenericMethodInstantiations());
    }

    // Set the instantiation in the per-inst section (this is actually a dictionary)
    void SetupUnsharedMethodInstantiation(TypeHandle *pInst)
    {
        // The first field is never used
        m_asInt = (INT_PTR) UnsharedGenericMethodInstantiation; 
        m_pMethInst = pInst; 

        _ASSERTE(!IsInstantiatingStub());
        _ASSERTE(!IsSharedByGenericMethodInstantiations());
        _ASSERTE(!IsGenericMethodDefinition());
    }

    void SetupGenericMethodDefinition()
    {
        // The first field is never used
        m_asInt = (INT_PTR) GenericMethodDefinition; 
        // Initialize the type variables to empty
        m_pTypicalInstantiation = NULL;

        _ASSERTE(IsGenericMethodDefinition());
    }

    void SetupInstantiatingStub(InstantiatedMethodDesc* sharedMD,TypeHandle *pInst)   
    {   
        _ASSERTE(sharedMD->IsSharedByGenericMethodInstantiations());

        m_asInt = (((INT_PTR) sharedMD) | InstantiatingStub); 
        m_pMethInst = pInst; 

        _ASSERTE(IsInstantiatingStub());
        _ASSERTE(((MethodDesc *) this)->IsInstantiatingStub());
    }

    void ModifyPerInstInfo(TypeHandle* pPerInstInfo)
    {
        _ASSERTE(IsInstantiatingStub());
        m_pPerInstInfo = pPerInstInfo; 
    }

    void ModifyDictionaryLayout(DictionaryLayout* d)
    {
        _ASSERTE(IsSharedByGenericMethodInstantiations());
        m_asInt = (((INT_PTR) d) | SharedGenericMethodInstantiation); 
    }

    void ModifyTypicalMethodInstantiation(TypeHandle *pTypicalInst)
    {
        _ASSERTE(IsGenericMethodDefinition());
        m_pTypicalInstantiation = pTypicalInst; 
    }

    enum
    {
        KindMask = 0x03,
        GenericMethodDefinition = 0x00,
        UnsharedGenericMethodInstantiation = 0x01,
        SharedGenericMethodInstantiation  = 0x02,
        InstantiatingStub  = 0x03,
    };

    // The lowest two bits of this pointer indicate
    // the kind of InstantiatedMethodDesc we're dealing with.
    //  00 --> GenericMethodDefition 
    //  01 --> UnsharedGenericMethodInstantiation 
    //  10 --> SharedGenericMethodInstantiation 
    //  11 --> InstantiatingStub 
    union {
        DictionaryLayout* m_pDictLayout; //SHARED

        InstantiatedMethodDesc* m_pSharedMethodDesc; // STUB

        // If this is a generic definition or unshared instantiation 
        // then this field just contains the tag // GENERIC, UNSHARED
        INT_PTR     m_asInt;       

    };

public:
    // Note we can't steal bits off m_pPerInstInfo as the JIT generates code to access through it!!
    union
    {
        // Type parameters to method, exact if this is unshared code, representative otherwise.
        // This is actually a dictionary and further slots may hang off the end of th
        // instantiation.
        TypeHandle* m_pPerInstInfo;  //SHARED

        // Type parameters to method, exact if this is unshared code or a stub
        TypeHandle* m_pMethInst;  // UNSHARED, STUB

        //@GENERICSVER: typical type parameters to method
        TypeHandle* m_pTypicalInstantiation; // GENERIC
    };

private:
    HRESULT AllocateMethodDict(DictionaryLayout *pDictLayout);

    static InstantiatedMethodDesc* FindInstantiatedMethodDesc(MethodTable *pMT, 
                                                              MethodDesc *pGenericMD, 
                                                              DWORD ntypars, 
                                                              TypeHandle *typars, 
                                                              BOOL getSharedNotStub);

    static InstantiatedMethodDesc* FindInstantiatedMethodDescInChunkList(MethodDescChunk *pChunk,
                                                                         MethodTable *pMT,
                                                                         mdToken tok,
                                                                         DWORD ntypars, 
                                                                         TypeHandle *typars,
                                                                         BOOL getSharedNotStub);
        

    static HRESULT NewInstantiatedMethodDesc(MethodTable *pMT, 
                                             MethodDesc* pGenericMDescInRepMT, 
                                             InstantiatedMethodDesc* pSharedMDescForStub, 
                                             InstantiatedMethodDesc** ppMD, 
                                             DWORD ntypars, 
                                             TypeHandle *typars, 
                                             BOOL getSharedNotStub);
};

class MI_InstantiatedMethodDesc : public InstantiatedMethodDesc
{
    friend class MethodImpl;

    MethodImpl m_Overrides;

public:

    MethodImpl* GetImplData()
    {
        return & m_Overrides;
    }
};

inline MethodTable* MethodDesc::GetMethodTable()
{
    return *(MethodTable**)((BYTE*)this + (*((char*)this + MDEnums::MD_IndexOffset) * MethodDesc::ALIGNMENT) + MDEnums::MD_SkewOffset);
}

inline mdMethodDef MethodDesc::GetMemberDef()
{
#ifdef TOKEN_IN_PREPAD
    BYTE   tokrange = MethodDescChunk::RecoverChunk(this)->GetTokRange();
    UINT16 tokremainder = GetStubCallInstrs()->m_wTokenRemainder;
    return MergeToken(tokrange, tokremainder) | mdtMethodDef;
#else
    return (m_dwToken & 0x00FFFFFF) | mdtMethodDef;
#endif
}

inline void MethodDesc::SetMemberDef(mdMethodDef mb)
{
    // Note: In order for this assert to work, SetChunkIndex must be called
    // before SetMemberDef.
#ifdef _DEBUG
    SMDDebugCheck(mb);
#endif

#ifdef TOKEN_IN_PREPAD
      BYTE tokrange;
      UINT16 tokremainder;
      SplitToken(mb, &tokrange, &tokremainder);

      GetStubCallInstrs()->m_wTokenRemainder = tokremainder;
      if (mb != 0)
      {
          _ASSERTE(MethodDescChunk::RecoverChunk(this)->GetTokRange() == tokrange);
      }
#else
      m_dwToken &= 0xFF000000;
      m_dwToken |= (mb & 0x00FFFFFF);
#endif
}



#ifdef _DEBUG
inline void MethodDesc::SMDDebugCheck(mdMethodDef mb)
{
    if (TypeFromToken(mb) != 0)
    {
        _ASSERTE( GetTokenRange(mb) == MethodDescChunk::RecoverChunk(this)->GetTokRange() );
    }
}
#endif

inline TypeHandle *MethodDesc::GetMethodInstantiation()
{
    return
        (GetClassification() == mcInstantiated)
        ? ((InstantiatedMethodDesc *) this)->GetMethodInstantiation() 
        : NULL;
}

inline BOOL MethodDesc::IsGenericMethodDefinition()
{
    return 
        (GetClassification() == mcInstantiated && ((InstantiatedMethodDesc *) this)->IsGenericMethodDefinition())
        || (GetClassification() == mcECall && GetNumGenericMethodArgs() > 0);
}

inline BOOL MethodDesc::HasMethodInstantiation()
{
    return (GetClassification() == mcInstantiated && !IsGenericMethodDefinition());
}

inline void MethodDesc::SetChunkIndex(DWORD index, int flags) {
    // Calculate size of each method desc.
    DWORD size = g_ClassificationSizeTable[flags & (mdcClassification|mdcMethodImpl)];

    // Calculate the negative offset (mod 8) from the chunk table header.
    _ASSERTE((size & ALIGNMENT_MASK) == 0);
    DWORD offset = -(int)(index * (size >> ALIGNMENT_SHIFT));

#ifdef TOKEN_IN_PREPAD
    GetStubCallInstrs()->m_chunkIndex = (BYTE)offset;
#else
    // Fill in the offset in the top eight bits of the token field
    // (since we don't need the token type).
    m_dwToken &= 0x00FFFFFF;
    m_dwToken |= offset << 24;
#endif

    // Make sure that the MethodDescChunk is setup correctly
    _ASSERTE(MethodDescChunk::RecoverChunk(this)->GetMethodDescSize() == size);
}
//
// Note: no one is allowed to use this mask outside of method.hpp and method.cpp. Don't make this public!
//
#undef METHOD_IS_IL_FLAG

#endif /* _METHOD_H */
