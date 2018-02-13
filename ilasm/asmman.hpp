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
// asmman.hpp - header file for manifest-related ILASM functions
//

#ifndef ASMMAN_HPP
#define ASMMAN_HPP

#include "strongname.h"

struct AsmManFile
{
    char*   szName;
    mdToken tkTok;
    DWORD   dwAttr;
    BinStr* pHash;
    CustomDescrList m_CustomDescrList;
    AsmManFile()
    {
        szName = NULL;
        pHash = NULL;
    }
    ~AsmManFile()
    {
        if(szName)  delete szName;
        if(pHash)   delete pHash;
    }
    int ComparedTo(AsmManFile* pX){ return strcmp(szName,pX->szName); }
};
typedef SORTEDARRAY<AsmManFile> AsmManFileList;

struct AsmManAssembly
{
	BOOL	isRef;
	char*	szName;
	char*	szAlias;
    DWORD   dwAlias;
	mdToken	tkTok;
	DWORD	dwAttr;
	BinStr* pPublicKey;
	BinStr* pPublicKeyToken;
	ULONG	ulHashAlgorithm;
	BinStr*	pHashBlob;
	USHORT	usVerMajor;
	USHORT	usVerMinor;
	USHORT	usBuild;
	USHORT	usRevision;
	BinStr*	pLocale;
    // Security attributes
    PermissionDecl* m_pPermissions;
    PermissionSetDecl* m_pPermissionSets;
	CustomDescrList m_CustomDescrList;
    AsmManAssembly()
    {
        szName = szAlias = NULL;
        pPublicKey = pPublicKeyToken =pHashBlob = pLocale = NULL;
    }
	~AsmManAssembly() 
	{
		if(szAlias && (szAlias != szName)) delete [] szAlias;
		if(szName) delete [] szName;
		if(pPublicKey) delete pPublicKey;
		if(pPublicKeyToken) delete pPublicKeyToken;
		if(pHashBlob) delete pHashBlob;
		if(pLocale) delete pLocale;
	}
    int ComparedTo(AsmManAssembly* pX){ return strcmp(szAlias,pX->szAlias); }
};
typedef SORTEDARRAY<AsmManAssembly> AsmManAssemblyList;

struct AsmManComType
{
    char*   szName;
    mdToken tkTok;
    DWORD   dwAttr;
    char*   szFileName;
    char*   szComTypeName;
    mdToken tkClass;
    CustomDescrList m_CustomDescrList;
    AsmManComType()
    {
        szName = szFileName = szComTypeName = NULL;
    }
    ~AsmManComType()
    {
        if(szName) delete szName;
        if(szFileName) delete szFileName;
    }
    int ComparedTo(AsmManComType* pX){ return strcmp(szName,pX->szName); }
};
typedef SORTEDARRAY<AsmManComType> AsmManComTypeList;


struct AsmManRes
{
	char*	szName;
	mdToken	tkTok;
	DWORD	dwAttr;
	char*	szFileName;
	ULONG	ulOffset;
	CustomDescrList m_CustomDescrList;
	char*	szAsmRefName;
	AsmManRes() { szName = szAsmRefName = NULL; ulOffset = 0; };
	~AsmManRes()
	{
		if(szName) delete szName;
		if(szFileName) delete szFileName;
		if(szAsmRefName) delete szAsmRefName;
	}
};
typedef FIFO<AsmManRes> AsmManResList;

struct AsmManModRef
{
    char*   szName;
    mdToken tkTok;
    AsmManModRef() {szName = NULL; tkTok = 0; };
    ~AsmManModRef() { if(szName) delete szName; };
};
typedef FIFO<AsmManModRef> AsmManModRefList;

struct AsmManStrongName
{
    BYTE   *m_pbPublicKey;
    DWORD   m_cbPublicKey;
    WCHAR  *m_wzKeyContainer;
    BOOL    m_fFullSign;
    DWORD   m_dwPublicKeyAllocated;
    WCHAR   m_wzKeyContainerBuffer[32];
    AsmManStrongName() { ZeroMemory(this, sizeof(*this)); }
    ~AsmManStrongName()
    {
        if (m_dwPublicKeyAllocated == 1)
            StrongNameFreeBuffer(m_pbPublicKey);
        else if (m_dwPublicKeyAllocated == 2)
            delete [] m_pbPublicKey;
    }
};

class ErrorReporter;

class AsmMan
{
    AsmManFileList      m_FileLst;
    AsmManComTypeList   m_ComTypeLst;
    AsmManResList       m_ManResLst;
    AsmManModRefList    m_ModRefLst;

    AsmManComType*      m_pCurComType;
    AsmManRes*          m_pCurManRes;
    ErrorReporter*      report;
    void*               m_pAssembler;
    
