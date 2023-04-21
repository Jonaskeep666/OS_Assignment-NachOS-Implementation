/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 **************************************************************/

// 23-0419[j]: MP1 注意
//             SysCall 介面 -> ksyscall.h -> filesys.h

#ifndef __USERPROG_KSYSCALL_H__ 
#define __USERPROG_KSYSCALL_H__ 

#include "kernel.h"
#include "synchconsole.h"


void SysHalt()
{
  kernel->interrupt->Halt();
}

void SysPrintInt(int val)
{ 
  DEBUG(dbgTraCode, "In ksyscall.h:SysPrintInt, into synchConsoleOut->PutInt, " << kernel->stats->totalTicks);
  kernel->synchConsoleOut->PutInt(val);
  DEBUG(dbgTraCode, "In ksyscall.h:SysPrintInt, return from synchConsoleOut->PutInt, " << kernel->stats->totalTicks);
}

int SysAdd(int op1, int op2)
{
  return op1 + op2;
}

int SysCreate(char *filename)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->fileSystem->Create(filename); // 22-1230[j]: 在 filesys.cc 中實作 
}
// 23-0103[j]: MP1 作業練習
// 23-0104[j]: 先瞭解規格 syscall.h 
// 23-0104[j]: 修改順序 exception.cc -> ksyscall.h -> filesys.h
// ------------------------------------------------------------------------

OpenFileId SysOpen(char *filename){
  return kernel->fileSystem->OpenF(filename);
}

int SysClose(OpenFileId fileId){
  return kernel->fileSystem->CloseF(fileId);
}

int SysWrite(OpenFileId fd,char *buffer, int nBytes){
  return kernel->fileSystem->WriteF(fd,buffer,nBytes);
}

int SysRead(OpenFileId fd,char *buffer, int nBytes){
  return kernel->fileSystem->ReadF(fd,buffer,nBytes);
}

// ------------------------------------------------------------------------

#endif /* ! __USERPROG_KSYSCALL_H__ */
