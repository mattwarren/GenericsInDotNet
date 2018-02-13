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
// File: enum.h
//
// Defines various enumerations such as TOKENID, etc.
//
// ===========================================================================

#ifndef _ENUM_H_
#define _ENUM_H_

// GLOBAL DEFINE                                                        
#define CSHARP_GENERICS

/////////////////////////////////////////////////////////////////////////////
// TOKENID

#define TOK(n, id, flags, type, stmtfunc, binop, unop, selfop, color) id,
typedef enum TOKENID
{
    #include "tokens.h"
    TID_NUMTOKENS,

    // aliases for generics    
    TID_OPENANGLE = TID_LESS,
    TID_CLOSEANGLE = TID_GREATER,
} TOKENID;

////////////////////////////////////////////////////////////////////////////////
// OPERATOR

#define OP(n,p,a,stmt,t,pn,e) OP_##n,
typedef enum OPERATOR
{
    #include "ops.h"
    OP_LAST
} OPERATOR;

////////////////////////////////////////////////////////////////////////////////
// PREDEFTYPE

#define PREDEFTYPEDEF(id, name, isRequired, simple, numeric, isclass, isstruct, isiface, isdelegate, ft, et, nicename, zero, qspec, asize, st, attr, keysig) id,
enum PREDEFTYPE {
    #include "predeftype.h"
    PT_COUNT,
    PT_VOID             // (special case)
};
#undef PREDEFTYPEDEF

////////////////////////////////////////////////////////////////////////////////
// NODEKIND enum -- describes all parse tree node kinds

#define NODEKIND(n,s,g) NK_##n,
enum NODEKIND
{
    #include "nodekind.h"
};

////////////////////////////////////////////////////////////////////////////////
// PREDEFNAME enum --  Predefined names.

#define PREDEFNAMEDEF(id, name) id,
enum PREDEFNAME {
    #include "predefname.h"
    PN_COUNT
};
#undef PREDEFNAMEDEF

////////////////////////////////////////////////////////////////////////////////
// PPTOKENID

enum PPTOKENID
{
    #define PPKWD(n,id,d) id,
    #include "ppkwds.h"
};

#define MAX_PPTOKEN_LEN 128

////////////////////////////////////////////////////////////////////////////////
// ATTRLOC

enum ATTRLOC
{
    #define ATTRLOCDEF(id, value, str) id = value,
    #include "attrloc.h"
};

#endif  // _ENUM_H_
