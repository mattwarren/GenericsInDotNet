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
/************************************************************************/
/*                           Assembler.h                                */
/************************************************************************/

#ifndef Assember_h
#define Assember_h

#define NEW_INLINE_NAMES

#include "cor.h"        // for CorMethodAttr ...
#include <crtdbg.h>     // For _ASSERTE
#include <corsym.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "utilcode.h"
#include "debugmacros.h"
#include "corpriv.h"
#include <sighelper.h>
//#include "asmparse.h"
#include "binstr.h"
#include "typar.hpp"

#include "asmenum.h"

#define OUTPUT_BUFFER_SIZE          8192    // initial size of asm code for a single method
#define OUTPUT_BUFFER_INCREMENT     1024    // size of code buffer increment when it's full
#define MAX_FILENAME_LENGTH         512     //256
#define MAX_SIGNATURE_LENGTH        256     // unused
#define MAX_LABEL_SIZE              256     //64
#define MAX_CALL_SIG_SIZE           32      // unused
#define MAX_SCOPE_LENGTH            256     //64

#define MAX_NAMESPACE_LENGTH        1024    //256    //64
#define MAX_MEMBER_NAME_LENGTH      1024    //256    //64

#define MAX_INTERFACES_IMPLEMENTED  16      // initial number; extended by 16 when needed
#define GLOBAL_DATA_SIZE            8192    // initial size of global data buffer
#define GLOBAL_DATA_INCREMENT       1024    // size of global data buffer increment when it's full
#define MAX_METHODS                 1024    // unused
#define MAX_INPUT_LINE_LEN          1024    // unused
#define MAX_TYPAR                       8
#define BASE_OBJECT_CLASSNAME   "System.Object"

// Fully-qualified class name separators:
#define NESTING_SEP     0xF8

extern WCHAR   wzUniBuf[]; // Unicode conversion global buffer (assem.cpp)
extern DWORD   dwUniBuf;   // Size of Unicode global buffer (8196)

class Class;
class Method;
class PermissionDecl;
class PermissionSetDecl;


/*****************************************************************************/
/* LIFO (stack) and FIFO (queue) templates (must precede #include "method.h")*/
template <class T>
class LIST_EL
{
public:
    T*  m_Ptr;
    LIST_EL <T> *m_Next;
    LIST_EL(T *item) {m_Next = NULL; m_Ptr = item; };
};
    
