/**
 *  @Author Lok Yan
 */

//Here is some sample code that uses the new piper to process a version 41 trace. Note that it require bap's toil program

//#define V41

#include <stdio.h>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/graph/graphviz.hpp>
using namespace std;
using namespace boost::program_options;

#include "TraceReaderBinX86.h"
#include "TraceProcessorX86Verify.h"
#include "TraceProcessorX86TaintSummary.h"

//hostfile is created by gcc -nostdlib -nostdinc -m32 host.s -o hostfile
// and then stripped to reduce the size using strip hostfile
// it is fairly easy to search for the 15 nops that I used to create host.s

#define BREAK_IF(x) \
    if(x)           \
    {               \
        break;      \
    }

class BapPrinter
{
public:
    BapPrinter(bool bVerbose) :
            m_BapVerifier(false, true)
    {
        m_bVerbose = bVerbose;
        m_cntBap = 0;
        m_BapVerifier.setVerbose(bVerbose);
    }

    int
    PrintBap(TraceReaderBinX86 &tr)
    {
        const TRInstructionX86& insn = tr.getCurInstruction();
        if (m_BapVerifier.isInterested(insn))
        {
            m_BapVerifier.processInstruction(insn);
            m_cntBap++;
            //only process 1000 instructions
            //remember this includes extra istructions such as XOR
            if (m_cntBap > 10000)
            {
                return -1;
                // break;
            }
        }
        else
        {
            if (m_bVerbose)
            {
                cout << "Solve result: SKIPPED @ " << tr.getFilePos() << endl;
                cout
                        << "---------------------------------------------------------"
                        << endl;
            }
        }
        //filepos = tr.getFilePos();
        return 0;
    }
    
private:
    size_t m_cntBap;
    TraceProcessorX86Verify m_BapVerifier;
    bool m_bVerbose;
};

class SummaryPrinter
{
public:
    SummaryPrinter() :
            m_TaintSummary(true)
    {
        size_t m_cntSum = 0;
    }

    int
    PrintSummary(const TRInstructionX86 &insn)
    {
        m_TaintSummary.processInstruction(insn);

        if (!m_TaintSummary.getResult().empty())
        {
            //cout << filepos << endl; break;
            cout << m_TaintSummary.getResult();
            m_cntSum++;
            //this should be okay for now since the xors at the end of each
            //iteration makes sure the registers are used and so the
            // taints are known
            if (m_cntSum > 10000)
            {
                return -1;
                // break;
            }
        }
        return 0;
    }

private:
    size_t m_cntSum;
    TraceProcessorX86TaintSummary m_TaintSummary;
};



bool g_bVerbose;
string g_sInFile;
// string g_sOutFile;
bool g_bBAP;
bool g_bSummary;

int
process_arg(int argc, char **argv);

int
main(int argc, char **argv)
{
    if (0 != process_arg(argc, argv))
    {
        return -1;
    }

    TraceReaderBinX86 tr;
    // TraceProcessorX86Verify bapVerifier(false, true);
    // TraceProcessorX86TaintSummary taintSummary(true);
    SummaryPrinter sum_printer;
    BapPrinter bap_printer(g_bVerbose);

    size_t counter = 0;
    // size_t bapcount = 0;
    // uint64_t filepos = 0;

    FILE* hostFile = NULL;

    //open the file
    tr.init(g_sInFile, ""); //defaults to cout
    //infact, we have to use COUT because of the use of system for the BAP test
    //tr.disableStringConversion(); //don't convert into strings yet

    //this is an approximate starting location of some tainted instructions
    // it should be 816330 instructions into the trace

    //tr.seekTo(startInterest);

    tr.setVerbose(g_bVerbose);
    // bapVerifier.setVerbose(g_bVerbose);

    //now iterate through the traces
    while (tr.readNextInstruction() == 0)
    {

        counter++;
        //grab a reference to the current instruction
        const TRInstructionX86& insn = tr.getCurInstruction();
        
        if (g_bBAP)
        {
            cout << "************************************************" << endl;
            cout << "********    " << counter << "    (" << tr.getFilePos()
                    << ")***************" << endl;
        }

        if ((g_bVerbose) /*|| (!bSummary && !bBAP) */)
        {
            cout << "(" << counter << ") " << tr.getInsnString();
        }

        if (g_bSummary)
        {
            BREAK_IF(0 != sum_printer.PrintSummary(insn));
        }

        if (g_bBAP)
        {
            BREAK_IF(0 != bap_printer.PrintBap(tr));
        }
    }

    //ofstream of("cfg.dot");
    //write_graphviz(of, cfa.m_CFG);

    printf("[%lu] Instructions Decoded\n", counter);

    return (0);
}
//#endif //end the test section

int
process_arg(int argc, char **argv)
{
    options_description opts("tracereader options");
    opts.add_options()
    ("help,h", "help message")
    ("input,i", value<string>(),"input filename")
    // ("output,o", value<string>(), "output filename")
    ("verbose,v", "print verbose messages")
    ("bap", "TODO: modify this")
    ("sum", "print summary")
    ;
    variables_map vm;
    store(parse_command_line(argc, argv, opts), vm);

    if (vm.size() == 0 || vm.count("help"))
    {
        cout << opts << endl;
        return -1;
    }

    if (!vm.count("input"))
    {
        cout << "Must specify --input" << endl;
        return -1;
    }
    g_sInFile = vm["input"].as<string>();

    // if(vm.count("output"))
    // {
    //     g_sOutFile = vm["output"].as<string>();
    // }

    g_bVerbose = (0 != vm.count("verbose"));
    g_bBAP = (0 != vm.count("bap"));
    g_bSummary = (0 != vm.count("sum"));

    return 0;
}
