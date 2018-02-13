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

#include "common.h"
#include "sigformat.h"

SigFormat::SigFormat() : _classInst(NULL)
{
        _size = SIG_INC;
        _pos = 0;
        _fmtSig = new char[_size];
}

SigFormat::SigFormat(MetaSig &metaSig, LPCUTF8 szMemberName, LPCUTF8 szClassName, LPCUTF8 szNameSpace) : _classInst(NULL)
{
    FormatSig(metaSig, szMemberName, szClassName, szNameSpace);
}
    

// SigFormat::SigFormat()
// This constructor will create the string representation of a 
//  method.
SigFormat::SigFormat(MethodDesc* pMeth, TypeHandle* classInst, BOOL fIgnoreMethodName) : _classInst(classInst)
{
    MetaSig sig(pMeth, classInst);
    if (fIgnoreMethodName)
    {
        FormatSig(sig, NULL);
    }
    else
    {
        FormatSig(sig, pMeth->GetName());
    }
}

    
SigFormat::~SigFormat()
{
    if (_fmtSig)
        delete [] _fmtSig;
}

STRINGREF SigFormat::GetString()
{
    STRINGREF p = NULL;
    COMPLUS_TRY {
        p = COMString::NewString(_fmtSig);
    } COMPLUS_CATCH {
    } COMPLUS_END_CATCH
    return p;
}

const char * SigFormat::GetCString()
{
    return _fmtSig;
}

const char * SigFormat::GetCStringParmsOnly()
{
     // _fmtSig looks like: "void Put (byte[], int, int)".
     // Skip to the '('.
     int skip;
     for(skip=0; _fmtSig[skip]!='('; skip++)
            ;  
     return _fmtSig + skip;
}


int SigFormat::AddSpace()
{
    if (_pos == _size) {
        char* temp = new char[_size+SIG_INC];
        if (!temp)
            return 0;
        memcpy(temp,_fmtSig,_size);
        delete [] _fmtSig;
        _fmtSig = temp;
        _size+=SIG_INC;
    }
    _fmtSig[_pos] = ' ';
    _fmtSig[++_pos] = 0;
    return 1;
}

int SigFormat::AddString(LPCUTF8 s)
{
    int len = (int)strlen(s);
    // Allocate on overflow
    if (_pos + len >= _size) {
        int newSize = (_size+SIG_INC > _pos + len) ? _size+SIG_INC : _pos + len + SIG_INC; 
        char* temp = new char[newSize];
        if (!temp)
            return 0;
        memcpy(temp,_fmtSig,_size);
        delete [] _fmtSig;
        _fmtSig = temp;
        _size=newSize;
    }
    strcpy(&_fmtSig[_pos],s);
    _pos += len;
    return 1;
}


