#ifndef _GENERICS_H
#define _GENERICS_H

class CrawlFrame;

class DictionaryLayout
{
public:
  DictionaryLayout* next;
  DWORD numSlots;
  BOOL hasSpillPointer; // does this dictionary bucket have a spill pointer?
  mdToken slots[1];  // numSlots of these
};

// Generics helper functions
class Generics
{
public:
  static void PrettyInstantiation(char *buf, DWORD max, DWORD numTyPars, TypeHandle *inst);

  static TypeHandle GetMethodDeclaringType(TypeHandle owner, MethodDesc *pMD, OBJECTREF *pThrowable);
  static TypeHandle GetFrameOwner(CrawlFrame *pCf);
  static WORD FindClassDictionaryToken(EEClass *pClass, unsigned token, WORD *offsets);
};

#endif
