// openfile.cc 
//	Routines to manage an open Nachos file.  As in UNIX, a
//	file must be open before we can read or write to it.
//	Once we're all done, we can close it (in Nachos, by deleting
//	the OpenFile data structure).
//
//	Also as in UNIX, for convenience, we keep the file header in
//	memory while the file is open.

// 23-0419[j]: MP4 使用

#ifndef FILESYS_STUB

#include "copyright.h"
#include "main.h"
#include "filehdr.h"
#include "openfile.h"
#include "synchdisk.h"

//----------------------------------------------------------------------
// OpenFile::OpenFile
// 	Open a Nachos file for reading and writing.  Bring the file header
//	into memory while the file is open.
//
//	"sector" -- the location on disk of the file header for this file
//----------------------------------------------------------------------
// 23-0505[j]: 從 Disk 的 sector 中，將 FileHeader Load 到 Memory (由 hdr 來指)，並設定 seekPosition = 0
OpenFile::OpenFile(int sector)
{ 
    hdr = new FileHeader;
    hdr->FetchFrom(sector);
    seekPosition = 0;
}

//----------------------------------------------------------------------
// OpenFile::~OpenFile
// 	Close a Nachos file, de-allocating any in-memory data structures.
//----------------------------------------------------------------------

OpenFile::~OpenFile()
{
    delete hdr;
}

//----------------------------------------------------------------------
// OpenFile::Seek
// 	Change the current location within the open file -- the point at
//	which the next Read or Write will start from.
//
//	"position" -- the location within the file for the next Read/Write
//----------------------------------------------------------------------

void
OpenFile::Seek(int position)
{
    seekPosition = position;
}	

//----------------------------------------------------------------------
// OpenFile::Read/Write
// 	Read/write a portion of a file, starting from seekPosition.
//	Return the number of bytes actually written or read, and as a
//	side effect, increment the current position within the file.
//
//	Implemented using the more primitive ReadAt/WriteAt.
//
//	"into" -- the buffer to contain the data to be read from disk 
//	"from" -- the buffer containing the data to be written to disk 
//	"numBytes" -- the number of bytes to transfer
//----------------------------------------------------------------------
/*
// 23-0504[j]: 需先看 ReadAt(..)/WriteAt(..)，再看此處 Read(..)/Write(..)
				-	主要功能：從 seekPosition 開始 R/W File，並 return R/W 的 Bytes 數目
					-	呼叫 ReadAt(..)/WriteAt(..)，從 seekPosition 開始 R/W File
						其中 ReadAt(..)/WriteAt(..)「不會改變」seekPosition 的位置
					-	(Side Effect) seekPosition += result;
					-	return 「成功」存取的 Bytes 數目(result)
						(若失敗，則 return 0)

				-	參數
					-	into：一個指標 指向暫存「讀出資料的 Buffer」
					-	from：一個指標 指向暫存「寫入資料的 Buffer」
					-	numBytes：欲 R/W 的 Bytes 數目
*/
int
OpenFile::Read(char *into, int numBytes)
{
   int result = ReadAt(into, numBytes, seekPosition);
   seekPosition += result;
   return result;
}

int
OpenFile::Write(char *from, int numBytes)
{
   int result = WriteAt(from, numBytes, seekPosition);
   seekPosition += result;
   return result;
}

