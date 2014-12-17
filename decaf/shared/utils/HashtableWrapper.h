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
 * HashtableWrapper.h
 *
 *  Created on: Dec 21, 2011
 *      Author: lok
 */

#ifndef HASHTABLEWRAPPER_H_
#define HASHTABLEWRAPPER_H_

#ifdef __cplusplus
extern "C"
{
#endif

  #include <stdio.h>
  #include "cpu.h"

  //A regular unordered hashtable
  typedef struct Hashtable Hashtable;
  //A regular unordered hashmap
  typedef struct Hashmap Hashmap;
  //An unordered hashtable that also maintains a count
  // of the number of times a certain key has been added
  // and decremented when a key is removed
  typedef struct CountingHashtable CountingHashtable;
  //Similar to the counting hashtable
  typedef struct CountingHashmap CountingHashmap;

  Hashtable* Hashtable_new(void);
  void Hashtable_free(Hashtable* pTable);
  int Hashtable_add(Hashtable* pHash, uint32_t item);
  int Hashtable_remove(Hashtable* pHash, uint32_t item);
  int Hashtable_exist(Hashtable* pHash, uint32_t item);
  void Hashtable_print(FILE* fp, Hashtable* pHash);

  CountingHashtable* CountingHashtable_new(void);
  void CountingHashtable_free(CountingHashtable* pTable);
  /** Returns the count for the key **/
  uint32_t CountingHashtable_add(CountingHashtable* pHash, uint32_t item);
  /** Returns the count for the key **/
  uint32_t CountingHashtable_remove(CountingHashtable* pHash, uint32_t item);
  int CountingHashtable_exist(CountingHashtable* pHash, uint32_t item);
  uint32_t CountingHashtable_count(CountingHashtable* pTable, uint32_t key);
  void CountingHashtable_print(FILE* fp, CountingHashtable* pHash);

  Hashmap* Hashmap_new(void);
  void Hashmap_free(Hashmap* pMap);
  int Hashmap_add(Hashmap* pMap, uint32_t key, uint32_t val);
  int Hashmap_remove(Hashmap* pMap, uint32_t key, uint32_t val);
  Hashtable* Hashmap_getHashtable(Hashmap* pMap, uint32_t key);
  int Hashmap_exist(Hashmap* pMap, uint32_t from, uint32_t to);
  void Hashmap_print(FILE* fp, Hashmap* pMap);

  CountingHashmap* CountingHashmap_new(void);
  void CountingHashmap_free(CountingHashmap* pMap);
  uint32_t CountingHashmap_add(CountingHashmap* pMap, uint32_t key, uint32_t val);
  uint32_t CountingHashmap_remove(CountingHashmap* pMap, uint32_t key, uint32_t val);
  CountingHashtable* CountingHashmap_getCountingHashtable(CountingHashmap* pMap, uint32_t key);
  int CountingHashmap_exist(CountingHashmap* pMap, uint32_t from, uint32_t to);
  void CountingHashmap_print(FILE* fp, CountingHashmap* pMap);

#ifdef __cplusplus
}
#endif
#endif /* HASHTABLEWRAPPER_H_ */
