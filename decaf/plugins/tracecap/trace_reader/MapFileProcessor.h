/**
 * @file Class for building the symbol maps.
 * @Author Lok Yan
 */
#ifndef MAPFILEPROCESSOR_H
#define MAPFILEPROCESSOR_H

#include <string>
#include <inttypes.h>

#include "HelperFunctions.h"

/**
 * Forward declaration.
 */
class SymbolMap;

/**
 * This class is used to process lines of a Linux memory map file. It does not actually read the map file itself, that job is left
 * for the "parent" of this processor. A single "parent" can have multiple processors of course. The SymbolMap itself is held by the "parent"
 * This processor basically takes a line of the map file, gets the address range, gets the name of the file that is mapped into it
 * and then looks for the symbol information (e.g. objdump) in the corresponding file, and builds the symbol map.
 */
class MapFileProcessor
{
public:
  /**
   * @param newDir The base directory for the symbol files.
   */
  MapFileProcessor(const std::string& newDir);
  /**
   * @param newDir The base directory for the symbol files
   * @param pNewParent The parent who will provide the map file lines as well as hold the symbolmap itself.
   */
  MapFileProcessor(const std::string& newDir, SymbolMap* pNewParent);
  /**
   * Set the parent.
   * @param pNewParent Pointer to the SymbolMap parent.
   */
  void setParent(SymbolMap* pNewParent) { pParent = pNewParent; }
  /**
   * Remove the parent.
   */
  void removePraent() { pParent = NULL; }
  /**
   * Determines whether this processor is interested in this particular map line.
   * @param mapLine The line of the mapfile
   * @return True if interested.
   */
  bool isInterested(const std::string& mapLine);
  /**
   * Adds all of the symbols in the file specified by the line of the map file into the symbol map. This assumes that isInterested returned true.
   * @param mapLine The line of the map file
   * @param header A header that is prepended to all symbol strings. It is empty by default.
   * @return 0 If successful.
   */
  int addSymbolsFromFileToMap(const std::string& mapLine, const std::string& header = "");
  /**
   * Parses the mapLine to obtain the start and end addresses
   * @param mapLine The line to parse
   * @param startAddr Reference to variable that will hold the start address
   * @param endAddr Reference to variable that will hold the end address
   * @return 0 If successful.
   */
  int getAddressRange(const std::string& mapLine, uint32_t& startAddr, uint32_t& endAddr);
  /**
   * Parses the mapLine to obtain the file name
   * @param str Reference to the string that will contain the filename
   * @param mapLine The line of the map file
   * @return 0 If successful.
   */
  int getSymbolFileName(std::string& str, const std::string& mapLine);
  /**
   * Parses a line of a symbol (e.g. objdump) file to get the address of the symbol as well as the symbol name.
   * @param s The line of the file.
   * @param add Reference to the uint32_t that will hold the address.
   * @param sym Reference to the string that will hold the symbol.
   * @return 0 If successful.
   */
  int getInfoFromSymbolLine(const std::string& s, uint32_t& add, std::string& sym);
  /**
   * Determines whether the line is a "text" line - that is if its a function, symbol of interest
   * @param str The line of the symbol (e.g. objdump) file.
   * @return True if its a "text" line
   */
  bool isTextLine(const std::string& str);
  ~MapFileProcessor();
private:
  SymbolMap* pParent;
  std::string strDirectory;
};

#endif//MAPFILEPROCESSOR_H
