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

#include "assembler.h"
#include "binstr.h"         
#include "nvpair.h"

#define FAIL_UNLESS(x, y) if (!(x)) { report->error y; return; }

/**************************************************************************/
void Assembler::StartNameSpace(char* name)
{
	m_NSstack.PUSH(m_szNamespace);
	m_szNamespace = name;
	unsigned L = (unsigned)strlen(m_szFullNS);
	unsigned l = (unsigned)strlen(name);
	if(L+l+1 >= m_ulFullNSLen)
	{
		char* pch = new char[((L+l)/MAX_NAMESPACE_LENGTH + 1)*MAX_NAMESPACE_LENGTH];
		if(pch)
		{
			memcpy(pch,m_szFullNS,L+1);
			delete [] m_szFullNS;
			m_szFullNS = pch;
			m_ulFullNSLen = ((L+l)/MAX_NAMESPACE_LENGTH + 1)*MAX_NAMESPACE_LENGTH;
		}
		else report->error("Failed to reallocate the NameSpace buffer\n");
	}
	if(L) strcat(m_szFullNS,NAMESPACE_SEPARATOR_STR);
	strcat(m_szFullNS,m_szNamespace);
}

/**************************************************************************/
void Assembler::EndNameSpace()
{
	char *p = &m_szFullNS[strlen(m_szFullNS)-strlen(m_szNamespace)];
	if(p > m_szFullNS) p--;
	*p = 0;
	delete [] m_szNamespace;
	if((m_szNamespace = m_NSstack.POP())==NULL) 
	{
		m_szNamespace = new char[2];
		m_szNamespace[0] = 0;
	}
}

/**************************************************************************/
void	Assembler::ClearImplList(void)
{
	while(m_nImplList) m_crImplList[--m_nImplList] = mdTypeRefNil;
}
/**************************************************************************/
void	Assembler::AddToImplList(mdToken tk)
{
	if(m_nImplList+1 >= m_nImplListSize)
	{
		mdToken	*ptr = new mdToken[m_nImplListSize + MAX_INTERFACES_IMPLEMENTED];
		if(ptr == NULL) 
		{
			report->error("Failed to reallocate Impl List from %d to %d bytes\n",
				m_nImplListSize*sizeof(mdToken),
                (m_nImplListSize+MAX_INTERFACES_IMPLEMENTED)*sizeof(mdToken));
			return;
		}
		memcpy(ptr,m_crImplList,m_nImplList*sizeof(mdToken));
		delete m_crImplList;
		m_crImplList = ptr;
		m_nImplListSize += MAX_INTERFACES_IMPLEMENTED;
	}
	m_crImplList[m_nImplList++] = tk;
	m_crImplList[m_nImplList] = mdTypeRefNil;
}

void	Assembler::ClearBoundList(void)
{
	m_TyParList = NULL;
}
/**************************************************************************/

mdToken Assembler::ResolveClassRef(mdToken tkResScope, char *pszFullClassName, Class** ppClass)
{
	Class *pClass = NULL;
	mdToken tkRet = mdTokenNil;
    mdToken *ptkSpecial = NULL;

	if(pszFullClassName == NULL) return mdTokenNil;
	if (m_fInitialisedMetaData == FALSE)
	{
		if (FAILED(InitMetaData())) // impl. see WRITER.CPP
		{
			_ASSERTE(0);
			if(ppClass) *ppClass = NULL;
			return mdTokenNil;
		}
	}
        if((m_tkSysObject==0)&&(strcmp(pszFullClassName,"System.Object")==0)) ptkSpecial = &m_tkSysObject;
        else if((m_tkSysString==0)&&(strcmp(pszFullClassName,"System.String")==0)) ptkSpecial = &m_tkSysString;
        else if((m_tkSysValue==0)&&(strcmp(pszFullClassName,"System.ValueType")==0)) ptkSpecial = &m_tkSysValue;
        else if((m_tkSysEnum==0)&&(strcmp(pszFullClassName,"System.Enum")==0)) ptkSpecial = &m_tkSysEnum;
    
        if(ptkSpecial && (!m_fIsMscorlib)) tkResScope = GetAsmRef("mscorlib");
        if(tkResScope == 1)
        {
             if((pClass = FindCreateClass(pszFullClassName))) tkRet = pClass->m_cl;
        }
	else
	{
	    tkRet = MakeTypeRef(tkResScope, pszFullClassName);
        pClass = NULL;
    }
    if(ppClass) *ppClass = pClass;
    if(ptkSpecial) *ptkSpecial = tkRet;
	return tkRet;
}

/**************************************************************************/
mdToken Assembler::ResolveTypeSpec(BinStr* typeSpec)
{
    mdToken tk;
    if(FAILED(m_pEmitter->GetTokenFromTypeSpec(typeSpec->ptr(), typeSpec->length(), &tk))) tk = mdTokenNil;
    return tk;
}

/**************************************************************************/
mdToken Assembler::GetAsmRef(char* szName)
{
    mdToken tkResScope = 0;
    if(strcmp(szName,"*")==0) tkResScope = mdTokenNil;
    else
    {
        tkResScope = m_pManifest->GetAsmRefTokByName(szName);
        if(RidFromToken(tkResScope)==0)
        {
            // if it's mscorlib or self, emit the AssemblyRef
            if((strcmp(szName,"mscorlib")==0)||RidFromToken(m_pManifest->GetAsmTokByName(szName)))
            {
                char *sz = new char[strlen(szName)+1];
                if(sz)
                {
                    strcpy(sz,szName);
                    AsmManAssembly *pAsmRef = m_pManifest->m_pCurAsmRef;
                    m_pManifest->StartAssembly(sz,NULL,0,TRUE);
                    m_pManifest->EndAssembly();
                    tkResScope = m_pManifest->GetAsmRefTokByName(szName);
                    m_pManifest->m_pCurAsmRef = pAsmRef;
                }
                else
                    report->error("\nOut of memory!\n");
            }
            else
                report->error("Undefined assembly ref '%s'\n",szName);
        }
    }
    return tkResScope;
}
/**************************************************************************/
mdToken Assembler::GetModRef(char* szName)
{
    mdToken tkResScope = 0;
    if(!strcmp(szName,m_szScopeName)) 
            tkResScope = 1; // scope is "this module"
    else
    {
        ImportDescriptor*	pID;	
        int i = 0;
        tkResScope = mdModuleRefNil;
        DWORD L = (DWORD)strlen(szName);
        while((pID=m_ImportList.PEEK(i++)))
        {
            if((pID->dwDllName==L)&& !strcmp(pID->szDllName,szName))
            {
                tkResScope = pID->mrDll;
                break;
            }
        }
        if(RidFromToken(tkResScope)==0)
            report->error("Undefined module ref '%s'\n",szName);
    }
    return tkResScope;
}
/**************************************************************************/
mdToken Assembler::MakeTypeRef(mdToken tkResScope, char* pszFullClassName)
{
    mdToken tkRet = mdTokenNil;
    if(pszFullClassName && *pszFullClassName)
    {
        char* pc = strrchr(pszFullClassName,NESTING_SEP);
        if(pc != NULL) // scope: enclosing class
        {
            DWORD L = (DWORD)(pc-pszFullClassName);
            char* szScopeName = new char[L+1];
            if(szScopeName != NULL)
            {
                memcpy(szScopeName,pszFullClassName,L);
                szScopeName[L] = 0;
                tkResScope = MakeTypeRef(tkResScope,szScopeName);
                delete [] szScopeName;
            }
            else
                report->error("\nOut of memory!\n");
        }
        else
            pc = pszFullClassName;
        if(*pc)
        {
            // convert name to widechar
            WszMultiByteToWideChar(g_uCodePage,0,pc,-1,wzUniBuf,dwUniBuf);
            if(FAILED(m_pEmitter->DefineTypeRefByName(tkResScope, wzUniBuf, &tkRet))) tkRet = mdTokenNil;
        }
    }
	return tkRet;
}
/**************************************************************************/

