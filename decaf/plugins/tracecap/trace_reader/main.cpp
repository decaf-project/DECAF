/**
 *  @Author Lok Yan
 */
#if 0
//#include <gtk/gtk.h>

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <string>
#include <iostream>

#include "Taint.h"

#include "TraceConverterARM.h"

#include "TraceProcessorX86TraceCompressor.h"

using namespace std;

#include "LibOpcodesWrapper_ARM.h"
#include "TraceReaderBinX86.h"
int main(int argc, char* argv[])
{
  TraceReaderBinX86 tr;
  string inFile;
  string outFile;
  bool bVerbose = false;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-i") == 0)
    {
      i++;
      if (i < argc)
      {
        inFile = argv[i];
      }
      else
      {
        std::cout << "Error: Need a filename after -i" << endl;
        return (-1);
      }
    }
    else if (strcmp(argv[i], "-o") == 0)
    {
      i++;
      if (i < argc)
      {
        outFile = argv[i];
      }
      else
      {
        std::cerr << "Error: Need a filename after -o" << endl;
        return (-1);
      }
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      bVerbose = true;
    }
    else if ((argv[i][0] != '-') && (inFile.empty()))
    {
      inFile = argv[i];
    }
    else
    {
      cerr << "Unknown option [" << argv[i] << endl;
      return (-1);
    }
  }

  if (inFile.empty())
  {
    fprintf(stderr, "Need an input file.\n");
    return (-1);
  }

  tr.init(inFile, outFile);
  tr.setVerbose(bVerbose);
  tr.run();
}


#endif
//#if 0

//Here is some sample code that uses the new piper to process a version 41 trace. Note that it require bap's toil program 

//#define V41

#include <stdio.h>
#include <string>
#include <cstring>
#include <fstream>
#include "TraceReaderBinX86.h"
#include "TraceProcessorX86Verify.h"
#include "TraceProcessorX86TaintSummary.h"


using namespace std;


//this one was the start of a tainted DL
//static std::ifstream::streampos startInterest = 49932042;
//this one is the start of a taint with T2
//static std::ifstream::streampos startInterest = 49938996;
//This is the beginning of the tainted area for ubuntu.904
//static std::ifstream::streampos startInterest = 85428141;
//this is the beginning for cmpxchg
static std::ifstream::streampos startInterest = 13952644;


//hostfile is created by gcc -nostdlib -nostdinc -m32 host.s -o hostfile
// and then stripped to reduce the size using strip hostfile
// it is fairly easy to search for the 15 nops that I used to create host.s
 
int main(int argc, char** argv)
{
  TraceReaderBinX86 tr;
  TraceProcessorX86Verify bapVerifier(false, true);
  TraceProcessorX86TaintSummary taintSummary(true);

  string strInFile;
  string strOutFile;
  bool bVerbose = false;
  size_t counter = 0;
  size_t bapcount = 0;
  size_t tsumcount = 0; 
  uint64_t filepos = 0;
  bool bBAP = false;
  bool bSummary = false;

  FILE* hostFile = NULL;
    
  //process the parameters 
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-i") == 0)
    {
      i++;
      if (i < argc)
      {
        strInFile = argv[i];
      }
      else
      {
        std::cout << "Error: Need a filename after -i" << endl;
        return (-1);
      }
    }
    else if (strcmp(argv[i], "-o") == 0)
    {
      i++;
      if (i < argc)
      {
        strOutFile = argv[i];
      }
      else
      {
        std::cerr << "Error: Need a filename after -o" << endl;
        return (-1);
      }
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      bVerbose = true;
    }
    else if ((argv[i][0] != '-') && (strInFile.empty()))
    {
      strInFile = argv[i];
    }
    else if (strcmp(argv[i], "-bap") == 0)
    {
      bBAP = true;
    }
    else if (strcmp(argv[i], "-tsum") == 0)
    {
      bSummary = true;
    }
    else
    {
      cerr << "Unknown option [" << argv[i] << "]" << endl;
      return (-1);
    }
  }

  if (strInFile.empty())
  {
    fprintf(stderr, "Need an input file.\n");
    return (-1);
  }

  //open the file
  tr.init(strInFile, ""); //defaults to cout
  //infact, we have to use COUT because of the use of system for the BAP test
  //tr.disableStringConversion(); //don't convert into strings yet

  //this is an approximate starting location of some tainted instructions
  // it should be 816330 instructions into the trace

  //tr.seekTo(startInterest);

  tr.setVerbose(bVerbose);
  bapVerifier.setVerbose(bVerbose);

  //now iterate through the traces
  while (tr.readNextInstruction() == 0)
  {
    counter++;
    //grab a reference to the current instruction
    const TRInstructionX86& insn = tr.getCurInstruction();
    if (bBAP)
    {
      cout << "************************************************" << endl;
      cout << "********    " << counter << "    (" << tr.getFilePos() << ")***************" << endl;
    }

    if ( (bVerbose) || (!bSummary && !bBAP) )
    {
      cout << "(" << counter << ") " << tr.getInsnString();
    }

    if (bSummary)
    {
      taintSummary.processInstruction(insn);
    
      if (!taintSummary.getResult().empty())
      {
      //cout << filepos << endl; break;
        cout << taintSummary.getResult();
        tsumcount++;
        //this should be okay for now since the xors at the end of each
        //iteration makes sure the registers are used and so the 
        // taints are known
        if (tsumcount > 10000)
        {
          break;
        }
      }
    }

    if (bBAP)
    {
      if (bapVerifier.isInterested(insn))
      {
        bapVerifier.processInstruction(insn);
        bapcount++;
        //only process 1000 instructions
        //remember this includes extra istructions such as XOR
        if (bapcount > 10000)
        {
          break;
        }
      }
      else
      {
        if (bVerbose)
        {
          cout << "Solve result: SKIPPED @ " << tr.getFilePos() << endl;
          cout << "---------------------------------------------------------" << endl;
        }
      }

      //filepos = tr.getFilePos();
    }

    if (counter >= 4)
    {
      //break;
    }
  }

  //clean up 
  printf("[%d] Instructions Decoded\n", counter);

out_error:

  return(0);
}

//#endif //end the test section
