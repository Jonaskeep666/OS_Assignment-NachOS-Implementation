// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   － is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)

// 23-0502[j]: 此處定義 NachOS 真實 File System 的方法

#ifndef FILESYS_STUB

#include "copyright.h"
#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0   // 23-0502[j]: bitmap File 存放的 Sector
#define DirectorySector 	1   // 23-0502[j]: Directory File 存放的 Sector

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte)
// 23-0511[j]: MP4
#ifndef NumDirEntries
#define NumDirEntries 		64
#endif
#define DirectoryFileSize 	(sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------
/*
// 23-0505[j]: FileSystem(bool format)
-	背景：在 class synchDisk 被初始化後(可以用 Disk 後)，才會建立 File System
		(因為 class synchDisk 建構子 才會 new Disk(..))

-	主要功能：格式化 raw Disk 並 Open Dir & Bitmap File
    -	若 format = TRUE，表示 Disk 需要「格式化」-> 建立 new Directory & Free Bitmap File
        -	new PersistentBitmap、Directory、FileHeader (Bitmap、Directory) 為了使用 Bitmap & Directory 的方法

        -	先分配、建立 File Header(FCB) for Bitmap & Dir
        1.	[修改 Bitmap、分配空間]
            在 new Bitmap (in Mem) 上，標記 Sector 0、1 為「使用中」，因為要分配給 Bitmap File Header & Dir File Header 使用
        2.	[修改 File Header、分配空間]
            分配 Free Sector 給 Bitmap & Directory 的 File Content (預設 1 Sector) 並修改 Bitmap 的值
        3.	[WB File Header]
            將 Bitmap File Header & Dir File Header 寫回 Disk

        -	有了 File Header(FCB) 才能「開啟、修改 File Content」for Bitmap & Dir
        4.	[Open File] 
            開啟 Bitmap & Directory -> 從 Disk 載入「指定 Sector #」的 File Header
        5.	[WB Bitmap File Content & Directory File Content]
            將 Bitmap (in Mem) & Directory (in Mem) 寫回 Disk
            -	其中 Bitmap 已經有修改過(已分配了 4 Sectors 給 Bitmap FCB、Bitmap File、Dir FCB、Dir File)
            -	其中 Directory File 則為空(尚未建立任何 File + name + FCB location 在裡面)
        
    -	若 format = FALSE，表示 Disk 不需要「格式化」-> 開啟 Old Directory & Free Bitmap File
        -	[Open File] 
            開啟 Bitmap & Directory -> 從 Disk 載入「指定 Sector #」的 File Header
*/

