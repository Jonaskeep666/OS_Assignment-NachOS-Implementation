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

#ifdef FILESYS_STUB
int SysCreate(char *filename)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->fileSystem->Create(filename); // 22-1230[j]: 在 filesys.h 中實作 
}
#else // FILESYS

// 23-0508[j]: MP4 
// int SysCreate(char *filename, int size){
//   return (kernel->fileSystem->Create(filename,size) == TRUE)?1:0 ;
// }
int SysCreate(char *filename, int size){
  return (kernel->fileSystem->Create(filename,size,0) == TRUE)?1:0 ;
}

#endif // FILESYS

// 23-0103[j]: MP1 作業練習
// 23-0104[j]: 先瞭解規格 syscall.h 
// 23-0104[j]: 修改順序 exception.cc -> ksyscall.h -> filesys.h
// ------------------------------------------------------------------------

OpenFileId SysOpen(char *filename){
  return kernel->fileSystem->OpenReturnId(filename);
}

int SysClose(OpenFileId fileId){
  return kernel->fileSystem->Close(fileId);
}

int SysWrite(OpenFileId fd,char *buffer, int nBytes){
  return kernel->fileSystem->Write(fd,buffer,nBytes);
}

int SysRead(OpenFileId fd,char *buffer, int nBytes){
  return kernel->fileSystem->Read(fd,buffer,nBytes);
}

// ------------------------------------------------------------------------

#endif /* ! __USERPROG_KSYSCALL_H__ */
