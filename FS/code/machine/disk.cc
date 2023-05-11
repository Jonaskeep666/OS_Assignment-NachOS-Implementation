// disk.cc 
//	Routines to simulate a physical disk device; reading and writing
//	to the disk is simulated as reading and writing to a UNIX file.
//	See disk.h for details about the behavior of disks (and
//	therefore about the behavior of this simulation).
//
//	Disk operations are asynchronous, so we have to invoke an interrupt
//	handler when the simulated operation completes.
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "disk.h"
#include "debug.h"
#include "sysdep.h"
#include "main.h"

// We put a magic number at the front of the UNIX file representing the
// disk, to make it less likely we will accidentally treat a useful file 
// as a disk (which would probably trash the file's contents).

const int MagicNumber = 0x456789ab;
const int MagicSize = sizeof(int);
const int DiskSize = (MagicSize + (NumSectors * SectorSize));


//----------------------------------------------------------------------
// Disk::Disk()
// 	Initialize a simulated disk.  Open the UNIX file (creating it
//	if it doesn't exist), and check the magic number to make sure it's 
// 	ok to treat it as Nachos disk storage.
//
//	"toCall" -- object to call when disk read/write request completes
//----------------------------------------------------------------------
/*
// 23-0428[j]: Disk(..)
-	主要功能：建立模擬Disk(一個 Host File)，當 I/O Request 完成時呼叫 toCall->CallBack()
    -	DiskName = Disk_%d，其中 %d = kernel->hostName
    -	若名為 DiskName 存在：
        檢查 檔案最開頭 的 Magic Number 是否正確，若不正確 -> 印出錯誤訊息
        ( 呼叫 OpenForReadWrite(..) 即調用 POSIX API open(..)，若檔案存在則進行讀寫操作)
    -	若名為 DiskName 不存在：
        -	建立一個 名為 Disk_%d 的檔案(Host File) 
			會存在 NachOS 執行檔 的同一個資料夾下 (In Host)
            ( 呼叫 OpenForWrite(..) 即調用 POSIX API open(..)，若檔案存在則進行寫入操作，否則建立新檔案)
        -	開頭寫入 Magic Number
        -	結尾寫入 tmp = 0，讓之後 read(..) 不會回傳 EOF
        
    -	初始化 active = FALSE
-	參數：
    *toCall：指標 指向「引發中斷」的 模擬硬體物件 
    -> 當 I/O 完成時呼叫 toCall->CallBack() 等同呼叫 模擬硬體物件的 CallBack()

*/
Disk::Disk(CallBackObj *toCall)
{
    int magicNum;
    int tmp = 0;

    DEBUG(dbgDisk, "Initializing the disk.");
    callWhenDone = toCall;
    lastSector = 0;
    bufferInit = 0;
    
    sprintf(diskname,"DISK_%d",kernel->hostName);

    fileno = OpenForReadWrite(diskname, FALSE);
    if (fileno >= 0) {		// file exists, check magic number 
        Read(fileno, (char *) &magicNum, MagicSize);
        ASSERT(magicNum == MagicNumber);
    } else {				// file doesn't exist, create it
        fileno = OpenForWrite(diskname);
	    magicNum = MagicNumber;  
	    WriteFile(fileno, (char *) &magicNum, MagicSize); // write magic number

	    // need to write at end of file, so that reads will not return EOF
        // 23-0428[j]: Current 從擋頭 往下移 DiskSize - MagicSize 的距離
        //             將 &tmp 指向的 4 Bytes(都是0) 寫入 檔案中
        Lseek(fileno, DiskSize - sizeof(int), 0);   
	    WriteFile(fileno, (char *)&tmp, sizeof(int));  
    }
    active = FALSE;
}

//----------------------------------------------------------------------
// Disk::~Disk()
// 	Clean up disk simulation, by closing the UNIX file representing the
//	disk.
//----------------------------------------------------------------------

Disk::~Disk()
{
    Close(fileno);
}

