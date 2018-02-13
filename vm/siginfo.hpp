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
// siginfo.hpp
//
#ifndef _H_SIGINFO
#define _H_SIGINFO

#include "util.hpp"
#include "vars.hpp"

#ifdef COMPLUS_EE
#include "gcscan.h"
#else
// Hack to allow the JIT executable to work
// They have a PELoader but not a Module. All the
// code that uses getScope() from module is currently
// ifdef'd out as well
#define Module mdScope
#endif


//---------------------------------------------------------------------------------------
// These macros define how arguments are mapped to the stack in the managed calling convention.
// We assume to be walking a method's signature left-to-right, in the virtual calling convention.
// See MethodDesc::Call for details on this virtual calling convention.
// These macros tell us whether the arguments we see as we proceed with the signature walk are mapped
//   to increasing or decreasing stack addresses. This is valid only for arguments that go on the stack.
//---------------------------------------------------------------------------------------
#if defined(_X86_)
#define STACK_GROWS_DOWN_ON_ARGS_WALK
#elif defined(_PPC_) || defined(_IA64_) || defined(_AMD64_) || defined(_SPARC_)
#define STACK_GROWS_UP_ON_ARGS_WALK
#else
PORTABILITY_WARNING("Platform calling convention not defined")
#define STACK_GROWS_UP_ON_ARGS_WALK
#endif



// uncompress encoded element type. throw away any custom modifier prefixes along
// the way.
FORCEINLINE CorElementType CorSigEatCustomModifiersAndUncompressElementType(//Element type
    PCCOR_SIGNATURE &pData)             // [IN,OUT] compressed data 
{
    while (ELEMENT_TYPE_CMOD_REQD == *pData || ELEMENT_TYPE_CMOD_OPT == *pData)
    {
        pData++;
        CorSigUncompressToken(pData);
    }
    return (CorElementType)*pData++;
}

// CorSig helpers which won't overflow your buffer

inline ULONG CorSigCompressDataSafe(ULONG iLen, BYTE *pDataOut, BYTE *pDataMax)
{
    BYTE buffer[4];
    ULONG result = CorSigCompressData(iLen, buffer);
    if (pDataOut + result < pDataMax)
        pDataMax = pDataOut + result;
	if (pDataMax > pDataOut)
		CopyMemory(pDataOut, buffer, pDataMax - pDataOut);
    return result;
}

inline ULONG CorSigCompressTokenSafe(mdToken tk, BYTE *pDataOut, BYTE *pDataMax)
{
    BYTE buffer[4];
    ULONG result = CorSigCompressToken(tk, buffer);
    if (pDataOut + result < pDataMax)
        pDataMax = pDataOut + result;
	if (pDataMax > pDataOut)
		CopyMemory(pDataOut, buffer, pDataMax - pDataOut);
    return result;
}

inline ULONG CorSigCompressSignedIntSafe(int iData, BYTE *pDataOut, BYTE *pDataMax)
{
    BYTE buffer[4];
    ULONG result = CorSigCompressSignedInt(iData, buffer);
    if (pDataOut + result < pDataMax)
        pDataMax = pDataOut + result;
	if (pDataMax > pDataOut)
		CopyMemory(pDataOut, buffer, pDataMax - pDataOut);
    return result;
}

inline ULONG CorSigCompressElementTypeSafe(CorElementType et, 
                                           BYTE *pDataOut, BYTE *pDataMax)
{
    if (pDataMax > pDataOut)
        return CorSigCompressElementType(et, pDataOut);
    else
        return 1;
}


struct ElementTypeInfo {
#ifdef _DEBUG
    int            m_elementType;     
#endif
    int            m_cbSize;
    CorInfoGCType  m_gc         : 3;
    int            m_fp         : 1;
    int            m_enregister : 1;
    int            m_isBaseType : 1;

};
extern const ElementTypeInfo gElementTypeInfo[];

unsigned GetSizeForCorElementType(CorElementType etyp);
const ElementTypeInfo* GetElementTypeInfo(CorElementType etyp);
BOOL    IsFP(CorElementType etyp);
BOOL    IsBaseElementType(CorElementType etyp);

//----------------------------------------------------------------------------
// enum StringType
// defines the various string types
enum StringType
{
    enum_WSTR = 0,
    enum_CSTR = 1,
};


//@GENERICS: flags returned from IsPolyType indicating the presence or absence of class and 
// method type parameters in a type whose instantiation cannot be determined at JIT-compile time
enum VarKind
{
  hasNoVars = 0,
  hasClassVar = 1,
  hasMethodVar = 2,
};

//------------------------------------------------------------------------
// Encapsulates how compressed integers and typeref tokens are encoded into
// a bytestream.
//
//------------------------------------------------------------------------
class SigPointer
{
    private:
        PCCOR_SIGNATURE m_ptr;

    public:
        //------------------------------------------------------------------------
        // Constructor.
        //------------------------------------------------------------------------
        SigPointer() {}

        //------------------------------------------------------------------------
        // Initialize 
        //------------------------------------------------------------------------
        FORCEINLINE SigPointer(PCCOR_SIGNATURE ptr)
        {
            m_ptr = ptr;
        }

        FORCEINLINE void SetSig(PCCOR_SIGNATURE ptr)
        {
            m_ptr = ptr;
        }

        //------------------------------------------------------------------------
        // Remove one compressed integer (using CorSigUncompressData) from
        // the head of the stream and return it.
        //------------------------------------------------------------------------
        FORCEINLINE ULONG GetData()
        {
            return CorSigUncompressData(m_ptr);
        }


