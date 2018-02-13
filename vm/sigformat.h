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
////////////////////////////////////////////////////////////////////////////////
// This Module contains routines that expose properties of Member (Classes, Constructors
//  Interfaces and Fields)
//
// Date: March/April 1998
////////////////////////////////////////////////////////////////////////////////


#ifndef _SIGFORMAT_H
#define _SIGFORMAT_H

#include "comclass.h"
#include "invokeutil.h"
#include "reflectutil.h"
#include "comstring.h"
#include "comvariant.h"
#include "comvarargs.h"
#include "field.h"

#define SIG_INC 256

class SigFormat
{
public:
	SigFormat();

        //@GENERICS: the exact method/array instantiation is required because pMeth can be shared between instantiations
	SigFormat(MethodDesc* pMeth, TypeHandle* classInst, BOOL fIgnoreMethodName = false);
	SigFormat(MetaSig &metaSig, LPCUTF8 memberName, LPCUTF8 className = NULL, LPCUTF8 ns = NULL);
    
	void FormatSig(MetaSig &metaSig, LPCUTF8 memberName, LPCUTF8 className = NULL, LPCUTF8 ns = NULL);
	
	~SigFormat();
	
	STRINGREF GetString();
	const char * GetCString();
	const char * GetCStringParmsOnly();
	
	int AddType(TypeHandle th);

protected:
	char*		_fmtSig;
	int			_size;
	int			_pos;
    TypeHandle  _arrayType; // null type handle if the sig is not for an array. This is currently only set 
                            // through the ctor taking a MethodInfo as its first argument. It will have to be 
                            // exposed some other way to be used in a more generic fashion

	// Exact instantiation used for instantiated types and arrays
        TypeHandle* _classInst; 

	int AddSpace();
	int AddString(LPCUTF8 s);
        void AddTypeString(Module* pModule, SigPointer sig, TypeHandle* classInst, TypeHandle *methodInst);
	
};

class FieldSigFormat : public SigFormat
{
public:
        //@GENERICS: the exact class instantiation is required because pFld can be shared between instantiations
	FieldSigFormat(FieldDesc* pFld, TypeHandle *classInst);
};

class PropertySigFormat : public SigFormat
{
public:
	PropertySigFormat(MetaSig &metaSig, LPCUTF8 memberName);
	void FormatSig(MetaSig &sig, LPCUTF8 memberName);
};

#endif // _SIGFORMAT_H

