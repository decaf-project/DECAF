/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

DECAF is based on QEMU, a whole-system emulator. You can redistribute
and modify it under the terms of the GNU LGPL, version 2.1 or later,
but it is made available WITHOUT ANY WARRANTY. See the top-level
README file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/
/*
 * HashtableWrapper.cpp
 *
 *  Created on: Dec 21, 2011
 *      Author: lok
 */

#include <tr1/unordered_set>

#include "HashtableWrapper.h"
#include "Output.h"
#include "DECAF_types.h"

using namespace std::tr1;

typedef unordered_set<uint32_t> uset;

Hashtable* Hashtable_new()
{
  uset* pTable = new uset();
  return ( (Hashtable*)pTable );
}

void Hashtable_free(Hashtable* pTable)
{
  if (pTable != NULL)
  {
    delete( (uset*)pTable );
  }
}

int Hashtable_add(Hashtable* pHash, uint32_t item)
{
  uset* pTable = (uset*) pHash;
  if (pTable == NULL)
  {
    return (NULL_POINTER_ERROR);
  }

  std::pair<uset::iterator, bool> ret = pTable->insert(item);
  if (ret.second)
  {
    #ifdef DECAF_DEBUG_VERBOSE
    DECAF_mprintf("Adding [%x]\n", item);
    #endif
    return (1);
  }
  return (0);
}

int Hashtable_remove(Hashtable* pHash, uint32_t item)
{
  uset* pTable = (uset*) pHash;
  if (pTable == NULL)
  {
    return (NULL_POINTER_ERROR);
  }
  #ifdef DECAF_DEBUG_VERBOSE
  DECAF_mprintf("Removing [%x]\n", item);
  #endif
  pTable->erase(item);
  return (1);
}

int Hashtable_exist(Hashtable* pHash, uint32_t item)
{ uset* pTable = (uset*) pHash;
  if (pTable == NULL)
  {
    return (0);
  }
  return ( (pTable->find(item)) != pTable->end() );
}

void Hashtable_print(FILE* fp, Hashtable* pHash)
{
  uset* pTable = (uset*)pHash;
  if (pTable == NULL)
  {
    return;
  }

  for (uset::const_iterator it = pTable->begin(); it != pTable->end(); it++)
  {
    DECAF_fprintf(fp, "    %x\n", *it);
  }
}

#include <tr1/unordered_map>

typedef unordered_map<uint32_t, uint32_t> cset;

CountingHashtable* CountingHashtable_new()
{
  return ( (CountingHashtable*)(new cset()));
}

void CountingHashtable_free(CountingHashtable* pTable)
{
  if (pTable != NULL)
  {
    delete ( (cset*)pTable );
  }
}

uint32_t CountingHashtable_add(CountingHashtable* pTable, uint32_t key)
{
  if (pTable == NULL)
  {
    return (0);
  }

  cset* pTemp = (cset*)pTable;

  //here I assume that accessing size is quicker than searching to
  // determine if the key exists - NOT THREAD SAFE
  size_t prevSize = pTemp->size();

  //get the reference to the value
  uint32_t& val = (*pTemp)[key];
  //increment it
  val++;
  //if we just increased the size (this means that this is a new key)
  // then reset the value to 1 - I do this because int is not
  // initialized to 0 by default
  if (pTemp->size() > prevSize)
  {
    val = 1;
  }

  return (val);
}

uint32_t CountingHashtable_remove(CountingHashtable* pTable, uint32_t key)
{
  //just going to use the [] operator, which happens to create a new hashtable
  // if its not there already - might change this later
  //Very similar to the add case, except we reset the uint32_t to 0
  cset* pTemp = (cset*)pTable;
  if (pTemp == NULL)
  {
    return (0);
  }

  size_t prevSize = pTemp->size();
  uint32_t& val = (*pTemp)[key];
  val--;
  if (pTemp->size() > prevSize)
  {
    val = 0;
  }

  return (val);
}


int CountingHashtable_exist(CountingHashtable* pTable, uint32_t key)
{
  if (pTable == NULL)
  {
    return (0);
  }

  cset* pTemp = (cset*)pTable;
  cset::const_iterator it = pTemp->find(key);
  if (it == pTemp->end())
  {
    return (0);
  }

  return (it->second > 0);
}

uint32_t CountingHashtable_count(CountingHashtable* pTable, uint32_t key)
{
  if (pTable == NULL)
  {
    return (0);
  }

  cset* pTemp = (cset*)pTable;
  cset::const_iterator it = pTemp->find(key);
  if (it == pTemp->end())
  {
    return (0);
  }

  return (it->second);
}

void CountingHashtable_print(FILE* fp, CountingHashtable* pTable)
{
  cset* pTemp = (cset*)pTable;

  if (pTemp == NULL)
  {
    return;
  }

  for (cset::const_iterator it = pTemp->begin(); it != pTemp->end(); it++)
  {
    if (it->second > 0)
    {
      DECAF_fprintf(fp, "  %x [%u] ->\n", it->first, it->second);
    }
  }
}

