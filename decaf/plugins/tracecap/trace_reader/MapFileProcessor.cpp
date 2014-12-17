/**
 *  @Author Lok Yan
 */
#include <fstream>
#include "MapFileProcessor.h"
#include "SymbolMap.h"

using namespace std;

MapFileProcessor::MapFileProcessor(const string& newDir) :  pParent(NULL)
{
  strDirectory = newDir;
}

MapFileProcessor::MapFileProcessor(const string& newDir, SymbolMap* pNewParent) : pParent(pNewParent)
{
  strDirectory = newDir;

  if (pNewParent != NULL)
  {
    pNewParent->addProcessor(this);
  }
}

MapFileProcessor::~MapFileProcessor()
{
  if (pParent != NULL)
  {
    pParent->removeProcessor(this);
  }
}

bool MapFileProcessor::isInterested(const string& str)
{
  return ( (str.find(" r-xp ") != string::npos) || (str.find(" r-xs ") != string::npos) );
}

bool MapFileProcessor::isTextLine(const string& str)
{
  return (str.find(".text") != string::npos);
}

int MapFileProcessor::addSymbolsFromFileToMap(const string& mapLine, const string& header)
{
  if (pParent == NULL)
  {
    return (-1);
  }

  uint32_t startAddr = 0;
  uint32_t endAddr = 0;
  ifstream tempFile;

  //first get the address range of the line
  if (getAddressRange(mapLine, startAddr, endAddr) == 0)
  {
    string s;

    if (getSymbolFileName(s, mapLine) != 0)
    {
      //cerr << "Could not get filename for line [" << mapLine << "]" << endl;
      return (-1);
    }

    tempFile.open(s.c_str(), ifstream::in);
    if (!tempFile.good())
    {
      tempFile.close();
      return (-1);
    }

    //now that we have the file open, we can process the symbols
    string header = s.substr(s.find_last_of('/') + 1);
    header += ":";

    string s2;
    uint32_t add = 0;
    string sym;

    while (tempFile.good())
    {
      getline(tempFile, s2);
      if (s2.empty())
      {
        continue;
      }
      if (getInfoFromSymbolLine(s2, add, sym) != 0)
      {
        continue;
      }
      if ((add + startAddr) >= endAddr)
      {
        cerr << "addSymbolsFromFileToMap: Address [" << add << "] + start [" << startAddr << "] is >= end [" << endAddr << "]" <<endl;
        continue;
      }
      pParent->add(add + startAddr, header+sym);
    }
    tempFile.close();
  }
  else
  {
    cerr << "could not parse line [" << mapLine << "] to get start and end addresses" << endl;
  }
  return (0);
}

int MapFileProcessor::getInfoFromSymbolLine(const string& str, uint32_t& add, string& sym)
{
  if (!isTextLine(str))
    {
      return (-1);
    }

    size_t t1 = str.find_first_of(' ');
    size_t t2 = str.find_last_of(' ');

    if (t1 == string::npos)
    {
      t1 = str.find_first_of('\t');
    }
    if (t2 == string::npos)
    {
      t2 = str.find_last_of('\t');
    }

    if ( (t1 == string::npos) || (t2 == string::npos) )
    {
      return (-1);
    }

    if (myHexStrToul(add, str.substr(0, t1)) < 0)
    {
      return (-1);
    }

    //we should have address here so lets get the symbol
    sym = str.substr(t2+1);

    if (sym.empty())
    {
      return (-1);
    }

    //we are done
    return (0);
}


int MapFileProcessor::getAddressRange(const string& str, uint32_t& startAddr, uint32_t& endAddr)
{
  size_t t1 = 0;
  size_t t2 = 0;

  //assuming that the format is "startAddr-endAddr "
  t1 = str.find_first_of('-');
  t2 = str.find_first_of(' ');

  if ( t2 == string::npos )
  {
    //try the tab character
    t2 = str.find_first_of('\t');
  }
  if ( (t1 == string::npos) || (t2 == string::npos) )
  {
    return (-1);
  }

  if ( (myHexStrToul(startAddr, str.substr(0, t1).c_str()) < 0) ||
       (myHexStrToul(endAddr, str.substr(t1+1, t2 - t1 - 1).c_str()) < 0) )
  {
    return (-2);
  }

  return (0);
}

int MapFileProcessor::getSymbolFileName(string& filename, const string& mapLine)
{
  size_t t = 0;
  string strTemp;

  //the library name is expected to be at the end of the string
  t = mapLine.find_last_of(' ');
  if (t == 0)
  {
    t = mapLine.find_last_of('\t');
  }
  if (t == 0)
  {
    return (-1);
  }

  //at this point we should have the beginning of the string
  strTemp = mapLine.substr(t + 1);

  //now that we have the full path name, try to find the last '/' so we can get
  // the filename
  t = strTemp.find_last_of('/');
  if (t != string::npos)
  {
    strTemp = strTemp.substr(t + 1);
  }

  //so strTemp is just the name of the library now
  // check to make sure that its not empty
  if (strTemp.empty())
  {
    return (-1);
  }

  //if we are here that means we can build everything together
  if (strDirectory.empty())
  {
    filename = strTemp + ".sym";
  }
  else
  {
    if (strDirectory[strDirectory.size() - 1] == '/')
    {
      filename = strDirectory + strTemp + ".sym";
    }
    else
    {
      filename = strDirectory + "/" + strTemp + ".sym";
    }
  }
  return (0);
}
