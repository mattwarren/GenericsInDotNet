// ---------------------------------------------------------------------------
// typeparse.cpp
// 
// Copyright (c) Microsoft 2002
// ---------------------------------------------------------------------------
//

#include "common.h"
#include "class.h"
#include "typehandle.h"
#include "typeparse.h"
#include "comclass.h"
#include "assemblynative.hpp"
#include "commember.h"

static BOOL HaveReflectionPermission(BOOL ThrowOnFalse)
{
    THROWSCOMPLUSEXCEPTION();

    BOOL haveReflectionPermission = TRUE;
    COMPLUS_TRY {
        COMMember::g_pInvokeUtil->CheckSecurity();
    }
    COMPLUS_CATCH {
        if (ThrowOnFalse)
            COMPlusRareRethrow();

        haveReflectionPermission = FALSE;
    } COMPLUS_END_CATCH

    return haveReflectionPermission;
}
 

// Get the caller's assembly, determined on demand from stack mark or return address
Assembly *TypeParser::GetCallersAssembly()
{
    if (m_pCallersAssembly == NULL) {
      if (m_stackMark)
        m_pCallersAssembly = SystemDomain::GetCallersAssembly(m_stackMark);

      else {
        MethodDesc *pCallingMD = IP2MethodDesc((const BYTE *)m_returnIP);
        if (pCallingMD)
            m_pCallersAssembly = pCallingMD->GetAssembly();
        else
            // If we failed to determine the caller's method desc, this might
            // indicate a late bound call through reflection. Attempt to
            // determine the real caller through the slower stackwalk method.     
            m_pCallersAssembly = SystemDomain::GetCallersAssembly((StackCrawlMark*)NULL);
      }
    }

    return m_pCallersAssembly;
}

// Get the caller's class, determined on demand from stack mark or return address
EEClass *TypeParser::GetCallersClass()
{
    if (m_pCallersClass == NULL) {
      if (m_stackMark)
        m_pCallersClass = SystemDomain::GetCallersClass(m_stackMark);
      else {
        MethodDesc *pCallingMD = IP2MethodDesc((const BYTE *)m_returnIP);
        if (pCallingMD)
            m_pCallersClass = pCallingMD->GetClass();
        else
            // If we failed to determine the caller's method desc, this might
            // indicate a late bound call through reflection. Attempt to
            // determine the real caller through the slower stackwalk method.
            m_pCallersClass = SystemDomain::GetCallersClass((StackCrawlMark*)NULL);
      }
    }

    return m_pCallersClass;
}

//------------------------------------------------------------------------
// Attempt to load a type given its namespace, name and (optional) assembly
//   szNamespace is the namespace (might be empty)
//   szName is the name
//   szAssembly is the assembly (might be empty)
//------------------------------------------------------------------------
TypeHandle TypeParser::LoadName(LPCUTF8 szNamespace,
                                LPCUTF8 szName,
                                LPCUTF8 szAssembly)
                     