        //-------------------------------------------------------------------------
        // Remove one byte and return it.
        //-------------------------------------------------------------------------
        FORCEINLINE BYTE GetByte()
        {
            return *(m_ptr++);
        }


        FORCEINLINE CorElementType GetElemType()
        {
            return (CorElementType) CorSigEatCustomModifiersAndUncompressElementType(m_ptr);
        }

        ULONG GetCallingConvInfo()  
        {   
            return CorSigUncompressCallingConv(m_ptr);  
        }   

        ULONG GetCallingConv()  
        {   
            return IMAGE_CEE_CS_CALLCONV_MASK & CorSigUncompressCallingConv(m_ptr); 
        }   

        //------------------------------------------------------------------------
        // Non-destructive read of compressed integer.
        //------------------------------------------------------------------------
        ULONG PeekData() const
        {
            PCCOR_SIGNATURE tmp = m_ptr;
            return CorSigUncompressData(tmp);
        }


        //------------------------------------------------------------------------
        // Non-destructive read of element type.
        //
        // This routine makes it look as if the String type is encoded
        // via ELEMENT_TYPE_CLASS followed by a token for the String class,
        // rather than the ELEMENT_TYPE_STRING. This is partially to avoid
        // rewriting client code which depended on this behavior previously.
        // But it also seems like the right thing to do generally.
        //------------------------------------------------------------------------
        CorElementType PeekElemType() const
        {
            PCCOR_SIGNATURE tmp = m_ptr;
            CorElementType typ = CorSigEatCustomModifiersAndUncompressElementType(tmp);
            if (typ == ELEMENT_TYPE_STRING || typ == ELEMENT_TYPE_OBJECT)
                return ELEMENT_TYPE_CLASS;
            return typ;
        }


        //------------------------------------------------------------------------
        // Removes a compressed metadata token and returns it.
        //------------------------------------------------------------------------
        FORCEINLINE mdTypeRef GetToken()
        {
            return CorSigUncompressToken(m_ptr);
        }


        //------------------------------------------------------------------------
        // Tests if two SigPointers point to the same location in the stream.
        //------------------------------------------------------------------------
        FORCEINLINE BOOL Equals(SigPointer sp) const
        {
            return m_ptr == sp.m_ptr;
        }


        //------------------------------------------------------------------------
        // Assumes that the SigPointer points to the start of an element type
        // (i.e. function parameter, function return type or field type.)
        // Advances the pointer to the first data after the element type.  This
        // will skip the following varargs sentinal if it is there.
        //------------------------------------------------------------------------
        VOID Skip();

        //------------------------------------------------------------------------
        // Like Skip, but will not skip a following varargs sentinal.
        //------------------------------------------------------------------------
        VOID SkipExactlyOne();

        //------------------------------------------------------------------------
        // Skip a sub signature (as immediately follows an ELEMENT_TYPE_FNPTR).
        //------------------------------------------------------------------------
        VOID SkipSignature();


        //------------------------------------------------------------------------
        // Get info about single-dimensional arrays
        //------------------------------------------------------------------------
        VOID GetSDArrayElementProps(SigPointer *pElemType, ULONG *pElemCount) const;


        //------------------------------------------------------------------------
        // Move signature to another scope
        //------------------------------------------------------------------------
        ULONG GetImportSignature(IMetaDataImport *pInputScope,
                                 IMetaDataAssemblyImport *pAssemblyInputScope,
                                 IMetaDataEmit *pEmitScope, 
                                 IMetaDataAssemblyEmit *pAssemblyEmitScope, 
                                 PCOR_SIGNATURE buffer, 
                                 PCOR_SIGNATURE bufferMax);
    
        ULONG GetImportFunctionSignature(IMetaDataImport *pInputScope,
                                         IMetaDataAssemblyImport *pAssemblyInputScope,
                                         IMetaDataEmit *pEmitScope, 
                                         IMetaDataAssemblyEmit *pAssemblyEmitScope, 
                                         PCOR_SIGNATURE buffer, 
                                         PCOR_SIGNATURE bufferMax);

// This functionality needs to "know" about internal VM structures (like EEClass).
// It is conditionally included so that other projects can use the rest of the
// functionality in this file.

#ifdef COMPLUS_EE
        // Use classInst and methodInst to look up class type params and method type params
        // Also reduce instantiated types to CLASS or VALUETYPE
        CorElementType Normalize(Module* pModule, TypeHandle *classInst = NULL, TypeHandle *methodInst = NULL) const;
        CorElementType Normalize(Module* pModule, CorElementType type, TypeHandle *classInst = NULL, TypeHandle *methodInst = NULL) const;

        FORCEINLINE CorElementType PeekElemTypeNormalized(Module* pModule, TypeHandle *classInst = NULL, TypeHandle *methodInst = NULL) const {
            return Normalize(pModule, classInst, methodInst);
        }

        //------------------------------------------------------------------------
        // Assumes that the SigPointer points to the start of an element type.
        // Returns size of that element in bytes. This is the minimum size that a
        // field of this type would occupy inside an object. 
        //------------------------------------------------------------------------
        UINT SizeOf(Module* pModule, TypeHandle *classInst = NULL, TypeHandle *methodInst = NULL) const;
        UINT SizeOf(Module* pModule, CorElementType type, TypeHandle *classInst = NULL, TypeHandle *methodInst = NULL) const;