template <class T>
class LIFO
{
public:
    inline LIFO() { m_pHead = NULL; };
    inline ~LIFO() {T *val; while(val = POP()) delete val; };
    void PUSH(T *item) 
    {
        m_pTemp = new LIST_EL <T>(item); 
        m_pTemp->m_Next = m_pHead; 
        m_pHead = m_pTemp;
    };
    T* POP() 
    {
        T* ret = NULL;
        if(m_pTemp = m_pHead)
        {
            m_pHead = m_pHead->m_Next;
            ret = m_pTemp->m_Ptr;
            delete m_pTemp;
        }
        return ret;
    };
private:
    LIST_EL <T> *m_pHead;
    LIST_EL <T> *m_pTemp;
};
template <class T>
class FIFO
{
public:
    FIFO() { m_Arr = NULL; m_ulArrLen = 0; m_ulCount = 0; m_ulOffset = 0; };
    ~FIFO() {
        if(m_Arr) {
            for(ULONG i=0; i < m_ulCount; i++) {
                if(m_Arr[i+m_ulOffset]) delete m_Arr[i+m_ulOffset];
            }
            delete [] m_Arr;
        }
    };
    void RESET() {
        if(m_Arr) {
            for(ULONG i=0; i < m_ulCount; i++) {
                if(m_Arr[i+m_ulOffset]) delete m_Arr[i+m_ulOffset];
            }
            m_ulCount = 0;
            m_ulOffset= 0;
        }
    };
    void PUSH(T *item) 
    {
		if(item)
		{
			if(m_ulCount+m_ulOffset >= m_ulArrLen)
			{
				if(m_ulOffset)
				{
					memcpy(m_Arr,&m_Arr[m_ulOffset],m_ulCount*sizeof(T*));
					m_ulOffset = 0;
				}
				else
				{
					m_ulArrLen += 1024;
					T** tmp = new T*[m_ulArrLen];
					if(tmp)
					{
						if(m_Arr)
						{
							memcpy(tmp,m_Arr,m_ulCount*sizeof(T*));
							delete [] m_Arr;
						}
						m_Arr = tmp;
					}
					else fprintf(stderr,"\nOut of memory!\n");
				}
			}
			m_Arr[m_ulOffset+m_ulCount] = item;
			m_ulCount++;
		}
    };
    ULONG COUNT() { return m_ulCount; };
    T* POP() 
    {
        T* ret = NULL;
        if(m_ulCount)
        {
            ret = m_Arr[m_ulOffset++];
            m_ulCount--;
        }
        return ret;
    };
    T* PEEK(ULONG idx) { return (idx < m_ulCount) ? m_Arr[m_ulOffset+idx] : NULL; };
private:
    T** m_Arr;
    ULONG       m_ulCount;
    ULONG       m_ulOffset;
    ULONG       m_ulArrLen;
};
template <class T>
class SORTEDARRAY
{
public:
    SORTEDARRAY() { m_Arr = NULL; m_ulArrLen = 0; m_ulCount = 0; m_ulOffset = 0; };
    ~SORTEDARRAY() {
        if(m_Arr) {
            for(ULONG i=0; i < m_ulCount; i++) {
                if(m_Arr[i+m_ulOffset]) delete m_Arr[i+m_ulOffset];
            }
            delete [] m_Arr;
        }
    };
    void RESET() {
        if(m_Arr) {
            for(ULONG i=0; i < m_ulCount; i++) {
                if(m_Arr[i+m_ulOffset]) delete m_Arr[i+m_ulOffset];
            }
            m_ulCount = 0;
            m_ulOffset= 0;
        }
    };
    void PUSH(T *item) 
    {
		if(item)
		{
			if(m_ulCount+m_ulOffset >= m_ulArrLen)
			{
				if(m_ulOffset)
				{
					memcpy(m_Arr,&m_Arr[m_ulOffset],m_ulCount*sizeof(T*));
					m_ulOffset = 0;
				}
				else
				{
					m_ulArrLen += 1024;
					T** tmp = new T*[m_ulArrLen];
					if(tmp)
					{
						if(m_Arr)
						{
							memcpy(tmp,m_Arr,m_ulCount*sizeof(T*));
							delete [] m_Arr;
						}
						m_Arr = tmp;
					}
					else fprintf(stderr,"\nOut of memory!\n");
				}
			}
            if(m_ulCount)
            {
                // find  1st arr.element > item
                T** low = &m_Arr[m_ulOffset];
                T** high = &m_Arr[m_ulOffset+m_ulCount-1];
                T** mid;
            
                if(item->ComparedTo(*high) > 0) mid = high+1;
                else if(item->ComparedTo(*low) < 0) mid = low;
                else for(;;) 
                {
                    mid = &low[(high - low) >> 1];
            
                    int cmp = item->ComparedTo(*mid);
            
                    if (mid == low)
                    {
                        if(cmp > 0) mid++;
                        break;
                    }
            
                    if (cmp > 0) low = mid;
                    else        high = mid;
                }

                /////////////////////////////////////////////
                 memmove(mid+1,mid,(BYTE*)&m_Arr[m_ulOffset+m_ulCount]-(BYTE*)mid);
                *mid = item;
            }
			else m_Arr[m_ulOffset+m_ulCount] = item;
			m_ulCount++;
		}
    };
    ULONG COUNT() { return m_ulCount; };
    T* POP() 
    {
        T* ret = NULL;
        if(m_ulCount)
        {
            ret = m_Arr[m_ulOffset++];
            m_ulCount--;
        }
        return ret;
    };
    T* PEEK(ULONG idx) { return (idx < m_ulCount) ? m_Arr[m_ulOffset+idx] : NULL; };
    T* FIND(T* item)
    {
        if(m_ulCount)
        {
            T** low = &m_Arr[m_ulOffset];
            T** high = &m_Arr[m_ulOffset+m_ulCount-1];
            T** mid;
            if(item->ComparedTo(*high) == 0) return(*high);
            for(;;) 
            {
                mid = &low[(high - low) >> 1];
                int cmp = item->ComparedTo(*mid);
                if (cmp == 0) return(*mid);
                if (mid == low)  break;
                if (cmp > 0) low = mid;
                else        high = mid;
            }
        }
        return NULL;
    }
private:
    T** m_Arr;
    ULONG       m_ulCount;
    ULONG       m_ulOffset;
    ULONG       m_ulArrLen;
};

struct MemberRefDescriptor
{
    mdToken             m_tdClass;
    Class*              m_pClass;
    char*               m_szName;
    DWORD               m_dwName;
    BinStr*             m_pSigBinStr;
    mdToken             m_tkResolved;
};
typedef FIFO<MemberRefDescriptor> MemberRefDList;


struct MethodImplDescriptor
{
    mdToken             m_tkImplementedMethod;
    mdToken             m_tkImplementingMethod;
    mdToken             m_tkDefiningClass;
};
typedef FIFO<MethodImplDescriptor> MethodImplDList;

struct LocalMemberRefFixup
{
    mdToken tk;
    size_t  offset;
    LocalMemberRefFixup(mdToken TK, size_t Offset)
    {
        tk = TK;
        offset = Offset;
    }
};
typedef FIFO<LocalMemberRefFixup> LocalMemberRefFixupList;

/**************************************************************************/
#include "method.hpp"
#include "iceefilegen.h"
#include "asmman.hpp"

#include "nvpair.h"


typedef enum
{
    STATE_OK,
    STATE_FAIL,
    STATE_ENDMETHOD,
    STATE_ENDFILE
} state_t;


class Label
{
public:
    char*   m_szName;
    DWORD   m_PC;

    Label(char *pszName, DWORD PC)
    {
        m_PC    = PC;
        m_szName = pszName;
    };
    ~Label(){ delete [] m_szName; };
    int ComparedTo(Label* L) { return strcmp(m_szName,L->m_szName); };
};
typedef SORTEDARRAY<Label> LabelList;

class GlobalLabel
{
public:
    char*           m_szName;
    DWORD           m_GlobalOffset; 
    HCEESECTION     m_Section;

    GlobalLabel(char *pszName, DWORD GlobalOffset, HCEESECTION section)
    {
        m_GlobalOffset  = GlobalOffset;
        m_Section       = section;
        m_szName = pszName;
    }
    ~GlobalLabel(){ delete [] m_szName; }
    int ComparedTo(GlobalLabel* L) { return strcmp(m_szName,L->m_szName); };
};
typedef SORTEDARRAY<GlobalLabel> GlobalLabelList;


