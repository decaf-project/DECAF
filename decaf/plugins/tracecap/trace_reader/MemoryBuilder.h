/**
 * @file Class used for rebuilding the state of memory from the instruction trace.
 * @Author Lok Yan
 */
#ifndef MEMORYBUILDER_H
#define MEMORYBUILDER_H

#include <map>
#include <vector>
#include <bitset>
#include <inttypes.h>

#include "RebuiltPage.h"

/**
 * Macro for turning a memory address into the KEY for the map.
 */
#define ADDR_TO_KEY(_addr) (_addr >> PAGE_SHIFT)

/**
 * Class that is used to rebuild the memory. Internally it uses a map to organize the memory. The map maps page addresses to RebuiltPages which contains
 * The contents and two bitmaps.
 */
class MemoryBuilder
{
public:
  MemoryBuilder();

  /**
   * Add a new item into the memory
   * @param add Address of the new item
   * @param ptr Pointer to the source
   * @param len The length of the item in bytes
   */
  void addItem(const uint32_t add, const uint8_t* ptr, const size_t len);
  /**
   * Removes (zeroes out and clears bitmap) of the item NOT INCLUDING endAdd
   * @param startAdd The starting address
   * @param endAdd the ending address, NOT INCLUSIVE
   */
  void removeItem(const uint32_t startAdd, const uint32_t endAdd);
  //not implementing this one yet, as it should not be needed
  //int getItem(uint32_t add, std::vector<uint8_t>& ptr);
  /**
   * Gets the item located at address add and copies it into ptr.
   * @param add The address of the item.
   * @param ptr Pointer to the target.
   * @param len The number of bytes to copy
   * @return 0 If successful.
   */
  int getItem(const uint32_t add, uint8_t* ptr, const size_t len);
  /**
   * Gets the item located at address add as well as the corresponding bitmap.
   * @param add The address of the item.
   * @param ptr Pointer to the target.
   * @param len The number of bytes to copy
   * @param bitmap Reference to the bitmap that will contain the corresponding bitmap
   * @return 0 If successful.
   */
  int getItem(const uint32_t add, uint8_t* ptr, const size_t len, std::vector<bool>& bitmap);
  /**
   * Gets the string at address add.
   * @param add The address
   * @param str Reference to the string that will hold the string at address add.
   * @return 0 If successful
   */
  int getString(const uint32_t add, std::string& str);

  /**
   * Dumps the contents of the memory into files that begin with the filenameheader followed by '_' followed by the
   * address of the page. the bitmap and dirty bitmap are also dumped into the .bitmap .dirty files.
   * @param filenameHeader The header to use.
   */
  void dumpMemory(const std::string& filenameHeader, const std::string& strDirectory = ".");

  /**
   * Loads the memory from the files dumped with dumpMemory.
   * @param strFileHeader The file header - see the header from dumpMemory
   * @param strDirectory The directory where the files are located. Defaults to the current directory.
   * @return 0 If successful
   */
  int loadFiles(const std::string& strFileHeader, const std::string& strDirectory = ".");

  const std::map< uint32_t, RebuiltPage* >& getMemory() const { return (memory); }
  const RebuiltPage* getPage(uint32_t addr);
  size_t getNumPages() { return (memory.size());}

  ~MemoryBuilder();
private:
  /**
   * The real workhorse that adds the item. Flag is used so we can also use this for removing items with flag = false.
   * @param add Address of the item
   * @param ptr Pointer to the item
   * @param len Length of the item in bytes
   * @param flag The flag is used to set the bitmap.
   */
  void addItem(const uint32_t add, const uint8_t* ptr, const size_t len, bool flag);
  /**
   * Workhorse for getting the strings. This function uses the bAppend flag to take care of the case
   * when the string goes over a page boundary.
   * @param add Address of the string
   * @param str Reference to the string that will hold the string at address add
   * @param bAppend True if the string at add should appended to str.
   * @return 0 If successful.
   */
  int getString(const uint32_t add, std::string& str, bool bAppend);
  /**
   * Workshorse for getting an item at address add. The bitmap here is REVERSED. Basically the public getItem reverses this one.
   * @param add Address of the item to get
   * @param ptr Pointer to the buffer to store the item in
   * @param len Number of bytes to read
   * @param bitmapREVERSED The bitmap
   * @param bNeedBitmap True if the bitmap is needed, false if bitmap is not.
   * @return 0 If successful.
   */
  int getItem(const uint32_t add, uint8_t* ptr, const size_t len, std::vector<bool>& bitmapREVERSED, bool bNeedBitmap);
  /**
   * memory is a map that maps a page address to a pair pointer
   * where the first element of the pair is the page contents and the second
   * element is a bitmap that tells which bytes are valid and which are not
   */
  std::map< uint32_t, RebuiltPage* > memory;
};

#endif//MEMORYBUILDER_H