        //------------------------------------------------------------------------
        // Assuming that the SigPointer points the start if an element type.
        // Use classInst to fill in any class type parameters
        // Use methodInst to fill in any method type parameters
        // Don't load anything in the pending list: just look them up
        //------------------------------------------------------------------------
        TypeHandle GetTypeHandle(Module* pModule,OBJECTREF *pThrowable=NULL, 
                                 BOOL dontRestoreTypes=FALSE,
                                 BOOL dontLoadTypes=FALSE,
                                 BOOL approxTypes=FALSE, // Approximate all reference types by Object
                                 TypeHandle* classInst=NULL, TypeHandle *methodInst = NULL, 
                                 Pending* pending=NULL, Substitution *pSubst = NULL) const;

        // Does this type contain class or method type parameters whose instantiation cannot
        // be determined at JIT-compile time from the instantiations in the method context? 
        // Return a combination of hasClassVar and hasMethodVar flags.
        //
        // Example: class C<A,B> containing instance method m<T,U>
        // Suppose that the method context is C<float,string>::m<double,object>
        // Then the type Dict<!0,!!0> is considered to have *no* "polymorphic" type parameters because 
        // !0 is known to be float and !!0 is known to be double
        // But Dict<!1,!!1> has polymorphic class *and* method type parameters because both
        // !1=string and !!1=object are reference types and so code using these can be shared with
        // other reference instantiations.
        VarKind IsPolyType(MethodDesc* pContextMD) const;

        // return the canonical name for the type pointed to by the sigPointer into
        // the buffer 'buff'.  'buff' is of length 'buffLen'.  Return the lenght of
        // the string returned.  Return 0 on failure
        // Use the instantiation to fill in any type variables
        unsigned GetNameForType(Module* pModule, LPUTF8 buff, unsigned buffLen, TypeHandle *classInst = NULL, TypeHandle *methodInst = NULL) const;

        //------------------------------------------------------------------------
        // Tests if the element type is a System.String. Accepts
        // either ELEMENT_TYPE_STRING or ELEMENT_TYPE_CLASS encoding.
        //------------------------------------------------------------------------
        BOOL IsStringType(Module* pModule, TypeHandle *classInst = NULL, TypeHandle *methodInst = NULL) const;


        //------------------------------------------------------------------------
        // Tests if the element class name is szClassName. 
        //------------------------------------------------------------------------
        BOOL IsClass(Module* pModule, LPCUTF8 szClassName, TypeHandle *classInst = NULL, TypeHandle *methodInst = NULL) const;

        //------------------------------------------------------------------------
        // Tests for the existence of a custom modifier
        //------------------------------------------------------------------------
        BOOL HasCustomModifier(Module *pModule, LPCSTR szModName, CorElementType cmodtype) const;

        //------------------------------------------------------------------------
        // Return pointer
        //------------------------------------------------------------------------
        PCCOR_SIGNATURE GetPtr() const
        {
            return m_ptr;
        }

#endif // COMPLUS_EE

};


//------------------------------------------------------------------------
// Encapsulates the format and simplifies walking of MetaData sigs.
//------------------------------------------------------------------------
class ExpandSig;

#ifdef _DEBUG
#define MAX_CACHED_SIG_SIZE     3       // To excercize non-cached code path
#else
#define MAX_CACHED_SIG_SIZE     15
#endif

#define SIG_OFFSETS_INITTED     0x0001
#define SIG_RET_TYPE_INITTED    0x0002


//------------------------------------------------------------------------
// A substitution represents the composition of several formal type instantiations
// It is used when matching formal signatures across the inheritance hierarchy.
//
// It has the form of a linked list:
//   [mod_1, <inst_1>] ->
//   [mod_2, <inst_2>] ->
//   ...
//   [mod_n, <inst_n>]
//
// Here the types in <inst_1> must be resolved in the scope of module mod_1 but
// may contain type variables instantiated by <inst_2>
// ...
// and the types in <inst_(n-1)> must be resolved in the scope of mould mod_(n-1) but
// may contain type variables instantiated by <inst_n>
//
// Any type variables in <inst_n> are treated as "free".
//------------------------------------------------------------------------
class Substitution
{
public:
  Module* pModule;        // Module in which instantiation lives (needed to resolve typerefs)
  PCCOR_SIGNATURE inst;   // Pointer to instantiation part of ELEMENT_TYPE_WITH (following no. of params)
  Substitution* rest;

  Substitution(Module* pModuleArg, PCCOR_SIGNATURE instArg) { pModule = pModuleArg; inst = instArg; rest = NULL; }
  Substitution(Module* pModuleArg, PCCOR_SIGNATURE instArg, Substitution *restArg) { pModule = pModuleArg; inst = instArg; rest = restArg; }
};

class MetaSig
{
    friend class ArgIterator;
    friend class ExpandSig;
    public:
        enum MetaSigKind { 
            sigMember, 
            sigLocalVars,
            sigField
            };

        //------------------------------------------------------------------
        // Constructor. Warning: Does NOT make a copy of szMetaSig.
        // The instantiation will be used to fill in type variables on calls
        // to GetTypeHandle and GetRetTypeHandle
        // WARNING: make sure you know what you're doing by leaving classInst and methodInst to default NULL
        // Are you sure the signature cannot be generic?
        //------------------------------------------------------------------
        MetaSig(PCCOR_SIGNATURE szMetaSig, Module* pModule, TypeHandle *classInst = NULL, TypeHandle *methodInst = NULL, 
          BOOL fConvertSigAsVarArg = FALSE, MetaSigKind kind = sigMember, BOOL fParamTypeArg = FALSE);

