/**************************************************************************/
/* a type parameter list */

#ifndef TYPAR_H
#define TYPAR_H

extern unsigned int g_uCodePage;

class TyParList {
public:
	TyParList(mdToken b, char* n, TyParList* nx = NULL) 
    { 
		bound = b; name = n; next = nx;
	};
	~TyParList() 
    { 
		if (next) delete next; 
	};
    int Count()
    {
        TyParList* tp = this;
        int n;
        for(n = 1; (tp = tp->next); n++);
        return n;
    };
    int ToArray(mdToken **bounds, LPCWSTR** names)
    {   
        int n = Count();
        mdToken *b = new mdToken [n];
        LPCWSTR *nam = new LPCWSTR [n];
        TyParList *tp = this;
        int i = 0;
        while (tp)
        {
            ULONG               cTemp = (ULONG)strlen(tp->name)+1;
            WCHAR*              wzDllName = new WCHAR [cTemp];
            // Convert name to UNICODE
            memset(wzDllName,0,sizeof(WCHAR)*cTemp);
            WszMultiByteToWideChar(g_uCodePage,0,tp->name,-1,wzDllName,cTemp);
            nam[i] = (LPCWSTR)wzDllName;
            b[i] = tp->bound;
            i++;
            tp = tp->next;
        }
        *bounds = b;
        *names = nam;          
        return n;
    };
      
private:
	mdToken bound;
    char*   name;
    TyParList* next;
};

typedef TyParList* pTyParList;

#endif

