// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk


#include "copyright.h"

#include "filehdr.h"
#include "debug.h"
#include "synchdisk.h"
#include "main.h"

// 23-0509[j]: MP4 

int WhichLevel(int sectors){
    int k3 = 32*32*32;
    int k2 = 32*32;
    int k1 = 32;

    if(sectors >0 && sectors <= 9){     // 1152 Bytes
        return 1;
    }
    else if(sectors >9 && sectors <= k1){  // 4096 Bytes
        return 2;
    }
    else if(sectors >k1 && sectors <= k2){  // 128 KB
        return 3;
    }
    else if(sectors >k2 && sectors <= (16*k3)){  // 64 MB
        return 4;
    }
}

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the size of the new file
//----------------------------------------------------------------------
/*
// 23-0503[j]: 	bool Allocate(PersistentBitmap *freeMap, int fileSize);
				-	主要功能
					-	針對大小 = fileSize(Bytes) 的 **New File**，分配足夠的 Free Sector 並記錄在 File Header 中
					-	若 Free Sectors 不夠，return FALSE
				-	參數
					-	freeMap 指向一個「存在 Disk 的 Bitmap 物件 (負責維護 Free Sector)」
					-	fileSize：File 的大小 (in Bytes)
*/
// bool
// FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)
// { 
//     // 23-0503[j]: 若檔案有 filesize 個 Bytes，至少需要 numSectors 個 Sectors 才能容納
//     numBytes = fileSize;
//     numSectors  = divRoundUp(fileSize, SectorSize);

//     // 23-0503[j]: 若 freeMap 中「為0位元」個數 < File 所佔 Sectors 個數
//     //             -> Free Sector 不夠，return FALSE
//     if (freeMap->NumClear() < numSectors)
// 	return FALSE;		// not enough space

//     // 23-0503[j]: 若 freeMap 中「為0位元」個數 足夠 -> 則 Pop Free Sector 並分配給 File
//     //             分配完成後 return TRUE
//     for (int i = 0; i < numSectors; i++) {
//         // dataSectors[i] = freeMap->FindAndSet();

//         // since we checked that there was enough free space,
//         // we expect this to succeed

//         // ASSERT(dataSectors[i] >= 0);

//     // 23-0509[j]: MP4 Combined Index Allocation

//         WriteIndexTable(i,freeMap->FindAndSet());
//         ASSERT(ReadIndexTable(i) >= 0);

//     }
//     return TRUE;
// }

// 23-0509[j]: MP4
bool
FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)
{ 
    // 23-0503[j]: 若檔案有 filesize 個 Bytes，至少需要 numSectors 個 Sectors 才能容納
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);

    // 23-0503[j]: 若 freeMap 中「為0位元」個數 < File 所佔 Sectors 個數
    //             -> Free Sector 不夠，return FALSE
    if (freeMap->NumClear() < numSectors)
	return FALSE;		// not enough space

    // 23-0503[j]: 若 freeMap 中「為0位元」個數 足夠 -> 則 Pop Free Sector 並分配給 File
    //             分配完成後 return TRUE
    for (int i = 0; i < numSectors; i++) {

    // 23-0509[j]: MP4 Combined Index Allocation

        LoadIndexTable(i,freeMap->FindAndSet());
        ASSERT(GetIndexTable(i) >= 0);

    }
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------
/*
// 23-0504[j]: void Deallocate(..);
-	主要功能：將 freeMap 中，File 所佔 Sector 代表的位元「設為0」
	(Disk 不必將 Sector 的資料清空，但 SSD 則需要)
*/
// void 
// FileHeader::Deallocate(PersistentBitmap *freeMap)
// {
//     for (int i = 0; i < numSectors; i++) {
//         // ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
//         // freeMap->Clear((int) dataSectors[i]);
//     }
// }

