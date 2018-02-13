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
// assem.cpp
//
// COM+ IL assembler                                              
//
//
#define INITGUID

#define DECLARE_DATA

#include "assembler.h"
unsigned int g_uCodePage = CP_ACP;

char g_szSourceFileName[MAX_FILENAME_LENGTH*3];

WCHAR   wzUniBuf[8192]; // Unicode conversion global buffer
DWORD   dwUniBuf=8192;  // Size of Unicode global buffer

Assembler::Assembler()
{
    m_pDisp = NULL; 
    m_pEmitter = NULL;  
    m_pImporter = NULL;  

    m_fCPlusPlus = FALSE;
    m_fWindowsCE = FALSE;
    m_fGenerateListing = FALSE;
    char* pszFQN = new char[16];
    strcpy(pszFQN,"<Module>");
    m_pModuleClass = new Class(pszFQN);
    m_lstClass.PUSH(m_pModuleClass);
	m_pModuleClass->m_cl = mdTokenNil;
    m_pModuleClass->m_bIsMaster = FALSE;

    m_fStdMapping   = FALSE;
    m_fDisplayTraceOutput= FALSE;

    m_pCurOutputPos = NULL;

    m_CurPC             = 0;    // PC offset in method
    m_pCurMethod        = NULL;
    m_pCurClass         = NULL;
	m_pCurEvent			= NULL;
	m_pCurProp			= NULL;

    m_pCeeFileGen            = NULL;
    m_pCeeFile               = 0;

	m_pManifest			= NULL;

	m_pCustomDescrList	= NULL;
    
    m_pGlobalDataSection = NULL;
    m_pILSection = NULL;
    m_pTLSSection = NULL;


	m_fDLL = FALSE;
	m_fEntryPointPresent = FALSE;
	m_fHaveFieldsWithRvas = FALSE;

    strcpy(m_szScopeName, "");
	m_crExtends = mdTypeDefNil;

	m_nImplList = 0;
    m_TyParList = NULL;

	m_SEHD = NULL;
	m_firstArgName = NULL;
	m_lastArgName = NULL;
	m_szNamespace = new char[2];
	m_szNamespace[0] = 0;
	m_NSstack.PUSH(m_szNamespace);

	m_szFullNS = new char[MAX_NAMESPACE_LENGTH];
	memset(m_szFullNS,0,MAX_NAMESPACE_LENGTH);
	m_ulFullNSLen = MAX_NAMESPACE_LENGTH;

    m_State             = STATE_OK;
    m_fInitialisedMetaData = FALSE;
    m_fAutoInheritFromObject = TRUE;

	m_ulLastDebugLine = 0xFFFFFFFF;
	m_ulLastDebugColumn = 0xFFFFFFFF;
	m_pSymWriter = NULL;
	m_pSymDocument = NULL;
	m_fIncludeDebugInfo = FALSE;
    m_fIsMscorlib = FALSE;
    m_tkSysObject = 0;
    m_tkSysString = 0;
    m_tkSysValue = 0;
    m_tkSysEnum = 0;

	m_pVTable = NULL;
	m_pMarshal = NULL;
	m_fReportProgress = TRUE;
	m_tkCurrentCVOwner = 1; // module
	m_pOutputBuffer = NULL;

	m_fOwnershipSet = FALSE;
	m_pbsOwner = NULL;

	m_dwSubsystem = IMAGE_SUBSYSTEM_UNKNOWN;
	m_dwComImageFlags = COMIMAGE_FLAGS_ILONLY;
	m_dwFileAlignment = 0;
    m_stBaseAddress = 0;

	g_szSourceFileName[0] = 0;

	m_guidLang = CorSym_LanguageType_ILAssembly;
	m_guidLangVendor = CorSym_LanguageVendor_Microsoft;
    m_guidDoc = CorSym_DocumentType_Text;
    for(int i=0; i<INSTR_POOL_SIZE; i++) m_Instr[i].opcode = -1;
    m_wzResourceFile = NULL;
    m_wzKeySourceName = NULL;
    OnErrGo = false;
    bClock = NULL;
}


Assembler::~Assembler()
{
    Fixup   *pFix;
    Label   *pLab;
    Class   *pClass;
    GlobalLabel *pGlobalLab;
    GlobalFixup *pGlobalFix;

	if(m_pMarshal) delete m_pMarshal;
	if(m_pManifest) delete m_pManifest;

	if(m_pVTable) delete m_pVTable;

    while((pFix = m_lstFixup.POP())) delete(pFix);
    while((pLab = m_lstLabel.POP())) delete(pLab);
    while((pGlobalLab = m_lstGlobalLabel.POP())) delete(pGlobalLab);
    while((pGlobalFix = m_lstGlobalFixup.POP())) delete(pGlobalFix);
    while((pClass = m_lstClass.POP())) delete(pClass);
    while((m_ClassStack.POP()));
	m_pCurClass = NULL;

    if (m_pOutputBuffer)	delete m_pOutputBuffer;
	if (m_crImplList)		delete m_crImplList;
 	if (m_TyParList)		delete m_TyParList;

    if (m_pCeeFileGen != NULL) {
        if (m_pCeeFile)
            m_pCeeFileGen->DestroyCeeFile(&m_pCeeFile);
        DestroyICeeFileGen(&m_pCeeFileGen);
        m_pCeeFileGen = NULL;
    }

    while((m_szNamespace = m_NSstack.POP())) ;
	delete [] m_szFullNS;

    if (m_pSymWriter != NULL)
    {
		m_pSymWriter->Close();
        m_pSymWriter->Release();
        m_pSymWriter = NULL;
    }
    if (m_pImporter != NULL)
    {
        m_pImporter->Release();
        m_pImporter = NULL;
    }
    if (m_pEmitter != NULL)
    {
        m_pEmitter->Release();
        m_pEmitter = NULL;
    }
    
    if (m_pDisp != NULL)    
    {   
        m_pDisp->Release(); 
        m_pDisp = NULL; 
    }   


}


BOOL Assembler::Init()
{

    if (FAILED(CreateICeeFileGen(&m_pCeeFileGen))) return FALSE;
    if (FAILED(m_pCeeFileGen->CreateCeeFile(&m_pCeeFile))) return FALSE;

    if (FAILED(m_pCeeFileGen->GetSectionCreate(m_pCeeFile, ".il", sdReadOnly, &m_pILSection))) return FALSE;
	if (FAILED(m_pCeeFileGen->GetSectionCreate (m_pCeeFile, ".sdata", sdReadWrite, &m_pGlobalDataSection))) return FALSE;
	if (FAILED(m_pCeeFileGen->GetSectionCreate (m_pCeeFile, ".tls", sdReadWrite, &m_pTLSSection))) return FALSE;

    m_pOutputBuffer = new BYTE[OUTPUT_BUFFER_SIZE];
    if (m_pOutputBuffer == NULL)
        return FALSE;
    
    m_pCurOutputPos = m_pOutputBuffer;
    m_pEndOutputPos = m_pOutputBuffer + OUTPUT_BUFFER_SIZE;

	m_crImplList = new mdTypeRef[MAX_INTERFACES_IMPLEMENTED];
	if(m_crImplList == NULL) return FALSE;
	m_nImplListSize = MAX_INTERFACES_IMPLEMENTED;

	m_pManifest = new AsmMan((void*)this);

    return TRUE;
}

