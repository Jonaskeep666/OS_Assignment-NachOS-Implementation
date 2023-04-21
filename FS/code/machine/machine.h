// machine.h 
//	Data structures for simulating the execution of user programs
//	running on top of Nachos.
//
//	User programs are loaded into "mainMemory"; to Nachos,
//	this looks just like an array of bytes.  Of course, the Nachos
//	kernel is in memory too -- but as in most machines these days,
//	the kernel is loaded into a separate memory region from user
//	programs, and accesses to kernel memory are not translated or paged.
//
//	In Nachos, user programs are executed one instruction at a time, 
//	by the simulator.  Each memory reference is translated, checked
//	for errors, etc.
//
//  DO NOT CHANGE EXCEPT AS NOTED BELOW -- part of the machine emulation

// 23-0419[j]: mipssim 是 模擬 MIPS CPU 的 指令集(ISA)
//             machine 是 模擬硬體(電腦)
//             RaiseException 是 模擬 CPU 的「例外處理電路」，並在此轉換 Kernel/User Mode

#ifndef MACHINE_H
#define MACHINE_H

#include "copyright.h"
#include "utility.h"
#include "translate.h"

// Definitions related to the size, and format of user memory

const int PageSize = 128; 		// set the page size equal to
								// the disk sector size, for simplicity
								// 23-0131[j]: 128 Bytes

//
// You are allowed to change this value.
// Doing so will change the number of pages of physical memory
// available on the simulated machine.
//
const int NumPhysPages = 128;

const int MemorySize = (NumPhysPages * PageSize);
const int TLBSize = 4;			// if there is a TLB, make it small

// 23-0419[j]: 此處定義 Exception 的類型，
//             其中包含了 SyscallException = 呼叫 System Call 
//             若呼叫 SysCall 會在 exception.cc 中以 switch case(type) 來判定是哪一種 SysCall

enum ExceptionType { NoException,           // Everything ok!
		     SyscallException,      // A program executed a system call.
		     PageFaultException,    // No valid translation found
		     ReadOnlyException,     // Write attempted to page marked 
					    // "read-only"
		     BusErrorException,     // Translation resulted in an 
					    // invalid physical address
		     AddressErrorException, // Unaligned reference or one that
					    // was beyond the end of the
					    // address space
		     OverflowException,     // Integer overflow in add or sub.
		     IllegalInstrException, // Unimplemented or reserved instr.
		     
		     NumExceptionTypes
};

// User program CPU state.  The full set of MIPS registers, plus a few
// more because we need to be able to start/stop a user program between
// any two instructions (thus we need to keep track of things like load
// delay slots, etc.)

#define StackReg	29	// User's stack pointer
#define RetAddrReg	31	// Holds return address for procedure calls
#define NumGPRegs	32	// 32 general purpose registers on MIPS
#define HiReg		32	// Double register to hold multiply result
#define LoReg		33
#define PCReg		34	// Current program counter
#define NextPCReg	35	// Next program counter (for branch delay) 
#define PrevPCReg	36	// Previous program counter (for debugging)
#define LoadReg		37	// The register target of a delayed load.
#define LoadValueReg 	38	// The value to be loaded by a delayed load.
#define BadVAddrReg	39	// The failing virtual address on an exception

// 23-0419[j]: NachOS 的模擬 CPU 有40個 Register
#define NumTotalRegs 	40 

// The following class defines the simulated host workstation hardware, as 
// seen by user programs -- the CPU registers, main memory, etc.
// User programs shouldn't be able to tell that they are running on our 
// simulator or on the real hardware, except 
//	we don't support floating point instructions
//	the system call interface to Nachos is not the same as UNIX 
//	  (10 system calls in Nachos vs. 200 in UNIX!)
// If we were to implement more of the UNIX system calls, we ought to be
// able to run Nachos on top of Nachos!
//
// The procedures in this class are defined in machine.cc, mipssim.cc, and
// translate.cc.

class Instruction;
class Interrupt;

class Machine {
  public:
    Machine(bool debug);	// Initialize the simulation of the hardware
				// for running user programs
    ~Machine();			// De-allocate the data structures

// Routines callable by the Nachos kernel
    void Run();	 		// Run a user program

    int ReadRegister(int num);	// read the contents of a CPU register

