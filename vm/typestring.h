// ---------------------------------------------------------------------------
// typestring.cpp
// 
// Copyright (c) Microsoft 2002
// ---------------------------------------------------------------------------
//
// This module contains all helper functions required to produce
// string representations of types, with options to control the
// appearance of namespace and assembly information.  Its primary use
// is in reflection (Type.Name, Type.FullName, Type.ToString, etc) but
// over time it could replace the use of TypeHandle.GetName etc for
// diagnostic messages.
//
// These functions append characters to their first parameter.
// If there is insufficient space, a new string is allocated,
// overwriting the old one.
//
// ---------------------------------------------------------------------------

#ifndef TYPESTRING_H
#define TYPESTRING_H

#include "common.h"
#include "class.h"
#include "typehandle.h"

class TypeString
{
public:

  // Append the name of the type td to the string
  // If fNamespace=TRUE, prefix by its namespace
  static LPCUTF8 AppendTypeDef(CQuickBytes *pBytes, IMDInternalImport *pImport, mdTypeDef td, BOOL fNamespace = TRUE);

  // Append a square-bracket-enclosed, comma-separated list of n type parameters in inst to the string s
  // If fNamespace=TRUE, include the namespace/enclosing classes for each type parameter
  // If fFullInst=TRUE (regardless of fNamespace), include namespace and full assembly qualification with each parameter 
  // and enclose each parameter in square brackets to disambiguate the commas
  static LPCUTF8 AppendInst(CQuickBytes *pBytes, int n, TypeHandle* inst, BOOL fNamespace = TRUE, BOOL fFullInst = FALSE);

  // Append a representation of the type t to the string s
  // If fNamespace=TRUE, include the namespace/enclosing classes
  // If fFullInst=TRUE (regardless of fNamespace and fAssembly), include namespace and assembly for any type parameters
  // If fAssembly=TRUE, suffix with a comma and the full assembly qualification 
  static LPCUTF8 AppendType(CQuickBytes *pBytes, TypeHandle t, BOOL fNamespace = TRUE, BOOL fFullInst = FALSE, BOOL fAssembly = FALSE);


};

#endif