class Fixup
{
public:
    char*   m_szLabel;
    BYTE *  m_pBytes; // where to make the fixup
    DWORD   m_RelativeToPC;
    BYTE    m_FixupSize;

    Fixup(char *pszName, BYTE *pBytes, DWORD RelativeToPC, BYTE FixupSize)
    {
        m_pBytes        = pBytes;
        m_RelativeToPC  = RelativeToPC;
        m_FixupSize     = FixupSize;
        m_szLabel = pszName;
    }
    ~Fixup(){ delete [] m_szLabel; }
};
typedef FIFO<Fixup> FixupList;

class GlobalFixup
{
public:
    char*   m_szLabel;
    BYTE *  m_pReference;               // The place to fix up

    GlobalFixup(char *pszName, BYTE* pReference)
    {
        m_pReference   = pReference;
        m_szLabel = pszName;
    }
    ~GlobalFixup(){ delete [] m_szLabel; }
};
typedef FIFO<GlobalFixup> GlobalFixupList;


typedef enum { ilRVA, ilToken, ilGlobal} ILFixupType;

class ILFixup
{
public:
    ILFixupType   m_Kind;
    DWORD         m_OffsetInMethod;
    GlobalFixup * m_Fixup;

    ILFixup(DWORD Offset, ILFixupType Kind, GlobalFixup *Fix)
    { 
      m_Kind           = Kind;
      m_OffsetInMethod = Offset;
      m_Fixup          = Fix;
    }
};
typedef FIFO<ILFixup> ILFixupList;

class CeeFileGenWriter;
class CeeSection;

class BinStr;

/************************************************************************/
/* represents an object that knows how to report errors back to the user */

class ErrorReporter
{
public:
    virtual void error(const char* fmt, ...) = 0;
    virtual void warn(const char* fmt, ...) = 0;
    virtual void msg(const char* fmt, ...) = 0; 
};

/**************************************************************************/
/* represents a switch table before the lables are bound */

struct Labels {
    Labels(char* aLabel, Labels* aNext, bool aIsLabel) : Label(aLabel), Next(aNext), isLabel(aIsLabel) {}
    ~Labels() { if(isLabel && Label) delete [] Label; delete Next; }
        
    char*       Label;
    Labels*     Next;
    bool        isLabel;
};

/**************************************************************************/
/* descriptor of the structured exception handling construct  */
struct SEH_Descriptor
{
    DWORD       sehClause;  // catch/filter/finally
    DWORD       tryFrom;    // start of try block
    DWORD       tryTo;      // end of try block
    DWORD       sehHandler; // start of exception handler
    DWORD       sehHandlerTo; // end of exception handler
    union {
        DWORD       sehFilter;  // start of filter block
        mdTypeRef   cException; // what to catch
    };
};


typedef LIFO<char> StringStack;
typedef LIFO<SEH_Descriptor> SEHD_Stack;

typedef FIFO<Method> MethodList;
typedef FIFO<mdToken> TokenList;
/**************************************************************************/
/* The field, event and property descriptor structures            */

struct FieldDescriptor
{
    mdTypeDef       m_tdClass;
    char*           m_szName;
    DWORD           m_dwName;
    mdFieldDef      m_fdFieldTok;
    ULONG           m_ulOffset;
    char*           m_rvaLabel;         // if field has RVA associated with it, label for it goes here. 
    BinStr*         m_pbsSig;
	Class*			m_pClass;
	BinStr*			m_pbsValue;
	BinStr*			m_pbsMarshal;
	PInvokeDescriptor*	m_pPInvoke;
    CustomDescrList     m_CustomDescrList;
	DWORD			m_dwAttr;
    // Security attributes
    PermissionDecl* m_pPermissions;
    PermissionSetDecl* m_pPermissionSets;
    FieldDescriptor()  { m_szName = NULL; m_pbsSig = NULL; };
    ~FieldDescriptor() { if(m_szName) delete m_szName; if(m_pbsSig) delete m_pbsSig; };
};
typedef FIFO<FieldDescriptor> FieldDList;

struct EventDescriptor
{
    mdTypeDef           m_tdClass;
    char*               m_szName;
    DWORD               m_dwAttr;
    mdToken             m_tkEventType;
    mdToken             m_tkAddOn;
    mdToken             m_tkRemoveOn;
    mdToken             m_tkFire;
    TokenList           m_tklOthers;
    mdEvent             m_edEventTok;
    CustomDescrList     m_CustomDescrList;
};
typedef FIFO<EventDescriptor> EventDList;

struct PropDescriptor
{
    mdTypeDef           m_tdClass;
    char*               m_szName;
    DWORD               m_dwAttr;
    COR_SIGNATURE*      m_pSig;
    DWORD               m_dwCSig;
    DWORD               m_dwCPlusTypeFlag;
    PVOID               m_pValue;
	DWORD				m_cbValue;
    mdToken             m_tkSet;
    mdToken             m_tkGet;
    TokenList           m_tklOthers;
    mdProperty          m_pdPropTok;
    CustomDescrList     m_CustomDescrList;
};
typedef FIFO<PropDescriptor> PropDList;

struct ImportDescriptor
{
//    char*   szDllName;
    char   szDllName[MAX_MEMBER_NAME_LENGTH];
    DWORD  dwDllName;
    mdModuleRef mrDll;
};
typedef FIFO<ImportDescriptor> ImportList;