void Assembler::SetDLL(BOOL IsDll)
{
#ifdef _DEBUG
    HRESULT OK =
#endif
    m_pCeeFileGen->SetDllSwitch(m_pCeeFile, IsDll);
	m_fDLL = IsDll;
	_ASSERTE(SUCCEEDED(OK));
}

void Assembler::SetOBJ(BOOL IsObj)
{
#ifdef _DEBUG
    HRESULT OK =
#endif
    m_pCeeFileGen->SetObjSwitch(m_pCeeFile, IsObj);
	m_fOBJ = IsObj;
	_ASSERTE(SUCCEEDED(OK));
}


void Assembler::ResetForNextMethod()
{
    Fixup   *pFix;
    Label   *pLab;

	if(m_firstArgName) delArgNameList(m_firstArgName);
	m_firstArgName = NULL;
	m_lastArgName = NULL;

    m_CurPC         = 0;
    m_pCurOutputPos = m_pOutputBuffer;
    m_State         = STATE_OK;
    while((pFix = m_lstFixup.POP())) delete(pFix);
    while((pLab = m_lstLabel.POP())) delete(pLab);
    m_pCurMethod = NULL;
}

BOOL Assembler::AddMethod(Method *pMethod)
{
	BOOL                     fIsInterface=FALSE, fIsImport=FALSE;
	ULONG                    PEFileOffset=0;
	
	_ASSERTE(m_pCeeFileGen != NULL);
	if (pMethod == NULL)
	{ 
		report->error("pMethod == NULL");
		return FALSE;
	}
    if(pMethod->m_pClass != NULL)
    {
        fIsInterface = IsTdInterface(pMethod->m_pClass->m_Attr);
        fIsImport = IsTdImport(pMethod->m_pClass->m_Attr);
    }
	if(m_CurPC)
	{
		char sz[1024];
		sz[0] = 0;
		if(fIsInterface  && (!IsMdStatic(pMethod->m_Attr))) strcat(sz," when non-static declared in interface");
        if(fIsImport) strcat(sz," being imported");
		if(IsMdAbstract(pMethod->m_Attr)) strcat(sz," being abstract");
		if(IsMdPinvokeImpl(pMethod->m_Attr)) strcat(sz," being pinvoke");
		if(!IsMiIL(pMethod->m_wImplAttr)) strcat(sz," being non-IL");
		if(IsMiRuntime(pMethod->m_wImplAttr)) strcat(sz," being runtime-supplied");
		if(IsMiInternalCall(pMethod->m_wImplAttr)) strcat(sz," being an internal call");
		if(strlen(sz))
		{
			report->error("Method can't have body%s\n",sz);
		}
	}
    else // method has no body
    {
        if(fIsImport || IsMdAbstract(pMethod->m_Attr) || IsMdPinvokeImpl(pMethod->m_Attr)
           || IsMiRuntime(pMethod->m_wImplAttr) || IsMiInternalCall(pMethod->m_wImplAttr)) return TRUE;
        if(OnErrGo)
        {
            report->error("Method has no body\n");
            return TRUE;
        }
        else
        {
            report->warn("Method has no body, 'ret' emitted\n");
            Instr* pIns = GetInstr();
            memset(pIns,0,sizeof(Instr));
            pIns->opcode = CEE_RET;
            EmitOpcode(pIns);
        }
    }


	COR_ILMETHOD_FAT fatHeader;
	fatHeader.SetFlags(pMethod->m_Flags);
	fatHeader.SetMaxStack((USHORT) pMethod->m_MaxStack);
	fatHeader.SetLocalVarSigTok(pMethod->m_LocalsSig);
	fatHeader.SetCodeSize(m_CurPC);
	bool moreSections            = (pMethod->m_dwNumExceptions != 0);
	
	// if max stack is specified <8, force fat header, otherwise (with tiny header) it will default to 8
	if((fatHeader.GetMaxStack() < 8)&&(fatHeader.GetLocalVarSigTok()==0)&&(fatHeader.GetCodeSize()<64)&&(!moreSections))
		fatHeader.SetFlags(fatHeader.GetFlags() | CorILMethod_InitLocals); //forces fat header but does nothing else, since LocalVarSigTok==0

	unsigned codeSizeAligned     = fatHeader.GetCodeSize();
	if (moreSections)
		codeSizeAligned          = (codeSizeAligned + 3) & ~3;    // to insure EH section aligned 

	unsigned headerSize = COR_ILMETHOD::Size(&fatHeader, moreSections);
	unsigned ehSize     = COR_ILMETHOD_SECT_EH::Size(pMethod->m_dwNumExceptions, pMethod->m_ExceptionList);
	unsigned totalSize  = headerSize + codeSizeAligned + ehSize;
	unsigned align      = 4;  
	if (headerSize == 1)      // Tiny headers don't need any alignement   
		align = 1;    

	BYTE* outBuff;
    BinStr* pbsBody;
    if((pbsBody = new BinStr())==NULL) return FALSE;
    if((outBuff = pbsBody->getBuff(totalSize))==NULL) return FALSE;
	BYTE* endbuf = &outBuff[totalSize];

    // Emit the header  
	outBuff += COR_ILMETHOD::Emit(headerSize, &fatHeader, moreSections, outBuff);

	pMethod->m_pCode = outBuff;
	pMethod->m_headerOffset= PEFileOffset;
	pMethod->m_methodOffset= PEFileOffset + headerSize;
	pMethod->m_CodeSize = m_CurPC;

    // Emit the code    
	if (codeSizeAligned) 
	{
		memset(outBuff,0,codeSizeAligned);
		memcpy(outBuff, m_pOutputBuffer, fatHeader.GetCodeSize());
		outBuff += codeSizeAligned;  
	}

	// Validate the eh
	COR_ILMETHOD_SECT_EH_CLAUSE_FAT* pEx;
	DWORD	TryEnd,HandlerEnd, dwEx, dwEf;
	for(dwEx = 0, pEx = pMethod->m_ExceptionList; dwEx < pMethod->m_dwNumExceptions; dwEx++, pEx++)
	{
		if(pEx->GetTryOffset() > m_CurPC) // i.e., pMethod->m_CodeSize
		{
			report->error("Invalid SEH clause #%d: Try block starts beyond code size\n",dwEx+1);
		}
		TryEnd = pEx->GetTryOffset()+pEx->GetTryLength();
		if(TryEnd > m_CurPC) 
		{
			report->error("Invalid SEH clause #%d: Tryb lock ends beyond code size\n",dwEx+1);
		}
		if(pEx->GetHandlerOffset() > m_CurPC)
		{
			report->error("Invalid SEH clause #%d: Handler block starts beyond code size\n",dwEx+1);
		}
		HandlerEnd = pEx->GetHandlerOffset()+pEx->GetHandlerLength();
		if(HandlerEnd > m_CurPC) 
		{
			report->error("Invalid SEH clause #%d: Handler block ends beyond code size\n",dwEx+1);
		}
		if(pEx->GetFlags() & COR_ILEXCEPTION_CLAUSE_FILTER)
		{
            if(!((pEx->GetFilterOffset() >= TryEnd)||(pEx->GetTryOffset() >= HandlerEnd)))
			{
				report->error("Invalid SEH clause #%d: Try and Filter/Handler blocks overlap\n",dwEx+1);
			}
			for(dwEf = 0; dwEf < pMethod->m_dwNumEndfilters; dwEf++)
			{
				if(pMethod->m_EndfilterOffsetList[dwEf] == pEx->GetHandlerOffset()) break;
			}
			if(dwEf >= pMethod->m_dwNumEndfilters)
			{
				report->error("Invalid SEH clause #%d: Filter block separated from Handler, or not ending with endfilter\n",dwEx+1);
			}
		}
		else
		if(!((pEx->GetHandlerOffset() >= TryEnd)||(pEx->GetTryOffset() >= HandlerEnd)))
		{
			report->error("Invalid SEH clause #%d: Try and Handler blocks overlap\n",dwEx+1);
		}
        }
    // Emit the eh  
	outBuff += COR_ILMETHOD_SECT_EH::Emit(ehSize, pMethod->m_dwNumExceptions, 
                                pMethod->m_ExceptionList, false, outBuff);  

	_ASSERTE(outBuff == endbuf);

    pMethod->m_pbsBody = pbsBody;

    LocalMemberRefFixup*			 pMRF;
    while((pMRF = pMethod->m_LocalMemberRefFixupList.POP()))
    {
        pMRF->offset += (size_t)(pMethod->m_pCode);
        m_LocalMemberRefFixupList.PUSH(pMRF); // transfer MRF to assembler's list
    }

    if(m_fReportProgress)
	{
		if (pMethod->IsGlobalMethod())
			report->msg("Assembled global method %s\n", pMethod->m_szName);
		else report->msg("Assembled method %s::%s\n", pMethod->m_pClass->m_szFQN,
				  pMethod->m_szName);
	}
	return TRUE;
}

