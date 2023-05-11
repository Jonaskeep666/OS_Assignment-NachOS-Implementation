// bitmap.cc
//	Routines to manage a bitmap -- an array of bits each of which
//	can be either on or off.  Represented as an array of integers.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "bitmap.h"

//----------------------------------------------------------------------
// BitMap::BitMap
// 	Initialize a bitmap with "numItems" bits, so that every bit is clear.
//	it can be added somewhere on a list.
//
//	"numItems" is the number of bits in the bitmap.
//----------------------------------------------------------------------
// 23-0503[j]: 主要功能：建立 numItems 個 bits 的 Bitmap，並設定初始值「全為0」
Bitmap::Bitmap(int numItems) 
{ 
    int i;

    ASSERT(numItems > 0);

    numBits = numItems;
    // 23-0503[j]: Words 的個數採「無條件進位」
    numWords = divRoundUp(numBits, BitsInWord);
    // 23-0503[j]: 指標 map 指向「unsigned int 陣列」包含 numWords 個元素(至少包含 numBits 個位元)
    map = new unsigned int[numWords];

    // 23-0503[j]: 初始化 所有位元值 = 0
    for (i = 0; i < numWords; i++) {
	    map[i] = 0;		// initialize map to keep Purify happy
    }
    for (i = 0; i < numBits; i++) {
        Clear(i);
    }
}

//----------------------------------------------------------------------
// Bitmap::~Bitmap
// 	De-allocate a bitmap.
//----------------------------------------------------------------------

Bitmap::~Bitmap()
{ 
    delete map;
}

//----------------------------------------------------------------------
// Bitmap::Set
// 	Set the "nth" bit in a bitmap.
//
//	"which" is the number of the bit to be set.
//----------------------------------------------------------------------
// 23-0503[j]: 主要功能：設定「第 which 個位元」= 1
void
Bitmap::Mark(int which) 
{ 
    ASSERT(which >= 0 && which < numBits);

    // 23-0503[j]: (1)  運算子順序："<<" 優先於 "|"
    //             (2)  先將 1 左移 (which % 32)
    //             (3)  再將 map[which/32] | 0000...1...0 等同設定 bit which
    map[which / BitsInWord] |= 1 << (which % BitsInWord);

    ASSERT(Test(which));
}
    
//----------------------------------------------------------------------
// Bitmap::Clear
// 	Clear the "nth" bit in a bitmap.
//
//	"which" is the number of the bit to be cleared.
//----------------------------------------------------------------------
// 23-0503[j]: 主要功能：設定「第 which 個位元」= 0
void 
Bitmap::Clear(int which) 
{
    ASSERT(which >= 0 && which < numBits);

    map[which / BitsInWord] &= ~(1 << (which % BitsInWord));

    ASSERT(!Test(which));
}

//----------------------------------------------------------------------
// Bitmap::Test
// 	Return TRUE if the "nth" bit is set.
//
//	"which" is the number of the bit to be tested.
//----------------------------------------------------------------------
// 23-0503[j]: 主要功能：若「第 which 個位元」= 1 則 return TRUE；否則 return FALSE
bool 
Bitmap::Test(int which) const
{
    ASSERT(which >= 0 && which < numBits);
    
    if (map[which / BitsInWord] & (1 << (which % BitsInWord))) {
	    return TRUE;
    } else {
	    return FALSE;
    }
}

//----------------------------------------------------------------------
// Bitmap::FindAndSet
// 	Return the number of the first bit which is clear.
//	As a side effect, set the bit (mark it as in use).
//	(In other words, find and allocate a bit.)
//
//	If no bits are clear, return -1.
//----------------------------------------------------------------------
/*
// 23-0503[j]: int FindAndSet()
-	主要功能：Pop out/Set Free Bit
    -	尋找 & 設定「首個 為0 位元」= 1 (Side effect 才是目的)，並 return Bit # (Pop out Free Bit)
    -	若沒有「為0位元」則 return -1 (沒有 Free Bit)
*/
int 
Bitmap::FindAndSet() 
{
    for (int i = 0; i < numBits; i++) {
        if (!Test(i)) {
            Mark(i);
            return i;
        }
    }
    return -1;
}

//----------------------------------------------------------------------
// Bitmap::NumClear
// 	Return the number of clear bits in the bitmap.
//	(In other words, how many bits are unallocated?)
//----------------------------------------------------------------------
// 23-0503[j]: 主要功能：return 為0位元 的個數 (Num of Clear Bits)
int 
Bitmap::NumClear() const
{
    int count = 0;

    for (int i = 0; i < numBits; i++) {
        if (!Test(i)) {
            count++;
        }
    }
    return count;
}

//----------------------------------------------------------------------
// Bitmap::Print
// 	Print the contents of the bitmap, for debugging.
//
//	Could be done in a number of ways, but we just print the #'s of
//	all the bits that are set in the bitmap.
//----------------------------------------------------------------------
// 23-0503[j]: 印出 哪些位元，其值 = 1 (ex. 0,3,5,6 表示 0110,1001)
void
Bitmap::Print() const
{
    cout << "Bitmap set:\n"; 
    for (int i = 0; i < numBits; i++) {
        if (Test(i)) {
            cout << i << ", ";
        }
    }
    cout << "\n"; 
}


//----------------------------------------------------------------------
// Bitmap::SelfTest
// 	Test whether this module is working.
//----------------------------------------------------------------------

void
Bitmap::SelfTest() 
{
    int i;
    
    ASSERT(numBits >= BitsInWord);	// bitmap must be big enough

    ASSERT(NumClear() == numBits);	// bitmap must be empty
    ASSERT(FindAndSet() == 0);
    Mark(31);
    ASSERT(Test(0) && Test(31));

    ASSERT(FindAndSet() == 1);
    Clear(0);
    Clear(1);
    Clear(31);

    for (i = 0; i < numBits; i++) {
        Mark(i);
    }
    ASSERT(FindAndSet() == -1);		// bitmap should be full!
    for (i = 0; i < numBits; i++) {
        Clear(i);
    }
}
