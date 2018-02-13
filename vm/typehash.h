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
// File: typehash.h
//
#ifndef _TYPE_HASH_H
#define _TYPE_HASH_H

//========================================================================================
// This hash table is used by class loaders to look up constructed types:
// arrays, pointers and instantiations of user-defined generic types.
//
// @GENERICS:
// We use the same hash table to do lookups based on instantiation-compatibility
// This means that
//     (a) The hash function must respect compatibility i.e. compatible instantiations
//         must hash to the same value
//     (b) We'll get more collisions and hence potentially slower lookup when doing
//         doing lookups based on exact instantiation. In future we might consider
//         replacing by two hash tables, one for exact lookups and one for
//         instantiation-compatible lookups. The downside is the additional space required.
//========================================================================================

class ClassLoader;
class NameHandle;

// The "blob" you get to store in the hash table

typedef void* HashDatum;


// One of these is present for each element in the table
typedef struct EETypeHashEntry
{
    struct EETypeHashEntry *pNext;
    DWORD               dwHashValue;
    HashDatum           Data;
    
    // For details of the representations used here, see NameHandle in clsload.hpp
    INT_PTR m_Key1;
    INT_PTR m_Key2;
} EETypeHashEntry_t;


// Type hashtable.
class EETypeHashTable 
{
    friend class ClassLoader;

protected:
    EETypeHashEntry_t **m_pBuckets;    // Pointer to first entry for each bucket
    DWORD           m_dwNumBuckets;
    DWORD           m_dwNumEntries;

public:
    LoaderHeap *    m_pHeap;

#ifdef _DEBUG
    DWORD           m_dwDebugMemory;
#endif

public:
    EETypeHashTable();
    ~EETypeHashTable();
    void *             operator new(size_t size, LoaderHeap *pHeap, DWORD dwNumBuckets);
    void               operator delete(void *p);

    // Insert a value in the hash table, keyed on exact type
    EETypeHashEntry_t * InsertValue(NameHandle* pName, HashDatum Data);

    // Look up a value in the hash table, keyed on exact type
    EETypeHashEntry_t *GetValue(NameHandle* pName, HashDatum *pData);
   
    EETypeHashEntry_t *AllocNewEntry();

private:
    EETypeHashEntry_t * FindItem(NameHandle* pName);
    void            GrowHashTable();

    // Calculate hash value; instantiated types whose instantiations are compatible MUST hash
    // to the same value.
    static DWORD Hash(NameHandle* pName);
};



#endif /* _TYPE_HASH_H */
