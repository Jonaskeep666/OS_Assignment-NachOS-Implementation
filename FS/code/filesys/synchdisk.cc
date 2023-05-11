// synchdisk.cc 
//	Routines to synchronously access the disk.  The physical disk 
//	is an asynchronous device (disk requests return immediately, and
//	an interrupt happens later on).  This is a layer on top of
//	the disk providing a synchronous interface (requests wait until
//	the request completes).
//
//	Use a semaphore to synchronize the interrupt handlers with the
//	pending requests.  And, because the physical disk can only
//	handle one operation at a time, use a lock to enforce mutual
//	exclusion.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synchdisk.h"


//----------------------------------------------------------------------
// SynchDisk::SynchDisk
// 	Initialize the synchronous interface to the physical disk, in turn
//	initializing the physical disk.
//
//----------------------------------------------------------------------
/*
// 23-0502[j]: SynchDisk()
    -	new Disk(this);		// 建立 模擬硬碟，並設定 中斷發生要呼叫 這裡的 CallBack()
    -	new Semaphore(0);	// Semaphore = 0，wait()=Sleep() & signal()=WakeUp()
    -	new Lock;			// 作為 Mutex 使用 Acquire() & Release()
*/
SynchDisk::SynchDisk()
{
    semaphore = new Semaphore("synch disk", 0);
    lock = new Lock("synch disk lock");
    disk = new Disk(this);
}

//----------------------------------------------------------------------
// SynchDisk::~SynchDisk
// 	De-allocate data structures needed for the synchronous disk
//	abstraction.
//----------------------------------------------------------------------

SynchDisk::~SynchDisk()
{
    delete disk;
    delete lock;
    delete semaphore;
}

//----------------------------------------------------------------------
// SynchDisk::ReadSector
// 	Read the contents of a disk sector into a buffer.  Return only
//	after the data has been read.
//
//	"sectorNumber" -- the disk sector to read
//	"data" -- the buffer to hold the contents of the disk sector
//----------------------------------------------------------------------
/*
// 23-0502[j]: ReadSector(..)/WriteSector(..)
-   主要功能：送出 I/O Request 給 Disk，並呼叫 Disk::ReadRequest/WriteRequest 來「實際存取」
    1.	送出 I/O Request 前，先「請求鑰匙/上鎖 = lock->Acquire()」
    2.	送出 I/O Request = 呼叫 disk->ReadRequest(..)
    3.	等待 Disk 工作完成 = semaphore->P() = Wait()
        若 Disk 完成 會呼叫 ISR = CallBack() = semaphore->V() = Signal()
        並喚醒 Thread 執行到一半的 ReadSector(..)/WriteSector(..)
    4.	完成 I/O Request 後，則「釋放鑰匙/解鎖 = lock->Release()」
*/
void
SynchDisk::ReadSector(int sectorNumber, char* data)
{
    lock->Acquire();			// only one disk I/O at a time
    disk->ReadRequest(sectorNumber, data);
    semaphore->P();			// wait for interrupt
    lock->Release();
}

//----------------------------------------------------------------------
// SynchDisk::WriteSector
// 	Write the contents of a buffer into a disk sector.  Return only
//	after the data has been written.
//
//	"sectorNumber" -- the disk sector to be written
//	"data" -- the new contents of the disk sector
//----------------------------------------------------------------------

void
SynchDisk::WriteSector(int sectorNumber, char* data)
{
    lock->Acquire();			// only one disk I/O at a time
    disk->WriteRequest(sectorNumber, data);
    semaphore->P();			// wait for interrupt
    lock->Release();
}

//----------------------------------------------------------------------
// SynchDisk::CallBack
// 	Disk interrupt handler.  Wake up any thread waiting for the disk
//	request to finish.
//----------------------------------------------------------------------

void
SynchDisk::CallBack()
{ 
    semaphore->V();
}