void Assembler::StartClass(char* name, DWORD attr, TyParList *typars)
{
	mdTypeRef	crExtends = mdTypeRefNil;
	Class *pEnclosingClass = m_pCurClass;
	char *szFQN;
	BOOL bIsEnum = FALSE;
	BOOL bIsValueType = FALSE;

    m_TyParList = typars;

	if (m_pCurMethod != NULL)
	{ 
        report->error("Class cannot be declared within a method scope\n");
	}
	if(pEnclosingClass)
	{
		szFQN = new char[pEnclosingClass->m_dwFQN+(int)strlen(name)+2];
		if (szFQN != NULL)
            sprintf(szFQN,"%s%c%s",pEnclosingClass->m_szFQN,NESTING_SEP,name);
		else
			report->error("\nOut of memory!\n");
	}
    else
	{
        szFQN = new char[(int)strlen(m_szFullNS)+(int)strlen(name)+2];
		if (szFQN != NULL) {
			if(strlen(m_szFullNS)) sprintf(szFQN,"%s.%s",m_szFullNS,name);
			else strcpy(szFQN,name);
			unsigned L = (unsigned)strlen(szFQN);
			if(L >= MAX_CLASSNAME_LENGTH)
				report->error("Full class name too long (%d characters, %d allowed).\n",L,MAX_CLASSNAME_LENGTH-1);
		}
		else
			report->error("\nOut of memory!\n");
	}
	if(szFQN == NULL) return;

    mdToken tkThis;
    if(m_fIsMscorlib)
        tkThis = ResolveClassRef(1,szFQN,&m_pCurClass); // boils down to FindCreateClass(szFQN)
    else
    {
        m_pCurClass = FindCreateClass(szFQN);
        tkThis = m_pCurClass->m_cl;
    }
	if(m_pCurClass->m_bIsMaster)
	{
    	if(!IsNilToken(m_crExtends))
    	{
    		// has a superclass
    		if(IsTdInterface(attr)) report->error("Base class in interface\n");
            bIsValueType = (m_crExtends == m_tkSysValue)&&(tkThis != m_tkSysEnum);
            bIsEnum = (m_crExtends == m_tkSysEnum);
            crExtends = m_crExtends;
    	}
    	else
    	{
    		bIsEnum = ((attr & 0x40000000) != 0);
    		bIsValueType = ((attr & 0x80000000) != 0);
    	}
    	attr &= 0x3FFFFFFF;
		if (m_fAutoInheritFromObject && (crExtends == mdTypeRefNil) && (!IsTdInterface(attr)))
		{
            mdToken tkMscorlib = m_fIsMscorlib ? 1 : GetAsmRef("mscorlib");
			crExtends = bIsEnum ? 
				ResolveClassRef(tkMscorlib,"System.Enum",NULL)
				:( bIsValueType ? 
					ResolveClassRef(tkMscorlib,"System.ValueType",NULL) 
					: ResolveClassRef(tkMscorlib, "System.Object",NULL));
		}
        {
			DWORD wasAttr = attr;
			if(pEnclosingClass && (!IsTdNested(attr)))
			{
				if(OnErrGo)
					report->error("Nested class has non-nested visibility (0x%08X)\n",attr);
				else
				{
					attr &= ~tdVisibilityMask;
					attr |= (IsTdPublic(wasAttr) ? tdNestedPublic : tdNestedPrivate);
					report->warn("Nested class has non-nested visibility (0x%08X), changed to nested (0x%08X)\n",wasAttr,attr);
				}
			}
			else if((pEnclosingClass==NULL) && IsTdNested(attr))
			{
				if(OnErrGo)
					report->error("Non-nested class has nested visibility (0x%08X)\n",attr);
				else
				{
					attr &= ~tdVisibilityMask;
					attr |= (IsTdNestedPublic(wasAttr) ? tdPublic : tdNotPublic);
					report->warn("Non-nested class has nested visibility (0x%08X), changed to non-nested (0x%08X)\n",wasAttr,attr);
				}
			}
		}
		m_pCurClass->m_Attr = attr;
		m_pCurClass->m_crExtends = (tkThis == m_tkSysObject)? mdTypeRefNil : crExtends;

        if ((m_pCurClass->m_dwNumInterfaces = m_nImplList))
		{
			if(bIsEnum)	report->error("Enum implementing interface(s)\n");
			m_pCurClass->m_crImplements = new mdTypeRef[m_nImplList+1];
			if(m_pCurClass->m_crImplements != NULL)
                memcpy(m_pCurClass->m_crImplements, m_crImplList, (m_nImplList+1)*sizeof(mdTypeRef));
			else
			{
				report->error("Failed to allocate Impl List for class '%s'\n", name);
				m_pCurClass->m_dwNumInterfaces = 0;
			}
		}
		else m_pCurClass->m_crImplements = NULL;
		if (m_TyParList)
		{
		    m_pCurClass->m_NumTyPars = m_TyParList->ToArray(&m_pCurClass->m_TyParBounds, &m_pCurClass->m_TyParNames);
		}
		else m_pCurClass->m_NumTyPars = 0;
		if(bIsValueType)
		{
			if(!IsTdSealed(attr))
			{
				if(OnErrGo)	report->error("Non-sealed value class\n");
				else
				{
					report->warn("Non-sealed value class, made sealed\n");
					m_pCurClass->m_Attr |= tdSealed;
				}
			}
		}
		m_pCurClass->m_pEncloser = pEnclosingClass;
		m_pCurClass->m_bIsMaster = FALSE;
	} // end if(old class) else
	m_tkCurrentCVOwner = 0;
    m_pCustomDescrList = &(m_pCurClass->m_CustDList);

	m_ClassStack.PUSH(pEnclosingClass);
	ClearImplList();
    ClearBoundList();
    m_crExtends = mdTypeRefNil;
}

/**************************************************************************/
void Assembler::EndClass()
{
	m_pCurClass = m_ClassStack.POP();
}

/**************************************************************************/
void Assembler::SetPinvoke(BinStr* DllName, int Ordinal, BinStr* Alias, int Attrs)
{
	if(m_pPInvoke) delete m_pPInvoke;
	if(DllName->length())
	{
        if((m_pPInvoke = new PInvokeDescriptor))
		{
			unsigned l;
			ImportDescriptor* pID;
            if((pID = EmitImport(DllName)))
			{
				m_pPInvoke->mrDll = pID->mrDll;
				m_pPInvoke->szAlias = NULL;
				if(Alias)
				{
					l = Alias->length();
                    if((m_pPInvoke->szAlias = new char[l+1]))
					{
						memcpy(m_pPInvoke->szAlias,Alias->ptr(),l);
						m_pPInvoke->szAlias[l] = 0;
					}
					else report->error("\nOut of memory!\n");
				}
				m_pPInvoke->dwAttrs = (DWORD)Attrs;
			}
			else
			{
				delete m_pPInvoke;
				m_pPInvoke = NULL;
				report->error("PInvoke refers to undefined imported DLL\n");
			}
		}
		else
			report->error("Failed to allocate PInvokeDescriptor\n");
	}
	else
	{
		m_pPInvoke = NULL; // No DLL name, it's "local" (IJW) PInvoke
		report->error("Local (embedded native) PInvoke method, the resulting PE file is unusable\n");
	}
	if(DllName) delete DllName;
	if(Alias) delete Alias;
}

/**************************************************************************/
void Assembler::StartMethod(char* name, BinStr* sig, CorMethodAttr flags, BinStr* retMarshal, DWORD retAttr, TyParList *typars)
{
    if (m_pCurMethod != NULL)
    {
        report->error("Cannot declare a method '%s' within another method\n",name);
    }
    if (!m_fInitialisedMetaData)
    {
        if (FAILED(InitMetaData())) // impl. see WRITER.CPP
        {
            _ASSERTE(0);
        }
    }
	if(strlen(name) >= MAX_CLASSNAME_LENGTH)
			report->error("Method '%s' -- name too long (%d characters).\n",name,strlen(name));
	if (!(flags & mdStatic))
		*(sig->ptr()) |= IMAGE_CEE_CS_CALLCONV_HASTHIS;
	else if(*(sig->ptr()) & (IMAGE_CEE_CS_CALLCONV_HASTHIS | IMAGE_CEE_CS_CALLCONV_EXPLICITTHIS))
	{
		if(OnErrGo)	report->error("Method '%s' -- both static and instance\n", name);
		else
		{
			report->warn("Method '%s' -- both static and instance, set to static\n", name);
			*(sig->ptr()) &= ~(IMAGE_CEE_CS_CALLCONV_HASTHIS | IMAGE_CEE_CS_CALLCONV_EXPLICITTHIS);
		}
	}

	if(!IsMdPrivateScope(flags))
	{
		Method* pMethod;
		Class* pClass = (m_pCurClass ? m_pCurClass : m_pModuleClass);
                DWORD L = (DWORD)strlen(name);
		for(int j=0; (pMethod = pClass->m_MethodList.PEEK(j)); j++)
		{
			if(	(pMethod->m_dwName == L) &&
                (!strcmp(pMethod->m_szName,name)) &&
				(pMethod->m_dwMethodCSig == sig->length())  &&
				(!memcmp(pMethod->m_pMethodSig,sig->ptr(),sig->length()))
				&&(!IsMdPrivateScope(pMethod->m_Attr)))
			{
				report->error("Duplicate method declaration\n");
				break;
			}
		}
	}
	if(m_pCurClass)
	{ // instance method
		if(IsMdAbstract(flags) && !IsTdAbstract(m_pCurClass->m_Attr))
		{
			report->error("Abstract method '%s' in non-abstract class '%s'\n",name,m_pCurClass->m_szFQN);
		}
		if(m_pCurClass->m_crExtends == m_tkSysEnum) report->error("Method in enum\n");
		if(!strcmp(name,COR_CTOR_METHOD_NAME))
		{
			flags = (CorMethodAttr)(flags | mdSpecialName);
			if(IsTdInterface(m_pCurClass->m_Attr)) report->error("Instance constructor in interface\n");

		}
		if(IsTdInterface(m_pCurClass->m_Attr))
		{
			if(!IsMdPublic(flags)) report->error("Non-public method in interface\n");
			if((!IsMdStatic(flags))&&(!(IsMdVirtual(flags) && IsMdAbstract(flags))))
			{
				if(OnErrGo)	report->error("Non-virtual, non-abstract instance method in interface\n");
				else
				{
					report->warn("Non-virtual, non-abstract instance method in interface, set to such\n");
					flags = (CorMethodAttr)(flags |mdVirtual | mdAbstract);
				}
			}

		}
		m_pCurMethod = new Method(this, m_pCurClass, name, sig, flags);
	}
	else
	{
		if(IsMdAbstract(flags))
		{
			if(OnErrGo)	report->error("Global method '%s' can't be abstract\n",name);
			else
			{
				report->warn("Global method '%s' can't be abstract, attribute removed\n",name);
				flags = (CorMethodAttr)(((int) flags) &~mdAbstract);
			}
		}
		if(!IsMdStatic(flags))
		{
			if(OnErrGo)	report->error("Non-static global method '%s'\n",name);
			else
			{
				report->warn("Non-static global method '%s', made static\n",name);
				flags = (CorMethodAttr)(flags | mdStatic);
			}
		}
		m_pCurMethod = new Method(this, m_pCurClass, name, sig, flags);
        if(m_pCurMethod->m_firstArgName)
        {
    		for(ARG_NAME_LIST *pAN=m_pCurMethod->m_firstArgName; pAN; pAN = pAN->pNext)
            {
    			if(pAN->dwName)
                {
                    int k = m_pCurMethod->findArgNum(pAN->pNext,pAN->szName,pAN->dwName);
                    if(k >= 0)
                        report->error("Duplicate param name '%s' in method '%s'\n",pAN->szName,name);
                }
            }
        }
	    if (m_pCurMethod)
		{
			m_pCurMethod->SetIsGlobalMethod();
			if (m_fInitialisedMetaData == FALSE) InitMetaData();
        }
	}
	if(m_pCurMethod)
	{
		m_pCurMethod->m_pRetMarshal = retMarshal;
		m_pCurMethod->m_dwRetAttr = retAttr;
		m_tkCurrentCVOwner = 0;
		m_pCustomDescrList = &(m_pCurMethod->m_CustomDescrList);
		m_pCurMethod->m_MainScope.dwStart = m_CurPC;
        if (typars)
        {
            m_pCurMethod->m_NumTyPars = typars->ToArray(&m_pCurMethod->m_TyParBounds,
            &m_pCurMethod->m_TyParNames);
        }
        else m_pCurMethod->m_NumTyPars = 0;
	}
	else report->error("Failed to allocate Method class\n");
}

