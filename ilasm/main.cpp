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
#include "asmparse.h"
#include "version/__file__.ver"
#include "version/corver.h"
#include "shimload.h"
#include <palstartupw.h>




WCHAR* EqualOrColon(WCHAR* szArg)
{
	WCHAR* pchE = wcschr(szArg,L'=');
	WCHAR* pchC = wcschr(szArg,L':');
	WCHAR* ret;
	if(pchE == NULL) ret = pchC;
	else if(pchC == NULL) ret = pchE;
	else ret = (pchE < pchC)? pchE : pchC;
	return ret;
}

static DWORD	g_dwSubsystem=0,g_dwComImageFlags=0,g_dwFileAlignment=0;
static size_t	g_stBaseAddress=0;
static BOOL	    g_bOnUnicode;

extern "C" int _cdecl wmain(int argc, WCHAR **argv)
{
    char        szInputFilename[MAX_FILENAME_LENGTH*3];
	WCHAR		wzInputFilename[MAX_FILENAME_LENGTH];
	WCHAR		wzOutputFilename[MAX_FILENAME_LENGTH];
	WCHAR		*pwzInputFiles[1024];
    int         i, NumFiles = 0;
    bool	    IsDLL = false, IsOBJ = false;
	char		szOpt[128];
	Assembler	*pAsm;
	FileReadStream *pIn;
	AsmParse	*pParser;
	int			exitval=1;
	bool		bLogo = TRUE;
	BOOL		bNoDebug = TRUE;

    unsigned    uCodePage;

    bool bClock = false;
    Clockwork   cw;

    memset(&cw,0,sizeof(Clockwork));
	cw.cBegin = GetTickCount();

	g_bOnUnicode = OnUnicodeSystem();
	memset(wzOutputFilename,0,sizeof(wzOutputFilename));

    if (argc < 2 || ! wcscmp(argv[1], L"/?") || ! wcscmp(argv[1],L"-?"))
    { 
		printf("\nMicrosoft (R) Shared Source CLI IL Assembler.  Version " VER_FILEVERSION_STR);
		printf("\n%S\n\n", VER_LEGALCOPYRIGHT_DOS_STR);
    ErrorExit:
      printf("\n\nUsage: ilasm [Options] <sourcefile> [Options]");
      printf("\n\nOptions:");
      printf("\n/LISTING		Type a formatted list file");
      printf("\n/NOLOGO			Don't type the logo");
      printf("\n/QUIET			Don't report assembly progress");
      printf("\n/DLL			Compile to .dll");
      printf("\n/EXE			Compile to .exe (default)");
      printf("\n/DEBUG			Include debug information");
      printf("\n/CLOCK			Measure and report compilation times");
	  printf("\n/RESOURCE=<res_file>	Link the specified resource file (*.res) \n\t\t\tinto resulting .exe or .dll");
	  printf("\n/OUTPUT=<targetfile>	Compile to file with specified name \n\t\t\t(user must provide extension, if any)");
	  printf("\n/KEY=<keyfile>		Compile with strong signature \n\t\t\t(<keyfile> contains private key)");
	  printf("\n/SUBSYSTEM=<int>	Set Subsystem value in the NT Optional header");
	  printf("\n/FLAGS=<int>		Set CLR ImageFlags value in the CLR header");
	  printf("\n/ALIGNMENT=<int>	Set FileAlignment value in the NT Optional header");
	  printf("\n/BASE=<int>		Set ImageBase value in the NT Optional header");
      printf("\n\nKey may be '-' or '/'\nOptions are recognized by first 3 characters\nDefault source file extension is .IL\n\n");

      exit(1);
    }
	g_uCodePage = g_bOnUnicode ? CP_UTF8 : CP_ACP;
    if((pAsm = new Assembler()))
	{
        uCodePage = g_bOnUnicode ? CP_UTF8 : CP_ACP;
        pAsm->SetCodePage(uCodePage);
		if(pAsm->Init())
		{
			pAsm->SetStdMapping(1);
			//-------------------------------------------------
			for (i = 1; i < argc; i++)
			{
				if((argv[i][0] == L'/') || (argv[i][0] == L'-'))
				{
					memset(szOpt,0,sizeof(szOpt));
					WszWideCharToMultiByte(uCodePage,0,&argv[i][1],10,szOpt,sizeof(szOpt),NULL,NULL);
					szOpt[3] = 0;
					if (!_stricmp(szOpt,"NOA"))
					{
						pAsm->m_fAutoInheritFromObject = FALSE;
					}
				    else if (!_stricmp(szOpt,"QUI"))
					{
						pAsm->m_fReportProgress = FALSE;
						bLogo = FALSE;
					}
					else if (!_stricmp(szOpt, "NOL"))
					{ 
						bLogo = FALSE;
					}
					else if (!_stricmp(szOpt, "LIS"))
					{ 
					  pAsm->m_fGenerateListing = TRUE;
					}
					else if (!_stricmp(szOpt, "DEB"))
					{ 
					  pAsm->m_fIncludeDebugInfo = TRUE;
					  bNoDebug = FALSE;
					}
					else if (!_stricmp(szOpt, "CLO"))
					{ 
					  bClock = true;
                      pAsm->SetClock(&cw);
					}
					else if (!_stricmp(szOpt, "DLL"))
					{ 
					  IsDLL = true; IsOBJ = false;
					}
					else if (!_stricmp(szOpt, "OBJ"))
					{ 
					  IsOBJ = true; IsDLL = false;
					}
					else if (!_stricmp(szOpt, "ERR"))
					{ 
					  pAsm->OnErrGo = true;
					}
					else if (!_stricmp(szOpt, "EXE"))
					{
					  IsDLL = false;
					}
					else if (!_stricmp(szOpt, "RES"))
					{
						if(pAsm->m_wzResourceFile==NULL)
						{
							WCHAR *pStr = EqualOrColon(argv[i]);
							if(pStr == NULL) goto ErrorExit;
							for(pStr++; *pStr == L' '; pStr++); //skip the blanks
							if(wcslen(pStr)==0) goto ErrorExit; //if no file name
							pAsm->m_wzResourceFile = pStr;
						}
						else
							printf("Multiple resource files not allowed. Option %ls skipped\n",argv[i]);
					}
					else if (!_stricmp(szOpt, "KEY"))
					{
						WCHAR *pStr = EqualOrColon(argv[i]);
						if(pStr == NULL) goto ErrorExit;
						for(pStr++; *pStr == L' '; pStr++); //skip the blanks
						if(wcslen(pStr)==0) goto ErrorExit; //if no file name
						pAsm->m_wzKeySourceName = pStr;
					}
					else if (!_stricmp(szOpt, "OUT"))
					{
						WCHAR *pStr = EqualOrColon(argv[i]);
						if(pStr == NULL) goto ErrorExit;
						for(pStr++; *pStr == L' '; pStr++); //skip the blanks
						if(wcslen(pStr)==0) goto ErrorExit; //if no file name
						wcscpy(wzOutputFilename,pStr);
					}
					else if (!_stricmp(szOpt, "SUB"))
					{
						WCHAR *pStr = EqualOrColon(argv[i]);
						if(pStr == NULL) goto ErrorExit;
						swscanf(pStr+1,L"%ld",&g_dwSubsystem);
						if(!g_dwSubsystem) swscanf(pStr+1,L"%lx",&g_dwSubsystem);
					}
					else if (!_stricmp(szOpt, "ALI"))
					{
						WCHAR *pStr = EqualOrColon(argv[i]);
						if(pStr == NULL) goto ErrorExit;
						swscanf(pStr+1,L"%ld",&g_dwFileAlignment);
						if(!g_dwFileAlignment) swscanf(pStr+1,L"%lx",&g_dwFileAlignment);
					}
					else if (!_stricmp(szOpt, "FLA"))
					{
						WCHAR *pStr = EqualOrColon(argv[i]);
						if(pStr == NULL) goto ErrorExit;
						swscanf(pStr+1,L"%ld",&g_dwComImageFlags);
						if(!g_dwComImageFlags) swscanf(pStr+1,L"%lx",&g_dwComImageFlags);
					}
					else if (!_stricmp(szOpt, "BAS"))
					{
						WCHAR *pStr = EqualOrColon(argv[i]);
						if(pStr == NULL) goto ErrorExit;
						swscanf(pStr+1,L"%ld",&g_stBaseAddress);
						if(!g_stBaseAddress) swscanf(pStr+1,L"%lx",&g_stBaseAddress);
					}
					else
					{
#ifdef PLATFORM_UNIX
                        if (argv[i][0] == L'/')
                        {
                            // On BSD it could be a fully qualified file that starts with '/'
                            // Not a switch, so it might be a FileName
                            goto AssumeFileName;
                        }
                        else
#endif
                        {
						    fprintf(stderr, "Error : Invalid Option: %S", argv[i]);
						    goto ErrorExit;
                        }
					}
				}
				else
				{
#ifdef PLATFORM_UNIX
AssumeFileName:
#endif
					pwzInputFiles[NumFiles++] = argv[i];
					if(NumFiles == 1)
					{
						memset(szInputFilename,0,sizeof(szInputFilename));
						WszWideCharToMultiByte(uCodePage,0,argv[i],-1,szInputFilename,MAX_FILENAME_LENGTH*3-1,NULL,NULL);
						size_t j = strlen(szInputFilename);
						do
						{
							j--;
							if(szInputFilename[j] == '.') break;
							if((szInputFilename[j] == '\\')||(j == 0))
							{
								strcat(szInputFilename,".IL");
								break;
							}
						}
						while(j);
					}
				}
				
			}
			if(NumFiles == 0) goto ErrorExit;

			if(wzOutputFilename[0] == 0)
			{
				wcscpy(wzOutputFilename,pwzInputFiles[0]);
				size_t j = wcslen(wzOutputFilename);
				do
				{
					j--;
					if(wzOutputFilename[j] == L'.')
					{
						wzOutputFilename[j] = 0;
						break;
					}
				}
				while(j);
				wcscat(wzOutputFilename, (IsDLL ? L".dll" : (IsOBJ ? L".obj" : L".exe")));
			}

			//------------ Assembler initialization done. Now, to business -----------------------
            if((pParser = new AsmParse(NULL, pAsm)))
			{
				uCodePage = g_bOnUnicode ? CP_UTF8 : CP_ACP;
                pAsm->SetCodePage(uCodePage);
                pParser->SetOnUnicode(g_bOnUnicode);
				//======================================================================
				if(bLogo)
				{
					printf("\nMicrosoft (R) .NET Framework IL Assembler.  Version " VER_FILEVERSION_STR);
					printf("\n%S", VER_LEGALCOPYRIGHT_DOS_STR);
				}

				pAsm->SetDLL(IsDLL);
				pAsm->SetOBJ(IsOBJ);
				wcscpy(pAsm->m_wzOutputFileName,wzOutputFilename);
				strcpy(pAsm->m_szSourceFileName,szInputFilename);

				if (SUCCEEDED(pAsm->InitMetaData()))
				{
					int iFile;
					BOOL fAllFilesPresent = TRUE;
					if(bClock) cw.cParsBegin = GetTickCount();
					for(iFile = 0; iFile < NumFiles; iFile++)
					{
						uCodePage = g_bOnUnicode ? CP_UTF8 : CP_ACP;
                        pAsm->SetCodePage(uCodePage);
						if(iFile) // for the first file, it's already done
						{
							memset(szInputFilename,0,sizeof(szInputFilename));
							WszWideCharToMultiByte(uCodePage,0,pwzInputFiles[iFile],-1,szInputFilename,MAX_FILENAME_LENGTH*3-1,NULL,NULL);
							size_t j = strlen(szInputFilename);
							do
							{
								j--;
								if(szInputFilename[j] == '.') break;
								if((szInputFilename[j] == '\\')||(j == 0))
								{
									strcat(szInputFilename,".IL");
									break;
								}
							}
							while(j);
						}
                        if(pAsm->m_fReportProgress)
                        {
    						pParser->msg("\nAssembling '%s' ", szInputFilename);
    						if(pAsm->m_fCPlusPlus)	pParser->msg(" C++");
    						if(pAsm->m_fWindowsCE)	pParser->msg(" WINCE");
    						if(!pAsm->m_fAutoInheritFromObject)	pParser->msg(" NOAUTOINHERIT");
    						pParser->msg(pAsm->m_fGenerateListing ? ", with listing file," : ", no listing file,");
    						pParser->msg(IsDLL ? " to DLL" : (IsOBJ? " to OBJ" : " to EXE"));
    						//======================================================================
    						if (pAsm->m_fStdMapping == FALSE)
    							pParser->msg(", with REFERENCE mapping");
    
    						{
    							char szOutputFilename[MAX_FILENAME_LENGTH*3];
    							memset(szOutputFilename,0,sizeof(szOutputFilename));
    							WszWideCharToMultiByte(uCodePage,0,wzOutputFilename,-1,szOutputFilename,MAX_FILENAME_LENGTH*3-1,NULL,NULL);
    							pParser->msg(" --> '%s'\n", szOutputFilename);
    						}
                        }

						if(g_bOnUnicode)
						{
							WszMultiByteToWideChar(CP_UTF8,0,szInputFilename,-1,wzInputFilename,MAX_FILENAME_LENGTH);
							pIn = new FileReadStream(wzInputFilename);
						}
						else
							pIn = new FileReadStream(szInputFilename);
						if ((!pIn) || !(pIn->IsValid())) 
						{
							pParser->msg("Could not open %s\n", szInputFilename);
							fAllFilesPresent = FALSE;
						}
						else
						{
							{
								pAsm->SetSourceFileName(szInputFilename);
								
								pParser->ParseFile(pIn);
							}
						}
						if(pIn) delete pIn;
					} // end for(iFile)
					if(bClock) cw.cParsEnd = GetTickCount();
					if ((pParser->Success() && fAllFilesPresent) || pAsm->OnErrGo)
					{
						HRESULT hr;
                        if(g_dwSubsystem)		pAsm->m_dwSubsystem = g_dwSubsystem;
                        if(g_dwComImageFlags)	pAsm->m_dwComImageFlags = g_dwComImageFlags;
                        if(g_dwFileAlignment)	pAsm->m_dwFileAlignment = g_dwFileAlignment;
                        if(g_stBaseAddress)		pAsm->m_stBaseAddress = g_stBaseAddress;
						if(FAILED(hr=pAsm->CreatePEFile(wzOutputFilename))) 
							pParser->msg("Could not create output file, error code=0x%08X\n",hr);
						else
						{
							if(pParser->Success() && fAllFilesPresent) exitval = 0;
							else
							{
								pParser->msg("Output file contains errors\n");
								if(pAsm->OnErrGo) exitval = 0;
							}
							if(exitval == 0) // Write the output file
							{
								if(bClock) cw.cFilegenEnd = GetTickCount();
								if(pAsm->m_fReportProgress) pParser->msg("Writing %s file\n", pAsm->m_fOBJ ? "COFF" : "PE");
								// Generate the file
								if (FAILED(hr = pAsm->m_pCeeFileGen->GenerateCeeFile(pAsm->m_pCeeFile)))
								{
									exitval = 1;
									pParser->msg("Failed to write output file, error code=0x%08X\n",hr);
								}
                                else if (pAsm->m_pManifest->m_sStrongName.m_fFullSign)
                                {
                                    // Strong name sign the resultant assembly.
                                    if(pAsm->m_fReportProgress) pParser->msg("Signing file with strong name\n");
                                    if (FAILED(hr=pAsm->StrongNameSign()))
                                    {
                                        exitval = 1;
                                        pParser->msg("Failed to strong name sign output file, error code=0x%08X\n",hr);
                                    }
                                }
								if(bClock) cw.cEnd = GetTickCount();
							}

						}
					}
				}
				else pParser->msg("Failed to initialize Meta Data\n");
				delete pParser;
			}
			else printf("Could not create parser\n");
		}
		else printf("Failed to initialize Assembler\n");
		delete pAsm;
	}
	else printf("Insufficient memory\n");

	if(exitval || bNoDebug)
	{
		// PE file was not created, or no debug info required. Kill PDB if any
		WCHAR* pc = wcsrchr(wzOutputFilename,L'.');
		if(pc==NULL)
		{
			pc = &wzOutputFilename[wcslen(wzOutputFilename)];
			*pc = L'.';
		}
		wcscpy(pc+1,L"PDB");
#undef DeleteFileW
		DeleteFileW(wzOutputFilename);
	}
    if (exitval == 0)
	{
        if(pAsm->m_fReportProgress) printf("Operation completed successfully\n");
		if(bClock)
		{
			printf("Timing (msec): Total run                 %d\n",(cw.cEnd-cw.cBegin));
			printf("               Startup                   %d\n",(cw.cParsBegin-cw.cBegin));
			printf("               - MD initialization       %d\n",(cw.cMDInitEnd - cw.cMDInitBegin));
			printf("               Parsing                   %d\n",(cw.cParsEnd - cw.cParsBegin));
			printf("               Emitting MD               %d\n",(cw.cMDEmitEnd - cw.cRef2DefEnd)+(cw.cRef2DefBegin - cw.cMDEmitBegin));
			//printf("                - global fixups         %d\n",(cw.cMDEmit1 - cw.cMDEmitBegin));
			printf("                - SN sig alloc           %d\n",(cw.cMDEmit2 - cw.cMDEmitBegin));
			printf("                - Classes,Methods,Fields %d\n",(cw.cRef2DefBegin - cw.cMDEmit2));
			printf("                - Events,Properties      %d\n",(cw.cMDEmit3 - cw.cRef2DefEnd));
			printf("                - MethodImpls            %d\n",(cw.cMDEmit4 - cw.cMDEmit3));
			printf("                - Manifest,CAs           %d\n",(cw.cMDEmitEnd - cw.cMDEmit4));
			printf("               Ref to Def resolution     %d\n",(cw.cRef2DefEnd - cw.cRef2DefBegin));
			printf("               Fixup and linking         %d\n",(cw.cFilegenBegin - cw.cMDEmitEnd));
			printf("               CEE file generation       %d\n",(cw.cFilegenEnd - cw.cFilegenBegin));
			printf("               PE file writing           %d\n",(cw.cEnd - cw.cFilegenEnd));
		}
	}
    else
	{
        printf("\n***** FAILURE ***** \n");
	}
    exit(exitval);
    return exitval;
}
