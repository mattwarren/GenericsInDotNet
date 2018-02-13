/*
 * Created by Microsoft VCBU Internal YACC from "asmparse.y"
 */

#line 2 "asmparse.y"

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

#include <asmparse.h>
#include <crtdbg.h>             // FOR ASSERTE
#include <string.h>             // for strcmp    
#include <mbstring.h>           // for _mbsinc    
#include <ctype.h>                      // for isspace    
#include <stdlib.h>             // for strtoul
#include "openum.h"             // for CEE_*
#include <stdarg.h>         // for vararg macros 

#define YYMAXDEPTH 65535

// #define DEBUG_PARSING
#ifdef DEBUG_PARSING
bool parseDBFlag = true;
#define dbprintf(x)     if (parseDBFlag) printf x
#define YYDEBUG 1
#else
#define dbprintf(x)     
#endif

#define FAIL_UNLESS(cond, msg) if (!(cond)) { parser->success = false; parser->error msg; }

static AsmParse* parser = 0;
#define PASM    (parser->assem)
#define PASMM   (parser->assem->m_pManifest)

static char* newStringWDel(char* str1, char delimiter, char* str3 = 0);
static char* newString(char* str1);
static void corEmitInt(BinStr* buff, unsigned data);
bool bParsingByteArray = FALSE;
bool bExternSource = FALSE;
unsigned  nExtLine,nExtCol,nCurrPC,nExtLineEnd,nExtColEnd;
int iOpcodeLen = 0;

ARG_NAME_LIST *palDummyFirst,*palDummyLast;

int  nTemp=0;

unsigned int uMethodBeginLine,uMethodBeginColumn;

#line 50 "asmparse.y"

#define UNION 1
typedef union  {
        CorRegTypeAttr classAttr;
        CorMethodAttr methAttr;
        CorFieldAttr fieldAttr;
        CorMethodImpl implAttr;
        CorEventAttr  eventAttr;
        CorPropertyAttr propAttr;
        CorPinvokeMap pinvAttr;
        CorDeclSecurity secAct;
        CorFileFlags fileAttr;
        CorAssemblyFlags asmAttr;
        CorAssemblyFlags asmRefAttr;
        CorTypeAttr comtAttr;
        CorManifestResourceFlags manresAttr;
        double*  float64;
        __int64* int64;
        __int32  int32;
        char*    string;
        BinStr*  binstr;
        Labels*  labels;
        Instr*   instr;         // instruction opcode
        NVPair*  pair;
        pTyParList typarlist;
        mdToken token;
} YYSTYPE;
# define ERROR_ 257 
# define BAD_COMMENT_ 258 
# define BAD_LITERAL_ 259 
# define ID 260 
# define DOTTEDNAME 261 
# define QSTRING 262 
# define SQSTRING 263 
# define INT32 264 
# define INT64 265 
# define FLOAT64 266 
# define HEXBYTE 267 
# define DCOLON 268 
# define ELIPSES 269 
# define VOID_ 270 
# define BOOL_ 271 
# define CHAR_ 272 
# define UNSIGNED_ 273 
# define INT_ 274 
# define INT8_ 275 
# define INT16_ 276 
# define INT32_ 277 
# define INT64_ 278 
# define FLOAT_ 279 
# define FLOAT32_ 280 
# define FLOAT64_ 281 
# define BYTEARRAY_ 282 
# define UINT_ 283 
# define UINT8_ 284 
# define UINT16_ 285 
# define UINT32_ 286 
# define UINT64_ 287 
# define FLAGS_ 288 
# define CALLCONV_ 289 
# define OBJECT_ 290 
# define STRING_ 291 
# define NULLREF_ 292 
# define DEFAULT_ 293 
# define CDECL_ 294 
# define VARARG_ 295 
# define STDCALL_ 296 
# define THISCALL_ 297 
# define FASTCALL_ 298 
# define CLASS_ 299 
# define TYPEDREF_ 300 
# define UNMANAGED_ 301 
# define FINALLY_ 302 
# define HANDLER_ 303 
# define CATCH_ 304 
# define FILTER_ 305 
# define FAULT_ 306 
# define EXTENDS_ 307 
# define IMPLEMENTS_ 308 
# define TO_ 309 
# define AT_ 310 
# define TLS_ 311 
# define TRUE_ 312 
# define FALSE_ 313 
# define VALUE_ 314 
# define VALUETYPE_ 315 
# define NATIVE_ 316 
# define INSTANCE_ 317 
# define SPECIALNAME_ 318 
# define STATIC_ 319 
# define PUBLIC_ 320 
# define PRIVATE_ 321 
# define FAMILY_ 322 
# define FINAL_ 323 
# define SYNCHRONIZED_ 324 
# define INTERFACE_ 325 
# define SEALED_ 326 
# define NESTED_ 327 
# define ABSTRACT_ 328 
# define AUTO_ 329 
# define SEQUENTIAL_ 330 
# define EXPLICIT_ 331 
# define ANSI_ 332 
# define UNICODE_ 333 
# define AUTOCHAR_ 334 
# define IMPORT_ 335 
# define ENUM_ 336 
# define VIRTUAL_ 337 
# define NOINLINING_ 338 
# define UNMANAGEDEXP_ 339 
# define BEFOREFIELDINIT_ 340 
# define METHOD_ 341 
# define FIELD_ 342 
# define PINNED_ 343 
# define MODREQ_ 344 
# define MODOPT_ 345 
# define SERIALIZABLE_ 346 
# define ASSEMBLY_ 347 
# define FAMANDASSEM_ 348 
# define FAMORASSEM_ 349 
# define PRIVATESCOPE_ 350 
# define HIDEBYSIG_ 351 
# define NEWSLOT_ 352 
# define RTSPECIALNAME_ 353 
# define PINVOKEIMPL_ 354 
# define _CTOR 355 
# define _CCTOR 356 
# define LITERAL_ 357 
# define NOTSERIALIZED_ 358 
# define INITONLY_ 359 
# define REQSECOBJ_ 360 
# define CIL_ 361 
# define OPTIL_ 362 
# define MANAGED_ 363 
# define FORWARDREF_ 364 
# define PRESERVESIG_ 365 
# define RUNTIME_ 366 
# define INTERNALCALL_ 367 
# define _IMPORT 368 
# define NOMANGLE_ 369 
# define LASTERR_ 370 
# define WINAPI_ 371 
# define AS_ 372 
# define INSTR_NONE 373 
# define INSTR_VAR 374 
# define INSTR_I 375 
# define INSTR_I8 376 
# define INSTR_R 377 
# define INSTR_BRTARGET 378 
# define INSTR_METHOD 379 
# define INSTR_FIELD 380 
# define INSTR_TYPE 381 
# define INSTR_STRING 382 
# define INSTR_SIG 383 
# define INSTR_RVA 384 
# define INSTR_TOK 385 
# define INSTR_SWITCH 386 
# define INSTR_PHI 387 
# define _CLASS 388 
# define _NAMESPACE 389 
# define _METHOD 390 
# define _FIELD 391 
# define _DATA 392 
# define _EMITBYTE 393 
# define _TRY 394 
# define _MAXSTACK 395 
# define _LOCALS 396 
# define _ENTRYPOINT 397 
# define _ZEROINIT 398 
# define _EVENT 399 
# define _ADDON 400 
# define _REMOVEON 401 
# define _FIRE 402 
# define _OTHER 403 
# define _PROPERTY 404 
# define _SET 405 
# define _GET 406 
# define _PERMISSION 407 
# define _PERMISSIONSET 408 
# define REQUEST_ 409 
# define DEMAND_ 410 
# define ASSERT_ 411 
# define DENY_ 412 
# define PERMITONLY_ 413 
# define LINKCHECK_ 414 
# define INHERITCHECK_ 415 
# define REQMIN_ 416 
# define REQOPT_ 417 
# define REQREFUSE_ 418 
# define PREJITGRANT_ 419 
# define PREJITDENY_ 420 
# define NONCASDEMAND_ 421 
# define NONCASLINKDEMAND_ 422 
# define NONCASINHERITANCE_ 423 
# define _LINE 424 
# define P_LINE 425 
# define _LANGUAGE 426 
# define _CUSTOM 427 
# define INIT_ 428 
# define _SIZE 429 
# define _PACK 430 
# define _VTABLE 431 
# define _VTFIXUP 432 
# define FROMUNMANAGED_ 433 
# define CALLMOSTDERIVED_ 434 
# define _VTENTRY 435 
# define _FILE 436 
# define NOMETADATA_ 437 
# define _HASH 438 
# define _ASSEMBLY 439 
# define _PUBLICKEY 440 
# define _PUBLICKEYTOKEN 441 
# define ALGORITHM_ 442 
# define _VER 443 
# define _LOCALE 444 
# define EXTERN_ 445 
# define _MRESOURCE 446 
# define NOAPPDOMAIN_ 447 
# define NOPROCESS_ 448 
# define NOMACHINE_ 449 
# define _MODULE 450 
# define _EXPORT 451 
# define MARSHAL_ 452 
# define CUSTOM_ 453 
# define SYSSTRING_ 454 
# define FIXED_ 455 
# define VARIANT_ 456 
# define CURRENCY_ 457 
# define SYSCHAR_ 458 
# define DECIMAL_ 459 
# define DATE_ 460 
# define BSTR_ 461 
# define TBSTR_ 462 
# define LPSTR_ 463 
# define LPWSTR_ 464 
# define LPTSTR_ 465 
# define OBJECTREF_ 466 
# define IUNKNOWN_ 467 
# define IDISPATCH_ 468 
# define STRUCT_ 469 
# define SAFEARRAY_ 470 
# define BYVALSTR_ 471 
# define LPVOID_ 472 
# define ANY_ 473 
# define ARRAY_ 474 
# define LPSTRUCT_ 475 
# define IN_ 476 
# define OUT_ 477 
# define OPT_ 478 
# define _PARAM 479 
# define _OVERRIDE 480 
# define WITH_ 481 
# define NULL_ 482 
# define HRESULT_ 483 
# define CARRAY_ 484 
# define USERDEFINED_ 485 
# define RECORD_ 486 
# define FILETIME_ 487 
# define BLOB_ 488 
# define STREAM_ 489 
# define STORAGE_ 490 
# define STREAMED_OBJECT_ 491 
# define STORED_OBJECT_ 492 
# define BLOB_OBJECT_ 493 
# define CF_ 494 
# define CLSID_ 495 
# define VECTOR_ 496 
# define _SUBSYSTEM 497 
# define _CORFLAGS 498 
# define ALIGNMENT_ 499 
# define _IMAGEBASE 500 
#define yyclearin yychar = -1
#define yyerrok yyerrflag = 0
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yylval, yyval;
#ifndef YYFARDATA
#define	YYFARDATA	/*nothing*/
#endif
#if ! defined YYSTATIC
#define	YYSTATIC	/*nothing*/
#endif
#if ! defined YYCONST
#define	YYCONST	/*nothing*/
#endif
#ifndef	YYACT
#define	YYACT	yyact
#endif
#ifndef	YYPACT
#define	YYPACT	yypact
#endif
#ifndef	YYPGO
#define	YYPGO	yypgo
#endif
#ifndef	YYR1
#define	YYR1	yyr1
#endif
#ifndef	YYR2
#define	YYR2	yyr2
#endif
#ifndef	YYCHK
#define	YYCHK	yychk
#endif
#ifndef	YYDEF
#define	YYDEF	yydef
#endif
#ifndef	YYV
#define	YYV	yyv
#endif
#ifndef	YYS
#define	YYS	yys
#endif
#ifndef	YYLOCAL
#define	YYLOCAL
#endif
#ifndef YYR_T
#define	YYR_T	int
#endif
typedef	YYR_T	yyr_t;
#ifndef YYEXIND_T
#define	YYEXIND_T	unsigned int
#endif
typedef	YYEXIND_T	yyexind_t;
#ifndef YYOPTTIME
#define	YYOPTTIME	0
#endif
# define YYERRCODE 256

#line 1503 "asmparse.y"

/********************************************************************************/
/* Code goes here */

/********************************************************************************/

void yyerror(char* str) {
    char tokBuff[64];
    char *ptr;
    size_t len = parser->curPos - parser->curTok;
    if (len > 63) len = 63;
    memcpy(tokBuff, parser->curTok, len);
    tokBuff[len] = 0;
    fprintf(stderr, "%s(%d) : error : %s at token '%s' in: %s\n", 
            parser->in->name(), parser->curLine, str, tokBuff, (ptr=parser->getLine(parser->curLine)));
    parser->success = false;
    delete ptr;
}

struct Keywords {
    const char* name;
    unsigned short token;
    unsigned short tokenVal;// this holds the instruction enumeration for those keywords that are instrs
};

#define NO_VALUE        ((unsigned short)-1)              // The token has no value

static Keywords keywords[] = {
// Attention! Because of aliases, the instructions MUST go first!
// Redefine all the instructions (defined in assembler.h <- asmenum.h <- opcode.def)
#undef InlineNone
#undef InlineVar        
#undef ShortInlineVar
#undef InlineI          
#undef ShortInlineI     
#undef InlineI8         
#undef InlineR          
#undef ShortInlineR     
#undef InlineBrTarget
#undef ShortInlineBrTarget
#undef InlineMethod
#undef InlineField 
#undef InlineType 
#undef InlineString
#undef InlineSig        
#undef InlineRVA        
#undef InlineTok        
#undef InlineSwitch
#undef InlinePhi        
#undef InlineVarTok     


#define InlineNone              INSTR_NONE
#define InlineVar               INSTR_VAR
#define ShortInlineVar          INSTR_VAR
#define InlineI                 INSTR_I
#define ShortInlineI            INSTR_I
#define InlineI8                INSTR_I8
#define InlineR                 INSTR_R
#define ShortInlineR            INSTR_R
#define InlineBrTarget          INSTR_BRTARGET
#define ShortInlineBrTarget             INSTR_BRTARGET
#define InlineMethod            INSTR_METHOD
#define InlineField             INSTR_FIELD
#define InlineType              INSTR_TYPE
#define InlineString            INSTR_STRING
#define InlineSig               INSTR_SIG
#define InlineRVA               INSTR_RVA
#define InlineTok               INSTR_TOK
#define InlineSwitch            INSTR_SWITCH
#define InlinePhi               INSTR_PHI

#define InlineVarTok            0
#define NEW_INLINE_NAMES
                // The volatile instruction collides with the volatile keyword, so 
                // we treat it as a keyword everywhere and modify the grammar accordingly (Yuck!) 
#define OPDEF(c,s,pop,push,args,type,l,s1,s2,ctrl) { s, args, c},
#define OPALIAS(alias_c, s, c) { s, NO_VALUE, c},
#include "opcode.def"
#undef OPALIAS
#undef OPDEF

                /* keywords */
#define KYWD(name, sym, val)    { name, sym, val},
#include "il_kywd.h"
#undef KYWD

};

/********************************************************************************/
/* used by qsort to sort the keyword table */
static int __cdecl keywordCmp(const void *op1, const void *op2)
{
    return  strcmp(((Keywords*) op1)->name, ((Keywords*) op2)->name);
}

/********************************************************************************/
/* looks up the keyword 'name' of length 'nameLen' (name does not need to be 
   null terminated)   Returns 0 on failure */

int findKeyword(const char* name, size_t nameLen, Instr** value) 
{
    Keywords* low = keywords;
    Keywords* high = &keywords[sizeof(keywords) / sizeof(Keywords)];

    _ASSERTE (high > low);          // Table is non-empty
    for(;;) 
    {
        Keywords* mid = &low[(high - low) >> 1];

                // compare the strings
        int cmp = strncmp(name, mid->name, nameLen);
        if ((cmp == 0) && (nameLen < strlen(mid->name))) --cmp;
        if (cmp == 0)
        {
            //printf("Token '%s' = %d opcode = %d\n", mid->name, mid->token, mid->tokenVal);
            if (mid->tokenVal != NO_VALUE)
            {
                *value = PASM->GetInstr();
                if(*value)
				{
					(*value)->opcode = mid->tokenVal;
                    (*value)->pWriter = PASM->m_pSymDocument;
                    if(bExternSource)
                    {
					    (*value)->linenum = nExtLine;
					    (*value)->column = nExtCol;
   					    (*value)->linenum_end = nExtLineEnd;
					    (*value)->column_end = nExtColEnd;
                        (*value)->pc = nCurrPC;
                    }
                    else
                    {
					    (*value)->linenum = parser->curLine;
					    (*value)->column = 1;
   					    (*value)->linenum_end = parser->curLine;
					    (*value)->column_end = 1;
                        (*value)->pc = PASM->m_CurPC;
                    }
				}
            }
            else *value = NULL;

            return(mid->token);
        }

        if (mid == low)  return(0);

        if (cmp > 0) low = mid;
        else        high = mid;
    }
}

/********************************************************************************/
/* convert str to a uint64 */

static unsigned __int64 str2uint64(const char* str, const char** endStr, unsigned radix) 
{
    static unsigned digits[256];
    static BOOL initialize=TRUE;
    unsigned __int64 ret = 0;
    unsigned digit;
    _ASSERTE(radix <= 36);
    if(initialize)
    {
        int i;
        memset(digits,255,sizeof(digits));
        for(i='0'; i <= '9'; i++) digits[i] = i - '0';
        for(i='A'; i <= 'Z'; i++) digits[i] = i + 10 - 'A';
        for(i='a'; i <= 'z'; i++) digits[i] = i + 10 - 'a';
        initialize = FALSE;
    }
    for(;;str++) 
    {
        digit = digits[*str];
        if (digit >= radix) 
        {
            *endStr = str;
            return(ret);
        }
        ret = ret * radix + digit;
    }
}

/********************************************************************************/
/* fetch the next token, and return it   Also set the yylval.union if the
   lexical token also has a value */

#define IsValidStartingSymbol(x) (isalpha((x)&0xFF)||((x)=='#')||((x)=='_')||((x)=='@')||((x)=='$'))
#define IsValidContinuingSymbol(x) (isalnum((x)&0xFF)||((x)=='_')||((x)=='@')||((x)=='$')||((x)=='?'))
char* nextchar(char* pos)
{
	return (g_uCodePage == CP_ACP) ? (char *)_mbsinc((const unsigned char *)pos) : ++pos;
}
int yylex() 
{
    char* curPos = parser->curPos;

        // Skip any leading whitespace and comments
    const unsigned eolComment = 1;
    const unsigned multiComment = 2;
    unsigned state = 0;
    for(;;) 
    {   // skip whitespace and comments
        if (curPos >= parser->limit) 
        {
            curPos = parser->fillBuff(curPos);
			if(strlen(curPos) < (unsigned)(parser->endPos - curPos))
			{
				yyerror("Not a text file");
				return 0;
			}
        }
        
        switch(*curPos) 
        {
            case 0: 
                if (state & multiComment) return (BAD_COMMENT_);
                return 0;       // EOF
            case '\n':
                state &= ~eolComment;
                parser->curLine++;
                PASM->m_ulCurLine = (bExternSource ? nExtLine : parser->curLine);
                PASM->m_ulCurColumn = (bExternSource ? nExtCol : 1);
                break;
            case '\r':
            case ' ' :
            case '\t':
            case '\f':
                break;

            case '*' :
                if(state == 0) goto PAST_WHITESPACE;
                if(state & multiComment)
                {
                    if (curPos[1] == '/') 
                    {
                        curPos++;
                        state &= ~multiComment;
                    }
                }
                break;

            case '/' :
                if(state == 0)
                {
                    if (curPos[1] == '/')  state |= eolComment;
                    else if (curPos[1] == '*') 
                    {
                        curPos++;
                        state |= multiComment;
                    }
                    else goto PAST_WHITESPACE;
                }
                break;

            default:
                if (state == 0)  goto PAST_WHITESPACE;
        }
        //curPos++;
		curPos = nextchar(curPos);
    }
PAST_WHITESPACE:

    char* curTok = curPos;
    parser->curTok = curPos;
    parser->curPos = curPos;
    int tok = ERROR_;
    yylval.string = 0;

    if(bParsingByteArray) // only hexadecimals w/o 0x, ')' and white space allowed!
    {
        int i,s=0;
        char ch;
        for(i=0; i<2; i++, curPos++)
        {
            ch = *curPos;
            if(('0' <= ch)&&(ch <= '9')) s = s*16+(ch - '0');
            else if(('A' <= ch)&&(ch <= 'F')) s = s*16+(ch - 'A' + 10);
            else if(('a' <= ch)&&(ch <= 'f')) s = s*16+(ch - 'a' + 10);
            else break; // don't increase curPos!
        }
        if(i)
        {
            tok = HEXBYTE;
            yylval.int32 = s;
        }
        else
        {
            if(ch == ')') 
            {
                bParsingByteArray = FALSE;
                goto Just_A_Character;
            }
        }
        parser->curPos = curPos;
        return(tok);
    }
    if(*curPos == '?') // '?' may be part of an identifier, if it's not followed by punctuation
    {
		if(IsValidContinuingSymbol(*(curPos+1))) goto Its_An_Id;
        goto Just_A_Character;
    }

    if (IsValidStartingSymbol(*curPos)) 
    { // is it an ID
Its_An_Id:
        size_t offsetDot = (size_t)-1; // first appearance of '.'
		size_t offsetDotDigit = (size_t)-1; // first appearance of '.<digit>' (not DOTTEDNAME!)
        do 
        {
            if (curPos >= parser->limit) 
            {
                size_t offsetInStr = curPos - curTok;
                curTok = parser->fillBuff(curTok);
                curPos = curTok + offsetInStr;
            }
            //curPos++;
			curPos = nextchar(curPos);
            if (*curPos == '.') 
            {
                if (offsetDot == (size_t)-1) offsetDot = curPos - curTok;
                curPos++;
				if((offsetDotDigit==(size_t)-1)&&(*curPos >= '0')&&(*curPos <= '9')) 
					offsetDotDigit = curPos - curTok - 1;
            }
        } while(IsValidContinuingSymbol(*curPos));
        size_t tokLen = curPos - curTok;

        // check to see if it is a keyword
        int token = findKeyword(curTok, tokLen, &yylval.instr);
        if (token != 0) 
        {
            //printf("yylex: TOK = %d, curPos=0x%8.8X\n",token,curPos);
            parser->curPos = curPos;
            parser->curTok = curTok;
            return(token);
        }
        if(*curTok == '#') 
        {
            parser->curPos = curPos;
            parser->curTok = curTok;
            return(ERROR_);
        }
        // Not a keyword, normal identifiers don't have '.' in them
        if (offsetDot < (size_t)-1) 
        {
			if(offsetDotDigit < (size_t)-1)
			{
				curPos = curTok+offsetDotDigit;
				tokLen = offsetDotDigit;
			}
			while((*(curPos-1)=='.')&&(tokLen))
			{
				curPos--;
				tokLen--;
			}
        }

        if((yylval.string = new char[tokLen+1]))
		{
			memcpy(yylval.string, curTok, tokLen);
			yylval.string[tokLen] = 0;
			tok = (offsetDot == (size_t)(-1))? ID : DOTTEDNAME;
			//printf("yylex: ID = '%s', curPos=0x%8.8X\n",yylval.string,curPos);
		}
		else return BAD_LITERAL_;
    }
    else if (isdigit((*curPos)&0xFF) 
        || (*curPos == '.' && isdigit(curPos[1]&0xFF))
        || (*curPos == '-' && isdigit(curPos[1]&0xFF))) 
        {
        // Refill buffer, we may be close to the end, and the number may be only partially inside
        if(parser->endPos - curPos < AsmParse::IN_OVERLAP)
        {
            curTok = parser->fillBuff(curPos);
            curPos = curTok;
        }
        const char* begNum = curPos;
        unsigned radix = 10;

        bool neg = (*curPos == '-');    // always make it unsigned 
        if (neg) curPos++;

        if (curPos[0] == '0' && curPos[1] != '.') 
        {
            curPos++;
            radix = 8;
            if (*curPos == 'x' || *curPos == 'X') 
            {
                curPos++;
                radix = 16;
            }
        }
        begNum = curPos;
        {
            unsigned __int64 i64 = str2uint64(begNum, const_cast<const char**>(&curPos), radix);
            yylval.int64 = new __int64(i64);
            tok = INT64;                    
            if (neg) *yylval.int64 = -*yylval.int64;
        }
        if (radix == 10 && ((*curPos == '.' && curPos[1] != '.') || *curPos == 'E' || *curPos == 'e')) 
        {
            yylval.float64 = new double(strtod(begNum, &curPos));
            if (neg) *yylval.float64 = -*yylval.float64;
            tok = FLOAT64;
        }
    }
    else 
    {   //      punctuation
        if (*curPos == '"' || *curPos == '\'') 
        {
            //char quote = *curPos++;
            char quote = *curPos;
			curPos = nextchar(curPos);
            char* fromPtr = curPos;
			char* prevPos;
            bool escape = false;
            BinStr* pBuf = new BinStr(); 
            for(;;) 
            {     // Find matching quote
                if (curPos >= parser->limit)
                { 
                    curTok = parser->fillBuff(curPos);
                    curPos = curTok;
                }
                
                if (*curPos == 0) { parser->curPos = curPos; delete pBuf; return(BAD_LITERAL_); }
                if (*curPos == '\r') curPos++;  //for end-of-line \r\n
                if (*curPos == '\n') 
                {
                    parser->curLine++;
                    PASM->m_ulCurLine = (bExternSource ? nExtLine : parser->curLine);
                    PASM->m_ulCurColumn = (bExternSource ? nExtCol : 1);
                    if (!escape) { parser->curPos = curPos; delete pBuf; return(BAD_LITERAL_); }
                }
                if ((*curPos == quote) && (!escape)) break;
                escape =(!escape) && (*curPos == '\\');
                //pBuf->appendInt8(*curPos++);
				prevPos = curPos;
				curPos = nextchar(curPos);
                while(prevPos < curPos) pBuf->appendInt8(*prevPos++);
            }
            //curPos++;               // skip closing quote
			curPos = nextchar(curPos);
                                
            // translate escaped characters
            unsigned tokLen = pBuf->length();
            char* toPtr = new char[tokLen+1];
			if(toPtr==NULL) return BAD_LITERAL_;
            yylval.string = toPtr;
            fromPtr = (char *)(pBuf->ptr());
            char* endPtr = fromPtr+tokLen;
            while(fromPtr < endPtr) 
            {
                if (*fromPtr == '\\') 
                {
                    fromPtr++;
                    switch(*fromPtr) 
                    {
                        case 't':
                                *toPtr++ = '\t';
                                break;
                        case 'n':
                                *toPtr++ = '\n';
                                break;
                        case 'b':
                                *toPtr++ = '\b';
                                break;
                        case 'f':
                                *toPtr++ = '\f';
                                break;
                        case 'v':
                                *toPtr++ = '\v';
                                break;
                        case '?':
                                *toPtr++ = '\?';
                                break;
                        case 'r':
                                *toPtr++ = '\r';
                                break;
                        case 'a':
                                *toPtr++ = '\a';
                                break;
                        case '\n':
                                do      fromPtr++;
                                while(isspace(*fromPtr));
                                --fromPtr;              // undo the increment below   
                                break;
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                                if (isdigit(fromPtr[1]&0xFF) && isdigit(fromPtr[2]&0xFF)) 
                                {
                                    *toPtr++ = ((fromPtr[0] - '0') * 8 + (fromPtr[1] - '0')) * 8 + (fromPtr[2] - '0');
                                    fromPtr+= 2;                                                            
                                }
                                else if(*fromPtr == '0') *toPtr++ = 0;
                                break;
                        default:
                                *toPtr++ = *fromPtr;
                    }
                    fromPtr++;
                }
                else
				//  *toPtr++ = *fromPtr++;
				{
					char* tmpPtr = fromPtr;
					fromPtr = nextchar(fromPtr);
					while(tmpPtr < fromPtr) *toPtr++ = *tmpPtr++;
				}

            } //end while(fromPtr < endPtr)
            *toPtr = 0;                     // terminate string
            if(quote == '"')
            {
                BinStr* pBS = new BinStr();
                unsigned size = (unsigned)(toPtr - yylval.string);
                memcpy(pBS->getBuff(size),yylval.string,size);
                delete yylval.string;
                yylval.binstr = pBS;
                tok = QSTRING;
            }
            else tok = SQSTRING;
            delete pBuf;
        } // end if (*curPos == '"' || *curPos == '\'')
        else if (strncmp(curPos, "::", 2) == 0) 
        {
            curPos += 2;
            tok = DCOLON;
        }       
        else if (strncmp(curPos, "...", 3) == 0) 
        {
            curPos += 3;
            tok = ELIPSES;
        }
        else if(*curPos == '.') 
        {
            do
            {
                curPos++;
                if (curPos >= parser->limit) 
                {
                    size_t offsetInStr = curPos - curTok;
                    curTok = parser->fillBuff(curTok);
                    curPos = curTok + offsetInStr;
                }
            }
            while(isalnum(*curPos) || *curPos == '_' || *curPos == '$'|| *curPos == '@'|| *curPos == '?');
            size_t tokLen = curPos - curTok;

            // check to see if it is a keyword
            int token = findKeyword(curTok, tokLen, &yylval.instr);
            if(token)
			{
                //printf("yylex: TOK = %d, curPos=0x%8.8X\n",token,curPos);
                parser->curPos = curPos;
                parser->curTok = curTok; 
                return(token);
            }
            tok = '.';
            curPos = curTok + 1;
        }
        else 
        {
Just_A_Character:
            tok = *curPos++;
        }
        //printf("yylex: PUNCT curPos=0x%8.8X\n",curPos);
    }
    dbprintf(("    Line %d token %d (%c) val = %s\n", parser->curLine, tok, 
            (tok < 128 && isprint(tok)) ? tok : ' ', 
            (tok > 255 && tok != INT32 && tok != INT64 && tok!= FLOAT64) ? yylval.string : ""));

    parser->curPos = curPos;
    parser->curTok = curTok; 
    return(tok);
}

