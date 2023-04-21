// translate.cc 
//	Routines to translate virtual addresses to physical addresses.
//	Software sets up a table of legal translations.  We look up
//	in the table on every memory reference to find the true physical
//	memory location.
//
//  Two types of translation are supported here.
//
//	Linear page table -- the virtual page # is used as an index
//	into the table, to find the physical page #.
//
//	Translation lookaside buffer(TLB) -- associative lookup in the table
//	to find an entry with the same virtual page #.  If found,
//	this entry is used for the translation.
//	If not, it traps to software with an exception. 
//
//	In practice, the TLB is much smaller than the amount of physical
//	memory (16 entries is common on a machine that has 1000's of
//	pages).  Thus, there must also be a backup translation scheme
//	(such as page tables), but the hardware doesn't need to know
//	anything at all about that.
//
//	Note that the contents of the TLB are specific to an address space.
//	If the address space changes, so does the contents of the TLB!
//
// DO NOT CHANGE -- part of the machine emulation
//

#include "copyright.h"
#include "main.h"

// Routines for converting Words and Short Words to and from the
// simulated machine's format of little endian.  These end up
// being NOPs when the host machine is also little endian (DEC and Intel).

unsigned int
WordToHost(unsigned int word) {
#ifdef HOST_IS_BIG_ENDIAN
	 register unsigned long result;
	 result = (word >> 24) & 0x000000ff;
	 result |= (word >> 8) & 0x0000ff00;
	 result |= (word << 8) & 0x00ff0000;
	 result |= (word << 24) & 0xff000000;
	 return result;
#else 
	 return word;
#endif /* HOST_IS_BIG_ENDIAN */
}

unsigned short
ShortToHost(unsigned short shortword) {
#ifdef HOST_IS_BIG_ENDIAN
	 register unsigned short result;
	 result = (shortword << 8) & 0xff00;
	 result |= (shortword >> 8) & 0x00ff;
	 return result;
#else 
	 return shortword;
#endif /* HOST_IS_BIG_ENDIAN */
}

unsigned int
WordToMachine(unsigned int word) { return WordToHost(word); }

unsigned short
ShortToMachine(unsigned short shortword) { return ShortToHost(shortword); }


//----------------------------------------------------------------------
// Machine::ReadMem
//      Read "size" (1, 2, or 4) bytes of virtual memory at "addr" into 
//	the location pointed to by "value".
//
//   	Returns FALSE if the translation step from virtual to physical memory
//   	failed.
//
//	"addr" -- the virtual address to read from
//	"size" -- the number of bytes to read (1, 2, or 4)
//	"value" -- the place to write the result
//----------------------------------------------------------------------
// 22-1223[j]: boot ReadMem(int addr, int size, int *value) 將 Memory 中 addr+size 上的資料，存入 *value

bool
Machine::ReadMem(int addr, int size, int *value)
{
    int data;
    ExceptionType exception;
    int physicalAddress;
    
    DEBUG(dbgAddr, "Reading VA " << addr << ", size " << size);
    
    exception = Translate(addr, &physicalAddress, size, FALSE);
    if (exception != NoException) {
		RaiseException(exception, addr);
		return FALSE;
    }
    switch (size) {
      case 1:
		// 22-1223[j]: 取出 主記憶體中的資料(1 byte) 存入 data (因為 mainMemory 的型態是 char *，直接取值 就取 1 byte)
		data = mainMemory[physicalAddress]; 
		*value = data;
		break;
	
      case 2:
		// 22-1223[j]: 將陣列元素位址 轉型為「指向 無號short的指標」再取值(2 byte) 存入 data
		data = *(unsigned short *) &mainMemory[physicalAddress]; 
		*value = ShortToHost(data);
		break;
	
      case 4:
		data = *(unsigned int *) &mainMemory[physicalAddress];
		*value = WordToHost(data);
		break;

      default: ASSERT(FALSE);
    }
    
    DEBUG(dbgAddr, "\tvalue read = " << *value);
    return (TRUE);
}

//----------------------------------------------------------------------
// Machine::WriteMem
//      Write "size" (1, 2, or 4) bytes of the contents of "value" into
//	virtual memory at location "addr".
//
//   	Returns FALSE if the translation step from virtual to physical memory
//   	failed.
//
//	"addr" -- the virtual address to write to
//	"size" -- the number of bytes to be written (1, 2, or 4)
//	"value" -- the data to be written
//----------------------------------------------------------------------

bool
Machine::WriteMem(int addr, int size, int value)
{
    ExceptionType exception;
    int physicalAddress;
     
    DEBUG(dbgAddr, "Writing VA " << addr << ", size " << size << ", value " << value);

    exception = Translate(addr, &physicalAddress, size, TRUE);
    if (exception != NoException) {
		RaiseException(exception, addr);
		return FALSE;
    }
    switch (size) {
      case 1:
		mainMemory[physicalAddress] = (unsigned char) (value & 0xff);
		break;

      case 2:
		*(unsigned short *) &mainMemory[physicalAddress]
			= ShortToMachine((unsigned short) (value & 0xffff));
	  	break;
      
      case 4:
		*(unsigned int *) &mainMemory[physicalAddress]
			= WordToMachine((unsigned int) value);
		break;
	
      default: ASSERT(FALSE);
    }
    
    return TRUE;
}