//----------------------------------------------------------------------
// Disk::PrintSector()
// 	Dump the data in a disk read/write request, for debugging.
//----------------------------------------------------------------------
// 23-0502[j]: PrintSector(..)
//             主要功能：將 data 位置上的資料 = sector (in Disk) 印出 (For 測試用)
static void
PrintSector (bool writing, int sector, char *data)
{
    int *p = (int *) data;

    if (writing)
        cout << "Writing sector: " << sector << "\n"; 
    else
        cout << "Reading sector: " << sector << "\n"; 
    for (unsigned int i = 0; i < (SectorSize/sizeof(int)); i++) {
	    cout << p[i] << " ";
    }
    cout << "\n"; 
}

//----------------------------------------------------------------------
// Disk::ReadRequest/WriteRequest
// 	Simulate a request to read/write a single disk sector
//	   Do the read/write immediately to the UNIX file
//	   Set up an interrupt handler to be called later,
//	      that will notify the caller when the simulator says
//	      the operation has completed.
//
//	Note that a disk only allows an entire sector to be read/written,
//	not part of a sector.
//
//	"sectorNumber" -- the disk sector to read/write
//	"data" -- the bytes to be written, the buffer to hold the incoming bytes
//----------------------------------------------------------------------
// 23-0502[j]: ReadRequest/WriteRequest(..)
//             實際處理 一個 R/W I/O Request(開啟 DiskFile 讀寫 Data)，並安排一個模擬中斷
//             (比較 SynchDisk::ReadSector() 是「送出」一個 I/O Request 給 Disk，並請 Disk 處理)

void
Disk::ReadRequest(int sectorNumber, char* data)
{
    // 23-0501[j]: 計算從 lastSector 到 newSector 所花費的時間
    int ticks = ComputeLatency(sectorNumber, FALSE);

    // 23-0501[j]: (1) 檢查「Disk 在忙嗎？」-> Disk 一次僅處理一個請求
    //             (2) 檢查「Sector # 有出界嗎？」
    ASSERT(!active); 			// only one request at a time
    ASSERT((sectorNumber >= 0) && (sectorNumber < NumSectors));
    
    DEBUG(dbgDisk, "Reading from sector " << sectorNumber);
    // 23-0501[j]: 直接呼叫 Linux POSIX API
    //             Lseek(fd,offset,Seek位置)：將 Current Pointer 從「Seek位置」移動 offset 距離
    //             -   SEEK_SET = 0 (檔案 頭)
    //             -   SEEK_CUR = 1 (目前位置)
    //             -   SEEK_END = 2 (檔案 尾)
    Lseek(fileno, SectorSize * sectorNumber + MagicSize, 0);
    Read(fileno, data, SectorSize);
    if (debug->IsEnabled('d'))
	    PrintSector(FALSE, sectorNumber, data); // 23-0501[j]: 若開 Debug 模式，印出 Sector # Data
    
    // 23-0501[j]: 開始讀取，設定 Disk 忙碌中 (active = TRUE)
    //             並更新 lastSector = 本次 sectorNumber
    active = TRUE;
    UpdateLast(sectorNumber);
    kernel->stats->numDiskReads++;  // 23-0501[j]: 統計一下 Disk 讀取的資料數
    // 23-0501[j]: 安排一個「模擬中斷」在過了「ticks 時刻之後引發」(類型是 DiskInt)
    kernel->interrupt->Schedule(this, ticks, DiskInt);
}

void
Disk::WriteRequest(int sectorNumber, char* data)
{
    int ticks = ComputeLatency(sectorNumber, TRUE);

    ASSERT(!active);
    ASSERT((sectorNumber >= 0) && (sectorNumber < NumSectors));
    
    DEBUG(dbgDisk, "Writing to sector " << sectorNumber);
    Lseek(fileno, SectorSize * sectorNumber + MagicSize, 0);
    WriteFile(fileno, data, SectorSize);
    if (debug->IsEnabled('d'))
	PrintSector(TRUE, sectorNumber, data);
    
    active = TRUE;
    UpdateLast(sectorNumber);
    kernel->stats->numDiskWrites++;
    kernel->interrupt->Schedule(this, ticks, DiskInt);
}

//----------------------------------------------------------------------
// Disk::CallBack()
// 	Called by the machine simulation when the disk interrupt occurs.
//----------------------------------------------------------------------