BOOL Assembler::EmitMethodBody(Method* pMethod)
{
    if(pMethod)
    {
        BinStr* pbsBody = pMethod->m_pbsBody;
        unsigned totalSize;
        if(pbsBody && (totalSize = pbsBody->length()))
        {
            unsigned headerSize = pMethod->m_methodOffset-pMethod->m_headerOffset;
            BYTE* outBuff;
            unsigned align = (headerSize == 1)? 1 : 4;
    	    ULONG    PEFileOffset, methodRVA;
            if (FAILED(m_pCeeFileGen->GetSectionBlock (m_pILSection, totalSize, 
                    align, (void **) &outBuff)))    return FALSE;
            memcpy(outBuff,pbsBody->ptr(),totalSize);
            // The offset where we start, (not where the alignment bytes start!   
            if (FAILED(m_pCeeFileGen->GetSectionDataLen (m_pILSection, &PEFileOffset)))
            	return FALSE;
            PEFileOffset -= totalSize;
        
            pMethod->m_pCode = outBuff + headerSize;
            pMethod->m_headerOffset= PEFileOffset;
            pMethod->m_methodOffset= PEFileOffset + headerSize;
            DoDeferredILFixups(pMethod);

            m_pCeeFileGen->GetMethodRVA(m_pCeeFile, PEFileOffset,&methodRVA);
            pMethod->m_headerOffset= methodRVA;
            pMethod->m_methodOffset= methodRVA + headerSize;
            delete pbsBody;
            pMethod->m_pbsBody = NULL;

            m_pEmitter->SetRVA(pMethod->m_Tok,pMethod->m_headerOffset);
        }
        return TRUE;
    }
    else return FALSE;
}
void Assembler::DoDeferredILFixups(Method* pMethod)
{ // Now that we know where in the file the code bytes will wind up,
  // we can update the RVAs and offsets.
	ILFixup *pSearch;
    HRESULT hr;
    GlobalFixup *Fix = NULL;
    while ((pSearch = m_lstILFixup.POP()))
    { 
		switch(pSearch->m_Kind)
		{
			case ilGlobal:
				Fix = pSearch->m_Fixup;
				_ASSERTE(Fix != NULL);
				Fix->m_pReference = pMethod->m_pCode+pSearch->m_OffsetInMethod;
				break;

			case ilToken:
				hr = m_pCeeFileGen->AddSectionReloc(m_pILSection, 
                    				pSearch->m_OffsetInMethod+pMethod->m_methodOffset,
                    				m_pILSection, 
                    				srRelocMapToken);
				_ASSERTE(SUCCEEDED(hr));
				break;

			case ilRVA:
				hr = m_pCeeFileGen->AddSectionReloc(m_pILSection, 
                    				pSearch->m_OffsetInMethod+pMethod->m_methodOffset,
                    				m_pGlobalDataSection, 
                    				srRelocAbsolute);
				_ASSERTE(SUCCEEDED(hr));
				break;

			default:
				;
		}
		delete(pSearch);
	}
}

ImportDescriptor* Assembler::EmitImport(BinStr* DllName)
{
	WCHAR*               wzDllName=&wzUniBuf[0];
	int i = 0;
    unsigned int l = 0;
	ImportDescriptor*	pID;
    char* sz=NULL;

	if(DllName) l = DllName->length();
	if(l)
	{
        sz = (char*)DllName->ptr();
        while((pID=m_ImportList.PEEK(i++)))
		{
			if((pID->dwDllName==l)&& !memcmp(pID->szDllName,sz,l)) return pID;
		}
	}
	else
	{
        while((pID=m_ImportList.PEEK(i++)))
		{
			if(pID->dwDllName==0) return pID;
		}
	}
    if((pID = new ImportDescriptor))
	{
        if(sz) memcpy(pID->szDllName,sz,l);
        pID->szDllName[l] = 0;
        pID->dwDllName = l;
            
        WszMultiByteToWideChar(g_uCodePage,0,pID->szDllName,-1,wzDllName,MAX_MEMBER_NAME_LENGTH);
        if(SUCCEEDED(m_pEmitter->DefineModuleRef(             // S_OK or error.   
                            wzDllName,            // [IN] DLL name    
                            &(pID->mrDll))))      // [OUT] returned   
        {
            m_ImportList.PUSH(pID);
            return pID;
        }
        else report->error("Failed to define module ref '%s'\n",pID->szDllName);
	}
	else report->error("Failed to allocate import descriptor\n");
	return NULL;
}

HRESULT Assembler::EmitPinvokeMap(mdToken tk, PInvokeDescriptor* pDescr)	
{
	WCHAR*               wzAlias=&wzUniBuf[0];
	
	memset(wzAlias,0,sizeof(WCHAR)*MAX_MEMBER_NAME_LENGTH);
	if(pDescr->szAlias)	WszMultiByteToWideChar(g_uCodePage,0,pDescr->szAlias,-1,wzAlias,MAX_MEMBER_NAME_LENGTH);

	return m_pEmitter->DefinePinvokeMap(		// Return code.
						tk,						// [IN] FieldDef, MethodDef or MethodImpl.
						pDescr->dwAttrs,		// [IN] Flags used for mapping.
						(LPCWSTR)wzAlias,		// [IN] Import name.
						pDescr->mrDll);			// [IN] ModuleRef token for the target DLL.
}