/**************************************************************************/
void Assembler::EndMethod()
{
	unsigned uLocals;

	if(m_pCurMethod->m_pCurrScope != &(m_pCurMethod->m_MainScope))
	{
		report->error("Invalid lexical scope structure in method %s\n",m_pCurMethod->m_szName);
	}
	m_pCurMethod->m_pCurrScope->dwEnd = m_CurPC;
	// ----------emit locals signature-------------------
    if((uLocals = m_pCurMethod->m_Locals.COUNT()))
	{
		VarDescr* pVD;
		BinStr*	  pbsSig = new BinStr();
		unsigned cnt;
		HRESULT hr;
		DWORD   cSig;
        const COR_SIGNATURE* mySig;

		pbsSig->appendInt8(IMAGE_CEE_CS_CALLCONV_LOCAL_SIG);
		cnt = CorSigCompressData(uLocals,pbsSig->getBuff(5));
		pbsSig->remove(5-cnt);
        for(cnt = 0; (pVD = m_pCurMethod->m_Locals.PEEK(cnt)); cnt++)
		{
			if(pVD->pbsSig) pbsSig->append(pVD->pbsSig);
			else report->error("Undefined type od local var slot %d in method %s\n",cnt,m_pCurMethod->m_szName);
		}

		cSig = pbsSig->length();
        mySig = (const COR_SIGNATURE *)(pbsSig->ptr());
	
		if (cSig > 1)    // non-empty signature
		{
			hr = m_pEmitter->GetTokenFromSig(mySig, cSig, &m_pCurMethod->m_LocalsSig);
			_ASSERTE(SUCCEEDED(hr));
		}
		delete pbsSig;
	}
	//-----------------------------------------------------
    if (DoFixups()) AddMethod(m_pCurMethod); //AddMethod - see ASSEM.CPP
	else
	{
		report->error("Method '%s' compilation failed.\n",m_pCurMethod->m_szName);
	}
    ResetForNextMethod(); // see ASSEM.CPP
}
/**************************************************************************/
BOOL Assembler::DoFixups()
{
    Fixup *pSearch;

    for (int i=0; (pSearch = m_lstFixup.PEEK(i)); i++)
    {
        Label * pLabel = FindLabel(pSearch->m_szLabel);
        long    offset;

        if (pLabel == NULL)
        {
            report->error("Unable to find forward reference label '%s' called from PC=%d\n",
                pSearch->m_szLabel, pSearch->m_RelativeToPC);

            m_State = STATE_FAIL;
            return FALSE;
        }

        offset = pLabel->m_PC - pSearch->m_RelativeToPC;

        if (pSearch->m_FixupSize == 1)
        {
            if (offset > 127 || offset < -128)
            {
                report->error("Offset of forward reference label '%s' called from PC=%d is too large for 1 byte pcrel\n",
                    pLabel->m_szName, pSearch->m_RelativeToPC);

                m_State = STATE_FAIL;
                return FALSE;
            }

            *pSearch->m_pBytes = (BYTE) offset;
        }   
        else if (pSearch->m_FixupSize == 4)
        {
            pSearch->m_pBytes[0] = (BYTE) offset;
            pSearch->m_pBytes[1] = (BYTE) (offset >> 8);
            pSearch->m_pBytes[2] = (BYTE) (offset >> 16);
            pSearch->m_pBytes[3] = (BYTE) (offset >> 24);
        }
    }

    return TRUE;
}

/**************************************************************************/
/* rvaLabel is the optional label that indicates this field points at a particular RVA */
void Assembler::AddField(char* name, BinStr* sig, CorFieldAttr flags, char* rvaLabel, BinStr* pVal, ULONG ulOffset)
{
	FieldDescriptor*	pFD;
	ULONG	i,n;
	mdToken tkParent = mdTokenNil;
	Class* pClass;

    if (m_pCurMethod)
		report->error("Field cannot be declared within a method\n");

	if(strlen(name) >= MAX_CLASSNAME_LENGTH)
			report->error("Field '%s' -- name too long (%d characters).\n",name,strlen(name));

	if(sig && (sig->length() >= 2))
	{
		if(sig->ptr()[1] == ELEMENT_TYPE_VOID)
			report->error("Illegal use of type 'void'\n");
	}

    if (m_pCurClass)
	{
		tkParent = m_pCurClass->m_cl;

		if(IsTdInterface(m_pCurClass->m_Attr))
		{
			if(!IsFdStatic(flags)) report->warn("Non-static field in interface (CLS violation)\n");
			if(!IsFdPublic(flags)) report->error("Non-public field in interface\n");
		}
	}
	else 
	{
		if(ulOffset != 0xFFFFFFFF)
		{
			report->warn("Offset in global field '%s' is ignored\n",name);
			ulOffset = 0xFFFFFFFF;
		}
		if(!IsFdStatic(flags))
		{
			if(OnErrGo)	report->error("Non-static global field\n");
			else
			{
				report->warn("Non-static global field, made static\n");
				flags = (CorFieldAttr)(flags | fdStatic);
			}
		}
	}
	pClass = (m_pCurClass ? m_pCurClass : m_pModuleClass);
	n = pClass->m_FieldDList.COUNT();
    DWORD L = (DWORD)strlen(name);
	for(i = 0; i < n; i++)
	{
		pFD = pClass->m_FieldDList.PEEK(i);
		if((pFD->m_tdClass == tkParent)&&(L==pFD->m_dwName)&&(!strcmp(pFD->m_szName,name))
			&&(pFD->m_pbsSig->length() == sig->length())
			&&(memcmp(pFD->m_pbsSig->ptr(),sig->ptr(),sig->length())==0))
		{
			report->error("Duplicate field declaration: '%s'\n",name);
			break;
		}
	}
	if (rvaLabel && !IsFdStatic(flags))
		report->error("Only static fields can have 'at' clauses\n");

	if(i >= n)
	{
        if((pFD = new FieldDescriptor))
		{
			pFD->m_tdClass = tkParent;
			pFD->m_szName = name;
            pFD->m_dwName = L;
			pFD->m_fdFieldTok = mdTokenNil;
			if((pFD->m_ulOffset = ulOffset) != 0xFFFFFFFF) pClass->m_dwNumFieldsWithOffset++;
			pFD->m_rvaLabel = rvaLabel;
			pFD->m_pbsSig = sig;
			pFD->m_pClass = pClass;
			pFD->m_pbsValue = pVal;
			pFD->m_pbsMarshal = m_pMarshal;
			pFD->m_pPInvoke = m_pPInvoke;
			pFD->m_dwAttr = flags;

			m_tkCurrentCVOwner = 0;
			m_pCustomDescrList = &(pFD->m_CustomDescrList);

			pClass->m_FieldDList.PUSH(pFD);
		}
		else
			report->error("Failed to allocate Field Descriptor\n");
	}
	else
	{
		if(pVal) delete pVal;
		if(m_pPInvoke) delete m_pPInvoke;
		if(m_pMarshal) delete m_pMarshal;
		delete name;
	}
	m_pPInvoke = NULL;
	m_pMarshal = NULL;
}