FileSystem::FileSystem(bool format)
{ 
    DEBUG(dbgFile, "Initializing the file system.");
    if (format) {
        PersistentBitmap *freeMap = new PersistentBitmap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

        DEBUG(dbgFile, "Formatting the file system.");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)

        // 23-0505[j]: [修改 Bitmap、分配空間]
        //             在 new Bitmap (in Mem) 上，標記 Sector 0、1 為「使用中」
        //             因為要分配給 Bitmap File Header & Dir File Header 使用
        freeMap->Mark(FreeMapSector);	    
        freeMap->Mark(DirectorySector);

        // 23-0509[j]: MP4 要自行 AllocateHDR
 
        mapHdr->AllocateHDR(freeMap, FreeMapFileSize, FALSE);
        dirHdr->AllocateHDR(freeMap, DirectoryFileSize, FALSE);

        cout << " AllocateHDR for map & Dit are done. " << endl;
        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        // 23-0505[j]: [修改 File Header、分配空間]
        //             分配 Free Sector 給 Bitmap & Directory 的 File Content (預設 1 Sector)
        //             並修改 Bitmap 的值
        ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

        cout << " Allocate for map & Dit are done. " << endl;
        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).

        // 23-0505[j]: [WB File Header] 將 Bitmap File Header & Dir File Header 寫回 Disk
        DEBUG(dbgFile, "Writing headers back to disk.");
        mapHdr->WriteBack(FreeMapSector);    
        dirHdr->WriteBack(DirectorySector);

        cout << " WriteBack for mapHDR & DitHDR are done. " << endl;
        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.

        // 23-0505[j]: [Open File] 開啟 Bitmap & Directory
        //             從 Disk 載入「指定 Sector #」的 File Header
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
     
        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        // 23-0505[j]: [WB Bitmap File Content & Directory File Content]
        //             將 Bitmap (in Mem) & Directory (in Mem) 寫回 Disk
        //             其中 Bitmap 已經有修改過(已分配了 4 Sectors 給 Bitmap FCB、Bitmap File、Dir FCB、Dir File)
        //             其中 Directory FilHDRe 則為空(尚未建立任何 File + name + FCB location 在裡面)
        DEBUG(dbgFile, "Writing bitmap and directory back to disk.");
        freeMap->WriteBack(freeMapFile);	 // flush changes to disk

        cout << " WriteBack fpr map are done. " << endl;
        directory->WriteBack(directoryFile);

        cout << " WriteBack fpr Dit are done. " << endl;

        if (debug->IsEnabled('f')) {
            freeMap->Print();
            directory->Print();
        }
        delete freeMap; 
        delete directory; 
        delete mapHdr; 
        delete dirHdr;
    } else {
    // if we are not formatting the disk, just open the files representing
    // the bitmap and directory; these are left open while Nachos is running

    // 23-0505[j]: 若 format = FALSE，表示 Disk 不需要「格式化」
    //             [Open File] 開啟 Bitmap & Directory -> 從 Disk 載入「指定 Sector #」的 File Header
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
    // 23-0507[j]: MP4 為了自行新增的 OpenFileTable 初始化
    for(int i=0;i<NumOFTEntries;i++) openFileTable[i] = NULL;
    openFileCount = 0;

    cout << "Format done!" << endl;
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------
/*
// 23-0507[j]: bool Create(char *name, int initialSize);
-	背景：NachOS 預設 FS 不支援「Runtime 改變 File Size」，故須在此「固定檔案大小」
-	主要功能：Create new File = 修改 Dir & Bitmap 並建立 File Header 
    -	![](assets/FS3.jpeg)
        1.	Load Directory 到 Memory
        2.	檢查 Directory 是否存在「同名 File」
            -	若是 return FALSE

        3.	Load Bitmap 到 Memory
        4.	[分配 1 Sector 給 File Header]
            -	檢查 Bitmap 是否有足夠的 Free Sectors for File Header
            -	若無 return FALSE

        5.	[分配 1 Dir Entry 給 File 存放 < name , Sector #>]
            -	檢查 Directory Table 是否有 Free Entry
            -	若無 return FALSE

        6.	[建立 File Header 並 分配 n Sectors 給 File Data Blocks]
            -	檢查 Bitmap 是否有足夠的 Free Sectors for File Data Blocks
            -	若無 return FALSE

        7.	[File 初始化完成 -> Write Back]
            -	Write Back File Header、Bitmap File、Directory File
*/

// bool
// FileSystem::Create(char *name, int initialSize)
// {
//     Directory *directory;
//     PersistentBitmap *freeMap;
//     FileHeader *hdr;
//     int sector;
//     bool success;

//     DEBUG(dbgFile, "Creating file " << name << " size " << initialSize);

//     // 23-0507[j]: Load Directory 到 Memory
//     directory = new Directory(NumDirEntries);
//     directory->FetchFrom(directoryFile);

//     // 23-0506[j]: 檢查是否存在「同名 File」
//     if (directory->Find(name) != -1)
//         success = FALSE;			// file is already in directory
//     else {	
//         freeMap = new PersistentBitmap(freeMapFile,NumSectors);

//         // 23-0506[j]: 檢查是否有足夠 1 Sector for File Header
//         // sector = freeMap->FindAndSet();	// find a sector to hold the file header
        
//         cout << "Create NachOS File Header" <<endl;

//         // 23-0509[j]: MP4 檢查是否有足夠 N Sector for Multilevel File Header
//         hdr = new FileHeader;   // 23-0509[j]: 為了先使用 hdr 的方法，故先配置一個
//         sector = hdr->AllocateHDR(freeMap,initialSize,TRUE);
        
//         cout << "File Header Created!! & Sector = " << sector << endl;

//     	if (sector == -1) 		
//             success = FALSE;		// no free block for file header 
//         // 23-0506[j]: 檢查 Directory Table 是否有 Free Entry
//         else if (!directory->Add(name, sector))
//             success = FALSE;	// no space in directory
//         else {
//             // hdr = new FileHeader;

//             cout << "Trying to allocate File" << endl;
//             // 23-0506[j]: 檢查是否有足夠 n Sector for File Data Blocks
//             if (!hdr->Allocate(freeMap, initialSize))
//                     success = FALSE;	// no space on disk for data
//             else {	
//                 success = TRUE;
//             // everthing worked, flush all changes back to disk
//             // 23-0506[j]: 所有檢查通過 -> 將 File Header、Directory、Bitmap 寫回

//                 cout << " Write Back All" << endl;
//                 hdr->WriteBack(sector); 		
//                 directory->WriteBack(directoryFile);
//                 freeMap->WriteBack(freeMapFile);
//             }
//             delete hdr;
//         }
//         delete freeMap;
//     }
//     delete directory;
//     return success;
// }

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//	To open a file:
//	  Find the location of the file's header, using the directory 
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------
/*
// 23-0507[j]: OpenFile* Open(char *name);
-	主要功能：Open a File = Load File Header 到 Memory
            並 return openFile物件(File Header + SeekPosition)
*/

// OpenFile *
// FileSystem::Open(char *name)
// { 
//     Directory *directory = new Directory(NumDirEntries);
//     OpenFile *openFile = NULL;
//     int sector;

//     DEBUG(dbgFile, "Opening file" << name);
//     // 23-0507[j]: Load Directory 到 Memory 
//     directory->FetchFrom(directoryFile);



//     // 23-0507[j]: Load File Header 到 Memory 
//     sector = directory->Find(name); 
//     if (sector >= 0){
//         openFile = new OpenFile(sector);	// name was found in directory
//         // cout << "File in FS:" << name << endl;
//     }	
	     
//     delete directory;
//     return openFile;				// return NULL if not found
// }

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------
/*
// 23-0507[j]: bool Remove(char *name);
-	主要功能：Remove a File = 收回已分配 Sectors & Dir Entry
*/
// bool
// FileSystem::Remove(char *name)
// { 
//     Directory *directory;
//     PersistentBitmap *freeMap;
//     FileHeader *fileHdr;
//     int sector;
    
//     // 23-0507[j]: Load Directory 到 Memory 
//     directory = new Directory(NumDirEntries);
//     directory->FetchFrom(directoryFile);

//     // 23-0507[j]: Load File Header 到 Memory 
//     sector = directory->Find(name);
//     if (sector == -1) {
//        delete directory;
//        return FALSE;			 // file not found 
//     }
//     fileHdr = new FileHeader;
//     fileHdr->FetchFrom(sector);

//     // 23-0507[j]: Load Bitmap Header 到 Memory 
//     freeMap = new PersistentBitmap(freeMapFile,NumSectors);

//     // 23-0507[j]: 收回已分配的 n Sectors for File Data Blocks
//     fileHdr->Deallocate(freeMap);  		// remove data blocks

//     // 23-0507[j]: 收回已分配的 1 Sectors for File Header
//     // freeMap->Clear(sector);			// remove header block
//     fileHdr->DeallocateHDR(freeMap,sector); // 23-0507[j]: MP4

//     // 23-0507[j]: 收回已分配的 1 Entry for <Filename, Sector #>
//     directory->Remove(name);

//     // 23-0507[j]: Write Back Bitmap File、Directory File
//     freeMap->WriteBack(freeMapFile);		// flush to disk
//     directory->WriteBack(directoryFile);        // flush to disk

//     delete fileHdr;
//     delete directory;
//     delete freeMap;
//     return TRUE;
// } 

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------
// 23-0507[j]: List Directory 中的 所有 Filename = 呼叫 directory->List()
// void
// FileSystem::List()
// {
//     Directory *directory = new Directory(NumDirEntries);

//     directory->FetchFrom(directoryFile);
//     directory->List();
//     delete directory;
// }

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile,NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    // 23-0507[j]: 印出 Bitmap File Header (Location Table \ FileSize) 
    //             & Bitmap File Content
    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    // 23-0507[j]: 印出 Directory File Header (Location Table \ FileSize) 
    //             & Directory File Content
    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    // 23-0507[j]: 印出 Bitmap File Content (以 0101... 的格式)
    freeMap->Print();

    // 23-0507[j]: 印出 Directory 中的 所有 Filename & File Content
    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
} 

