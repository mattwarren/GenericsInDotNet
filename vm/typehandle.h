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
// File: TYPEHANDLE.H
//n
// ===========================================================================

#ifndef TYPEHANDLE_H
#define TYPEHANDLE_H

#include <member-offset-info.h>

/*************************************************************************/
// A TypeHandle is the FUNDAMENTAL concept of type identity in the COM+
// runtime.  That is two types are equal if and only if their type handles
// are equal.  A TypeHandle, is a pointer sized struture that encodes 
// everything you need to know to figure out what kind of type you are
// actually dealing with.  

// At the present time a TypeHandle can point at two possible things
//
//      1) A MethodTable    (in the case where 'normal' class with an unshared method table). 
//      2) A TypeDesc       (all other cases)  
//
// TypeDesc in turn break down into several variants.  To the extent
// possible, you should probably be using TypeHandles.  TypeDescs are
// for special cases around the edges
//    - array types whose method tables get share
//    - types for function pointers for verification and reflection
//    - types for generic parameters for verification and reflection
//

// Generic type instantiations (in C# syntax: C<ty_1,...,ty_n>) are represented by
// MethodTables, i.e. a new MethodTable gets allocated for each such instantiation.
// The entries in these tables (i.e. the code) are, however, often shared.
// Clients of TypeHandle don't need to know any of this detail; just use the
// GetInstantiation, GetNumGenericArgs, GetGenericTypeDefinition 
// and HasInstantiation methods.

class TypeDesc;
class ArrayTypeDesc;
class MethodTable;
class EEClass;
class Module;
class ExpandSig;
class Assembly;
class ReflectClass;
class BaseDomain;

#ifndef DEFINE_OBJECTREF
#define DEFINE_OBJECTREF
#ifdef _DEBUG
class OBJECTREF;
#else
class Object;
typedef Object *        OBJECTREF;
#endif
#endif

class TypeHandle 
{
public:
    TypeHandle() { 
        m_asPtr = 0; 
    }

    explicit TypeHandle(void* aPtr)     // somewhat unsafe, would be nice to get rid of
    { 
        m_asPtr = aPtr; 
        INDEBUG(Verify());
    }  

    TypeHandle(MethodTable* aMT) {
        m_asMT = aMT; 
        INDEBUG(Verify());
    }

    // only valid when EEClass is not shared between instantiations
    explicit TypeHandle(EEClass* aClass); 

    TypeHandle(TypeDesc *aType) {
        m_asInt = (((INT_PTR) aType) | 2); 
        INDEBUG(Verify());
    }

    int operator==(const TypeHandle& typeHnd) const {
        return(m_asPtr == typeHnd.m_asPtr);
    }

    int operator!=(const TypeHandle& typeHnd) const {
        return(m_asPtr != typeHnd.m_asPtr);
    }

        // Methods for probing exactly what kind of a type handle we have
    BOOL IsNull() const { 
        return(m_asPtr == 0); 
    }
    FORCEINLINE BOOL IsUnsharedMT() const {
        return((m_asInt & 2) == 0); 
    }
    BOOL IsTypeDesc() const  {
        return(!IsUnsharedMT());
    }

    BOOL IsEnum();

        // Methods to allow you get get a the two possible representations
    MethodTable* AsMethodTable() {        
        return(m_asMT);
    }

    TypeDesc* AsTypeDesc() {
        _ASSERTE(IsTypeDesc());
        return (TypeDesc*) (m_asInt & ~2);
    }

    // To the extent possible, you should try to use methods like the ones
    // below that treat all types uniformly.

    // Gets the size that this type would take up embedded in another object
    // thus objects all return sizeof(void*).  
    unsigned GetSize();

    // Store the full, correct, name for this type into the given buffer.  
    unsigned GetName(char* buff, unsigned buffLen);

    // Returns the ELEMENT_TYPE_* that you would use in a signature
    // The only normalization that happens is that for type handles
    // for instantiated types (e.g. class List<String> or
	// value type Pair<int,int>)) this returns either ELEMENT_TYPE_CLASS
 	// or ELEMENT_TYPE_VALUE, _not_ ELEMENT_TYPE_WITH.
    CorElementType GetSigCorElementType();
         
    // This helper:
    // - Will return enums underlying type
    // - Will return underlying primitive for System.Int32 etc...
    // - Will return underlying primitive as will be used in the calling convention
    //      For example
    //              struct t
    //              {
    //                  public int i;
    //              }
    //      will return ELEMENT_TYPE_I4 in x86 instead of ELEMENT_TYPE_VALUETYPE. We
    //      call this type of value type a primitive value type
    //
    // Internal representation is used among another things for the calling convention
    // (jit benefits of primitive value types) or optimizing marshalling.
    //
    // This will NOT convert E_T_ARRAY, E_T_SZARRAY etc. to E_T_CLASS (though it probably
    // should).  Use CorTypeInfo::IsObjRef for that.
    CorElementType GetNormCorElementType(); 