//------------------------------------------------------------------------
// Replacement for SigFormat::AddType that avoids class loading
// and copes with formal type parameters
//------------------------------------------------------------------------
void SigFormat::AddTypeString(Module* pModule, SigPointer sig, TypeHandle *classInst, TypeHandle *methodInst)
{
    LPCUTF8     szcName;
    LPCUTF8     szcNameSpace;
	
    CorElementType type = sig.GetElemType();

    // Format the output
    switch (type) 
    {
    case ELEMENT_TYPE_VOID:     AddString("Void"); break;
    case ELEMENT_TYPE_BOOLEAN:  AddString("Boolean"); break;
    case ELEMENT_TYPE_I1:       AddString("SByte"); break;
    case ELEMENT_TYPE_U1:       AddString("Byte"); break;
    case ELEMENT_TYPE_I2:       AddString("Int16"); break;
    case ELEMENT_TYPE_U2:       AddString("UInt16"); break;
    case ELEMENT_TYPE_CHAR:     AddString("Char"); break;
    case ELEMENT_TYPE_I:        AddString("IntPtr"); break;
    case ELEMENT_TYPE_U:        AddString("UIntPtr"); break;
    case ELEMENT_TYPE_I4:       AddString("Int32"); break;
    case ELEMENT_TYPE_U4:       AddString("UInt32"); break;
    case ELEMENT_TYPE_I8:       AddString("Int64"); break;
    case ELEMENT_TYPE_U8:       AddString("UInt64"); break;
    case ELEMENT_TYPE_R4:       AddString("Single"); break;
    case ELEMENT_TYPE_R8:       AddString("Double"); break;
    case ELEMENT_TYPE_OBJECT:   AddString(g_ObjectClassName); break;
    case ELEMENT_TYPE_STRING:   AddString(g_StringClassName); break;

    // For Value Classes we fall through unless the pVMC is an Array Class, 
    // If its an array class we need to get the name of the underlying type from 
    // it.
    case ELEMENT_TYPE_VALUETYPE:
    case ELEMENT_TYPE_CLASS:
        {
	  IMDInternalImport *pInternalImport = pModule->GetMDImport();
          mdToken token = sig.GetToken();

	  if (TypeFromToken(token) == mdtTypeDef)
	    pInternalImport->GetNameOfTypeDef(token, &szcName, &szcNameSpace);
	  else if (TypeFromToken(token) == mdtTypeRef)
	    pInternalImport->GetNameOfTypeRef(token, &szcName, &szcNameSpace);
          else break;

            if (*szcNameSpace)
            {
                AddString(szcNameSpace);
                AddString(".");
            }
            AddString(szcName);
            break;
        }
    case ELEMENT_TYPE_TYPEDBYREF:
        {
            AddString("TypedReference");
            break;
        }

    case ELEMENT_TYPE_BYREF:
        {
            AddTypeString(pModule, sig, classInst, methodInst);          
            AddString(" ByRef");
        }
        break;

    case ELEMENT_TYPE_MVAR :
      {
        DWORD ix = sig.GetData();
        if (methodInst != NULL)
	{
           AddType(methodInst[ix]);
        }
        else
	{
          char smallbuf[20];
          sprintf(smallbuf, "!!%d", ix);
          AddString(smallbuf);
        }    
      }
      break;

    case ELEMENT_TYPE_VAR :
      {
        DWORD ix = sig.GetData();
        if (classInst != NULL)
	{
           AddType(classInst[ix]);
        }
        else
        {
          AddString("!");
          char smallbuf[20];
          sprintf(smallbuf, "!%d", ix);
          AddString(smallbuf);
        }
      }
      break;

    case ELEMENT_TYPE_WITH :
      {
        AddTypeString(pModule, sig, classInst, methodInst);
        sig.SkipExactlyOne();
        DWORD n = sig.GetData();
        AddString("<");
        for (DWORD i = 0; i < n; i++)
	{
          if (i > 0)
            AddString(",");
          AddTypeString(pModule,sig, classInst, methodInst);
          sig.SkipExactlyOne();
        }
        AddString(">");
        break;
      }

    case ELEMENT_TYPE_SZARRAY:      // Single Dim, Zero
    case ELEMENT_TYPE_ARRAY:        // General Array
        {
            AddTypeString(pModule, sig, classInst, methodInst);
            sig.SkipExactlyOne();
            if (type == ELEMENT_TYPE_ARRAY) {
                AddString("[");         
                int len = sig.GetData();
                for (int i=0;i<len-1;i++)
                    AddString(",");
                AddString("]");
            }
            else {
                AddString("[]");
            }
        } 
        break;

    case ELEMENT_TYPE_PTR:
        {
            // This will pop up on methods that take a pointer to a block of unmanaged memory.
            AddTypeString(pModule, sig, classInst, methodInst);
            AddString("*");
            break;
        }
    default:
        AddString("**UNKNOWN TYPE**");

    }
}