        // If classInst/methodInst are omitted then *representative* instantiations are
        // obtained from pMD
        MetaSig(MethodDesc *pMD, TypeHandle *classInst = NULL, TypeHandle *methodInst = NULL);

        // If classInst is omitted then a *representative* instantiation is obtained from pFD
        MetaSig(FieldDesc *pFD, TypeHandle *classInst = NULL);

        //------------------------------------------------------------------
        // Constructor. Fast copy of bytes out of an ExpandSig, for thread-
        // safety reasons.
        //------------------------------------------------------------------
        MetaSig(ExpandSig &shared);

        //------------------------------------------------------------------
        // Constructor. Copy state from existing MetaSig (does not deep copy
        // zsMetaSig). Iterator fields are reset.
        //------------------------------------------------------------------
        MetaSig(MetaSig *pSig) { memcpy(this, pSig, sizeof(MetaSig)); Reset(); }

        void GetRawSig(BOOL fIsStatic, PCCOR_SIGNATURE *pszMetaSig, DWORD *cbSize);

    //------------------------------------------------------------------
        // Returns type of current argument, then advances the argument
        // index. Returns ELEMENT_TYPE_END if already past end of arguments.
        //------------------------------------------------------------------
        CorElementType NextArg();

        //------------------------------------------------------------------
        // Retreats argument index, then returns type of the argument
        // under the new index. Returns ELEMENT_TYPE_END if already at first
        // argument.
        //------------------------------------------------------------------
        CorElementType PrevArg();

        //------------------------------------------------------------------
        // Returns type of current argument index. Returns ELEMENT_TYPE_END if already past end of arguments.
        //------------------------------------------------------------------
        CorElementType PeekArg();

        //------------------------------------------------------------------
        // Returns a read-only SigPointer for the last type to be returned
        // via NextArg() or PrevArg(). This allows extracting more information
        // for complex types.
        //------------------------------------------------------------------
        const SigPointer & GetArgProps() const
        {
            return m_pLastType;
        }

        //------------------------------------------------------------------
        // Returns a read-only SigPointer for the return type.
        // This allows extracting more information for complex types.
        //------------------------------------------------------------------
        const SigPointer & GetReturnProps() const
        {
            return m_pRetType;
        }


        //------------------------------------------------------------------------
        // Returns # of arguments. Does not count the return value.
        // Does not count the "this" argument (which is not reflected om the
        // sig.) 64-bit arguments are counted as one argument.
        //------------------------------------------------------------------------
        static UINT NumFixedArgs(Module* pModule, PCCOR_SIGNATURE pSig);

        //------------------------------------------------------------------------
        // Returns # of arguments. Does not count the return value.
        // Does not count the "this" argument (which is not reflected om the
        // sig.) 64-bit arguments are counted as one argument.
        //------------------------------------------------------------------------
        UINT NumFixedArgs()
        {
            return m_nArgs;
        }
        
        //----------------------------------------------------------
        // Returns the calling convention (see IMAGE_CEE_CS_CALLCONV_*
        // defines in cor.h)
        //----------------------------------------------------------
        static BYTE GetCallingConvention(Module* pModule, PCCOR_SIGNATURE pSig)
        {
            return (BYTE)(IMAGE_CEE_CS_CALLCONV_MASK & (CorSigUncompressCallingConv(/*modifies*/pSig)));
        }

        //----------------------------------------------------------
        // Returns the calling convention (see IMAGE_CEE_CS_CALLCONV_*
        // defines in cor.h)
        //----------------------------------------------------------
        static BYTE GetCallingConventionInfo(Module* pModule, PCCOR_SIGNATURE pSig)
        {
            return (BYTE)CorSigUncompressCallingConv(/*modifies*/pSig);
        }

        //----------------------------------------------------------
        // Returns the calling convention (see IMAGE_CEE_CS_CALLCONV_*
        // defines in cor.h)
        //----------------------------------------------------------
        BYTE GetCallingConvention()
        {
            return m_CallConv & IMAGE_CEE_CS_CALLCONV_MASK; 
        }

        //----------------------------------------------------------
        // Returns the calling convention & flags (see IMAGE_CEE_CS_CALLCONV_*
        // defines in cor.h)
        //----------------------------------------------------------
        BYTE GetCallingConventionInfo()
        {
            return m_CallConv;
        }

        //----------------------------------------------------------
        // Has a 'this' pointer?
        //----------------------------------------------------------
        BOOL HasThis()
        {
            return m_CallConv & IMAGE_CEE_CS_CALLCONV_HASTHIS;
        }  

        //----------------------------------------------------------
        // Is a generic method with explicit arity?
        //----------------------------------------------------------
        BOOL IsGenericMethod()
        {
            return m_CallConv & IMAGE_CEE_CS_CALLCONV_GENERIC;
        }  

        //----------------------------------------------------------
        // Is vararg?
        //----------------------------------------------------------
        BOOL IsVarArg()
        {
            return GetCallingConvention() == IMAGE_CEE_CS_CALLCONV_VARARG;
        }

        //----------------------------------------------------------
        // Is vararg?
        //----------------------------------------------------------
        static BOOL IsVarArg(Module* pModule, PCCOR_SIGNATURE pSig)
        {
            return GetCallingConvention(pModule, pSig) == IMAGE_CEE_CS_CALLCONV_VARARG;
        }



#ifdef COMPLUS_EE
        Module* GetModule() const {
            return m_pModule;
        }
            
        //----------------------------------------------------------
        // Returns the unmanaged calling convention.
        //----------------------------------------------------------
        static CorPinvokeMap GetUnmanagedCallingConvention(Module *pModule, PCCOR_SIGNATURE pSig, ULONG cSig);