//----------------------------------------------------------------------
// Machine::Translate
// 	Translate a virtual address into a physical address, using 
//	either a page table or a TLB.  Check for alignment and all sorts 
//	of other errors, and if everything is ok, set the use/dirty bits in 
//	the translation table entry, and store the translated physical 
//	address in "physAddr".  If there was an error, returns the type
//	of the exception.
//
//	"virtAddr" -- the virtual address to translate
//	"physAddr" -- the place to store the physical address
//	"size" -- the amount of memory being read or written
// 	"writing" -- if TRUE, check the "read-only" bit in the TLB
//----------------------------------------------------------------------
// 23-0127[j]: 實作 2種翻譯方案的地方(Page Table 或 TLB)
//             檢查是否異常 & 翻譯位址 存入 *physAddr
// 23-0131[j]: 每次使用 WriteMem(..) & ReadMem(..) 都會呼叫到


ExceptionType
Machine::Translate(int virtAddr, int* physAddr, int size, bool writing)
{
    int i;
    unsigned int vpn, offset;
    TranslationEntry *entry;	// 23-0127[j]: 指向 virtAddr 所在的 Page Entry
    unsigned int pageFrame;

    DEBUG(dbgAddr, "\tTranslate " << virtAddr << (writing ? " , write" : " , read"));

// 23-0127[j]: 這是什麼???
// check for alignment errors
    if (((size == 4) && (virtAddr & 0x3)) || ((size == 2) && (virtAddr & 0x1))){
		DEBUG(dbgAddr, "Alignment problem at " << virtAddr << ", size " << size);
		return AddressErrorException;
    }
    // we must have either a TLB or a page table, but not both!
	// 23-0127[j]: ??? 為什麼不能兩個都有？
    ASSERT(tlb == NULL || pageTable == NULL);	
    ASSERT(tlb != NULL || pageTable != NULL);	

// calculate the virtual page number, and offset within the page,
// from the virtual address
    vpn = (unsigned) virtAddr / PageSize;		// 23-0127[j]: vpn = Page Num(p)
    offset = (unsigned) virtAddr % PageSize;	// 23-0127[j]: offset(d)

	// 23-0127[j]: 採用 page table
    if (tlb == NULL) {		// => page table => vpn is index into table
		if (vpn >= pageTableSize) {
			DEBUG(dbgAddr, "Illegal virtual page # " << virtAddr);
			return AddressErrorException;
		} else if (!pageTable[vpn].valid) {
			DEBUG(dbgAddr, "Invalid virtual page # " << virtAddr);
			return PageFaultException;
		}
		entry = &pageTable[vpn];	// 23-0127[j]: entry 指向 Page table entry 的 實體位址

    } 
	// 23-0127[j]: 採用 TLB，並逐一搜尋 TLB Entry (若有硬體支援，可在O(1)搜尋完畢)
	else {				
        for (entry = NULL, i = 0; i < TLBSize; i++)
			// 23-0127[j]: 若 tlb[i] = Valid 
			//             且 Page Num(virtualPage)等於 virtAddr所在Page 的 Page Num(p)
			//             則將 entry 指向 tlb entry 的 實體位址
    	    if (tlb[i].valid && (tlb[i].virtualPage == ((int)vpn))) {
				entry = &tlb[i];			// TLB hit!
				break;
			}
		if (entry == NULL) {				// TLB miss!
			DEBUG(dbgAddr, "Invalid TLB entry for this virtual page!");
			return PageFaultException;		// really, this is a TLB fault,
											// the page may be in memory,
											// but not in the TLB
		}
    }

    if (entry->readOnly && writing) {	// trying to write to a read-only page
		DEBUG(dbgAddr, "Write to read-only page at " << virtAddr);
		return ReadOnlyException;
    }
	// 23-0127[j]: pageFrame = Page Num 對應的 Frame Num
    pageFrame = entry->physicalPage;	

    // if the pageFrame is too big, there is something really wrong! 
    // An invalid translation was loaded into the page table or TLB. 
    if (pageFrame >= NumPhysPages) { 
		DEBUG(dbgAddr, "Illegal pageframe " << pageFrame);
		return BusErrorException;
    }
	// 23-0127[j]: 設定 use(Ref. bit) & dirty(Dirty bit)
    entry->use = TRUE;		// set the use, dirty bits
    if (writing) entry->dirty = TRUE;

	// 23-0127[j]: 翻譯位址，譯後位址(*physAddr) = pageFrame(f) * PageSize + offset	
    *physAddr = pageFrame * PageSize + offset;

    ASSERT((*physAddr >= 0) && ((*physAddr + size) <= MemorySize));
    DEBUG(dbgAddr, "phys addr = " << *physAddr);
    return NoException;
}
