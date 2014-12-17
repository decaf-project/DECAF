/**
 * @file Processor that processes Dalvik symbols
 * @Author Lok Yan
 */
#ifndef MAPFILEPROCESSORDALVIK_H
#define MAPFILEPROCESSORDALVIK_H

#include <string>
#include "MapFileProcessor.h"

/**
 * Class for processing Dalvik symbols. It is based off of the MapFileProcessor but instead of looking for objdump files it looks for dexdump files
 * to build the symbols.
 */
class MapFileProcessorDalvik : public MapFileProcessor
{
public:
  /**
   * Constructor
   * @param newDir The directory where the dexdumped files are located
   */
  MapFileProcessorDalvik(const std::string& newDir) : MapFileProcessor(newDir) {}
  /**
   * @param newDir The directory where the dexdumped files are located
   * @param pNewParent Pointer to the parent
   */
  MapFileProcessorDalvik(const std::string& newDir, SymbolMap* pNewParent) : MapFileProcessor(newDir, pNewParent) {}
  /**
   * @param mapLine The line of the map file
   * @return True if its a Dalvik line in the memory map file.
   */
  bool isInterested(const std::string& mapLine);
  /**
   * Processes the file to get all of the Dalvik related symbols.
   * @param mapLine Line of the memory map file
   * @param header The header prepended to all symbol names. Default is the empty string.
   * @return 0 If successful.
   */
  int addSymbolsFromFileToMap(const std::string& mapLine, const std::string& header = "");
  /**
   * Similar to isTextLine in MapFileProcessor but this one determines whether a symbol exists.
   * @param s Line of the dexdump file.
   * @return True if there is a symbol in this line.
   */
  bool isDalvikMethodLine(const std::string& s);
  /**
   * Parses the line of the dexdump file to get the address and symbol.
   * @param s The line of the dexdump file
   * @param add Reference to the uint32_t that will hold the address
   * @param sym Reference to the string that will hold the symbol
   * @return 0 If successful.
   */
  int getInfoFromSymbolLine(const std::string& s, uint32_t& add, std::string& sym);
};

#endif//MAPFILEPROCESSORDALVIK_H