        //------------------------------------------------------------------
        // Like NextArg, but return only normalized type (enums flattned to 
        // underlying type ...
        //------------------------------------------------------------------
        CorElementType NextArgNormalized() {
            m_pLastType = m_pWalk;
            if (m_iCurArg == m_nArgs)
            {
                return ELEMENT_TYPE_END;
            }
            else
            {
                m_iCurArg++;
                CorElementType mt = m_pWalk.Normalize(m_pModule, m_classInst, m_methodInst);
                m_pWalk.Skip();
                return mt;
            }
        }

        CorElementType NextArgNormalized(UINT32 *size) {
            m_pLastType = m_pWalk;
            if (m_iCurArg == m_nArgs)
            {
                return ELEMENT_TYPE_END;
            }
            else
            {
                m_iCurArg++;
                CorElementType type = m_pWalk.PeekElemType();
                CorElementType mt = m_pWalk.Normalize(m_pModule, type, m_classInst, m_methodInst);
                *size = m_pWalk.SizeOf(m_pModule, type, m_classInst, m_methodInst);
                m_pWalk.Skip();
                return mt;
            }
        }
        //------------------------------------------------------------------
        // Like NextArg, but return only normalized type (enums flattned to 
        // underlying type ...
        //------------------------------------------------------------------
        CorElementType PeekArgNormalized();

        // Is there a hidden parameter for the return parameter.  

        BOOL HasRetBuffArg() const
        {
            BOOL fResult;
            CorElementType type = GetReturnTypeNormalized();
            fResult = (type == ELEMENT_TYPE_VALUETYPE || type == ELEMENT_TYPE_TYPEDBYREF);
            return fResult;
        }


        //------------------------------------------------------------------------
        // Tests if the return type is an object ref 
        //------------------------------------------------------------------------
        BOOL IsObjectRefReturnType()
        {
           switch (GetReturnTypeNormalized())
            {
            case ELEMENT_TYPE_CLASS:
            case ELEMENT_TYPE_SZARRAY:
            case ELEMENT_TYPE_ARRAY:
            case ELEMENT_TYPE_STRING:
            case ELEMENT_TYPE_OBJECT:
            case ELEMENT_TYPE_VAR:
                return TRUE;
            default:
                break;
            }
           return FALSE;
        }

        static UINT GetFPReturnSize(Module* pModule, PCCOR_SIGNATURE pSig, TypeHandle *classInst, TypeHandle *methodInst)
        {
            MetaSig msig(pSig, pModule, classInst, methodInst);
            CorElementType rt = msig.GetReturnTypeNormalized();
            return rt == ELEMENT_TYPE_R4 ? 4 : 
                   rt == ELEMENT_TYPE_R8 ? 8 : 0;

        }

        UINT GetReturnTypeSize()
        {    
            return m_pRetType.SizeOf(m_pModule, m_classInst, m_methodInst);
        }

        static int GetReturnTypeSize(Module* pModule, PCCOR_SIGNATURE pSig, TypeHandle *classInst, TypeHandle *methodInst) 
        {
            MetaSig msig(pSig, pModule, classInst, methodInst);
            return msig.GetReturnTypeSize();
        }

        int GetLastTypeSize() 
        {
            return m_pLastType.SizeOf(m_pModule, m_classInst, m_methodInst);
        }

        //------------------------------------------------------------------
        // Perform type-specific GC promotion on the value (based upon the
        // last type retrieved by NextArg()).
        //------------------------------------------------------------------
        VOID GcScanRoots(LPVOID pValue, promote_func *fn, ScanContext* sc);

        //------------------------------------------------------------------
        // Is the return type 64 bit?
        //------------------------------------------------------------------
        BOOL Is64BitReturn() const
        {
            CorElementType rt = GetReturnTypeNormalized();
            return (rt == ELEMENT_TYPE_I8 || rt == ELEMENT_TYPE_U8 || rt == ELEMENT_TYPE_R8);
        }
#endif
        //------------------------------------------------------------------
        // Moves index to end of argument list.
        //------------------------------------------------------------------
        VOID GotoEnd();

        //------------------------------------------------------------------
        // reset: goto start pos
        //------------------------------------------------------------------
        VOID Reset();

        //------------------------------------------------------------------
        // Returns type of return value.
        //------------------------------------------------------------------
        FORCEINLINE CorElementType GetReturnType() const
        {
            return m_pRetType.PeekElemType();
        }



// This functionality needs to "know" about internal VM structures (like EEClass).
// It is conditionally included so that other projects can use the rest of the
// functionality in this file.

#ifdef COMPLUS_EE
        FORCEINLINE CorElementType GetReturnTypeNormalized() const
        {
            
            if (m_fCacheInitted & SIG_RET_TYPE_INITTED)
                return m_corNormalizedRetType;
            MetaSig *tempSig = (MetaSig *)this;
            tempSig->m_corNormalizedRetType = m_pRetType.Normalize(m_pModule, m_classInst, m_methodInst);
            tempSig->m_fCacheInitted |= SIG_RET_TYPE_INITTED;
            return tempSig->m_corNormalizedRetType;
        }

        //------------------------------------------------------------------
        // Determines if the current argument is System/String.
        // Caller must determine first that the argument type is ELEMENT_TYPE_CLASS.
        //------------------------------------------------------------------
        BOOL IsStringType() const;

        //------------------------------------------------------------------
        // Determines if the current argument is a particular class.
        // Caller must determine first that the argument type is ELEMENT_TYPE_CLASS.
        //------------------------------------------------------------------
        BOOL IsClass(LPCUTF8 szClassName) const;


