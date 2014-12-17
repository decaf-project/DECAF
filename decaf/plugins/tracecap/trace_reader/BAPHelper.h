/**
 * Helper for interfacing and dealing with BAP
 * @Author Lok Yan
 * @date 22 APR 2013
**/

#ifndef BAP_HELPER_H
#define BAP_HELPER_H

#include <string>
#include <stdio.h>
#include "TRInstructionX86.h"
#include "ChildPiper.h"

class BAPHelper
{
public:
  
  BAPHelper(const std::string& toilPath, const std::string& hostFilePath, long int offsetToTextSection, long int offsetToTextSectionAddr, const std::string& tempFile);

  int init();
  int cleanup();

  ~BAPHelper();

  int toBAPIL(std::string& str, const TRInstructionX86& insn);

  //some static functions
  static int concretizeEFlags(std::string& str, uint32_t EFLAGS, bool verbose = false);
  static int setupOperand(std::string& str, const OperandVal& op, char c, const std::string& tail = "");
  static int setupOperand(std::string& str, const EntryHeader& eh, unsigned int offset, char c);
  static int concretizeOperand(std::string& str, const OperandVal& op, bool bBitTaint = false, bool bIgnoreTaint = false, const std::string& tail = "");
  //HACK HACK to ignore the taint in pointers
  static int concretizeOperand(std::string& str, const EntryHeader& eh, unsigned int offset, bool bBitTaint = false);
  static int concretizeTaint(std::string& str, const OperandVal& op, bool bBitTaint = false);
  static int concretizeLiteral(std::string& str, uint64_t val, size_t byteLen);
  static int operandToString(std::string& str, const OperandVal& op);
  static uint16_t operandSize(const OperandVal& op);
  static inline int concretizeValue(std::string& str, const OperandVal& op);
  static inline int concretizeAddr(std::string& str, const OperandVal& op);
  static inline int concretizeU8(std::string& str, uint8_t val);
  static inline int concretizeU16(std::string& str, uint16_t val);
  static inline int concretizeU32(std::string& str, uint32_t val);
  static inline int concretizeU64(std::string& str, uint64_t val);

  static int concretizeOperand(std::string& str, const OperandValV41& op);
 
    
private:
  
  ChildPiper prCP;
  std::string prHostFilePath;
  std::string prToilPath;
  std::string prTempFilePath;
  std::string prLastIL;

  FILE* prHostFile;
  FILE* prTempFile;

  long int prInsnLoc;
  long int prInsnAddrLoc; 

  bool prbInit;
};

#endif//BAP_HELPER_H