BOOL Assembler::EmitField(FieldDescriptor* pFD)
{
    WCHAR*   wzFieldName=&wzUniBuf[0];
    HRESULT hr;
    DWORD   cSig;
    COR_SIGNATURE* mySig;
    mdFieldDef mb;
	BYTE	ValType = ELEMENT_TYPE_VOID;
	void * pValue = NULL;
	unsigned lVal = 0;
	BOOL ret = TRUE;

	cSig = pFD->m_pbsSig->length();
	mySig = (COR_SIGNATURE*)(pFD->m_pbsSig->ptr());

	WszMultiByteToWideChar(g_uCodePage,0,pFD->m_szName,-1,wzFieldName,dwUniBuf); //int)cFieldNameLength);
	if(IsFdPrivateScope(pFD->m_dwAttr))
	{
		WCHAR* p = wcsstr(wzFieldName,L"$PST04");
		if(p) *p = 0;
	}

	if(pFD->m_pbsValue && pFD->m_pbsValue->length())
	{
		ValType = *(pFD->m_pbsValue->ptr());
		lVal = pFD->m_pbsValue->length() - 1; // 1 is type byte
		pValue = (void*)(pFD->m_pbsValue->ptr() + 1);
		if(ValType == ELEMENT_TYPE_STRING)
		{
			//while(lVal % sizeof(WCHAR)) { pFD->m_pbsValue->appendInt8(0); lVal++; }
			lVal /= sizeof(WCHAR);
		}
	}

    hr = m_pEmitter->DefineField(
        pFD->m_tdClass,
        wzFieldName,
        pFD->m_dwAttr,
        mySig,
        cSig,
        ValType,
        pValue,
        lVal,
        &mb
    );
    if (FAILED(hr))
	{
		report->error("Failed to define field '%s' (HRESULT=0x%08X)\n",pFD->m_szName,hr);
		ret = FALSE;
	}
	else
	{
		//--------------------------------------------------------------------------------
		if(IsFdPinvokeImpl(pFD->m_dwAttr)&&(pFD->m_pPInvoke))
		{
			if(pFD->m_pPInvoke->szAlias == NULL) pFD->m_pPInvoke->szAlias = pFD->m_szName;
			if(FAILED(EmitPinvokeMap(mb,pFD->m_pPInvoke)))
			{
				report->error("Failed to define PInvoke map of .field '%s'\n",pFD->m_szName);
				ret = FALSE;
			}
		}
		//--------------------------------------------------------------------------
		if(pFD->m_pbsMarshal)
		{
			if(FAILED(hr = m_pEmitter->SetFieldMarshal (    
										mb,						// [IN] given a fieldDef or paramDef token  
						(PCCOR_SIGNATURE)(pFD->m_pbsMarshal->ptr()),   // [IN] native type specification   
										pFD->m_pbsMarshal->length())))  // [IN] count of bytes of pvNativeType
			{
				report->error("Failed to set field marshaling for '%s' (HRESULT=0x%08X)\n",pFD->m_szName,hr);
				ret = FALSE;
			}
		}
		//--------------------------------------------------------------------------------
		// Set the the RVA to a dummy value.  later it will be fixed
		// up to be something correct, but if we don't emit something
		// the size of the meta-data will not be correct
		if (pFD->m_rvaLabel) 
		{
			m_fHaveFieldsWithRvas = TRUE;
			hr = m_pEmitter->SetFieldRVA(mb, 0xCCCCCCCC);
			if (FAILED(hr))
			{
				report->error("Failed to set RVA for field '%s' (HRESULT=0x%08X)\n",pFD->m_szName,hr);
				ret = FALSE;
			}
		}
		//--------------------------------------------------------------------------------
		EmitCustomAttributes(mb, &(pFD->m_CustomDescrList));

	}
	pFD->m_fdFieldTok = mb;
	return ret;
}

/**************************************************************************/
void Assembler::EmitByte(int val)
{
	char ch = (char)val;
	//if((val < -128)||(val > 127))
   // 		report->warn("Emitting 0x%X as a byte: data truncated to 0x%X\n",(unsigned)val,(BYTE)ch);
	EmitBytes((BYTE *)&ch,1);
}

/**************************************************************************/
void Assembler::NewSEHDescriptor(void) //sets m_SEHD
{
	m_SEHDstack.PUSH(m_SEHD);
	m_SEHD = new SEH_Descriptor;
	if(m_SEHD == NULL) report->error("Failed to allocate SEH descriptor\n");
}
/**************************************************************************/
void Assembler::SetTryLabels(char * szFrom, char *szTo)
{
	if(!m_SEHD) return;
	Label *pLbl = FindLabel(szFrom);
	if(pLbl)
	{
		m_SEHD->tryFrom = pLbl->m_PC;
        if((pLbl = FindLabel(szTo)))    m_SEHD->tryTo = pLbl->m_PC; //FindLabel: ASSEM.CPP
		else report->error("Undefined 2nd label in 'try <label> to <label>'\n");
	}
	else report->error("Undefined 1st label in 'try <label> to <label>'\n");
}
/**************************************************************************/
void Assembler::SetFilterLabel(char *szFilter)
{
	if(!m_SEHD) return;
	Label *pLbl = FindLabel(szFilter);
	if(pLbl)	m_SEHD->sehFilter = pLbl->m_PC;
	else report->error("Undefined label in 'filter <label>'\n");
}
/**************************************************************************/
void Assembler::SetCatchClass(mdToken catchClass)
{
	if(!m_SEHD) return;
	m_SEHD->cException = catchClass;

}
/**************************************************************************/
void Assembler::SetHandlerLabels(char *szHandlerFrom, char *szHandlerTo)
{
	if(!m_SEHD) return;
	Label *pLbl = FindLabel(szHandlerFrom);
	if(pLbl)
	{
		m_SEHD->sehHandler = pLbl->m_PC;
		if(szHandlerTo) 
		{
			pLbl = FindLabel(szHandlerTo);
			if(pLbl)
			{
				m_SEHD->sehHandlerTo = pLbl->m_PC;
				return;
			}
		}
		else
		{
			m_SEHD->sehHandlerTo = m_SEHD->sehHandler - 1;
			return;
		}
	}
	report->error("Undefined label in 'handler <label> to <label>'\n");
}
/**************************************************************************/
void Assembler::EmitTry(void) //enum CorExceptionFlag kind, char* beginLabel, char* endLabel, char* handleLabel, char* filterOrClass) 
{
	if(m_SEHD)
	{
		bool isFilter=(m_SEHD->sehClause == COR_ILEXCEPTION_CLAUSE_FILTER), 
			 isFault=(m_SEHD->sehClause == COR_ILEXCEPTION_CLAUSE_FAULT),
			 isFinally=(m_SEHD->sehClause == COR_ILEXCEPTION_CLAUSE_FINALLY);

		AddException(m_SEHD->tryFrom, m_SEHD->tryTo, m_SEHD->sehHandler, m_SEHD->sehHandlerTo,
			m_SEHD->cException, isFilter, isFault, isFinally);
	}
	else report->error("Attempt to EmitTry with NULL SEH descriptor\n");
}
/**************************************************************************/

void Assembler::AddException(DWORD pcStart, DWORD pcEnd, DWORD pcHandler, DWORD pcHandlerTo, mdTypeRef crException, BOOL isFilter, BOOL isFault, BOOL isFinally)
{
    if (m_pCurMethod == NULL)
    {
        report->error("Exceptions can be declared only when in a method scope\n");
        return;
    }

    if (m_pCurMethod->m_dwNumExceptions >= m_pCurMethod->m_dwMaxNumExceptions)
    {
		COR_ILMETHOD_SECT_EH_CLAUSE_FAT *ptr = 
			new COR_ILMETHOD_SECT_EH_CLAUSE_FAT[m_pCurMethod->m_dwMaxNumExceptions+MAX_EXCEPTIONS];
		if(ptr == NULL)
		{
			report->error("Failed to reallocate SEH buffer\n");
			return;
		}
		memcpy(ptr,m_pCurMethod->m_ExceptionList,m_pCurMethod->m_dwNumExceptions*sizeof(COR_ILMETHOD_SECT_EH_CLAUSE_FAT));
		delete m_pCurMethod->m_ExceptionList;
		m_pCurMethod->m_ExceptionList = ptr;
		m_pCurMethod->m_dwMaxNumExceptions += MAX_EXCEPTIONS;
    }
    m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].SetFlags(COR_ILEXCEPTION_CLAUSE_OFFSETLEN);
    m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].SetTryOffset(pcStart);
    m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].SetTryLength(pcEnd - pcStart);
    m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].SetHandlerOffset(pcHandler);
    m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].SetHandlerLength(pcHandlerTo - pcHandler);
    m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].SetClassToken(crException);
    if (isFilter) {
        int flag = m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].GetFlags() | COR_ILEXCEPTION_CLAUSE_FILTER;
        m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].SetFlags((CorExceptionFlag)flag);
    }   
    if (isFault) {    
        int flag = m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].GetFlags() | COR_ILEXCEPTION_CLAUSE_FAULT;
        m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].SetFlags((CorExceptionFlag)flag);
    }   
    if (isFinally) {    
        int flag = m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].GetFlags() | COR_ILEXCEPTION_CLAUSE_FINALLY;
        m_pCurMethod->m_ExceptionList[m_pCurMethod->m_dwNumExceptions].SetFlags((CorExceptionFlag)flag);
    }   
    m_pCurMethod->m_dwNumExceptions++;
}

/**************************************************************************/
void Assembler::EmitMaxStack(unsigned val)
{
	if(val > 0xFFFF) report->warn(".maxstack parameter exceeds 65535, truncated to %d\n",val&0xFFFF);
    if (m_pCurMethod) m_pCurMethod->m_MaxStack = val&0xFFFF;
    else  report->error(".maxstack can be used only within a method scope\n");
}

