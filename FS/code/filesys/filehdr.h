// filehdr.h 
//	Data structures for managing a disk file header.  
//
//	A file header describes where on disk to find the data in a file,
//	along with other information about the file (for instance, its
//	length, owner, etc.)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "pbitmap.h"

/*
// 23-0503[j]:  NumDirect = File Header 的 Entry 總數 = (Table 大小)/(Entry 大小)
                -	Table 大小 = SectorSize - 2 * sizeof(int) = 1 SectorSize(128 Bytes) - 2*4 Bytes
                  (File Header 預設佔用 1 Sector Size)
                -	Entry 大小 = sizeof(int)，存放 Sector #
                -	預設 NumDirect = (128-8)/4 = 30 個 Entry
*/
// #define NumDirect 	((SectorSize - 2 * sizeof(int)) / sizeof(int))
// #define MaxFileSize 	(NumDirect * SectorSize)
// 23-0503[j]: MaxFileSize	= NumDirect( Entry總數 = 最多 Sector 數目) * SectorSize
//             最大 File Size = 30 * 128 B = 3840 Bytes < 4 KB


// 23-0508[j]: MP4 Combined Index Allocation
#define NumDirect 	12
#define NumTriple	16
#define MaxFileSize 	67108864 // 23-0508[j]: 64 MB

// The following class defines the Nachos "file header" (in UNIX terms,  
// the "i-node"), describing where on disk to find all of the data in the file.
// The file header is organized as a simple table of pointers to
// data blocks. 
//
// The file header data structure can be stored in memory or on disk.
// When it is on disk, it is stored in a single sector -- this means
// that we assume the size of this data structure to be the same
// as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// There is no constructor; rather the file header can be initialized
// by allocating blocks for the file (if it is a new file), or by
// reading it from disk.

/*
// 23-0503[j]: File Header = FCB(File Control Block) = inode (in Unix)
	-	一個 File 會對應 一個 FCB
	-	每個 FCB 會存放 File's Data 在 Disk 存放的位置 & File Size
		(真實 FS 的 FCB 仍會包含「存取權限、Owner、上一次修改時間...etc」)

	-	實作方式
		-	File Size：用2個int變數 numBytes & numSectors 表示 File 的大小(Bytes) & File 所包含的 Sectors 數目
		-	Location Table
			-	用一個「固定大小的 Table(int Array)」，每個 Entry(int) 存放 File's Data Block 所在的 Sector #
				-	宣告 int dataSectors[NumDirect]，NumDirect 為 Table 的 Entry 總數
					其中 dataSectors[i] = k，表示 File 的「第i個 Data Block」存在 Sector k
					(預設不採用 indirect / doubly indirect blocks)
			-	Table Size = (預設)1 SectorSize - 8 Bytes = 120 Bytes
				(扣掉 8 Bytes = 2個int變數 存放 numBytes & numSectors )

		-	File Size：包含 int numBytes & int numSectors(File 包含的 Sectors 數目)

	-	初始化時機
		-	New File：直接修改「已建立在 Memory 的 File Header」，讓其指向「新分配的 Sector #」
		-	File in Disk：從 Disk 載入 File Header 到 Memory
*/

class FileHeader {
  public:
    // 23-0503[j]: 分配 Free Sector & 收回 Allocated Sector

    bool Allocate(PersistentBitmap *bitMap, int fileSize);// Initialize a file header, 
						//  including allocating space 
						//  on disk for the file data
    void Deallocate(PersistentBitmap *bitMap);  // De-allocate this file's 
						//  data blocks

    // 23-0503[j]: 從 Disk 存取「存在 sectorNumber」的 File Header

    void FetchFrom(int sectorNumber); 	// Initialize file header from disk
    void WriteBack(int sectorNumber); 	// Write modifications to file header
					//  back to disk

    int ByteToSector(int offset);	// Convert a byte offset into the file
					// to the disk sector containing
					// the byte

    int FileLength();			// Return the length of the file 
					// in bytes

    void Print();			// Print the contents of the file.

	// 23-0509[j]: MP4 Combined Index Allocation
    //             要先 Allocate File Header 佔用的空間
    //             依照不同 File Size -> 使用到不同 Level 的 indirect Table 
    //             並產生「不同 Size 的 File Header (包含 indirect Tables)」
	int AllocateHDR(PersistentBitmap *freeMap, int fileSize, bool firstSector);
	void DeallocateHDR(PersistentBitmap *freeMap, int sector);

  private:
    int numBytes;			// Number of bytes in the file
    int numSectors;			// Number of data sectors in the file
    // int dataSectors[NumDirect];		// Disk sector numbers for each data 
					// block in the file
    
    // 23-0503[j]: File Header 中「指向 File's Data 存放位置 的 Table」，是一個 int 陣列
		//             每個 Entry = dataSectors[i] 代表「第i個Data Block」對應存放的「Sector #」
		//             其中 Entry 數目最多 有 NumDirect(30)個，表示 File Size 最大 = 30 * 128 B

	// 23-0508[j]: MP4 Combined Index Allocation
	int direct[9];
	int singleLv;
	int *sTable;

	int doubleLv;
	int **dTable;

	int tripleLv[16];
	int ****tTable;

	void ReadTable(int sector, int* table);
	void WriteTable(int sector, int* table);

	int GetIndexTable(int logic);
	void LoadIndexTable(int logic, int data);

	void AllocateTableInMem(int lv);
	void DeAllocateTableInMem(int lv);

};

#endif // FILEHDR_H