/**************************************************************************/
static char* newString(char* str1) 
{
    char* ret = new char[strlen(str1)+1];
    if(ret) strcpy(ret, str1);
    return(ret);
}

/**************************************************************************/
/* concatenate strings and release them */

static char* newStringWDel(char* str1, char delimiter, char* str3) 
{
    size_t len1 = strlen(str1);
    size_t len = len1+2;
    if (str3) len += strlen(str3);
    char* ret = new char[len];
    if(ret)
	{
		strcpy(ret, str1);
		delete [] str1;
        ret[len1] = delimiter;
        ret[len1+1] = 0;
		if (str3)
		{
			strcat(ret, str3);
			delete [] str3;
		}
	}
    return(ret);
}

/**************************************************************************/
static void corEmitInt(BinStr* buff, unsigned data) 
{
    unsigned cnt = CorSigCompressData(data, buff->getBuff(5));
    buff->remove(5 - cnt);
}

/**************************************************************************/
/* move 'ptr past the exactly one type description */

static unsigned __int8* skipType(unsigned __int8* ptr) 
{
    mdToken  tk;
AGAIN:
    switch(*ptr++) {
        case ELEMENT_TYPE_VOID         :
        case ELEMENT_TYPE_BOOLEAN      :
        case ELEMENT_TYPE_CHAR         :
        case ELEMENT_TYPE_I1           :
        case ELEMENT_TYPE_U1           :
        case ELEMENT_TYPE_I2           :
        case ELEMENT_TYPE_U2           :
        case ELEMENT_TYPE_I4           :
        case ELEMENT_TYPE_U4           :
        case ELEMENT_TYPE_I8           :
        case ELEMENT_TYPE_U8           :
        case ELEMENT_TYPE_R4           :
        case ELEMENT_TYPE_R8           :
        case ELEMENT_TYPE_U            :
        case ELEMENT_TYPE_I            :
        case ELEMENT_TYPE_R            :
        case ELEMENT_TYPE_STRING       :
        case ELEMENT_TYPE_OBJECT       :
        case ELEMENT_TYPE_TYPEDBYREF   :
                /* do nothing */
                break;

        case ELEMENT_TYPE_VALUETYPE   :
        case ELEMENT_TYPE_CLASS        :
                ptr += CorSigUncompressToken(ptr, &tk);
                break;

        case ELEMENT_TYPE_CMOD_REQD    :
        case ELEMENT_TYPE_CMOD_OPT     :
                ptr += CorSigUncompressToken(ptr, &tk);
                goto AGAIN;

		/*                                                                   
		        */

        case ELEMENT_TYPE_ARRAY         :
                {
                    ptr = skipType(ptr);                    // element Type
                    unsigned rank = CorSigUncompressData((PCCOR_SIGNATURE&) ptr);
                    if (rank != 0)
                    {
                        unsigned numSizes = CorSigUncompressData((PCCOR_SIGNATURE&) ptr);
                        while(numSizes > 0)
						{
                            CorSigUncompressData((PCCOR_SIGNATURE&) ptr);
							--numSizes;
						}
                        unsigned numLowBounds = CorSigUncompressData((PCCOR_SIGNATURE&) ptr);
                        while(numLowBounds > 0)
						{
                            CorSigUncompressData((PCCOR_SIGNATURE&) ptr);
							--numLowBounds;
						}
                    }
                }
                break;

                // Modifiers or depedant types
        case ELEMENT_TYPE_PINNED                :
        case ELEMENT_TYPE_PTR                   :
        case ELEMENT_TYPE_BYREF                 :
        case ELEMENT_TYPE_SZARRAY               :
                // tail recursion optimization
                // ptr = skipType(ptr);
                // break
                goto AGAIN;

        case ELEMENT_TYPE_VAR:
        case ELEMENT_TYPE_MVAR:
                CorSigUncompressData((PCCOR_SIGNATURE&) ptr);  // bound
                break;

        case ELEMENT_TYPE_FNPTR: 
                {
                    CorSigUncompressData((PCCOR_SIGNATURE&) ptr);    // calling convention
                    unsigned argCnt = CorSigUncompressData((PCCOR_SIGNATURE&) ptr);    // arg count
                    ptr = skipType(ptr);                             // return type
                    while(argCnt > 0)
                    {
                        ptr = skipType(ptr);
                        --argCnt;
                    }
                }
                break;

        case ELEMENT_TYPE_WITH: 
               {
			       ptr = skipType(ptr);			// type constructor
			       unsigned argCnt = CorSigUncompressData((PCCOR_SIGNATURE&) ptr);		// arg count
			       while(argCnt > 0) {
				       ptr = skipType(ptr);
				       --argCnt;
				   }
			   }
			   break;                        

        default:
        case ELEMENT_TYPE_SENTINEL              :
        case ELEMENT_TYPE_END                   :
                _ASSERTE(!"Unknown Type");
                break;
    }
    return(ptr);
}

/**************************************************************************/
static unsigned corCountArgs(BinStr* args) 
{
    unsigned __int8* ptr = args->ptr();
    unsigned __int8* end = &args->ptr()[args->length()];
    unsigned ret = 0;
    while(ptr < end) 
        {
        if (*ptr != ELEMENT_TYPE_SENTINEL) 
                {
            ptr = skipType(ptr);
            ret++;
        }
        else ptr++;
    }
    return(ret);
}

/********************************************************************************/
AsmParse::AsmParse(ReadStream* aIn, Assembler *aAssem) 
{
#ifdef DEBUG_PARSING
    extern int yydebug;
    yydebug = 1;
#endif

    in = aIn;
    assem = aAssem;
    assem->SetErrorReporter((ErrorReporter *)this);

    char* buffBase = new char[IN_READ_SIZE+IN_OVERLAP+1];                // +1 for null termination
    _ASSERTE(buffBase);
	if(buffBase)
	{
		curTok = curPos = endPos = limit = buff = &buffBase[IN_OVERLAP];     // Offset it 
		curLine = 1;
		assem->m_ulCurLine = curLine;
		assem->m_ulCurColumn = 1;
        m_bOnUnicode = TRUE;

		hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
		hstderr = GetStdHandle(STD_ERROR_HANDLE);

		success = true; 
		_ASSERTE(parser == 0);          // Should only be one parser instance at a time
    
                // Resolve aliases
                for (unsigned int i = 0; i < sizeof(keywords) / sizeof(Keywords); i++)
                {
                        if (keywords[i].token == NO_VALUE)
                        keywords[i].token = keywords[keywords[i].tokenVal].token;
                }

                // Sort the keywords for fast lookup 
                qsort(keywords, sizeof(keywords) / sizeof(Keywords), sizeof(Keywords), keywordCmp);
                parser = this;
                //yyparse();
	}
	else
	{
		assem->report->error("Failed to allocate parsing buffer\n");
		delete this;
	}
}

/********************************************************************************/
AsmParse::~AsmParse() 
{
    parser = 0;
    delete [] &buff[-IN_OVERLAP];
}

/**************************************************************************/
DWORD AsmParse::IsItUnicode(CONST LPVOID pBuff, int cb, LPINT lpi)
{
	if(*((WORD*)pBuff) == 0xFEFF)
	{
		if(lpi) *lpi = IS_TEXT_UNICODE_SIGNATURE;
		return 1;
	}
	return 0;
}
/**************************************************************************/
char* AsmParse::fillBuff(char* pos) 
{
	static bool bUnicode = false;
    int iRead,iPutToBuffer,iOrdered;
	static char* readbuff = buff;

    _ASSERTE((buff-IN_OVERLAP) <= pos && pos <= &buff[IN_READ_SIZE]);
    curPos = pos;
    size_t tail = endPos - curPos; // endPos points just past the end of valid data in the buffer
    _ASSERTE(tail <= IN_OVERLAP);
    if(tail) memcpy(buff-tail, curPos, tail);    // Copy any stuff to the begining 
    curPos = buff-tail;
	iOrdered = m_iReadSize;
	if(m_bFirstRead)
	{
		int iOptions = IS_TEXT_UNICODE_UNICODE_MASK;
		m_bFirstRead = false;
		g_uCodePage = CP_ACP;
		if(bUnicode) // leftover fron previous source file
		{
			delete [] readbuff;
			readbuff = buff;
		}
		bUnicode = false;

	    iRead = in->read(readbuff, iOrdered);

		if(IsItUnicode(buff,iRead,&iOptions))
		{
			bUnicode = true;
			g_uCodePage = CP_UTF8;
            if((readbuff = new char[iOrdered+2])) // buffer for reading Unicode chars
			{
				if(iOptions & IS_TEXT_UNICODE_SIGNATURE)
					memcpy(readbuff,buff+2,iRead-2);   // only first time, next time it will be read into new buffer
				else
					memcpy(readbuff,buff,iRead);   // only first time, next time it will be read into new buffer
				if(assem->m_fReportProgress) printf("Source file is UNICODE\n\n");
			}
			else
				assem->report->error("Failed to allocate read buffer\n");
		}
		else
		{
			m_iReadSize = IN_READ_SIZE;
			if(((buff[0]&0xFF)==0xEF)&&((buff[1]&0xFF)==0xBB)&&((buff[2]&0xFF)==0xBF))
			{
				g_uCodePage = CP_UTF8;
				curPos += 3;
				if(assem->m_fReportProgress) printf("Source file is UTF-8\n\n");
			}
			else
				if(assem->m_fReportProgress) printf("Source file is ANSI\n\n");
		}
	}
	else  iRead = in->read(readbuff, iOrdered);

	if(bUnicode)
	{
		WCHAR* pwc = (WCHAR*)readbuff;
		pwc[iRead/2] = 0;
		memset(buff,0,IN_READ_SIZE);
		WszWideCharToMultiByte(CP_UTF8,0,pwc,-1,(LPSTR)buff,IN_READ_SIZE,NULL,NULL);
		iPutToBuffer = (int)strlen(buff);
	}
	else iPutToBuffer = iRead;

    endPos = buff + iPutToBuffer;
    *endPos = 0;                        // null Terminate the buffer

    limit = endPos; // endPos points just past the end of valid data in the buffer
    if (iRead == iOrdered) 
    {
        limit-=4; // max look-ahead without reloading - 3 (checking for "...")
    }
    return(curPos);
}

/********************************************************************************/
BinStr* AsmParse::MakeSig(unsigned callConv, BinStr* retType, BinStr* args, int ntyargs) 
{
    _ASSERTE((ntyargs != 0) == ((callConv & IMAGE_CEE_CS_CALLCONV_GENERIC) != 0));
    BinStr* ret = new BinStr();
	if(ret)
	{
		//if (retType != 0) 
				ret->insertInt8(callConv);
        if (ntyargs != 0)
            corEmitInt(ret, ntyargs);
		corEmitInt(ret, corCountArgs(args));

		if (retType != 0) 
			{
			ret->append(retType); 
			delete retType;
		}
		ret->append(args); 
	}
	else
		assem->report->error("\nOut of memory!\n");

    delete args;
    return(ret);
}

/********************************************************************************/
BinStr* AsmParse::MakeTypeArray(CorElementType kind, BinStr* elemType, BinStr* bounds) 
{
    // 'bounds' is a binary buffer, that contains an array of 'struct Bounds' 
    struct Bounds {
        int lowerBound;
        unsigned numElements;
    };

    _ASSERTE(bounds->length() % sizeof(Bounds) == 0);
    unsigned boundsLen = bounds->length() / sizeof(Bounds);
    _ASSERTE(boundsLen > 0);
    Bounds* boundsArr = (Bounds*) bounds->ptr();

    BinStr* ret = new BinStr();

    ret->appendInt8(kind);
    ret->append(elemType);
    corEmitInt(ret, boundsLen);                     // emit the rank

    unsigned lowerBoundsDefined = 0;
    unsigned numElementsDefined = 0;
    unsigned i;
    for(i=0; i < boundsLen; i++) 
    {
        if(boundsArr[i].lowerBound < 0x7FFFFFFF) lowerBoundsDefined = i+1;
        else boundsArr[i].lowerBound = 0;

        if(boundsArr[i].numElements < 0x7FFFFFFF) numElementsDefined = i+1;
        else boundsArr[i].numElements = 0;
    }

    corEmitInt(ret, numElementsDefined);                    // emit number of bounds

    for(i=0; i < numElementsDefined; i++) 
    {
        _ASSERTE (boundsArr[i].numElements >= 0);               // enforced at rule time
        corEmitInt(ret, boundsArr[i].numElements);

    }

    corEmitInt(ret, lowerBoundsDefined);    // emit number of lower bounds
    for(i=0; i < lowerBoundsDefined; i++)
	{
		unsigned cnt = CorSigCompressSignedInt(boundsArr[i].lowerBound, ret->getBuff(5));
		ret->remove(5 - cnt);
	}
    delete elemType;
    delete bounds;
    return(ret);
}

/********************************************************************************/
BinStr* AsmParse::MakeTypeClass(CorElementType kind, mdToken tk) 
{

    BinStr* ret = new BinStr();
    _ASSERTE(kind == ELEMENT_TYPE_CLASS || kind == ELEMENT_TYPE_VALUETYPE ||
                     kind == ELEMENT_TYPE_CMOD_REQD || kind == ELEMENT_TYPE_CMOD_OPT);
    ret->appendInt8(kind);
    unsigned cnt = CorSigCompressToken(tk, ret->getBuff(5));
    ret->remove(5 - cnt);
    return(ret);
}
/**************************************************************************/
void PrintANSILine(FILE* pF, char* sz)
{
	WCHAR wz[4096];
	if(g_uCodePage != CP_ACP)
	{
		memset(wz,0,sizeof(WCHAR)*4096);
		WszMultiByteToWideChar(g_uCodePage,0,sz,-1,wz,4096);

		memset(sz,0,4096);
		WszWideCharToMultiByte(CP_ACP,0,wz,-1,sz,4096,NULL,NULL);
	}
	fprintf(pF,sz);
}
/**************************************************************************/
void AsmParse::error(const char* fmt, ...)
{
	char sz[4096], *psz=&sz[0];
    success = false;
    va_list args;
    va_start(args, fmt);

    if(in) psz+=sprintf(psz, "%s(%d) : ", in->name(), curLine);
    psz+=sprintf(psz, "error -- ");
    vsprintf(psz, fmt, args);
	PrintANSILine(stderr,sz);
}

/**************************************************************************/
void AsmParse::warn(const char* fmt, ...)
{
	char sz[4096], *psz=&sz[0];
    va_list args;
    va_start(args, fmt);

    if(in) psz+=sprintf(psz, "%s(%d) : ", in->name(), curLine);
    psz+=sprintf(psz, "warning -- ");
    vsprintf(psz, fmt, args);
	PrintANSILine(stderr,sz);
}
/**************************************************************************/
void AsmParse::msg(const char* fmt, ...)
{
	char sz[4096];
    va_list args;
    va_start(args, fmt);

    vsprintf(sz, fmt, args);
	PrintANSILine(stdout,sz);
}


/**************************************************************************/
/*
#include <stdio.h>

int main(int argc, char* argv[]) {
    printf ("Beginning\n");
    if (argc != 2)
        return -1;

    FileReadStream in(argv[1]);
    if (!in) {
        printf("Could not open %s\n", argv[1]);
        return(-1);
        }

    Assembler assem;
    AsmParse parser(&in, &assem);
    printf ("Done\n");
    return (0);
}
*/

//#undef __cplusplus
YYSTATIC YYCONST short yyexca[] = {
#if !(YYOPTTIME)
-1, 1,
#endif
	0, -1,
	-2, 0,
#if !(YYOPTTIME)
-1, 336,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 521,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 543,
#endif
	268, 370,
	46, 370,
	47, 370,
	-2, 342,
#if !(YYOPTTIME)
-1, 634,
#endif
	268, 370,
	46, 370,
	47, 370,
	-2, 115,
#if !(YYOPTTIME)
-1, 636,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 647,
#endif
	123, 121,
	-2, 370,
#if !(YYOPTTIME)
-1, 672,
#endif
	40, 192,
	60, 192,
	-2, 377,
#if !(YYOPTTIME)
-1, 676,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 947,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 951,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 953,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 988,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 994,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1029,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1073,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1094,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1114,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1131,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1133,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1135,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1137,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1139,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1141,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1143,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1145,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1170,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1219,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1221,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1223,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1225,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1227,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1229,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1231,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1245,
#endif
	41, 361,
	-2, 193,
#if !(YYOPTTIME)
-1, 1268,
#endif
	41, 361,
	-2, 193,

};

# define YYNPROD 660
#if YYOPTTIME
YYSTATIC YYCONST yyexind_t yyexcaind[] = {
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    4,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    8,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   12,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   20,    0,   28,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   32,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   36,    0,    0,    0,   42,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   46,    0,    0,
    0,   50,    0,   54,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   58,    0,
    0,    0,    0,    0,   62,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   66,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   70,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   74,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   78,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   82,    0,   86,    0,   90,    0,   94,    0,   98,
    0,  102,    0,  106,    0,  110,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  114,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  118,
    0,  122,    0,  126,    0,  130,    0,  134,    0,  138,
    0,  142,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  146,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  150
};
#endif
# define YYLAST 2791