/**************************************************************************/
#include "class.hpp"
typedef LIFO<Class> ClassStack;
typedef FIFO<Class> ClassList;

/**************************************************************************/
/* Classes to hold lists of security permissions and permission sets. We build
   these lists as we find security directives in the input stream and drain
   them every time we see a class or method declaration (to which the
   security info is attached). */

class PermissionDecl
{
public:
    PermissionDecl(CorDeclSecurity action, mdToken type, NVPair *pairs)
    {
        m_Action = action;
        m_TypeSpec = type;
        BuildConstructorBlob(action, pairs);
        m_Next = NULL;
    }

    ~PermissionDecl()
    {
        delete [] m_Blob;
    }

    CorDeclSecurity     m_Action;
    mdToken             m_TypeSpec;
    BYTE               *m_Blob;
    long                m_BlobLength;
    PermissionDecl     *m_Next;

private:
    void BuildConstructorBlob(CorDeclSecurity action, NVPair *pairs)
    {
        NVPair *p = pairs;
        int count = 0;
        int bytes = 8;
        int length;
        int i;
        BYTE *pBlob;

        // Calculate number of name/value pairs and the memory required for the
        // custom attribute blob.
        while (p) {
            BYTE *pVal = (BYTE*)p->Value()->ptr();
            count++;
            bytes += 2; // One byte field/property specifier, one byte type code

            length = (int)strlen((const char *)p->Name()->ptr());
            bytes += CPackedLen::Size(length) + length;

            switch (pVal[0]) {
            case SERIALIZATION_TYPE_BOOLEAN:
                bytes += 1;
                break;
            case SERIALIZATION_TYPE_I4:
                bytes += 4;
                break;
            case SERIALIZATION_TYPE_STRING:
                length = (int)strlen((const char *)&pVal[1]);
                bytes += CPackedLen::Size(length) + length;
                break;
            case SERIALIZATION_TYPE_ENUM:
                length = (int)strlen((const char *)&pVal[1]);
                bytes += CPackedLen::Size((ULONG)length) + length;
                bytes += 4;
                break;
            }
            p = p->Next();
        }

        m_Blob = new BYTE[bytes];
		if(m_Blob==NULL)
		{
			fprintf(stderr,"\nOut of memory!\n");
			return;
		}

        m_Blob[0] = 0x01;           // Version
        m_Blob[1] = 0x00;
        m_Blob[2] = (BYTE)action;   // Constructor arg (security action code)
        m_Blob[3] = 0x00;
        m_Blob[4] = 0x00;
        m_Blob[5] = 0x00;
        m_Blob[6] = (BYTE)count;    // Property/field count
        m_Blob[7] = (BYTE)(count >> 8);

        for (i = 0, pBlob = &m_Blob[8], p = pairs; i < count; i++, p = p->Next()) {
            BYTE *pVal = (BYTE*)p->Value()->ptr();
            char *szType;

            // Set field/property setter type.
            *pBlob++ = SERIALIZATION_TYPE_PROPERTY;

            // Set type code. There's additional info for enums (the enum class
            // name).
            *pBlob++ = pVal[0];
            if (pVal[0] == SERIALIZATION_TYPE_ENUM) {
                szType = (char *)&pVal[1];
                length = (int)strlen(szType);
                pBlob = (BYTE*)CPackedLen::PutLength(pBlob, length);
                strcpy((char *)pBlob, szType);
                pBlob += length;
            }

            // Record the field/property name.
            length = (int)strlen((const char *)p->Name()->ptr());
            pBlob = (BYTE*)CPackedLen::PutLength(pBlob, length);
            strcpy((char *)pBlob, (const char *)p->Name()->ptr());
            pBlob += length;

            // Record the serialized value.
            switch (pVal[0]) {
            case SERIALIZATION_TYPE_BOOLEAN:
                *pBlob++ = pVal[1];
                break;
            case SERIALIZATION_TYPE_I4:
                *(__int32*)pBlob = *(__int32*)&pVal[1];
                pBlob += 4;
                break;
            case SERIALIZATION_TYPE_STRING:
                length = (int)strlen((const char *)&pVal[1]);
                pBlob = (BYTE*)CPackedLen::PutLength(pBlob, length);
                strcpy((char *)pBlob, (const char *)&pVal[1]);
                pBlob += length;
                break;
            case SERIALIZATION_TYPE_ENUM:
                length = (int)strlen((const char *)&pVal[1]);
                // We can have enums with base type of I1, I2 and I4.
                switch (pVal[1 + length + 1]) {
                case 1:
                    *(__int8*)pBlob = *(__int8*)&pVal[1 + length + 2];
                    pBlob += 1;
                    break;
                case 2:
                    *(__int16*)pBlob = *(__int16*)&pVal[1 + length + 2];
                    pBlob += 2;
                    break;
                case 4:
                    *(__int32*)pBlob = *(__int32*)&pVal[1 + length + 2];
                    pBlob += 4;
                    break;
                default:
                    _ASSERTE(!"Invalid enum size");
                }
                break;
            }

        }

        _ASSERTE((pBlob - m_Blob) == bytes);

        m_BlobLength = (long)bytes;
    }
};

class PermissionSetDecl
{
public:
    PermissionSetDecl(CorDeclSecurity action, BinStr *value)
    {
        m_Action = action;
        m_Value = value;
        m_Next = NULL;
    }

    ~PermissionSetDecl()
    {
        delete m_Value;
    }

    CorDeclSecurity     m_Action;
    BinStr             *m_Value;
    PermissionSetDecl  *m_Next;
};