void Assembler::EmitScope(Scope* pSCroot)
{
	static ULONG32      	scopeID;
	static ARG_NAME_LIST	*pVarList;
	int						i;
	WCHAR*                  wzVarName=&wzUniBuf[0];
	char*        			szPhonyName=(char*)&wzUniBuf[dwUniBuf >> 1];
	Scope*					pSC = pSCroot;
	if(pSC && m_pSymWriter)
	{
		if(SUCCEEDED(m_pSymWriter->OpenScope(pSC->dwStart,&scopeID)))
		{
			if(pSC->pLocals)
			{
                for(pVarList = pSC->pLocals; pVarList; pVarList = pVarList->pNext)
				{
					if(pVarList->pSig)
					{
						if(strlen(pVarList->szName)) strcpy(szPhonyName,pVarList->szName);
						else sprintf(szPhonyName,"V_%d",pVarList->dwAttr);

						WszMultiByteToWideChar(g_uCodePage,0,szPhonyName,-1,wzVarName,dwUniBuf >> 1);

						m_pSymWriter->DefineLocalVariable(wzVarName,0,pVarList->pSig->length(),
							(BYTE*)pVarList->pSig->ptr(),ADDR_IL_OFFSET,pVarList->dwAttr,0,0,0,0);
					}
					else
					{
						report->error("Local Var '%s' has no signature\n",pVarList->szName);
					}
				}
			}
            for(i = 0; (pSC = pSCroot->SubScope.PEEK(i)); i++) EmitScope(pSC);
			m_pSymWriter->CloseScope(pSCroot->dwEnd);
		}
	}
}

