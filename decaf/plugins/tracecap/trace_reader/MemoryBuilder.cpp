/**
 *  @Author Lok Yan
 */
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

#include <dirent.h>

#include "MemoryBuilder.h"
#include "ErrorCodes.h"
#include "HelperFunctions.h"

using namespace std;

MemoryBuilder::MemoryBuilder()
{
  //Nothing to do here yet
}

MemoryBuilder::~MemoryBuilder()
{
  //go through the map and free all of the pointers
  map< uint32_t, RebuiltPage* >::iterator it;

  for (it = memory.begin(); it != memory.end(); it++)
  {
    if ( (*it).second != NULL )
    {
      //cout << "Deleting page: " << hex << (*it).first << endl;

      delete ((*it).second);
      (*it).second = NULL; //set it to NULL just in case
    }
  }
  //now that everything is freed, the rest is up to the map's destructor
}

int MemoryBuilder::loadFiles(const string& strFileHeader, const string& strDirectory)
{
  DIR* pDir = NULL;
  dirent* pDirent = NULL;
  bool bEq = true;
  char* pTemp = NULL;
  uint32_t addr = 0;

  pDir = opendir(strDirectory.c_str());
  if (pDir == NULL)
  {
    return (-1);
  }

  for (pDirent = readdir(pDir); pDirent != NULL; pDirent = readdir(pDir))
  {
    bEq = true;
    if (pDirent->d_type == DT_REG)
    {
      pTemp = strchr(pDirent->d_name, '_');
      if (pTemp != NULL)
      {
        size_t i = 0;
        for (; i < strFileHeader.size(); i++)
        {
          if (strFileHeader[i] != pDirent->d_name[i]) // this should be fine since d_name is guaranteed to end with a NULL
          {
            bEq = false;
          }
        }
        if (bEq && (pDirent->d_name + i == pTemp)) //if the files headers are the same then try to extract the addresses out
        {
          pTemp += 1;
          if (myHexStrToul(addr, pTemp) == 0)
          {
            //now we have the address that we are looking for,
            // so we can open up this plus the .bitmap and .dirtymap files

            //NOTE that because myHexStrToul uses strtoul which tops whenenver the uint32 is filled, .bitmap and .dirtymap filenames
            // also satisfy this criteria -- will need to implement my own HexStrToul function soon

            map< uint32_t, RebuiltPage* >::iterator it;
            RebuiltPage* pRP = NULL;
            it = memory.find(ADDR_TO_KEY(addr));
            if (it == memory.end()) // if the page does not exist, then create it
            {
              pRP = new (RebuiltPage);
              memory[ADDR_TO_KEY(addr)] = pRP;
            }
            else
            {
              pRP = (*it).second;
            }
            pRP->loadFromFile(pDirent->d_name, strDirectory);
          }
        }
      }
    }
  }
  return (0);
}

void MemoryBuilder::addItem(const uint32_t add, const uint8_t* ptr, const size_t len)
{
  if (ptr == NULL)
  {
    return;
  }
  if (len == 0)
  {
    return;
  }

  addItem(add, ptr, len, true);
}

const RebuiltPage* MemoryBuilder::getPage(uint32_t addr)
{
  map< uint32_t, RebuiltPage* >::iterator it;
  it = memory.find(ADDR_TO_KEY(addr));
  if (it == memory.end())
  {
    return (NULL);
  }

  return ((*it).second);
}

void MemoryBuilder::addItem(const uint32_t add, const uint8_t* ptr, const size_t len, bool flag)
{
  uint32_t key = ADDR_TO_KEY(add);
  uint32_t offset = add & ~PAGE_MASK;

  map< uint32_t, RebuiltPage* >::iterator it;

  it = memory.find(key);

  RebuiltPage* pTemp = NULL;

  //if it doesn't exist then create a new item.
  if (it == memory.end())
  {
    pTemp = new (RebuiltPage);
    memory[key] = pTemp;
  }
  else
  {
    pTemp = (*it).second;
  }

  //at this point pTemp is pointing to the memory page of interest
  // so lets copy over the info

  //but first we need to take care of the special case where this
  // goes over the page boundary
  size_t left = len;

  if ((offset + len - 1) & PAGE_MASK)
  {
    //if it goes across the page then process the overflow first
    left = (1 << PAGE_SHIFT) - offset;

    if (ptr != NULL)
    {
      addItem(add + left, ptr + left, len - left, flag);
    }
    else
    {
      addItem(add + left, ptr, len - left, flag);
    }
  }

  if (ptr != NULL)
  {
    memcpy((void*)(pTemp->page + offset), ptr, left);
  }
  else
  {
    memset((void*)(pTemp->page + offset), 0, left);
  }

  //now set the corresponding bits in the bitmap to 1
  for (uint32_t i = offset; i < (left + offset); i++)
  {
    pTemp->bitmap[i] = flag;
  }

  //now we are done
}