YYSTATIC YYCONST short YYFARDATA YYACT[] = {
  288,  933, 1169,  843,  836,  263,  516,  360,  264,  728,
  412,  588,  270,  620,  607,  414,  615,   76,  701,  573,
  261,  547,  518,  143,  175,   23,   63,   84,  128,  142,
   57,   10,  609,    6,  141,   92,   17,  112,    5, 1246,
  114,    3,  549,  144,  962,   18,   55,  660,  925, 1007,
  873,  529,  138,  923,    7, 1004,  924,   55,  466,  178,
  225,  569,   54,   56,  110,  296,   90,  122,  123,   62,
  874,  469,  554,  407,   88,  470,  400,   66,   65,  336,
   67,  397,   66,   65,  413,   67,  778,  779,  825,  466,
  251,  255,  256,  474,  259,   66,   65,  491,   67,  202,
  177,  148, 1005,  202,  229,  466,  834,  804,  344,  203,
 1258,  231,  528,  224,   66,   65,  201,   67,  293,  777,
  300,  578,  579,  580,  468,  126,  298,  202,  395,  839,
  246,  938,  939,  469,  402,  727,  838,  470,   83,  553,
  613,  552,  697,  352,  291,  350,  354,  353,  581,  582,
  583,  572,  328,   23,  478,  474,  301,  302,  322,   10,
  260,  171,  319,  321,   17,   49,  369,  315,  629,  384,
  316,  875,  323,   18,  519,  334,   59,  335,  298,  792,
  373,  320,    7,   60,  362,  365,  468, 1266,  372,  340,
  234,  236,  238,  240,  242,  371,  355,  357,  363,  364,
  368,  366,  423,  427,  374,  488,  487,  376,  387,  661,
  662,  490,  383,  341, 1192,  489,   86,  393,  393,  406,
  411,  381,  416,  417,  418, 1142,  613,  370,  118,  431,
 1140,   55,  119, 1138,  120,  616, 1136,  566,  453,  817,
  121,  455,  780,  781,  304,  245,  305,  306,  307,  419,
  420,  421, 1041, 1040, 1039, 1038,  115,  707,  708,  709,
 1134, 1132, 1130,  465,  252,  253,  254,   93,   66,   65,
  116,   67,  179,  362, 1028,  454,   66,   65,  967,   67,
  457,  950,  458,  842,  459,  803,  476,  363,  364,  359,
  479,  461,  462,   66,   65,  673,   67,  644,  635,   24,
   25,   42,   27,   43,  524,  497,   55,   66,   65, 1066,
   67,  480,   55,  481,  482,  808,  616,   60,   35,   37,
  477,  928, 1000,   66,   65,  806,   67,  493,  257,  258,
  249, 1056,  358,  361,  380,   45,   46,   41,   38,  496,
  930,  530,   44,   30,  248,  576,  501,   21,  502,   66,
   32,  405,   67,   66,   55,   66,   67,   33,   67,   55,
  949,   34,  483,  484,  485,  486,  498,  508,  509, 1068,
 1069, 1070,  998,  558,  541,   38,  471,  472,  473,  494,
  546,  171,  522,  525,  409,  604,  526,  410,  661,  662,
   38,  693,  124,  511,  537,  295,  293,  405,   55,  404,
  630,  545,  616,  535,  510,  565,  520,  292,   19,   20,
  568,   22,  456,  448, 1171,  507,  534,  555,   66,   65,
  559,   67,  550,  506,  557,  561,  712,  562,  202,  463,
   38,  388,  148,  556,  584,  542,   38,  505,  471,  472,
  473,  400,   53,  394,  401,  404,  390,  391,  567,  515,
  413,   35,   37,   52,  527,  469,   51,  676,   50,  470,
  531,  532,  533,  587,  606,  577,  625,  337,  612, 1123,
   48,   38,  586,   47,  333,  946,  623,  474,  624, 1122,
  621,  504,  386,  593,  394,  706,  627,  390,  391,   66,
   65, 1120,   67,  113,  570, 1065,  863,  467,  862,  626,
 1059,  465,  632,  861,  860,  469,  665, 1006,  468,  470,
  592,  617,  865,  467,   60,  362,  653,  821,   66,  819,
  820,   67,  202,   55,  628,  663,  656,  474,  657,  363,
  364,  664,  658,  631,  640,  643,  202,  611,   45,   46,
   41,   38,  672,  617,   55,   55,  791,  832,  250,  247,
 1058,   66,   65,  645,   67,  650,  148,  596,  468,  929,
  699,  377,  460,  633, 1003,   66,  659,  692,   67,  797,
  694,  447, 1002,  667,  293,  232,  343,  674,  725,  646,
  339,  668,  293,  789,  721,  663,  722,  723,  724,  671,
  810,  811,  812,  813,  202,  790,  681,  682,  695,  713,
   87,  293,   70,  766,  603,  469,  398,  670,  293,  470,
  428,  425,  424,  837,   45,   46,   41,   38,  426,  787,
  788,  807,  716,  717,  718,  118,  798,  474,  710,  119,
  241,  120,  705,  672,  769,  800,  564,  121,  698,  793,
  794,  610,  703,  805,  239,  663,  237,  782,  783, 1093,
  870,  827,  235,  115,  652,  795,  829,  233,  468,  715,
  719,  720,  563,  230,  328,  816,  824,  116, 1055,  870,
  322,  814,  823, 1051,  319,  321,  869,  870,  830,  315,
  802,  232,  316,  847,  323,  815,  822,  406,  844,  651,
   75,  560,  826,  320,  503,  232,  415,  232,  801,  871,
  848,  849,  469,  232,  294,  795,  799,  771,  232,  772,
  773,  774,  775,  776,  232,  491,  669,  127,  871,  855,
 1127,  858,  856,  796,  474, 1126,  871,  850, 1125, 1116,
  835,  867,  991,  786,  684,  605,  851,   24,   25,   42,
   27,   43,  118,  523,  338,  864,  119,    1,  120, 1012,
 1008, 1009, 1010, 1011,  121,  468,   35,   37,  475,  725,
  471,  472,  473,  854,  202,  721,  477,  722,  723,  724,
  115,  491, 1237,   45,   46,   41,   38,  491, 1061,  993,
   44,   30,  702,  785,  116,   21,  679,  678,   32,  880,
  876,  877,  878,  879,  655,   33,  129,  638,  940,   34,
  931,  945,  449,  716,  717,  718,  787,  937,  941,  310,
  471,  472,  473,  491, 1173,   64,  491, 1172, 1261,  936,
  491,  846,  770,  491,  491,  227, 1270,   66, 1262,  934,
   67, 1259, 1256,  948,  617,  942, 1255, 1254, 1253, 1252,
  715,  719,  720,  663, 1251, 1250,   19,   20, 1236,   22,
  621,  955,  956,  957,  958,  983, 1234, 1232,  989, 1230,
  959,  960,  961,  992, 1228,  140, 1226, 1224, 1222,  995,
  982, 1220,  766,  714, 1218, 1217,  996,  180, 1216, 1200,
 1199, 1198,  986, 1197,  329, 1196,   42,   27,   43, 1195,
  987, 1177, 1166,  963, 1165,  330, 1164, 1163, 1162, 1161,
  331, 1156, 1155,   35,   37, 1001, 1154, 1153, 1152,  997,
  471,  472,  473, 1151,  999, 1150, 1149, 1146, 1128, 1124,
   45,   46,   41,   38,   74,  324,  325,   82,   81,   80,
   79, 1117,   77,   78,   83,   66,   65,  293,   67, 1115,
   55,  311, 1113, 1075, 1074, 1072, 1057,  332,  990,  954,
  944,  663,  935,  943, 1016,  940,  926,  857, 1018,  852,
 1019, 1020, 1021, 1022, 1023, 1024, 1025, 1026,  663,  356,
  841,  840,  833, 1013,  367, 1030,  327,  784,  766,  711,
  375,  704,  689,  688,  686,  382,  683,  938,  939,  677,
  675, 1049,  654,  637,  601, 1050,  600,  599, 1017,  598,
  597, 1054,  595, 1027,  594,  540,  495,  471,  472,  473,
  452, 1048,  309,  308, 1268, 1031,  290,  244, 1245, 1235,
 1231,  672,  672,  672,  672,  672,  672,  672, 1229,  663,
 1227, 1225, 1223, 1090, 1052, 1221, 1092, 1064, 1219, 1170,
 1095, 1097, 1145, 1060,  451, 1062, 1063, 1143, 1141, 1112,
 1139, 1137, 1135, 1133, 1131, 1114,  663, 1067, 1071, 1053,
  408, 1106,  898, 1105, 1104, 1103, 1094, 1073, 1077, 1079,
 1081, 1083, 1085, 1087, 1089, 1047, 1091, 1046, 1045, 1044,
 1129, 1043, 1042, 1037, 1036, 1035, 1076, 1078, 1080, 1082,
 1084, 1086, 1088,  313, 1121, 1034, 1033, 1032, 1029, 1015,
 1014, 1148, 1119, 1118,   95,   96,   97,   98,   99,  100,
  101,  102,  103,  104,  105,  106,  107,  108,  109,  994,
  725, 1167,  988,  965,  953,  951,  721,  947,  722,  723,
  724,  663,  872,  663,  866,  663,  859,  663, 1179,  663,
 1181,  663, 1183,  663, 1185,  691, 1187,  690, 1189,  687,
 1191, 1194, 1193,  685,  636,  619, 1144,  618,  591, 1168,
  590,  544,  539,  538,  716,  717,  718,  536, 1174, 1175,
 1176,  521,  492,  464,  450,  430,  429, 1214, 1178,  422,
 1180,  378, 1182,  312, 1184,  543, 1186,   75, 1188,   94,
 1190,  303,  243,  663,  548,  228,  140,  403,  396,  399,
  226,  715,  719,  720,  392, 1215,  389,  385,   31, 1096,
 1098, 1099, 1100, 1101, 1102,  125,   71,   28,  351, 1107,
 1108, 1109, 1110, 1111,  349,  348, 1238,  111, 1239,   73,
 1240,  575, 1241,  347, 1242,  293, 1243,  346, 1244, 1248,
 1233,  169,  345,  150,  981,  980,  975,  589,  974,  973,
  972,  971, 1257,  969,  970,   83,  168,  979,  978,  977,
  976,  139,  133,  131,  135,  984,   26,  663,  818, 1264,
 1249,  809,   72,  326,  642, 1269,  318,  909, 1247,  641,
 1157, 1158, 1159, 1160,  317,  314,  853,  696,  571,   29,
  885,  886,   16,  893,  907,  887,  888,  889,  890, 1263,
  891,  892,  176,  908,  894,  895,  896,  897,   15,   14,
  171,  174,  639,  634, 1267,   13,  173,   12,   11,    9,
  140,  742,    8,    4,    2,  165,  157,   91,   89,  647,
 1265,   58,   36,  608,  734,  735,  514,  743,  760,  736,
  737,  738,  739,  513,  740,  741,  666,  761,  744,  745,
  746,  747,  223,   68,   61,  574,  329,  622,   42,   27,
   43,  932,  968,   85,  379,  614,  517,  330,  680,   69,
  881,  602,  331,  297,   40,   35,   37,   39,  117,  726,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  758,
  700,  762,   45,   46,   41,   38,  764,  324,  325,    0,
    0,    0,    0,    0,    0,  289,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  171,  332,
  551,   74,    0,    0,   82,   81,   80,   79,    0,   77,
   78,   83,    0,    0,    0,    0,  767,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   66,  327,    0,
   67,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  828,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  831,    0,    0,    0,  883,  884,    0,  899,
  900,  901,    0,  902,  903,    0,    0,  904,  905,  845,
  906,    0,    0,    0,  548,  548,    0,  171,    0,  130,
    0,    0,  882,  910,  911,  912,  913,  914,  915,  916,
  917,  918,  919,  920,  921,  922,    0,  729,  575,  730,
  731,  732,  733,  748,  749,  750,  765,  751,  752,  753,
  754,  755,  756,  757,  759,  763,    0,    0,    0,  768,
    0,    0,    0,  868,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   66,    0,    0,   67,    0,
  152,  153,  154,  155,  156,  158,  159,  160,  161,  162,
  163,  164,  170,  166,  167,  271,    0,    0,    0,   43,
  132,  172,  134,  151,  136,  137,    0,    0,    0,    0,
    0,    0,    0,  927,   35,   37,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   45,   46,   41,   38,    0,    0,    0,    0,  952,
    0,    0,  146,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   66,    0,    0,   67,  145,    0,
    0,    0,    0,    0,    0,  964,    0,    0,    0,    0,
  966,    0,    0,    0,  985,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  149,  147,  152,  153,
  154,  155,  156,  158,  159,  160,  161,  162,  163,  164,
  170,  166,  167,    0,    0,    0,    0,   43,  132,  172,
  134,  151,  136,  137,    0,    0,    0,    0,    0,    0,
    0,    0,   35,   37,  271,    0,    0,    0,    0,  469,
    0,    0,    0,  470,    0,    0,    0,    0,    0,   45,
   46,   41,   38,    0,    0,    0,    0,    0,    0,    0,
  146,  474,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  145,  152,  153,  154,
  155,  156,  158,  159,  160,  161,  162,  163,  164,  170,
  166,  167,  500,    0,    0,    0,   43,  132,  172,  134,
  151,  136,  137,    0,  149,  147,    0,    0,    0,    0,
    0,   35,   37,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   45,   46,
   41,   38,    0,    0,    0,    0,    0,    0,    0,  146,
    0,    0,  274,  275,  273,  282,    0,  276,  277,  278,
  279,    0,  280,  281,    0,  145,  283,  284,  285,  286,
  446,    0,  266,  267,    0,    0,    0,    0,  271,    0,
    0,  265,  272,  469,    0,    0,    0,  799,    0,    0,
    0,    0,    0,  149,  147,    0,  268,  269,  287,    0,
  438,  432,  433,  434,  435,  474,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  289,    0,    0,    0,    0,    0,  440,
  441,  442,  443,    0,    0,  437,  500,    0,    0,  444,
  445,  436,    0,    0,    0,    0,    0,    0,    0, 1147,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   66,   65,    0,   67,    0,    0,    0,    0,    0,
    0,  274,  275,  273,  282,  271,  276,  277,  278,  279,
  469,  280,  281,    0,  470,  283,  284,  285,  286,    0,
    0,  266,  267,    0,    0,    0, 1260,    0,    0,    0,
  265,  272,  474,    0,    0,    0, 1201,    0,    0,    0,
    0,    0,    0,    0,    0,  268,  269,  287,  221,  118,
    0,    0,    0,  119,  439,  120,    0,    0,    0,    0,
    0,  121,    0,  500,    0,    0,    0,    0,    0,    0,
    0,    0,  289,    0,  471,  472,  473,  115,  209,  204,
  205,  206,  207,  208,    0,    0,  661,  662,  211,    0,
    0,  116,    0,    0,    0,  271,    0,  210,    0,  219,
    0,    0,    0,    0,    0,    0,    0,  212,  213,  214,
  215,  216,  217,  218,  222,    0,    0,    0,    0,    0,
  220,    0,    0,    0,    0,   66,   65,    0,   67,    0,
    0,    0,    0,    0,    0,  274,  275,  273,  282,    0,
  276,  277,  278,  279,    0,  280,  281,    0,    0,  283,
  284,  285,  286,  262,    0,  266,  267,    0,    0,    0,
    0,    0,    0,    0,  265,  272,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  268,
  269,  287,    0,    0,    0,    0,    0,    0,    0,    0,
    0, 1213,    0,    0,    0,    0,    0,    0,    0,    0,
    0, 1213,    0,    0, 1206,    0,  289,    0,  471,  472,
  473,    0,    0,    0, 1206,  271,    0,    0,    0, 1202,
  661,  662,    0,    0,    0,    0,    0, 1211,    0, 1202,
    0,    0,   66,   65,    0,   67,    0, 1211,    0,    0,
    0, 1212,  274,  275,  273,  282,    0,  276,  277,  278,
  279, 1212,  280,  281,    0,    0,  283,  284,  285,  286,
    0,    0,  266,  267, 1203, 1204, 1205, 1207, 1208, 1209,
 1210,  265,  272,  262, 1203, 1204, 1205, 1207, 1208, 1209,
 1210,    0,    0,    0,    0,    0,  268,  269,  287,    0,
    0,    0,  271,    0,    0,    0,    0,  469,    0,    0,
    0,  470,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  289,    0,  471,  472,  473,    0,  474,
    0,    0,   66,   65,    0,   67,    0,  499,    0,    0,
    0,    0,  274,  275,  273,  282,    0,  276,  277,  278,
  279,  271,  280,  281,    0,    0,  283,  284,  285,  286,
  500,   66,  266,  267,   67,    0,    0,    0,    0,    0,
    0,  265,  272,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  268,  269,  287,  200,
  649,    0,    0,    0,    0,    0,    0,    0,  271,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  262,
    0,    0,    0,  289,    0,  183,    0,    0,    0,  198,
    0,  181,  182,    0,    0,  648,  185,  186,  196,  187,
  188,  189,  190,  191,  192,  193,  194,  184,    0,    0,
    0,  197,    0,    0,    0,  271,    0,  195,    0,    0,
    0,    0,   66,   65,  199,   67,  262,    0,    0,    0,
    0,    0,  274,  275,  273,  282,    0,  276,  277,  278,
  279,    0,  280,  281,    0,    0,  283,  284,  285,  286,
    0,    0,  266,  267,    0,    0,    0,    0,    0,    0,
    0,  265,  272,  271,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  262,    0,    0,  268,  269,  287,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   66,
   65,    0,   67,  289,  299,    0,    0,    0,    0,  274,
  275,  273,  282,    0,  276,  277,  278,  279,    0,  280,
  281,  585,  271,  283,  284,  285,  286,    0,    0,  266,
  267,    0,    0,    0,    0,    0,    0,    0,  265,  272,
    0,    0,    0,    0,    0,    0,    0,    0,   66,   65,
    0,   67,    0,  268,  269,  287,    0,    0,  274,  275,
  273,  282,    0,  276,  277,  278,  279,    0,  280,  281,
    0,    0,  283,  284,  285,  286,    0,    0,  266,  267,
  289,    0,  471,  472,  473,    0,    0,  265,  272,    0,
    0,    0,    0,    0,    0,   66,   65,    0,   67,    0,
    0,    0,  268,  269,  287,  274,  275,  273,  282,    0,
  276,  277,  278,  279,    0,  280,  281,    0,    0,  283,
  284,  285,  286,    0,    0,  266,  267,    0,    0,  289,
    0,    0,    0,    0,  265,  272,    0,    0,    0,    0,
    0,    0,   66,   65,    0,   67,    0,    0,    0,  268,
  269,  287,  274,  275,  273,  282,    0,  276,  277,  278,
  279,    0,  280,  281,    0,    0,  283,  284,  285,  286,
    0,    0,  266,  267,    0,    0,  512,    0,    0,    0,
    0,  265,  272,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  268,  269,  287,    0,
  274,  275,  273,  282,    0,  276,  277,  278,  279,    0,
  280,  281,    0,    0,  283,  284,  285,  286,    0,    0,
  266,  267,    0,  342,    0,    0,    0,    0,    0,  265,
  272,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  268,  269,  287,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  274,
  275,  273,  282,    0,  276,  277,  278,  279,    0,  280,
  281,  289,    0,  283,  284,  285,  286,    0,    0,  266,
  267,    0,    0,    0,    0,    0,    0,    0,  265,  272,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  268,  269,  287,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  289
};

YYSTATIC YYCONST short YYFARDATA YYPACT[] = {
-1000,  -89,-1000,  350,  347,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,  335,  333,  330,  319,-1000,-1000,-1000,   41,
   41, -469,   52,-1000, -376,  158,-1000,  511, 1149,  -51,
  509,   41, -379,-1000, -178,  695,  -51,  695,  453,  -51,
  -51,  129,-1000, -186,  656,-1000,-1000,-1000,-1000, 1374,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,   41, -165,-1000,
-1000, 2031,-1000,  718,-1000,-1000,-1000,-1000, 1700,-1000,
   41,-1000,  652,-1000,  783, 1155,  -51,  623,  617,  612,
  606,  604,  590, 1152,  976,  -22,-1000,   41,  286, -183,
  158,    8,  718,  158, 2248,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
  975,  346,  643, 2122, 2449,  -61,  -61,-1000, 1151,-1000,
-1000,  -50,  972,  971,  765,   93,-1000, 1143,  968,  349,
-1000,-1000,   41,-1000,   41,   39,-1000,-1000,-1000,-1000,
  686,-1000,-1000,-1000,-1000,  489,   41, 2342,-1000,  485,
 -159,-1000,-1000,   89,   41,   52,  249,  -51,   89,  -61,
 2449, 2248, -144,  -61,   89, 2122, 1141,-1000,-1000,  258,
-1000,-1000,-1000,   44,    3,    9,  -52,-1000,   53,-1000,
  636,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,  -98,-1000,-1000,-1000,
 1139,  291,  158,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000, 1136, 1135, 1542,  478,  288,  758, 1134,   93,  969,
    7,-1000,   41,    7,-1000,   52,-1000,   41,-1000,   41,
-1000,   41,-1000,-1000,-1000,-1000,  469,-1000,   41,   41,
-1000,  718,-1000,-1000,-1000,   57,  718,-1000,-1000,  718,
 1133,-1000,   63,  467,  711,  229,-1000,-1000, -145,  229,
  -61,  280,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,   87,-1000,-1000,-1000,-1000,  -68,  718,-1000,
-1000,  770, 1132,-1000,  339,  965,-1000,-1000,  -61, 2449,
 1912,-1000,-1000,   41,-1000,-1000,-1000,-1000,-1000,-1000,
   85,  633,-1000,-1000,-1000,-1000,  314,  300,  292,-1000,
-1000,-1000,-1000,-1000,   41,   41,  281, 2295,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,  -95, 1131,-1000,   41,
  685,   36,  -61,   41,-1000, -159,   38,   38,   38,   38,
 2248,  258,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000, 1123, 1122,  964,-1000,-1000, 2449, 2199,
-1000,  770, 1121,  -51, 2449,-1000,-1000,-1000,   89,   41,
 1295,-1000, -168, -170,-1000,-1000, -370,-1000,-1000,  -51,
   41,  312,  -51,-1000,  630,-1000,-1000,  -51,-1000,  -51,
  601,  575,-1000,-1000,  158, -208,-1000,-1000,-1000,  158,
 -384,-1000, -362,-1000, -156,  305,-1000,-1000,-1000,-1000,
-1000,-1000,   41,  718,-1000,-1000, -199,  718, 2390,   41,
  134,  567,-1000,-1000,-1000,-1000,-1000,-1000,-1000, 1120,
-1000,-1000,-1000,-1000,-1000,-1000, 1118,-1000,-1000,  652,
  134,  963,-1000,  961,  464,  959,  958,  956,  955,  953,
-1000,  341,  677,  158,  134,  548,  444,  158,  133,-1000,
-1000,-1000, 1117, 1115, 2449,  158,-1000,   16,  229,-1000,
 2449,   41,-1000,-1000,-1000,-1000,-1000,-1000, -106,-1000,
-1000,  138,-1000,  770,-1000,  -61, 2449, 2199,   30, 1114,
   47,  952,  753,-1000, 1187,-1000,-1000,-1000,-1000,-1000,
-1000,   29,  -61, 2002,  336,  291,  951,  750,-1000,-1000,
 2390,  -95,  439,   41, -146, 2449,  413,-1000,-1000,-1000,
   89,-1000,-1000,-1000,-1000,-1000,-1000,-1000,   41,   52,
-1000, 1671,   27,-1000,  134,  949,  417,  948,  743,  742,
-1000,-1000,   93,   41,   41,  945,  676,  770, 1113,  943,
 1109,  942,  941, 1107, 1105,  718,  158,-1000,   81,  158,
  -51, -166, 2248,  498,   93,  738, 2248,  940,-1000,-1000,
-1000,-1000,-1000,-1000,   33, -219,  938,   54,  832, -175,
 1064,   41,-1000,  781,-1000,  484,-1000,  484,  484,  484,
  484,  484, -191,-1000,   41,   41,  718,  936,  739,  672,
  158,  158,  490,-1000,  502,-1000,-1000,  -90,  229,  229,
  661,  467,-1000,  718,  476,  158,-1000,  664,-1000,-1000,
-1000,  574, 1805,   17,-1000, -248,  -95,-1000,   62,-1000,
  496,  190,  114,  -37, -146, 2449,   93,-1000,-1000,-1000,
 2449,-1000,-1000,  718,-1000,  -95,   95,  931, -266,-1000,
-1000,-1000,-1000,  718, 2199,  552, -173, -180,  930,  929,
   15,  628,  718,   93,  780,-1000,  -95,-1000,   89,   89,
-1000,-1000,-1000,-1000,   41,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,  718,   41,  718,  918,-1000, 2248,-1000,-1000,
  738,-1000,  305,  916,-1000,  636, 1096,  411,  410,  405,
  403,-1000,  134,  471,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000, 1094,  552,   93,  635, 1092,
 -404, -100,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,  515,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000, 1020,
-1000,-1000, -416,-1000, -405,-1000,-1000, -425,-1000,  915,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,   93,-1000,-1000,
-1000,-1000,   58,  296,-1000,  134,  675,  711,  711,  158,
-1000,  -34,   41,  912,  909, 2449,-1000,  158,  382, 1087,
  320,   13, 1085,   93, 1084,  908,-1000,-1000,-1000,-1000,
  -61,  -61,  -61,  -61,-1000,-1000,-1000,-1000,-1000,  -61,
  -61,  -61,-1000,-1000,-1000,-1000, -437, 2199,-1000,  567,
-1000,-1000, 1083,-1000,   93,   10,-1000,  973,   93,   41,
-1000,-1000, -146, 1082, 2449,-1000,-1000,  907,-1000,-1000,
  674,-1000, -313,  735,-1000,-1000,-1000,-1000, 1079, 1064,
-1000,-1000,-1000,-1000,  770,-1000,   41,-1000,-1000,-1000,
-1000,  279,  134,  481,  473,-1000,-1000,-1000,-1000,-1000,
-1000,   11,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,  475,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
   41,-1000,-1000,-1000,-1000, 1060,  770, 1059,-1000,-1000,
  711,-1000,-1000,-1000,-1000,  467,  158,  -95,  770,-1000,
 -146,  -95,-1000,  -95,-1000, 2449, 2449, 2449, 2449, 2449,
 2449, 2449,  -61,    6, 1058, 1064,-1000, -146,-1000, 1057,
 1056, 1055, 1045, 1044, 1043,  -23, 1042, 1041, 1039, 1038,
 1037, 1035,  770,  -51,-1000,-1000,-1000,  628,  -95,  611,
-1000,   41,-1000, 2248,  -95,  627,  290,  905,-1000,  457,
   41,  734,   41,   41,  134,  402,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,   46,   41,   94,  904, 1027,  903,  902,
 1671, 1671, 1671, 1671, 1671, 1671, 1671, 2449, -146,  -95,
  608, 1026,  -82,  -82,   52,   52,   52,   52, 1025, 1024,
 1023, 1021,   52,   52,   52,   52,   52, -181,  901, 1015,
  898,-1000,  671,-1000,  890, -146,-1000,-1000,-1000,   41,
  398,  134,  386,  376,  770,-1000,-1000,  878,  670,  667,
  662,  877,-1000,  -95,-1000,-1000,   -6, 1014,   -7, 1013,
   -8, 1012,  -32, 1011,  -35, 1010,  -38, 1008,  -43, 1007,
 2199, 1002,  876,   93,  -95,  875,  874,  872,  867,  866,
  865,  861,  860,   52,   52,   52,   52,  858,  857,  856,
  855,  853,  851,-1000,  -95,-1000,   41,-1000,  999,  321,
-1000,  773,-1000,-1000,-1000,   41,   41,   41,-1000,  850,
 -146,  -95, -146,  -95, -146,  -95, -146,  -95, -146,  -95,
 -146,  -95, -146,  -95,  -54,  -95,  552,-1000,  848,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,  844,  842,  840,
  839,-1000,-1000,-1000,-1000,-1000,-1000,  838,-1000, 1853,
  -95,-1000,  134,-1000,  837,  834,  833,-1000,  998,  830,
  995,  827,  992,  826,  991,  825,  990,  823,  988,  818,
  980,  816, -146,  815,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,  979,  807,  728,-1000,-1000,-1000,  -95,
-1000,  -95,-1000,  -95,-1000,  -95,-1000,  -95,-1000,  -95,
-1000,  -95,-1000,  978, -442,   41,-1000,  134,  804,  803,
  798,  797,  796,  795,  791,  -95, -231,  790, 1843,  777,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,  787,  -61,-1000,
-1000,-1000,-1000, 2449, 2199,  -81, -146,  974,  -95,  785,
-1000
};

YYSTATIC YYCONST short YYFARDATA YYPGO[] = {
    0,    0,  815,   47, 1379,    8,   21,   40, 1378,   42,
 1377, 1374,   37,  395, 1373, 1371,  406,  111, 1370, 1369,
    1,    7,  176,    6, 1366,   22,    5,   16, 1365, 1364,
   27, 1363, 1362,    9,    4,   32, 1361,    3,   13, 1357,
   18,   19,   15, 1355,   20,   65, 1354, 1353, 1352,    2,
 1343, 1336,   11,   14, 1333, 1189, 1332, 1331,   10, 1328,
  116, 1327, 1326, 1325,  747, 1324,   41,   28, 1323,   38,
  165,   33,   52, 1322, 1319,   29, 1318, 1317, 1316, 1315,
 1311, 1309,   24, 1308, 1302, 1292,   34,   43,   23, 1289,
 1288, 1287, 1286, 1285, 1284, 1279, 1276, 1274, 1273,   12,
 1271, 1268, 1266, 1264, 1263, 1262, 1261,   51, 1256, 1243,
  108, 1242, 1241, 1237,  112, 1233, 1225, 1224, 1218, 1217,
 1216, 1215,   60, 1200,   17, 1208,   81, 1207,  431, 1206,
 1204, 1199, 1198, 1197, 1060
};
YYSTATIC YYCONST yyr_t YYFARDATA YYR1[]={

   0,  64,  64,  65,  65,  65,  65,  65,  65,  65,
  65,  65,  65,  65,  65,  65,  65,  65,  65,  65,
  65,  65,  65,  65,  35,  35,  88,  88,  88,  87,
  87,  87,  87,  87,  87,  85,  85,  85,  74,  15,
  15,  15,  15,  15,  73,  89,  68,  66,  46,  46,
  46,  46,  46,  46,  46,  46,  46,  46,  46,  46,
  46,  46,  46,  46,  46,  46,  46,  46,  46,  46,
  46,  46,  46,  46,  90,  90,  91,  91,  67,  67,
  92,  92,  42,  42,  41,  41,  40,  40,  93,  93,
  93,  93,  93,  93,  93,  93,  93,  93,  93,  93,
  93,  93,  93,  71,   4,   4,  34,  34,  19,  19,
  10,  11,  14,  14,  14,  14,  12,  12,  13,  13,
  94,  94,  50,  50,  50,  95,  95, 100, 100, 100,
 100, 100, 100, 100, 100, 100, 100, 100,  96,  51,
  51,  51,  97,  97, 101, 101, 101, 101, 101, 101,
 101, 101, 101, 102,  69,  69,  47,  47,  47,  47,
  47,  47,  47,  47,  47,  47,  47,  47,  47,  47,
  47,  47,  47,  47,  47,  47,  47,  47,  52,  52,
  52,  52,  52,  52,  52,  52,  52,  52,  52,  52,
   3,   3,   3,  16,  16,  16,  16,  16,  48,  48,
  48,  48,  48,  48,  48,  48,  48,  48,  48,  48,
  48,  48,  48,  48,  49,  49,  49,  49,  49,  49,
  49,  49,  49,  49,  49,  49,  49, 103, 104, 104,
 104, 104, 104, 104, 104, 104, 104, 104, 104, 104,
 104, 104, 104, 104, 104, 104, 104, 104, 104, 107,
 108, 105, 110, 110, 109, 109, 109, 112, 111, 111,
 111, 111, 115, 115, 115, 118, 113, 116, 117, 114,
 114, 114,  70,  70,  72, 119, 119, 121, 121, 120,
 120, 122, 122,  17,  17, 123, 123, 123, 123, 123,
 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,
  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
  32, 124,  30,  30,  31,  31,  62,  63,  99, 106,
 106, 106, 106, 106, 106, 106, 106, 106, 106, 106,
 106, 106, 106, 106, 106, 106, 106, 106, 106, 106,
 106, 106, 106,  37,  37,  38,  38,  38,  39,  39,
  43,  23,  23,  24,  24,  25,  25,  25,  25,  25,
   1,   1,   1,  44,  44,  44,  44,   5,   5,  45,
  45,  45,  45,   7,   7,   7,   7,   8,   8,   8,
   8,   8,   8,   8,  33,  33,  33,  33,  33,  33,
  33,  33,  33,  33,  33,  33,  33,  33,  33,  33,
  33,  33,  33,  33,  33,  33,  33,  33,  33,  33,
  33,  33,  33,  33,  33,  33,  33,  33,  33,  33,
  33,  33,  33,  33,  33,  33,  33,  33,  33,  33,
  33,  33,  33,  33,  33,  33,  33,  33,  18,  18,
  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,
  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,
  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,
  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,
  18,  18,  18,  18,  18,  18,  18,  26,  26,  26,
  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
  26,  26,  26,  26,  26,  28,  28,  27,  27,  27,
  27,  27,   6,   6,   6,   6,   6,   2,   2,  29,
  29,   9,  22,  21,  21,  21,  86,  86,  86,  86,
  56,  53,  53,  54,  20,  20,  36,  36,  36,  36,
  36,  36,  36,  36,  55,  55,  55,  55,  55,  55,
  55,  55,  55,  55,  55,  55,  55,  55,  55, 125,
 125,  75,  75,  75,  75,  75,  75,  75,  75,  75,
  75,  75,  76,  76,  57,  57,  58,  58, 126,  77,
  59,  59,  59,  59,  78,  78, 127, 127, 127, 128,
 128, 128, 128, 128, 129, 131, 130,  79,  79,  80,
  80, 132, 132, 132,  81,  98,  60,  60,  60,  60,
  60,  60,  60,  60,  60,  82,  82, 133, 133, 133,
 133,  83,  61,  61,  61,  84,  84, 134, 134, 134 };
