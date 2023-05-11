// thread.h 
//	Data structures for managing threads.  A thread represents
//	sequential execution of code within a program.
//	So the state of a thread includes the program counter,
//	the processor registers, and the execution stack.
//	
// 	Note that because we allocate a fixed size stack for each
//	thread, it is possible to overflow the stack -- for instance,
//	by recursing to too deep a level.  The most common reason
//	for this occuring is allocating large data structures
//	on the stack.  For instance, this will cause problems:
//
//		void foo() { int buf[1000]; ...}
//
//	Instead, you should allocate all data structures dynamically:
//
//		void foo() { int *buf = new int[1000]; ...}
//
//
// 	Bad things happen if you overflow the stack, and in the worst 
//	case, the problem may not be caught explicitly.  Instead,
//	the only symptom may be bizarre segmentation faults.  (Of course,
//	other problems can cause seg faults, so that isn't a sure sign
//	that your thread stacks are too small.)
//	
//	One thing to try if you find yourself with seg faults is to
//	increase the size of thread stack -- ThreadStackSize.
//
//  	In this interface, forking a thread takes two steps.
//	We must first allocate a data structure for it: "t = new Thread".
//	Only then can we do the fork: "t->fork(f, arg)".
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef THREAD_H
#define THREAD_H

#include "copyright.h"
#include "utility.h"
#include "sysdep.h"
#include "machine.h"
#include "addrspace.h"


// CPU register state to be saved on context switch.  
// The x86 needs to save only a few registers, 
// SPARC and MIPS needs to save 10 registers, 
// the Snake needs 18,
// and the RS6000 needs to save 75 (!)
// For simplicity, I just take the maximum over all architectures.

#define MachineStateSize 75 

// 23-0131[j]: 比較 addrspace.h 定義的 UserStackSize(1024) & thread.h 定義的 StackSize(8*1024)
//             (1)	UserStackSize(1024)
//                  -	是在 AddrSpace::Load() 中，配置給 User Program 使用的 User Stack
//                  -	其對應的實體空間是「模擬的」實體記憶體 machine->mainMemory[]
//                  -	在 ForkExecute(..) -> machine->Run() 真正執行 ProgramFile 的內容時，才使用

//             (2)	StackSize(8*1024)
//                  -	是在 StackAllocate() 中，配置給 Thread 用於「函數轉換」的 Kernel Stack
//                  -	其對應的實體空間是「Host的」實體記憶體
//                  -	在 Kernel 端，針對 Host 系統 作 Context Switch(SWITCH)、流程控制(ThreadRoot)時，會用到

// Size of the thread's private execution stack.
// WATCH OUT IF THIS ISN'T BIG ENOUGH!!!!!
const int StackSize = (8 * 1024);	// in words // 23-0131[j]: 因為 sizeof(int) = 4 Bytes


// Thread state
enum ThreadStatus { JUST_CREATED, RUNNING, READY, BLOCKED, ZOMBIE };

// 23-0306[j]:  MP3 新增 class Burst 用來紀錄每個 Thread 的時間 
// 23-0305[j]:  計算 new預測時間(NextBurst) = 0.5 x 實際Burst Time(NowBurst) + 0.5 x old預測時間(NextBurst)
class Burst{
  public:
    Burst();  
    ~Burst();

    void Start(int uTick) { start = (double)uTick;}
    void SetAccumWait(int uTick) { accumWaitTime = uTick;}
    
    double PauseNow(int uTick);       //  if(!now) now = end-start else now = end-start + now 
    double UpdateNext();              //  next = 0.5now + 0.5next
    void ResetNow() {now = 0.0;}        // 23-0303[j]:  本次 預測時間(NextBurst) 計算完畢，本次 實際Burst Time 歸零

    int GetAccumWait() { return accumWaitTime;}
    double getTotal(){return total;}  
    double getNow(){return now;}      
    double getNext(){return next;}    

  private:
    // 23-0306[j]:  開始執行時間(start)、結束執行時間(end)、實際Burst Time(NowBurst)、預測時間(NextBurst)
    double start;
    double end;
    double now;
    double next;

    // 23-0306[j]:  accumWaitTime = 從上次 Aging 調整以後，待在系統未執行的時間總和(Wait + Ready 的時間和)
    //              = TotalTicks - Running Time(NowBurst) - 「以前」未執行時間總和
    //              實際計算 見 Scheduler::Aging()
    int accumWaitTime;  

    // 23-0306[j]:  total = 每一段「實際Burst Time(NowBurst)」的總和 = Running Time
    double total;
};