struct VTFEntry
{
    char*   m_szLabel;
    WORD    m_wCount;
    WORD    m_wType;
    VTFEntry(WORD wCount, WORD wType, char* szLabel) { m_wCount = wCount; m_wType = wType; m_szLabel = szLabel; }
    ~VTFEntry() { if(m_szLabel) delete m_szLabel; }
};
typedef FIFO<VTFEntry> VTFList;

struct	EATEntry
{
	DWORD	dwStubRVA;
	DWORD	dwOrdinal;
	char*	szAlias;
};
typedef FIFO<EATEntry> EATList;

struct DocWriter
{
    char* Name;
    ISymUnmanagedDocumentWriter* pWriter;
    DocWriter() { Name=NULL; pWriter=NULL; };
    ~DocWriter() { delete [] Name; };
};
typedef FIFO<DocWriter> DocWriterList;
/**************************************************************************/
/* The assembler object does all the code generation (dealing with meta-data)
   writing a PE file etc etc. But does NOT deal with syntax (that is what
   AsmParse is for).  Thus the API below is how AsmParse 'controls' the 
   Assember.  Note that the Assembler object does know about the 
   AsmParse object (that is Assember is more fundamental than AsmParse) */
struct Instr
{
    int opcode;
    unsigned linenum;
	unsigned column;
    unsigned linenum_end;
	unsigned column_end;
    unsigned pc;
    ISymUnmanagedDocumentWriter* pWriter;
};
#define INSTR_POOL_SIZE 16

struct Clockwork
{
    DWORD  cBegin;
    DWORD  cEnd;
    DWORD  cParsBegin;
    DWORD  cParsEnd;
    DWORD  cMDInitBegin;
    DWORD  cMDInitEnd;
    DWORD  cMDEmitBegin;
    DWORD  cMDEmitEnd;
    DWORD  cMDEmit1;
    DWORD  cMDEmit2;
    DWORD  cMDEmit3;
    DWORD  cMDEmit4;
    DWORD  cRef2DefBegin;
    DWORD  cRef2DefEnd;
    DWORD  cFilegenBegin;
    DWORD  cFilegenEnd;
};

class Assembler {
public:
    Assembler();
    ~Assembler();
    //--------------------------------------------------------  
	LabelList		m_lstLabel;
	GlobalLabelList m_lstGlobalLabel;
	GlobalFixupList m_lstGlobalFixup;
	ILFixupList		m_lstILFixup;
	FixupList		m_lstFixup;

    Class *			m_pModuleClass;
    ClassList		m_lstClass;

    BYTE *  m_pOutputBuffer;
    BYTE *  m_pCurOutputPos;
    BYTE *  m_pEndOutputPos;


    DWORD   m_CurPC;
    BOOL    m_fStdMapping;
    BOOL    m_fDisplayTraceOutput;
    BOOL    m_fInitialisedMetaData;
    BOOL    m_fAutoInheritFromObject;
    BOOL    m_fReportProgress;
    BOOL    m_fIsMscorlib;
    mdToken m_tkSysObject;
    mdToken m_tkSysString;
    mdToken m_tkSysValue;
    mdToken m_tkSysEnum;

    IMetaDataDispenser *m_pDisp;
    IMetaDataEmit      *m_pEmitter;
    ICeeFileGen        *m_pCeeFileGen;
    IMetaDataImport    *m_pImporter;			// Import interface.
    HCEEFILE m_pCeeFile;
    HCEESECTION m_pGlobalDataSection;
    HCEESECTION m_pILSection;
    HCEESECTION m_pTLSSection;
    HCEESECTION m_pCurSection;      // The section EmitData* things go to

    AsmMan*     m_pManifest;

    char    m_szScopeName[MAX_SCOPE_LENGTH];
    char    *m_szNamespace; //[MAX_NAMESPACE_LENGTH];
    char    *m_szFullNS; //[MAX_NAMESPACE_LENGTH];
	unsigned	m_ulFullNSLen;

    StringStack m_NSstack;
    mdTypeSpec      m_crExtends;

    //    char    m_szExtendsClause[MAX_CLASSNAME_LENGTH];

    mdToken   *m_crImplList;
    int     m_nImplList;
    int     m_nImplListSize;
    
    TyParList       *m_TyParList;
    
    Method *m_pCurMethod;
    Class   *m_pCurClass;
    ClassStack m_ClassStack; // for nested classes

    // moved to Class
    //MethodList  m_MethodList;

    BOOL    m_fCPlusPlus;
    BOOL    m_fWindowsCE;
    BOOL    m_fGenerateListing;
    BOOL    m_fDLL;
    BOOL    m_fOBJ;
    BOOL    m_fEntryPointPresent;
    BOOL    m_fHaveFieldsWithRvas;

    state_t m_State;