BOOL Assembler::EmitMethod(Method *pMethod)
{ 
// Emit the metadata for a method definition
	BOOL                fSuccess = FALSE;
	WCHAR*              wzMemberName=&wzUniBuf[0];
	BOOL                fIsInterface;
	DWORD               cSig;
	ULONG               methodRVA = 0;
	mdMethodDef         MethodToken;
	mdTypeDef           ClassToken = mdTypeDefNil;
	char                *pszMethodName;
	COR_SIGNATURE       *mySig;

    _ASSERTE((m_pCeeFileGen != NULL) && (pMethod != NULL));
	fIsInterface = ((pMethod->m_pClass != NULL) && IsTdInterface(pMethod->m_pClass->m_Attr));
	pszMethodName = pMethod->m_szName;
	mySig = pMethod->m_pMethodSig;
	cSig = pMethod->m_dwMethodCSig;

	// If  this is an instance method, make certain the signature says so

	if (!(pMethod->m_Attr & mdStatic))
		*mySig |= IMAGE_CEE_CS_CALLCONV_HASTHIS;
    ClassToken = (pMethod->IsGlobalMethod())? mdTokenNil 
									: pMethod->m_pClass->m_cl;
	// Convert name to UNICODE
	memset(wzMemberName,0,sizeof(wzMemberName));
	WszMultiByteToWideChar(g_uCodePage,0,pszMethodName,-1,wzMemberName,MAX_MEMBER_NAME_LENGTH);

	if(IsMdPrivateScope(pMethod->m_Attr))
	{
		WCHAR* p = wcsstr(wzMemberName,L"$PST06");
		if(p) *p = 0;
	}

	if (FAILED(m_pEmitter->DefineMethod(ClassToken,       // parent class
									  wzMemberName,     // member name
									  pMethod->m_Attr & ~mdReservedMask,  // member attributes
									  mySig, // member signature
									  cSig,
									  methodRVA,                // RVA
									  pMethod->m_wImplAttr,                // implflags
									  &MethodToken))) 
	{
		report->error("Failed to define method '%s'\n",pszMethodName);
		goto exit;
	}
    pMethod->m_Tok = MethodToken;
	//--------------------------------------------------------------------------------
	// the only way to set mdRequireSecObject:
	if(pMethod->m_Attr & mdRequireSecObject)
	{
		mdToken tkPseudoClass;
		if(FAILED(m_pEmitter->DefineTypeRefByName(1, COR_REQUIRES_SECOBJ_ATTRIBUTE, &tkPseudoClass)))
			report->error("Unable to define type reference '%s'\n", COR_REQUIRES_SECOBJ_ATTRIBUTE_ANSI);
		else
		{
			mdToken tkPseudoCtor;
			BYTE bSig[3] = {IMAGE_CEE_CS_CALLCONV_HASTHIS,0,ELEMENT_TYPE_VOID};
			if(FAILED(m_pEmitter->DefineMemberRef(tkPseudoClass, L".ctor", (PCCOR_SIGNATURE)bSig, 3, &tkPseudoCtor)))
				report->error("Unable to define member reference '%s::.ctor'\n", COR_REQUIRES_SECOBJ_ATTRIBUTE_ANSI);
			else DefineCV(new CustomDescr(MethodToken,tkPseudoCtor,NULL));
		}
	}

    if (pMethod->m_NumTyPars) 
      m_pEmitter->SetGenericPars(MethodToken, pMethod->m_NumTyPars, pMethod->m_TyParBounds, pMethod->m_TyParNames);

	//--------------------------------------------------------------------------------
    EmitSecurityInfo(MethodToken,
                     pMethod->m_pPermissions,
                     pMethod->m_pPermissionSets);
	//--------------------------------------------------------------------------------
	if(m_fOBJ)
	{
		TokenRelocDescr *pTRD;
		//if(pMethod->m_fEntryPoint)
		{
			char* psz;
			if(pMethod->IsGlobalMethod())
			{
                psz = new char[pMethod->m_dwName+1];
                if(psz)
					strcpy(psz,pMethod->m_szName);
			}
			else
			{
				psz = new char[pMethod->m_dwName+pMethod->m_pClass->m_dwFQN+3];
                if(psz)
					sprintf(psz,"%s::%s",pMethod->m_pClass->m_szFQN,pMethod->m_szName);
			}
			m_pCeeFileGen->AddSectionReloc(m_pILSection,(DWORD)psz,m_pILSection,(CeeSectionRelocType)0x7FFA);
		}
		m_pCeeFileGen->AddSectionReloc(m_pILSection,MethodToken,m_pILSection,(CeeSectionRelocType)0x7FFF);
		m_pCeeFileGen->AddSectionReloc(m_pILSection,pMethod->m_headerOffset,m_pILSection,(CeeSectionRelocType)0x7FFD);
        while((pTRD = pMethod->m_TRDList.POP()))
		{
			m_pCeeFileGen->AddSectionReloc(m_pILSection,pTRD->token,m_pILSection,(CeeSectionRelocType)0x7FFE);
			m_pCeeFileGen->AddSectionReloc(m_pILSection,pTRD->offset+pMethod->m_methodOffset,m_pILSection,(CeeSectionRelocType)0x7FFD);
			delete pTRD;
		}
	}
	if (pMethod->m_fEntryPoint)
	{ 
		if(fIsInterface) report->error("Entrypoint in Interface: Method '%s'\n",pszMethodName); 

		if (FAILED(m_pCeeFileGen->SetEntryPoint(m_pCeeFile, MethodToken)))
		{
			report->error("Failed to set entry point for method '%s'\n",pszMethodName);
			goto exit;
		}

		if (FAILED(m_pCeeFileGen->SetComImageFlags(m_pCeeFile, 0)))
		{
			report->error("Failed to set COM image flags for method '%s'\n",pszMethodName);
			goto exit;
		}

	}
	//--------------------------------------------------------------------------------
	if(IsMdPinvokeImpl(pMethod->m_Attr))
	{
		if(pMethod->m_pPInvoke)
		{
			HRESULT hr;
			if(pMethod->m_pPInvoke->szAlias == NULL) pMethod->m_pPInvoke->szAlias = pszMethodName;
			hr = EmitPinvokeMap(MethodToken,pMethod->m_pPInvoke);
			if(pMethod->m_pPInvoke->szAlias == pszMethodName) pMethod->m_pPInvoke->szAlias = NULL;

			if(FAILED(hr))
			{
				report->error("Failed to set PInvoke map for method '%s'\n",pszMethodName);
				goto exit;
			}
		}
	}

	//--------------------------------------------------------------------------------
	if(m_fIncludeDebugInfo)
	{
        m_pSymWriter->OpenMethod(MethodToken);
        ULONG N = pMethod->m_LinePCList.COUNT();
        if(pMethod->m_fEntryPoint) m_pSymWriter->SetUserEntryPoint(MethodToken);
        if(N)
        {
            LinePC	*pLPC;
            ULONG32  *offsets=new ULONG32[N], *lines = new ULONG32[N], *columns = new ULONG32[N];
            ULONG32  *endlines=new ULONG32[N], *endcolumns=new ULONG32[N];
            if(offsets && lines && columns && endlines && endcolumns)
            {
                DocWriter* pDW;
                unsigned j=0;
                while((pDW = m_DocWriterList.PEEK(j++)))
                {
                    m_pSymDocument = pDW->pWriter;
            		if(m_pSymDocument)
                    {
    					int n = 0;
                        for(int i=0; (pLPC = pMethod->m_LinePCList.PEEK(i)); i++)
    					{
                            if (pLPC->pWriter == m_pSymDocument)
                            {
                                offsets[n] = pLPC->PC;
                                lines[n] = pLPC->Line;
                                columns[n] = pLPC->Column;
                                endlines[n] = pLPC->LineEnd;
                                endcolumns[n] = pLPC->ColumnEnd;
                                n++;
                            }
    					}
                        if(n) m_pSymWriter->DefineSequencePoints(m_pSymDocument,n,
                                                           offsets,lines,columns,endlines,endcolumns);
                    } // end if(pSymDocument)
                } // end while(pDW = next doc.writer)
                while((pLPC = pMethod->m_LinePCList.POP()))
                    delete pLPC;
            }
            else report->error("\nOutOfMemory!\n");
            delete [] offsets;
            delete [] lines;
            delete [] columns;
            delete [] endlines;
            delete [] endcolumns;
        }//enf if(N)
        HRESULT hrr;
        if(pMethod->m_ulLines[1])
            hrr = m_pSymWriter->SetMethodSourceRange(m_pSymDocument,pMethod->m_ulLines[0], pMethod->m_ulColumns[0],
                                               m_pSymDocument,pMethod->m_ulLines[1], pMethod->m_ulColumns[1]);
        EmitScope(&(pMethod->m_MainScope)); // recursively emits all nested scopes
        m_pSymWriter->CloseMethod();
	} // end if(fIncludeDebugInfo)
	{ // add parameters to metadata
		void const *pValue=NULL;
		ULONG		cbValue;
		DWORD dwCPlusTypeFlag=0;
		mdParamDef pdef;
		WCHAR* wzParName=&wzUniBuf[0];
		char*  szPhonyName=(char*)&wzUniBuf[dwUniBuf >> 1];
		if(pMethod->m_dwRetAttr || pMethod->m_pRetMarshal || pMethod->m_RetCustDList.COUNT())
		{
			if(pMethod->m_pRetValue)
			{
				dwCPlusTypeFlag= (DWORD)*(pMethod->m_pRetValue->ptr());
				pValue = (void const *)(pMethod->m_pRetValue->ptr()+1);
				cbValue = pMethod->m_pRetValue->length()-1;
				if(dwCPlusTypeFlag == ELEMENT_TYPE_STRING) cbValue /= sizeof(WCHAR);
			}
			else
			{
				pValue = NULL;
                cbValue = (ULONG) -1;
				dwCPlusTypeFlag=0;
			}
			m_pEmitter->DefineParam(MethodToken,0,NULL,pMethod->m_dwRetAttr,dwCPlusTypeFlag,pValue,cbValue,&pdef);

			if(pMethod->m_pRetMarshal)
			{
				if(FAILED(m_pEmitter->SetFieldMarshal (    
											pdef,						// [IN] given a fieldDef or paramDef token  
							(PCCOR_SIGNATURE)(pMethod->m_pRetMarshal->ptr()),   // [IN] native type specification   
											pMethod->m_pRetMarshal->length())))  // [IN] count of bytes of pvNativeType
					report->error("Failed to set param marshaling for return\n");

			}
			EmitCustomAttributes(pdef, &(pMethod->m_RetCustDList));
		}
        for(ARG_NAME_LIST *pAN=pMethod->m_firstArgName; pAN; pAN = pAN->pNext)
		{
			if(pAN->nNum >= 65535) 
			{
				report->error("Method '%s': Param.sequence number (%d) exceeds 65535, unable to define parameter\n",pszMethodName,pAN->nNum+1);
				continue;
			}
			if(pAN->dwName) strcpy(szPhonyName,pAN->szName);
			else sprintf(szPhonyName,"A_%d",pAN->nNum);

			WszMultiByteToWideChar(g_uCodePage,0,szPhonyName,-1,wzParName,dwUniBuf >> 1);

			if(pAN->pValue)
			{
				dwCPlusTypeFlag= (DWORD)*(pAN->pValue->ptr());
				pValue = (void const *)(pAN->pValue->ptr()+1);
				cbValue = pAN->pValue->length()-1;
				if(dwCPlusTypeFlag == ELEMENT_TYPE_STRING) cbValue /= sizeof(WCHAR);
			}
			else
			{
				pValue = NULL;
                cbValue = (ULONG) -1;
				dwCPlusTypeFlag=0;
			}
			m_pEmitter->DefineParam(MethodToken,pAN->nNum+1,wzParName,pAN->dwAttr,dwCPlusTypeFlag,pValue,cbValue,&pdef);
			if(pAN->pMarshal)
			{
				if(FAILED(m_pEmitter->SetFieldMarshal (    
											pdef,						// [IN] given a fieldDef or paramDef token  
							(PCCOR_SIGNATURE)(pAN->pMarshal->ptr()),   // [IN] native type specification   
											pAN->pMarshal->length())))  // [IN] count of bytes of pvNativeType
					report->error("Failed to set param marshaling for '%s'\n",pAN->szName);
			}
			EmitCustomAttributes(pdef, &(pAN->CustDList));
		}
	}
	fSuccess = TRUE;
	//--------------------------------------------------------------------------------
	// Update method implementations for this method
	{
		MethodImplDescriptor*	pMID;
        while((pMID = pMethod->m_MethodImplDList.POP()))
		{
			pMID->m_tkImplementingMethod = MethodToken;
			// don't delete it here, it's still in the general list
		}
	}
	//--------------------------------------------------------------------------------

	EmitCustomAttributes(MethodToken, &(pMethod->m_CustomDescrList));
exit:
	if (fSuccess == FALSE) m_State = STATE_FAIL;
	return fSuccess;
}

BOOL Assembler::EmitMethodImpls()
{
	MethodImplDescriptor*	pMID;
	BOOL ret = TRUE;
	while((pMID = m_MethodImplDList.POP()))
	{
        pMID->m_tkImplementingMethod = ResolveLocalMemberRef(pMID->m_tkImplementingMethod);
        pMID->m_tkImplementedMethod = ResolveLocalMemberRef(pMID->m_tkImplementedMethod);
        if(FAILED(m_pEmitter->DefineMethodImpl( pMID->m_tkDefiningClass,
                                                pMID->m_tkImplementingMethod,
                                                pMID->m_tkImplementedMethod)))
        {
            report->error("Failed to define Method Implementation");
            ret = FALSE;
        }
        delete pMID;
	}// end while
	return ret;
}

