/**
 * @file Data structure for holding a rebuilt page.
 * @Author Lok Yan
 */
#ifndef REBUILTPAGE_H
#define REBUILTPAGE_H

#include <inttypes.h>
#include <bitset>
#include <string>

/**
 * Page size
 */
#define PAGE_SIZE 4096
/**
 * Amount to shift to get to the page number
 */
#define PAGE_SHIFT 12
/**
 * A mask used to get the page number
 */
#define PAGE_MASK 0xFFFFF000

/**
 * Data structure representing a page of memory. The pages are allocated and freed on the heap by the constructors and destructors.
 * In addition to the page contents, a bitmap for specifying whether the contents is valid and another for whether it is dirty
 * is also used. In this way, when memory is being rebuilt from the instruction trace, we can keep track of the bytes that are
 * valid (i.e. actually from the trace) and the ones that are missing (if the bitmap is false). We can also keep track of which
 * bytes are dirty - that is the value contained within does not match the value in an instruction. This happens if the memory
 * location was changed by another process (not contained within the instruction trace) or the OS.
 */
class RebuiltPage
{
public:
  RebuiltPage();
  ~RebuiltPage();
  /**
   * Load the contents of the page from file. The page address is parsed out from the file name "..._xxxxxxxx" where ... is the header
   * and xxxxxxxx is the full page address. Then it looks for the .bitmap and .dirty files
   * @param strFilename The name of the file to load
   * @return 0 if successful
   */
  int loadFromFile(const std::string& strFilename, const std::string& strDirectory = ".");
  /**
   * Writes the contents of the page to a file with strFilename as the name in addition to .bitmap and .dirty files.
   * @param strFilename The filename to write.
   * @return
   */
  int writeToFile(const std::string& strFilename, const std::string& strDirectory = ".");

  /**
   * Pointer to the page
   */
  uint8_t* page;
  /**
   * Valid bitmap
   */
  std::bitset<PAGE_SIZE> bitmap;
  /**
   * Dirty bitmap
   */
  std::bitset<PAGE_SIZE> dirtymap;
};

#endif//REBUILTPAGE_H
