// pbitmap.c 
//	Routines to manage a persistent bitmap -- a bitmap that is
//	stored on disk.
//
// Copyright (c) 1992,1993,1995 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "pbitmap.h"

//----------------------------------------------------------------------
// PersistentBitmap::PersistentBitmap(int)
// 	Initialize a bitmap with "numItems" bits, so that every bit is clear.
//	it can be added somewhere on a list.
//
//	"numItems" is the number of bits in the bitmap.
//
//      This constructor does not initialize the bitmap from a disk file
//----------------------------------------------------------------------

PersistentBitmap::PersistentBitmap(int numItems):Bitmap(numItems) 
{ 
}

//----------------------------------------------------------------------
// PersistentBitmap::PersistentBitmap(OpenFile*,int)
// 	Initialize a persistent bitmap with "numItems" bits,
//      so that every bit is clear.
//
//	"numItems" is the number of bits in the bitmap.
//      "file" refers to an open file containing the bitmap (written
//        by a previous call to PersistentBitmap::WriteBack
//
//      This constructor initializes the bitmap from a disk file
//----------------------------------------------------------------------
/*
// 23-0504[j]: 有2種 建立 Bitmap 的方式
1.	在 Memory 中，動態配置一個 Bitmap (in RAM) = Bitmap(int numItems)
    = PersistentBitmap(int)

2.	假設已開啟「已存在 Disk」的「**Bitmap File**」(NachOS 開機時 即會開啟)
    = PersistentBitmap(OpenFile*,int)
    -   Bitmap File 存在 Sector 0
    -	file->ReadAt((char *)map, numWords * sizeof(unsigned), 0);
        從 position = 0 處開始讀取，將「numWords * 4」Bytes 的資料 存入 map 指向的空間
*/
PersistentBitmap::PersistentBitmap(OpenFile *file, int numItems):Bitmap(numItems) 
{ 
    // map has already been initialized by the BitMap constructor,
    // but we will just overwrite that with the contents of the
    // map found in the file
    file->ReadAt((char *)map, numWords * sizeof(unsigned), 0);
}

//----------------------------------------------------------------------
// PersistentBitmap::~PersistentBitmap
// 	De-allocate a persistent bitmap.
//----------------------------------------------------------------------

PersistentBitmap::~PersistentBitmap()
{ 
}

//----------------------------------------------------------------------
// PersistentBitmap::FetchFrom
// 	Initialize the contents of a persistent bitmap from a Nachos file.
//
//	"file" is the place to read the bitmap from
//----------------------------------------------------------------------
// 23-0504[j]: 主要功能：從 File (in Disk) 中讀出 Bitmap，存入 map 指向的空間
void
PersistentBitmap::FetchFrom(OpenFile *file) 
{
    file->ReadAt((char *)map, numWords * sizeof(unsigned), 0);
}

//----------------------------------------------------------------------
// PersistentBitmap::WriteBack
// 	Store the contents of a persistent bitmap to a Nachos file.
//
//	"file" is the place to write the bitmap to
//----------------------------------------------------------------------
// 23-0504[j]: 將 map 指向的空間 = 修改的 Bitmap 存入 File (in Disk) 中
void
PersistentBitmap::WriteBack(OpenFile *file)
{
   file->WriteAt((char *)map, numWords * sizeof(unsigned), 0);
}