// 23-0507[j]: MP4
//             對接 System Call Interface in ksyscall.h

int FileSystem::FindFreeEntry(){
    if(openFileCount >= NumOFTEntries) return -1;
    for(int i=0;i<NumOFTEntries;i++){
        // cout << "openFileTable[i]=" <<openFileTable[i] << endl;
        if(openFileTable[i] == NULL) return i;
    }
    return -1;
}

OpenFileId FileSystem::OpenReturnId(char *name){
    if(openFileCount >= NumOFTEntries){
        return -1;
    }

    int index = -1;
    OpenFile *openFile = Open(name);
    if(openFile == NULL){
        return -1;
    }
    else{
        openFileCount++;
        index = FindFreeEntry();
        openFileTable[index] = openFile;
    }
    // cout << "index=" << index << endl;
    return index;
}

int FileSystem::Close(OpenFileId fileId){
    if(!openFileCount) return -1;
    if(openFileTable[fileId] == NULL) return -1;
    else{
        openFileCount--;
        openFileTable[fileId] = NULL;
    }
    return 1;
}

int FileSystem::Write(OpenFileId fd,char *buffer, int nBytes){
    return openFileTable[fd]->Write(buffer,nBytes);
}
    
int FileSystem::Read(OpenFileId fd,char *buffer, int nBytes){
    return openFileTable[fd]->Read(buffer,nBytes);
}