YYSTATIC YYCONST yyr_t YYFARDATA YYR2[]={

   0,   0,   2,   4,   4,   3,   1,   1,   1,   1,
   1,   1,   4,   4,   4,   4,   1,   1,   1,   2,
   2,   3,   2,   1,   1,   3,   2,   4,   6,   2,
   4,   3,   5,   7,   3,   1,   2,   3,   7,   0,
   2,   2,   2,   2,   3,   3,   2,   6,   0,   2,
   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,
   2,   2,   2,   5,   0,   2,   0,   2,   0,   2,
   3,   1,   0,   3,   3,   2,   0,   2,   3,   4,
   4,   4,   1,   1,   1,   1,   1,   2,   2,   4,
  13,  20,   1,   7,   0,   2,   0,   2,   0,   3,
   4,   7,   9,   7,   5,   3,   8,   6,   1,   1,
   4,   3,   0,   2,   2,   0,   2,   9,   7,   9,
   7,   9,   7,   9,   7,   1,   1,   1,   9,   0,
   2,   2,   0,   2,   9,   7,   9,   7,   9,   7,
   1,   1,   1,   1,  12,  15,   0,   2,   2,   2,
   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
   2,   2,   2,   2,   5,   8,   6,   5,   0,   2,
   2,   2,   2,   2,   2,   2,   2,   2,   2,   5,
   1,   1,   1,   0,   4,   4,   4,   4,   0,   2,
   2,   2,   2,   2,   2,   2,   5,   2,   2,   2,
   2,   2,   2,   5,   0,   2,   2,   2,   2,   2,
   2,   2,   2,   2,   2,   2,   5,   1,   2,   1,
   2,   4,   5,   1,   1,   1,   1,   2,   1,   1,
   1,   1,   4,   6,   4,   4,  10,   1,   5,   3,
   1,   2,   2,   1,   2,   4,   4,   1,   2,   2,
   2,   2,   2,   2,   2,   1,   2,   1,   1,   1,
   4,   4,   0,   2,   2,   4,   2,   0,   1,   3,
   1,   3,   1,   0,   3,   5,   4,   3,   5,   5,
   5,   5,   5,   5,   2,   2,   2,   2,   2,   2,
   4,   4,   4,   4,   4,   4,   4,   4,   5,   5,
   5,   5,   4,   4,   4,   4,   4,   4,   1,   3,
   1,   2,   0,   1,   1,   2,   2,   1,   1,   1,
   2,   2,   2,   2,   2,   2,   3,   2,   2,  10,
   8,   5,   3,   2,   2,   5,   4,   6,   2,   2,
   2,   4,   2,   0,   3,   0,   1,   1,   3,   2,
   3,   0,   1,   1,   3,   1,   2,   3,   6,   7,
   1,   1,   3,   4,   4,   5,   1,   1,   3,   1,
   3,   4,   1,   2,   2,   1,   4,   0,   1,   1,
   2,   2,   2,   2,   0,  10,   6,   5,   5,   1,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
   1,   2,   2,   2,   2,   1,   1,   1,   1,   2,
   3,   4,   6,   5,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   1,   2,   4,   1,   2,   1,
   2,   1,   2,   1,   2,   1,   2,   1,   0,   1,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
   2,   2,   2,   2,   1,   1,   1,   1,   1,   3,
   2,   2,   1,   1,   1,   1,   1,   1,   1,   1,
   1,   2,   1,   1,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   1,   1,   1,   2,   1,   1,
   3,   2,   3,   4,   2,   2,   2,   5,   5,   7,
   4,   3,   2,   1,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   2,   2,   2,   2,   1,   1,   1,
   1,   2,   3,   2,   2,   1,   3,   0,   1,   1,
   3,   2,   0,   3,   3,   1,   1,   1,   1,   0,
   2,   1,   1,   1,   4,   4,   6,   3,   3,   3,
   4,   1,   3,   3,   1,   1,   1,   1,   4,   1,
   6,   6,   6,   4,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
   1,   3,   2,   5,   4,   7,   6,   7,   6,   9,
   8,   3,   8,   4,   0,   2,   0,   1,   3,   3,
   0,   2,   2,   2,   0,   2,   3,   1,   1,   3,
   8,   2,   3,   1,   3,   3,   3,   3,   5,   0,
   2,   3,   1,   3,   4,   3,   0,   2,   2,   3,
   3,   3,   3,   3,   3,   0,   2,   2,   3,   2,
   1,   3,   0,   2,   2,   0,   2,   4,   3,   1 };
YYSTATIC YYCONST short YYFARDATA YYCHK[]={

-1000, -64, -65, -66, -68, -69, -71, -72, -73, -74,
 -75, -76, -77, -79, -81, -83, -85, -86, -87, 497,
 498, 436, 500, -88, 388, 389,-102, 391,-119, -89,
 432,-125, 439, 446, 450, 407, -56, 408, 427, -10,
 -11, 426, 390, 392, 431, 424, 425, 123, 123, -70,
 123, 123, 123, 123,  -9, 265,  -9, 499, -57, -22,
 265, -46, 445,  -1,  -2, 261, 260, 263, -47, -19,
  91,-120, 123,-123, 272,  38,-124, 280, 281, 278,
 277, 276, 275, 282, -30, -31, 267,  91,  -9, -59,
 445, -61,  -1, 445, -55, 409, 410, 411, 412, 413,
 414, 415, 416, 417, 418, 419, 420, 421, 422, 423,
 -30, -55, -12,  40,  -7, 317, 331,  -8, 289, 293,
 295, 301, -30, -30, 263,-121, 311,  61, -67, -64,
 125,-104, 393,-105, 395,-103, 397, 398, -72,-106,
  -2, -86, -75, -88, -87, 451, 435, 480,-107, 479,
-109, 396, 373, 374, 375, 376, 377, -62, 378, 379,
 380, 381, 382, 383, 384, -63, 386, 387,-108,-112,
 385, 123, 394, -78, -80, -82, -84,  -9,  -1, 437,
  -2, 320, 321, 314, 336, 325, 326, 328, 329, 330,
 331, 332, 333, 334, 335, 346, 327, 340, 318, 353,
 288, -60,  46,  -7, 319, 320, 321, 322, 323, 318,
 337, 328, 347, 348, 349, 350, 351, 352, 353, 339,
 360, 288, 354, -48,  -9,-122,-123,  42,  40, -30,
  40, -17,  91,  40, -17,  40, -17,  40, -17,  40,
 -17,  40, -17,  40,  41, 267,  -9, 263,  58,  44,
 262,  -1, 447, 448, 449,  -1,  -1, 320, 321,  -1,
 -45, -44,  91, -26,  -5, 299, 290, 291, 314, 315,
 -99,  33, 300, 272, 270, 271, 275, 276, 277, 278,
 280, 281, 273, 284, 285, 286, 287, 316,  -1, 341,
  41, -35,  61, 262,  61, -13, -45, -14, -99, 342,
 -26,  -7,  -7,  40, 294, 296, 297, 298,  41,  41,
  44,  -2,  40, 125, -93, -69, -66, -94, -96, -71,
 -72, -86, -75, -87, 429, 430, -98, 480, -88, 388,
 399, 404, 451, 125,  -9,  -9,  40, 428,  58,  91,
  -9, -45, 341,  91,-110,-111,-113,-115,-116,-117,
 304,-118, 302, 306, 305,  -9,  -2,  -9, -22,  40,
 -21, -22, 266, 280, 281, -30,  -9,  -2,  -7, -26,
 -45, -35, 332,-124,  -7,  -2,  -9, -13,  40, -29,
 -70,-107,  -2,  -9, 125,-127, 438, -86,-128,-129,
 443, 444,-130, -87, 440, 125,-132,-126,-128,-131,
 438, 441, 125,-133, 436, 388, -87, 125,-134, 436,
 439, -87, -58, 397, -42,  60, 320, 321, 322, 347,
 348, 349,  40,  -1, 321, 320, 327,  -1, -16,  40,
  40, -26, 319, 320, 321, 322, 359, 353, 318, 452,
 347, 348, 349, 350, 357, 358, 288,  93, 125,  44,
  40,  -2,  41, -21,  -9, -21, -22,  -9,  -9,  -9,
  93,  -9,  -9, 372,  40,  -1,  42, 450,  91,  38,
  42, 343, 344, 345,  60,  47, -44,  91, 299, -44,
  -7,  33,  -9, 275, 276, 277, 278, 274, 273, 283,
 279,  43,  40, -35,  40,  41,  -7, -26, -45, 355,
  91,  -9, 263,  61, -70, 123, 123, 123,  -9,  -9,
 123, -45, 341, -50, -51, -60, -23, -24, -25, 269,
 -16,  40,  -9,  58, 268,  -7,  -9,-110,-114,-107,
 303,-114,-114,-114, -45,-107,  -2,  -9,  40,  40,
  41, -26, -45,  -2,  40, -30, -26,  -6,  -2,  -9,
  -9, 125, 309, 309, 442, -30,  -9, -35,  61, -30,
  61, -30, -30,  61,  61,  -1, 445,  -9,  -1, 445,
-126, -90, 307, -41, -43,  -2,  40,  -9, 320, 321,
 322, 347, 348, 349, -26,  91,  -9, -35, -52,  -2,
  40,  40,-122, -35,  41,  41,  93,  41,  41,  41,
  41,  41, -15, 263,  44,  58,  -1, -53, -54, -35,
  93,  93,  -1,  93, -28, -27, 269,  -9,  40,  40,
 -38, -26, -39,  -1,  -1, 450, -44, -26,  -9, 274,
 262, -12, -26, -45,  -2, 268,  40,  41,  44, 125,
 -67, -95, -97, -82, 268,  -7, -45,  -2, 353, 318,
  -7, 353, 318,  -1,  41,  44, -26, -23,  93,  -9,
  -3, 355, 356,  -1, -26,  93,  -2,  -9,  -9, -22,
 -45,  -3,  -1, 268, -35,  41,  40,  41,  44,  44,
  -2,  -9,  -9,  41,  58,  40,  41,  40,  41,  41,
  40,  40,  -1, 310,  -1, -30, -91, 308, -45,  62,
  -2, -40,  44, -45,  41,  -3, 452, 476, 477, 478,
  -9,  41, 372, -52,  41, 369, 332, 333, 334, 370,
 371, 294, 296, 297, 298, 288,  -4, 310, -33, 453,
 455, 456, 457, 458, 270, 271, 275, 276, 277, 278,
 280, 281, 257, 273, 284, 285, 286, 287, 459, 460,
 461, 463, 464, 465, 466, 467, 468, 469, 325, 470,
 274, 283, 327, 471, 332, 462, -99, 372, 475,  -9,
  41, -17, -17, -17, -17, -17, -17, 310, 277, 278,
 433, 434,  -9,  -9,  41,  44,  61,  -5,  -5,  93,
  93,  44, 269, -44, -44,  44,  62,  93,  -1,  42,
  61, -45,  -3, 268, 355, -23, 263, 125, 125,-100,
 400, 401, 402, 403, -75, -87, -88, 125,-101, 405,
 406, 403, -87, -75, -88, 125,  -3, -26,  -2, -26,
 -25,  -2, 452,  41, 372, -45, -34,  61, 309, 309,
  41,  41, 268, -37,  60,  -2,  41, -23,  -6,  -6,
  -9,  -9,  41, -92, -45, -40, -41,  41, -42,  40,
  93,  93,  93,  93, -35,  41,  40, -34,  -2,  41,
  42,  91,  40, 454, 474, 271, 275, 276, 277, 278,
 274, -18, 482, 456, 457, 270, 271, 275, 276, 277,
 278, 280, 281, 273, 284, 285, 286, 287,  42, 459,
 460, 461, 463, 464, 467, 468, 470, 274, 283, 257,
 483, 484, 485, 486, 487, 488, 489, 490, 491, 492,
 493, 494, 495, 469, 461, 473,  41,  -2, 263, 263,
  44, -53, -36, -20,  -9, 277, -35, -44, 312, 313,
  -5, -27,  -9,  41,  41, -26,  93,  40, -35,  40,
 268,  40,  -2,  40,  41,  -7,  -7,  -7,  -7,  -7,
  -7,  -7, 481, -45,  -2,  40,  -2, 268, -32, 280,
 281, 278, 277, 276, 275, 273, 287, 286, 285, 284,
 272, 271, -35,-124, 292,  -2,  -9,  -3,  40, -38,
  41,  58, -58,  44,  40, -33, -52,  -9,  93,  -9,
  43, -35,  91,  91,  44,  91, 496,  38, 275, 276,
 277, 278, 274,  -9,  40,  40, -23,  -3, -23, -23,
 -26, -26, -26, -26, -26, -26, -26,  -7, 268,  40,
 -33,  -3,  40,  40,  40,  40,  40,  40, 278, 277,
 276, 275,  40,  40,  40,  40,  40,  40, -30, -37,
 -23,  62,  -9, -45, -23,  41,  41,  41,  93,  43,
  -9,  44,  -9,  -9, -35,  93, 263,  -9, 275, 276,
 277,  -9,  41,  40,  41,  41, -45,  -3, -45,  -3,
 -45,  -3, -45,  -3, -45,  -3, -45,  -3, -45,  -3,
 -26,  -3, -23,  41,  40, -21, -22, -21, -22, -22,
 -22, -22, -22,  40,  40,  40,  40, -22, -22, -22,
 -22, -22, -20,  41,  40,  41,  58,  41,  -3,  -9,
  93, -35,  93,  93,  41,  58,  58,  58,  41, -23,
 268,  40, 268,  40, 268,  40, 268,  40, 268,  40,
 268,  40, 268,  40, -45,  40,  41,  -2, -23,  41,
  41,  41,  41,  41,  41,  41,  41, -22, -22, -22,
 -22,  41,  41,  41,  41,  41,  41, -23,  -9, -49,
  40,  93,  44,  41,  -9,  -9,  -9,  41,  -3, -23,
  -3, -23,  -3, -23,  -3, -23,  -3, -23,  -3, -23,
  -3, -23, 268, -23, -34,  41,  41,  41,  41,  41,
  41, 123, 316, 361, 362, 363, 301, 364, 365, 366,
 367, 324, 338, 288, -23, -35,  41,  41,  41,  40,
  41,  40,  41,  40,  41,  40,  41,  40,  41,  40,
  41,  40,  41,  -3,  41,  40,  41,  44, -23, -23,
 -23, -23, -23, -23, -23,  40, 481,  -9, -49, -35,
  41,  41,  41,  41,  41,  41,  41, -23, 341,  41,
 123,  41,  41,  -7, -26, -45, 268,  -3,  40, -23,
  41 };
YYSTATIC YYCONST short YYFARDATA YYDEF[]={

   1,  -2,   2,   0,   0, 272,   6,   7,   8,   9,
  10,  11,   0,   0,   0,   0,  16,  17,  18,   0,
   0, 604,   0,  23,  48,   0, 156, 108,   0, 322,
   0,   0, 610, 652,  35,   0, 322,   0, 387, 322,
 322,   0, 153, 277,   0, 589, 590,  78,   1,   0,
 614, 629, 645, 655,  19, 551,  20,   0,   0,  22,
 552,   0, 636,  46, 370, 371, 547, 548, 387, 198,
   0, 274,   0, 280,   0,   0, 322, 283, 283, 283,
 283, 283, 283,   0,   0, 323, 324,   0, 592,   0,
   0,   0,  36,   0,   0, 574, 575, 576, 577, 578,
 579, 580, 581, 582, 583, 584, 585, 586, 587, 588,
   0,   0,  29,   0,   0, 387, 387, 385,   0, 388,
 389,   0,   0,   0,  26, 276, 278,   0,   0,   0,
   5, 273,   0, 229,   0,   0, 233, 234, 235, 236,
   0, 238, 239, 240, 241,   0,   0,   0, 247,   0,
   0, 227, 329,   0,   0,   0,   0, 322,   0, 387,
   0,   0,   0, 387,   0,   0,   0, 549, 272,   0,
 327, 250, 257,   0,   0,   0,   0,  21, 606, 605,
  82,  49,  50,  51,  52,  53,  54,  55,  56,  57,
  58,  59,  60,  61,  62,  63,   0,  70,  71,  72,
   0,   0,   0, 193, 157, 158, 159, 160, 161, 162,
 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
 173,   0,   0,   0,   0,   0, 282,   0,   0,   0,
   0, 294,   0,   0, 295,   0, 296,   0, 297,   0,
 298,   0, 299, 321,  44, 325,   0, 591,   0,   0,
 601, 609, 611, 612, 613, 627, 651, 653, 654,  37,
 557, 379,   0, 382, 376,   0, 498, 499,   0,   0,
 387,   0, 513, 514, 515, 516, 517, 518, 519, 520,
 521, 522,   0, 527, 528, 529, 530,   0, 377, 328,
 558, 559,   0,  24,   0,   0, 118, 119, 387,   0,
   0, 383, 384,   0, 390, 391, 392, 393,  31,  34,
   0,   0,  45,   3,  79, 272,   0,   0,   0,  92,
  93,  94,  95,  96,   0,   0,   0,   0, 102,  48,
 122, 139, 636,   4, 228, 230,  -2,   0, 237,   0,
   0,   0, 328,   0, 251, 253,   0,   0,   0,   0,
   0,   0, 267, 268, 265, 330, 331, 332, 333, 326,
 334, 335, 553,   0,   0,   0, 337, 338,   0,   0,
 343, 344,   0, 322,   0, 348, 349, 350, 542, 352,
   0, 254,   0,   0,  12, 615,   0, 617, 618, 322,
   0,   0, 322, 623,   0,  13, 630, 322, 632, 322,
   0,   0,  14, 646,   0,   0, 650,  15, 656,   0,
   0, 659, 603, 607,  74,   0,  64,  65,  66,  67,
  68,  69,   0, 634, 637, 638,   0, 372,   0,   0,
 178,   0, 199, 200, 201, 202, 203, 204, 205,   0,
 207, 208, 209, 210, 211, 212,   0, 109, 279,   0,
   0,   0, 287,   0,   0,   0,   0,   0,   0,   0,
  39, 594,   0,   0,   0,   0,   0,   0, 537, 504,
 505, 506,   0,   0, 355,   0, 497,   0,   0, 501,
   0,   0, 512, 523, 524, 525, 526, 531,   0, 533,
 534,   0, 560,  30, 110, 387,   0,   0,   0,   0,
 537,   0,  27, 275,   0,  78, 125, 142,  97,  98,
 645,   0, 328,   0, 387,   0,   0, 362, 363, 365,
   0,  -2,   0,   0,   0,   0,   0, 252, 258, 269,
   0, 259, 260, 261, 266, 262, 263, 264,   0,   0,
 336,   0,   0,  -2,   0,   0,   0,   0, 545, 546,
 550, 249,   0,   0,   0,   0,   0, 621,   0,   0,
   0,   0,   0,   0,   0, 647,   0, 649,   0,   0,
 322,  76,   0,   0,   0,  86,   0,   0, 639, 640,
 641, 642, 643, 644,   0,   0,   0, 178,   0, 104,
 394,   0, 281,   0, 286, 283, 284, 283, 283, 283,
 283, 283,   0, 593,   0,   0, 628,   0, 561,   0,
 380,   0,   0, 502,   0, 535, 538, 539,   0,   0,
   0, 356, 357, 378,   0,   0, 500,   0, 511, 532,
  25,  32,   0,   0,  -2,   0,  -2, 386,   0,  88,
   0,   0,   0,   0,   0,   0,   0,  -2, 123, 124,
   0, 140, 141, 635, 231, 193, 366,   0, 242, 244,
 245, 190, 191, 192,   0, 106,   0,   0,   0,   0,
   0, 353,  -2,   0,   0, 346,  -2, 351, 542, 542,
 255, 256, 616, 619,   0, 626, 622, 624, 631, 633,
 608, 625, 648,   0, 658,   0,  47,   0,  75,  83,
  86,  85,   0,   0,  73,  82,   0,   0,   0,   0,
   0, 174,   0,   0, 177, 179, 180, 181, 182, 183,
 184, 185, 186, 187, 188,   0, 106,   0,   0,   0,
   0, 399, 400, 401, 402, 403, 404, 405, 406, 407,
 408, 409, 410,   0, 415, 416, 417, 418, 424, 425,
 426, 427, 428, 429, 430, 431, 432, 433, 434, 448,
 437, 439,   0, 441,   0, 443, 445,   0, 447,   0,
 285, 288, 289, 290, 291, 292, 293,   0,  40,  41,
  42,  43, 596, 598, 556,   0,   0, 373, 374, 381,
 503, 537, 541,   0,   0, 359, 510,   0,   0, 505,
   0,   0,   0,   0,   0,   0,  28,  89,  90, 126,
 387, 387, 387, 387, 135, 136, 137,  91, 143, 387,
 387, 387, 150, 151, 152,  99,   0,   0, 120,   0,
 364, 367,   0, 232,   0,   0, 248,   0,   0,   0,
 554, 555,   0,   0, 355, 341, 345,   0, 543, 544,
   0, 657, 606,  77,  81,  84,  87, 360,   0, 394,
 194, 195, 196, 197, 178, 176,   0, 103, 105, 206,
 419,   0,   0,   0,   0, 444, 411, 412, 413, 414,
 438, 435, 449, 450, 451, 452, 453, 454, 455, 456,
 457, 458, 459,   0, 464, 465, 466, 467, 468, 472,
 473, 474, 475, 476, 477, 478, 479, 480, 482, 483,
 484, 485, 486, 487, 488, 489, 490, 491, 492, 493,
 494, 495, 496, 440, 442, 446, 213,  38, 595, 597,
   0, 562, 563, 566, 567,   0, 569,   0, 564, 565,
 375, 536, 540, 507, 508, 358,   0,  -2,  33, 111,
   0,  -2, 114,  -2, 117,   0,   0,   0,   0,   0,
   0,   0, 387,   0,   0, 394, 243,   0, 107,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0, 318, 322, 320, 270, 271, 353,  -2,   0,
 347,   0, 602,   0,  -2,   0,   0,   0, 420,   0,
   0,   0,   0,   0,   0,   0, 470, 471, 460, 461,
 462, 463, 481, 600,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,  -2,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0, 354,   0,  80,   0,   0, 175, 189, 421,   0,
   0,   0,   0,   0, 436, 469, 599,   0,   0,   0,
   0,   0, 509,  -2, 113, 116,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 368,  -2,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 319,  -2, 340,   0, 214,   0,   0,
 423,   0, 397, 398, 568,   0,   0,   0, 573,   0,
   0,  -2,   0,  -2,   0,  -2,   0,  -2,   0,  -2,
   0,  -2,   0,  -2,   0,  -2, 106, 369,   0, 300,
 302, 301, 303, 304, 305, 306, 307,   0,   0,   0,
   0, 312, 313, 314, 315, 316, 317,   0, 620,   0,
  -2, 422,   0, 396,   0,   0,   0, 112,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0, 138, 246, 308, 309, 310, 311,
 339, 154, 215, 216, 217, 218, 219, 220, 221, 222,
 223, 224, 225,   0,   0,   0, 570, 571, 572,  -2,
 128,  -2, 130,  -2, 132,  -2, 134,  -2, 145,  -2,
 147,  -2, 149,   0,   0,   0, 214,   0,   0,   0,
   0,   0,   0,   0,   0,  -2,   0,   0,   0,   0,
 127, 129, 131, 133, 144, 146, 148,   0, 387, 226,
 155, 395, 100,   0,   0,   0,   0,   0,  -2,   0,
 101 };
#ifdef YYRECOVER
YYSTATIC YYCONST short yyrecover[] = {
-1000
};
#endif

/* SCCSWHAT( "@(#)yypars.c	3.1 88/11/16 22:00:49	" ) */
#line 3 "yypars.c"
#if ! defined(YYAPI_PACKAGE)
/*
**  YYAPI_TOKENNAME		: name used for return value of yylex	
**	YYAPI_TOKENTYPE		: type of the token
**	YYAPI_TOKENEME(t)	: the value of the token that the parser should see
**	YYAPI_TOKENNONE		: the representation when there is no token
**	YYAPI_VALUENAME		: the name of the value of the token
**	YYAPI_VALUETYPE		: the type of the value of the token (if null, then the value is derivable from the token itself)
**	YYAPI_VALUEOF(v)	: how to get the value of the token.
*/
#define	YYAPI_TOKENNAME		yychar
#define	YYAPI_TOKENTYPE		int
#define	YYAPI_TOKENEME(t)	(t)
#define	YYAPI_TOKENNONE		-1
#define	YYAPI_TOKENSTR(t)	(sprintf(yytokbuf, "%d", t), yytokbuf)
#define	YYAPI_VALUENAME		yylval
#define	YYAPI_VALUETYPE		YYSTYPE
#define	YYAPI_VALUEOF(v)	(v)
#endif
#if ! defined(YYAPI_CALLAFTERYYLEX)
#define	YYAPI_CALLAFTERYYLEX(x)
#endif

# define YYFLAG -1000
# define YYERROR goto yyerrlab
# define YYACCEPT return(0)
# define YYABORT return(1)

#ifdef YYDEBUG				/* RRR - 10/9/85 */
char yytokbuf[20];
# ifndef YYDBFLG
#  define YYDBFLG (yydebug)
# endif
# define yyprintf(a, b, c, d) if (YYDBFLG) YYPRINT(a, b, c, d)
#else
# define yyprintf(a, b, c, d)
#endif

#ifndef YYPRINT
#define	YYPRINT	printf
#endif

/*	parser for yacc output	*/

#ifdef YYDUMP
int yydump = 1; /* 1 for dumping */
void yydumpinfo(void);
#endif
#ifdef YYDEBUG
YYSTATIC int yydebug = 0; /* 1 for debugging */
#endif
YYSTATIC YYSTYPE yyv[YYMAXDEPTH];	/* where the values are stored */
YYSTATIC short	yys[YYMAXDEPTH];	/* the parse stack */

#if ! defined(YYRECURSIVE)
YYSTATIC YYAPI_TOKENTYPE	YYAPI_TOKENNAME = YYAPI_TOKENNONE;
#if defined(YYAPI_VALUETYPE)
// YYSTATIC YYAPI_VALUETYPE	YYAPI_VALUENAME;	 FIX 
#endif
YYSTATIC int yynerrs = 0;			/* number of errors */
YYSTATIC short yyerrflag = 0;		/* error recovery flag */
#endif

