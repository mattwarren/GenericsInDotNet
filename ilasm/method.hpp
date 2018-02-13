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
#ifndef _METHOD_HPP
#define _METHOD_HPP

class Assembler;
class PermissionDecl;
class PermissionSetDecl;

#define MAX_EXCEPTIONS 16	// init.number; increased by 16 when needed

/**************************************************************************/
struct LinePC
{
	ULONG	Line;
	ULONG	Column;
	ULONG	LineEnd;
	ULONG	ColumnEnd;
	ULONG	PC;
    ISymUnmanagedDocumentWriter* pWriter;
};
typedef FIFO<LinePC> LinePCList;


struct PInvokeDescriptor
{
	mdModuleRef	mrDll;
	char*	szAlias;
	DWORD	dwAttrs;
};

struct TokenRelocDescr // for OBJ generation only!
{
	DWORD	offset;
	mdToken token;
	TokenRelocDescr(DWORD off, mdToken tk) { offset = off; token = tk; };
};
typedef FIFO<TokenRelocDescr> TRDList;
/* structure - element of [local] signature name list */

struct CustomDescr
{
	mdToken	tkType;
    mdToken tkOwner;
	BinStr* pBlob;
	CustomDescr(mdToken tko, mdToken tk, BinStr* pblob) { tkType = tk; pBlob = pblob; tkOwner = tko;};
	CustomDescr(mdToken tk, BinStr* pblob) { tkType = tk; pBlob = pblob; tkOwner = 0;};
	~CustomDescr() { if(pBlob) delete pBlob; };
};
typedef FIFO<CustomDescr> CustomDescrList;
struct	ARG_NAME_LIST
{
	char szName[1024];
    DWORD dwName;
	BinStr*   pSig; // argument's signature  ptr
	BinStr*	  pMarshal;
	BinStr*	  pValue;
	int	 nNum;
	DWORD	  dwAttr;
	CustomDescrList	CustDList;
	ARG_NAME_LIST *pNext;
	__forceinline ARG_NAME_LIST(int i, char *sz, BinStr *pbSig, BinStr *pbMarsh, DWORD attr) 
	{
        nNum = i;
        dwName = (DWORD)strlen(sz); 
        strcpy(szName,sz); 
        pNext = NULL; 
        pSig=pbSig; 
        pMarshal = pbMarsh; 
        dwAttr = attr; 
        pValue=NULL; 
    };
	inline ~ARG_NAME_LIST() 
    { 
        if(pSig) delete pSig; 
        if(pMarshal) delete pMarshal; 
        if(pValue) delete pValue; 
    }
};

struct Scope;
typedef FIFO<Scope> ScopeList;
struct Scope
{
	DWORD	dwStart;
	DWORD	dwEnd;
	ARG_NAME_LIST*	pLocals;
	ScopeList		SubScope;
	Scope*			pSuperScope;
	Scope() { dwStart = dwEnd = 0; pLocals = NULL; pSuperScope = NULL; };
};
struct VarDescr
{
	DWORD	dwSlot;
	BinStr*	pbsSig;
	BOOL	bInScope;
    VarDescr() { dwSlot = (DWORD) -1; pbsSig = NULL; bInScope = FALSE; };
};
typedef FIFO<VarDescr> VarDescrList;