// 23-0510[j]: MP4 Subdirectory

// 23-0511[j]: 主要功能
//             分析 Path 回傳 ParentDirectory 的 Sector # 以及 Path 代表的 Filename
int FileSystem::PathParse(char *path, char *filename){

    Directory *directory;
    int sector;

    // 23-0507[j]: Load Root Directory 到 Memory
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);

    // 23-0510[j]: 分析 path
    char** pathName;

    pathName = new char*[10];
    for(int i=0;i<10;i++){
        pathName[i] = new char[FileNameMaxLen+1];
        pathName[i][0]='\0';    // 23-0511[j]: 第一個字元，先設定爲 '\0'
    }

    char* cursor = path;
    int pi = 0;
    int i=0, j=0;
    int last;

    while(cursor[pi] != '\0'){
        pathName[i][j] = cursor[pi]; 

        // 23-0511[j]: 遇到 '/' 表示這段是 File/Dir Name，將 '/' 覆蓋爲 '\0' 開始下一段
        if(pathName[i][j] == '/'){
            pathName[i][j]='\0';
            i++; j=0;
        }
        else{   // 23-0511[j]: 沒遇到 '/' 則繼續讀下一個字元
            j++;
            if(j > 9) return -1;    // 23-0511[j]: Name > 9 字元
        }
        pi++;
        if(pi > 255) return -1;     // 23-0511[j]: Path > 255 字元
    }
    pathName[i][j]='\0';    // 23-0511[j]: 跳出迴圈，最後一串就是「最終 Filename」，末端加上 '\0'
    last = i;

    for(int k=0;k<=FileNameMaxLen;k++){
        filename[k] = pathName[last][k];
    }
            
    // 23-0510[j]: 分析 path 完成，開始打開 路徑目錄

    
    if(last == 1)   // 23-0510[j]: File/Dir 就剛好在 root 下，直接回傳 root Directory 的 Sector # = 1
        sector = 1;
    else{   // 23-0511[j]: File/Dir 在其他目錄下

        sector = directory->Find(pathName[1]);   // 23-0510[j]: 先找 root 下的 File/Dir
        if(sector < 0) return -1;

        for(int i=2;i<last;i++){
            // 23-0510[j]: Open File
            OpenFile *subDir;
            Directory *subDirectory;
        
            subDir = new OpenFile(sector);
            subDirectory = new Directory(NumDirEntries);
            subDirectory->FetchFrom(subDir);
            // 23-0511[j]: 根據 filename 的父目錄 找到 file 的 sector #
            sector = subDirectory->Find(pathName[i]);   
            delete subDir;
            delete subDirectory;
        }
    }

    for(int i=0;i<10;i++){
        delete pathName[i];
    }
    delete [] pathName;

    // 23-0511[j]: 回傳 File Sector #
    return sector;
}