    // This helper will return the same as GetSignatueCorElementType except:
    // - Will return enums underlying type
    CorElementType GetVerifierCorElementType();

    // returns true of 'this' can be cast to 'type' 
    BOOL CanCastTo(TypeHandle type);
    
    // get the parent (superclass) of this type
    TypeHandle GetParent(); 

    // Obtain underlying uninstantiated generic type from an instantiated type
    // Null if not instantiated
    TypeHandle GetGenericTypeDefinition();

    // Obtain element type for an array or pointer, returning NULL otherwise
    TypeHandle GetTypeParam();

    // Obtain instantiation from an instantiated type
    // NULL if not instantiated
    TypeHandle* GetInstantiation();

    // Obtain instantiation from an instantiated type *or* a pointer to the element type for an array 
    TypeHandle* GetClassOrArrayInstantiation();

    // Is this type instantiated?
    BOOL HasInstantiation();

    // Is this an uninstantiated generic type?
    BOOL IsGenericTypeDefinition() {
      return !HasInstantiation() && GetNumGenericArgs() != 0;
    }

    // Strip off the instantiation if there is one
    TypeHandle StripInstantiation() {
      TypeHandle t = GetGenericTypeDefinition();
      if (t.IsNull())
          return *this;
      else
          return t;
    }

    // Obtain the "underlying" type that has a corresponding TypeDef
    // This is what determines a type's accessibility, namespace, assembly, etc.
    // First remove array & pointer qualifiers (e.g. string*[][] -> string)
    // Then remove instantiation (e.g. List<int>[]& -> List)
    TypeHandle GetTypeWithDef(); 

    // Given the current ShareRef and ShareVal settings, is the specified type representation-sharable as a type parameter to a generic type or method ?
    BOOL IsSharableRep();

    static BOOL IsSharableInstantiation(DWORD ntypars, TypeHandle *inst);

    // For an uninstantiated generic type, return the number of type parameters required for instantiation
    // For an instantiated type, return the number of type parameters in the instantiation
    // Otherwise return 0
    DWORD GetNumGenericArgs();

    // For an generic type instance return the representative within the class of
    // all type handles that share code.  For example, 
    //    <int> --> <int>,
    //    <object> --> <object>,
    //    <string> --> <object>,
    //    <List<string>> --> <object>,
    //    <Struct<string>> --> <Struct<object>>
    //
    // If the code for the type handle is not shared then return 
    // the type handle itself.
    TypeHandle GetCanonicalFormAsGenericArgument();

#ifdef _DEBUG
    static BOOL CheckInstantiationIsCanonical(DWORD ntypars, TypeHandle *inst);
#endif

    //@GENERICS: these are just indirected through EEClass; they're here as well to make things simple
    BOOL IsValueType();
    BOOL IsInterface();
    WORD GetNumVtableSlots();
    WORD GetNumMethodSlots();

        // Unlike the AsMethodTable, GetMethodTable, will get the method table
        // of the type, regardless of whether it is an array etc. Note, however
        // this method table may be shared, and some types (like TypeByRef), have
        // no method table 
    MethodTable* GetMethodTable();

    Module* GetModule();

    Assembly* GetAssembly();

    // BEWARE using this on instantiated types whose EEClass pointer may be shared
    // between compatible instantiations as described above
    EEClass* GetClass();

        // Shortcuts
    BOOL IsArray();
    BOOL IsGenericVariable();
    BOOL IsByRef();
    BOOL IsRestored();
    void CheckRestore();

    // Not clear we should have this.  
    ArrayTypeDesc* AsArray();

    EEClass* AsClass();                // Try not to use this one too much

    void* AsPtr() {                     // Please don't use this if you can avoid it
        return(m_asPtr); 
    }

    INDEBUG(BOOL Verify();)             // DEBUGGING Make certain this is a valid type handle 

#if CHECK_APP_DOMAIN_LEAKS
    BOOL IsAppDomainAgile();
    BOOL IsCheckAppDomainAgile();

    BOOL IsArrayOfElementsAppDomainAgile();
    BOOL IsArrayOfElementsCheckAppDomainAgile();
#endif

    //<REVIEW>GENERICS: review this</REVIEW>
    EEClass* GetClassOrTypeParam();

    OBJECTREF CreateClassObj();
    
    static TypeHandle MergeArrayTypeHandlesToCommonParent(
        TypeHandle ta, TypeHandle tb);

    static TypeHandle MergeTypeHandlesToCommonParent(
        TypeHandle ta, TypeHandle tb);

private:
    union 
    {
        INT_PTR         m_asInt;        // we look at the low order bits 
        void*           m_asPtr;
        TypeDesc*       m_asTypeDesc;
        MethodTable*    m_asMT;
    };
};


/*************************************************************************/
/* TypeDesc is a discriminated union of all types that can not be directly
   represented by a simple MethodTable*.  These include all parameterized 
   types, as well as others.    The discrimintor of the union at the present
   time is the CorElementType numeration.  The subclass of TypeDesc are
   the possible variants of the union.  
*/ 

