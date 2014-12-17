/**
 *  @Author Lok Yan
 */
#include "RebuiltPage.h"
#include <iostream>
#include <fstream>
#include <cstring>

using namespace std;

RebuiltPage::RebuiltPage()
{
  page = new uint8_t[PAGE_SIZE];
  memset(page, 0, PAGE_SIZE);
  //the bitsets are initialized with 0s so we are good
}

RebuiltPage::~RebuiltPage()
{
  if (page != NULL)
  {
    delete (page);
  }
  page = NULL;
}

int RebuiltPage::loadFromFile(const string& strFilename, const string& strDirectory)
{
  ifstream ifs;
  ifstream ifsb;
  ifstream ifsd;

  string strFn = strDirectory + "/";
  strFn += strFilename;
  string strBitmapName = strFn + ".bitmap";
  string strDirtymapName = strFn + ".dirtymap";

  ifs.open(strFn.c_str(), ifstream::binary);
  if (!ifs.good())
  {
    //cerr << "Could not open [" << strFilename << "] for read" << endl;
    return (-1);
  }

  ifsb.open(strBitmapName.c_str());
  if (!ifsb.good())
  {
    //cerr << "Could not open [" << strBitmapName << "] for read" << endl;
    ifs.close();
    return (-1);
  }

  ifsd.open(strDirtymapName.c_str());
  if (!ifsd.good())
  {
    //cerr << "Could not open [" << strDirtymapName << "] for read" << endl;
    ifs.close();
    ifsb.close();
    return (-1);
  }

  //now that the files are open, lets read them in
  ifs.read((char*)page, PAGE_SIZE);
  if (ifs.gcount() != PAGE_SIZE)
  {
    return (-2);
  }

  int i = 0;

  for (i = 0; (i < PAGE_SIZE) && (ifsb.good()); i++)
  {
    bitmap[i] = (ifsb.get() == '1');
  }
  if (i != PAGE_SIZE)
  {
    return (-2);
  }

  for (i = 0; (i < PAGE_SIZE) && (ifsd.good()); i++)
  {
    dirtymap[i] = (ifsd.get() == '1');
  }
  if (i != PAGE_SIZE)
  {
    return (-2);
  }

  ifs.close();
  ifsb.close();
  ifsd.close();
  /*
  for (int j = 0; j < 10; j++)
  {
    cerr << hex << (int) page[j] << " -- " << bitmap[j] << " " << dirtymap[j] << endl;
  }
  */
  return (0);
}

int RebuiltPage::writeToFile(const string& strFilename, const string& strDirectory)
{
  ofstream ofs;
  ofstream ofsb;
  ofstream ofsd;

  string strFn = strDirectory + "/";
  strFn += strFilename;
  string strBitmapName = strFn + ".bitmap";
  string strDirtymapName = strFn + ".dirtymap";

  ofs.open(strFn.c_str(), ofstream::binary | ofstream::trunc);
  if (!ofs.good())
  {
    //cerr << "Could not open [" << strFilename << "] for write" << endl;
    return (-1);
  }

  ofsb.open(strBitmapName.c_str(), ofstream::trunc);
  if (!ofsb.good())
  {
    //cerr << "Could not open [" << strBitmapName << "] for write" << endl;
    ofs.close();
    return (-1);
  }

  ofsd.open(strDirtymapName.c_str(), ofstream::trunc);
  if (!ofsd.good())
  {
    //cerr << "Could not open [" << strDirtymapName << "] for write" << endl;
    ofs.close();
    ofsb.close();
    return (-1);
  }

  //now that the two files are open, lets write them out;
  ofs.write((char*)page, PAGE_SIZE);
  for (int i = 0; i < PAGE_SIZE; i++)
  {
    ofsb << bitmap[i];
  }
  for (int i = 0; i < PAGE_SIZE; i++)
  {
    ofsd << dirtymap[i];
  }

  ofs.close();
  ofsb.close();
  ofsd.close();

  return (0);
}