{
    THROWSCOMPLUSEXCEPTION();

    LOG((LF_CLASSLOADER, LL_INFO1000, "TypeParser::LoadName: namespace %s and name %s from assembly %s\n", 
         szNamespace, szName, szAssembly));

    Assembly *pAssembly = NULL;
    TypeHandle typeHnd = TypeHandle();
    NameHandle typeName(szNamespace, szName);
    
    // Set up the name handle
    if(m_bIgnoreCase)
        typeName.SetCaseInsensitive();
    
    OBJECTREF Throwable = NULL;
    GCPROTECT_BEGIN(Throwable);
   
    // An explicit assembly has been specified so look for the type there
    if (*szAssembly) {
        
        AssemblySpec spec;
        HRESULT hr = spec.Init(szAssembly);
        
        if (SUCCEEDED(hr)) {
            
            EEClass *pCallersClass = GetCallersClass();
            Assembly *pCallersAssembly = (pCallersClass) ? pCallersClass->GetAssembly() : NULL;
            if (pCallersAssembly && (!pCallersAssembly->IsShared()))
                spec.GetCodeBase()->SetParentAssembly(pCallersAssembly->GetFusionAssembly());
            
            hr = spec.LoadAssembly(&pAssembly, &Throwable, NULL, (m_pbAssemblyIsLoading != NULL));
            if(SUCCEEDED(hr)) {
                typeHnd = pAssembly->FindNestedTypeHandle(&typeName, &Throwable);
                
                if (typeHnd.IsNull() && (Throwable == NULL)) 
                    // If it wasn't in the available table, maybe it's an internal type
                    typeHnd = pAssembly->GetInternalType(&typeName, m_bThrowOnError, &Throwable);
            }
            else if (m_pbAssemblyIsLoading &&
                     (hr == MSEE_E_ASSEMBLYLOADINPROGRESS))
                *m_pbAssemblyIsLoading = TRUE;
        }
    }

    // There's no explicit assembly so look in the assembly specified by the original caller (Assembly.GetType)
    else if (m_pAssembly) {
      // Returning NULL only means that the type is not in this assembly.
      typeHnd = m_pAssembly->FindNestedTypeHandle(&typeName, &Throwable);

      if (typeHnd.IsNull() && Throwable == NULL) 
         typeHnd = m_pAssembly->GetInternalType(&typeName, m_bThrowOnError, &Throwable);

    } 

    // Otherwise look in the caller's assembly then the system assembly
    else {    

        // Look for type in caller's assembly
        EEClass *pCallersClass = GetCallersClass();
        Assembly *pCallersAssembly = (pCallersClass) ? pCallersClass->GetAssembly() : NULL;

        if(pCallersAssembly) {
            typeHnd = pCallersAssembly->FindNestedTypeHandle(&typeName, &Throwable);
            if (typeHnd.IsNull() && (Throwable == NULL))
                // If it wasn't in the available table, maybe it's an internal type
                typeHnd = pCallersAssembly->GetInternalType(&typeName, m_bThrowOnError, &Throwable);
        }


        // Look for type in system assembly
        if (typeHnd.IsNull() && (Throwable == NULL) && (pCallersAssembly != SystemDomain::SystemAssembly())) {  
            typeHnd = SystemDomain::SystemAssembly()->FindNestedTypeHandle(&typeName, &Throwable);
        }
            
        
        BaseDomain *pDomain = SystemDomain::GetCurrentDomain();
        if (typeHnd.IsNull() &&
            (pDomain != SystemDomain::System())) {
            if ((pAssembly = ((AppDomain*) pDomain)->RaiseTypeResolveEvent(szName, &Throwable)) != NULL) {
                typeHnd = pAssembly->FindNestedTypeHandle(&typeName, &Throwable);
                
                if (typeHnd.IsNull() && (Throwable == NULL)) {
                    // If it wasn't in the available table, maybe it's an internal type
                    typeHnd = pAssembly->GetInternalType(&typeName, m_bThrowOnError, &Throwable);
                }
                else
                    Throwable = NULL;
            }
        }
    }
    

    // Optionally throw an exception if the type does not exist
    if (Throwable != NULL && m_bThrowOnError)
        COMPlusThrow(Throwable);

    GCPROTECT_END();

    // Now verify access to the type
    BOOL fVisible = TRUE;
    if (!typeHnd.IsNull() && m_bVerifyAccess) {
        EEClass *pClass = typeHnd.GetClass();
        if (m_bPublicOnly && !(IsTdPublic(pClass->GetProtection()) || IsTdNestedPublic(pClass->GetProtection())))
                    // the user is asking for a public class but the class we have is not public, discard
                    fVisible = FALSE;
                else {
                    // if the class is a top level public there is no check to perform
                    if (!IsTdPublic(pClass->GetProtection())) {
                        if (GetCallersAssembly() && // full trust for interop
                            !ClassLoader::CanAccess(GetCallersClass(),
                                                    GetCallersAssembly(), 
                                                    pClass,
                                                    pClass->GetAssembly(),
                                                    pClass->GetAttrClass())) {
                            // This is not legal if the user doesn't have reflection permission
                            if (!HaveReflectionPermission(m_bThrowOnError))
                                fVisible = FALSE;
                        }
                    }
                }
            }

    if (!fVisible) 
      typeHnd = TypeHandle();

    if (typeHnd.IsNull() && m_bThrowOnError) {
        Throwable = NULL;
        GCPROTECT_BEGIN(Throwable);

        if (szAssembly && *szAssembly) {
            MAKE_WIDEPTR_FROMUTF8(pwzAssemblyName, szAssembly);
            PostTypeLoadException(NULL, m_szTypeExpr, pwzAssemblyName,
                                  NULL, IDS_CLASSLOAD_GENERIC, &Throwable);
        }
        else if (m_pAssembly)
            m_pAssembly->PostTypeLoadException(m_szTypeExpr,
                                               IDS_CLASSLOAD_GENERIC,
                                               &Throwable);
        else if (GetCallersAssembly() != NULL)
            GetCallersAssembly()->PostTypeLoadException(m_szTypeExpr,
                                                        IDS_CLASSLOAD_GENERIC,
                                                        &Throwable);
        else {
            WCHAR   wszTemplate[30];
            if (FAILED(LoadStringRC(IDS_EE_NAME_UNKNOWN,
                                    wszTemplate,
                                    sizeof(wszTemplate)/sizeof(wszTemplate[0]),
                                    FALSE)))
                wszTemplate[0] = L'\0';
            PostTypeLoadException(NULL, m_szTypeExpr, wszTemplate,
                                  NULL, IDS_CLASSLOAD_GENERIC, &Throwable);
        }

        COMPlusThrow(Throwable);
        GCPROTECT_END();
    }

    return(typeHnd);
}