void
Disk::CallBack ()
{ 
    active = FALSE;
    callWhenDone->CallBack();
}

//----------------------------------------------------------------------
// Disk::TimeToSeek()
//	Returns how long it will take to position the disk head over the correct
//	track on the disk.  Since when we finish seeking, we are likely
//	to be in the middle of a sector that is rotating past the head,
//	we also return how long until the head is at the next sector boundary.
//	
//   	Disk seeks at one track per SeekTime ticks (cf. stats.h)
//   	and rotates at one sector per RotationTime ticks
//----------------------------------------------------------------------
/*
// 23-0428[j]: TimeToSeek(..)
-   主要功能
    -	return Disk Head 移動到 newSector 花費的時間(Tick數)
    -	若移動到 new Track，但 Head 讀取 Sector 到一半，需要等待 (*rotation) 的時間「等它讀完」
        -> 如此才能開始讀下一個 Sector -> return (*rotation)
*/
int
Disk::TimeToSeek(int newSector, int *rotation) 
{
    // 23-0428[j]: Track # = Sector# / 一個 Track 的 Sector 數
    int newTrack = newSector / SectorsPerTrack;
    int oldTrack = lastSector / SectorsPerTrack;

    // 23-0428[j]: 在 stat.h 定義
	//             RotationTime = 500 (Disk 轉動 1 Sector 的時間)
	//             SeekTime = 500 (Disk Head 移動 1 Track 的時間)

    // 23-0428[j]: 計算 Seek Time (讀寫頭移動的時間)
    int seek = abs(newTrack - oldTrack) * SeekTime;
				// how long will seek take?

    // 23-0428[j]: 判斷 Head 是否讀取 Sector 到一半？
    //             (總Tick數) % RotationTime = 0 (剛好讀完一個 Sector)
    //             (總Tick數) % RotationTime ≠ 0 (Sector 讀到一半) 
    //                  -> 還需要轉動 (*rotation) Ticks 的時間，才能讀完目前 Sector
    int over = (kernel->stats->totalTicks + seek) % RotationTime; 
				// will we be in the middle of a sector when
				// we finish the seek?

    *rotation = 0;
    if (over > 0)	 	// if so, need to round up to next full sector
   	    *rotation = RotationTime - over;
    return seek;
}

//----------------------------------------------------------------------
// Disk::ModuloDiff()
// 	Return number of sectors of rotational delay between target sector
//	"to" and current sector position "from"
//----------------------------------------------------------------------

int 
Disk::ModuloDiff(int to, int from)
{
    int toOffset = to % SectorsPerTrack;
    int fromOffset = from % SectorsPerTrack;

    return ((toOffset - fromOffset) + SectorsPerTrack) % SectorsPerTrack;
}