    Instr   m_Instr[INSTR_POOL_SIZE]; // 16
    inline  Instr* GetInstr() 
    {
        int i=0;
        for(; (i<INSTR_POOL_SIZE)&&(m_Instr[i].opcode != -1); i++);
        _ASSERTE(i<INSTR_POOL_SIZE);
        return &m_Instr[i];
    }
    //--------------------------------------------------------------------------------
    void    ClearImplList(void);
    void    AddToImplList(mdToken);
    void    ClearBoundList(void);
    //--------------------------------------------------------------------------------
    BOOL Init();
    void ProcessLabel(char *pszName);
    Label *FindLabel(char *pszName);
    Label *FindLabel(DWORD PC);
    GlobalLabel *FindGlobalLabel(char *pszName);
    void AddLabel(DWORD CurPC, char *pszName);
    void AddDeferredFixup(char *pszLabel, BYTE *pBytes, DWORD RelativeToPC, BYTE FixupSize);
    GlobalFixup *AddDeferredGlobalFixup(char *pszLabel, BYTE* reference);
    void AddDeferredDescrFixup(char *pszLabel);
    BOOL DoFixups();
    BOOL DoGlobalFixups();
    BOOL DoDescrFixups();
    BOOL GenerateListingFile(Method *pMethod);
    OPCODE DecodeOpcode(const BYTE *pCode, DWORD *pdwLen);
    BOOL AddMethod(Method *pMethod);
    void SetTLSSection()        
	{ m_pCurSection = m_pTLSSection; m_dwComImageFlags &= ~COMIMAGE_FLAGS_ILONLY; m_dwComImageFlags |= COMIMAGE_FLAGS_32BITREQUIRED;}
    void SetDataSection()       { m_pCurSection = m_pGlobalDataSection; }
    BOOL EmitMethod(Method *pMethod);
    BOOL EmitMethodBody(Method* pMethod);
    BOOL EmitClass(Class *pClass);
    HRESULT CreatePEFile(WCHAR *pwzOutputFilename);
    HRESULT CreateTLSDirectory();
    HRESULT CreateDebugDirectory();
    HRESULT InitMetaData();
    Class *FindCreateClass(char *pszFQN);
    BOOL EmitFieldRef(char *pszArg, int opcode);
    BOOL EmitSwitchData(char *pszArg);
    mdToken ResolveClassRef(mdToken tkResScope, char *pszClassName, Class** ppClass);
    mdToken ResolveTypeSpec(BinStr* typeSpec);
    mdToken GetAsmRef(char* szName);
    mdToken GetModRef(char* szName);
    char* ReflectionNotation(mdToken tk);
    HRESULT ConvLocalSig(char* localsSig, CQuickBytes* corSig, DWORD* corSigLen, BYTE*& localTypes);
    DWORD GetCurrentILSectionOffset();
    BOOL EmitCALLISig(char *p);
    void AddException(DWORD pcStart, DWORD pcEnd, DWORD pcHandler, DWORD pcHandlerTo, mdTypeRef crException, BOOL isFilter, BOOL isFault, BOOL isFinally);
    state_t CheckLocalTypeConsistancy(int instr, unsigned arg);
    state_t AddGlobalLabel(char *pszName, HCEESECTION section);
    void DoDeferredILFixups(Method* pMethod);
    void AddDeferredILFixup(ILFixupType Kind);
    void AddDeferredILFixup(ILFixupType Kind, GlobalFixup *GFixup);
    void SetDLL(BOOL);
    void SetOBJ(BOOL);
    void ResetForNextMethod();
    void SetStdMapping(BOOL val = TRUE) { m_fStdMapping = val; };

    //--------------------------------------------------------------------------------
    BOOL isShort(unsigned instr) { return ((OpcodeInformation[instr].Type & 16) != 0); };
    void SetErrorReporter(ErrorReporter* aReport) { report = aReport; if(m_pManifest) m_pManifest->SetErrorReporter(aReport); }

    void StartNameSpace(char* name);
    void EndNameSpace();
    void StartClass(char* name, DWORD attr, TyParList *typars);
    void EndClass();
    void StartMethod(char* name, BinStr* sig, CorMethodAttr flags, BinStr* retMarshal, DWORD retAttr, TyParList *typars = NULL);
    void EndMethod();

    void AddField(char* name, BinStr* sig, CorFieldAttr flags, char* rvaLabel, BinStr* pVal, ULONG ulOffset);
	BOOL EmitField(FieldDescriptor* pFD);
    void EmitByte(int val);
    //void EmitTry(enum CorExceptionFlag kind, char* beginLabel, char* endLabel, char* handleLabel, char* filterOrClass);
    void EmitMaxStack(unsigned val);
    void EmitLocals(BinStr* sig);
    void EmitEntryPoint();
    void EmitZeroInit();
    void SetImplAttr(unsigned short attrval);
    void EmitData(void* buffer, unsigned len);
    void EmitDD(char *str);
    void EmitDataString(BinStr* str);

    void EmitInstrVar(Instr* instr, int var);
    void EmitInstrVarByName(Instr* instr, char* label);
    void EmitInstrI(Instr* instr, int val);
    void EmitInstrI8(Instr* instr, __int64* val);
    void EmitInstrR(Instr* instr, double* val);
    void EmitInstrBrOffset(Instr* instr, int offset);
    void EmitInstrBrTarget(Instr* instr, char* label);
    mdToken MakeMemberRef(mdToken typeSpec, char* name, BinStr* sig, unsigned opcode_len);
    mdToken MakeMethodSpec(mdToken tkParent, BinStr* sig, unsigned opcode_len);
    mdToken MakeTypeRef(mdToken tkResScope, char* szFummName);
    void EmitInstrStringLiteral(Instr* instr, BinStr* literal, BOOL ConvertToUnicode);
    void EmitInstrSig(Instr* instr, BinStr* sig);
    void EmitInstrRVA(Instr* instr, char* label, bool islabel);
    void EmitInstrSwitch(Instr* instr, Labels* targets);
    void EmitInstrPhi(Instr* instr, BinStr* vars);
    void EmitLabel(char* label);
    void EmitDataLabel(char* label);