static void GetString(CQuickBytes& result, LPCUTF8 szName, LPCUTF8 szEnd, BOOL removeEscapes) 
{
    THROWSCOMPLUSEXCEPTION();

    _ASSERTE(szEnd >= szName);
    _ASSERTE(result.Size() == 0);

    result.Alloc(szEnd-szName+1);
    LPSTR pStr = (LPSTR)result.Ptr();
    if (removeEscapes) {
        while (szName != szEnd) {
            if (*szName == BACKSLASH_CHAR) {
                szName++;
                if (szName == szEnd) {
                    *pStr++ = szName[-1];
                    break;
                }
            }
            *pStr++ = *szName;
            szName++;
        }
        *pStr = '\0';
    }
    else {
      strncpy(pStr, szName, szEnd-szName);
      pStr[szEnd-szName] = '\0';
    }

    return;
}

//------------------------------------------------------------------------
// Given a type expression, split it into the following parts:
//   namespace, including nesting (e.g. System.Collections.Generic or N.M.C+N2.M2)
//   name (e.g. ArrayList)
//   array/pointer/inst qualifiers (e.g. [System.Int32] or [System.Int32][,])
//   assembly qualifier (e.g. mscorlib, ...)
// The type expression is terminated by zero or a closing square bracket ]
// or by a comma if no assembly qualifier is expected (pszAssembly is NULL)
//
// Name and namespace are returned with escapes interpreted (e.g. the
// expression N\+1.N2.X\[Y[] has namespace N+1.N2 and name X[Y.
//
// Can throw on out-of-memory.
//------------------------------------------------------------------------
void TypeParser::Split(LPCUTF8 *pszTypeExpr,
                       BOOL bNoAssemblyQualifier,
                       CQuickBytes& sNamespace,  // namespace (possibly empty)
                       CQuickBytes& sName,       // name
                       CQuickBytes& sAssembly,   // assembly
                       LPCUTF8* pszQualifiers,   // array/pointer/instantiation qualifiers
                       LPCUTF8* pszQualifiersEnd)// character after end of qualifiers
{
    THROWSCOMPLUSEXCEPTION();

    // Preconditions: pointers are all non-null
    _ASSERTE(sNamespace.Size() == 0);
    _ASSERTE(sName.Size() == 0);
    _ASSERTE(sAssembly.Size() == 0);

    LPCUTF8 szTypeExpr = *pszTypeExpr;

    // First character after end of nested type name    
    LPCUTF8 szName = szTypeExpr;

    // First non-whitespace character after assembly separator
    LPCUTF8 szAssembly = NULL;

    // First character after end of name, if a qualifier is present
    *pszQualifiers = NULL; 

    // First character after end of type part, not including assembly
    *pszQualifiersEnd = NULL;

    // Number of enclosing square brackets
    int nbrackets = 0;

    // Have we seen an escape (\) character?
    BOOL foundEscape = FALSE;

    // Are we finished (reached , or ] or zero terminator)?
    BOOL finished = FALSE;

    LPCUTF8 p = szTypeExpr;

    // Find the splits 
    while (!finished) {

      switch (*p) {

        // Skip escaped characters, flag that we found one
        case BACKSLASH_CHAR :
          if (p[1] != 0) {
            foundEscape = TRUE;
            p++;
          }
          break;

        // We've found a namespace separator character
        case NAMESPACE_SEPARATOR_CHAR :
          if (*pszQualifiers == NULL && *pszQualifiersEnd == NULL)
            szName = p+1;
          break;

        // Pointer qualifiers
        case '*' : case '&' :
          if (*pszQualifiers == NULL)
            *pszQualifiers = p;
          break;

        // Array or generic instance opener
        case '[' :
          if (*pszQualifiers == NULL)
            *pszQualifiers = p;
          nbrackets++;
          break;

        // Array or generic instance closer
        case ']' :
          nbrackets--;
          if (nbrackets < 0) {
            finished = TRUE;
          }
          break;

        case 0 :          
          finished = TRUE;
          break;

        // Rank separator, type argument separator, or assembly separator
        case ASSEMBLY_SEPARATOR_CHAR : 
          if (nbrackets == 0)
          {
            // Not expecting an assembly so this must be the terminator
            if (bNoAssemblyQualifier) {
              finished = TRUE;
            }

            // This is the assembly
            else if (szAssembly == NULL)
            {
              *pszQualifiersEnd = p;
              while (COMCharacter::nativeIsWhiteSpace(*(++p))); 
              szAssembly = p;
            }
          }
          break;
      }
      p++;
    }
    *pszTypeExpr = p-1;
    if (*pszQualifiersEnd == NULL)
      *pszQualifiersEnd = *pszTypeExpr;
    if (*pszQualifiers == NULL)
      *pszQualifiers = *pszQualifiersEnd;

    // Extract namespace and name
    if (szName != szTypeExpr)
      GetString(sNamespace, szTypeExpr, szName-1, foundEscape);
    GetString(sName, szName, *pszQualifiers, foundEscape);
    if (szAssembly)
      GetString(sAssembly, szAssembly, *pszTypeExpr, FALSE);

#ifdef _DEBUG
    {
      LOG((LF_CLASSLOADER, LL_INFO1000, "SPLIT: Extracted namespace %s and name %s and assembly %s from type expression %s\n", 
          sNamespace.Size() == 0 ? "" : (LPCUTF8)sNamespace.Ptr(),
          sName.Size() == 0 ? "" : (LPCUTF8)sName.Ptr(),
          sAssembly.Size() == 0 ? "" : (LPCUTF8)sAssembly.Ptr(),
          szTypeExpr));
    }
#endif

    // Postcondition: bNoAssemblyQualifer implies sAssembly is empty
    _ASSERTE(!bNoAssemblyQualifier || sAssembly.Size() == 0);
    // Postconditions: various splits are in order
    _ASSERTE(*pszQualifiersEnd >= *pszQualifiers);
    _ASSERTE(*pszTypeExpr >= *pszQualifiersEnd);

    return;
}