    AsmManFile*         GetFileByName(char* szFileName);
    AsmManAssembly*     GetAsmRefByName(char* szAsmRefName);
    AsmManComType*      GetComTypeByName(char* szComTypeName);
    mdToken             GetComTypeTokByName(char* szComTypeName);

    IMetaDataAssemblyEmit*  m_pAsmEmitter;
    IMetaDataEmit*          m_pEmitter;

public:
    AsmManAssemblyList  m_AsmRefLst;
    AsmManAssembly*     m_pAssembly;
    AsmManAssembly*     m_pCurAsmRef;
    char*   m_szScopeName;
    BinStr* m_pGUID;
    AsmManStrongName    m_sStrongName;
	// Embedded manifest resources paraphernalia:
	WCHAR*  m_wzMResName[1024];
	DWORD	m_dwMResSize[1024];
	DWORD	m_dwMResNum;
	DWORD	m_dwMResSizeTotal;
	AsmMan() { m_pAssembly = NULL; m_szScopeName = NULL; m_pGUID = NULL; m_pAsmEmitter = NULL; 
				memset(m_wzMResName,0,sizeof(m_wzMResName)); 
				memset(m_dwMResSize,0,sizeof(m_dwMResSize)); 
				m_dwMResNum = m_dwMResSizeTotal = 0; };
	AsmMan(void* pAsm) { m_pAssembly = NULL; m_szScopeName = NULL; m_pGUID = NULL; m_pAssembler = pAsm;  m_pAsmEmitter = NULL;
				memset(m_wzMResName,0,sizeof(m_wzMResName)); 
				memset(m_dwMResSize,0,sizeof(m_dwMResSize)); 
				m_dwMResNum = m_dwMResSizeTotal = 0; };
	AsmMan(ErrorReporter* rpt) { m_pAssembly = NULL; m_szScopeName = NULL; m_pGUID = NULL; report = rpt;  m_pAsmEmitter = NULL;
				memset(m_wzMResName,0,sizeof(m_wzMResName)); 
				memset(m_dwMResSize,0,sizeof(m_dwMResSize)); 
				m_dwMResNum = m_dwMResSizeTotal = 0; };
	~AsmMan() 
	{ 
		if(m_pAssembly) delete m_pAssembly; 
		if(m_szScopeName) delete m_szScopeName; 
		if(m_pGUID) delete m_pGUID; 
	};
	void	SetErrorReporter(ErrorReporter* rpt) { report = rpt; };
	HRESULT EmitManifest(void);

    void    SetEmitter( IMetaDataEmit* pEmitter) { m_pEmitter = pEmitter; };

    void    SetModuleName(char* szName);

    void    AddFile(char* szName, DWORD dwAttr, BinStr* pHashBlob);
	void	EmitDebuggableAttribute(mdToken tkOwner);

	void	StartAssembly(char* szName, char* szAlias, DWORD dwAttr, BOOL isRef);
	void	EndAssembly();
	void	SetAssemblyPublicKey(BinStr* pPublicKey);
	void	SetAssemblyPublicKeyToken(BinStr* pPublicKeyToken);
	void	SetAssemblyHashAlg(ULONG ulAlgID);
	void	SetAssemblyVer(USHORT usMajor, USHORT usMinor, USHORT usBuild, USHORT usRevision);
	void	SetAssemblyLocale(BinStr* pLocale, BOOL bConvertToUnicode);
	void	SetAssemblyHashBlob(BinStr* pHashBlob);

    void    StartComType(char* szName, DWORD dwAttr);
    void    EndComType();
    void    SetComTypeFile(char* szFileName);
    void    SetComTypeComType(char* szComTypeName);
    void    SetComTypeClassTok(mdToken tkClass);

    void    StartManifestRes(char* szName, DWORD dwAttr);
    void    EndManifestRes();
    void    SetManifestResFile(char* szFileName, ULONG ulOffset);
    void    SetManifestResAsmRef(char* szAsmRefName);

    mdToken             GetFileTokByName(char* szFileName);
    mdToken             GetAsmRefTokByName(char* szAsmRefName);
    mdToken             GetAsmTokByName(char* szAsmName) 
        { return (m_pAssembly && (strcmp(m_pAssembly->szName,szAsmName)==0)) ? m_pAssembly->tkTok : 0; };

    mdToken GetModuleRefTokByName(char* szName)
    {
        if(szName && *szName)
        {
            AsmManModRef* pMR;
            for(unsigned i=0; (pMR=m_ModRefLst.PEEK(i)); i++)
            {
                if(!strcmp(szName, pMR->szName)) return pMR->tkTok;
            }
        }
        return 0;
    };

};

#endif