class Method
{
public:
    Class  *m_pClass;
    mdToken *m_TyParBounds;
    LPCWSTR *m_TyParNames;
    DWORD   m_NumTyPars;
    DWORD   m_SigInfoCount;
    DWORD   m_MaxStack;
    mdSignature  m_LocalsSig;
    DWORD   m_Flags;
    char*   m_szName;
    DWORD   m_dwName;
    char*   m_szExportAlias;
	DWORD	m_dwExportOrdinal;
    COR_ILMETHOD_SECT_EH_CLAUSE_FAT *m_ExceptionList;
    DWORD   m_dwNumExceptions;
	DWORD	m_dwMaxNumExceptions;
	DWORD*	m_EndfilterOffsetList;
    DWORD   m_dwNumEndfilters;
	DWORD	m_dwMaxNumEndfilters;
    DWORD   m_Attr;
    BOOL    m_fEntryPoint;
    BOOL    m_fGlobalMethod;
    DWORD   m_methodOffset;
    DWORD   m_headerOffset;
    BYTE *  m_pCode;
    DWORD   m_CodeSize;
	WORD	m_wImplAttr;
	ULONG	m_ulLines[2];
	ULONG	m_ulColumns[2];
	// PInvoke attributes
	PInvokeDescriptor* m_pPInvoke;
    // Security attributes
    PermissionDecl* m_pPermissions;
    PermissionSetDecl* m_pPermissionSets;
	// VTable attributes
	WORD			m_wVTEntry;
	WORD			m_wVTSlot;
	// Return marshaling
	BinStr*	m_pRetMarshal;
	BinStr* m_pRetValue;
	DWORD	m_dwRetAttr;
	CustomDescrList m_RetCustDList;
	// Member ref fixups
	LocalMemberRefFixupList  m_LocalMemberRefFixupList;
    // Method body (header+code+EH)
    BinStr* m_pbsBody;
    mdToken m_Tok;
    Method(Assembler *pAssembler, Class *pClass, char *pszName, BinStr* pbsSig, DWORD Attr);
    ~Method() 
	{ 
		delete [] m_szName;
		if(m_szExportAlias) delete [] m_szExportAlias;
        delArgNameList(m_firstArgName);
		delArgNameList(m_firstVarName);
		delete m_pbsMethodSig;
		delete [] m_ExceptionList;
		delete [] m_EndfilterOffsetList;
		if(m_pRetMarshal) delete m_pRetMarshal;
		if(m_pRetValue) delete m_pRetValue;
		while(m_MethodImplDList.POP()); // ptrs in m_MethodImplDList are dups of those in Assembler
        if(m_pbsBody) delete m_pbsBody;
	};

    BOOL IsGlobalMethod()
    {
        return m_fGlobalMethod;
    };

    void SetIsGlobalMethod()
    {
        m_fGlobalMethod = TRUE;
    };
	
	void delArgNameList(ARG_NAME_LIST *pFirst)
	{
		ARG_NAME_LIST *pArgList=pFirst, *pArgListNext;
		for(; pArgList;	pArgListNext=pArgList->pNext,
						delete pArgList, 
						pArgList=pArgListNext);
	};
	
	ARG_NAME_LIST *catArgNameList(ARG_NAME_LIST *pBase, ARG_NAME_LIST *pAdd)
	{
		if(pAdd) //even if nothing to concatenate, result == head
		{
			ARG_NAME_LIST *pAN = pBase;
			if(pBase)
			{
				int i;
				for(; pAN->pNext; pAN = pAN->pNext) ;
				pAN->pNext = pAdd;
				i = pAN->nNum;
				for(pAN = pAdd; pAN; pAN->nNum = ++i, pAN = pAN->pNext);
			}
			else pBase = pAdd; //nothing to concatenate to, result == tail
		}
		return pBase;
	};

	int	findArgNum(ARG_NAME_LIST *pFirst, char *szArgName, DWORD dwArgName)
	{
		int ret=-1;
        if(dwArgName)
        {
    		ARG_NAME_LIST *pAN;
    		for(pAN=pFirst; pAN; pAN = pAN->pNext)
    		{
    			if((pAN->dwName == dwArgName)&& !strcmp(pAN->szName,szArgName))
    			{
    				ret = pAN->nNum;
    				break;
    			}
    		}
        }
		return ret;
	};

	BinStr	*m_pbsMethodSig;
	COR_SIGNATURE*	m_pMethodSig;
	DWORD	m_dwMethodCSig;
	ARG_NAME_LIST *m_firstArgName;
	ARG_NAME_LIST *m_firstVarName;
	// to call error() from Method:
    const char* m_FileName;
    unsigned m_LineNum;
	// debug info
	LinePCList m_LinePCList;
	// custom values
	CustomDescrList m_CustomDescrList;
	// token relocs (used for OBJ generation only)
	TRDList m_TRDList;
	// method's own list of method impls
	MethodImplDList	m_MethodImplDList;
	// lexical scope handling 
	Assembler*		m_pAssembler;
	Scope			m_MainScope;
	Scope*			m_pCurrScope;
	VarDescrList	m_Locals;
	void OpenScope();
	void CloseScope();
};

#endif /* _METHOD_HPP */

