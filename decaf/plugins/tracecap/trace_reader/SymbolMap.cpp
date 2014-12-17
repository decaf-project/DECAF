/**
 *  @Author Lok Yan
 */
#include <fstream>
#include <iomanip>
#include <inttypes.h>

#include "SymbolMap.h"
#include "MapFileProcessor.h"

#include "Common.h"

using namespace std;

const string& SymbolMap::find(uint32_t key)
{
  map<uint32_t, string>::iterator it;

  it = addToSym.find(key);
  if (it != addToSym.end())
  {
    return ((*it).second);
  }
  return (strEmpty);
}

void SymbolMap::add(uint32_t add, const string& sym)
{
  addToSym[add] = sym;
}

int SymbolMap::addProcessor(MapFileProcessor* pProcessor)
{
  if (numProcs >= MAX_MAP_PROCESSORS)
  {
    return (-1);
  }

  aProcessors[numProcs] = pProcessor;
  //pProcessor->setParent(this);
  numProcs++;
  return (0);
}

int SymbolMap::removeProcessor(MapFileProcessor* pProcessor)
{
  if (numProcs == 0)
  {
    return (0);
  }

  for (size_t i = 0; i < numProcs; i++)
  {
    if (aProcessors[i] == pProcessor)
    {
      for (size_t j = i; (j + 1) < numProcs; j++)
      {
        aProcessors[j] = aProcessors[j+1];
      }
      //pProcessor->removePraent();
      break;
    }
  }
  numProcs--;
  return (0);
}

int SymbolMap::buildMap(const string& mapFilename)
{
  ifstream mapFile;

  mapFile.open(mapFilename.c_str(), ifstream::in);

  string strTemp;

  while (mapFile.good())
  {
    getline(mapFile, strTemp);
    if (strTemp.empty())
    {
      continue;
    }

    for (size_t i = 0; i < numProcs; i++)
    {
      if (aProcessors[i]->isInterested(strTemp))
      {
        aProcessors[i]->addSymbolsFromFileToMap(strTemp);
      }
    }
  }
  mapFile.close();
  return (0);
}

int SymbolMap::readMapFromFile(const string& inMapFilename)
{
  if (inMapFilename.empty())
  {
    return (-1);
  }

  ifstream ifs(inMapFilename.c_str(), ifstream::in);
  string s;

  uint32_t addr;
  string sym;

  size_t t;

  while (ifs.good())
  {
    getline(ifs, s);
    if (s.empty())
    {
      continue;
    }
    t = s.find_first_of(',');
    if (t == string::npos)
    {
      continue;
    }
    if (myHexStrToul(addr, s) != 0)
    {
      cerr << "readSymbolMap: Error with the following line [" << s << "]" << endl;
      continue;
    }
    sym = s.substr(t+1);
    addToSym[addr] = sym;
  }

  if (addToSym.empty())
  {
    return (-1);
  }
  return (0);
}

void SymbolMap::printMap(ostream& outs)
{
  map<uint32_t, string>::iterator it;

  for (it = addToSym.begin(); it != addToSym.end(); it++)
  {
    outs << setw(8) << setfill('0') << hex << (*it).first << "," << (*it).second << endl;
  }
}