        //------------------------------------------------------------------
        // Determines if the return type is System/String.
        // Caller must determine first that the return type is ELEMENT_TYPE_CLASS.
        //------------------------------------------------------------------
        BOOL IsStringReturnType() const;

        //------------------------------------------------------------------
        // If the last thing returned was an Object
        //  this method will return the TypeHandle for the class
        //------------------------------------------------------------------
        TypeHandle GetTypeHandle(OBJECTREF *pThrowable = NULL, 
                                 BOOL dontRestoreTypes=FALSE,
                                 BOOL dontLoadTypes=FALSE) const
        {
             return m_pLastType.GetTypeHandle(m_pModule, pThrowable, dontRestoreTypes, dontLoadTypes, FALSE, m_classInst, m_methodInst);
        }

        //------------------------------------------------------------------
        // If the Return type is an Object 
        //  this method will return the TypeHandle for the class
        //------------------------------------------------------------------
        TypeHandle GetRetTypeHandle(OBJECTREF *pThrowable = NULL,
                                    BOOL dontRestoreTypes = FALSE,
                                    BOOL dontLoadTypes = FALSE) const
        {
             return m_pRetType.GetTypeHandle(m_pModule, pThrowable, dontRestoreTypes, dontLoadTypes, FALSE, m_classInst, m_methodInst);
        }

        //------------------------------------------------------------------
        // GetByRefType
        //  returns the base type of the reference
        // and for object references, class of the reference
        // the in-out info for this byref param
        //------------------------------------------------------------------
        CorElementType GetByRefType(TypeHandle* pTy, OBJECTREF *pThrowable = NULL) const;

        // Compare types in two signatures, first applying
        // - optional substitutions pSubst1 and pSubst2
        //   to class type parameters (E_T_VAR) in the respective signatures
        // - optional instantiations pMethodInst1 and pMethodInst2 
        //   to method type parameters (E_T_MVAR) in the respective signatures
        static BOOL CompareElementType(PCCOR_SIGNATURE &pSig1,   PCCOR_SIGNATURE &pSig2, 
                                       PCCOR_SIGNATURE pEndSig1, PCCOR_SIGNATURE pEndSig2, 
                                       Module*         pModule1, Module*         pModule2, 
                                       Substitution   *pSubst1,  Substitution   *pSubst2,
                                       SigPointer *pMethodInst1 = NULL, SigPointer *pMethodInst2 = NULL);

        // Compare two complete method signatures, first applying optional substitutions pSubst1 and pSubst2
        // to class type parameters (E_T_VAR) in the respective signatures
        static BOOL CompareMethodSigs(
            PCCOR_SIGNATURE pSig1, 
            DWORD       cSig1, 
            Module*     pModule1, 
            Substitution* pSubst1,
            PCCOR_SIGNATURE pSig2, 
            DWORD       cSig2, 
            Module*     pModule2,
            Substitution* pSubst2
        );

        static BOOL CompareFieldSigs(
            PCCOR_SIGNATURE pSig1, 
            DWORD       cSig1, 
            Module*     pModule1, 
            PCCOR_SIGNATURE pSig2, 
            DWORD       cSig2, 
            Module*     pModule2
        );

        //------------------------------------------------------------------------
        // Returns # of stack bytes required to create a call-stack using
        // the internal calling convention.
        // Includes indication of "this" pointer since that's not reflected
        // in the sig.
        //------------------------------------------------------------------------
        static UINT SizeOfVirtualFixedArgStack(Module* pModule, PCCOR_SIGNATURE szMetaSig, BOOL fIsStatic, 
          TypeHandle *classInst, TypeHandle *methodInst);
        static UINT SizeOfActualFixedArgStack(Module* pModule, PCCOR_SIGNATURE szMetaSig, BOOL fIsStatic,
          TypeHandle *classInst, TypeHandle *methodInst, BOOL fParamTypeArg = FALSE, int *paramTypeReg = NULL);


        //------------------------------------------------------------------------
        // Returns # of argument slots required to create a MethodDesc call using
        // the internal calling convention.
        // Includes indication of "this" pointer since that's not reflected
        // in the sig.
        //------------------------------------------------------------------------
        static UINT NumVirtualFixedArgs(Module* pModule, PCCOR_SIGNATURE szMetaSig, BOOL fIsStatic);

        //------------------------------------------------------------------------
        // Returns # of stack bytes to pop upon return.
        // Includes indication of "this" pointer since that's not reflected
        // in the sig.
        //------------------------------------------------------------------------
        static UINT CbStackPop(Module* pModule, PCCOR_SIGNATURE szMetaSig, BOOL fIsStatic, TypeHandle *classInst, TypeHandle *methInst)
        {
#if defined(_X86_) || defined(_AMD64_)
            if (MetaSig::IsVarArg(pModule, szMetaSig))
            {
                return 0;
            }
            else
            {
                return SizeOfActualFixedArgStack(pModule, szMetaSig, fIsStatic, classInst, methInst);
            }
#else
            // no meaning on other platforms
            return 0;
#endif
        }

        //------------------------------------------------------------------
        // Ensures that all the value types in the sig are loaded. This
        // should be called on sig's that have value types before they
        // are passed to Call(). This ensures that value classes will not
        // be loaded during the operation to determine the size of the
        // stack. Thus preventing the resulting GC hole.
        //------------------------------------------------------------------
        static void EnsureSigValueTypesLoaded(MethodDesc *pMD)
        {
            MetaSig(pMD).ForceSigWalk(FALSE);
        }