//----------------------------------------------------------------------
// OpenFile::ReadAt/WriteAt
// 	Read/write a portion of a file, starting at "position".
//	Return the number of bytes actually written or read, but has
//	no side effects (except that Write modifies the file, of course).
//
//	There is no guarantee the request starts or ends on an even disk sector
//	boundary; however the disk only knows how to read/write a whole disk
//	sector at a time.  Thus:
//
//	For ReadAt:
//	   We read in all of the full or partial sectors that are part of the
//	   request, but we only copy the part we are interested in.
//	For WriteAt:
//	   We must first read in any sectors that will be partially written,
//	   so that we don't overwrite the unmodified portion.  We then copy
//	   in the data that will be modified, and write back all the full
//	   or partial sectors that are part of the request.
//
//	"into" -- the buffer to contain the data to be read from disk 
//	"from" -- the buffer containing the data to be written to disk 
//	"numBytes" -- the number of bytes to transfer
//	"position" -- the offset within the file of the first byte to be
//			read/written
//----------------------------------------------------------------------
/*
// 23-0504[j]: ReadAt(..)
    -	背景：因為 Disk 只能存取「整個 Sector」，故在此會 區分/存取「需要的部分」
    -	主要功能：從 postition 處 開始讀取 numBytes 的資料，並存入 into 指向的空間；最後回傳 numBytes
    -   參數
        -	into：一個指標 指向暫存「讀出資料的 存放的空間」
        -	numBytes：欲 R/W 的 Bytes 數目
        -	position：欲從 postition 處 開始存取
*/
int
OpenFile::ReadAt(char *into, int numBytes, int position)
{
    int fileLength = hdr->FileLength();
    int i, firstSector, lastSector, numSectors;
    char *buf;

    // 23-0504[j]: 若參數不合法，則 return 0
    if ((numBytes <= 0) || (position >= fileLength))
    	return 0; 				// check request

    // 23-0504[j]: 若欲讀 Bytes 會超過 fileLength，則僅讀「剩下的部分」 
    if ((position + numBytes) > fileLength)		
	    numBytes = fileLength - position;

    DEBUG(dbgFile, "Reading " << numBytes << " bytes at " << position << " from file of length " << fileLength);

    // 23-0504[j]: 確定 要存取的「1st Sector & last Sector & Sector 數目」
    firstSector = divRoundDown(position, SectorSize);
    lastSector = divRoundDown(position + numBytes - 1, SectorSize);
    numSectors = 1 + lastSector - firstSector;

    // read in all the full and partial sectors that we need

    /*
    // 23-0504[j]: 讀出資料
    -   動態配置一個 Buffer 來暫存 讀入的資料 char buf[Sector數*SectorSize]
    -   讀取 sectorNumber 的資料：synchDisk->ReadSector(sectorNumber,*data)
    -   查詢 File Header 找出 第 i * SectorSize 個 Bytes 在哪個 Sector #
        -   sectorNumber = hdr->ByteToSector(i * SectorSize) 
    -   將讀出的 Sector 存入 buf[ (i - firstSector) * SectorSize ]
    */
    buf = new char[numSectors * SectorSize];
    for (i = firstSector; i <= lastSector; i++)	
        kernel->synchDisk->ReadSector(hdr->ByteToSector(i * SectorSize), 
					&buf[(i - firstSector) * SectorSize]);

    // copy the part we want
    // 23-0504[j]: 將 position 處開始往後 numBytes 的資料，複製到 into指向空間 中
    bcopy(&buf[position - (firstSector * SectorSize)], into, numBytes);
    delete [] buf;
    return numBytes;
}

/*
// 23-0504[j]: WriteAt(..)
    -	背景：因為 Disk 只能存取「整個 Sector」，故在此會 區分/存取「需要的部分」
    -	主要功能：將「已修改資料 = from指向空間」+「部分未修改的原資料」存入 buf 後，再整個 WriteBack
                最後回傳 numBytes
        1.	檢查 參數是否合法，若不合法 且 無法修正，則 return 0
        2.	計算 要存取的「1st Sector & last Sector & Sector 數目」
        3.	工具配置
            -	動態配置一個 Buffer 來暫存資料 char buf[Sector數*SectorSize]
            -   檢查 position 是否「剛好」位於 1st Sector 的開頭
                -> 若是，firstAligned = TRUE
            -   檢查 position + numBytes 是否「剛好」位於 last Sector 的末端
                -> 若是，lastAligned = TRUE

        4.	將「已修改的資料 (from 指向空間)」覆蓋到 buf 中
            -   若 firstAligned、lastAligned = FALSE (沒對齊)
                則先將 1st Sector & last Sector 的資料讀進 buf 中，以便之後覆蓋
            -	從 from 指向位置 往後 numBytes 的資料，複製到 buf(position以後的空間) 中
                ```C++
                bcopy(from, &buf[position - (firstSector * SectorSize)], numBytes);
                ```
        5.	將 暫存「修改資料」的 buf (逐個 Sector) 寫回 Disk
            -   ```C++
                for (i = firstSector; i <= lastSector; i++)	
                kernel->synchDisk->WriteSector(hdr->ByteToSector(i * SectorSize), 
                        &buf[(i - firstSector) * SectorSize]);
                ```
        6.	return numBytes;
        
    -   參數
        -	from：一個指標 指向暫存「寫入資料的 存放的空間」
        -	numBytes：欲 R/W 的 Bytes 數目
        -	position：欲從 postition 處 開始存取
*/

