// ---------------------------------------------------------------------------
// typestring.cpp
// 
// Copyright (c) Microsoft 2002
// ---------------------------------------------------------------------------
//
// This module contains a helper function used to produce string
// representations of types, with options to control the appearance of
// namespace and assembly information.  Its primary use is in
// reflection (Type.Name, Type.FullName, Type.ToString, etc) but over
// time it could replace the use of TypeHandle.GetName etc for
// diagnostic messages.
//
// See the header file for more details
//
// The string processing in here could be op[timised for speed, if desired.
// ---------------------------------------------------------------------------

#include "common.h"
#include "class.h"
#include "typehandle.h"
#include "typestring.h"

// Append the name of the type td to the string
// If fNamespace=TRUE, prefix by its namespace
LPCUTF8 TypeString::AppendTypeDef(CQuickBytes *pBytes, IMDInternalImport *pImport, mdTypeDef td, BOOL fNamespace) 
{ 
  LPCUTF8 szName;
  LPCUTF8 szNameSpace;
  pImport->GetNameOfTypeDef(td, &szName, &szNameSpace);

  _ASSERTE(szName);
  _ASSERTE(pBytes);

  bool newstr = pBytes->Size() == 0;
  size_t len = newstr ? 1 : pBytes->Size(); // Room for trailing NULL
  LPSTR pStr;
  bool include_namespace = fNamespace && szNameSpace != NULL && (*szNameSpace != 0);
  if (include_namespace)
      len += strlen(szNameSpace) + strlen(szName) + 1;
  else
      len += strlen(szName);
  if (!SUCCEEDED(pBytes->ReSize(len)))
      FatalOutOfMemoryError();
  pStr = (LPSTR)pBytes->Ptr();
  if (newstr)
      pStr[0] = '\0';
  if (include_namespace) {
      strcat(pStr, szNameSpace);
      LPSTR pTmp = &pStr[strlen(pStr)];
      *pTmp++ = NAMESPACE_SEPARATOR_CHAR;
      *pTmp = '\0';
  }
  strcat(pStr, szName);
  return pStr;
}

// Append a square-bracket-enclosed, comma-separated list of n type parameters in inst to the string s
// If fNamespace=TRUE, include the namespace/enclosing classes for each type parameter
// If fFullInst=TRUE (regardless of fNamespace), include namespace and full assembly qualification with each parameter 
// and enclose each parameter in square brackets to disambiguate the commas
LPCUTF8 TypeString::AppendInst(CQuickBytes *pBytes, int n, TypeHandle *inst, BOOL fNamespace, BOOL fFullInst) 
{
  bool newstr = pBytes->Size() == 0;
  //Allocate space for punctuation
  size_t len = (newstr ? 1 : pBytes->Size()) // Final NULL
             + 2 // Outer [...]
             + (n ? n-1 : 0) // Comma separators
             + (fFullInst ? 2*n : 0); // Inner [...]
  if (!SUCCEEDED(pBytes->ReSize(len)))
      FatalOutOfMemoryError();
  LPSTR pStr = (LPSTR)pBytes->Ptr();
  if (newstr)
    pStr[0] = '\0';
  strcat(pStr, "[");
  for (int i = 0; i < n; i++) {
    if (i > 0)
        strcat(pStr, ",");
    if (fFullInst) {
      strcat(pStr, "[");
      AppendType(pBytes, inst[i], TRUE, TRUE, TRUE);
      pStr = (LPSTR)pBytes->Ptr();
      strcat(pStr, "]");
    }
    else
      AppendType(pBytes, inst[i], fNamespace, FALSE, FALSE);
  }
  pStr = (LPSTR)pBytes->Ptr();
  strcat(pStr, "]");

  return pStr;
}

