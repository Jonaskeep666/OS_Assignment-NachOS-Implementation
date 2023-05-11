// disk.h 
//	Data structures to emulate a physical disk.  A physical disk
//	can accept (one at a time) requests to read/write a disk sector;
//	when the request is satisfied, the CPU gets an interrupt, and 
//	the next request can be sent to the disk.
//
//	Disk contents are preserved across machine crashes, but if
//	a file system operation (eg, create a file) is in progress when the 
//	system shuts down, the file system may be corrupted.
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef DISK_H
#define DISK_H

#include "copyright.h"
#include "utility.h"
#include "callback.h"

// The following class defines a physical disk I/O device.  The disk
// has a single surface, split up into "tracks", and each track split
// up into "sectors" (the same number of sectors on each track, and each
// sector has the same number of bytes of storage).  
//
// Addressing is by sector number -- each sector on the disk is given
// a unique number: track * SectorsPerTrack + offset within a track.
//
// As with other I/O devices, the raw physical disk is an asynchronous device --
// requests to read or write portions of the disk return immediately,
// and an interrupt is invoked later to signal that the operation completed.
//
// The physical disk is in fact simulated via operations on a UNIX file.
//
// To make life a little more realistic, the simulated time for
// each operation reflects a "track buffer" -- RAM to store the contents
// of the current track as the disk head passes by.  The idea is that the
// disk always transfers to the track buffer, in case that data is requested
// later on.  This has the benefit of eliminating the need for 
// "skip-sector" scheduling -- a read request which comes in shortly after 
// the head has passed the beginning of the sector can be satisfied more 
// quickly, because its contents are in the track buffer.  Most 
// disks these days now come with a track buffer.
//
// The track buffer simulation can be disabled by compiling with -DNOTRACKBUF

const int SectorSize = 128;		// number of bytes per disk sector
const int SectorsPerTrack  = 32;	// number of sectors per disk track 
// const int NumTracks = 32;		// number of tracks per disk

// 23-0508[j]: MP4 將 Disk 擴增到 64 MB
const int NumTracks = 16384;		// number of tracks per disk
const int NumSectors = (SectorsPerTrack * NumTracks);
					// total # of sectors per disk
/*
// 23-0502[j]: class Disk (繼承於 CallBackObj 自然繼承 CallBack() 方法 )
	-	主要功能：
		-	模擬 Physical Disk，提供 Disk 最底層的功能(上層 NachOS API(synchdisk) 會呼叫以下方法)
			-	模擬 Physical Disk 一次只能處理一個 Access Request
				當 Disk 收到請求，會立即 return，等到操作完成後再發送 Interrupt 通知 CPU
				此時 CPU 才可送出下個 Access Request
			-	操作「模擬 Physical Disk」= 操作一個「UNIX File」
      
		-	Track Buffer (in RAM)
			-	Track Buffer 會暫存「Disk Head 掃過的同一條 Track 的資料」(in RAM)
				若有 I/O Request 可以直接從此處取值 -> 速度快
				（ 在設計 Disk Scheduling 時，可以不必設計「Skip-Sector」的排班方式」）
			-	Track Buffer 僅儲存一個 Track 的資料
				（ 在 Disk Head 開始讀取 下一個 Track 時，會先將自己清空 ）
			-	開關：可在 build.linux/Makefile 中，可決定是否採用 Track Buffer，宣告下可「關閉」此功能
				> DEFINES =  -DNOTRACKBUF 
*/
class Disk : public CallBackObj {
  public:
    Disk(CallBackObj *toCall);          // Create a simulated disk.  
					// Invoke toCall->CallBack() 
					// when each request completes.
    ~Disk();				// Deallocate the disk.
    
    void ReadRequest(int sectorNumber, char* data);
    					// Read/write an single disk sector.
					// These routines send a request to 
    					// the disk and return immediately.
    					// Only one request allowed at a time!
    void WriteRequest(int sectorNumber, char* data);

    void CallBack();			// Invoked when disk request 
					// finishes. In turn calls, callWhenDone.

    int ComputeLatency(int newSector, bool writing);	
    					// Return how long a request to 
					// newSector will take: 
					// (seek + rotational delay + transfer)

  private:
    int fileno;				      // UNIX file number for simulated disk 
    char diskname[32];			// name of simulated disk's file
    CallBackObj *callWhenDone;		// Invoke when any disk request finishes
    bool active;     			  // Is a disk operation in progress?
    int lastSector;			    // The previous disk request 

    // 23-0428[j]: Track Buffer 開始暫存「目前 Track 資料」的時刻
    int bufferInit;			// When the track buffer started 
					// being loaded

    int TimeToSeek(int newSector, int *rotate); // time to get to the new track
    int ModuloDiff(int to, int from);        // # sectors between to and from
    void UpdateLast(int newSector);
};

#endif // DISK_H