void MemoryBuilder::removeItem(const uint32_t startAdd, const uint32_t endAdd)
{
  addItem(startAdd, NULL, endAdd - startAdd, false);
}

//this is pretty much the same as addItem - warning that bitmap will be in REVERSE order
int MemoryBuilder::getItem(const uint32_t add, uint8_t* ptr, const size_t len, vector<bool>& bitmap, bool bNeedBitmap)
{
  if (ptr == NULL)
  {
    return (NULL_POINTER_ERROR);
  }

  //clear out the bitmap first
  if (bNeedBitmap)
  {
    bitmap.clear();
  }

  if (len == 0)
  {
    return (0);
  }

  uint32_t offset = (add & ~PAGE_MASK);

  map< uint32_t, RebuiltPage* >::iterator it;
  it = memory.find(ADDR_TO_KEY(add));
  if (it == memory.end())
  {
    return (-1);
  }

  RebuiltPage* pTemp = (*it).second;

  size_t left = len;
  //check to see if we need to go to the next page
  if ((offset + len - 1) & PAGE_MASK)
  {
    //since we do, lets process the next page first
    left = (1 << PAGE_SHIFT) - offset;
    int ret = getItem(add + left, ptr + left, len - left, bitmap, bNeedBitmap);
    if (ret != 0)
    {
      return (ret);
    }
  }

  memcpy(ptr, (void*)(pTemp->page + offset), left);
  //now we need to fill in the bitmap BACKWARDS to maintain the right order
  // this is mainly because we are filling in the array backwards
  // The only problem is that the vector is cheap when pushing back
  // and not from the front, thus the warning at the beginning.

  if (bNeedBitmap)
  {
    for (size_t i = 0; i < left; ++i)
    {
      bitmap.push_back(pTemp->bitmap[offset+left-i-1]);
    }
  }

  return (0);
}

int MemoryBuilder::getString(const uint32_t add, string& str, bool bAppend)
{
  if (!bAppend)
  {
    str.clear();
  }

  uint32_t offset = (add & ~PAGE_MASK);

  //first lets find the right page
  map< uint32_t, RebuiltPage* >::iterator it;
  it = memory.find(ADDR_TO_KEY(add));
  if (it == memory.end())
  {
    return (-1);
  }

  RebuiltPage* pTemp = (*it).second;

  char* src = (char*)(pTemp->page);
  size_t i = offset;
  while ((i < PAGE_SIZE) && (src[i] != '\0') && (pTemp->bitmap[i]))
  {
    str.push_back(src[i]);
    i++;
  }

  //now check to see if we are at a page boundary, if we are then we need to continue
  if (i == PAGE_SIZE)
  {
    getString((add & PAGE_MASK) + (0x1 << PAGE_SHIFT), str, true);
  }
  return (0);
}

int MemoryBuilder::getString(const uint32_t add, string& str)
{
  return getString(add, str, false);
}

int MemoryBuilder::getItem(const uint32_t add, uint8_t* ptr, const size_t len)
{
  vector<bool> temp;
  return (getItem(add, ptr, len, temp, false));
}

int MemoryBuilder::getItem(const uint32_t add, uint8_t* ptr, const size_t len, vector<bool>& bitmap)
{
  vector<bool> temp;
  int ret = getItem(add, ptr, len, temp, true);
  if (ret != 0)
  {
    return (ret);
  }

  bitmap.clear();
  for(int i = (temp.size()-1); i >= 0; i--)
  {
    bitmap.push_back(temp[i]);
  }
  return (0);
}

void MemoryBuilder::dumpMemory(const string& filenameHeader, const string& strDirectory)
{
  map< uint32_t, RebuiltPage* >::iterator it;

  for (it = memory.begin(); it != memory.end(); it++)
  {
    RebuiltPage* pTemp = (*it).second;
    if ( pTemp != NULL )
    {
      stringstream ss;
      ss << filenameHeader << '_' << setw(8) << setfill('0') << hex << ((*it).first << PAGE_SHIFT);
      pTemp->writeToFile(ss.str(), strDirectory);
    }
  }
}