/**************************************************************************/
void Assembler::EmitLocals(BinStr* sig)
{
	if(sig)
	{
		if (m_pCurMethod)
		{
            ARG_NAME_LIST	*pAN, *pList= getArgNameList();
			if(pList)
			{
				VarDescr*		pVD;
				for(pAN=pList; pAN; pAN = pAN->pNext)
				{
					if(pAN->dwAttr == 0) pAN->dwAttr = m_pCurMethod->m_Locals.COUNT() +1;
					(pAN->dwAttr)--;
                    if((pVD = m_pCurMethod->m_Locals.PEEK(pAN->dwAttr)))
					{
						if(pVD->bInScope)
						{
							report->warn("Local var slot %d is in use\n",pAN->dwAttr);
						}
						if(pVD->pbsSig && ((pVD->pbsSig->length() != pAN->pSig->length()) ||
							(memcmp(pVD->pbsSig->ptr(),pAN->pSig->ptr(),pVD->pbsSig->length()))))
						{
							report->error("Local var slot %d: type conflict\n",pAN->dwAttr);
						}
					}
					else
					{ // create new entry:
						for(unsigned n = m_pCurMethod->m_Locals.COUNT(); n <= pAN->dwAttr; n++) 
							m_pCurMethod->m_Locals.PUSH(pVD = new VarDescr);
					}
					pVD->dwSlot = pAN->dwAttr;
					pVD->pbsSig = pAN->pSig;
					pVD->bInScope = TRUE;
				}
				if(pVD->pbsSig && (pVD->pbsSig->length() == 1))
				{
					if(pVD->pbsSig->ptr()[0] == ELEMENT_TYPE_VOID)
						report->error("Illegal local var type: 'void'\n");
				}
				m_pCurMethod->m_pCurrScope->pLocals = 
					m_pCurMethod->catArgNameList(m_pCurMethod->m_pCurrScope->pLocals, pList);
			}
		}
		else	report->error(".locals can be used only within a method scope\n");
		delete sig;
	}
	else report->error("Attempt to EmitLocals with NULL argument\n");
}

/**************************************************************************/
void Assembler::EmitEntryPoint()
{
    if (m_pCurMethod)
    {
		if(!m_fEntryPointPresent)
		{
			if(IsMdStatic(m_pCurMethod->m_Attr))
			{
				m_pCurMethod->m_fEntryPoint = TRUE;
				m_fEntryPointPresent = TRUE;
			}
			else report->error("Non-static method as entry point\n");
		}
		else report->error("Multiple .entrypoint declarations\n");
	}
	else report->error(".entrypoint can be used only within a method scope\n");
}

/**************************************************************************/
void Assembler::EmitZeroInit()
{
    if (m_pCurMethod) m_pCurMethod->m_Flags |= CorILMethod_InitLocals;
	else report->error(".zeroinit can be used only within a method scope\n");
}

/**************************************************************************/
void Assembler::SetImplAttr(unsigned short attrval)
{
	if (m_pCurMethod)
	{
		if(IsMiNative(attrval)||IsMiOPTIL(attrval)||IsMiUnmanaged(attrval))
			report->error("Cannot compile native/unmanaged method\n");
		m_pCurMethod->m_wImplAttr = attrval;
	}
}

/**************************************************************************/
void Assembler::EmitData(void* buffer, unsigned len)
{
	if(buffer && len)
	{
		void* ptr;
		HRESULT hr = m_pCeeFileGen->GetSectionBlock(m_pCurSection, len, 1, &ptr); 
		if (FAILED(hr)) 
		{
			report->error("Could not extend data section (out of memory?)");
			exit(1);
		}
		memcpy(ptr, buffer, len);
	}
}

/**************************************************************************/
void Assembler::EmitDD(char *str)
{
    DWORD       dwAddr = 0;
    GlobalLabel *pLabel = FindGlobalLabel(str);

	ULONG loc;
	HRESULT hr = m_pCeeFileGen->GetSectionDataLen(m_pCurSection, &loc);
	_ASSERTE(SUCCEEDED(hr));

	DWORD* ptr;
	hr = m_pCeeFileGen->GetSectionBlock(m_pCurSection, sizeof(DWORD), 1, (void**) &ptr); 
	if (FAILED(hr)) 
	{
		report->error("Could not extend data section (out of memory?)");
		exit(1);
	}

	if (pLabel != 0) {
		dwAddr = pLabel->m_GlobalOffset;
		if (pLabel->m_Section != m_pGlobalDataSection) {
			report->error("For '&label', label must be in data section");
			m_State = STATE_FAIL;
			}
		}
	else
		AddDeferredGlobalFixup(str, (BYTE*) ptr);

    hr = m_pCeeFileGen->AddSectionReloc(m_pCurSection, loc, m_pGlobalDataSection, srRelocHighLow);
	_ASSERTE(SUCCEEDED(hr));
	m_dwComImageFlags &= ~COMIMAGE_FLAGS_ILONLY; 
	m_dwComImageFlags |= COMIMAGE_FLAGS_32BITREQUIRED;
    *ptr = dwAddr;
}

/**************************************************************************/
GlobalLabel *Assembler::FindGlobalLabel(char *pszName)
{
    GlobalLabel lSearch(pszName,0,NULL), *pL;
    pL =  m_lstGlobalLabel.FIND(&lSearch);
    lSearch.m_szName = NULL;
    return pL;
}

/**************************************************************************/

GlobalFixup *Assembler::AddDeferredGlobalFixup(char *pszLabel, BYTE* pReference) 
{
    GlobalFixup *pNew = new GlobalFixup(pszLabel, (BYTE*) pReference);
    if (pNew == NULL)
    {
        report->error("Failed to allocate global fixup\n");
        m_State = STATE_FAIL;
    }
	else
		m_lstGlobalFixup.PUSH(pNew);

    return pNew;
}

/**************************************************************************/
void Assembler::AddDeferredILFixup(ILFixupType Kind)
{ 
    _ASSERTE(Kind != ilGlobal);
  AddDeferredILFixup(Kind, NULL);
}
/**************************************************************************/

void Assembler::AddDeferredILFixup(ILFixupType Kind,
                                   GlobalFixup *GFixup)
{ 
    ILFixup *pNew = new ILFixup(m_CurPC, Kind, GFixup);

	_ASSERTE(m_pCurMethod != NULL);
	if (pNew == NULL)
	{ 
        report->error("Failed to allocate IL fixup\n");
		m_State = STATE_FAIL;
	}
	else
		m_lstILFixup.PUSH(pNew);
}

/**************************************************************************/
void Assembler::EmitDataString(BinStr* str) 
{
	if(str)
	{
		str->appendInt8(0);
		DWORD   DataLen = str->length();
		char	*pb = (char*)(str->ptr());	
		WCHAR   *UnicodeString = (DataLen >= dwUniBuf) ? new WCHAR[DataLen] : &wzUniBuf[0];

		if(UnicodeString)
		{
			WszMultiByteToWideChar(g_uCodePage,0,pb,-1,UnicodeString,DataLen);
			EmitData(UnicodeString,DataLen*sizeof(WCHAR));
			if(DataLen >= dwUniBuf) delete [] UnicodeString;
		}
		else report->error("\nOut of memory!\n");
		delete str;
	}
}



/**************************************************************************/
unsigned Assembler::OpcodeLen(Instr* instr)
{
	return (m_fStdMapping ? OpcodeInformation[instr->opcode].Len : 3);
}
/**************************************************************************/
void Assembler::EmitOpcode(Instr* instr)
{
	if(m_fIncludeDebugInfo &&
       ((instr->linenum != m_ulLastDebugLine)||(instr->column != m_ulLastDebugColumn)))
	{
		if(m_pCurMethod)
		{
			LinePC *pLPC=NULL;
            unsigned N = m_pCurMethod->m_LinePCList.COUNT();
            if(N)
            {
                pLPC = m_pCurMethod->m_LinePCList.PEEK(N-1);
                if(pLPC->PC != instr->pc) pLPC = NULL;
            }
            if(pLPC)
            {
                pLPC->Line = instr->linenum;
                pLPC->Column = instr->column;
                pLPC->LineEnd = instr->linenum_end;
                pLPC->ColumnEnd = instr->column_end;
                pLPC->pWriter = instr->pWriter;
            }
            else
            {
                pLPC = new LinePC;
    			if(pLPC)
    			{
    				pLPC->Line = instr->linenum;
    				pLPC->Column = instr->column;
    				pLPC->LineEnd = instr->linenum_end;
    				pLPC->ColumnEnd = instr->column_end;
    				pLPC->PC = instr->pc;
                    pLPC->pWriter = instr->pWriter;
    				m_pCurMethod->m_LinePCList.PUSH(pLPC);
    			}
    			else report->error("\nOut of memory!\n");
		    }
        }
		m_ulLastDebugLine = instr->linenum;
		m_ulLastDebugColumn = instr->column;
	}
	if(instr->opcode == CEE_ENDFILTER)
	{
		if(m_pCurMethod)
		{
			if(m_pCurMethod->m_dwNumEndfilters >= m_pCurMethod->m_dwMaxNumEndfilters)
			{
				DWORD *pdw = new DWORD[m_pCurMethod->m_dwMaxNumEndfilters+MAX_EXCEPTIONS];
				if(pdw == NULL)
				{
					report->error("Failed to reallocate auxiliary SEH buffer\n");
					return;
				}
				memcpy(pdw,m_pCurMethod->m_EndfilterOffsetList,m_pCurMethod->m_dwNumEndfilters*sizeof(DWORD));
				delete m_pCurMethod->m_EndfilterOffsetList;
				m_pCurMethod->m_EndfilterOffsetList = pdw;
				m_pCurMethod->m_dwMaxNumEndfilters += MAX_EXCEPTIONS;
			}
			m_pCurMethod->m_EndfilterOffsetList[m_pCurMethod->m_dwNumEndfilters++] = m_CurPC+2;
		}
	}
    if (m_fStdMapping)
    {
        if (OpcodeInformation[instr->opcode].Len == 2) 
			EmitByte(OpcodeInformation[instr->opcode].Std1);
        EmitByte(OpcodeInformation[instr->opcode].Std2);
    }
    else
    {
		unsigned short us = (unsigned short)instr->opcode;
        EmitByte(REFPRE);
        EmitBytes((BYTE *)&us,2);
    }
    instr->opcode = -1;
}

