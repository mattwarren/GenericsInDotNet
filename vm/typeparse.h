// ---------------------------------------------------------------------------
// typeparse.h
// 
// Copyright (c) Microsoft 2002
// ---------------------------------------------------------------------------

#ifndef TYPEPARSE_H
#define TYPEPARSE_H

#include "common.h"
#include "class.h"
#include "typehandle.h"

// Parser for type expressions, as used in Type.GetType, Assembly.GetType and Module.GetType
class TypeParser
{
  private:
    // The full type expression
    LPCUTF8 m_szTypeExpr;

    // Should we throw an exception when the type does not exist?
    BOOL m_bThrowOnError;

    // Should the case of type names be ignored?
    BOOL m_bIgnoreCase;

    // Should access be checked?
    BOOL m_bVerifyAccess;

    // Should only public types be visible?
    BOOL m_bPublicOnly;

    // Stack mark, used if caller's class or assembly is relevant
    StackCrawlMark *m_stackMark;
    void* m_returnIP;

    // Calling class and assembly, filled in on demand
    Assembly *m_pCallersAssembly;
    EEClass *m_pCallersClass;

    // Explicit assembly, for Assembly.GetType
    Assembly *m_pAssembly;

    BYTE* m_pbAssemblyIsLoading;
    
  public:

    // Constructor used when searching in a particular assembly
    TypeParser(LPCUTF8 szTypeExpr, BOOL bThrowOnError, BOOL bIgnoreCase, BOOL bVerifyAccess, BOOL bPublicOnly, Assembly *pAssembly) :
      m_szTypeExpr(szTypeExpr),
      m_bThrowOnError(bThrowOnError),
      m_bIgnoreCase(bIgnoreCase),
      m_bVerifyAccess(bVerifyAccess),
      m_bPublicOnly(bPublicOnly),
      m_stackMark(NULL),
      m_returnIP(NULL),
      m_pCallersAssembly(NULL),
      m_pCallersClass(NULL),
      m_pAssembly(pAssembly),
      m_pbAssemblyIsLoading(NULL)
      { }

    // Constructor used otherwise (Type.GetType)
    TypeParser(LPCUTF8 szTypeExpr, BOOL bThrowOnError, BOOL bIgnoreCase, BOOL bVerifyAccess, BOOL bPublicOnly, BYTE* pbAssemblyIsLoading,
      StackCrawlMark *stackMark) :
      m_szTypeExpr(szTypeExpr),
      m_bThrowOnError(bThrowOnError),
      m_bIgnoreCase(bIgnoreCase),
      m_bVerifyAccess(bVerifyAccess),
      m_bPublicOnly(bPublicOnly),
      m_stackMark(stackMark),
      m_returnIP(NULL),
      m_pCallersAssembly(NULL),
      m_pCallersClass(NULL),
      m_pAssembly(NULL),
      m_pbAssemblyIsLoading(pbAssemblyIsLoading)
      { }

    // Parse the expression starting at *szTypeExpr, leave *szTypeExpr pointing to terminating character
    TypeHandle Parse(LPCUTF8 *szTypeExpr, BOOL bNoAssemblyQualifier);
                   
    // See implementation for details
  private:
    TypeHandle LoadName(LPCUTF8 szNamespace, LPCUTF8 szName, LPCUTF8 szAssembly);

    Assembly* GetCallersAssembly();
    EEClass*  GetCallersClass();

    void Split(LPCUTF8* szTypeExpr,
               BOOL bNoAssemblyQualifier,
               CQuickBytes& sNamespace,    // namespace (possibly empty)
               CQuickBytes& sName,         // name
               CQuickBytes& sAssembly,     // assembly
               LPCUTF8* pszQualifiers,     // array/pointer/instantiation qualifiers
               LPCUTF8* pszQualifiersEnd); // character after qualifiers
};

#endif