void SigFormat::FormatSig(MetaSig &sig, LPCUTF8 szMemberName, LPCUTF8 szClassName, LPCUTF8 szNameSpace)
{
    UINT            cArgs;

    _size = SIG_INC;
    _pos = 0;
    _fmtSig = new char[_size];

    TypeHandle *methodInst = sig.GetMethodInst();
    AddTypeString(sig.GetModule(), sig.GetReturnProps(), _classInst, methodInst);

    AddSpace();
    if (szNameSpace != NULL)
    {
        AddString(szNameSpace);
        AddString(".");
    }
    if (szClassName != NULL) 
    {
        AddString(szClassName);
        AddString(".");
    }
    if (szMemberName != NULL)
    {
        AddString(szMemberName);
    }

    cArgs = sig.NumFixedArgs();
    sig.Reset();
    // If the first parameter is the magic value return type
    //  suck it up.

    AddString("(");

    // Loop through all of the args
    for (UINT i=0;i<cArgs;i++) {
        sig.NextArg();
       AddTypeString(sig.GetModule(), sig.GetArgProps(), _classInst, methodInst);
       if (i != cArgs-1)
           AddString(", ");
    }

    // Display vararg signature at end
    if (sig.IsVarArg())
    {
        if (cArgs)
            AddString(", ");
        AddString("...");
    }

    AddString(")");
}

int SigFormat::AddType(TypeHandle th)
{
    LPCUTF8     szcName;
    LPCUTF8     szcNameSpace;
    ExpandSig  *pSig;
    ULONG       cArgs;
    VOID       *pEnum;
    ULONG       i;

    if (th.IsNull()) {
        AddString("**UNKNOWN TYPE**");
        return(1);
    }
  
    CorElementType type = th.GetSigCorElementType();

	if ((type == ELEMENT_TYPE_I) && (!(th.AsMethodTable()->GetClass()->IsTruePrimitive())))
		type = ELEMENT_TYPE_VALUETYPE;
	
    // Format the output
    switch (type) 
    {
    case ELEMENT_TYPE_VOID:     AddString("Void"); break;
    case ELEMENT_TYPE_BOOLEAN:  AddString("Boolean"); break;
    case ELEMENT_TYPE_I1:       AddString("SByte"); break;
    case ELEMENT_TYPE_U1:       AddString("Byte"); break;
    case ELEMENT_TYPE_I2:       AddString("Int16"); break;
    case ELEMENT_TYPE_U2:       AddString("UInt16"); break;
    case ELEMENT_TYPE_CHAR:     AddString("Char"); break;
    case ELEMENT_TYPE_I:        AddString("IntPtr"); break;
    case ELEMENT_TYPE_U:        AddString("UIntPtr"); break;
    case ELEMENT_TYPE_I4:       AddString("Int32"); break;
    case ELEMENT_TYPE_U4:       AddString("UInt32"); break;
    case ELEMENT_TYPE_I8:       AddString("Int64"); break;
    case ELEMENT_TYPE_U8:       AddString("UInt64"); break;
    case ELEMENT_TYPE_R4:       AddString("Single"); break;
    case ELEMENT_TYPE_R8:       AddString("Double"); break;
    case ELEMENT_TYPE_OBJECT:   AddString(g_ObjectClassName); break;
    case ELEMENT_TYPE_STRING:   AddString(g_StringClassName); break;

    // For Value Classes we fall through unless the pVMC is an Array Class, 
    // If its an array class we need to get the name of the underlying type from 
    // it.
    case ELEMENT_TYPE_VALUETYPE:
    case ELEMENT_TYPE_CLASS:
        {
            EEClass* pEEC = th.AsClass();
            pEEC->GetMDImport()->GetNameOfTypeDef(pEEC->GetCl(), &szcName, &szcNameSpace);

            if (*szcNameSpace)
            {
                AddString(szcNameSpace);
                AddString(".");
            }
            AddString(szcName);
            if (th.HasInstantiation())
	    {
              TypeHandle *inst = th.GetInstantiation();
              AddString("<");
              for (DWORD i = 0; i < th.GetNumGenericArgs(); i++)
	      {
                if (i > 0) AddString(",");
                AddType(inst[i]);
              }
              AddString(">");
            }
            break;
        }
    case ELEMENT_TYPE_TYPEDBYREF:
        {
            AddString("TypedReference");
            break;
        }

    case ELEMENT_TYPE_BYREF:
        {
            TypeHandle h = th.AsTypeDesc()->GetTypeParam();
            AddType(h);
            AddString(" ByRef");
        }
        break;

    case ELEMENT_TYPE_SZARRAY:      // Single Dim, Zero
    case ELEMENT_TYPE_ARRAY:        // General Array
        {
            ArrayTypeDesc* aTD = th.AsArray();
            AddType(aTD->GetElementTypeHandle());
            if (type == ELEMENT_TYPE_ARRAY) {
                AddString("[");
                int len = aTD->GetRank();
                for (int i=0;i<len-1;i++)
                    AddString(",");
                AddString("]");
            }
            else {
                AddString("[]");
            }
        }
        break;

    case ELEMENT_TYPE_PTR:
        {
            // This will pop up on methods that take a pointer to a block of unmanaged memory.
            TypeHandle h = th.AsTypeDesc()->GetTypeParam();
            AddType(h);
            AddString("*");
            break;
        }
    case ELEMENT_TYPE_FNPTR:
        pSig = ((FunctionTypeDesc*)th.AsTypeDesc())->GetSig();
        AddType(pSig->GetReturnTypeHandle());
        AddSpace();
        AddString("(");
        cArgs = pSig->NumFixedArgs();
        pSig->Reset(&pEnum);
        for (i = 0; i < cArgs; i++) {
            AddType(pSig->NextArgExpanded(&pEnum));
            if (i != (cArgs - 1))
                AddString(", ");
        }
        if (pSig->IsVarArg()) {
            if (cArgs)
                AddString(", ");
            AddString("...");
        }
        AddString(")");
        break;

    default:
        AddString("**UNKNOWN TYPE**");

    }
    return 1;
}


