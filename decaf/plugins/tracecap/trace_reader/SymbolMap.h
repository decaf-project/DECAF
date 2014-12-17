/**
 *  @Author Lok Yan
 */
#ifndef SYMBOLMAP_H
#define SYMBOLMAP_H

#include <map>
#include <string>
#include <iostream>
#include <inttypes.h>

class MapFileProcessor;

#define MAX_MAP_PROCESSORS 3

class SymbolMap
{
public:
  SymbolMap() : numProcs(0) {}
  int buildMap(const std::string& mapFilename);

  int addProcessor(MapFileProcessor* pMapProc);
  int removeProcessor(MapFileProcessor* pMapProc);

  void add(uint32_t add, const std::string& sym);

  int readMapFromFile(const std::string& inMapFilename);
  void printMap(std::ostream& outs);

  const std::string& find(uint32_t key);
private:
  std::map<uint32_t, std::string> addToSym;
  MapFileProcessor* aProcessors[MAX_MAP_PROCESSORS];
  size_t numProcs;
};

#endif//SYMBOLMAP_H