#ifdef YYRECOVER
/*
**  yyscpy : copy f onto t and return a ptr to the null terminator at the
**  end of t.
*/
YYSTATIC	char	*yyscpy(register char*t, register char*f)
	{
	while(*t = *f++)
		t++;
	return(t);	/*  ptr to the null char  */
	}
#endif

#ifndef YYNEAR
#define YYNEAR
#endif
#ifndef YYPASCAL
#define YYPASCAL
#endif
#ifndef YYLOCAL
#define YYLOCAL
#endif
#if ! defined YYPARSER
#define YYPARSER yyparse
#endif
#if ! defined YYLEX
#define YYLEX yylex
#endif

#if defined(YYRECURSIVE)

	YYSTATIC YYAPI_TOKENTYPE	YYAPI_TOKENNAME = YYAPI_TOKENNONE;
  #if defined(YYAPI_VALUETYPE)
	YYSTATIC YYAPI_VALUETYPE	YYAPI_VALUENAME;  
  #endif
	YYSTATIC int yynerrs = 0;			/* number of errors */
	YYSTATIC short yyerrflag = 0;		/* error recovery flag */

	YYSTATIC short	yyn;
	YYSTATIC short	yystate = 0;
	YYSTATIC short	*yyps= &yys[-1];
	YYSTATIC YYSTYPE	*yypv= &yyv[-1];
	YYSTATIC short	yyj;
	YYSTATIC short	yym;

#endif

#ifdef _MSC_VER
#pragma warning(disable:102)
#endif

YYLOCAL int YYNEAR YYPASCAL YYPARSER()
{
#if ! defined(YYRECURSIVE)

	register	short	yyn;
	short		yystate, *yyps;
	YYSTYPE		*yypv;
	short		yyj, yym;

	YYAPI_TOKENNAME = YYAPI_TOKENNONE;
	yystate = 0;
	yyps= &yys[-1];
	yypv= &yyv[-1];
#endif

#ifdef YYDUMP
	yydumpinfo();
#endif
 yystack:	 /* put a state and value onto the stack */

#ifdef YYDEBUG
	if(YYAPI_TOKENNAME == YYAPI_TOKENNONE) {
		yyprintf( "state %d, token # '%d'\n", yystate, -1, 0 );
		}
	else {
		yyprintf( "state %d, token # '%s'\n", yystate, YYAPI_TOKENSTR(YYAPI_TOKENNAME), 0 );
		}
#endif
	if( ++yyps > &yys[YYMAXDEPTH] ) {
		yyerror( "yacc stack overflow" );
		return(1);
	}
	*yyps = yystate;
	++yypv;

	*yypv = yyval;

yynewstate:

	yyn = YYPACT[yystate];

	if( yyn <= YYFLAG ) {	/*  simple state, no lookahead  */
		goto yydefault;
	}
	if( YYAPI_TOKENNAME == YYAPI_TOKENNONE ) {	/*  need a lookahead */
		YYAPI_TOKENNAME = YYLEX();
		YYAPI_CALLAFTERYYLEX(YYAPI_TOKENNAME);
	}
	if( ((yyn += YYAPI_TOKENEME(YYAPI_TOKENNAME)) < 0) || (yyn >= YYLAST) ) {
		goto yydefault;
	}
	if( YYCHK[ yyn = YYACT[ yyn ] ] == YYAPI_TOKENEME(YYAPI_TOKENNAME) ) {		/* valid shift */
		yyval = YYAPI_VALUEOF(YYAPI_VALUENAME);
		yystate = yyn;
 		yyprintf( "SHIFT: saw token '%s', now in state %4d\n", YYAPI_TOKENSTR(YYAPI_TOKENNAME), yystate, 0 );
		YYAPI_TOKENNAME = YYAPI_TOKENNONE;
		if( yyerrflag > 0 ) {
			--yyerrflag;
		}
		goto yystack;
	}

 yydefault:
	/* default state action */

	if( (yyn = YYDEF[yystate]) == -2 ) {
		register	YYCONST short	*yyxi;

		if( YYAPI_TOKENNAME == YYAPI_TOKENNONE ) {
			YYAPI_TOKENNAME = YYLEX();
			YYAPI_CALLAFTERYYLEX(YYAPI_TOKENNAME);
 			yyprintf("LOOKAHEAD: token '%s'\n", YYAPI_TOKENSTR(YYAPI_TOKENNAME), 0, 0);
		}
/*
**  search exception table, we find a -1 followed by the current state.
**  if we find one, we'll look through terminal,state pairs. if we find
**  a terminal which matches the current one, we have a match.
**  the exception table is when we have a reduce on a terminal.
*/

#if YYOPTTIME
		yyxi = yyexca + yyexcaind[yystate];
		while(( *yyxi != YYAPI_TOKENEME(YYAPI_TOKENNAME) ) && ( *yyxi >= 0 )){
			yyxi += 2;
		}
#else
		for(yyxi = yyexca;
			(*yyxi != (-1)) || (yyxi[1] != yystate);
			yyxi += 2
		) {
			; /* VOID */
			}

		while( *(yyxi += 2) >= 0 ){
			if( *yyxi == YYAPI_TOKENEME(YYAPI_TOKENNAME) ) {
				break;
				}
		}
#endif
		if( (yyn = yyxi[1]) < 0 ) {
			return(0);   /* accept */
			}
		}

	if( yyn == 0 ){ /* error */
		/* error ... attempt to resume parsing */

		switch( yyerrflag ){

		case 0:		/* brand new error */
#ifdef YYRECOVER
			{
			register	int		i,j;

			for(i = 0;
				(yyrecover[i] != -1000) && (yystate > yyrecover[i]);
				i += 3
			) {
				;
			}
			if(yystate == yyrecover[i]) {
				yyprintf("recovered, from state %d to state %d on token # %d\n",
						yystate,yyrecover[i+2],yyrecover[i+1]
						);
				j = yyrecover[i + 1];
				if(j < 0) {
				/*
				**  here we have one of the injection set, so we're not quite
				**  sure that the next valid thing will be a shift. so we'll
				**  count it as an error and continue.
				**  actually we're not absolutely sure that the next token
				**  we were supposed to get is the one when j > 0. for example,
				**  for(+) {;} error recovery with yyerrflag always set, stops
				**  after inserting one ; before the +. at the point of the +,
				**  we're pretty sure the guy wants a 'for' loop. without
				**  setting the flag, when we're almost absolutely sure, we'll
				**  give him one, since the only thing we can shift on this
				**  error is after finding an expression followed by a +
				*/
					yyerrflag++;
					j = -j;
					}
				if(yyerrflag <= 1) {	/*  only on first insertion  */
					yyrecerr(YYAPI_TOKENNAME, j);	/*  what was, what should be first */
				}
				yyval = yyeval(j);
				yystate = yyrecover[i + 2];
				goto yystack;
				}
			}
#endif
		yyerror("syntax error");

		yyerrlab:
			++yynerrs;

		case 1:
		case 2: /* incompletely recovered error ... try again */

			yyerrflag = 3;

			/* find a state where "error" is a legal shift action */

			while ( yyps >= yys ) {
			   yyn = YYPACT[*yyps] + YYERRCODE;
			   if( yyn>= 0 && yyn < YYLAST && YYCHK[YYACT[yyn]] == YYERRCODE ){
			      yystate = YYACT[yyn];  /* simulate a shift of "error" */
 				  yyprintf( "SHIFT 'error': now in state %4d\n", yystate, 0, 0 );
			      goto yystack;
			      }
			   yyn = YYPACT[*yyps];

			   /* the current yyps has no shift onn "error", pop stack */

 			   yyprintf( "error recovery pops state %4d, uncovers %4d\n", *yyps, yyps[-1], 0 );
			   --yyps;
			   --yypv;
			   }

			/* there is no state on the stack with an error shift ... abort */

	yyabort:
			return(1);


		case 3:  /* no shift yet; clobber input char */

 			yyprintf( "error recovery discards token '%s'\n", YYAPI_TOKENSTR(YYAPI_TOKENNAME), 0, 0 );

			if( YYAPI_TOKENEME(YYAPI_TOKENNAME) == 0 ) goto yyabort; /* don't discard EOF, quit */
			YYAPI_TOKENNAME = YYAPI_TOKENNONE;
			goto yynewstate;   /* try again in the same state */
			}
		}

	/* reduction by production yyn */
		{
		register	YYSTYPE	*yypvt;
		yypvt = yypv;
		yyps -= YYR2[yyn];
		yypv -= YYR2[yyn];
		yyval = yypv[1];
 		yyprintf("REDUCE: rule %4d, popped %2d tokens, uncovered state %4d, ",yyn, YYR2[yyn], *yyps);
		yym = yyn;
		yyn = YYR1[yyn];		/* consult goto table to find next state */
		yyj = YYPGO[yyn] + *yyps + 1;
		if( (yyj >= YYLAST) || (YYCHK[ yystate = YYACT[yyj] ] != -yyn) ) {
			yystate = YYACT[YYPGO[yyn]];
			}
 		yyprintf("goto state %4d\n", yystate, 0, 0);
#ifdef YYDUMP
		yydumpinfo();
#endif
		switch(yym){
			
case 3:
#line 194 "asmparse.y"
{ PASM->EndClass(); } break;
case 4:
#line 195 "asmparse.y"
{ PASM->EndNameSpace(); } break;
case 5:
#line 196 "asmparse.y"
{ if(PASM->m_pCurMethod->m_ulLines[1] ==0)
																				  {  PASM->m_pCurMethod->m_ulLines[1] = PASM->m_ulCurLine;
																					 PASM->m_pCurMethod->m_ulColumns[1]=PASM->m_ulCurColumn;}
														  						  PASM->EndMethod(); } break;
case 12:
#line 206 "asmparse.y"
{ PASMM->EndAssembly(); } break;
case 13:
#line 207 "asmparse.y"
{ PASMM->EndAssembly(); } break;
case 14:
#line 208 "asmparse.y"
{ PASMM->EndComType(); } break;
case 15:
#line 209 "asmparse.y"
{ PASMM->EndManifestRes(); } break;
case 19:
#line 213 "asmparse.y"
{ PASM->m_dwSubsystem = yypvt[-0].int32; } break;
case 20:
#line 214 "asmparse.y"
{ PASM->m_dwComImageFlags = yypvt[-0].int32; } break;
case 21:
#line 215 "asmparse.y"
{ PASM->m_dwFileAlignment = yypvt[-0].int32; } break;
case 22:
#line 216 "asmparse.y"
{ PASM->m_stBaseAddress = (size_t)(*(yypvt[-0].int64)); delete yypvt[-0].int64; } break;
case 24:
#line 220 "asmparse.y"
{ yyval.binstr = yypvt[-0].binstr; } break;
case 25:
#line 221 "asmparse.y"
{ yyval.binstr = yypvt[-2].binstr; yyval.binstr->append(yypvt[-0].binstr); delete yypvt[-0].binstr; } break;
case 26:
#line 224 "asmparse.y"
{ LPCSTRToGuid(yypvt[-0].string,&(PASM->m_guidLang)); } break;
case 27:
#line 225 "asmparse.y"
{ LPCSTRToGuid(yypvt[-2].string,&(PASM->m_guidLang)); 
						                                                          LPCSTRToGuid(yypvt[-0].string,&(PASM->m_guidLangVendor));} break;
case 28:
#line 227 "asmparse.y"
{ LPCSTRToGuid(yypvt[-4].string,&(PASM->m_guidLang)); 
						                                                          LPCSTRToGuid(yypvt[-2].string,&(PASM->m_guidLangVendor));
						                                                          LPCSTRToGuid(yypvt[-2].string,&(PASM->m_guidDoc));} break;
case 29:
#line 232 "asmparse.y"
{ if(PASM->m_tkCurrentCVOwner) 
                                                                                    PASM->DefineCV(new CustomDescr(PASM->m_tkCurrentCVOwner, yypvt[-0].int32, NULL));
                                                                                  else if(PASM->m_pCustomDescrList)
                                                                                    PASM->m_pCustomDescrList->PUSH(new CustomDescr(yypvt[-0].int32, NULL)); } break;
case 30:
#line 236 "asmparse.y"
{ if(PASM->m_tkCurrentCVOwner) 
                                                                                    PASM->DefineCV(new CustomDescr(PASM->m_tkCurrentCVOwner, yypvt[-2].int32, yypvt[-0].binstr));
                                                                                  else if(PASM->m_pCustomDescrList)
                                                                                    PASM->m_pCustomDescrList->PUSH(new CustomDescr(yypvt[-2].int32, yypvt[-0].binstr)); } break;
case 31:
#line 240 "asmparse.y"
{ if(PASM->m_tkCurrentCVOwner) 
                                                                                    PASM->DefineCV(new CustomDescr(PASM->m_tkCurrentCVOwner, yypvt[-2].int32, yypvt[-1].binstr));
                                                                                   else if(PASM->m_pCustomDescrList)
                                                                                    PASM->m_pCustomDescrList->PUSH(new CustomDescr(yypvt[-2].int32, yypvt[-1].binstr)); } break;
case 32:
#line 244 "asmparse.y"
{ PASM->DefineCV(new CustomDescr(yypvt[-2].int32, yypvt[-0].int32, NULL)); } break;
case 33:
#line 245 "asmparse.y"
{ PASM->DefineCV(new CustomDescr(yypvt[-4].int32, yypvt[-2].int32, yypvt[-0].binstr)); } break;
case 34:
#line 246 "asmparse.y"
{ PASM->DefineCV(new CustomDescr(PASM->m_tkCurrentCVOwner, yypvt[-2].int32, yypvt[-1].binstr)); } break;
case 35:
#line 249 "asmparse.y"
{ PASMM->SetModuleName(NULL); PASM->m_tkCurrentCVOwner=1; } break;
case 36:
#line 250 "asmparse.y"
{ PASMM->SetModuleName(yypvt[-0].string); PASM->m_tkCurrentCVOwner=1; } break;
case 37:
#line 251 "asmparse.y"
{ BinStr* pbs = new BinStr();
						                                                          strcpy((char*)(pbs->getBuff((unsigned)strlen(yypvt[-0].string)+1)),yypvt[-0].string);
																				  PASM->EmitImport(pbs); delete pbs;} break;
case 38:
#line 256 "asmparse.y"
{ /*PASM->SetDataSection(); PASM->EmitDataLabel($7);*/
                                                                                  PASM->m_VTFList.PUSH(new VTFEntry((USHORT)yypvt[-4].int32, (USHORT)yypvt[-2].int32, yypvt[-0].string)); } break;
case 39:
#line 260 "asmparse.y"
{ yyval.int32 = 0; } break;
case 40:
#line 261 "asmparse.y"
{ yyval.int32 = yypvt[-1].int32 | COR_VTABLE_32BIT; } break;
case 41:
#line 262 "asmparse.y"
{ yyval.int32 = yypvt[-1].int32 | COR_VTABLE_64BIT; } break;
case 42:
#line 263 "asmparse.y"
{ yyval.int32 = yypvt[-1].int32 | COR_VTABLE_FROM_UNMANAGED; } break;
case 43:
#line 264 "asmparse.y"
{ yyval.int32 = yypvt[-1].int32 | COR_VTABLE_CALL_MOST_DERIVED; } break;
case 44:
#line 267 "asmparse.y"
{ PASM->m_pVTable = yypvt[-1].binstr; } break;
case 45:
#line 270 "asmparse.y"
{ bParsingByteArray = TRUE; } break;
case 46:
#line 273 "asmparse.y"
{ PASM->StartNameSpace(yypvt[-0].string); } break;
case 47:
#line 277 "asmparse.y"
{ PASM->StartClass(yypvt[-3].string, yypvt[-4].classAttr, yypvt[-2].typarlist); } break;
case 48:
#line 280 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) 0; } break;
case 49:
#line 281 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-1].classAttr & ~tdVisibilityMask) | tdPublic); } break;
case 50:
#line 282 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-1].classAttr & ~tdVisibilityMask) | tdNotPublic); } break;
case 51:
#line 283 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].classAttr | 0x80000000); } break;
case 52:
#line 284 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].classAttr | 0x40000000); } break;
case 53:
#line 285 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].classAttr | tdInterface | tdAbstract); } break;
case 54:
#line 286 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].classAttr | tdSealed); } break;
case 55:
#line 287 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].classAttr | tdAbstract); } break;
case 56:
#line 288 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-1].classAttr & ~tdLayoutMask) | tdAutoLayout); } break;
case 57:
#line 289 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-1].classAttr & ~tdLayoutMask) | tdSequentialLayout); } break;
case 58:
#line 290 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-1].classAttr & ~tdLayoutMask) | tdExplicitLayout); } break;
case 59:
#line 291 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-1].classAttr & ~tdStringFormatMask) | tdAnsiClass); } break;
case 60:
#line 292 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-1].classAttr & ~tdStringFormatMask) | tdUnicodeClass); } break;
case 61:
#line 293 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-1].classAttr & ~tdStringFormatMask) | tdAutoClass); } break;
case 62:
#line 294 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].classAttr | tdImport); } break;
case 63:
#line 295 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].classAttr | tdSerializable); } break;
case 64:
#line 296 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-2].classAttr & ~tdVisibilityMask) | tdNestedPublic); } break;
case 65:
#line 297 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-2].classAttr & ~tdVisibilityMask) | tdNestedPrivate); } break;
case 66:
#line 298 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-2].classAttr & ~tdVisibilityMask) | tdNestedFamily); } break;
case 67:
#line 299 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-2].classAttr & ~tdVisibilityMask) | tdNestedAssembly); } break;
case 68:
#line 300 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-2].classAttr & ~tdVisibilityMask) | tdNestedFamANDAssem); } break;
case 69:
#line 301 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) ((yypvt[-2].classAttr & ~tdVisibilityMask) | tdNestedFamORAssem); } break;
case 70:
#line 302 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].classAttr | tdBeforeFieldInit); } break;
case 71:
#line 303 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].classAttr | tdSpecialName); } break;
case 72:
#line 304 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].classAttr); } break;
case 73:
#line 305 "asmparse.y"
{ yyval.classAttr = (CorRegTypeAttr) (yypvt[-1].int32); } break;
case 75:
#line 309 "asmparse.y"
{ PASM->m_crExtends = yypvt[-0].token; } break;
case 80:
#line 320 "asmparse.y"
{ PASM->AddToImplList(yypvt[-0].token); } break;
case 81:
#line 321 "asmparse.y"
{ PASM->AddToImplList(yypvt[-0].token); } break;
case 82:
#line 324 "asmparse.y"
{ yyval.typarlist = NULL; } break;
case 83:
#line 325 "asmparse.y"
{ yyval.typarlist = yypvt[-1].typarlist; } break;
case 84:
#line 328 "asmparse.y"
{ yyval.typarlist = new TyParList(yypvt[-2].token, yypvt[-1].string, yypvt[-0].typarlist); } break;
case 85:
#line 329 "asmparse.y"
{ yyval.typarlist = new TyParList(NULL, yypvt[-1].string, yypvt[-0].typarlist); } break;
case 86:
#line 332 "asmparse.y"
{ yyval.typarlist = NULL; } break;
case 87:
#line 333 "asmparse.y"
{ yyval.typarlist = yypvt[-0].typarlist; } break;
case 88:
#line 336 "asmparse.y"
{ if(PASM->m_pCurMethod->m_ulLines[1] ==0)
															  {  PASM->m_pCurMethod->m_ulLines[1] = PASM->m_ulCurLine;
																 PASM->m_pCurMethod->m_ulColumns[1]=PASM->m_ulCurColumn;}
															  PASM->EndMethod(); } break;
case 89:
#line 340 "asmparse.y"
{ PASM->EndClass(); } break;
case 90:
#line 341 "asmparse.y"
{ PASM->EndEvent(); } break;
case 91:
#line 342 "asmparse.y"
{ PASM->EndProp(); } break;
case 97:
#line 348 "asmparse.y"
{ PASM->m_pCurClass->m_ulSize = yypvt[-0].int32; } break;
case 98:
#line 349 "asmparse.y"
{ PASM->m_pCurClass->m_ulPack = yypvt[-0].int32; } break;
case 99:
#line 350 "asmparse.y"
{ PASMM->EndComType(); } break;
case 100:
#line 352 "asmparse.y"
{ BinStr *sig1 = parser->MakeSig(yypvt[-7].int32, yypvt[-6].binstr, yypvt[-1].binstr); 
                                                                  BinStr *sig2 = new BinStr(); sig2->append(sig1); 
                                                                  PASM->AddMethodImpl(yypvt[-11].token,yypvt[-9].string,sig1,yypvt[-5].token,yypvt[-3].string,sig2); } break;
case 101:
#line 356 "asmparse.y"
{ PASM->AddMethodImpl(yypvt[-15].token,yypvt[-13].string,
                                                                      parser->MakeSig(yypvt[-17].int32,yypvt[-16].binstr,yypvt[-11].binstr),yypvt[-5].token,yypvt[-3].string,
                                                                      parser->MakeSig(yypvt[-7].int32,yypvt[-6].binstr,yypvt[-1].binstr)); } break;
case 103:
#line 363 "asmparse.y"
{ yypvt[-3].binstr->insertInt8(IMAGE_CEE_CS_CALLCONV_FIELD);
                                                              PASM->AddField(yypvt[-2].string, yypvt[-3].binstr, yypvt[-4].fieldAttr, yypvt[-1].string, yypvt[-0].binstr, yypvt[-5].int32); } break;
case 104:
#line 368 "asmparse.y"
{ yyval.string = 0; } break;
case 105:
#line 369 "asmparse.y"
{ yyval.string = yypvt[-0].string; } break;
case 106:
#line 372 "asmparse.y"
{ yyval.binstr = NULL; } break;
case 107:
#line 373 "asmparse.y"
{ yyval.binstr = yypvt[-0].binstr; } break;
case 108:
#line 376 "asmparse.y"
{ yyval.int32 = 0xFFFFFFFF; } break;
case 109:
#line 377 "asmparse.y"
{ yyval.int32 = yypvt[-1].int32; } break;
case 110:
#line 380 "asmparse.y"
{ yyval.int32 = yypvt[-2].int32; bParsingByteArray = TRUE; } break;
case 111:
#line 384 "asmparse.y"
{ PASM->m_pCustomDescrList = NULL;
															  PASM->m_tkCurrentCVOwner = yypvt[-4].int32;
															  yyval.int32 = yypvt[-2].int32; bParsingByteArray = TRUE; } break;
case 112:
#line 390 "asmparse.y"
{ yyval.int32 = PASM->MakeMemberRef(yypvt[-5].token, yypvt[-3].string, parser->MakeSig(yypvt[-7].int32, yypvt[-6].binstr, yypvt[-1].binstr),iOpcodeLen); 
                                                               PASM->delArgNameList(PASM->m_firstArgName);
                                                               PASM->m_firstArgName = palDummyFirst;
                                                               PASM->m_lastArgName = palDummyLast;
															} break;
case 113:
#line 396 "asmparse.y"
{ yyval.int32 = PASM->MakeMemberRef(mdTokenNil, yypvt[-3].string, parser->MakeSig(yypvt[-5].int32, yypvt[-4].binstr, yypvt[-1].binstr),iOpcodeLen); 
                                                               PASM->delArgNameList(PASM->m_firstArgName);
                                                               PASM->m_firstArgName = palDummyFirst;
                                                               PASM->m_lastArgName = palDummyLast;
															} break;
case 114:
#line 402 "asmparse.y"
{ yypvt[-3].binstr->insertInt8(IMAGE_CEE_CS_CALLCONV_FIELD); 
                                                               yyval.int32 = PASM->MakeMemberRef(yypvt[-2].token, yypvt[-0].string, yypvt[-3].binstr, 0); } break;
case 115:
#line 405 "asmparse.y"
{ yypvt[-1].binstr->insertInt8(IMAGE_CEE_CS_CALLCONV_FIELD); 
                                                               yyval.int32 = PASM->MakeMemberRef(NULL, yypvt[-0].string, yypvt[-1].binstr, 0); } break;
case 116:
#line 410 "asmparse.y"
{ yyval.int32 = PASM->MakeMemberRef(yypvt[-5].token, newString(COR_CTOR_METHOD_NAME), parser->MakeSig(yypvt[-7].int32, yypvt[-6].binstr, yypvt[-1].binstr),0); } break;
case 117:
#line 412 "asmparse.y"
{ yyval.int32 = PASM->MakeMemberRef(mdTokenNil, newString(COR_CTOR_METHOD_NAME), parser->MakeSig(yypvt[-5].int32, yypvt[-4].binstr, yypvt[-1].binstr),0); } break;
case 118:
#line 415 "asmparse.y"
{ yyval.int32 = yypvt[-0].token; } break;
case 119:
#line 416 "asmparse.y"
{ yyval.int32 = yypvt[-0].int32; } break;
case 120:
#line 419 "asmparse.y"
{ PASM->ResetEvent(yypvt[-0].string, yypvt[-1].token, yypvt[-2].eventAttr); } break;
case 121:
#line 420 "asmparse.y"
{ PASM->ResetEvent(yypvt[-0].string, mdTypeRefNil, yypvt[-1].eventAttr); } break;
case 122:
#line 424 "asmparse.y"
{ yyval.eventAttr = (CorEventAttr) 0; } break;
case 123:
#line 425 "asmparse.y"
{ yyval.eventAttr = yypvt[-1].eventAttr; } break;
case 124:
#line 426 "asmparse.y"
{ yyval.eventAttr = (CorEventAttr) (yypvt[-1].eventAttr | evSpecialName); } break;
case 127:
#line 434 "asmparse.y"
{ PASM->SetEventMethod(0, yypvt[-5].token, yypvt[-3].string, parser->MakeSig(yypvt[-7].int32, yypvt[-6].binstr, yypvt[-1].binstr)); } break;
case 128:
#line 436 "asmparse.y"
{ PASM->SetEventMethod(0, NULL, yypvt[-3].string, parser->MakeSig(yypvt[-5].int32, yypvt[-4].binstr, yypvt[-1].binstr)); } break;
case 129:
#line 438 "asmparse.y"
{ PASM->SetEventMethod(1, yypvt[-5].token, yypvt[-3].string, parser->MakeSig(yypvt[-7].int32, yypvt[-6].binstr, yypvt[-1].binstr)); } break;
case 130:
#line 440 "asmparse.y"
{ PASM->SetEventMethod(1, NULL, yypvt[-3].string, parser->MakeSig(yypvt[-5].int32, yypvt[-4].binstr, yypvt[-1].binstr)); } break;
case 131:
#line 442 "asmparse.y"
{ PASM->SetEventMethod(2, yypvt[-5].token, yypvt[-3].string, parser->MakeSig(yypvt[-7].int32, yypvt[-6].binstr, yypvt[-1].binstr)); } break;
case 132:
#line 444 "asmparse.y"
{ PASM->SetEventMethod(2, NULL, yypvt[-3].string, parser->MakeSig(yypvt[-5].int32, yypvt[-4].binstr, yypvt[-1].binstr)); } break;
case 133:
#line 446 "asmparse.y"
{ PASM->SetEventMethod(3, yypvt[-5].token, yypvt[-3].string, parser->MakeSig(yypvt[-7].int32, yypvt[-6].binstr, yypvt[-1].binstr)); } break;
case 134:
#line 448 "asmparse.y"
{ PASM->SetEventMethod(3, NULL, yypvt[-3].string, parser->MakeSig(yypvt[-5].int32, yypvt[-4].binstr, yypvt[-1].binstr)); } break;
case 138:
#line 455 "asmparse.y"
{ PASM->ResetProp(yypvt[-4].string, 
                                                              parser->MakeSig((IMAGE_CEE_CS_CALLCONV_PROPERTY |
                                                              (yypvt[-6].int32 & IMAGE_CEE_CS_CALLCONV_HASTHIS)),yypvt[-5].binstr,yypvt[-2].binstr), yypvt[-7].propAttr, yypvt[-0].binstr); } break;
case 139:
#line 460 "asmparse.y"
{ yyval.propAttr = (CorPropertyAttr) 0; } break;
case 140:
#line 461 "asmparse.y"
{ yyval.propAttr = yypvt[-1].propAttr; } break;
case 141:
#line 462 "asmparse.y"
{ yyval.propAttr = (CorPropertyAttr) (yypvt[-1].propAttr | prSpecialName); } break;
case 144:
#line 471 "asmparse.y"
{ PASM->SetPropMethod(0, yypvt[-5].token, yypvt[-3].string, parser->MakeSig(yypvt[-7].int32, yypvt[-6].binstr, yypvt[-1].binstr)); } break;
case 145:
#line 473 "asmparse.y"
{ PASM->SetPropMethod(0, NULL, yypvt[-3].string, parser->MakeSig(yypvt[-5].int32, yypvt[-4].binstr, yypvt[-1].binstr)); } break;
case 146:
#line 475 "asmparse.y"
{ PASM->SetPropMethod(1, yypvt[-5].token, yypvt[-3].string, parser->MakeSig(yypvt[-7].int32, yypvt[-6].binstr, yypvt[-1].binstr)); } break;
case 147:
#line 477 "asmparse.y"
{ PASM->SetPropMethod(1, NULL, yypvt[-3].string, parser->MakeSig(yypvt[-5].int32, yypvt[-4].binstr, yypvt[-1].binstr)); } break;
case 148:
#line 479 "asmparse.y"
{ PASM->SetPropMethod(2, yypvt[-5].token, yypvt[-3].string, parser->MakeSig(yypvt[-7].int32, yypvt[-6].binstr, yypvt[-1].binstr)); } break;
case 149:
#line 481 "asmparse.y"
{ PASM->SetPropMethod(2, NULL, yypvt[-3].string, parser->MakeSig(yypvt[-5].int32, yypvt[-4].binstr, yypvt[-1].binstr)); } break;
case 153:
#line 488 "asmparse.y"
{ PASM->ResetForNextMethod(); 
															  uMethodBeginLine = PASM->m_ulCurLine;
															  uMethodBeginColumn=PASM->m_ulCurColumn;} break;
case 154:
#line 494 "asmparse.y"
{ BinStr* sig;
							                                  if (yypvt[-5].typarlist == NULL) sig = parser->MakeSig(yypvt[-9].int32, yypvt[-7].binstr, yypvt[-3].binstr);
							                                  else sig = parser->MakeSig(yypvt[-9].int32 | IMAGE_CEE_CS_CALLCONV_GENERIC, yypvt[-7].binstr, yypvt[-3].binstr, yypvt[-5].typarlist->Count());
							                                  PASM->StartMethod(yypvt[-6].string, sig, yypvt[-10].methAttr, NULL, yypvt[-8].int32, yypvt[-5].typarlist);
                                                              PASM->SetImplAttr((USHORT)yypvt[-1].implAttr);  
															  PASM->m_pCurMethod->m_ulLines[0] = uMethodBeginLine;
															  PASM->m_pCurMethod->m_ulColumns[0]=uMethodBeginColumn; } break;
case 155:
#line 502 "asmparse.y"
{ PASM->StartMethod(yypvt[-5].string, parser->MakeSig(yypvt[-12].int32, yypvt[-10].binstr, yypvt[-3].binstr), yypvt[-13].methAttr, yypvt[-7].binstr, yypvt[-11].int32);
                                                              PASM->SetImplAttr((USHORT)yypvt[-1].implAttr);
															  PASM->m_pCurMethod->m_ulLines[0] = uMethodBeginLine;
															  PASM->m_pCurMethod->m_ulColumns[0]=uMethodBeginColumn; } break;
case 156:
#line 509 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) 0; } break;
case 157:
#line 510 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) (yypvt[-1].methAttr | mdStatic); } break;
case 158:
#line 511 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) ((yypvt[-1].methAttr & ~mdMemberAccessMask) | mdPublic); } break;
case 159:
#line 512 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) ((yypvt[-1].methAttr & ~mdMemberAccessMask) | mdPrivate); } break;
case 160:
#line 513 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) ((yypvt[-1].methAttr & ~mdMemberAccessMask) | mdFamily); } break;
case 161:
#line 514 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) (yypvt[-1].methAttr | mdFinal); } break;
case 162:
#line 515 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) (yypvt[-1].methAttr | mdSpecialName); } break;
case 163:
#line 516 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) (yypvt[-1].methAttr | mdVirtual); } break;
case 164:
#line 517 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) (yypvt[-1].methAttr | mdAbstract); } break;
case 165:
#line 518 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) ((yypvt[-1].methAttr & ~mdMemberAccessMask) | mdAssem); } break;
case 166:
#line 519 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) ((yypvt[-1].methAttr & ~mdMemberAccessMask) | mdFamANDAssem); } break;
case 167:
#line 520 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) ((yypvt[-1].methAttr & ~mdMemberAccessMask) | mdFamORAssem); } break;
case 168:
#line 521 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) ((yypvt[-1].methAttr & ~mdMemberAccessMask) | mdPrivateScope); } break;
case 169:
#line 522 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) (yypvt[-1].methAttr | mdHideBySig); } break;
case 170:
#line 523 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) (yypvt[-1].methAttr | mdNewSlot); } break;
case 171:
#line 524 "asmparse.y"
{ yyval.methAttr = yypvt[-1].methAttr; } break;
case 172:
#line 525 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) (yypvt[-1].methAttr | mdUnmanagedExport); } break;
case 173:
#line 526 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) (yypvt[-1].methAttr | mdRequireSecObject); } break;
case 174:
#line 527 "asmparse.y"
{ yyval.methAttr = (CorMethodAttr) (yypvt[-1].int32); } break;
case 175:
#line 529 "asmparse.y"
{ PASM->SetPinvoke(yypvt[-4].binstr,0,yypvt[-2].binstr,yypvt[-1].pinvAttr); 
                                                              yyval.methAttr = (CorMethodAttr) (yypvt[-7].methAttr | mdPinvokeImpl); } break;
case 176:
#line 532 "asmparse.y"
{ PASM->SetPinvoke(yypvt[-2].binstr,0,NULL,yypvt[-1].pinvAttr); 
                                                              yyval.methAttr = (CorMethodAttr) (yypvt[-5].methAttr | mdPinvokeImpl); } break;
case 177:
#line 535 "asmparse.y"
{ PASM->SetPinvoke(new BinStr(),0,NULL,yypvt[-1].pinvAttr); 
                                                              yyval.methAttr = (CorMethodAttr) (yypvt[-4].methAttr | mdPinvokeImpl); } break;
case 178:
#line 539 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) 0; } break;
case 179:
#line 540 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].pinvAttr | pmNoMangle); } break;
case 180:
#line 541 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].pinvAttr | pmCharSetAnsi); } break;
case 181:
#line 542 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].pinvAttr | pmCharSetUnicode); } break;
case 182:
#line 543 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].pinvAttr | pmCharSetAuto); } break;
case 183:
#line 544 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].pinvAttr | pmSupportsLastError); } break;
case 184:
#line 545 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].pinvAttr | pmCallConvWinapi); } break;
case 185:
#line 546 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].pinvAttr | pmCallConvCdecl); } break;
case 186:
#line 547 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].pinvAttr | pmCallConvStdcall); } break;
case 187:
#line 548 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].pinvAttr | pmCallConvThiscall); } break;
case 188:
#line 549 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].pinvAttr | pmCallConvFastcall); } break;
case 189:
#line 550 "asmparse.y"
{ yyval.pinvAttr = (CorPinvokeMap) (yypvt[-1].int32); } break;
case 190:
#line 553 "asmparse.y"
{ yyval.string = newString(COR_CTOR_METHOD_NAME); } break;
case 191:
#line 554 "asmparse.y"
{ yyval.string = newString(COR_CCTOR_METHOD_NAME); } break;
case 192:
#line 555 "asmparse.y"
{ yyval.string = yypvt[-0].string; } break;
case 193:
#line 558 "asmparse.y"
{ yyval.int32 = 0; } break;
case 194:
#line 559 "asmparse.y"
{ yyval.int32 = yypvt[-3].int32 | pdIn; } break;
case 195:
#line 560 "asmparse.y"
{ yyval.int32 = yypvt[-3].int32 | pdOut; } break;
case 196:
#line 561 "asmparse.y"
{ yyval.int32 = yypvt[-3].int32 | pdOptional; } break;
case 197:
#line 562 "asmparse.y"
{ yyval.int32 = yypvt[-1].int32 + 1; } break;
case 198:
#line 565 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) 0; } break;
case 199:
#line 566 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) (yypvt[-1].fieldAttr | fdStatic); } break;
case 200:
#line 567 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) ((yypvt[-1].fieldAttr & ~mdMemberAccessMask) | fdPublic); } break;
case 201:
#line 568 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) ((yypvt[-1].fieldAttr & ~mdMemberAccessMask) | fdPrivate); } break;
case 202:
#line 569 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) ((yypvt[-1].fieldAttr & ~mdMemberAccessMask) | fdFamily); } break;
case 203:
#line 570 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) (yypvt[-1].fieldAttr | fdInitOnly); } break;
case 204:
#line 571 "asmparse.y"
{ yyval.fieldAttr = yypvt[-1].fieldAttr; } break;
case 205:
#line 572 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) (yypvt[-1].fieldAttr | fdSpecialName); } break;
case 206:
#line 585 "asmparse.y"
{ PASM->m_pMarshal = yypvt[-1].binstr; } break;
case 207:
#line 586 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) ((yypvt[-1].fieldAttr & ~mdMemberAccessMask) | fdAssembly); } break;
case 208:
#line 587 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) ((yypvt[-1].fieldAttr & ~mdMemberAccessMask) | fdFamANDAssem); } break;
case 209:
#line 588 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) ((yypvt[-1].fieldAttr & ~mdMemberAccessMask) | fdFamORAssem); } break;
case 210:
#line 589 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) ((yypvt[-1].fieldAttr & ~mdMemberAccessMask) | fdPrivateScope); } break;
case 211:
#line 590 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) (yypvt[-1].fieldAttr | fdLiteral); } break;
case 212:
#line 591 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) (yypvt[-1].fieldAttr | fdNotSerialized); } break;
case 213:
#line 592 "asmparse.y"
{ yyval.fieldAttr = (CorFieldAttr) (yypvt[-1].int32); } break;
case 214:
#line 595 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) (miIL | miManaged); } break;
case 215:
#line 596 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) ((yypvt[-1].implAttr & 0xFFF4) | miNative); } break;
case 216:
#line 597 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) ((yypvt[-1].implAttr & 0xFFF4) | miIL); } break;
case 217:
#line 598 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) ((yypvt[-1].implAttr & 0xFFF4) | miOPTIL); } break;
case 218:
#line 599 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) ((yypvt[-1].implAttr & 0xFFFB) | miManaged); } break;
case 219:
#line 600 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) ((yypvt[-1].implAttr & 0xFFFB) | miUnmanaged); } break;
case 220:
#line 601 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) (yypvt[-1].implAttr | miForwardRef); } break;
case 221:
#line 602 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) (yypvt[-1].implAttr | miPreserveSig); } break;
case 222:
#line 603 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) (yypvt[-1].implAttr | miRuntime); } break;
case 223:
#line 604 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) (yypvt[-1].implAttr | miInternalCall); } break;
case 224:
#line 605 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) (yypvt[-1].implAttr | miSynchronized); } break;
case 225:
#line 606 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) (yypvt[-1].implAttr | miNoInlining); } break;
case 226:
#line 607 "asmparse.y"
{ yyval.implAttr = (CorMethodImpl) (yypvt[-1].int32); } break;
case 227:
#line 610 "asmparse.y"
{ PASM->delArgNameList(PASM->m_firstArgName); PASM->m_firstArgName = NULL;PASM->m_lastArgName = NULL; } break;
case 228:
#line 614 "asmparse.y"
{ PASM->EmitByte(yypvt[-0].int32); } break;
case 229:
#line 615 "asmparse.y"
{ delete PASM->m_SEHD; PASM->m_SEHD = PASM->m_SEHDstack.POP(); } break;
case 230:
#line 616 "asmparse.y"
{ PASM->EmitMaxStack(yypvt[-0].int32); } break;
case 231:
#line 617 "asmparse.y"
{ PASM->EmitLocals(parser->MakeSig(IMAGE_CEE_CS_CALLCONV_LOCAL_SIG, 0, yypvt[-1].binstr)); } break;
case 232:
#line 618 "asmparse.y"
{ PASM->EmitZeroInit(); 
                                                              PASM->EmitLocals(parser->MakeSig(IMAGE_CEE_CS_CALLCONV_LOCAL_SIG, 0, yypvt[-1].binstr)); } break;
case 233:
#line 620 "asmparse.y"
{ PASM->EmitEntryPoint(); } break;
case 234:
#line 621 "asmparse.y"
{ PASM->EmitZeroInit(); } break;
case 237:
#line 624 "asmparse.y"
{ PASM->AddLabel(PASM->m_CurPC,yypvt[-1].string); /*PASM->EmitLabel($1);*/ } break;
case 242:
#line 629 "asmparse.y"
{ if(PASM->m_pCurMethod->m_dwExportOrdinal == 0xFFFFFFFF)
						                                      {
						                                          PASM->m_pCurMethod->m_dwExportOrdinal = yypvt[-1].int32;
															      PASM->m_pCurMethod->m_szExportAlias = NULL;
															  }
														      else
															      PASM->report->warn("Duplicate .export directive, ignored\n");
															} break;
case 243:
#line 637 "asmparse.y"
{ if(PASM->m_pCurMethod->m_dwExportOrdinal == 0xFFFFFFFF)
						                                      {
						                                          PASM->m_pCurMethod->m_dwExportOrdinal = yypvt[-3].int32;
															      PASM->m_pCurMethod->m_szExportAlias = yypvt[-0].string;
															  }
														      else
															      PASM->report->warn("Duplicate .export directive, ignored\n");
															} break;
case 244:
#line 645 "asmparse.y"
{ PASM->m_pCurMethod->m_wVTEntry = (WORD)yypvt[-2].int32;
                                                              PASM->m_pCurMethod->m_wVTSlot = (WORD)yypvt[-0].int32; } break;
case 245:
#line 648 "asmparse.y"
{ PASM->AddMethodImpl(yypvt[-2].token,yypvt[-0].string,NULL,NULL,NULL,NULL); } break;
case 246:
#line 651 "asmparse.y"
{ PASM->AddMethodImpl(yypvt[-5].token,yypvt[-3].string,
                                                                   parser->MakeSig(yypvt[-7].int32,yypvt[-6].binstr,yypvt[-1].binstr),NULL,NULL,NULL); } break;
case 248:
#line 655 "asmparse.y"
{ if( yypvt[-2].int32 ) {
                                                                ARG_NAME_LIST* pAN=PASM->findArg(PASM->m_pCurMethod->m_firstArgName, yypvt[-2].int32 - 1);
                                                                if(pAN)
                                                                {
                                                                    PASM->m_pCustomDescrList = &(pAN->CustDList);
                                                                    pAN->pValue = yypvt[-0].binstr;
                                                                }
                                                                else
                                                                {
                                                                    PASM->m_pCustomDescrList = NULL;
                                                                    if(yypvt[-0].binstr) delete yypvt[-0].binstr;
                                                                }
                                                              } else {
                                                                PASM->m_pCustomDescrList = &(PASM->m_pCurMethod->m_RetCustDList);
                                                                PASM->m_pCurMethod->m_pRetValue = yypvt[-0].binstr;
                                                              }
                                                              PASM->m_tkCurrentCVOwner = 0;
                                                            } break;
case 249:
#line 675 "asmparse.y"
{ PASM->m_pCurMethod->CloseScope(); } break;
case 250:
#line 678 "asmparse.y"
{ PASM->m_pCurMethod->OpenScope(); } break;
case 254:
#line 688 "asmparse.y"
{ PASM->m_SEHD->tryTo = PASM->m_CurPC; } break;
case 255:
#line 689 "asmparse.y"
{ PASM->SetTryLabels(yypvt[-2].string, yypvt[-0].string); } break;
case 256:
#line 690 "asmparse.y"
{ if(PASM->m_SEHD) {PASM->m_SEHD->tryFrom = yypvt[-2].int32;
                                                              PASM->m_SEHD->tryTo = yypvt[-0].int32;} } break;
case 257:
#line 694 "asmparse.y"
{ PASM->NewSEHDescriptor();
                                                              PASM->m_SEHD->tryFrom = PASM->m_CurPC; } break;
case 258:
#line 699 "asmparse.y"
{ PASM->EmitTry(); } break;
case 259:
#line 700 "asmparse.y"
{ PASM->EmitTry(); } break;
case 260:
#line 701 "asmparse.y"
{ PASM->EmitTry(); } break;
case 261:
#line 702 "asmparse.y"
{ PASM->EmitTry(); } break;
case 262:
#line 706 "asmparse.y"
{ PASM->m_SEHD->sehHandler = PASM->m_CurPC; } break;
case 263:
#line 707 "asmparse.y"
{ PASM->SetFilterLabel(yypvt[-0].string); 
                                                               PASM->m_SEHD->sehHandler = PASM->m_CurPC; } break;
case 264:
#line 709 "asmparse.y"
{ PASM->m_SEHD->sehFilter = yypvt[-0].int32; 
                                                               PASM->m_SEHD->sehHandler = PASM->m_CurPC; } break;
case 265:
#line 713 "asmparse.y"
{ PASM->m_SEHD->sehClause = COR_ILEXCEPTION_CLAUSE_FILTER;
                                                               PASM->m_SEHD->sehFilter = PASM->m_CurPC; } break;
case 266:
#line 717 "asmparse.y"
{  PASM->m_SEHD->sehClause = COR_ILEXCEPTION_CLAUSE_NONE;
                                                               PASM->SetCatchClass(yypvt[-0].token); 
                                                               PASM->m_SEHD->sehHandler = PASM->m_CurPC; } break;
case 267:
#line 722 "asmparse.y"
{ PASM->m_SEHD->sehClause = COR_ILEXCEPTION_CLAUSE_FINALLY;
                                                               PASM->m_SEHD->sehHandler = PASM->m_CurPC; } break;
case 268:
#line 726 "asmparse.y"
{ PASM->m_SEHD->sehClause = COR_ILEXCEPTION_CLAUSE_FAULT;
                                                               PASM->m_SEHD->sehHandler = PASM->m_CurPC; } break;
case 269:
#line 730 "asmparse.y"
{ PASM->m_SEHD->sehHandlerTo = PASM->m_CurPC; } break;
case 270:
#line 731 "asmparse.y"
{ PASM->SetHandlerLabels(yypvt[-2].string, yypvt[-0].string); } break;
case 271:
#line 732 "asmparse.y"
{ PASM->m_SEHD->sehHandler = yypvt[-2].int32;
                                                               PASM->m_SEHD->sehHandlerTo = yypvt[-0].int32; } break;
case 275:
#line 744 "asmparse.y"
{ PASM->EmitDataLabel(yypvt[-1].string); } break;
case 277:
#line 748 "asmparse.y"
{ PASM->SetDataSection(); } break;
case 278:
#line 749 "asmparse.y"
{ PASM->SetTLSSection(); } break;
case 283:
#line 760 "asmparse.y"
{ yyval.int32 = 1; } break;
case 284:
#line 761 "asmparse.y"
{ FAIL_UNLESS(yypvt[-1].int32 > 0, ("Illegal item count: %d\n",yypvt[-1].int32)); yyval.int32 = yypvt[-1].int32; } break;
case 285:
#line 764 "asmparse.y"
{ PASM->EmitDataString(yypvt[-1].binstr); } break;
case 286:
#line 765 "asmparse.y"
{ PASM->EmitDD(yypvt[-1].string); } break;
case 287:
#line 766 "asmparse.y"
{ PASM->EmitData(yypvt[-1].binstr->ptr(),yypvt[-1].binstr->length()); } break;
case 288:
#line 768 "asmparse.y"
{ float f = (float) (*yypvt[-2].float64); float* pf = new float[yypvt[-0].int32];
                                                               for(int i=0; i < yypvt[-0].int32; i++) pf[i] = f;
                                                               PASM->EmitData(pf, sizeof(float)*yypvt[-0].int32); delete yypvt[-2].float64; delete pf; } break;
case 289:
#line 772 "asmparse.y"
{ double* pd = new double[yypvt[-0].int32];
                                                               for(int i=0; i<yypvt[-0].int32; i++) pd[i] = *(yypvt[-2].float64);
                                                               PASM->EmitData(pd, sizeof(double)*yypvt[-0].int32); delete yypvt[-2].float64; delete pd; } break;
case 290:
#line 776 "asmparse.y"
{ __int64* pll = new __int64[yypvt[-0].int32];
                                                               for(int i=0; i<yypvt[-0].int32; i++) pll[i] = *(yypvt[-2].int64);
                                                               PASM->EmitData(pll, sizeof(__int64)*yypvt[-0].int32); delete yypvt[-2].int64; delete pll; } break;
case 291:
#line 780 "asmparse.y"
{ __int32* pl = new __int32[yypvt[-0].int32];
                                                               for(int i=0; i<yypvt[-0].int32; i++) pl[i] = yypvt[-2].int32;
                                                               PASM->EmitData(pl, sizeof(__int32)*yypvt[-0].int32); delete pl; } break;
case 292:
#line 784 "asmparse.y"
{ __int16 i = (__int16) yypvt[-2].int32; FAIL_UNLESS(i == yypvt[-2].int32, ("Value %d too big\n", yypvt[-2].int32));
                                                               __int16* ps = new __int16[yypvt[-0].int32];
                                                               for(int j=0; j<yypvt[-0].int32; j++) ps[j] = i;
                                                               PASM->EmitData(ps, sizeof(__int16)*yypvt[-0].int32); delete ps; } break;
case 293:
#line 789 "asmparse.y"
{ __int8 i = (__int8) yypvt[-2].int32; FAIL_UNLESS(i == yypvt[-2].int32, ("Value %d too big\n", yypvt[-2].int32));
                                                               __int8* pb = new __int8[yypvt[-0].int32];
                                                               for(int j=0; j<yypvt[-0].int32; j++) pb[j] = i;
                                                               PASM->EmitData(pb, sizeof(__int8)*yypvt[-0].int32); delete pb; } break;
case 294:
#line 793 "asmparse.y"
{ float* pf = new float[yypvt[-0].int32];
                                                               PASM->EmitData(pf, sizeof(float)*yypvt[-0].int32); delete pf; } break;
case 295:
#line 795 "asmparse.y"
{ double* pd = new double[yypvt[-0].int32];
                                                               PASM->EmitData(pd, sizeof(double)*yypvt[-0].int32); delete pd; } break;
case 296:
#line 797 "asmparse.y"
{ __int64* pll = new __int64[yypvt[-0].int32];
                                                               PASM->EmitData(pll, sizeof(__int64)*yypvt[-0].int32); delete pll; } break;
case 297:
#line 799 "asmparse.y"
{ __int32* pl = new __int32[yypvt[-0].int32];
                                                               PASM->EmitData(pl, sizeof(__int32)*yypvt[-0].int32); delete pl; } break;
case 298:
#line 801 "asmparse.y"
{ __int16* ps = new __int16[yypvt[-0].int32];
                                                               PASM->EmitData(ps, sizeof(__int16)*yypvt[-0].int32); delete ps; } break;
case 299:
#line 803 "asmparse.y"
{ __int8* pb = new __int8[yypvt[-0].int32];
                                                               PASM->EmitData(pb, sizeof(__int8)*yypvt[-0].int32); delete pb; } break;
case 300:
#line 807 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_R4); 
                                                               float f = (float) (*yypvt[-1].float64); yyval.binstr->appendInt32(*((int*)&f)); delete yypvt[-1].float64; } break;
case 301:
#line 809 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_R8); 
                                                               yyval.binstr->appendInt64((__int64 *)yypvt[-1].float64); delete yypvt[-1].float64; } break;
case 302:
#line 811 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_R4); 
                                                               int f = *((int*)yypvt[-1].int64); 
                                                               yyval.binstr->appendInt32(f); delete yypvt[-1].int64; } break;
