// directory.cc 
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------
/*
// 23-0504[j]: Directory(int size)
-	背景：當 Disk 剛被格式化，需建立一個 new Directory (不然直接 FetchFrom Disk 即可)
-	主要功能：建立/初始化 Table[size]
    -	Table Entry = class DirectoryEntry，且所有 Table Entry 皆未使用(inUse = FALSE)
    -	tableSize = size
*/
// Directory::Directory(int size)
// {
//     table = new DirectoryEntry[size];
//     tableSize = size;
//     for (int i = 0; i < tableSize; i++){
//         table[i].inUse = FALSE;
//     }    
// }

// 23-0511[j]: MP4
Directory::Directory(int size)
{
    table = new DirectoryEntry[size];
    tableSize = size;
    for (int i = 0; i < tableSize; i++){
        table[i].inUse = FALSE;
        // 23-0511[j]: MP4
        table[i].isDir = IsFile;
    }    
}


//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{ 
    delete [] table;
} 

/*
// 23-0504[j]: FetchFrom(..) / WriteBack(..)
-   主要功能：從 Disk 將 Direcotry 載入 Memory / 將「已修改」Direcotry 寫回 Disk
-	假設已開啟「存在 Disk」的「**Direcotry File**」(NachOS 開機時 即會開啟)
    -	Direcotry File 存在 Sector 1

    -	file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
        (從 position = 0 處開始讀取，將「tableSize * sizeof(DirectoryEntry)」Bytes 的資料 存入 table 指向的空間)

    -   file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
		(將 table 指向空間中「tableSize * sizeof(DirectoryEntry)」Bytes 的資料，寫入 File position = 0 處)
*/

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void
Directory::FetchFrom(OpenFile *file)
{
    (void) file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void
Directory::WriteBack(OpenFile *file)
{
    (void) file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------
// 23-0504[j]: int FindIndex(char *name)
//             根據 name 搜尋 Directory，若找到 Filename = name，則回傳該 Entry <Filename,Sector#> 的 Index
int
Directory::FindIndex(char *name)
{
    for (int i = 0; i < tableSize; i++)
        // 23-0504[j]: strncmp(char *s1, char *s2, size_t n)
        //             比較 s1 & s2 的「前n個字元」
        //             若 s1 > s2，回傳值 > 0；若 s1 = s2，回傳值 = 0

        // 23-0504[j]: 若 Entry 被使用，且 Filename = 欲搜尋的 name -> return Entry Index(i)
        //             若沒找到 return -1
        if (table[i].inUse && !strncmp(table[i].name, name, FileNameMaxLen))
	        return i;
    return -1;		// name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't 
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------
/*
// 23-0504[j]: int Find(char *name)
        -   主要功能：找到 name 的 File Header 位置(Sector #)
            -	呼叫 FindIndex(..)
            -	根據 name 搜尋 Directory
                若找到 Filename = name，則 return FCB(File Header) 所在的 Sector #
                若找不到，return -1
*/
int
Directory::Find(char *name)
{
    int i = FindIndex(name);

    if (i != -1)
	    return table[i].sector;
    return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------
/*
// 23-0504[j]: bool Add(char *name, int newSector)
    -	尋找 Directory Table 中「未使用的 Entry」
    -	若找到 -> 將該 Entry 分配給「名為 name 的 File」使用，並 return TRUE
        ( 設定 table[i].inUse = TRUE & table[i].name = name & table[i].sector = newSector )
    -	若沒找到 -> return FALSE
*/

// bool
// Directory::Add(char *name, int newSector)
// { 
//     // 23-0504[j]: 若 name 在 Directory 中已經重複，則 return FALSE
//     if (FindIndex(name) != -1)
// 	    return FALSE;

//     // 23-0504[j]: 竟然 從頭找 未使用的 Entry，再將其分配給「名為 name 的 File」使用
//     for (int i = 0; i < tableSize; i++)
//         if (!table[i].inUse) {
//             table[i].inUse = TRUE;
//             // 23-0504[j]: 複製 name 的「前 9 個字元」到 table[i].name
//             strncpy(table[i].name, name, FileNameMaxLen); 
//             table[i].sector = newSector;
//         return TRUE;
// 	    }
//     return FALSE;	// no space.  Fix when we have extensible files.
// }



//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory. 
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

// 23-0504[j]: bool Remove(char *name)
//             根據 name 搜尋 Directory，若找到該 Entry 則設為「未使用」並 return TRUE
//             否則 return FALSE

bool
Directory::Remove(char *name)
{ 
    int i = FindIndex(name);

    if (i == -1)    return FALSE; 	// name not in directory
    table[i].inUse = FALSE;
    return TRUE;	
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory. 
//----------------------------------------------------------------------
// 23-0504[j]: void List()
//             印出 Directory 所有的 File (使用中 Entry 的 name)
void
Directory::List()
{
    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse)
            printf("%s\n", table[i].name);
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------
// 23-0504[j]: void Print();
//             印出 Directory 所有的 File、對應 Sector # 以及 File Header + File 的內容
void
Directory::Print()
{ 
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse) {
            printf("Name: %s, Sector: %d\n", table[i].name, table[i].sector);
            hdr->FetchFrom(table[i].sector);
            // 23-0504[j]: 印出 File Header(FCB) & File 的所有內容
            hdr->Print();
        }
    printf("\n");
    delete hdr;
}

// 23-0510[j]: MP4

// 23-0511[j]: 根據 isfile 紀錄 new Entry 是 File 或 Directory
bool
Directory::Add(char *name, int newSector, int isfile)
{ 
    // 23-0504[j]: 若 name 在 Directory 中已經重複，則 return FALSE
    if (FindIndex(name) != -1)
	    return FALSE;

    // 23-0504[j]: 竟然 從頭找 未使用的 Entry，再將其分配給「名為 name 的 File」使用
    for (int i = 0; i < tableSize; i++)
        if (!table[i].inUse) {
            table[i].inUse = TRUE;
            // 23-0504[j]: 複製 name 的「前 9 個字元」到 table[i].name
            strncpy(table[i].name, name, FileNameMaxLen); 
            table[i].sector = newSector;
            table[i].isDir = isfile;
            return TRUE;
	    }
    return FALSE;	// no space.  Fix when we have extensible files.
}

// 23-0511[j]: 印出 n 的 ' ' 字元，是 printf() 的有趣應用
void PrintNBlanks(int n){
    printf("%*c",n,' ');
}

 
// 23-0511[j]: 將「某個目錄」以下的 table[i].name 印出
//             若 table[i].isDir = 1 則 再次呼叫 RecursiveList(cnt+3) 將其下元素印出
//             其中 cnt 表示 遞迴深度：遞迴越深，一開始要印出的空格越多 

void Directory::RecursiveList(int cnt)
{
    for (int i = 0; i < tableSize; i++){
        if (table[i].inUse){
            // 23-0511[j]: 先印出 cnt 個空格，在印出 table[i].name
            PrintNBlanks(cnt);
            printf("%s\n", table[i].name);

            // 23-0511[j]: 若 table[i] 是 Dir 則要將其「開啟」並 呼叫 RecursiveList(cnt+3) 往下印
            if(table[i].isDir){
                OpenFile* parentDirFile = new OpenFile(table[i].sector);
                Directory *directory = new Directory(NumDirEntries);
                directory->FetchFrom(parentDirFile);
                directory->RecursiveList(cnt+3);
                delete directory;
                delete parentDirFile;
            }
        }
    }
}
// 23-0511[j]: 在該層級的 Directory 下，刪除 所有的 File (Desllocate & Remove Entry)
//             注意：因刪除時 需要「收回空間」故必須傳遞「freeMap」，並在將來 WriteBack
void Directory::RecursiveRemove(PersistentBitmap* freeMap){

    OpenFile* dirFile;
    Directory *directory;
    FileHeader* fileHdr;

    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse){
            // 23-0511[j]: 若 table[i] 是 Dir 則要將其「開啟」並 呼叫 RecursiveRemove(..) 往下刪除
            if(table[i].isDir){
                dirFile = new OpenFile(table[i].sector);
                directory = new Directory(NumDirEntries);
                directory->FetchFrom(dirFile);
                directory->RecursiveRemove(freeMap);
                delete directory;
                delete dirFile;
            }
            // 23-0511[j]: 不管 table[i] 是 File 還是 Dir 都要被刪除
            fileHdr = new FileHeader;
            fileHdr->FetchFrom(table[i].sector);
            fileHdr->Deallocate(freeMap); 
            fileHdr->DeallocateHDR(freeMap,table[i].sector); 
            
            // 23-0511[j]: 注意！請 this(目錄) 將 table[i] 從自己身上「移除」
            Remove(table[i].name);
            delete fileHdr;
        }
}

// 23-0511[j]: MP4 判斷名為 name 的 File/Dir 是不是 Directory
int Directory::IsDirectory(char *name){
    int idx = FindIndex(name);
    if(idx < 0) return -1;
    else return table[idx].isDir;
}