// 23-0509[j]: MP4
void 
FileHeader::Deallocate(PersistentBitmap *freeMap)
{
    for (int i = 0; i < numSectors; i++) {
        ASSERT(freeMap->Test((int) GetIndexTable(i)));
        freeMap->Clear((int) GetIndexTable(i));
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    kernel->synchDisk->ReadSector(sector, (char *)this);

    // 23-0509[j]: MP4
    int k3 = 32*32*32;
    int k2 = 32*32;
    int k1 = 32;

    int tempTable1[32];
    int tempTable2[32];

    int level = WhichLevel(numSectors);

    AllocateTableInMem(level);

    switch (WhichLevel(numSectors)){
        case 1:{    // Direct
            break;
        }
        case 2:{    // 1-Lv indirect
            ReadTable(singleLv, sTable);
            break;
        }
        case 3:{    // 2-Lv indirect
            ReadTable(doubleLv, tempTable1);
            for(int i=0;i<32;i++){
                ReadTable(tempTable1[i],dTable[i]);
            }
            break;
        }
        case 4:{    // 3-Lv indirect x 16
            for(int i=0;i<16;i++){
                ReadTable(tripleLv[i],tempTable1);
                for(int j=0;j<32;j++){
                    ReadTable(tempTable1[j],tempTable2);
                    for(int k=0;k<32;k++){
                        ReadTable(tempTable2[k],tTable[i][j][k]);
                    }
                }
            }
            break;
        }
    }
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    kernel->synchDisk->WriteSector(sector, (char *)this); 

    // 23-0509[j]: MP4
    int k3 = 32*32*32;
    int k2 = 32*32;
    int k1 = 32;

    int tempTable1[32];
    int tempTable2[32];

    int level = WhichLevel(numSectors);

    switch (level){
        case 1:{    // Direct
            break;
        }        
        case 2:{    // 1-Lv indirect
            WriteTable(singleLv, sTable);
            break;
        }
        case 3:{    // 2-Lv indirect
            ReadTable(doubleLv, tempTable1);
            for(int i=0;i<32;i++){
                WriteTable(tempTable1[i],dTable[i]);
            }
            break;
        }
        case 4:{    // 3-Lv indirect x 16
            for(int i=0;i<16;i++){
                ReadTable(tripleLv[i],tempTable1);
                for(int j=0;j<32;j++){
                    ReadTable(tempTable1[j],tempTable2);
                    for(int k=0;k<32;k++){
                        WriteTable(tempTable2[k],tTable[i][j][k]);
                    }
                }
            }
            break;
        }
    }

    DeAllocateTableInMem(level);
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------
/*
// 23-0503[j]: int ByteToSector(int offset)
-	主要功能：return 第offset個 Bytes 的資料，存在「哪個 Sector #」
    -   Sector # = dataSectors[offset / SectorSize]
    -	ex. offset = 392，表示「第392個 Bytes 的資料」存在 File 所佔的「第 392/SectorSize 個 Sector」
            又 392/128 = 3.06，即為 第4個 Sector = dataSector[3] (從0開始算)
-	參數
    -	offset：代表 File 中「第幾個 Bytes」
*/
int
FileHeader::ByteToSector(int offset)
{
    // return(dataSectors[offset / SectorSize]);

    // 23-0508[j]: MP4 Combined Index Allocation

    int logicSector = (offset / SectorSize);
    return GetIndexTable(logicSector);
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

/*
// 23-0503[j]: 主要功能：印出 File Header(FCB) & File 的所有內容
-	先印出 File Size & Loaction Table
-	再印出 File 的所有內容 (讀取 & 印出 dataSectors[0]～dataSectors[numSectors-1])
*/

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];  
    // 23-0503[j]: 動態配置一個 char data[128]，暫存 1 Sector(128 Bytes) 的資料，

    // 23-0503[j]: 先印出 File Size & Loaction Table (dataSectors[..]=File 佔用 Sector #) 
    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < numSectors; i++)
	    // printf("%d ", dataSectors[i]);
        // 23-0508[j]: MP4 Combined Index Allocation
        printf("%d ", GetIndexTable(i));

    // 23-0503[j]: 印出 File 的所有內容 (讀取 & 印出 dataSectors[0]～dataSectors[numSectors-1])
    printf("\nFile contents:\n");

    for (i = k = 0; i < numSectors; i++) {
	    // kernel->synchDisk->ReadSector(dataSectors[i], data);
        // 23-0508[j]: MP4 Combined Index Allocation
        kernel->synchDisk->ReadSector(GetIndexTable(i), data);

        // 23-0503[j]: 依序印出 每個 Sector 的 data[0]～data[127]，直到 File 印完
        //             並 只印出「可印字元 040(space)～176(~)」
        //             若為「不可印字元」則印出 ASCII Code in HEX(16進位編碼)
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
            if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                printf("%c", data[j]);
            else
                printf("\\%x", (unsigned char)data[j]);
        }
        printf("\n"); 
    }
    delete [] data;
}