// 23-0511[j]: 主要功能
//             根據 type (0 File, 1 Dir) 來在 absolutePath 建立 File/Dir
bool FileSystem::Create(char *absolutePath, int initialSize, int type){

    OpenFile* parentDirFile;
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    // 23-0511[j]: 取得 File 的「父目錄 Sector」& Filename
    char filename[FileNameMaxLen+1];
    int parentSector = PathParse(absolutePath,filename);

    // 23-0511[j]: 若要建立目錄 FileSize = DirectoryFileSize
    if(type){
        initialSize = DirectoryFileSize;
        DEBUG(dbgFile, "Creating Dir " << filename << " size " << initialSize);
    }
    else DEBUG(dbgFile, "Creating file " << filename << " size " << initialSize);

    // 23-0511[j]: 開啟 parentDirectory
    //             若 parentSector = 1，表示 File 的父目錄 = Root
    //             若 parentSector != 1，表示 File 的父目錄爲其他(要先打開，才能 Fetch)
    if(parentSector == 1){
        parentDirFile = directoryFile;
    }
    else parentDirFile = new OpenFile(parentSector);

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(parentDirFile);

    // cout << "Find filename:" << filename << endl;

    // 23-0511[j]: 檢查 filename 是否已存在/重複
    if (directory->Find(filename) != -1){
        success = FALSE;
        cout << "success = " << success << endl;
    }
    else {	
        freeMap = new PersistentBitmap(freeMapFile,NumSectors);
        
        // cout << "Create NachOS File Header" <<endl;

        hdr = new FileHeader; 
        sector = hdr->AllocateHDR(freeMap,initialSize,TRUE);
        
        // cout << "File Header Created!! & Sector = " << sector << endl;

    	if (sector == -1) 		
            success = FALSE;		// no free block for file header 
  
        else if (!directory->Add(filename, sector,type))
            success = FALSE;	    // no space in directory
        else {

            cout << "Trying to allocate File" << endl;
            // 23-0506[j]: 檢查是否有足夠 n Sector for File Data Blocks
            if (!hdr->Allocate(freeMap, initialSize))
                    success = FALSE;	// no space on disk for data
            else {	
                success = TRUE;
            // everthing worked, flush all changes back to disk
            // 23-0506[j]: 所有檢查通過 -> 將 File Header、Directory、Bitmap 寫回

                cout << " Write Back All" << endl;

                // 23-0511[j]: 若建立的 File 是目錄，則要為其「初始化」並寫回 Disk (所有 inUse = FALSE 才對)
                if(type){
                    OpenFile* subDir = new OpenFile(sector);
                    Directory* subDirectory = new Directory(NumDirEntries); // Initialize subDirectory
                    subDirectory->WriteBack(subDir);
                    delete subDirectory;
                    delete subDir;
                }
                
                hdr->WriteBack(sector); 		
                directory->WriteBack(parentDirFile);
                freeMap->WriteBack(freeMapFile);
            }
            delete hdr;
        }
        delete freeMap;
    }
    if(parentSector != 1) delete parentDirFile;
    delete directory;

    if(success) cout << "Create success!" << endl;

    return success;    
}