    unsigned OpcodeLen(Instr* instr); //returns opcode length
    // Emit just the opcode (no parameters to the instruction stream.
    void EmitOpcode(Instr* instr);

    // Emit primitive types to the instruction stream.
    void EmitBytes(BYTE*, unsigned len);

    ErrorReporter* report;

	BOOL EmitFieldsMethods(Class* pClass);
	BOOL EmitEventsProps(Class* pClass);

    // named args/vars paraphernalia:
public:
    void addArgName(char *szNewName, BinStr* pbSig, BinStr* pbMarsh, DWORD dwAttr)
    {
        if(pbSig && (*(pbSig->ptr()) == ELEMENT_TYPE_VOID))
            report->error("Illegal use of type 'void'\n");
        if(m_lastArgName)
        {
            m_lastArgName->pNext = new ARG_NAME_LIST(m_lastArgName->nNum+1,szNewName,pbSig,pbMarsh,dwAttr);
            m_lastArgName = m_lastArgName->pNext;
        }
        else
        {
            m_lastArgName = new ARG_NAME_LIST(0,szNewName,pbSig,pbMarsh,dwAttr);
            m_firstArgName = m_lastArgName;
        }
    };
    ARG_NAME_LIST *getArgNameList(void)
    { ARG_NAME_LIST *pRet = m_firstArgName; m_firstArgName=NULL; m_lastArgName=NULL; return pRet;};
    // Added because recursive destructor of ARG_NAME_LIST may overflow the system stack
    void delArgNameList(ARG_NAME_LIST *pFirst)
    {
        ARG_NAME_LIST *pArgList=pFirst, *pArgListNext;
        for(; pArgList; pArgListNext=pArgList->pNext,
                        delete pArgList, 
                        pArgList=pArgListNext);
    };

    ARG_NAME_LIST   *findArg(ARG_NAME_LIST *pFirst, int num)
    {
        ARG_NAME_LIST *pAN;
        for(pAN=pFirst; pAN; pAN = pAN->pNext)
        {
            if(pAN->nNum == num) return pAN;
        }
        return NULL;
    };
    ARG_NAME_LIST *m_firstArgName;
    ARG_NAME_LIST *m_lastArgName;

    // Structured exception handling paraphernalia:
public:
    SEH_Descriptor  *m_SEHD;    // current descriptor ptr
    void NewSEHDescriptor(void); //sets m_SEHD
    void SetTryLabels(char * szFrom, char *szTo);
    void SetFilterLabel(char *szFilter);
    void SetCatchClass(mdToken catchClass);
    void SetHandlerLabels(char *szHandlerFrom, char *szHandlerTo);
    void EmitTry(void);         //uses m_SEHD

//private:
    SEHD_Stack  m_SEHDstack;

    // Events and Properties paraphernalia:
public:
    void EndEvent(void);    //emits event definition
    void EndProp(void);     //emits property definition
    void ResetEvent(char * szName, mdToken typeSpec, DWORD dwAttr);
    void ResetProp(char * szName, BinStr* bsType, DWORD dwAttr, BinStr* bsValue);
    void SetEventMethod(int MethodCode, mdToken typeSpec, char* pszMethodName, BinStr* sig);
    void SetPropMethod(int MethodCode, mdToken typeSpec, char* pszMethodName, BinStr* sig);
    BOOL EmitEvent(EventDescriptor* pED);   // impl. in ASSEM.CPP
    BOOL EmitProp(PropDescriptor* pPD); // impl. in ASSEM.CPP
    EventDescriptor*    m_pCurEvent;
    PropDescriptor*     m_pCurProp;

private:
    MemberRefDList           m_LocalMethodRefDList;
    MemberRefDList           m_LocalFieldRefDList;
    LocalMemberRefFixupList  m_LocalMemberRefFixupList;
    MemberRefDList           m_MethodSpecList;

public:
    HRESULT ResolveLocalMemberRefs();
    HRESULT DoLocalMemberRefFixups();
    mdToken ResolveLocalMemberRef(mdToken tok);

    // PInvoke paraphernalia
public:
    PInvokeDescriptor*  m_pPInvoke;
    ImportList  m_ImportList;
    void SetPinvoke(BinStr* DllName, int Ordinal, BinStr* Alias, int Attrs);
    HRESULT EmitPinvokeMap(mdToken tk, PInvokeDescriptor* pDescr);
    ImportDescriptor* EmitImport(BinStr* DllName);

    // Debug metadata paraphernalia
public:
    ISymUnmanagedWriter* m_pSymWriter;
    ISymUnmanagedDocumentWriter* m_pSymDocument;
    DocWriterList m_DocWriterList;
    ULONG m_ulCurLine; // set by Parser
    ULONG m_ulCurColumn; // set by Parser
    ULONG m_ulLastDebugLine;
    ULONG m_ulLastDebugColumn;
    BOOL  m_fIncludeDebugInfo;
    char m_szSourceFileName[MAX_FILENAME_LENGTH*3];
    WCHAR m_wzOutputFileName[MAX_FILENAME_LENGTH];
	GUID	m_guidLang;
	GUID	m_guidLangVendor;
	GUID	m_guidDoc;