case 303:
#line 814 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_R8); 
                                                               yyval.binstr->appendInt64((__int64 *)yypvt[-1].int64); delete yypvt[-1].int64; } break;
case 304:
#line 816 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_I8); 
                                                               yyval.binstr->appendInt64((__int64 *)yypvt[-1].int64); delete yypvt[-1].int64; } break;
case 305:
#line 818 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_I4); 
                                                               yyval.binstr->appendInt32(*((__int32*)yypvt[-1].int64)); delete yypvt[-1].int64;} break;
case 306:
#line 820 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_I2); 
                                                               yyval.binstr->appendInt16(*((__int16*)yypvt[-1].int64)); delete yypvt[-1].int64;} break;
case 307:
#line 822 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_I1); 
                                                               yyval.binstr->appendInt8(*((__int8*)yypvt[-1].int64)); delete yypvt[-1].int64; } break;
case 308:
#line 824 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U8); 
                                                               yyval.binstr->appendInt64((__int64 *)yypvt[-1].int64); delete yypvt[-1].int64; } break;
case 309:
#line 826 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U4); 
                                                               yyval.binstr->appendInt32(*((__int32*)yypvt[-1].int64)); delete yypvt[-1].int64;} break;
case 310:
#line 828 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U2); 
                                                               yyval.binstr->appendInt16(*((__int16*)yypvt[-1].int64)); delete yypvt[-1].int64;} break;
case 311:
#line 830 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U1); 
                                                               yyval.binstr->appendInt8(*((__int8*)yypvt[-1].int64)); delete yypvt[-1].int64; } break;
case 312:
#line 832 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U8); 
                                                               yyval.binstr->appendInt64((__int64 *)yypvt[-1].int64); delete yypvt[-1].int64; } break;
case 313:
#line 834 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U4); 
                                                               yyval.binstr->appendInt32(*((__int32*)yypvt[-1].int64)); delete yypvt[-1].int64;} break;
