/**
 * @author Lok Yan
 * @date 4/26/2012
 */
//Here is the piped version, 

#ifndef CHILDPIPER_H
#define CHILDPIPER_H

#include <string>
#include <vector>
#include "History.h"

class ChildPiper
{
public:

  ChildPiper(bool useHistory = false) : bInit(false), bStarted(false), childPid(0), cbuf(0), counter(0), bUseHistory(useHistory)
  { 
    p2cpipe[0] = -1;
    p2cpipe[1] = -1; 
    c2ppipe[0] = -1;
    c2ppipe[1] = -1;
  }

  ChildPiper(const std::string& path, const std::vector<std::string>& params, bool useHistory = false) : bInit(true), bStarted(false), bUseHistory(useHistory), childPid(0), cbuf(0), counter(0), childPath(path), childParams(params)
  {
    p2cpipe[0] = -1;
    p2cpipe[1] = -1;
    c2ppipe[0] = -1;
    c2ppipe[1] = -1;
  }

  void setPath(const std::string& path);
  
  void addParam(const std::string& param); 

  const std::string& getPath() const;
  const std::vector<std::string>& getParams() const;
  
  /**
   * Closes any open file descriptors and resets values
   */
  void cleanup();
  
  void sendEOF();

  ~ChildPiper() { cleanup(); }

  int spawn();

  int writeline(const char* str);
  
  //reads a line from the child to str
  // returns the number of characters read (minus the endline character)
  //This means that 0 means an endline was read
  // whereas 1 means 1 character was read followed by endline, or end of file
  int readline(std::string& str);

  int printHistory(FILE* fp);

private:
  int p2cpipe[2];
  int c2ppipe[2];
  bool bInit;
  bool bStarted;
  pid_t childPid;
  
  char cbuf;

  History<std::string> history;
  size_t counter;
  bool bUseHistory;

  std::string childPath; 
  std::vector<std::string> childParams; 
};

#endif//CHILDPIPER_H
