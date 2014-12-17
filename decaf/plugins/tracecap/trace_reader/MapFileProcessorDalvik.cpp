/**
 *  @Author Lok Yan
 */
#include "MapFileProcessorDalvik.h"

using namespace std;

bool MapFileProcessorDalvik::isInterested(const string& s)
{
  return ((MapFileProcessor::isInterested(s)) && (s.find("/data/dalvik-cache/") != string::npos));
}

bool MapFileProcessorDalvik::isDalvikMethodLine(const string& s)
{
  return (s.find("|[") != string::npos);
}

int MapFileProcessorDalvik::getInfoFromSymbolLine(const string& str, uint32_t& address, string& sym)
{
  if (!isDalvikMethodLine(str))
  {
    return (-1);
  }

  size_t t1 = str.find_first_of("|[");
  size_t t2 = str.find(']', t1);

  if ( (t1 == string::npos) || (t2 == string::npos) )
  {
    return (-1);
  }

  if (myHexStrToul(address, str.substr(t1 + 2, t2 - t1 - 2)) != 0)
  {
    return (-1);
  }

  //we should have address here so lets get the symbol
  for (t2++ ; (t2 < str.size()) && ((str[t2] == ' ') || (str[t2] == '\t')); t2++);

  sym = str.substr(t2);

  if (sym.empty())
  {
    return (-1);
  }

//ADD 0x28 to the address which is our offset from .sym to .dex
address += 0x28;

  //we are done
  return (0);
}