        // this walks the sig and checks to see if all  types in the sig can be loaded
        static void CheckSigTypesCanBeLoaded(PCCOR_SIGNATURE pSig, Module *pModule);

        // See the comments about thread-safety in ForceSigWalk to understand why
        // this predicate cannot be arbitrarily changed to some other member.
        BOOL NeedsSigWalk()
        {
            return (m_nVirtualStack == (UINT32) -1);
        }

        //------------------------------------------------------------------------
        // The following three routines are the same as the above routines except
        //  they are called on the MetaSig which will cache these values
        //------------------------------------------------------------------------

        UINT SizeOfVirtualFixedArgStack(BOOL fIsStatic)
        {
            if (NeedsSigWalk())
                ForceSigWalk(fIsStatic);
            _ASSERTE(!!fIsStatic == !!m_WalkStatic);    // booleanize
            return m_nVirtualStack;
        }


        UINT SizeOfActualFixedArgStack(BOOL fIsStatic)
        {
            if (NeedsSigWalk())
                ForceSigWalk(fIsStatic);
            _ASSERTE(!!fIsStatic == !!m_WalkStatic);    // booleanize
            return m_nActualStack;
        }

        UINT NumVirtualFixedArgs(BOOL fIsStatic)
        {
            if (NeedsSigWalk())
                ForceSigWalk(fIsStatic);
            _ASSERTE(!!fIsStatic == !!m_WalkStatic);    // booleanize
            return m_nNumVirtualFixedArgs;
        }

        //------------------------------------------------------------------------

        UINT CbStackPop(BOOL fIsStatic)
        {
#if defined(_X86_) || defined(_AMD64_)
            if (IsVarArg())
            {
                return 0;
            }
            else
            {
                return SizeOfActualFixedArgStack(fIsStatic);
            }
#else
            // no meaning on other platforms
            return 0;
#endif
        }
        
        UINT GetFPReturnSize()
        {
            CorElementType rt = GetReturnTypeNormalized();
            return rt == ELEMENT_TYPE_R4 ? 4 : 
                   rt == ELEMENT_TYPE_R8 ? 8 : 0;
        }

        void ForceSigWalk(BOOL fIsStatic);

        TypeHandle *GetMethodInst() { return m_methodInst; }
        TypeHandle *GetClassInst() { return m_classInst; }

        static ULONG GetSignatureForTypeHandle(IMetaDataAssemblyEmit *pAssemblyEmitScope,
                                               IMetaDataEmit *pEmitScope,
                                               TypeHandle type,
                                               PCOR_SIGNATURE buffer,
                                               PCOR_SIGNATURE bufferMax);

        static mdToken GetTokenForTypeHandle(IMetaDataAssemblyEmit *pAssemblyEmitScope,
                                             IMetaDataEmit *pEmitScope,
                                             TypeHandle type);

#endif  // COMPLUS_EE


    // These are protected because Reflection subclasses Metasig
    protected:

    static const UINT32 s_cSigHeaderOffset;

        Module*      m_pModule;
        SigPointer   m_pStart;
        SigPointer   m_pWalk;
        SigPointer   m_pLastType;
        SigPointer   m_pRetType;
        UINT32       m_nArgs;
        UINT32       m_iCurArg;
    UINT32       m_cbSigSize;
    PCCOR_SIGNATURE m_pszMetaSig;


        // The following are cached so we don't the signature
        //  multiple times
        UINT32       m_nVirtualStack;   // Size of the virtual stack
        UINT32       m_nActualStack;    // Size of the actual stack
        UINT32       m_nNumVirtualFixedArgs;   // number of virtual fixed arguments
        BYTE         m_CallConv;
        BYTE         m_WalkStatic;      // The type of function we walked

        TypeHandle  *m_classInst;         // Instantiation for class type parameters
        TypeHandle  *m_methodInst;        // Instantiation for method type parameters
        BYTE            m_types[MAX_CACHED_SIG_SIZE + 1];
        short           m_sizes[MAX_CACHED_SIG_SIZE + 1];
        short           m_offsets[MAX_CACHED_SIG_SIZE + 1];
        CorElementType  m_corNormalizedRetType;
        DWORD           m_fCacheInitted;

            // used to treat some sigs as special case vararg
            // used by calli to unmanaged target
        BYTE         m_fTreatAsVarArg;
        BOOL        IsTreatAsVarArg()
        {
                    return m_fTreatAsVarArg;
        }
};

//@GENERICS: 
// DEPRECATED because it doesn't handle instantiations
// Instead use MetaSig with sigField or via the FieldDesc constructor
class FieldSig
{
    // For new-style signatures only.
    SigPointer m_pStart;
    Module*    m_pModule;
public:
        //------------------------------------------------------------------
        // Constructor. Warning: Does NOT make a copy of szMetaSig.
        //------------------------------------------------------------------
        
        FieldSig(PCCOR_SIGNATURE szMetaSig, Module* pModule)
        {
            _ASSERTE(*szMetaSig == IMAGE_CEE_CS_CALLCONV_FIELD);
            m_pModule = pModule;
            m_pStart = SigPointer(szMetaSig);
            m_pStart.GetData();     // Skip "calling convention"
        }
        //------------------------------------------------------------------
        // Returns type of the field
        //------------------------------------------------------------------
        CorElementType GetFieldType()
        {
            return m_pStart.PeekElemType();
        }


// This functionality needs to "know" about internal VM structures (like EEClass).
// It is conditionally included so that other projects can use the rest of the
// functionality in this file.

#ifdef COMPLUS_EE