// The following class defines a "thread control block" -- which
// represents a single thread of execution.
//
//  Every thread has:
//     an execution stack for activation records ("stackTop" and "stack")
//     space to save CPU registers while not running ("machineState")
//     a "status" (running/ready/blocked)
//    
//  Some threads also belong to a user address space; threads
//  that only run in the kernel have a NULL address space.

// 23-0127[j]: 建立的 Thread 物件 = Thread Control Block = 儲存 Thread 所需的資訊
//             所有的 Thread 都有
//             (1)	stack/stackTop	// Kernel Stack空間的 起點 & 終點，兩者撐開整個空間
//                  -	stack 指向低位址
//                  -	stackTop 指向高位址
//                  -	注意!!!! Stack 從 高位址往下長
//             (2)	machineState	  // Context Switch 時 儲存 Host CPU registers的空間
//             (3)	status			    // Thread 狀態(running/ready/blocked)

// 23-0130[j]: 詳見 MP2_Note
class Thread {
  private:
    // NOTE: DO NOT CHANGE the order of these first two members.
    // THEY MUST be in this position for SWITCH to work.
    int *stackTop;			 // the current stack pointer
    void *machineState[MachineStateSize];  // all registers except for stackTop

  public:
    Thread(char* debugName, int threadID);		// initialize a Thread 
    ~Thread(); 				// deallocate a Thread
					// NOTE -- thread being deleted
					// must not be running when delete 
					// is called

    // basic thread operations

    void Fork(VoidFunctionPtr func, void *arg); 
    				// Make thread run (*func)(arg)
    void Yield();  		// Relinquish the CPU if any other thread is runnable
    void Sleep(bool finishing); // Put the thread to sleep and relinquish the processor
    void Begin();		// Startup code for the thread	
    void Finish();  		// The thread is done executing
    
    void CheckOverflow();   	// Check if thread stack has overflowed
    
    void setStatus(ThreadStatus st) { status = st; }
    ThreadStatus getStatus() { return (status); }
	  char* getName() { return (name); }
	  int getID() { return (ID); }

    // 23-0302[j]: MP3
    int getPriority() { return (priority); }
    void setPriority(int p) { priority = p; }

    void Print() { cout << name; }
    void SelfTest();		// test whether thread impl is working



  private:
    // some of the private data for this class is listed above
    
    int *stack; 	 	// Bottom of the stack 
				// NULL if this is the main thread
				// (If NULL, don't deallocate stack)
    ThreadStatus status;	// ready, running or blocked
    char* name;
	  int   ID;
    void StackAllocate(VoidFunctionPtr func, void *arg);
    				// Allocate a stack for thread.
				// Used internally by Fork()
    
    // 23-0302[j]: MP3 新增
    int priority;

// A thread running a user program actually has *two* sets of CPU registers -- 
// one for its state while executing user code, one for its state 
// while executing kernel code.

    int userRegisters[NumTotalRegs];	// user-level CPU register state

  public:
    void SaveUserState();		// save user-level register state
    void RestoreUserState();		// restore user-level register state

    AddrSpace *space;			// User code this thread is running. 
                          // 23-0127[j]: 指向 AddrSpace物件 = Page Table

    // 23-0303[j]:  計算 預測時間(NextBurst) 的物件，在 Kernel::Exec(char* name,int pry) new 出來
    Burst *busrt;
};

// external function, dummy routine whose sole job is to call Thread::Print
extern void ThreadPrint(Thread *thread);	 

// Magical machine-dependent routines, defined in switch.s
// 23-0127[j]: 當 Thread Context Switch 時，會用到以下函數
//             其內容在 switch.s
//             因其實作在 switch.S，故呼叫後，會直接跳至switch.S執行
//             原因：Context Switch 與硬體高度相關(為了要快)，所以用組語寫
extern "C" {
  // First frame on thread execution stack; 
  //   	call ThreadBegin
  //	call "func"
  //	(when func returns, if ever) call ThreadFinish()

  // 23-0127[j]: ThreadRoot(int InitialPC, int InitialArg, int WhenDonePC, int StartupPC)
  //             此函數會採用 switch.h 定義的暫存器資料
  //             InitialPC = %esi、InitialArg = %edx、WhenDonePC = %edi、StartupPC = %ecx
  //             主要功能：

  void ThreadRoot();

  // Stop running oldThread and start running newThread
  void SWITCH(Thread *oldThread, Thread *newThread);
}

#endif // THREAD_H