/**************************************************************************/
void Assembler::EmitInstrVar(Instr* instr, int var) 
{
	unsigned opc = instr->opcode;
	EmitOpcode(instr);
	if (isShort(opc)) 
	{
		EmitByte(var);
	}
	else
	{ 
		short sh = (short)var;
		EmitBytes((BYTE *)&sh,2);
	}
} 

/**************************************************************************/
void Assembler::EmitInstrVarByName(Instr* instr, char* label)
{
	int idx = -1, nArgVarFlag=0;
	switch(instr->opcode)
	{
		case CEE_LDARGA:
		case CEE_LDARGA_S:
		case CEE_LDARG:
		case CEE_LDARG_S:
		case CEE_STARG:
		case CEE_STARG_S:
			nArgVarFlag++;
		case CEE_LDLOCA:
		case CEE_LDLOCA_S:
		case CEE_LDLOC:
		case CEE_LDLOC_S:
		case CEE_STLOC:
		case CEE_STLOC_S:

			if(m_pCurMethod)
			{
                DWORD L = (DWORD)strlen(label);
				if(nArgVarFlag == 1)
				{
					idx = m_pCurMethod->findArgNum(m_pCurMethod->m_firstArgName,label,L);
				}
				else
				{
					for(Scope* pSC = m_pCurMethod->m_pCurrScope; pSC; pSC=pSC->pSuperScope)
					{
					    idx = m_pCurMethod->findArgNum(pSC->pLocals,label,L);
						if(idx >= 0) break;
					}
				}
				if(idx >= 0) EmitInstrVar(instr, 
					((nArgVarFlag==0)||(m_pCurMethod->m_Attr & mdStatic))? idx : idx+1);
				else	report->error("Undeclared identifier %s\n",label);
			}
			else
				report->error("Instructions can be used only when in a method scope\n");
			break;
		default:
			report->error("Named argument illegal for this instruction\n");
	}
}

/**************************************************************************/
void Assembler::EmitInstrI(Instr* instr, int val) 
{
	unsigned opc = instr->opcode;
	EmitOpcode(instr);
	if (isShort(opc)) 
	{
		EmitByte(val);
	}
	else
	{
		int i = val;
		EmitBytes((BYTE *)&i,sizeof(int));
	}
}

/**************************************************************************/
void Assembler::EmitInstrI8(Instr* instr, __int64* val)
{
	EmitOpcode(instr);
	EmitBytes((BYTE *)val, sizeof(__int64));
	delete val;
}

/**************************************************************************/
void Assembler::EmitInstrR(Instr* instr, double* pval)
{
	unsigned opc = instr->opcode;
	EmitOpcode(instr);
	if (isShort(opc)) 
	{
		float val = (float)*pval;
		EmitBytes((BYTE *)&val, sizeof(float));
	}
	else
		EmitBytes((BYTE *)pval, sizeof(double));
}

/**************************************************************************/
void Assembler::EmitInstrBrTarget(Instr* instr, char* label) 
{
	int pcrelsize = (isShort(instr->opcode) ? 1 : 4);
	EmitOpcode(instr);
    AddDeferredFixup(label, m_pCurOutputPos,
                                   (m_CurPC + pcrelsize), pcrelsize);
	if(pcrelsize == 1) EmitByte(0);
	else
	{
		DWORD i = 0;
		EmitBytes((BYTE *)&i,4);
	}
}
/**************************************************************************/
void Assembler::AddDeferredFixup(char *pszLabel, BYTE *pBytes, DWORD RelativeToPC, BYTE FixupSize)
{
    Fixup *pNew = new Fixup(pszLabel, pBytes, RelativeToPC, FixupSize);

    if (pNew == NULL)
    {
        report->error("Failed to allocate deferred fixup\n");
        m_State = STATE_FAIL;
    }
    else
		m_lstFixup.PUSH(pNew);
}
/**************************************************************************/
void Assembler::EmitInstrBrOffset(Instr* instr, int offset) 
{
	unsigned opc=instr->opcode;
	EmitOpcode(instr);
	if(isShort(opc))	EmitByte(offset);
	else
	{
		int i = offset;
		EmitBytes((BYTE *)&i,4);
	}
}

/**************************************************************************/
mdToken Assembler::MakeMemberRef(mdToken cr, char* pszMemberName, BinStr* sig, unsigned opcode_len)
{	
    DWORD			cSig = sig->length();
    COR_SIGNATURE*	mySig = (COR_SIGNATURE *)(sig->ptr());
	mdToken		    mr = mdMemberRefNil;
	Class*			pClass = NULL;
    if(TypeFromToken(cr) == mdtTypeDef) pClass = m_lstClass.PEEK(RidFromToken(cr)-1);
	if((TypeFromToken(cr) == mdtTypeDef)||(cr == mdTokenNil))
	{
		MemberRefDescriptor* pMRD = new MemberRefDescriptor;
        if(pMRD)
        {
            pMRD->m_tdClass = cr;
            pMRD->m_pClass = pClass;
            pMRD->m_szName = pszMemberName;
            pMRD->m_dwName = (DWORD)strlen(pszMemberName);
            pMRD->m_pSigBinStr = sig;
            if(*(sig->ptr())== IMAGE_CEE_CS_CALLCONV_FIELD)
            {
                m_LocalFieldRefDList.PUSH(pMRD);
                mr = 0x98000000 | m_LocalFieldRefDList.COUNT();
            }
            else
            {
                m_LocalMethodRefDList.PUSH(pMRD);
                mr = 0x99000000 | m_LocalMethodRefDList.COUNT();
            }
        }
        else
        {
            report->error("Failed to allocate MemberRef Descriptor\n");
            return 0;
        }
        if(opcode_len) m_pCurMethod->m_LocalMemberRefFixupList.PUSH(
                new LocalMemberRefFixup(mr,(size_t)(m_CurPC + opcode_len)));
	}
	else
	{
		WszMultiByteToWideChar(g_uCodePage,0,pszMemberName,-1,wzUniBuf,dwUniBuf);

		if(cr == mdTokenNil) cr = mdTypeRefNil;
		if(TypeFromToken(cr) == mdtAssemblyRef)
		{
			report->error("Cross-assembly global references are not supported ('%s')\n", pszMemberName);
			mr = 0;
		}
		else
		{
			HRESULT hr = m_pEmitter->DefineMemberRef(cr, wzUniBuf, mySig, cSig, &mr);
			if(FAILED(hr))
			{
				report->error("Unable to define member reference '%s'\n", pszMemberName);
				mr = 0;
			}
		}
		//if(m_fOBJ)	m_pCurMethod->m_TRDList.PUSH(new TokenRelocDescr(m_CurPC,mr));
		delete pszMemberName;
		delete sig;
	}
    return mr;
}

/**************************************************************************/
mdToken Assembler::MakeMethodSpec(mdToken tkParent, BinStr* sig, unsigned opcode_len)
{	
    DWORD			cSig = sig->length();
    COR_SIGNATURE*	mySig = (COR_SIGNATURE *)(sig->ptr());
    mdMethodSpec mi = mdMethodSpecNil;
	if(TypeFromToken(tkParent) == 0x99000000) // Local MemberRef: postpone until resolved
	{
        MemberRefDescriptor* pMRD = new MemberRefDescriptor;
        if(pMRD)
        {
            memset(pMRD,0,sizeof(MemberRefDescriptor));
            pMRD->m_tdClass = tkParent;
            pMRD->m_pSigBinStr = sig;
            m_MethodSpecList.PUSH(pMRD);
            mi = 0x9A000000 | m_MethodSpecList.COUNT();
        }
        else
        {
            report->error("Failed to allocate MemberRef Descriptor\n");
            return 0;
        }
        if(opcode_len) m_pCurMethod->m_LocalMemberRefFixupList.PUSH(
                new LocalMemberRefFixup(mi,(size_t)(m_CurPC + opcode_len)));
	}
    else
    {
        HRESULT hr = m_pEmitter->DefineMethodSpec(tkParent, mySig, cSig, &mi);
        if(FAILED(hr))
        {
        	report->error("Unable to define method instantiation");
        	return 0;
        }
    }
    return mi;
}

/**************************************************************************/
void Assembler::EndEvent(void) 
{ 
	Class* pClass = (m_pCurClass ? m_pCurClass : m_pModuleClass);
	pClass->m_EventDList.PUSH(m_pCurEvent); 
	m_pCurEvent = NULL; 
}

void Assembler::ResetEvent(char* szName, mdToken typeSpec, DWORD dwAttr) 
{
	if(strlen(szName) >= MAX_CLASSNAME_LENGTH)
			report->error("Event '%s' -- name too long (%d characters).\n",szName,strlen(szName));
    if((m_pCurEvent = new EventDescriptor))
	{
		memset(m_pCurEvent,0,sizeof(EventDescriptor));
		m_pCurEvent->m_tdClass = m_pCurClass->m_cl;
		m_pCurEvent->m_szName = szName;
		m_pCurEvent->m_dwAttr = dwAttr;
		m_pCurEvent->m_tkEventType = typeSpec;
		m_tkCurrentCVOwner = 0;
		m_pCustomDescrList = &(m_pCurEvent->m_CustomDescrList);
	}
	else report->error("Failed to allocate Event Descriptor\n");

}