// Append a representation of the type t to the string s
// If fNamespace=TRUE, include the namespace/enclosing classes
// If fFullInst=TRUE (regardless of fNamespace and fAssembly), include namespace and assembly for any type parameters
// If fAssembly=TRUE, suffix with a comma and the full assembly qualification 
LPCUTF8 TypeString::AppendType(CQuickBytes *pBytes, TypeHandle ty, BOOL fNamespace, BOOL fFullInst, BOOL fAssembly) 
{
  // Instantiated types have format generic_ty[ty_1,...,ty_n]
  if (ty.HasInstantiation()) {
    AppendType(pBytes, ty.GetGenericTypeDefinition(), fNamespace, FALSE, FALSE);
    AppendInst(pBytes, ty.GetNumGenericArgs(), ty.GetInstantiation(), fNamespace, fFullInst);
  }

  // Array types have format
  //   element_ty[] (1-d) 
  //   element_ty[,] (2-d) etc
  else if (ty.IsArray()) {
    DWORD rank = ty.AsArray()->GetRank(); 
    TypeHandle elemType = ty.GetTypeParam();
    _ASSERTE(!elemType.IsNull());
    AppendType(pBytes, elemType, fNamespace, fFullInst);
    //Allocate space for punctuation
    //This estimate will be 1 too high in most case, but the extra complexity
    //to be more accurate isn't worth it.
    size_t len = pBytes->Size() + 2 + rank;
    if (!SUCCEEDED(pBytes->ReSize(len)))
      FatalOutOfMemoryError();
    LPSTR pStr = (LPSTR)pBytes->Ptr();
    strcat(pStr, "[");
    if (ty.GetSigCorElementType() == ELEMENT_TYPE_ARRAY && rank == 1)
      strcat(pStr, "*");
    for (DWORD i = 1; i < rank; i++)
      strcat(pStr, ",");
    strcat(pStr, "]");
  }

  // ...or byref &
  else if (ty.IsByRef()) {
    TypeHandle elemType = ty.GetTypeParam();
    _ASSERTE(!elemType.IsNull());
    AppendType(pBytes, elemType, fNamespace, fFullInst);
    size_t len = pBytes->Size() + 1;
    if (!SUCCEEDED(pBytes->ReSize(len)))
      FatalOutOfMemoryError();
    LPSTR pStr = (LPSTR)pBytes->Ptr();
    strcat(pStr, "&");
  } 

  // ...or pointer *
  else if (ty.IsTypeDesc()) {
    TypeHandle elemType = ty.GetTypeParam();
    _ASSERTE(!elemType.IsNull());
    AppendType(pBytes, elemType, fNamespace, fFullInst);
    size_t len = pBytes->Size() + 1;
    if (!SUCCEEDED(pBytes->ReSize(len)))
      FatalOutOfMemoryError();
    LPSTR pStr = (LPSTR)pBytes->Ptr();
    strcat(pStr, "*");
  } 

  // otherwise it's just a plain type def
  else {
    // Get the TypeDef token and attributes
    IMDInternalImport *pImport = ty.GetClass()->GetMDImport();
    mdTypeDef td = ty.GetClass()->GetCl();
    DWORD dwAttr;
    pImport->GetTypeDefProps(td, &dwAttr, NULL);      

    LPCUTF8 innerStr = AppendTypeDef(pBytes, pImport, td, fNamespace);

    if (fNamespace) {   
      if (IsTdNested(dwAttr)) {
        while (SUCCEEDED(pImport->GetNestedClassProps(td, &td))) {
          CQuickBytes strEncl;
          AppendTypeDef(&strEncl, pImport, td, fNamespace);

          size_t len = strEncl.Size() + 1 + pBytes->Size();
          if (!SUCCEEDED(strEncl.ReSize(len)))
              FatalOutOfMemoryError();
          LPSTR pEnc = (LPSTR)strEncl.Ptr();
          LPSTR pTmp = &pEnc[strlen(pEnc)];
          *pTmp++ = NESTED_SEPARATOR_CHAR;
          *pTmp = '\0';
          strcat(pEnc, innerStr);

          // Now copy back to the original
          if (!SUCCEEDED(pBytes->ReSize(strEncl.Size())))
              FatalOutOfMemoryError();
          LPSTR pStr = (LPSTR)pBytes->Ptr();
          strcpy (pStr, pEnc);
        }
      }
    }   
  }

  // Now append the assembly
  if (fAssembly) {
      Assembly* pAssembly = ty.GetAssembly();
      _ASSERTE(pAssembly != NULL);
      LPCWSTR pAssemblyName;
      if(SUCCEEDED(pAssembly->GetFullName(&pAssemblyName))) {
           size_t init_len = pBytes->Size() + strlen(ASSEMBLY_SEPARATOR_STR);
           size_t wlen = wcslen(pAssemblyName);
           size_t incr_len = WszWideCharToMultiByte(CP_UTF8, 0, pAssemblyName, (int)wlen, 0, 0, NULL, NULL);
           if (!SUCCEEDED(pBytes->ReSize(init_len+incr_len)))
               FatalOutOfMemoryError();
           LPSTR pStr = (LPSTR)pBytes->Ptr();
           strcat(pStr, ASSEMBLY_SEPARATOR_STR);
           LPSTR pTmp = &pStr[strlen(pStr)];
           WszWideCharToMultiByte(CP_UTF8, 0, pAssemblyName, (int)wlen, pTmp, pBytes->Size() - (int)init_len, NULL, NULL);
           pTmp[incr_len] = '\0';
      }
  }              

  return (LPSTR)pBytes->Ptr();
}