mdToken Assembler::ResolveLocalMemberRef(mdToken tok)
{
    if(TypeFromToken(tok) == 0x99000000)
    {
        tok = RidFromToken(tok);
        if(tok) tok = m_LocalMethodRefDList.PEEK(tok-1)->m_tkResolved;
    }
    else if(TypeFromToken(tok) == 0x98000000)
    {
        tok = RidFromToken(tok);
        if(tok) tok = m_LocalFieldRefDList.PEEK(tok-1)->m_tkResolved;
    }
    return tok;
}

BOOL Assembler::EmitEvent(EventDescriptor* pED)
{
	mdMethodDef mdAddOn=mdMethodDefNil,
				mdRemoveOn=mdMethodDefNil,
				mdFire=mdMethodDefNil,
				*mdOthers;
	int					nOthers;
	WCHAR*              wzMemberName=&wzUniBuf[0];

	if(!pED) return FALSE;

	WszMultiByteToWideChar(g_uCodePage,0,pED->m_szName,-1,wzMemberName,MAX_MEMBER_NAME_LENGTH);

	nOthers = pED->m_tklOthers.COUNT();
	mdOthers = new mdMethodDef[nOthers+1];
	if(mdOthers == NULL)
	{
		report->error("Failed to allocate Others array for event descriptor\n");
		nOthers = 0;
	}
    mdAddOn = ResolveLocalMemberRef(pED->m_tkAddOn);
    mdRemoveOn = ResolveLocalMemberRef(pED->m_tkRemoveOn);
    mdFire = ResolveLocalMemberRef(pED->m_tkFire);
	for(int j=0; j < nOthers; j++)
	{
		mdOthers[j] = ResolveLocalMemberRef((mdToken)(pED->m_tklOthers.PEEK(j)));
	}
	mdOthers[nOthers] = mdMethodDefNil; // like null-terminator
	
    if(FAILED(m_pEmitter->DefineEvent(	pED->m_tdClass,
										wzMemberName,
										pED->m_dwAttr,
										pED->m_tkEventType,
										mdAddOn,
										mdRemoveOn,
										mdFire,
										mdOthers,
										&(pED->m_edEventTok))))
	{
		report->error("Failed to define event '%s'.\n",pED->m_szName);
		delete mdOthers;
		return FALSE;
	}
	EmitCustomAttributes(pED->m_edEventTok, &(pED->m_CustomDescrList));
	return TRUE;
}

BOOL Assembler::EmitProp(PropDescriptor* pPD)
{
	mdMethodDef mdSet, mdGet, *mdOthers;
	int nOthers;
	WCHAR*              wzMemberName=&wzUniBuf[0];

	if(!pPD) return FALSE;

	WszMultiByteToWideChar(g_uCodePage,0,pPD->m_szName,-1,wzMemberName,MAX_MEMBER_NAME_LENGTH);
	
	nOthers = pPD->m_tklOthers.COUNT();
	mdOthers = new mdMethodDef[nOthers+1];
	if(mdOthers == NULL)
	{
		report->error("Failed to allocate Others array for prop descriptor\n");
		nOthers = 0;
	}
    mdSet = ResolveLocalMemberRef(pPD->m_tkSet);
    mdGet = ResolveLocalMemberRef(pPD->m_tkGet);
	for(int j=0; j < nOthers; j++)
	{
		mdOthers[j] = ResolveLocalMemberRef((mdToken)(pPD->m_tklOthers.PEEK(j)));
	}
	mdOthers[nOthers] = mdMethodDefNil; // like null-terminator
	
	if(FAILED(m_pEmitter->DefineProperty(	pPD->m_tdClass,
											wzMemberName,
											pPD->m_dwAttr,
											pPD->m_pSig,
											pPD->m_dwCSig,
											pPD->m_dwCPlusTypeFlag,
											pPD->m_pValue,
											pPD->m_cbValue,
											mdSet,
											mdGet,
											mdOthers,
											&(pPD->m_pdPropTok))))
	{
		report->error("Failed to define property '%s'.\n",pPD->m_szName);
		delete mdOthers;
		return FALSE;
	}
	EmitCustomAttributes(pPD->m_pdPropTok, &(pPD->m_CustomDescrList));
	return TRUE;
}

Class *Assembler::FindCreateClass(char *pszFQN)
{
    Class *pSearch = NULL;

	if(pszFQN)
	{
		char* pch;
        DWORD dwFQN = (DWORD)strlen(pszFQN);

        for (int i = 0; (pSearch = m_lstClass.PEEK(i)); i++)
		{
            if(dwFQN != pSearch->m_dwFQN) continue;
			if (!strcmp(pSearch->m_szFQN, pszFQN)) break;
		}
        if(!pSearch)
        {
            Class *pEncloser = NULL;
            char* pszNewFQN = new char[dwFQN+1];
            strcpy(pszNewFQN,pszFQN);
            pch = strrchr(pszNewFQN, NESTING_SEP);
            if(pch)
            {
                *pch = 0;
                pEncloser = FindCreateClass(pszNewFQN);
                *pch = (char)NESTING_SEP;
            }
            pSearch = new Class(pszNewFQN);
            if (pSearch == NULL)
                report->error("Failed to create class '%s'\n",pszNewFQN);
            else
            {
                pSearch->m_pEncloser = pEncloser;
                m_lstClass.PUSH(pSearch);
                pSearch->m_cl = mdtTypeDef | m_lstClass.COUNT();
            }
        }
	}
    return pSearch;
}


BOOL Assembler::EmitClass(Class *pClass)
{
    LPUTF8              szFullName;
    WCHAR*              wzFullName=&wzUniBuf[0];
    HRESULT             hr = E_FAIL;
    GUID                guid;
    //size_t              L;
    mdToken             tok;

    if(pClass == NULL) return FALSE;

    hr = CoCreateGuid(&guid);
    if (FAILED(hr))
    {
        printf("Unable to create GUID\n");
        m_State = STATE_FAIL;
        return FALSE;
    }

    if(pClass->m_pEncloser)
        szFullName = strrchr(pClass->m_szFQN,NESTING_SEP) + 1;
    else
        szFullName = pClass->m_szFQN;
	    
    WszMultiByteToWideChar(g_uCodePage,0,szFullName,-1,wzFullName,dwUniBuf);

    if (pClass->m_pEncloser)
    {
        hr = m_pEmitter->DefineNestedType( wzFullName,
									    pClass->m_Attr,      // attributes
									    pClass->m_crExtends,  // CR extends class
									    pClass->m_crImplements,// implements
                                        pClass->m_pEncloser->m_cl,  // Enclosing class.
									    &tok);
    }
    else
    {
        hr = m_pEmitter->DefineTypeDef( wzFullName,
									    pClass->m_Attr,      // attributes
									    pClass->m_crExtends,  // CR extends class
									    pClass->m_crImplements,// implements
									    &tok);
    }
    _ASSERTE(tok == pClass->m_cl);
    //delete [] wzFullName;
    if (FAILED(hr)) goto exit;
    if (pClass->m_NumTyPars) 
      m_pEmitter->SetGenericPars(pClass->m_cl, pClass->m_NumTyPars, pClass->m_TyParBounds, pClass->m_TyParNames);
    EmitCustomAttributes(pClass->m_cl, &(pClass->m_CustDList));
    hr = S_OK;

exit:
    return SUCCEEDED(hr);
}