void Assembler::SetEventMethod(int MethodCode, mdToken typeSpec, char* pszMethodName, BinStr* sig) 
{
    mdToken tk = MakeMemberRef(typeSpec,pszMethodName,sig,0);
	switch(MethodCode)
	{
		case 0:
			m_pCurEvent->m_tkAddOn = tk;
			break;
		case 1:
			m_pCurEvent->m_tkRemoveOn = tk;
			break;
		case 2:
			m_pCurEvent->m_tkFire = tk;
			break;
		case 3:
			m_pCurEvent->m_tklOthers.PUSH((mdToken*)tk);
			break;
	}
}
/**************************************************************************/

void Assembler::EndProp(void)
{ 
	Class* pClass = (m_pCurClass ? m_pCurClass : m_pModuleClass);
	pClass->m_PropDList.PUSH(m_pCurProp); 
	m_pCurProp = NULL; 
}

void Assembler::ResetProp(char * szName, BinStr* bsType, DWORD dwAttr, BinStr* pValue) 
{
    DWORD			cSig = bsType->length();
    COR_SIGNATURE*	mySig = (COR_SIGNATURE *)(bsType->ptr());

	if(strlen(szName) >= MAX_CLASSNAME_LENGTH)
			report->error("Property '%s' -- name too long (%d characters).\n",szName,strlen(szName));
	m_pCurProp = new PropDescriptor;
	if(m_pCurProp == NULL)
	{
		report->error("Failed to allocate Property Descriptor\n");
		return;
	}
	memset(m_pCurProp,0,sizeof(PropDescriptor));
	m_pCurProp->m_tdClass = m_pCurClass->m_cl;
	m_pCurProp->m_szName = szName;
	m_pCurProp->m_dwAttr = dwAttr;

	m_pCurProp->m_pSig = new COR_SIGNATURE[cSig];
	if(m_pCurProp->m_pSig == NULL)
	{
		report->error("\nOut of memory!\n");
		return;
	}
	memcpy(m_pCurProp->m_pSig,mySig,cSig);
	m_pCurProp->m_dwCSig = cSig;

	if(pValue && pValue->length())
	{
		BYTE* pch = pValue->ptr();
		m_pCurProp->m_dwCPlusTypeFlag = (DWORD)(*pch);
		m_pCurProp->m_cbValue = pValue->length() - 1;
		m_pCurProp->m_pValue = (PVOID)(pch+1);
		if(m_pCurProp->m_dwCPlusTypeFlag == ELEMENT_TYPE_STRING) m_pCurProp->m_cbValue /= sizeof(WCHAR);
		m_pCurProp->m_dwAttr |= prHasDefault;
	}
	else
	{
		m_pCurProp->m_dwCPlusTypeFlag = ELEMENT_TYPE_VOID;
		m_pCurProp->m_pValue = NULL;
		m_pCurProp->m_cbValue = 0;
	}
	m_tkCurrentCVOwner = 0;
	m_pCustomDescrList = &(m_pCurProp->m_CustomDescrList);
}

void Assembler::SetPropMethod(int MethodCode, mdToken typeSpec, char* pszMethodName, BinStr* sig)
{
    mdToken tk = MakeMemberRef(typeSpec,pszMethodName,sig,0);
    switch(MethodCode)
	{
		case 0:
			m_pCurProp->m_tkSet = tk;
			break;
		case 1:
			m_pCurProp->m_tkGet = tk;
			break;
		case 2:
			m_pCurProp->m_tklOthers.PUSH((mdToken*)tk);
			break;
	}
}

/**************************************************************************/
void Assembler::EmitInstrStringLiteral(Instr* instr, BinStr* literal, BOOL ConvertToUnicode)
{
    DWORD   DataLen = literal->length(),L;
	unsigned __int8	*pb = literal->ptr();
	HRESULT hr = S_OK;
	mdToken tk;
    WCHAR   *UnicodeString;
	if(DataLen == 0) 
	{
		//report->warn("Zero length string emitted\n");
		ConvertToUnicode = FALSE;
	}
	if(ConvertToUnicode)
	{
		UnicodeString = (DataLen >= dwUniBuf) ? new WCHAR[DataLen+1] : &wzUniBuf[0];
		literal->appendInt8(0);
		pb = literal->ptr();
		// convert string to Unicode
		L = UnicodeString ? WszMultiByteToWideChar(g_uCodePage,0,(char*)pb,-1,UnicodeString,DataLen+1) : 0;
		if(L == 0)
		{
			char* sz=NULL;
			DWORD dw;
			switch(dw=GetLastError())
			{
				case ERROR_INSUFFICIENT_BUFFER: sz = "ERROR_INSUFFICIENT_BUFFER"; break;
				case ERROR_INVALID_FLAGS:		sz = "ERROR_INVALID_FLAGS"; break;
				case ERROR_INVALID_PARAMETER:	sz = "ERROR_INVALID_PARAMETER"; break;
				case ERROR_NO_UNICODE_TRANSLATION: sz = "ERROR_NO_UNICODE_TRANSLATION"; break;
			}
			if(sz)	report->error("Failed to convert string '%s' to Unicode: %s\n",(char*)pb,sz);
			else	report->error("Failed to convert string '%s' to Unicode: error 0x%08X\n",(char*)pb,dw);
			delete instr;
			goto OuttaHere;
		}
		L--;
	}
	else
	{
		if(DataLen & 1)
		{
			literal->appendInt8(0);
			pb = literal->ptr();
			DataLen++;
		}
		UnicodeString = (WCHAR*)pb;
		L = DataLen/sizeof(WCHAR);
	}
	// Add the string data to the metadata, which will fold dupes.
	hr = m_pEmitter->DefineUserString(
		UnicodeString,
		L,
		&tk
	);
	if (FAILED(hr))
    {
        report->error("Failed to add user string using DefineUserString, hr=0x%08x, data: '%S'\n",
               hr, UnicodeString);
		delete instr;
    }
	else
	{
		EmitOpcode(instr);
		if(m_fOBJ)	m_pCurMethod->m_TRDList.PUSH(new TokenRelocDescr(m_CurPC,tk));

		EmitBytes((BYTE *)&tk,sizeof(mdToken));
	}
OuttaHere:
	delete literal;
	if(((void*)UnicodeString != (void*)pb)&&(DataLen >= dwUniBuf)) delete [] UnicodeString;
}

/**************************************************************************/
void Assembler::EmitInstrSig(Instr* instr, BinStr* sig)
{
	mdSignature MetadataToken;
    DWORD       cSig = sig->length();
    COR_SIGNATURE* mySig = (COR_SIGNATURE *)(sig->ptr());

	if (FAILED(m_pEmitter->GetTokenFromSig(mySig, cSig, &MetadataToken)))
	{
		report->error("Unable to convert signature to metadata token.\n");
		delete instr;
	}
	else
	{
		EmitOpcode(instr);
		if(m_fOBJ)	m_pCurMethod->m_TRDList.PUSH(new TokenRelocDescr(m_CurPC,MetadataToken));
		EmitBytes((BYTE *)&MetadataToken, sizeof(mdSignature));
	}
	delete sig;
}

/**************************************************************************/
void Assembler::EmitInstrRVA(Instr* instr, char* label, bool islabel)
{
    long lOffset = 0;
    GlobalLabel *pGlobalLabel;

	EmitOpcode(instr);

	if(islabel)
	{
		AddDeferredILFixup(ilRVA);
        if((pGlobalLabel = FindGlobalLabel(label))) lOffset = pGlobalLabel->m_GlobalOffset;
        else
		{
			GlobalFixup *GFixup = AddDeferredGlobalFixup(label, m_pCurOutputPos);
			AddDeferredILFixup(ilGlobal, GFixup);
		}
	}
	else
	{
		lOffset = (long)label;
	}
	EmitBytes((BYTE *)&lOffset,4);
}

/**************************************************************************/
void Assembler::EmitInstrSwitch(Instr* instr, Labels* targets) 
{
	Labels	*pLbls;
    int     NumLabels;
	Label	*pLabel;
	long	offset;

	EmitOpcode(instr);

    // count # labels
	for(pLbls = targets, NumLabels = 0; pLbls; pLbls = pLbls->Next, NumLabels++);

    EmitBytes((BYTE *)&NumLabels,sizeof(int));
    DWORD PC_nextInstr = m_CurPC + 4*NumLabels;
	for(pLbls = targets; pLbls; pLbls = pLbls->Next)
	{
		if(pLbls->isLabel)
		{
            if((pLabel = FindLabel(pLbls->Label)))
			{
				offset = pLabel->m_PC - PC_nextInstr;
				if (m_fDisplayTraceOutput) report->msg("%d\n", offset);
			}
			else
			{
				// defer until we find the label
				AddDeferredFixup(pLbls->Label, m_pCurOutputPos, PC_nextInstr, 4 /* pcrelsize */ );
				offset = 0;
                pLbls->Label = NULL;
				if (m_fDisplayTraceOutput) report->msg("forward label %s\n", pLbls->Label);
			}
		}
		else
		{
            offset = (long)pLbls->Label;
            if (m_fDisplayTraceOutput) report->msg("%d\n", offset);
		}
        EmitBytes((BYTE *)&offset, sizeof(long));
	}
	delete targets;
}