case 314:
#line 836 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U2); 
                                                               yyval.binstr->appendInt16(*((__int16*)yypvt[-1].int64)); delete yypvt[-1].int64;} break;
case 315:
#line 838 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U1); 
                                                               yyval.binstr->appendInt8(*((__int8*)yypvt[-1].int64)); delete yypvt[-1].int64; } break;
case 316:
#line 840 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_CHAR); 
                                                               yyval.binstr->appendInt16((int)*((unsigned __int16*)yypvt[-1].int64)); delete yypvt[-1].int64;} break;
case 317:
#line 842 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_BOOLEAN); 
                                                               yyval.binstr->appendInt8(yypvt[-1].int32);} break;
case 318:
#line 844 "asmparse.y"
{ yyval.binstr = BinStrToUnicode(yypvt[-0].binstr); yyval.binstr->insertInt8(ELEMENT_TYPE_STRING);} break;
case 319:
#line 845 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_STRING);
                                                               yyval.binstr->append(yypvt[-1].binstr); delete yypvt[-1].binstr;} break;
case 320:
#line 847 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_CLASS); 
															    yyval.binstr->appendInt32(0); } break;
case 321:
#line 851 "asmparse.y"
{ bParsingByteArray = TRUE; } break;
case 322:
#line 854 "asmparse.y"
{ yyval.binstr = new BinStr(); } break;
case 323:
#line 855 "asmparse.y"
{ yyval.binstr = yypvt[-0].binstr; } break;
case 324:
#line 858 "asmparse.y"
{ __int8 i = (__int8) yypvt[-0].int32; yyval.binstr = new BinStr(); yyval.binstr->appendInt8(i); } break;
case 325:
#line 859 "asmparse.y"
{ __int8 i = (__int8) yypvt[-0].int32; yyval.binstr = yypvt[-1].binstr; yyval.binstr->appendInt8(i); } break;
case 326:
#line 862 "asmparse.y"
{ yyval.instr = yypvt[-1].instr; bParsingByteArray = TRUE; } break;
case 327:
#line 865 "asmparse.y"
{ yyval.instr = yypvt[-0].instr; iOpcodeLen = PASM->OpcodeLen(yypvt[-0].instr); } break;
case 328:
#line 868 "asmparse.y"
{ palDummyFirst = PASM->m_firstArgName;
                                                               palDummyLast = PASM->m_lastArgName;   
                                                               PASM->m_firstArgName = NULL;
                                                               PASM->m_lastArgName = NULL; } break;
case 329:
#line 874 "asmparse.y"
{ PASM->EmitOpcode(yypvt[-0].instr); } break;
case 330:
#line 875 "asmparse.y"
{ PASM->EmitInstrVar(yypvt[-1].instr, yypvt[-0].int32); } break;
case 331:
#line 876 "asmparse.y"
{ PASM->EmitInstrVarByName(yypvt[-1].instr, yypvt[-0].string); } break;
case 332:
#line 877 "asmparse.y"
{ PASM->EmitInstrI(yypvt[-1].instr, yypvt[-0].int32); } break;
case 333:
#line 878 "asmparse.y"
{ PASM->EmitInstrI8(yypvt[-1].instr, yypvt[-0].int64); } break;
case 334:
#line 879 "asmparse.y"
{ PASM->EmitInstrR(yypvt[-1].instr, yypvt[-0].float64); delete (yypvt[-0].float64);} break;
case 335:
#line 880 "asmparse.y"
{ double f = (double) (*yypvt[-0].int64); PASM->EmitInstrR(yypvt[-1].instr, &f); } break;
case 336:
#line 881 "asmparse.y"
{ unsigned L = yypvt[-1].binstr->length();
                                                               FAIL_UNLESS(L >= sizeof(float), ("%d hexbytes, must be at least %d\n",
                                                                           L,sizeof(float))); 
                                                               if(L < sizeof(float)) {YYERROR; } 
                                                               else {
                                                                   double f = (L >= sizeof(double)) ? *((double *)(yypvt[-1].binstr->ptr()))
                                                                                    : (double)(*(float *)(yypvt[-1].binstr->ptr())); 
                                                                   PASM->EmitInstrR(yypvt[-2].instr,&f); }
                                                               delete yypvt[-1].binstr; } break;
case 337:
#line 890 "asmparse.y"
{ PASM->EmitInstrBrOffset(yypvt[-1].instr, yypvt[-0].int32); } break;
case 338:
#line 891 "asmparse.y"
{ PASM->EmitInstrBrTarget(yypvt[-1].instr, yypvt[-0].string); } break;
case 339:
#line 893 "asmparse.y"
{ if(yypvt[-9].instr->opcode == CEE_NEWOBJ || yypvt[-9].instr->opcode == CEE_CALLVIRT)
                                                                   yypvt[-8].int32 = yypvt[-8].int32 | IMAGE_CEE_CS_CALLCONV_HASTHIS; 
							                                   mdToken mr;
                                                               if (yypvt[-3].binstr == NULL)
                                                                 mr = PASM->MakeMemberRef(yypvt[-6].token, yypvt[-4].string, parser->MakeSig(yypvt[-8].int32, yypvt[-7].binstr, yypvt[-1].binstr), PASM->OpcodeLen(yypvt[-9].instr));
                                                               else
							                                   {
                                                                 mr = PASM->MakeMemberRef(yypvt[-6].token, yypvt[-4].string, parser->MakeSig(yypvt[-8].int32 | IMAGE_CEE_CS_CALLCONV_GENERIC, yypvt[-7].binstr, yypvt[-1].binstr, corCountArgs(yypvt[-3].binstr)), 0);
                                                                 mr = PASM->MakeMethodSpec(mr, 
                                                                   parser->MakeSig(IMAGE_CEE_CS_CALLCONV_INSTANTIATION, 0, yypvt[-3].binstr),PASM->OpcodeLen(yypvt[-9].instr));
							                                   }
                                                               PASM->EmitInstrI(yypvt[-9].instr,mr);
                                                               PASM->m_tkCurrentCVOwner = mr;
                                                               PASM->m_pCustomDescrList = NULL;
                                                             } break;
case 340:
#line 909 "asmparse.y"
{ if(yypvt[-7].instr->opcode == CEE_NEWOBJ || yypvt[-7].instr->opcode == CEE_CALLVIRT)
                                                                   yypvt[-6].int32 = yypvt[-6].int32 | IMAGE_CEE_CS_CALLCONV_HASTHIS; 
							                                   mdToken mr;
                                                               if (yypvt[-3].binstr == NULL)
                                                                 mr = PASM->MakeMemberRef(mdTokenNil, yypvt[-4].string, parser->MakeSig(yypvt[-6].int32, yypvt[-5].binstr, yypvt[-1].binstr), PASM->OpcodeLen(yypvt[-7].instr));
                                                               else
							                                   {
                                                                 mr = PASM->MakeMemberRef(mdTokenNil, yypvt[-4].string, parser->MakeSig(yypvt[-6].int32 | IMAGE_CEE_CS_CALLCONV_GENERIC, yypvt[-5].binstr, yypvt[-1].binstr, corCountArgs(yypvt[-3].binstr)), 0);
                                                                 mr = PASM->MakeMethodSpec(mr, 
                                                                   parser->MakeSig(IMAGE_CEE_CS_CALLCONV_INSTANTIATION, 0, yypvt[-3].binstr),PASM->OpcodeLen(yypvt[-7].instr));
							                                   }
                                                               PASM->EmitInstrI(yypvt[-7].instr,mr);
                                                               PASM->m_tkCurrentCVOwner = mr;
                                                               PASM->m_pCustomDescrList = NULL;
                                                             } break;
case 341:
#line 925 "asmparse.y"
{ yypvt[-3].binstr->insertInt8(IMAGE_CEE_CS_CALLCONV_FIELD); 
                                                               mdToken mr = PASM->MakeMemberRef(yypvt[-2].token, yypvt[-0].string, yypvt[-3].binstr, PASM->OpcodeLen(yypvt[-4].instr));
                                                               PASM->EmitInstrI(yypvt[-4].instr,mr);
                                                               PASM->m_tkCurrentCVOwner = mr;
                                                               PASM->m_pCustomDescrList = NULL;
                                                             } break;
case 342:
#line 932 "asmparse.y"
{ yypvt[-1].binstr->insertInt8(IMAGE_CEE_CS_CALLCONV_FIELD); 
                                                               mdToken mr = PASM->MakeMemberRef(mdTokenNil, yypvt[-0].string, yypvt[-1].binstr, PASM->OpcodeLen(yypvt[-2].instr));
                                                               PASM->EmitInstrI(yypvt[-2].instr,mr);
                                                               PASM->m_tkCurrentCVOwner = mr;
                                                               PASM->m_pCustomDescrList = NULL;
                                                             } break;
case 343:
#line 938 "asmparse.y"
{ PASM->EmitInstrI(yypvt[-1].instr, yypvt[-0].token); 
                                                               PASM->m_tkCurrentCVOwner = yypvt[-0].token;
                                                               PASM->m_pCustomDescrList = NULL;
                                                             } break;
case 344:
#line 942 "asmparse.y"
{ PASM->EmitInstrStringLiteral(yypvt[-1].instr, yypvt[-0].binstr,TRUE); } break;
case 345:
#line 944 "asmparse.y"
{ PASM->EmitInstrStringLiteral(yypvt[-4].instr, yypvt[-1].binstr,FALSE); } break;
case 346:
#line 946 "asmparse.y"
{ PASM->EmitInstrStringLiteral(yypvt[-3].instr, yypvt[-1].binstr,FALSE); } break;
case 347:
#line 948 "asmparse.y"
{ PASM->EmitInstrSig(yypvt[-5].instr, parser->MakeSig(yypvt[-4].int32, yypvt[-3].binstr, yypvt[-1].binstr)); } break;
case 348:
#line 949 "asmparse.y"
{ PASM->EmitInstrRVA(yypvt[-1].instr, yypvt[-0].string, TRUE); 
                                                               PASM->report->warn("Deprecated instruction 'ldptr'\n"); } break;
case 349:
#line 951 "asmparse.y"
{ PASM->EmitInstrRVA(yypvt[-1].instr, (char *)yypvt[-0].int32, FALSE); 
                                                               PASM->report->warn("Deprecated instruction 'ldptr'\n"); } break;
case 350:
#line 954 "asmparse.y"
{ PASM->EmitInstrI(yypvt[-1].instr,yypvt[-0].int32);
                                                               PASM->m_tkCurrentCVOwner = yypvt[-0].int32;
                                                               PASM->m_pCustomDescrList = NULL;
															   iOpcodeLen = 0;
                                                             } break;
case 351:
#line 959 "asmparse.y"
{ PASM->EmitInstrSwitch(yypvt[-3].instr, yypvt[-1].labels); } break;
case 352:
#line 960 "asmparse.y"
{ PASM->EmitInstrPhi(yypvt[-1].instr, yypvt[-0].binstr); } break;
case 353:
#line 963 "asmparse.y"
{ yyval.binstr = NULL; } break;
case 354:
#line 964 "asmparse.y"
{ yyval.binstr = yypvt[-1].binstr; } break;
case 355:
#line 967 "asmparse.y"
{ yyval.binstr = NULL; } break;
case 356:
#line 968 "asmparse.y"
{ yyval.binstr = yypvt[-0].binstr; } break;
case 357:
#line 969 "asmparse.y"
{ yyval.binstr = yypvt[-0].binstr; } break;
case 358:
#line 972 "asmparse.y"
{ yyval.binstr = yypvt[-2].binstr; yyval.binstr->append(yypvt[-0].binstr); delete yypvt[-0].binstr; } break;
case 359:
#line 973 "asmparse.y"
{ yyval.binstr = yypvt[-1].binstr; yyval.binstr->append(NULL); } break;
case 360:
#line 976 "asmparse.y"
{ yyval.token = yypvt[-1].token; } break;
case 361:
#line 979 "asmparse.y"
{ yyval.binstr = new BinStr(); } break;
case 362:
#line 980 "asmparse.y"
{ yyval.binstr = yypvt[-0].binstr;} break;
case 363:
#line 983 "asmparse.y"
{ yyval.binstr = yypvt[-0].binstr; } break;
case 364:
#line 984 "asmparse.y"
{ yyval.binstr = yypvt[-2].binstr; yyval.binstr->append(yypvt[-0].binstr); delete yypvt[-0].binstr; } break;
case 365:
#line 987 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_SENTINEL); } break;
case 366:
#line 988 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->append(yypvt[-0].binstr); PASM->addArgName("", yypvt[-0].binstr, NULL, yypvt[-1].int32); } break;
case 367:
#line 989 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->append(yypvt[-1].binstr); PASM->addArgName(yypvt[-0].string, yypvt[-1].binstr, NULL, yypvt[-2].int32); delete yypvt[-0].string;} break;
case 368:
#line 991 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->append(yypvt[-4].binstr); PASM->addArgName("", yypvt[-4].binstr, yypvt[-1].binstr, yypvt[-5].int32); } break;
case 369:
#line 993 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->append(yypvt[-5].binstr); PASM->addArgName(yypvt[-0].string, yypvt[-5].binstr, yypvt[-2].binstr, yypvt[-6].int32); delete yypvt[-0].string;} break;
case 370:
#line 996 "asmparse.y"
{ yyval.string = yypvt[-0].string; } break;
case 371:
#line 997 "asmparse.y"
{ yyval.string = yypvt[-0].string; } break;
case 372:
#line 998 "asmparse.y"
{ yyval.string = newStringWDel(yypvt[-2].string, '.', yypvt[-0].string); } break;
case 373:
#line 1001 "asmparse.y"
{ yyval.token = PASM->ResolveClassRef(PASM->GetAsmRef(yypvt[-2].string), yypvt[-0].string, NULL); delete[] yypvt[-2].string;} break;
case 374:
#line 1002 "asmparse.y"
{ yyval.token = PASM->ResolveClassRef(mdTokenNil, yypvt[-0].string, NULL); } break;
case 375:
#line 1003 "asmparse.y"
{ yyval.token = PASM->ResolveClassRef(PASM->GetModRef(yypvt[-2].string),yypvt[-0].string, NULL); delete[] yypvt[-2].string;} break;
case 376:
#line 1004 "asmparse.y"
{ yyval.token = PASM->ResolveClassRef(1,yypvt[-0].string,NULL); } break;
case 377:
#line 1007 "asmparse.y"
{ yyval.string = yypvt[-0].string; } break;
case 378:
#line 1008 "asmparse.y"
{ yyval.string = newStringWDel(yypvt[-2].string, (char)NESTING_SEP, yypvt[-0].string); } break;
case 379:
#line 1011 "asmparse.y"
{ yyval.token = yypvt[-0].token;} break;
case 380:
#line 1012 "asmparse.y"
{ yyval.token = PASM->GetAsmRef(yypvt[-1].string); delete[] yypvt[-1].string;} break;
case 381:
#line 1013 "asmparse.y"
{ yyval.token = PASM->GetModRef(yypvt[-1].string); delete[] yypvt[-1].string;} break;
case 382:
#line 1014 "asmparse.y"
{ yyval.token = PASM->ResolveTypeSpec(yypvt[-0].binstr); } break;
case 383:
#line 1017 "asmparse.y"
{ yyval.int32 = (yypvt[-0].int32 | IMAGE_CEE_CS_CALLCONV_HASTHIS); } break;
case 384:
#line 1018 "asmparse.y"
{ yyval.int32 = (yypvt[-0].int32 | IMAGE_CEE_CS_CALLCONV_EXPLICITTHIS); } break;
case 385:
#line 1019 "asmparse.y"
{ yyval.int32 = yypvt[-0].int32; } break;
case 386:
#line 1020 "asmparse.y"
{ yyval.int32 = yypvt[-1].int32; } break;
case 387:
#line 1023 "asmparse.y"
{ yyval.int32 = IMAGE_CEE_CS_CALLCONV_DEFAULT; } break;
case 388:
#line 1024 "asmparse.y"
{ yyval.int32 = IMAGE_CEE_CS_CALLCONV_DEFAULT; } break;
case 389:
#line 1025 "asmparse.y"
{ yyval.int32 = IMAGE_CEE_CS_CALLCONV_VARARG; } break;
case 390:
#line 1026 "asmparse.y"
{ yyval.int32 = IMAGE_CEE_CS_CALLCONV_C; } break;
case 391:
#line 1027 "asmparse.y"
{ yyval.int32 = IMAGE_CEE_CS_CALLCONV_STDCALL; } break;
case 392:
#line 1028 "asmparse.y"
{ yyval.int32 = IMAGE_CEE_CS_CALLCONV_THISCALL; } break;
case 393:
#line 1029 "asmparse.y"
{ yyval.int32 = IMAGE_CEE_CS_CALLCONV_FASTCALL; } break;
case 394:
#line 1032 "asmparse.y"
{ yyval.binstr = new BinStr(); } break;
case 395:
#line 1034 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_CUSTOMMARSHALER);
                                                                corEmitInt(yyval.binstr,yypvt[-7].binstr->length()); yyval.binstr->append(yypvt[-7].binstr);
                                                                corEmitInt(yyval.binstr,yypvt[-5].binstr->length()); yyval.binstr->append(yypvt[-5].binstr);
                                                                corEmitInt(yyval.binstr,yypvt[-3].binstr->length()); yyval.binstr->append(yypvt[-3].binstr);
                                                                corEmitInt(yyval.binstr,yypvt[-1].binstr->length()); yyval.binstr->append(yypvt[-1].binstr); 
																PASM->report->warn("Deprecated 4-string form of custom marshaler, first two strings ignored\n");} break;
case 396:
#line 1041 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_CUSTOMMARSHALER);
                                                                corEmitInt(yyval.binstr,0);
                                                                corEmitInt(yyval.binstr,0);
                                                                corEmitInt(yyval.binstr,yypvt[-3].binstr->length()); yyval.binstr->append(yypvt[-3].binstr);
                                                                corEmitInt(yyval.binstr,yypvt[-1].binstr->length()); yyval.binstr->append(yypvt[-1].binstr); } break;
case 397:
#line 1046 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_FIXEDSYSSTRING);
                                                                corEmitInt(yyval.binstr,yypvt[-1].int32); } break;
case 398:
#line 1048 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_FIXEDARRAY);
                                                                corEmitInt(yyval.binstr,yypvt[-1].int32); } break;
case 399:
#line 1050 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_VARIANT); 
																PASM->report->warn("Deprecated native type 'variant'\n"); } break;
case 400:
#line 1052 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_CURRENCY); } break;
case 401:
#line 1053 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_SYSCHAR); 
																PASM->report->warn("Deprecated native type 'syschar'\n"); } break;
case 402:
#line 1055 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_VOID); 
																PASM->report->warn("Deprecated native type 'void'\n"); } break;
case 403:
#line 1057 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_BOOLEAN); } break;
case 404:
#line 1058 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_I1); } break;
case 405:
#line 1059 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_I2); } break;
case 406:
#line 1060 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_I4); } break;
case 407:
#line 1061 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_I8); } break;
case 408:
#line 1062 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_R4); } break;
case 409:
#line 1063 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_R8); } break;
case 410:
#line 1064 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_ERROR); } break;
case 411:
#line 1065 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_U1); } break;
case 412:
#line 1066 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_U2); } break;
case 413:
#line 1067 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_U4); } break;
case 414:
#line 1068 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_U8); } break;
case 415:
#line 1069 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_U1); } break;
case 416:
#line 1070 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_U2); } break;
case 417:
#line 1071 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_U4); } break;
case 418:
#line 1072 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_U8); } break;
case 419:
#line 1073 "asmparse.y"
{ yyval.binstr = yypvt[-1].binstr; yyval.binstr->insertInt8(NATIVE_TYPE_PTR); 
																PASM->report->warn("Deprecated native type '*'\n"); } break;
case 420:
#line 1075 "asmparse.y"
{ yyval.binstr = yypvt[-2].binstr; if(yyval.binstr->length()==0) yyval.binstr->appendInt8(NATIVE_TYPE_MAX);
                                                                yyval.binstr->insertInt8(NATIVE_TYPE_ARRAY); } break;
case 421:
#line 1077 "asmparse.y"
{ yyval.binstr = yypvt[-3].binstr; if(yyval.binstr->length()==0) yyval.binstr->appendInt8(NATIVE_TYPE_MAX); 
                                                                yyval.binstr->insertInt8(NATIVE_TYPE_ARRAY);
                                                                corEmitInt(yyval.binstr,0);
                                                                corEmitInt(yyval.binstr,yypvt[-1].int32); } break;
case 422:
#line 1081 "asmparse.y"
{ yyval.binstr = yypvt[-5].binstr; if(yyval.binstr->length()==0) yyval.binstr->appendInt8(NATIVE_TYPE_MAX); 
                                                                yyval.binstr->insertInt8(NATIVE_TYPE_ARRAY);
                                                                corEmitInt(yyval.binstr,yypvt[-1].int32);
                                                                corEmitInt(yyval.binstr,yypvt[-3].int32); } break;
case 423:
#line 1085 "asmparse.y"
{ yyval.binstr = yypvt[-4].binstr; if(yyval.binstr->length()==0) yyval.binstr->appendInt8(NATIVE_TYPE_MAX); 
                                                                yyval.binstr->insertInt8(NATIVE_TYPE_ARRAY);
                                                                corEmitInt(yyval.binstr,yypvt[-1].int32); } break;
case 424:
#line 1088 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_DECIMAL); 
																PASM->report->warn("Deprecated native type 'decimal'\n"); } break;
case 425:
#line 1090 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_DATE); 
																PASM->report->warn("Deprecated native type 'date'\n"); } break;
case 426:
#line 1092 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_BSTR); } break;
case 427:
#line 1093 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_LPSTR); } break;
case 428:
#line 1094 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_LPWSTR); } break;
case 429:
#line 1095 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_LPTSTR); } break;
case 430:
#line 1096 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_OBJECTREF); 
																PASM->report->warn("Deprecated native type 'objectref'\n"); } break;
case 431:
#line 1098 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_IUNKNOWN); } break;
case 432:
#line 1099 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_IDISPATCH); } break;
case 433:
#line 1100 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_STRUCT); } break;
case 434:
#line 1101 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_INTF); } break;
case 435:
#line 1102 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_SAFEARRAY); 
                                                                corEmitInt(yyval.binstr,yypvt[-0].int32); 
                                                                corEmitInt(yyval.binstr,0);} break;
case 436:
#line 1105 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_SAFEARRAY); 
                                                                corEmitInt(yyval.binstr,yypvt[-2].int32); 
                                                                corEmitInt(yyval.binstr,yypvt[-0].binstr->length()); yyval.binstr->append(yypvt[-0].binstr); } break;
case 437:
#line 1109 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_INT); } break;
case 438:
#line 1110 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_UINT); } break;
case 439:
#line 1111 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_UINT); } break;
case 440:
#line 1112 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_NESTEDSTRUCT); 
																PASM->report->warn("Deprecated native type 'nested struct'\n"); } break;
case 441:
#line 1114 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_BYVALSTR); } break;
case 442:
#line 1115 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_ANSIBSTR); } break;
case 443:
#line 1116 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_TBSTR); } break;
case 444:
#line 1117 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_VARIANTBOOL); } break;
case 445:
#line 1118 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_FUNC); } break;
case 446:
#line 1119 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_ASANY); } break;
case 447:
#line 1120 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(NATIVE_TYPE_LPSTRUCT); } break;
case 448:
#line 1123 "asmparse.y"
{ yyval.int32 = VT_EMPTY; } break;
case 449:
#line 1124 "asmparse.y"
{ yyval.int32 = VT_NULL; } break;
case 450:
#line 1125 "asmparse.y"
{ yyval.int32 = VT_VARIANT; } break;
case 451:
#line 1126 "asmparse.y"
{ yyval.int32 = VT_CY; } break;
case 452:
#line 1127 "asmparse.y"
{ yyval.int32 = VT_VOID; } break;
case 453:
#line 1128 "asmparse.y"
{ yyval.int32 = VT_BOOL; } break;
case 454:
#line 1129 "asmparse.y"
{ yyval.int32 = VT_I1; } break;
case 455:
#line 1130 "asmparse.y"
{ yyval.int32 = VT_I2; } break;
case 456:
#line 1131 "asmparse.y"
{ yyval.int32 = VT_I4; } break;
case 457:
#line 1132 "asmparse.y"
{ yyval.int32 = VT_I8; } break;
case 458:
#line 1133 "asmparse.y"
{ yyval.int32 = VT_R4; } break;
case 459:
#line 1134 "asmparse.y"
{ yyval.int32 = VT_R8; } break;
case 460:
#line 1135 "asmparse.y"
{ yyval.int32 = VT_UI1; } break;
case 461:
#line 1136 "asmparse.y"
{ yyval.int32 = VT_UI2; } break;
case 462:
#line 1137 "asmparse.y"
{ yyval.int32 = VT_UI4; } break;
case 463:
#line 1138 "asmparse.y"
{ yyval.int32 = VT_UI8; } break;
case 464:
#line 1139 "asmparse.y"
{ yyval.int32 = VT_UI1; } break;
case 465:
#line 1140 "asmparse.y"
{ yyval.int32 = VT_UI2; } break;
case 466:
#line 1141 "asmparse.y"
{ yyval.int32 = VT_UI4; } break;
case 467:
#line 1142 "asmparse.y"
{ yyval.int32 = VT_UI8; } break;
case 468:
#line 1143 "asmparse.y"
{ yyval.int32 = VT_PTR; } break;
case 469:
#line 1144 "asmparse.y"
{ yyval.int32 = yypvt[-2].int32 | VT_ARRAY; } break;
case 470:
#line 1145 "asmparse.y"
{ yyval.int32 = yypvt[-1].int32 | VT_VECTOR; } break;
case 471:
#line 1146 "asmparse.y"
{ yyval.int32 = yypvt[-1].int32 | VT_BYREF; } break;
case 472:
#line 1147 "asmparse.y"
{ yyval.int32 = VT_DECIMAL; } break;
case 473:
#line 1148 "asmparse.y"
{ yyval.int32 = VT_DATE; } break;
case 474:
#line 1149 "asmparse.y"
{ yyval.int32 = VT_BSTR; } break;
case 475:
#line 1150 "asmparse.y"
{ yyval.int32 = VT_LPSTR; } break;
case 476:
#line 1151 "asmparse.y"
{ yyval.int32 = VT_LPWSTR; } break;
case 477:
#line 1152 "asmparse.y"
{ yyval.int32 = VT_UNKNOWN; } break;
case 478:
#line 1153 "asmparse.y"
{ yyval.int32 = VT_DISPATCH; } break;
case 479:
#line 1154 "asmparse.y"
{ yyval.int32 = VT_SAFEARRAY; } break;
case 480:
#line 1155 "asmparse.y"
{ yyval.int32 = VT_INT; } break;
case 481:
#line 1156 "asmparse.y"
{ yyval.int32 = VT_UINT; } break;
case 482:
#line 1157 "asmparse.y"
{ yyval.int32 = VT_UINT; } break;
case 483:
#line 1158 "asmparse.y"
{ yyval.int32 = VT_ERROR; } break;
case 484:
#line 1159 "asmparse.y"
{ yyval.int32 = VT_HRESULT; } break;
case 485:
#line 1160 "asmparse.y"
{ yyval.int32 = VT_CARRAY; } break;
case 486:
#line 1161 "asmparse.y"
{ yyval.int32 = VT_USERDEFINED; } break;
case 487:
#line 1162 "asmparse.y"
{ yyval.int32 = VT_RECORD; } break;
case 488:
#line 1163 "asmparse.y"
{ yyval.int32 = VT_FILETIME; } break;
case 489:
#line 1164 "asmparse.y"
{ yyval.int32 = VT_BLOB; } break;
case 490:
#line 1165 "asmparse.y"
{ yyval.int32 = VT_STREAM; } break;
case 491:
#line 1166 "asmparse.y"
{ yyval.int32 = VT_STORAGE; } break;
case 492:
#line 1167 "asmparse.y"
{ yyval.int32 = VT_STREAMED_OBJECT; } break;
case 493:
#line 1168 "asmparse.y"
{ yyval.int32 = VT_STORED_OBJECT; } break;
case 494:
#line 1169 "asmparse.y"
{ yyval.int32 = VT_BLOB_OBJECT; } break;
case 495:
#line 1170 "asmparse.y"
{ yyval.int32 = VT_CF; } break;
case 496:
#line 1171 "asmparse.y"
{ yyval.int32 = VT_CLSID; } break;
case 497:
#line 1174 "asmparse.y"
{ if(yypvt[-0].token == PASM->m_tkSysString)
                                                                {     yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_STRING); }
                                                                else if(yypvt[-0].token == PASM->m_tkSysObject)
                                                                {     yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_OBJECT); }
                                                                else  
                                                                 yyval.binstr = parser->MakeTypeClass(ELEMENT_TYPE_CLASS, yypvt[-0].token); } break;