#define PADDING 34

BOOL Assembler::GenerateListingFile(Method *pMethod)
{
    DWORD   PC = 0;
    BYTE *  pCode =  pMethod->m_pCode;
    BOOL    fNeedNewLine = FALSE;

    printf("PC (hex)  Opcodes and data (hex)           Label           Instruction\n");
    printf("-------- -------------------------------- --------------- ----------------\n");

    while (PC < pMethod->m_CodeSize)
    {
        DWORD   Len;
        DWORD   i;
        OPCODE  instr;
        Label * pLabel;
        char    szLabelStr[256];

        // does this PC have a label?
        pLabel = FindLabel(PC);
        
        if (pLabel != NULL)
        {
            sprintf(szLabelStr, "%-15s", pLabel->m_szName);
            fNeedNewLine = TRUE;
        }
        else
        {
            sprintf(szLabelStr, "%-15s", "");
        }

        instr = DecodeOpcode(&pCode[PC], &Len);

        if (instr == CEE_COUNT)
        {
            report->error("Instruction decoding error: invalid opcode\n");
            return FALSE;
        }

        if (fNeedNewLine)
        {
            fNeedNewLine = FALSE;
            printf("\n");
        }

        printf("%08x ", PC);

        for (i = 0; i < Len; i++)
            printf("%02x", pCode[PC+i]);

        PC += Len;
        printf("|");
        Len++;

        Len *= 2;

        char *pszInstrName = OpcodeInformation[instr].pszName;

        switch (OpcodeInformation[instr].Type)
        {
            default:
            {
                printf("Unknown instruction\n");
                return FALSE;
            }

            case InlineNone:
            {
                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }

                printf("%-10s %s\n", szLabelStr, pszInstrName);
                break;
            }

            case ShortInlineVar:
            case ShortInlineI:
            {
                printf("%02x ", pCode[PC]);
                Len += 3;
                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }

                printf("%s %-10s %d\n", szLabelStr, pszInstrName, pCode[PC]);

                PC++;
                break;
            }

            case InlineVar:
            {
                printf("%02x%02x ", pCode[PC], pCode[PC+1]);
                Len += 5;
                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }

                USHORT v = pCode[PC] + (pCode[PC+1] << 8);

                printf("%s %-10s %d\n", szLabelStr, pszInstrName, v);
                PC += 2;
                break;
            }

            case InlineSig:
            case InlineI:
            case InlineString:
            {
                DWORD v = pCode[PC] + (pCode[PC+1] << 8) + (pCode[PC+2] << 16) + (pCode[PC+3] << 24);

                printf("%02x%02x%02x%02x ", pCode[PC], pCode[PC+1], pCode[PC+2], pCode[PC+3]);
                Len += 9;
                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }
                
                printf("%s %-10s %d\n", szLabelStr, pszInstrName, v);
                PC += 4;
                break;
            }

            case InlineI8:
            {
                __int64 v = (__int64) pCode[PC] + 
                            (((__int64) pCode[PC+1]) << 8) +
                            (((__int64) pCode[PC+2]) << 16) +
                            (((__int64) pCode[PC+3]) << 24) +
                            (((__int64) pCode[PC+4]) << 32) +
                            (((__int64) pCode[PC+5]) << 40) +
                            (((__int64) pCode[PC+6]) << 48) +
                            (((__int64) pCode[PC+7]) << 56);
                            
                printf("%02x%02x%02x%02x%02x%02x%02x%02x ", 
                    pCode[PC], pCode[PC+1], pCode[PC+2], pCode[PC+3],
                    pCode[PC+4], pCode[PC+5], pCode[PC+6], pCode[PC+7]);
                Len += (8*2+1);
                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }
                
                printf("%s %-10s 0x%I64x\n", szLabelStr, pszInstrName, v);
                PC += 8;
                break;
            }

            case ShortInlineR:
            {
                __int32 v = (__int32) pCode[PC] + 
                            (((__int32) pCode[PC+1]) << 8) +
                            (((__int32) pCode[PC+2]) << 16) +
                            (((__int32) pCode[PC+3]) << 24);

                float f = (float&)v;

                printf("%02x%02x%02x%02x ", pCode[PC], pCode[PC+1], pCode[PC+2], pCode[PC+3]);
                Len += 9;
                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }
                
                printf("%s %-10s %f\n", szLabelStr, pszInstrName, f);
                PC += 4;
                break;
            }

            case InlineR:
            {
                __int64 v = (__int64) pCode[PC] + 
                            (((__int64) pCode[PC+1]) << 8) +
                            (((__int64) pCode[PC+2]) << 16) +
                            (((__int64) pCode[PC+3]) << 24) +
                            (((__int64) pCode[PC+4]) << 32) +
                            (((__int64) pCode[PC+5]) << 40) +
                            (((__int64) pCode[PC+6]) << 48) +
                            (((__int64) pCode[PC+7]) << 56);

                double d = (double&)v;

                printf("%02x%02x%02x%02x%02x%02x%02x%02x ", 
                    pCode[PC], pCode[PC+1], pCode[PC+2], pCode[PC+3],
                    pCode[PC+4], pCode[PC+5], pCode[PC+6], pCode[PC+7]);
                Len += (8*2+1);
                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }
                
                printf("%s %-10s %f\n", szLabelStr, pszInstrName, d);
                PC += 8;
                break;
            }

            case ShortInlineBrTarget:
            {
                char offset = (char) pCode[PC];
                long dest = (PC + 1) + (long) offset;
                Label *pLab = FindLabel(dest);

                printf("%02x ", pCode[PC]);
                Len += 3;
                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }

                if (pLab != NULL)
                    printf("%s %-10s %s\n", szLabelStr, pszInstrName, pLab->m_szName);
                else
                    printf("%s %-10s (abs) %d\n", szLabelStr, pszInstrName, dest);

                PC++;

                fNeedNewLine = TRUE;
                break;
            }

            case InlineBrTarget:
            {
                long offset = pCode[PC] + (pCode[PC+1] << 8) + (pCode[PC+2] << 16) + (pCode[PC+3] << 24);
                long dest = (PC + 4) + (long) offset;
                Label *pLab = FindLabel(dest);

                printf("%02x%02x%02x%02x ", pCode[PC], pCode[PC+1], pCode[PC+2], pCode[PC+3]);
                Len += 9;
                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }

                if (pLab != NULL)
                    printf("%s %-10s %s\n", szLabelStr, pszInstrName, pLab->m_szName);
                else
                    printf("%s %-10s (abs) %d\n", szLabelStr, pszInstrName, dest);

                PC += 4;

                fNeedNewLine = TRUE;
                break;
            }

            case InlineSwitch:
            {
                DWORD cases = pCode[PC] + (pCode[PC+1] << 8) + (pCode[PC+2] << 16) + (pCode[PC+3] << 24);

                printf("%02x%02x%02x%02x ", pCode[PC], pCode[PC+1], pCode[PC+2], pCode[PC+3]);
                Len += 9;
                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }

                PC += 4;
                DWORD PC_nextInstr = PC + 4 * cases;

                printf("%s %-10s (%d cases)\n", szLabelStr, pszInstrName, cases);

                for (i = 0; i < cases; i++)
                {
                    long offset = pCode[PC] + (pCode[PC+1] << 8) + (pCode[PC+2] << 16) + (pCode[PC+3] << 24);
                    long dest = PC_nextInstr + (long) offset;
                    Label *pLab = FindLabel(dest);

                    printf("%04d %02x%02x%02x%02x %-*s",
                        PC, pCode[PC], pCode[PC+1], pCode[PC+2], pCode[PC+3], PADDING+6, "");

                    PC += 4;

                    if (pLab != NULL)
                        printf("%s (abs %d)\n", pLab->m_szName, dest);
                    else
                        printf("(abs) %d\n", dest);
                }

                break;
            }

            case InlinePhi:
            {
                DWORD words = pCode[PC];
                PC += (words * 2);  
                printf("PHI NODE\n");   
                break;
            }

            case InlineMethod:
            case InlineField:
            case InlineType:
            {
                printf("%02x%02x%02x%02x ", pCode[PC], pCode[PC+1], pCode[PC+2], pCode[PC+3]);

                Len += 9;

                while (Len < PADDING)
                {
                    printf(" ");
                    Len++;
                }

                PC += 4;

                printf("%s %-10s\n", szLabelStr, pszInstrName);
                break;
            }

        }
    }

    printf("----------------------------------------------------------------------\n\n");
    return TRUE;
}