        CorElementType GetFieldTypeNormalized() const
        {
            return m_pStart.Normalize(m_pModule);
        }

        //------------------------------------------------------------------
        // If the last thing returned was an Object
        //  this method will return the TypeHandle for the class
        //------------------------------------------------------------------
        TypeHandle GetTypeHandle(OBJECTREF *pThrowable = NULL, 
                                 BOOL dontRestoreTypes = FALSE,
                                 BOOL dontLoadTypes = FALSE) const
        {
             return m_pStart.GetTypeHandle(m_pModule, pThrowable, dontRestoreTypes, dontLoadTypes);
        }

        //------------------------------------------------------------------
        // Determines if the current argument is a particular class.
        // Caller must determine first that the argument type is ELEMENT_TYPE_CLASS.
        //------------------------------------------------------------------
        BOOL IsClass(LPCUTF8 szClassName) const
        {
            return m_pStart.IsClass(m_pModule, szClassName, NULL, NULL);
        }

        //------------------------------------------------------------------
        // Determines if the current argument is System/String.
        // Caller must determine first that the argument type is ELEMENT_TYPE_CLASS.
        //------------------------------------------------------------------
        BOOL IsStringType() const;

        //------------------------------------------------------------------
        // Returns a read-only SigPointer for extracting more information
        // for complex types.
        //------------------------------------------------------------------
        const SigPointer & GetProps() const
        {
            return m_pStart;
        }

#endif // COMPLUS_EE

};




//=========================================================================
// Indicates whether an argument is to be put in a register using the
// default IL calling convention. This should be called on each parameter
// in the order it appears in the call signature. For a non-static method,
// this function should also be called once for the "this" argument, prior
// to calling it for the "real" arguments. Pass in a typ of IMAGE_CEE_CS_OBJECT.
//
//  *pNumRegistersUsed:  [in,out]: keeps track of the number of argument
//                       registers assigned previously. The caller should
//                       initialize this variable to 0 - then each call
//                       will update it.
//
//  typ:                 the signature type
//  structSize:          for structs, the size in bytes
//  fThis:               is this about the "this" pointer?
//  callconv:            see IMAGE_CEE_CS_CALLCONV_*
//  *pOffsetIntoArgumentRegisters:
//                       If this function returns TRUE, then this out variable
//                       receives the identity of the register, expressed as a
//                       byte offset into the ArgumentRegisters structure.
//
// 
//=========================================================================
BOOL IsArgumentInRegister(int   *pNumRegistersUsed,
                          BYTE   typ,
                          UINT32 structSize,
                          BOOL   fThis,
                          BYTE   callconv,
                          int    *pOffsetIntoArgumentRegisters);

#ifdef COMPLUS_EE


/*****************************************************************/
/* CorTypeInfo is a single global table that you can hang information
   about ELEMENT_TYPE_* */

class CorTypeInfo {
public:
    static LPCUTF8 GetFullName(CorElementType type) {
        _ASSERTE(type < infoSize);
        return info[type].className;
    }

    static void CheckConsistancy() {
        for(int i=0; i < infoSize; i++)
            _ASSERTE(info[i].type == i);
    }

    static CorInfoGCType GetGCType(CorElementType type) {
        _ASSERTE(type < infoSize);
        _ASSERTE(type != ELEMENT_TYPE_WITH);
        return info[type].gcType;
    }

    static BOOL IsObjRef(CorElementType type) {
        return (GetGCType(type) == TYPE_GC_REF);
    }

    static BOOL IsGenericVariable(CorElementType type) {
        return (type == ELEMENT_TYPE_VAR || type == ELEMENT_TYPE_MVAR);
    }

    static BOOL IsArray(CorElementType type) {
        _ASSERTE(type < infoSize);
        return info[type].isArray;
    }

    static BOOL IsFloat(CorElementType type) {
        _ASSERTE(type < infoSize);
        return info[type].isFloat;
    }

    static BOOL IsModifier(CorElementType type) {
        _ASSERTE(type < infoSize);
        return info[type].isModifier;
    }

    static BOOL IsPrimitiveType(CorElementType type) {
        _ASSERTE(type < infoSize);
        return info[type].isPrim;
    }

        // aways returns ELEMENT_TYPE_CLASS for object references (including arrays)
    static CorElementType Normalize(CorElementType type) {
        if (IsObjRef(type))
            return(ELEMENT_TYPE_CLASS); 
        return(type);
    }

    static unsigned Size(CorElementType type) {
        _ASSERTE(type < infoSize);
        _ASSERTE(type != ELEMENT_TYPE_WITH);
        return info[type].size;
    }

    static CorElementType FindPrimitiveType(LPCUTF8 fullName);
    static CorElementType FindPrimitiveType(LPCUTF8 nameSp, LPCUTF8 name);
private:
    struct CorTypeInfoEntry {
        CorElementType type;
        LPCUTF8        className;
        unsigned       size         : 8;
        CorInfoGCType  gcType       : 3;
        unsigned       isArray      : 1;
        unsigned       isPrim       : 1;
        unsigned       isFloat      : 1;
        unsigned       isModifier   : 1;
    };

    static CorTypeInfoEntry info[];
    static const int infoSize;
};



BOOL CompareTypeTokens(mdToken tk1, mdToken tk2, Module *pModule1, Module *pModule2);


#endif // COMPLUS_EE


#ifndef COMPLUS_EE
#undef Module 
#endif

#endif /* _H_SIGINFO */