FieldSigFormat::FieldSigFormat(FieldDesc* pFld, TypeHandle *classInst)
{
    PCCOR_SIGNATURE pSig;
    DWORD           cSig;

    pFld->GetSig(&pSig,&cSig);

    _size = SIG_INC;
    _pos = 0;

    SigPointer sig(pSig);
    sig.GetCallingConvInfo();

    AddTypeString(pFld->GetModule(), sig, classInst, NULL);
    AddSpace();
    AddString(pFld->GetName());
}


PropertySigFormat::PropertySigFormat(MetaSig &metaSig, LPCUTF8 memberName)
{
    FormatSig(metaSig, memberName);
}


void PropertySigFormat::FormatSig(MetaSig &sig, LPCUTF8 memberName)
{
    THROWSCOMPLUSEXCEPTION();

    UINT            cArgs;
    TypeHandle      th;

    _size = SIG_INC;
    _pos = 0;

    // _fmtSig is already allocated in the base class SigFormat's constructor.
    _ASSERTE(_fmtSig);

    OBJECTREF throwable = NULL;
    GCPROTECT_BEGIN(throwable);
    th = sig.GetRetTypeHandle(&throwable);
    if (throwable != NULL)
        COMPlusThrow(throwable);

    AddType(th);
    AddSpace();
    if (memberName != NULL)
    {
        AddString(memberName);
    }

    cArgs = sig.NumFixedArgs();
    sig.Reset();
    // If the first parameter is the magic value return type
    //  suck it up.

    if (cArgs || sig.IsVarArg()) // For indexed properties and varargs
    {
        AddSpace();
        AddString("[");
            
        // Loop through all of the args
        for (UINT i=0;i<cArgs;i++) {
            sig.NextArg();
            th = sig.GetTypeHandle();

           AddType(th);
           if (i != cArgs-1)
               AddString(", ");
        }

        // Display vararg signature at end
        if (sig.IsVarArg())
        {
            if (cArgs)
                AddString(", ");
            AddString("...");
        }

        AddString("]");
    }

    GCPROTECT_END();
}