/**************************************************************************/
void Assembler::EmitInstrPhi(Instr* instr, BinStr* vars) 
{
	BYTE i = (BYTE)(vars->length() / 2);
	EmitOpcode(instr);
	EmitBytes((BYTE *)&i,1);
	_ASSERTE(!"The following call to EmitBytes isn't endian aware");
	EmitBytes((BYTE *)vars->ptr(), vars->length());
	delete vars;
}

/**************************************************************************/
void Assembler::EmitLabel(char* label) 
{
	_ASSERTE(m_pCurMethod);
	AddLabel(m_CurPC, label);
}
/**************************************************************************/
void Assembler::EmitDataLabel(char* label) 
{
	AddGlobalLabel(label, m_pCurSection);
}

/**************************************************************************/
void Assembler::EmitBytes(BYTE *p, unsigned len) 
{
	if(m_pCurOutputPos + len >= m_pEndOutputPos)
	{
		size_t buflen = m_pEndOutputPos - m_pOutputBuffer;
		size_t newlen = buflen+(len/OUTPUT_BUFFER_INCREMENT + 1)*OUTPUT_BUFFER_INCREMENT;
		BYTE *pb = new BYTE[newlen];
		if(pb == NULL)
		{
			report->error("Failed to extend output buffer from %d to %d bytes. Aborting\n",
				buflen, newlen);
			exit(1);
		}
		size_t delta = pb - m_pOutputBuffer;
		int i;
		Fixup* pSearch;
		GlobalFixup *pGSearch;
        for (i=0; (pSearch = m_lstFixup.PEEK(i)); i++) pSearch->m_pBytes += delta;
        for (i=0; (pGSearch = m_lstGlobalFixup.PEEK(i)); i++) //need to move only those pointing to output buffer
		{
			if((pGSearch->m_pReference >= m_pOutputBuffer)&&(pGSearch->m_pReference <= m_pEndOutputPos))
				pGSearch->m_pReference += delta;
		}

		
		memcpy(pb,m_pOutputBuffer,m_CurPC);
		delete m_pOutputBuffer;
		m_pOutputBuffer = pb;
		m_pCurOutputPos = &m_pOutputBuffer[m_CurPC];
		m_pEndOutputPos = &m_pOutputBuffer[newlen];

	}

	switch (len)
	{
        case 1:
		*m_pCurOutputPos = *p;
                break;
        case 2:
		SET_UNALIGNED_VAL16(m_pCurOutputPos, GET_UNALIGNED_16(p));
                break;
        case 4:
		SET_UNALIGNED_VAL32(m_pCurOutputPos, GET_UNALIGNED_32(p));
                break;
        case 8:
		SET_UNALIGNED_VAL64(m_pCurOutputPos, GET_UNALIGNED_64(p));
                break;
        default:
		_ASSERTE(!"NYI");
                break;
	}

	m_pCurOutputPos += len;
	m_CurPC += len;
}

/**************************************************************************/
void Assembler::EmitSecurityInfo(mdToken            token,
                                 PermissionDecl*    pPermissions,
                                 PermissionSetDecl* pPermissionSets)
{
    PermissionDecl     *pPerm, *pPermNext;
    PermissionSetDecl  *pPset, *pPsetNext;
    unsigned            uCount = 0;
    COR_SECATTR        *pAttrs;
    unsigned            i;
    unsigned            uLength;
    mdTypeRef           tkTypeRef;
    BinStr             *pSig;
    char               *szMemberName;
    DWORD               dwErrorIndex;

    if (pPermissions) {

        for (pPerm = pPermissions; pPerm; pPerm = pPerm->m_Next)
            uCount++;

        if((pAttrs = new COR_SECATTR[uCount])==NULL)
		{
			report->error("\nOut of memory!\n");
			return;
		}

        mdToken tkMscorlib = m_fIsMscorlib ? 1 : GetAsmRef("mscorlib");
        tkTypeRef = ResolveClassRef(tkMscorlib,"System.Security.Permissions.SecurityAction", NULL);
        for (pPerm = pPermissions, i = 0; pPerm; pPerm = pPermNext, i++) {
            pPermNext = pPerm->m_Next;

            pSig = new BinStr();
            pSig->appendInt8(IMAGE_CEE_CS_CALLCONV_DEFAULT_HASTHIS);
            pSig->appendInt8(1);
            pSig->appendInt8(ELEMENT_TYPE_VOID);
            pSig->appendInt8(ELEMENT_TYPE_VALUETYPE);
            uLength = CorSigCompressToken(tkTypeRef, pSig->getBuff(5));
            pSig->remove(5 - uLength);

            if((szMemberName = new char[strlen(COR_CTOR_METHOD_NAME) + 1]))
			{
				strcpy(szMemberName, COR_CTOR_METHOD_NAME);
		        pAttrs[i].tkCtor = MakeMemberRef(pPerm->m_TypeSpec, szMemberName, pSig, 0);
				pAttrs[i].pCustomAttribute = (const void *)pPerm->m_Blob;
				pAttrs[i].cbCustomAttribute = pPerm->m_BlobLength;
				//pPerm->m_TypeSpec = NULL;
				pPerm->m_Blob = NULL;
			}
			else report->error("\nOut of memory!\n");
            delete pPerm;
        }

        if (m_pEmitter->DefineSecurityAttributeSet(token,
                                                   pAttrs,
                                                   uCount,
                                                   &dwErrorIndex))
            if (dwErrorIndex == uCount)
                report->error("Failed to define security attribute set for 0x%08X\n", token);
            else
                report->error("Failed to define security attribute set for 0x%08X\n  (error in permission %u)\n",
                              token, uCount - dwErrorIndex);

        for (i =0; i < uCount; i++)
            delete [] (BYTE*)pAttrs[i].pCustomAttribute;
        delete [] pAttrs;
    }

    for (pPset = pPermissionSets; pPset; pPset = pPsetNext) {
        pPsetNext = pPset->m_Next;
        if(m_pEmitter->DefinePermissionSet(token,
                                           pPset->m_Action,
                                           pPset->m_Value->ptr(),
                                           pPset->m_Value->length(),
                                           NULL))
            report->error("Failed to define security permission set for 0x%08X\n", token);
        delete pPset;
    }
}

void Assembler::AddMethodImpl(mdToken tkImplementedTypeSpec, char* szImplementedName, BinStr* pImplementedSig, 
			      mdToken tkImplementingTypeSpec, char* szImplementingName, BinStr* pImplementingSig)
{
    if(m_pCurClass)
    {
        MethodImplDescriptor*	pMID = new MethodImplDescriptor;
        if(pMID == NULL)
        {
            report->error("Failed to allocate MethodImpl Descriptor\n");
            return;
        }
        pMID->m_tkDefiningClass = m_pCurClass->m_cl;
        if(szImplementingName) //called from class scope, overriding method specified
        {
            pMID->m_tkImplementedMethod = MakeMemberRef(tkImplementedTypeSpec,szImplementedName,pImplementedSig,0);
            pMID->m_tkImplementingMethod = MakeMemberRef(tkImplementingTypeSpec,szImplementingName,pImplementingSig,0);
        }
        else	//called from method scope, use current method as overriding
        {
            if(m_pCurMethod)
            {
                if (pImplementedSig == NULL)
                {
                    pImplementedSig = new BinStr();
                    memcpy(pImplementedSig->getBuff(m_pCurMethod->m_dwMethodCSig),
                       m_pCurMethod->m_pMethodSig,m_pCurMethod->m_dwMethodCSig);
                }
                pMID->m_tkImplementedMethod = MakeMemberRef(tkImplementedTypeSpec,szImplementedName,pImplementedSig,0);
                pMID->m_tkImplementingMethod = 0;
                
                m_pCurMethod->m_MethodImplDList.PUSH(pMID); // copy goes to method's own list (ptr only)
            }
            else
            {
                report->error("No overriding method specified");
                delete pMID;
                return;
            }
        }
        m_MethodImplDList.PUSH(pMID);
    }
    else
        report->error(".override directive outside class scope");
}
// source file name paraphernalia
void Assembler::SetSourceFileName(char* szName)
{
	if(szName)
	{
        if(*szName)
        {
    		if(strcmp(m_szSourceFileName,szName))
    		{
    			strcpy(m_szSourceFileName,szName);
    		}
            if(m_fIncludeDebugInfo)
            {
                DocWriter* pDW;
                unsigned i=0;
                while((pDW = m_DocWriterList.PEEK(i++)))
                {
                    if(!strcmp(szName,pDW->Name)) break;
                }
                if(pDW)
                {
                     m_pSymDocument = pDW->pWriter;
                }
                else if(m_pSymWriter)
                {
                    HRESULT hr;
                    WszMultiByteToWideChar(g_uCodePage,0,szName,-1,wzUniBuf,dwUniBuf);
                    if(FAILED(hr=m_pSymWriter->DefineDocument(wzUniBuf,&m_guidLang,
                        &m_guidLangVendor,&m_guidDoc,&m_pSymDocument)))
                    { 
                        m_pSymDocument = NULL;
                        report->error("Failed to define a document writer");
                    }
                    pDW = new DocWriter();
                    if(pDW) {
                        pDW->Name = szName;
                        pDW->pWriter = m_pSymDocument;
                        m_DocWriterList.PUSH(pDW);
                    }
                    else
                    {
                        report->error("Out of memory");
                    }
                }
            }
        }
    }
}
void Assembler::SetSourceFileName(BinStr* pbsName)
{
	if(pbsName && pbsName->length())
	{
		pbsName->appendInt8(0);
		SetSourceFileName((char*)(pbsName->ptr()));
	}
}