// 23-0511[j]: 主要功能
//             根據 absolutePath 打開 File (載入 File Header 並回傳)
OpenFile* FileSystem::Open(char *absolutePath)
{ 
    OpenFile* parentDirFile;
    Directory *directory;
    OpenFile *openFile = NULL;
    int sector;

    char name[FileNameMaxLen+1];
    int parentSector = PathParse(absolutePath,name);

    // 23-0510[j]: 開啟 parentDirectory
    if(parentSector == 1){
        parentDirFile = directoryFile;
    }
    else parentDirFile = new OpenFile(parentSector);

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(parentDirFile);

    DEBUG(dbgFile, "Opening file" << name);

    // 23-0511[j]: 找到 File Sector
    sector = directory->Find(name); 

    // 23-0507[j]: Load File Header 到 Memory 
    if (sector >= 0){
        openFile = new OpenFile(sector);	// name was found in directory
        // cout << "File in FS:" << name << endl;
    }	
	     
    delete directory;
    if(parentSector != 1) delete parentDirFile;
    return openFile;				// return NULL if not found
}

// 23-0511[j]: 主要功能
//             根據 recursice 來 List Path 的 File/Dir
//             0: 僅列出當前目錄下的檔案
//             1: 列出當前目錄下，以及其內的所有 檔案/目錄
void FileSystem::List(char *path, bool recursice)
{
    OpenFile* parentDirFile;
    OpenFile* dirFile;
    Directory *directory;
    int dirSector;

    char filename[FileNameMaxLen+1];
    int parentSector = PathParse(path,filename);

    // 23-0511[j]: 若 filename[0]=='\0' 表示 path = /，則設定 filename = "root"
    if(filename[0]=='\0')
        strcpy(filename,"root");

    cout << "---------------------------------------" << endl;
    printf("Target Directory Name = %s & parentSector = %d \n",filename,parentSector);

    if(parentSector == 1){
        parentDirFile = directoryFile;
    }
    else parentDirFile = new OpenFile(parentSector);

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(parentDirFile);

    // 23-0511[j]: filename = "root" 表示 直接列出 Root 目錄下的檔案/目錄
    if(!strcmp(filename,"root")){    
        // 23-0511[j]: 依照 recursice 決定列印的範圍
        if(recursice) directory->RecursiveList(0);
        else directory->List();
    }
    // 23-0511[j]: 要 List 的是 root之下的 某個 File/SubDir
    else{   
        // cout << "Listing ... " << filename << endl;

        // 23-0511[j]: 根據 父目錄 找到「當前目錄 的 Sector#」
        dirSector = directory->Find(filename);

        if(directory->IsDirectory(filename)){   // 23-0511[j]: 若 File 屬於 Directory，才要 Recursive List
            dirFile = new OpenFile(dirSector);
            Directory *subDirectory = new Directory(NumDirEntries);
            subDirectory->FetchFrom(dirFile);

            // 23-0511[j]: 依照 recursice 決定列印的範圍
            if(recursice) subDirectory->RecursiveList(0);
            else subDirectory->List();

            delete dirFile;
            delete subDirectory;
        }
        else{   // 23-0511[j]: 若 File 不是 Directory，直接下一步 印出自身
            printf("%s",filename);
        } 
    }

    cout << "---------------------------------------" << endl;
    cout << "List done! " <<endl;
}

