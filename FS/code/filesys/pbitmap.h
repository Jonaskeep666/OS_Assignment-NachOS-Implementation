// pbitmap.h 
//	Data structures defining a "persistent" bitmap -- a bitmap
//	that can be stored and fetched off of disk
//
//    A persistent bitmap can either be initialized from the disk
//    when it is created, or it can be initialized later using
//    the FetchFrom method
//
// Copyright (c) 1992,1993,1995 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef PBITMAP_H
#define PBITMAP_H

#include "copyright.h"
#include "bitmap.h"
#include "openfile.h"

// The following class defines a persistent bitmap.  It inherits all
// the behavior of a bitmap (see bitmap.h), adding the ability to
// be read from and stored to the disk.

// 23-0504[j]: Bitmap 會在 Memory 被建立，寫回 Disk 時，會存成一個 NachOS File，成為 Persistent Bitmap
//             預設 Free Sector Bitmap File 存在 Sector 0 = FreeMapSector


class PersistentBitmap : public Bitmap {
  public:
    PersistentBitmap(OpenFile *file,int numItems); //initialize bitmap from disk 
    PersistentBitmap(int numItems); // or don't...

    ~PersistentBitmap(); 			// deallocate bitmap

    void FetchFrom(OpenFile *file);     // read bitmap from the disk
    void WriteBack(OpenFile *file); 	// write bitmap contents to disk 
};

#endif // PBITMAP_H