// 23-0509[j]: MP4 
//             Allocate File Header 佔用的空間
int FileHeader::AllocateHDR(PersistentBitmap *freeMap, int fileSize, bool firstSector){

    // 23-0503[j]: 若檔案有 filesize 個 Bytes，至少需要 numSectors 個 Sectors 才能容納
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);

    int tempTable1[32];
    int tempTable2[32];

    int hdrSector = -2;
    int level = WhichLevel(numSectors);

    if(firstSector){
        hdrSector = freeMap->FindAndSet();
        if(hdrSector < 0) return -1;
    }

    switch (level){
        case 1:{    // Direct
            break;
        }
        case 2:{    // 1-Lv indirect
            singleLv = freeMap->FindAndSet();
            if(singleLv < 0) return -1;
            break;
        }
        case 3:{    // 2-Lv indirect
        
            doubleLv = freeMap->FindAndSet();
            if(doubleLv < 0) return -1;

            for(int i=0;i<32;i++){
                tempTable1[i] = freeMap->FindAndSet();
                if(tempTable1[i] < 0) return -1;
            }
            WriteTable(doubleLv,tempTable1);

            break;
        }
        case 4:{    // 3-Lv indirect x 16

            for(int i=0;i<16;i++){
                tripleLv[i] = freeMap->FindAndSet();
                if(tripleLv[i] < 0) return -1;


                for(int j=0;j<32;j++){
                    tempTable1[j] = freeMap->FindAndSet();
                    if(tempTable1[j] < 0) return -1;                    

                    for(int k=0;k<32;k++){
                        tempTable2[k] = freeMap->FindAndSet();
                        if(tempTable2[k] < 0) return -1;
                    }
                    WriteTable(tempTable1[j],tempTable2);
                }
                WriteTable(tripleLv[i],tempTable1);
            }
            break;
        }
    }
    AllocateTableInMem(level);
    return hdrSector;
}

void FileHeader::DeallocateHDR(PersistentBitmap *freeMap, int sector){
    int tempTable1[32];
    int tempTable2[32];

    int level = WhichLevel(numSectors);

    switch (level){
        case 1:{    // Direct
            break;
        }
        case 2:{    // 1-Lv indirect
            freeMap->Clear(singleLv);
            break;
        }
        case 3:{    // 2-Lv indirect

            ReadTable(doubleLv,tempTable1);

            for(int i=0;i<32;i++){
                freeMap->Clear(tempTable1[i]);
            }
            freeMap->Clear(doubleLv);
            break;
        }
        case 4:{    // 3-Lv indirect x 16

            for(int i=0;i<16;i++){
                ReadTable(tripleLv[i],tempTable1);

                for(int j=0;j<32;j++){
                    ReadTable(tempTable1[j],tempTable2);

                    for(int k=0;k<32;k++){
                        freeMap->Clear(tempTable2[k]);
                    }
                    freeMap->Clear(tempTable1[j]);
                }
                freeMap->Clear(tripleLv[i]);
            }
            break;
        }
    }
    freeMap->Clear(sector);
    DeAllocateTableInMem(level);
}