    void WriteRegister(int num, int value);
				// store a value into a CPU register

// Data structures accessible to the Nachos kernel -- main memory and the
// page table/TLB.
//
// Note that *all* communication between the user program and the kernel 
// are in terms of these data structures (plus the CPU registers).

// 23-0127[j]: 模擬的主記憶體，用一個 char指標 指向「模擬的主記憶體空間」
    char *mainMemory;		// physical memory to store user program,
							// code and data, while executing

// NOTE: the hardware translation of virtual addresses in the user program
// to physical addresses (relative to the beginning of "mainMemory")
// can be controlled by one of:
//	a traditional linear page table
//  	a software-loaded translation lookaside buffer (tlb) -- a cache of 
//	  mappings of virtual page #'s to physical page #'s
//
// If "tlb" is NULL, the linear page table is used
// If "tlb" is non-NULL, the Nachos kernel is responsible for managing
//	the contents of the TLB.  But the kernel can use any data structure
//	it wants (eg, segmented paging) for handling TLB cache misses.
// 
// For simplicity, both the page table pointer and the TLB pointer are
// public.  However, while there can be multiple page tables (one per address
// space, stored in memory), there is only one TLB (implemented in hardware).
// Thus the TLB pointer should be considered as *read-only*, although 
// the contents of the TLB are free to be modified by the kernel software.

// 23-0127[j]: 將 Process 的虛擬記憶體 轉成 實體記憶體  有兩種方案
//             a.	傳統的單一 Page Table (若 TLB 沒啟用(NULL))
//             b.	軟體模擬的 TLB (若 TLB 啟用(non-NULL))
//             TLB pointer 為 *read-only*，除非 Kernel 要清空 TLB

//             實作重點：Kernel 可以也可以採用 segmented paging 的方式管理記憶體

    TranslationEntry *tlb;		// this pointer should be considered 
								// "read-only" to Nachos kernel code

    TranslationEntry *pageTable;
    unsigned int pageTableSize;

    bool ReadMem(int addr, int size, int* value);
    bool WriteMem(int addr, int size, int value);
    			// Read or write 1, 2, or 4 bytes of virtual 
				// memory (at addr).  Return FALSE if a 
				// correct translation couldn't be found.
  private:

// Routines internal to the machine simulation -- DO NOT call these directly
    void DelayedLoad(int nextReg, int nextVal);  	
				// Do a pending delayed load (modifying a reg)

// 23-0419[j]: 模擬 CPU 執行一道指令
    void OneInstruction(Instruction *instr); 	
    				// Run one instruction of a user program.
    

// 23-0127[j]: 翻譯 virtAddr -> physAddr 的函數，在translate.cc那邊實作
    ExceptionType Translate(int virtAddr, int* physAddr, int size,bool writing);
    				// Translate an address, and check for 
					// alignment.  Set the use and dirty bits in 
					// the translation entry appropriately,
    				// and return an exception code if the 
					// translation couldn't be completed.

// 23-0419[j]: 在 mipssim.cc 實作
    void RaiseException(ExceptionType which, int badVAddr);
				// Trap to the Nachos kernel, because of a
				// system call or other exception.  

    void Debugger();		// invoke the user program debugger
    void DumpState();		// print the user CPU and memory state 


// Internal data structures

    int registers[NumTotalRegs]; // CPU registers, for executing user programs

    bool singleStep;		// drop back into the debugger after each
				// simulated instruction
    int runUntilTime;		// drop back into the debugger when simulated
				// time reaches this value

    friend class Interrupt;		// calls DelayedLoad()    
};

extern void ExceptionHandler(ExceptionType which);
				// Entry point into Nachos for handling
				// user system calls and exceptions
				// Defined in exception.cc


// Routines for converting Words and Short Words to and from the
// simulated machine's format of little endian.  If the host machine
// is little endian (DEC and Intel), these end up being NOPs.
//
// What is stored in each format:
//	host byte ordering:
//	   kernel data structures
//	   user registers
//	simulated machine byte ordering:
//	   contents of main memory

unsigned int WordToHost(unsigned int word);
unsigned short ShortToHost(unsigned short shortword);
unsigned int WordToMachine(unsigned int word);
unsigned short ShortToMachine(unsigned short shortword);

#endif // MACHINE_H