class TypeDesc {
    friend struct MEMBER_OFFSET_INFO(TypeDesc);
public:
    TypeDesc(CorElementType type) { 
        u.m_Type = type;
        INDEBUG(u.m_IsParamDesc = 0;)
    }

    // This is the ELEMENT_TYPE* that would be used in the type sig for this type
    // For enums this is the uderlying type
    CorElementType GetNormCorElementType() { 
        return (CorElementType) u.m_Type;
    }

    // Get the exact parent (superclass) of this type  
    TypeHandle GetParent();

    // Returns the name of the array.  Note that it returns
    // the length of the returned string 
    static unsigned ConstructName(CorElementType kind, TypeHandle param, int rank, 
                                  char* buff, unsigned buffLen);
    unsigned GetName(char* buff, unsigned buffLen);

    BOOL CanCastTo(TypeHandle type);

    BOOL TypeDesc::IsByRef() {              // BYREFS are often treated specially 
        return(GetNormCorElementType() == ELEMENT_TYPE_BYREF);
    }



    Module* GetModule();

    Assembly* GetAssembly();

    MethodTable*  GetMethodTable();         // only meaningful for ParamTypeDesc
    TypeHandle GetTypeParam();              // only meaningful for ParamTypeDesc
    TypeHandle* GetClassOrArrayInstantiation();      // only meaningful for ParamTypeDesc; see above
    BaseDomain *GetDomain();                // only meaningful for ParamTypeDesc

protected:
    // Strike needs to be able to determine the offset of certain bitfields.
    // Bitfields can't be used with /offsetof/.
    // Thus, the union/structure combination is used to determine where the
    // bitfield begins, without adding any additional space overhead.
    union
        {
		struct
            {
            unsigned char m_Type_begin;
            } offset;
        struct
            {
            // This is used to discriminate what kind of TypeDesc we are
            CorElementType  m_Type : 8;
            INDEBUG(unsigned m_IsParamDesc : 1;)    // is a ParamTypeDesc
                // unused bits  
            } u;
        };
};

/*************************************************************************/
// This variant is used for parameterized types that have exactly one argument
// type.  This includes arrays, byrefs, pointers.  

class ParamTypeDesc : public TypeDesc {
    friend class TypeDesc;
    friend class JIT_TrialAlloc;
    friend struct MEMBER_OFFSET_INFO(ParamTypeDesc);

public:
    ParamTypeDesc(CorElementType type, MethodTable* pMT, TypeHandle arg) 
        : TypeDesc(type), m_TemplateMT(pMT), m_Arg(arg), m_ReflectClassObject(NULL) {
        INDEBUG(u.m_IsParamDesc = 1;)
        INDEBUG(Verify());
    }

    INDEBUG(BOOL Verify();)

    OBJECTREF CreateClassObj();
    ReflectClass* GetReflectClassIfExists() { return m_ReflectClassObject; }

    friend class StubLinkerCPU;
protected:
        // the m_Type field in TypeDesc tell what kind of parameterized type we have
    MethodTable*    m_TemplateMT;       // The shared method table, some variants do not use this field (it is null)
    TypeHandle      m_Arg;              // The type that is being modifiedj
    ReflectClass    *m_ReflectClassObject;    // pointer back to the internal reflection Type object
};

/*************************************************************************/
/* represents a function type.  */

class FunctionTypeDesc : public TypeDesc {
public:
    FunctionTypeDesc(CorElementType type, ExpandSig* sig) 
        : TypeDesc(type), m_Sig(sig) {
        _ASSERTE(type == ELEMENT_TYPE_FNPTR);   // At the moment only one possibile function type
    }
    ExpandSig* GetSig()     { return(m_Sig); }
    
protected:
    ExpandSig* m_Sig;       // Signature for function type
};


/*************************************************************************/
/* represents a class or method type variable  */

class TypeVarTypeDesc : public TypeDesc {
public:
    TypeVarTypeDesc(EEClass* gClass, unsigned int index)
      : TypeDesc(ELEMENT_TYPE_VAR), m_GenericClass(gClass), m_index(index){}

    TypeVarTypeDesc(MethodDesc* gMethod, unsigned int index)
      : TypeDesc(ELEMENT_TYPE_MVAR), m_index(index), m_GenericMethod(gMethod){}

        // placement new operator
    void* operator new(size_t size, void* spot) {   return (spot); }

    EEClass* GetGenericClass()     
    { 
        _ASSERTE(GetNormCorElementType() == ELEMENT_TYPE_VAR);
        return(m_GenericClass); 
    }
    MethodDesc* GetGenericMethod() 
    {
        _ASSERTE(GetNormCorElementType() == ELEMENT_TYPE_MVAR);
        return(m_GenericMethod); 
    }
    unsigned int GetIndex() {return(m_index);}
    
protected:
    union { 
      EEClass* m_GenericClass;       
      MethodDesc* m_GenericMethod; 
    };

    unsigned int m_index;
};


#endif // TYPEHANDLE_H