//------------------------------------------------------------------------
// Parse and load a type from its string representation
//   *pszTypeExpr contains the string, terminated by zero or ]
//   bNoAssemblyQualifier=FALSE indicates that explicit assembly qualification
//     is not recognised; instead, a comma can terminate the type expression
// On return, leave *pszTypeExpr pointing to the terminating character
//------------------------------------------------------------------------
TypeHandle TypeParser::Parse(LPCUTF8 *pszTypeExpr, BOOL bNoAssemblyQualifier)
{
    THROWSCOMPLUSEXCEPTION();

    // First check for empty strings
    if (!**pszTypeExpr)
      COMPlusThrow(kArgumentException, L"Format_StringZeroLength");

    // Next find splits between different parts of the type expression    
    LPCUTF8 szQualifier, szQualifierEnd;
    CQuickBytes sName, sNamespace, sAssembly;
    Split(pszTypeExpr, bNoAssemblyQualifier, sNamespace, sName, sAssembly, &szQualifier, &szQualifierEnd);

    TypeHandle typeHnd = LoadName(sNamespace.Size() == 0 ? "" : (LPCUTF8)sNamespace.Ptr(),
                                  sName.Size() == 0 ? "" : (LPCUTF8)sName.Ptr(),
                                  sAssembly.Size() == 0 ? "" : (LPCUTF8)sAssembly.Ptr());
    
    // Now parse the qualifiers
    while (szQualifier != szQualifierEnd && !typeHnd.IsNull()) 
    {
        switch (*szQualifier) {
            case '&' : 
            case '*' :
                typeHnd = ClassLoader::GetPointerOrByrefType(*szQualifier == '&' ? ELEMENT_TYPE_BYREF : ELEMENT_TYPE_PTR, typeHnd);
                szQualifier++;
                break;

            // Array qualifier or instantiation
            case '[' :
                szQualifier++;

                // Is it an array qualifier?
                if (*szQualifier == ']' || *szQualifier == ',' || *szQualifier == '*') {
                    DWORD rank = 1;
                    BOOL nobounds = FALSE;
                    while (*szQualifier != ']') {

                        // A star indicates no bounds on this dimension
                        if (*szQualifier == '*') {
                            nobounds = TRUE;
                            szQualifier++;
                        }

                        if (*szQualifier == ',')
                        {
                            rank++;
                            szQualifier++;
                        }
                        else if (*szQualifier != ']')
                            COMPlusThrow(kArgumentException, L"Argument_InvalidArrayName");

                    }          
                    szQualifier++;

                    OBJECTREF Throwable = NULL;
                    GCPROTECT_BEGIN(Throwable);
                    typeHnd = ClassLoader::GetArrayType(rank == 1 && !nobounds ? ELEMENT_TYPE_SZARRAY : ELEMENT_TYPE_ARRAY, 
                                                        typeHnd, rank, &Throwable);
                    if (Throwable != NULL && m_bThrowOnError)
                        COMPlusThrow(Throwable);
                    GCPROTECT_END();
                }

                // Otherwise assume that it is an instantiation of a generic type
                else {          
                  if (!typeHnd.IsGenericTypeDefinition())
                    COMPlusThrow(kArgumentException, L"Argument_InvalidArrayName");

                  DWORD ntypes = typeHnd.GetNumGenericArgs();
                  TypeHandle* inst = (TypeHandle*) _alloca(ntypes * sizeof(TypeHandle));
                  for (DWORD i = 0; i < ntypes; i++) {

                    // We've got an assembly-qualified type
                    if (*szQualifier == '[') {
                      szQualifier++;
                      inst[i] = Parse(&szQualifier, FALSE);
                      if (*szQualifier != ']')
                        COMPlusThrow(kArgumentException, L"Argument_InvalidGenericInstantiation");
                      szQualifier++;
                    }

                    // We've got a non-assembly-qualified type
                    else
                      inst[i] = Parse(&szQualifier, TRUE);
                    
                    if (inst[i].IsNull()) {
                      typeHnd = TypeHandle();
                      goto Skip;
                    }

                    // Commas separate generic type arguments
                    if (i < ntypes-1) {
                      if (*szQualifier != ',')
                        COMPlusThrow(kArgumentException, L"Argument_InvalidGenericInstantiation");
                      szQualifier++;
                    }
                  }

                  if (*szQualifier != ']')
                    COMPlusThrow(kArgumentException, L"Argument_InvalidGenericInstantiation");
                  szQualifier++;

                  OBJECTREF Throwable = NULL;
                  GCPROTECT_BEGIN(Throwable);
                  typeHnd = ClassLoader::LoadGenericInstantiation(typeHnd, inst, ntypes, &Throwable);
                  if (Throwable != NULL && m_bThrowOnError)
                    COMPlusThrow(Throwable);
                  GCPROTECT_END();
                }
            Skip:
                break;

            default :
                COMPlusThrow(kArgumentException, L"Argument_InvalidArrayName");
        }
    }
    
    _ASSERTE(!(m_bThrowOnError && typeHnd.IsNull()));

    return(typeHnd);
}