//----------------------------------------------------------------------
// Disk::ComputeLatency()
// 	Return how long will it take to read/write a disk sector, from
//	the current position of the disk head.
//
//   	Latency = seek time + rotational latency + transfer time
//   	Disk seeks at one track per SeekTime ticks (cf. stats.h)
//   	and rotates at one sector per RotationTime ticks
//
//   	To find the rotational latency, we first must figure out where the 
//   	disk head will be after the seek (if any).  We then figure out
//   	how long it will take to rotate completely past newSector after 
//	that point.
//
//   	The disk also has a "track buffer"; the disk continuously reads
//   	the contents of the current disk track into the buffer.  This allows 
//   	read requests to the current track to be satisfied more quickly.
//   	The contents of the track buffer are discarded after every seek to 
//   	a new track.
//----------------------------------------------------------------------
/*
// 23-0428[j]: ComputeLatency(..)
主要功能：計算 R/W 要花費的時間(以安排 模擬中斷 發生的時間)
-	若有採用 Track Buffer，則檢查「是否已暫存 目前 Track 所含的所有 Sectors」
    -	若「(目前時間 - TB開始紀錄時間) > (TB開始紀錄Sector 走到 newSector 的時間)」
        -> 表示 TB 已經紀錄「超過一圈」
    -	若是 Read 操作，直接從 TB 取值即可 -> 花費 1 RotationTime 的時間
        -> Return Latency = 1 RotationTime

-	其餘狀況：Latency = Seek Time + Rotation Latency + transfer time
    (1)	Seek Time = seek = TimeToSeek(..) 見下方說明(lastSector 到 new Track 的時間差)

    (2)	Rotation Latency = rotation = Part 1 + Part 2

        -	Part 1：從 old Sector 讀到一半 ～ 讀完 Old Sector (可以開始讀下個 Sector 的時間)
            ( "[---v---],[------]" ～ "[------],v[------]" 的時間差 )
        -	Part 2：目前位置 轉到 newSector 所花的時間

    (3)	Transfer time = 1 RotationTime = 從 new Sector 開頭 讀到 尾端 的時間差
    (4)	Return Latency = Seek Time + Rotation Latency + transfer time
*/
int
Disk::ComputeLatency(int newSector, bool writing)
{
    int rotation;
    int seek = TimeToSeek(newSector, &rotation);

    // 23-0428[j]: timeAfter 表示 Disk Head「定位到 new Track 並可以開始讀下個 Sector」的時間點
    //             seek = Head 從 lastSector 移動到 newSector 所在 Track
    //             rotation = 讀完目前所在 Sector 的時間花費
    int timeAfter = kernel->stats->totalTicks + seek + rotation;

#ifndef NOTRACKBUF	// turn this on if you don't want the track buffer stuff
    // check if track buffer applies

    /*
    // 23-0428[j]: 若 seek = 0 (lastSector & newSector 在同一條 Track 上)
    -   [X] = (timeAfter - bufferInit) / RotationTime
            =   (lastSector 定位到 newTrack 的時間點 - TB 開始存 Track 的時間) / RotationTime
            =   TB 開始存 Track 的時間 到現在，最少儲存的 Sector 數目 = [X] 個 Sectors
    -   [Y] = ModuloDiff(newSector, bufferInit / RotationTime))
            =   從 TB 開始紀錄的 Sector 轉到 newSector 要掃過 [Y]個 Sector
    -   若 [X] > [Y] 表示 TB 已經紀錄同一個 Track 超過一圈 = TB 已經記完 該Track 所有資料
        -> 故直接花費 1 RotationTime 的時間，將 newSector 的資料從 RAM 中取出即可
        (若 [X] = [Y] 表示 TB 還沒有 記完 該Track 所有資料)
    */
    if ((writing == FALSE) && (seek == 0) 
		&& (((timeAfter - bufferInit) / RotationTime) 
	     		> ModuloDiff(newSector, bufferInit / RotationTime))) {
        DEBUG(dbgDisk, "Request latency = " << RotationTime);
	    return RotationTime; // time to transfer sector from the track buffer
    }
#endif

    // 23-0428[j]: 目前位置 轉到 newSector 所花的時間
    rotation += ModuloDiff(newSector, timeAfter / RotationTime) * RotationTime;

    DEBUG(dbgDisk, "Request latency = " << (seek + rotation + RotationTime));
    // 23-0428[j]: 回傳 Latency = Seek Time + Rotation Latency + transfer time
    return(seek + rotation + RotationTime);
}

//----------------------------------------------------------------------
// Disk::UpdateLast
//   	Keep track of the most recently requested sector.  So we can know
//	what is in the track buffer.
//----------------------------------------------------------------------
/*
// 23-0428[j]: UpdateLast(..)
-   主要功能
    -	更新 lastSector 為 newSector
    -	更新 bufferInit (TB 開始存 newTrack 的時刻) = Disk Head 移動到 new Track 的時間點
        若 現在 Head 不在 new Track(newSector 所在 Track) 上，更新 bufferInit
*/
void
Disk::UpdateLast(int newSector)
{
    int rotate;
    int seek = TimeToSeek(newSector, &rotate);
    
    // 23-0428[j]: 若 現在 Head 不在 new Track(newSector 所在 Track) 上
    //             (seek != 0 代表 Current Head 不在 new Track 上)
    //             則更新 bufferInit 為 Disk Head 移動到 new Track 的時間點
    if (seek != 0)
	    bufferInit = kernel->stats->totalTicks + seek + rotate;

    lastSector = newSector;
    DEBUG(dbgDisk, "Updating last sector = " << lastSector << " , " << bufferInit);
}