    // Security paraphernalia
public:
    void AddPermissionDecl(CorDeclSecurity action, mdToken type, NVPair *pairs)
    {
        PermissionDecl *decl = new PermissionDecl(action, type, pairs);
		if(decl==NULL)
		{
			report->error("\nOut of memory!\n");
			return;
		}
        if(m_pCurClass && IsTdInterface(m_pCurClass->m_Attr))
            report->error("Security permissions in interface\n");
        if (m_pCurMethod) {
            decl->m_Next = m_pCurMethod->m_pPermissions;
            m_pCurMethod->m_pPermissions = decl;
        } else if (m_pCurClass) {
            decl->m_Next = m_pCurClass->m_pPermissions;
            m_pCurClass->m_pPermissions = decl;
        } else if (m_pManifest && m_pManifest->m_pAssembly) {
            decl->m_Next = m_pManifest->m_pAssembly->m_pPermissions;
            m_pManifest->m_pAssembly->m_pPermissions = decl;
        } else {
            report->error("Cannot declare security permissions without the owner\n");
            delete decl;
        }
    };

    void AddPermissionSetDecl(CorDeclSecurity action, BinStr *value)
    {
        PermissionSetDecl *decl = new PermissionSetDecl(action, value);
		if(decl==NULL)
		{
			report->error("\nOut of memory!\n");
			return;
		}
        if(m_pCurClass && IsTdInterface(m_pCurClass->m_Attr))
            report->error("Security permission set in interface\n");
        if (m_pCurMethod) {
            decl->m_Next = m_pCurMethod->m_pPermissionSets;
            m_pCurMethod->m_pPermissionSets = decl;
        } else if (m_pCurClass) {
            decl->m_Next = m_pCurClass->m_pPermissionSets;
            m_pCurClass->m_pPermissionSets = decl;
        } else if (m_pManifest && m_pManifest->m_pAssembly) {
            decl->m_Next = m_pManifest->m_pAssembly->m_pPermissionSets;
            m_pManifest->m_pAssembly->m_pPermissionSets = decl;
        } else {
            report->error("Cannot declare security permission sets without the owner\n");
            delete decl;
        }
    };
    void EmitSecurityInfo(mdToken           token,
                          PermissionDecl*   pPermissions,
                          PermissionSetDecl*pPermissionSets);
    
    HRESULT AllocateStrongNameSignature();
    HRESULT StrongNameSign();

    // Custom values paraphernalia:
public:
    mdToken m_tkCurrentCVOwner;
    CustomDescrList* m_pCustomDescrList;
    CustomDescrList  m_CustomDescrList;
    void DefineCV(CustomDescr* pCD)
    {
        if(pCD)
        {
            ULONG               cTemp = 0;
            void *          pBlobBody = NULL;
            mdToken         cv;
            mdToken tkOwnerType, tkTypeType = TypeFromToken(pCD->tkType);
            if((tkTypeType != 0x99000000)&&(tkTypeType != 0x98000000))
            {
                tkOwnerType = TypeFromToken(pCD->tkOwner);
                if((tkOwnerType != 0x99000000)&&(tkOwnerType != 0x98000000))
                {
                    if(pCD->pBlob)
                    {
                        pBlobBody = (void *)(pCD->pBlob->ptr());
                        cTemp = pCD->pBlob->length();
                    }
                    m_pEmitter->DefineCustomAttribute(pCD->tkOwner,pCD->tkType,pBlobBody,cTemp,&cv);
                    delete pCD;
                    return;
                }
            }
            m_CustomDescrList.PUSH(pCD);
        }
    };
    void EmitCustomAttributes(mdToken tok, CustomDescrList* pCDL)
    {
        CustomDescr *pCD;
        if(pCDL == NULL || RidFromToken(tok)==0) return;
        while((pCD = pCDL->POP()))
        {
            pCD->tkOwner = tok;
            DefineCV(pCD);
        }
    };

    void EmitUnresolvedCustomAttributes(); // implementation: writer.cpp
    // VTable blob (if any)
public:
    BinStr *m_pVTable;
    // Field marshaling
    BinStr *m_pMarshal;
    // VTable fixup list
    VTFList m_VTFList;
	// Export Address Table entries list
	EATList m_EATList;
	HRESULT CreateExportDirectory();
	DWORD	EmitExportStub(DWORD dwVTFSlotRVA);

    // Method implementation paraphernalia:
private:
    MethodImplDList m_MethodImplDList;
public:
    void AddMethodImpl(mdToken tkImplementedTypeSpec, char* szImplementedName, BinStr* pImplementedSig, 
                    mdToken tkImplementingTypeSpec, char* szImplementingName, BinStr* pImplementingSig);
    BOOL EmitMethodImpls();
    // lexical scope handling paraphernalia:
    void EmitScope(Scope* pSCroot); // struct Scope - see Method.hpp
    // obfuscating paraphernalia:
    BOOL    m_fOwnershipSet;
    BinStr* m_pbsOwner;
    // source file name paraphernalia
    BOOL m_fSourceFileSet;
    void SetSourceFileName(char* szName);
    void SetSourceFileName(BinStr* pbsName);
    // header flags
    DWORD   m_dwSubsystem;
    DWORD   m_dwComImageFlags;
	DWORD	m_dwFileAlignment;
	size_t	m_stBaseAddress;
    // Former globals
    WCHAR *m_wzResourceFile;
    WCHAR *m_wzKeySourceName;
    bool OnErrGo;
    void SetCodePage(unsigned val) { g_uCodePage = val; };
    Clockwork* bClock;
    void SetClock(Clockwork* val) { bClock = val; };
};

#endif  // Assember_h