int
OpenFile::WriteAt(char *from, int numBytes, int position)
{
    int fileLength = hdr->FileLength();
    int i, firstSector, lastSector, numSectors;
    bool firstAligned, lastAligned;
    char *buf;

    if ((numBytes <= 0) || (position >= fileLength))
	    return 0;				// check request
    if ((position + numBytes) > fileLength)
	    numBytes = fileLength - position;

    DEBUG(dbgFile, "Writing " << numBytes << " bytes at " << position << " from file of length " << fileLength);

    firstSector = divRoundDown(position, SectorSize);
    lastSector = divRoundDown(position + numBytes - 1, SectorSize);
    numSectors = 1 + lastSector - firstSector;

    /*
    // 23-0504[j]: 工具配置
                -   動態配置一個 Buffer 來暫存資料 char buf[Sector數*SectorSize]
                -   檢查 position 是否「剛好」位於 1st Sector 的開頭
                    -> 若是，firstAligned = TRUE
                -   檢查 position + numBytes 是否「剛好」位於 last Sector 的末端
                    -> 若是，lastAligned = TRUE
    */
    buf = new char[numSectors * SectorSize];

    firstAligned = (position == (firstSector * SectorSize));
    lastAligned = ((position + numBytes) == ((lastSector + 1) * SectorSize));

    /*
    // 23-0504[j]: 將「已修改的資料 (from 指向空間)」覆蓋到 buf 中
    -   若 firstAligned、lastAligned = FALSE (沒對齊)
        則先將 1st Sector & last Sector 的資料讀進 buf 中，以便之後覆蓋
    -   從 from 指向位置 往後 numBytes 的資料，複製到 buf(position以後的空間) 中
    */

// read in first and last sector, if they are to be partially modified
    if (!firstAligned)
        ReadAt(buf, SectorSize, firstSector * SectorSize);	
    if (!lastAligned && ((firstSector != lastSector) || firstAligned))
        ReadAt(&buf[(lastSector - firstSector) * SectorSize], 
				SectorSize, lastSector * SectorSize);	

// copy in the bytes we want to change 
    bcopy(from, &buf[position - (firstSector * SectorSize)], numBytes);

// write modified sectors back

    /*
    // 23-0504[j]: 將 暫存「修改資料」的 buf 整個寫回 Disk
    -   將資料寫入 sectorNumber：synchDisk->WriteSector(sectorNumber,*data)
    -   查詢 File Header 找出 第 i * SectorSize 個 Bytes 在哪個 Sector #
        -   sectorNumber = hdr->ByteToSector(i * SectorSize) 
    -   將 &buf[ (i - firstSector) * SectorSize ] 上的資料(1 Sector) 存入 指令Sector
    */
    for (i = firstSector; i <= lastSector; i++){

        kernel->synchDisk->WriteSector(hdr->ByteToSector(i * SectorSize), 
					&buf[(i - firstSector) * SectorSize]);
    }
    delete [] buf;
    return numBytes;
}

//----------------------------------------------------------------------
// OpenFile::Length
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
OpenFile::Length() 
{ 
    return hdr->FileLength(); 
}

#endif //FILESYS_STUB