case 498:
#line 1180 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_OBJECT); } break;
case 499:
#line 1181 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_STRING); } break;
case 500:
#line 1182 "asmparse.y"
{ yyval.binstr = parser->MakeTypeClass(ELEMENT_TYPE_VALUETYPE, yypvt[-0].token); } break;
case 501:
#line 1183 "asmparse.y"
{ yyval.binstr = parser->MakeTypeClass(ELEMENT_TYPE_VALUETYPE, yypvt[-0].token); } break;
case 502:
#line 1184 "asmparse.y"
{ yyval.binstr = yypvt[-2].binstr; yyval.binstr->insertInt8(ELEMENT_TYPE_SZARRAY); } break;
case 503:
#line 1185 "asmparse.y"
{ yyval.binstr = parser->MakeTypeArray(ELEMENT_TYPE_ARRAY, yypvt[-3].binstr, yypvt[-1].binstr); } break;
case 504:
#line 1189 "asmparse.y"
{ yyval.binstr = yypvt[-1].binstr; yyval.binstr->insertInt8(ELEMENT_TYPE_BYREF); } break;
case 505:
#line 1190 "asmparse.y"
{ yyval.binstr = yypvt[-1].binstr; yyval.binstr->insertInt8(ELEMENT_TYPE_PTR); } break;
case 506:
#line 1191 "asmparse.y"
{ yyval.binstr = yypvt[-1].binstr; yyval.binstr->insertInt8(ELEMENT_TYPE_PINNED); } break;
case 507:
#line 1192 "asmparse.y"
{ yyval.binstr = parser->MakeTypeClass(ELEMENT_TYPE_CMOD_REQD, yypvt[-1].token);
                                                                yyval.binstr->append(yypvt[-4].binstr); } break;
case 508:
#line 1194 "asmparse.y"
{ yyval.binstr = parser->MakeTypeClass(ELEMENT_TYPE_CMOD_OPT, yypvt[-1].token);
                                                                yyval.binstr->append(yypvt[-4].binstr); } break;
case 509:
#line 1197 "asmparse.y"
{ yyval.binstr = parser->MakeSig(yypvt[-5].int32, yypvt[-4].binstr, yypvt[-1].binstr);
                                                                yyval.binstr->insertInt8(ELEMENT_TYPE_FNPTR); 
                                                                PASM->delArgNameList(PASM->m_firstArgName);
                                                                PASM->m_firstArgName = palDummyFirst;
                                                                PASM->m_lastArgName = palDummyLast;
                                                              } break;
case 510:
#line 1203 "asmparse.y"
{ yyval.binstr = new BinStr(); 
                                                                yyval.binstr->appendInt8(ELEMENT_TYPE_WITH); 
                                                                yyval.binstr->append(yypvt[-3].binstr);
                                                                yyval.binstr->appendInt8(corCountArgs(yypvt[-1].binstr)); 
                                                                yyval.binstr->append(yypvt[-1].binstr); delete yypvt[-3].binstr; delete yypvt[-1].binstr; } break;
case 511:
#line 1208 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_MVAR); yyval.binstr->appendInt8(yypvt[-0].int32); } break;
case 512:
#line 1209 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_VAR); yyval.binstr->appendInt8(yypvt[-0].int32); } break;
case 513:
#line 1210 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_TYPEDBYREF); } break;
case 514:
#line 1211 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_CHAR); } break;
case 515:
#line 1212 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_VOID); } break;
case 516:
#line 1213 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_BOOLEAN); } break;
case 517:
#line 1214 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_I1); } break;
case 518:
#line 1215 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_I2); } break;
case 519:
#line 1216 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_I4); } break;
case 520:
#line 1217 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_I8); } break;
case 521:
#line 1218 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_R4); } break;
case 522:
#line 1219 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_R8); } break;
case 523:
#line 1220 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U1); } break;
case 524:
#line 1221 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U2); } break;
case 525:
#line 1222 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U4); } break;
case 526:
#line 1223 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U8); } break;
case 527:
#line 1224 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U1); } break;
case 528:
#line 1225 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U2); } break;
case 529:
#line 1226 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U4); } break;
case 530:
#line 1227 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U8); } break;
case 531:
#line 1228 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_I); } break;
case 532:
#line 1229 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U); } break;
case 533:
#line 1230 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_U); } break;
case 534:
#line 1231 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt8(ELEMENT_TYPE_R); } break;
case 535:
#line 1234 "asmparse.y"
{ yyval.binstr = yypvt[-0].binstr; } break;
case 536:
#line 1235 "asmparse.y"
{ yyval.binstr = yypvt[-2].binstr; yypvt[-2].binstr->append(yypvt[-0].binstr); delete yypvt[-0].binstr; } break;
case 537:
#line 1238 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt32(0x7FFFFFFF); yyval.binstr->appendInt32(0x7FFFFFFF);  } break;
case 538:
#line 1239 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt32(0x7FFFFFFF); yyval.binstr->appendInt32(0x7FFFFFFF);  } break;
case 539:
#line 1240 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt32(0); yyval.binstr->appendInt32(yypvt[-0].int32); } break;
case 540:
#line 1241 "asmparse.y"
{ FAIL_UNLESS(yypvt[-2].int32 <= yypvt[-0].int32, ("lower bound %d must be <= upper bound %d\n", yypvt[-2].int32, yypvt[-0].int32));
                                                                if (yypvt[-2].int32 > yypvt[-0].int32) { YYERROR; };        
                                                                yyval.binstr = new BinStr(); yyval.binstr->appendInt32(yypvt[-2].int32); yyval.binstr->appendInt32(yypvt[-0].int32-yypvt[-2].int32+1); } break;
case 541:
#line 1244 "asmparse.y"
{ yyval.binstr = new BinStr(); yyval.binstr->appendInt32(yypvt[-1].int32); yyval.binstr->appendInt32(0x7FFFFFFF); } break;
case 542:
#line 1247 "asmparse.y"
{ yyval.labels = 0; } break;
case 543:
#line 1248 "asmparse.y"
{ yyval.labels = new Labels(yypvt[-2].string, yypvt[-0].labels, TRUE); } break;
case 544:
#line 1249 "asmparse.y"
{ yyval.labels = new Labels((char *)yypvt[-2].int32, yypvt[-0].labels, FALSE); } break;
case 545:
#line 1250 "asmparse.y"
{ yyval.labels = new Labels(yypvt[-0].string, NULL, TRUE); } break;
case 546:
#line 1251 "asmparse.y"
{ yyval.labels = new Labels((char *)yypvt[-0].int32, NULL, FALSE); } break;
case 547:
#line 1255 "asmparse.y"
{ yyval.string = yypvt[-0].string; } break;
case 548:
#line 1256 "asmparse.y"
{ yyval.string = yypvt[-0].string; } break;
case 549:
#line 1259 "asmparse.y"
{ yyval.binstr = new BinStr();  } break;
case 550:
#line 1260 "asmparse.y"
{ FAIL_UNLESS((yypvt[-0].int32 == (__int16) yypvt[-0].int32), ("Value %d too big\n", yypvt[-0].int32));
                                                                yyval.binstr = yypvt[-1].binstr; yyval.binstr->appendInt8(yypvt[-0].int32); yyval.binstr->appendInt8(yypvt[-0].int32 >> 8); } break;
case 551:
#line 1264 "asmparse.y"
{ yyval.int32 = (__int32)(*yypvt[-0].int64); delete yypvt[-0].int64; } break;
case 552:
#line 1267 "asmparse.y"
{ yyval.int64 = yypvt[-0].int64; } break;
case 553:
#line 1270 "asmparse.y"
{ yyval.float64 = yypvt[-0].float64; } break;
case 554:
#line 1271 "asmparse.y"
{ float f; *((__int32*) (&f)) = yypvt[-1].int32; yyval.float64 = new double(f); } break;
case 555:
#line 1272 "asmparse.y"
{ yyval.float64 = (double*) yypvt[-1].int64; } break;
case 556:
#line 1276 "asmparse.y"
{ PASM->AddPermissionDecl(yypvt[-4].secAct, yypvt[-3].token, yypvt[-1].pair); } break;
case 557:
#line 1277 "asmparse.y"
{ PASM->AddPermissionDecl(yypvt[-1].secAct, yypvt[-0].token, NULL); } break;
case 558:
#line 1278 "asmparse.y"
{ PASM->AddPermissionSetDecl(yypvt[-2].secAct, yypvt[-1].binstr); } break;
case 559:
#line 1280 "asmparse.y"
{ PASM->AddPermissionSetDecl(yypvt[-1].secAct,BinStrToUnicode(yypvt[-0].binstr));} break;
case 560:
#line 1283 "asmparse.y"
{ yyval.secAct = yypvt[-2].secAct; bParsingByteArray = TRUE; } break;
case 561:
#line 1286 "asmparse.y"
{ yyval.pair = yypvt[-0].pair; } break;
case 562:
#line 1287 "asmparse.y"
{ yyval.pair = yypvt[-2].pair->Concat(yypvt[-0].pair); } break;
case 563:
#line 1290 "asmparse.y"
{ yypvt[-2].binstr->appendInt8(0); yyval.pair = new NVPair(yypvt[-2].binstr, yypvt[-0].binstr); } break;
case 564:
#line 1293 "asmparse.y"
{ yyval.int32 = 1; } break;
case 565:
#line 1294 "asmparse.y"
{ yyval.int32 = 0; } break;
case 566:
#line 1297 "asmparse.y"
{ yyval.binstr = new BinStr();
                                                                yyval.binstr->appendInt8(SERIALIZATION_TYPE_BOOLEAN);
                                                                yyval.binstr->appendInt8(yypvt[-0].int32); } break;
case 567:
#line 1300 "asmparse.y"
{ yyval.binstr = new BinStr();
                                                                yyval.binstr->appendInt8(SERIALIZATION_TYPE_I4);
                                                                yyval.binstr->appendInt32(yypvt[-0].int32); } break;
case 568:
#line 1303 "asmparse.y"
{ yyval.binstr = new BinStr();
                                                                yyval.binstr->appendInt8(SERIALIZATION_TYPE_I4);
                                                                yyval.binstr->appendInt32(yypvt[-1].int32); } break;
case 569:
#line 1306 "asmparse.y"
{ yyval.binstr = new BinStr();
                                                                yyval.binstr->appendInt8(SERIALIZATION_TYPE_STRING);
                                                                yyval.binstr->append(yypvt[-0].binstr); delete yypvt[-0].binstr;
                                                                yyval.binstr->appendInt8(0); } break;
case 570:
#line 1310 "asmparse.y"
{ yyval.binstr = new BinStr();
                                                                yyval.binstr->appendInt8(SERIALIZATION_TYPE_ENUM);
                                                                char* sz = PASM->ReflectionNotation(yypvt[-5].token);
                                                                strcpy((char *)yyval.binstr->getBuff((unsigned)strlen(sz) + 1), sz);
                                                                yyval.binstr->appendInt8(1);
                                                                yyval.binstr->appendInt32(yypvt[-1].int32); } break;
case 571:
#line 1316 "asmparse.y"
{ yyval.binstr = new BinStr();
                                                                yyval.binstr->appendInt8(SERIALIZATION_TYPE_ENUM);
                                                                char* sz = PASM->ReflectionNotation(yypvt[-5].token);
                                                                strcpy((char *)yyval.binstr->getBuff((unsigned)strlen(sz) + 1), sz);
                                                                yyval.binstr->appendInt8(2);
                                                                yyval.binstr->appendInt32(yypvt[-1].int32); } break;
case 572:
#line 1322 "asmparse.y"
{ yyval.binstr = new BinStr();
                                                                yyval.binstr->appendInt8(SERIALIZATION_TYPE_ENUM);
                                                                char* sz = PASM->ReflectionNotation(yypvt[-5].token);
                                                                strcpy((char *)yyval.binstr->getBuff((unsigned)strlen(sz) + 1), sz);
                                                                yyval.binstr->appendInt8(4);
                                                                yyval.binstr->appendInt32(yypvt[-1].int32); } break;
case 573:
#line 1328 "asmparse.y"
{ yyval.binstr = new BinStr();
                                                                yyval.binstr->appendInt8(SERIALIZATION_TYPE_ENUM);
                                                                char* sz = PASM->ReflectionNotation(yypvt[-3].token);
                                                                strcpy((char *)yyval.binstr->getBuff((unsigned)strlen(sz) + 1), sz);
                                                                yyval.binstr->appendInt8(4);
                                                                yyval.binstr->appendInt32(yypvt[-1].int32); } break;
case 574:
#line 1336 "asmparse.y"
{ yyval.secAct = dclRequest; } break;
case 575:
#line 1337 "asmparse.y"
{ yyval.secAct = dclDemand; } break;
case 576:
#line 1338 "asmparse.y"
{ yyval.secAct = dclAssert; } break;
case 577:
#line 1339 "asmparse.y"
{ yyval.secAct = dclDeny; } break;
case 578:
#line 1340 "asmparse.y"
{ yyval.secAct = dclPermitOnly; } break;
case 579:
#line 1341 "asmparse.y"
{ yyval.secAct = dclLinktimeCheck; } break;
case 580:
#line 1342 "asmparse.y"
{ yyval.secAct = dclInheritanceCheck; } break;
case 581:
#line 1343 "asmparse.y"
{ yyval.secAct = dclRequestMinimum; } break;
case 582:
#line 1344 "asmparse.y"
{ yyval.secAct = dclRequestOptional; } break;
case 583:
#line 1345 "asmparse.y"
{ yyval.secAct = dclRequestRefuse; } break;
case 584:
#line 1346 "asmparse.y"
{ yyval.secAct = dclPrejitGrant; } break;
case 585:
#line 1347 "asmparse.y"
{ yyval.secAct = dclPrejitDenied; } break;
case 586:
#line 1348 "asmparse.y"
{ yyval.secAct = dclNonCasDemand; } break;
case 587:
#line 1349 "asmparse.y"
{ yyval.secAct = dclNonCasLinkDemand; } break;
case 588:
#line 1350 "asmparse.y"
{ yyval.secAct = dclNonCasInheritance; } break;
case 589:
#line 1353 "asmparse.y"
{ nCurrPC = PASM->m_CurPC; bExternSource = TRUE;} break;
case 590:
#line 1354 "asmparse.y"
{ nCurrPC = PASM->m_CurPC; bExternSource = TRUE;} break;
case 591:
#line 1357 "asmparse.y"
{ nExtLine = nExtLineEnd = yypvt[-1].int32;
                                                                nExtCol = 1; nExtColEnd  = 1;
                                                                PASM->SetSourceFileName(yypvt[-0].string);} break;
case 592:
#line 1360 "asmparse.y"
{ nExtLine = nExtLineEnd = yypvt[-0].int32;
                                                                nExtCol = 1; nExtColEnd  = 1; } break;
case 593:
#line 1362 "asmparse.y"
{ nExtLine = nExtLineEnd = yypvt[-3].int32; 
                                                                nExtCol=yypvt[-1].int32; nExtColEnd = nExtCol;
                                                                PASM->SetSourceFileName(yypvt[-0].string);} break;
case 594:
#line 1365 "asmparse.y"
{ nExtLine = nExtLineEnd = yypvt[-2].int32; 
                                                                nExtCol=yypvt[-0].int32; nExtColEnd = nExtCol;} break;
case 595:
#line 1368 "asmparse.y"
{ nExtLine = nExtLineEnd = yypvt[-5].int32; 
                                                                nExtCol=yypvt[-3].int32; nExtColEnd = yypvt[-1].int32;
                                                                PASM->SetSourceFileName(yypvt[-0].string);} break;
case 596:
#line 1372 "asmparse.y"
{ nExtLine = nExtLineEnd = yypvt[-4].int32; 
                                                                nExtCol=yypvt[-2].int32; nExtColEnd = yypvt[-0].int32; } break;
case 597:
#line 1375 "asmparse.y"
{ nExtLine = yypvt[-5].int32; nExtLineEnd = yypvt[-3].int32; 
                                                                nExtCol=yypvt[-1].int32; nExtColEnd = nExtCol;
                                                                PASM->SetSourceFileName(yypvt[-0].string);} break;
case 598:
#line 1379 "asmparse.y"
{ nExtLine = yypvt[-4].int32; nExtLineEnd = yypvt[-2].int32; 
                                                                nExtCol=yypvt[-0].int32; nExtColEnd = nExtCol; } break;
case 599:
#line 1382 "asmparse.y"
{ bExternSource = TRUE; 
                                                                nExtLine = yypvt[-7].int32; nExtLineEnd = yypvt[-5].int32; 
                                                                nExtCol=yypvt[-3].int32; nExtColEnd = yypvt[-1].int32;
                                                                PASM->SetSourceFileName(yypvt[-0].string);} break;
case 600:
#line 1387 "asmparse.y"
{ nExtLine = yypvt[-6].int32; nExtLineEnd = yypvt[-4].int32; 
                                                                nExtCol=yypvt[-2].int32; nExtColEnd = yypvt[-0].int32; } break;
case 601:
#line 1389 "asmparse.y"
{ nExtLine = nExtLineEnd = yypvt[-1].int32;
                                                                nExtCol = 1; nExtColEnd  = 1;
                                                                PASM->SetSourceFileName(yypvt[-0].binstr);} break;
case 602:
#line 1395 "asmparse.y"
{ PASMM->AddFile(yypvt[-5].string, yypvt[-6].fileAttr|yypvt[-4].fileAttr|yypvt[-0].fileAttr, yypvt[-2].binstr); } break;
case 603:
#line 1396 "asmparse.y"
{ PASMM->AddFile(yypvt[-1].string, yypvt[-2].fileAttr|yypvt[-0].fileAttr, NULL); } break;
case 604:
#line 1399 "asmparse.y"
{ yyval.fileAttr = (CorFileFlags) 0; } break;
case 605:
#line 1400 "asmparse.y"
{ yyval.fileAttr = (CorFileFlags) (yypvt[-1].fileAttr | ffContainsNoMetaData); } break;
case 606:
#line 1403 "asmparse.y"
{ yyval.fileAttr = (CorFileFlags) 0; } break;
case 607:
#line 1404 "asmparse.y"
{ yyval.fileAttr = (CorFileFlags) 0x80000000; } break;
case 608:
#line 1407 "asmparse.y"
{ bParsingByteArray = TRUE; } break;
case 609:
#line 1410 "asmparse.y"
{ PASMM->StartAssembly(yypvt[-0].string, NULL, (DWORD)yypvt[-1].asmAttr, FALSE); } break;
case 610:
#line 1413 "asmparse.y"
{ yyval.asmAttr = (CorAssemblyFlags) 0; } break;
case 611:
#line 1414 "asmparse.y"
{ yyval.asmAttr = (CorAssemblyFlags) (yypvt[-1].asmAttr | afNonSideBySideAppDomain); } break;
case 612:
#line 1415 "asmparse.y"
{ yyval.asmAttr = (CorAssemblyFlags) (yypvt[-1].asmAttr | afNonSideBySideProcess); } break;
case 613:
#line 1416 "asmparse.y"
{ yyval.asmAttr = (CorAssemblyFlags) (yypvt[-1].asmAttr | afNonSideBySideMachine); } break;
case 616:
#line 1423 "asmparse.y"
{ PASMM->SetAssemblyHashAlg(yypvt[-0].int32); } break;
case 619:
#line 1428 "asmparse.y"
{ PASMM->SetAssemblyPublicKey(yypvt[-1].binstr); } break;
case 620:
#line 1430 "asmparse.y"
{ PASMM->SetAssemblyVer((USHORT)yypvt[-6].int32, (USHORT)yypvt[-4].int32, (USHORT)yypvt[-2].int32, (USHORT)yypvt[-0].int32); } break;
case 621:
#line 1431 "asmparse.y"
{ yypvt[-0].binstr->appendInt8(0); PASMM->SetAssemblyLocale(yypvt[-0].binstr,TRUE); } break;
case 622:
#line 1432 "asmparse.y"
{ PASMM->SetAssemblyLocale(yypvt[-1].binstr,FALSE); } break;
case 624:
#line 1436 "asmparse.y"
{ bParsingByteArray = TRUE; } break;
case 625:
#line 1439 "asmparse.y"
{ bParsingByteArray = TRUE; } break;
case 626:
#line 1442 "asmparse.y"
{ bParsingByteArray = TRUE; } break;
case 627:
#line 1445 "asmparse.y"
{ PASMM->StartAssembly(yypvt[-0].string, NULL, 0, TRUE); } break;
case 628:
#line 1446 "asmparse.y"
{ PASMM->StartAssembly(yypvt[-2].string, yypvt[-0].string, 0, TRUE); } break;
case 631:
#line 1453 "asmparse.y"
{ PASMM->SetAssemblyHashBlob(yypvt[-1].binstr); } break;
case 633:
#line 1455 "asmparse.y"
{ PASMM->SetAssemblyPublicKeyToken(yypvt[-1].binstr); } break;
case 634:
#line 1458 "asmparse.y"
{ PASMM->StartComType(yypvt[-0].string, yypvt[-1].comtAttr);} break;
case 635:
#line 1461 "asmparse.y"
{ PASMM->StartComType(yypvt[-0].string, yypvt[-1].comtAttr); } break;
case 636:
#line 1464 "asmparse.y"
{ yyval.comtAttr = (CorTypeAttr) 0; } break;
case 637:
#line 1465 "asmparse.y"
{ yyval.comtAttr = (CorTypeAttr) (yypvt[-1].comtAttr | tdNotPublic); } break;
case 638:
#line 1466 "asmparse.y"
{ yyval.comtAttr = (CorTypeAttr) (yypvt[-1].comtAttr | tdPublic); } break;
case 639:
#line 1467 "asmparse.y"
{ yyval.comtAttr = (CorTypeAttr) (yypvt[-2].comtAttr | tdNestedPublic); } break;
case 640:
#line 1468 "asmparse.y"
{ yyval.comtAttr = (CorTypeAttr) (yypvt[-2].comtAttr | tdNestedPrivate); } break;
case 641:
#line 1469 "asmparse.y"
{ yyval.comtAttr = (CorTypeAttr) (yypvt[-2].comtAttr | tdNestedFamily); } break;
case 642:
#line 1470 "asmparse.y"
{ yyval.comtAttr = (CorTypeAttr) (yypvt[-2].comtAttr | tdNestedAssembly); } break;
case 643:
#line 1471 "asmparse.y"
{ yyval.comtAttr = (CorTypeAttr) (yypvt[-2].comtAttr | tdNestedFamANDAssem); } break;
case 644:
#line 1472 "asmparse.y"
{ yyval.comtAttr = (CorTypeAttr) (yypvt[-2].comtAttr | tdNestedFamORAssem); } break;
case 647:
#line 1479 "asmparse.y"
{ PASMM->SetComTypeFile(yypvt[-0].string); } break;
case 648:
#line 1480 "asmparse.y"
{ PASMM->SetComTypeComType(yypvt[-0].string); } break;
case 649:
#line 1481 "asmparse.y"
{ PASMM->SetComTypeClassTok(yypvt[-0].int32); } break;
case 651:
#line 1485 "asmparse.y"
{ PASMM->StartManifestRes(yypvt[-0].string, yypvt[-1].manresAttr); } break;
case 652:
#line 1488 "asmparse.y"
{ yyval.manresAttr = (CorManifestResourceFlags) 0; } break;
case 653:
#line 1489 "asmparse.y"
{ yyval.manresAttr = (CorManifestResourceFlags) (yypvt[-1].manresAttr | mrPublic); } break;
case 654:
#line 1490 "asmparse.y"
{ yyval.manresAttr = (CorManifestResourceFlags) (yypvt[-1].manresAttr | mrPrivate); } break;
case 657:
#line 1497 "asmparse.y"
{ PASMM->SetManifestResFile(yypvt[-2].string, (ULONG)yypvt[-0].int32); } break;
case 658:
#line 1498 "asmparse.y"
{ PASMM->SetManifestResAsmRef(yypvt[-0].string); } break;/* End of actions */
#line 329 "yypars.c"
			}
		}
		goto yystack;  /* stack new state and value */
	}
#ifdef _MSC_VER
#pragma warning(default:102)
#endif

#ifdef YYDUMP
YYLOCAL void YYNEAR YYPASCAL yydumpinfo(void)
{
	short stackindex;
	short valindex;

	//dump yys
	printf("short yys[%d] {\n", YYMAXDEPTH);
	for (stackindex = 0; stackindex < YYMAXDEPTH; stackindex++){
		if (stackindex)
			printf(", %s", stackindex % 10 ? "\0" : "\n");
		printf("%6d", yys[stackindex]);
		}
	printf("\n};\n");

	//dump yyv
	printf("YYSTYPE yyv[%d] {\n", YYMAXDEPTH);
	for (valindex = 0; valindex < YYMAXDEPTH; valindex++){
		if (valindex)
			printf(", %s", valindex % 5 ? "\0" : "\n");
		printf("%#*x", 3+sizeof(YYSTYPE), yyv[valindex]);
		}
	printf("\n};\n");
	}
#endif
