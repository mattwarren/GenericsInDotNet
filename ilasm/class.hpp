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
// class.hpp
//

#ifndef _CLASS_HPP
#define _CLASS_HPP

class PermissionDecl;
class PermissionSetDecl;

class Class
{
public:
	Class * m_pEncloser;
	char  *	m_szFQN;
    DWORD   m_dwFQN;
    mdTypeDef m_cl;
    mdTypeRef m_crExtends;
    mdTypeRef *m_crImplements;
    mdToken *m_TyParBounds;
    LPCWSTR *m_TyParNames;
    DWORD   m_NumTyPars;
    DWORD   m_Attr;
    DWORD   m_MemberAttr;
    DWORD   m_dwNumInterfaces;
	DWORD	m_dwNumFieldsWithOffset;
    PermissionDecl* m_pPermissions;
    PermissionSetDecl* m_pPermissionSets;
	ULONG	m_ulSize;
	ULONG	m_ulPack;
	BOOL	m_bIsMaster;

    MethodList			m_MethodList;
    FieldDList          m_FieldDList;	
    EventDList          m_EventDList;
    PropDList           m_PropDList;
	CustomDescrList     m_CustDList;

    Class(char* pszFQN)
    {
		m_pEncloser = NULL;
        m_cl = mdTypeDefNil;
        m_crExtends = mdTypeRefNil;
        m_TyParBounds = NULL;
        m_NumTyPars = 0;
        m_TyParNames = NULL;
        m_dwNumInterfaces = 0;
		m_dwNumFieldsWithOffset = 0;
		m_crImplements = NULL;
		m_szFQN = pszFQN;
        m_dwFQN = pszFQN ? (DWORD)strlen(pszFQN) : 0;

        m_Attr = tdPublic;
        m_MemberAttr = 0;

		m_bIsMaster  = TRUE;

        m_pPermissions = NULL;
        m_pPermissionSets = NULL;

		m_ulPack = 0;
		m_ulSize = 0xFFFFFFFF;
    }
	
	~Class()
	{
		delete [] m_szFQN;
	}
};


#endif /* _CLASS_HPP */