#undef PADDING

BOOL Assembler::DoGlobalFixups()
{
    GlobalFixup *pSearch;

    for (int i=0; (pSearch = m_lstGlobalFixup.PEEK(i)); i++)
    {
        GlobalLabel *   pLabel = FindGlobalLabel(pSearch->m_szLabel);
        if (pLabel == NULL)
        {
            report->error("Unable to find forward reference global label '%s'\n",
                pSearch->m_szLabel);

            m_State = STATE_FAIL;
            return FALSE;
        }
        BYTE * pReference = pSearch->m_pReference;
        DWORD  GlobalOffset = pLabel->m_GlobalOffset;
		memcpy(pReference,&GlobalOffset,4);
    }

    return TRUE;
}

state_t Assembler::AddGlobalLabel(char *pszName, HCEESECTION section)
{
    if (FindGlobalLabel(pszName) != NULL)
    {
        report->error("Duplicate global label '%s'\n", pszName);
        m_State = STATE_FAIL;
		return m_State;
    }

    ULONG GlobalOffset;
#ifdef _DEBUG
    HRESULT hr =
#endif
    m_pCeeFileGen->GetSectionDataLen(section, &GlobalOffset);
	_ASSERTE(SUCCEEDED(hr));

	GlobalLabel *pNew = new GlobalLabel(pszName, GlobalOffset, section);
	if (pNew == 0)
	{
		report->error("Failed to allocate global label '%s'\n",pszName);
		m_State = STATE_FAIL;
		return m_State;
	}

	m_lstGlobalLabel.PUSH(pNew);
    return m_State;
}

void Assembler::AddLabel(DWORD CurPC, char *pszName)
{
    if (FindLabel(pszName) != NULL)
    {
        report->error("Duplicate label: '%s'\n", pszName);

        m_State = STATE_FAIL;
    }
    else
    {
        Label *pNew = new Label(pszName, CurPC);

        if (pNew != NULL)
			m_lstLabel.PUSH(pNew);
        else
        {
            report->error("Failed to allocate label '%s'\n",pszName);
            m_State = STATE_FAIL;
        }
    }
}

OPCODE Assembler::DecodeOpcode(const BYTE *pCode, DWORD *pdwLen)
{
    OPCODE opcode;

    *pdwLen = 1;
    opcode = OPCODE(pCode[0]);
    switch(opcode) {
        case CEE_PREFIX1:
            opcode = OPCODE(pCode[1] + 256);
            if (opcode < 0 || opcode >= CEE_COUNT)
                return CEE_COUNT;
            *pdwLen = 2;
            break;

        case CEE_PREFIXREF:
        case CEE_PREFIX2:
        case CEE_PREFIX3:
        case CEE_PREFIX4:
        case CEE_PREFIX5:
        case CEE_PREFIX6:
        case CEE_PREFIX7:
            return CEE_COUNT;
        default:
            break;
    }
    return opcode;
}

Label *Assembler::FindLabel(char *pszName)
{
    Label lSearch(pszName,0), *pL;
    pL =  m_lstLabel.FIND(&lSearch);
    lSearch.m_szName = NULL;
    return pL;
}


Label *Assembler::FindLabel(DWORD PC)
{
    Label *pSearch;

    for (int i = 0; (pSearch = m_lstLabel.PEEK(i)); i++)
    {
        if (pSearch->m_PC == PC)
            return pSearch;
    }

    return NULL;
}

char* Assembler::ReflectionNotation(mdToken tk)
{
    char *sz = (char*)&wzUniBuf[dwUniBuf>>1], *pc;
    *sz=0;
    switch(TypeFromToken(tk))
    {
        case mdtTypeDef:
            {
                Class *pClass = m_lstClass.PEEK(RidFromToken(tk)-1);
                if(pClass)
                {
                    strcpy(sz,pClass->m_szFQN);
                    pc = sz;
                    while((pc = strchr(pc,NESTING_SEP)))
                    {
                        *pc = '+';
                        pc++;
                    }
                }
            }
            break;

        case mdtTypeRef:
            {
                ULONG   N;
                mdToken tkResScope;
                if(SUCCEEDED(m_pImporter->GetTypeRefProps(tk,&tkResScope,wzUniBuf,dwUniBuf>>1,&N)))
                {
                    WszWideCharToMultiByte(CP_UTF8,0,wzUniBuf,-1,sz,dwUniBuf>>1,NULL,NULL);
                    if(TypeFromToken(tkResScope)==mdtAssemblyRef)
                    {
                        AsmManAssembly *pAsmRef = m_pManifest->m_AsmRefLst.PEEK(RidFromToken(tkResScope)-1);
                        if(pAsmRef)
                        {
                            pc = &sz[strlen(sz)];
                            pc+=sprintf(pc,", %s, Version=%d.%d.%d.%d, Culture=",pAsmRef->szName,
                                    pAsmRef->usVerMajor,pAsmRef->usVerMinor,pAsmRef->usBuild,pAsmRef->usRevision);
                            ULONG L=0;
                            if(pAsmRef->pLocale && (L=pAsmRef->pLocale->length()))
                            {
                                memcpy(wzUniBuf,pAsmRef->pLocale->ptr(),L);
                                wzUniBuf[L>>1] = 0;
                                WszWideCharToMultiByte(CP_UTF8,0,wzUniBuf,-1,pc,dwUniBuf>>1,NULL,NULL);
                            }
                            else pc+=sprintf(pc,"neutral");
                            pc = &sz[strlen(sz)];
                            if(pAsmRef->pPublicKeyToken && (L=pAsmRef->pPublicKeyToken->length()))
                            {
                                pc+=sprintf(pc,", Publickeytoken=");
                                BYTE* pb = (BYTE*)(pAsmRef->pPublicKeyToken->ptr());
                                for(N=0; N<L; N++,pb++) pc+=sprintf(pc,"%2.2x",*pb);
                            }
                        }
                    }
                }
            }
            break;

        default:
            break;
    }
    return sz;
}