void FileHeader::LoadIndexTable(int logic, int sector){
    int k3 = 32*32*32;
    int k2 = 32*32;
    int k1 = 32;

    switch (WhichLevel(numSectors)){
        case 1:{    // Direct
            direct[logic] = sector;
            break;
        }
        case 2:{    // 1-Lv indirect
            sTable[logic] = sector;
            break;
        }
        case 3:{    // 2-Lv indirect
            int i1 = logic / k1;
            int i2 = logic % k1;
            dTable[i1][i2] = sector;
            break;
        }
        case 4:{    // 3-Lv indirect x 16
            int i0 = logic / k3;
            int i1 = (logic % k3) / k2;
            int i2 = (logic % k2) / k1;
            int i3 = logic % k1;
            tTable[i0][i1][i2][i3] = sector;
            break;
        }
    }
}

int FileHeader::GetIndexTable(int logic){
    int k3 = 32*32*32;
    int k2 = 32*32;
    int k1 = 32;

    switch (WhichLevel(numSectors)){
        case 1:{    // Direct
            return direct[logic];
        }
        case 2:{    // 1-Lv indirect
            return sTable[logic];
        }
        case 3:{    // 2-Lv indirect
            int i1 = logic / k1;
            int i2 = logic % k1;
            return dTable[i1][i2];
        }
        case 4:{    // 3-Lv indirect x 16
            int i0 = logic / k3;
            int i1 = (logic % k3) / k2;
            int i2 = (logic % k2) / k1;
            int i3 = logic % k1;
            return tTable[i0][i1][i2][i3];
        }
    }
}

void FileHeader::ReadTable(int sector, int* table){
    char buf[128];

    kernel->synchDisk->ReadSector(sector, (char *)buf);
    int *tempArray = (int *)buf;

    for(int i=0;i<32;i++)
        table[i] = tempArray[i];

}
void FileHeader::WriteTable(int sector, int* table){
    char buf[128];

    // kernel->synchDisk->ReadSector(sector, (char *)buf);
    int *tempArray = (int *)buf;

    for(int i=0;i<32;i++)
        tempArray[i] = table[i];

    kernel->synchDisk->WriteSector(sector, (char *)buf);
}

void FileHeader::AllocateTableInMem(int lv){
    switch (lv){
        case 1:{    // Direct
            break;
        }
        case 2:{    // 1-Lv indirect
            sTable = new int[32];
            break;
        }
        case 3:{    // 2-Lv indirect                

            dTable = new int*[32];
            for(int i = 0; i < 32; i++){
                dTable[i] = new int[32];
            }

            break;
        }
        case 4:{    // 3-Lv indirect x 16

            tTable = new int***[16];
            for(int i=0;i<16;i++){
                tTable[i] = new int**[32];
                for(int j=0;j<32;j++){
                    tTable[i][j] = new int*[32];
                    for(int k=0;k<32;k++){
                        tTable[i][j][k] = new int[32];
                    }
                }
            }
            break;
        }
    }
}
void FileHeader::DeAllocateTableInMem(int lv){
    switch (lv){
        case 1:{    // Direct
            break;
        }
        case 2:{    // 1-Lv indirect
            delete [] sTable;
            break;
        }
        case 3:{    // 2-Lv indirect

            for(int i=0;i<32;i++){
                delete [] dTable[i];
            }
            delete [] dTable;

            break;
        }
        case 4:{    // 3-Lv indirect x 16

            for(int i=0;i<16;i++){
                for(int j=0;j<32;j++){
                    for(int k=0;k<32;k++){
                        delete [] tTable[i][j][k];
                    }
                    delete [] tTable[i][j];
                }
                delete [] tTable[i];
            }
            delete [] tTable;
            break;
        }
    }
}