typedef unordered_map<uint32_t, uset> umap;

Hashmap* Hashmap_new()
{
  return ( (Hashmap*)(new umap()));
}

void Hashmap_free(Hashmap* pMap)
{
  if (pMap != NULL)
  {
    delete ( (umap*)pMap );
  }
}

int Hashmap_add(Hashmap* pMap, uint32_t key, uint32_t val)
{
  umap* pUmap = (umap*)pMap;
  if (pUmap == NULL)
  {
    return (NULL_POINTER_ERROR);
  }

  std::pair<uset::iterator, bool> ret = (*pUmap)[key].insert(val);
  if (ret.second)
  {
    return (1);
  }

  return (0);
}

int Hashmap_remove(Hashmap* pMap, uint32_t key, uint32_t val)
{
  //just going to use the [] operator, which happens to create a new hashtable
  // if its not there already - might change this later
  umap* pUmap = (umap*)pMap;
  if (pUmap == NULL)
  {
    return (NULL_POINTER_ERROR);
  }

  (*pUmap)[key].erase(val);
  return (1);
}


Hashtable* Hashmap_getHashtable(Hashmap* pMap, uint32_t key)
{
  umap* pUmap = (umap*)pMap;
  if (pUmap == NULL)
  {
    return (NULL);
  }

  umap::iterator it = pUmap->find(key);
  if (it == pUmap->end())
  {
    return (NULL);
  }

  return ((Hashtable*)(&(it->second)));
}

int Hashmap_exist(Hashmap* pMap, uint32_t from, uint32_t to)
{
  Hashtable* pTable = Hashmap_getHashtable(pMap, from);

  if (pTable == NULL)
  {
    return (0);
  }

  return (Hashtable_exist(pTable, to));
}

void Hashmap_print(FILE* fp, Hashmap* pMap)
{
  umap* pUmap = (umap*)pMap;

  if (pUmap == NULL)
  {
    return;
  }

  for (umap::const_iterator it = pUmap->begin(); it != pUmap->end(); it++)
  {
    DECAF_fprintf(fp, "  %x ->\n", it->first);
    Hashtable_print(fp, (Hashtable*)(&(it->second)));
  }
}

typedef unordered_map<uint32_t, cset> cmap;

CountingHashmap* CountingHashmap_new()
{
  return ( (CountingHashmap*)(new cmap()));
}

void CountingHashmap_free(CountingHashmap* pMap)
{
  if (pMap != NULL)
  {
    delete ( (cmap*)pMap );
  }
}

uint32_t CountingHashmap_add(CountingHashmap* pMap, uint32_t key, uint32_t val)
{
  cmap* pCmap = (cmap*)pMap;
  if (pCmap == NULL)
  {
    return (0);
  }

  return (CountingHashtable_add((CountingHashtable*)(&(*pCmap)[key]), val));
}

uint32_t CountingHashmap_remove(CountingHashmap* pMap, uint32_t key, uint32_t val)
{
  cmap* pCmap = (cmap*)pMap;
  if (pCmap == NULL)
  {
    return (0);
  }

  return (CountingHashtable_remove((CountingHashtable*)(&(*pCmap)[key]), val));
}

CountingHashtable* CountingHashmap_getCountingHashtable(CountingHashmap* pMap, uint32_t key)
{
  cmap* pCmap = (cmap*)pMap;
  if (pCmap == NULL)
  {
    return (NULL);
  }

  cmap::iterator it = pCmap->find(key);
  if (it == pCmap->end())
  {
    return (NULL);
  }

  return ((CountingHashtable*)(&(it->second)));
}

int CountingHashmap_exist(CountingHashmap* pMap, uint32_t from, uint32_t to)
{
  CountingHashtable* pTable = CountingHashmap_getCountingHashtable(pMap, from);

  if (pTable == NULL)
  {
    return (0);
  }

  return (CountingHashtable_exist(pTable, to));
}

uint32_t CountingHashmap_count(CountingHashmap* pMap, uint32_t key, uint32_t val)
{
  cmap* pCmap = (cmap*)pMap;
  if (pCmap == NULL)
  {
    return (NULL_POINTER_ERROR);
  }

  return (CountingHashtable_count((CountingHashtable*)(&(*pCmap)[key]), val));
}

void CountingHashmap_print(FILE* fp, CountingHashmap* pMap)
{
  cmap* pCmap = (cmap*)pMap;

  if (pCmap == NULL)
  {
    return;
  }

  for (cmap::const_iterator it = pCmap->begin(); it != pCmap->end(); it++)
  {
    DECAF_fprintf(fp, "  %x ->\n", it->first);
    CountingHashtable_print(fp, (CountingHashtable*)(&(it->second)));
  }
}