// 23-0511[j]: 主要功能
//             刪除 absolutePath 所代表的 File/Dir
bool
FileSystem::Remove(char *absolutePath)
{ 
    OpenFile *parentDirFile;
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *fileHdr;
    int sector;

    char name[FileNameMaxLen+1];
    int parentSector = PathParse(absolutePath,name);

    // 23-0510[j]: 開啟 parentDirectory
    if(parentSector == 1){
        parentDirFile = directoryFile;
    }
    else parentDirFile = new OpenFile(parentSector);

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(parentDirFile);

    DEBUG(dbgFile, "Removing file: " << name);

    sector = directory->Find(name); 

    if (sector == -1) {
       delete directory;
       return FALSE;			 // file not found 
    }

    // 23-0511[j]: 找到 File Sector 後，Load File Header
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    // 23-0511[j]: 收回分配的 Sector
    freeMap = new PersistentBitmap(freeMapFile,NumSectors);
    fileHdr->Deallocate(freeMap); 
    fileHdr->DeallocateHDR(freeMap,sector); 

    // 23-0511[j]: 從 Directory 中移除
    directory->Remove(name);

    // 23-0507[j]: Write Back Bitmap File、Directory File
    freeMap->WriteBack(freeMapFile);		// flush to disk
    directory->WriteBack(parentDirFile);        // flush to disk

    if(parentSector != 1) delete parentDirFile;
    delete fileHdr;
    delete directory;
    delete freeMap;

    cout << " Remove Success!!! " <<endl;
    return TRUE;
}

// 23-0511[j]: 主要功能
//             刪除 path 所在目錄下 的所有 Files/Directories
void FileSystem::RecursiveRemove(char *path)
{
    OpenFile* parentDirFile;
    OpenFile* dirFile;
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *fileHdr;
    int dirSector;
    

    char filename[FileNameMaxLen+1];
    int parentSector = PathParse(path,filename);

    // 23-0511[j]: 若 filename[0]=='\0' 表示 path = /，則設定 filename = "root"
    if(filename[0]=='\0')
        strcpy(filename,"root");

    printf("Target Directory Name = %s & parentSector = %d \n",filename,parentSector);

    if(parentSector == 1){
        parentDirFile = directoryFile;
    }
    else parentDirFile = new OpenFile(parentSector);

    // 23-0511[j]: Load Parent Directory
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(parentDirFile);

    // 23-0511[j]: Load Bitmap
    freeMap = new PersistentBitmap(freeMapFile,NumSectors);

    // 23-0511[j]: 待刪除的是 root 以下的所有檔案，root 自己不能刪除
    if(!strcmp(filename,"root")){
        directory->RecursiveRemove(freeMap);
    }
    // 23-0511[j]: 待刪除 是root之下的 某個 File/Dir，該 File/Dir 要在最後被刪除
    else{
        // cout << "Recursive Removing: " << filename <<endl;
        // 23-0511[j]: 找到「要刪除的 File/Dir 的 Sector #」
        dirSector = directory->Find(filename);

        // 23-0511[j]: 若「要刪除的 File/Dir」屬於 Directory 才需要「遞迴刪除」
        if(directory->IsDirectory(filename)){ 

            // 23-0511[j]: 開啟 待刪除的 Directory
            dirFile = new OpenFile(dirSector);
            Directory *subDirectory = new Directory(NumDirEntries);
            subDirectory->FetchFrom(dirFile);

            // 23-0511[j]: 將 待刪除的 Directory 其內所含的 File/Dir 都刪除
            subDirectory->RecursiveRemove(freeMap);

            delete dirFile;
            delete subDirectory;
        }
        else{} // 23-0511[j]: 若 File 不是 Directory，直接下一步 刪除自身

        // 23-0511[j]: 開始刪除 該 Directory/File 本身
        //             1. 先收回空間
        //             2. 再從 目錄 移除
        fileHdr = new FileHeader;
        fileHdr->FetchFrom(dirSector);
        fileHdr->Deallocate(freeMap); 
        fileHdr->DeallocateHDR(freeMap,dirSector); 

        directory->Remove(filename);

        delete fileHdr;
    }

    // 23-0507[j]: Write Back Bitmap File、Directory File
    freeMap->WriteBack(freeMapFile);		// flush to disk
    directory->WriteBack(parentDirFile);        // flush to disk

    delete directory;
    delete freeMap;

    cout << " Recursive Remove Success!!! " <<endl;
}

#endif // FILESYS_STUB
