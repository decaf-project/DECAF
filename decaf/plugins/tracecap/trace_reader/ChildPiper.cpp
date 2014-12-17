/**
 * @author Lok Yan
 * @date 4/26/2012
**/
//Here is the piped version, 
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include "ChildPiper.h"

using namespace std;


void ChildPiper::setPath(const string& path)
{
  if (bStarted)
  {
    return;
  }

  childPath = path;
  bInit = true;
}

void ChildPiper::addParam(const string& param)
{
  if (bStarted)
  {
    return;
  }

  childParams.push_back(param);
}

void ChildPiper::sendEOF()
{
  if (!bInit)
  {
    return;
  }
 
  if (p2cpipe[1] != -1)
  {
    close(p2cpipe[1]);
    p2cpipe[1] = -1;
  }
}


void ChildPiper::cleanup()
{
  if (!bInit)
  {
    return;
  }

  bInit = false;

  //if it has been initialized 
  childPath.clear();
  childParams.clear();

  if (!bStarted) //but not started
  {
    return;
  }

  if (p2cpipe[0] != -1)
  {
    close(p2cpipe[0]);
    p2cpipe[0] = -1;
  }
  if (p2cpipe[1] != -1)
  {
    close(p2cpipe[1]);
    p2cpipe[1] = -1;
  }
  if (c2ppipe[0] != -1)
  {
    close(c2ppipe[0]);
    c2ppipe[0] = -1;
  }
  if (c2ppipe[1] != -1)
  {
    close(c2ppipe[1]);
    c2ppipe[1] = -1;
  }

  if (childPid != (pid_t) 0)
  {
    //since the child is still there, maybe we should kill it
    //we kill the child here if its not already dead
    int stat = 0;
    waitpid(childPid, &stat, WNOHANG);
    if (WIFEXITED(stat))
    {
      waitpid(childPid, &stat, 0); //to clear out the status
      //don't know why WNOHANG doesn't do it.
      childPid = 0;
    }
    else
    {
      kill(childPid, SIGKILL);
      waitpid(childPid, &stat, 0);
    }

    childPid = (pid_t)0;
  }

  bStarted = false;

  if (bUseHistory)
  {
    history.clear();
    counter = 0;
  }
}

int ChildPiper::spawn()
{
  if (!bInit)
  {
    return (-1);
  }

  if (bStarted)
  {
    return (0);
  }

  if (pipe(p2cpipe) != 0)
  {
    return (-1);
  }

  if (pipe(c2ppipe) != 0)
  {
    cleanup();
    return (-1);
  }


  childPid = fork();
  if (childPid < (pid_t)0)
  {
    //close the pipes
    cleanup();
    return (-1);
  }

  if (childPid == 0)
  {
    //we are in the child
    //we don't need the write end of the p2cpipe
    close(p2cpipe[1]);
    p2cpipe[1] = -1;

    // we need to duplicate the pipes to stdin and out
    close(0);
    dup2(p2cpipe[0], 0); //STDIN);

    //we don't need the read end of c2ppipe
    close(c2ppipe[0]);
    c2ppipe[0] = -1;

    close(1);
    dup2(c2ppipe[1], 1); //STDOUT);
    dup2(c2ppipe[1], 2); //STDOUT);

    //convert the parameters to a cstring list
    size_t len = childParams.size();
    char** temp = (char**)calloc(len + 1, sizeof(char*));
    if (temp == NULL) // uh-oh out of memory just die. (does this work?)
    {
      exit(-1);
    }
 
    for (size_t i = 0; i < len; i++)
    {
      size_t j = childParams[i].length();
      temp[i] = (char*)malloc(j + 1);
      if (temp[i] == NULL)
      {
        exit(-1);
      }
      childParams[i].copy(temp[i], j);
      temp[i][j] = '\0';
    }

    temp[len] = NULL; 

    //now that we have it setup, we can spawn the child process
    execve(childPath.c_str(), temp, NULL);
    //execve(Z3PATH, NULL, NULL);

/** Used for debugging the pipe setup - child simply echos
    string s;
    while (1)
    {
      getline(cin, s);
      printf("%s\n", s.c_str());
      fflush(stdout);
    }

    exit(0);
/** Ignore this warning - I used comments within comments on purpose **/
  }
  else
  {
    //we are in the parent so do the opposite as the child
    close(p2cpipe[0]);
    p2cpipe[0] = -1;
    close(c2ppipe[1]);
    c2ppipe[1] = -1;
  }

  bStarted = true;
  return (0);
}

int ChildPiper::writeline(const char* str)
{
  if (p2cpipe[1] != -1)
  {
    string s(str);
    s.append("\n");
    if (bUseHistory)
    {
      history.push(str);
      counter++;
    }
    return (write(p2cpipe[1], s.c_str(), s.length()));
  }

  return (-1);
}

int ChildPiper::readline(string& str)
{
  int i = 0;

  if ( cbuf == EOF )
  {
    return (-1);
  }
  if (c2ppipe[0] == -1)
  {
    return (-1);
  }

  str.clear();

  ssize_t ret = read(c2ppipe[0], &cbuf, 1);

  while ((cbuf != '\n') && (cbuf != EOF) && (ret > 0) )
  {
    str.append(1, cbuf); //add the new character in there
    i++;
    ret = read(c2ppipe[0], &cbuf, 1);
  }

  /** NOTHING TO DO IN THIS CASE, just default to 0 **
  if (cbuf == '\n') // if it is the newline character, then return 1
  {
  }
  **/

  if ( (cbuf == EOF) || (ret == 0) ) //0 means end of file
  {
    cleanup();
    return (-1);
  }

  return (i);
}

int ChildPiper::printHistory(FILE* fp)
{
  if (fp == NULL)
  {
    return (-1);
  }

  for (size_t i = 0; i < history.size(); i++)
  {
    fprintf(fp, "%d:  %s\n", counter - (history.size() - i) + 1, history.at(history.size() - i - 1).c_str());
  }
  return (0);
}
