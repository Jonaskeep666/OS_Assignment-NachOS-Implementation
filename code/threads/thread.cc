// thread.cc 
//	Routines to manage threads.  These are the main operations:
//
//	Fork -- create a thread to run a procedure concurrently
//		with the caller (this is done in two steps -- first
//		allocate the Thread object, then call Fork on it)
//	Begin -- called when the forked procedure starts up, to turn
//		interrupts on and clean up after last thread
//	Finish -- called when the forked procedure finishes, to clean up
//	Yield -- relinquish control over the CPU to another ready thread
//	Sleep -- relinquish control over the CPU, but thread is now blocked.
//		In other words, it will not run again, until explicitly 
//		put back on the ready queue.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "thread.h"
#include "switch.h"
#include "synch.h"
#include "sysdep.h"

// this is put at the top of the execution stack, for detecting stack overflows
// 23-0127[j]: 又來一個 Magic Num 用來識別 Stack是不是滿了
const int STACK_FENCEPOST = 0xdedbeef;

//----------------------------------------------------------------------
// Thread::Thread
// 	Initialize a thread control block, so that we can then call
//	Thread::Fork.
//
//	"threadName" is an arbitrary string, useful for debugging.
//----------------------------------------------------------------------

Thread::Thread(char* threadName, int threadID)
{
	ID = threadID;
    name = threadName;
    stackTop = NULL;
    stack = NULL;
    status = JUST_CREATED;
    for (int i = 0; i < MachineStateSize; i++) {
        machineState[i] = NULL;		// not strictly necessary, since
                                    // new thread ignores contents 
                                    // of machine registers
    }
    space = NULL;

    // 23-0306[j]:  MP3 新增
    busrt = new Burst();
    
}

//----------------------------------------------------------------------
// Thread::~Thread
// 	De-allocate a thread.
//
// 	NOTE: the current thread *cannot* delete itself directly,
//	since it is still running on the stack that we need to delete.
//
//      NOTE: if this is the main thread, we can't delete the stack
//      because we didn't allocate it -- we got it automatically
//      as part of starting up Nachos.
//----------------------------------------------------------------------

Thread::~Thread()
{
    DEBUG(dbgThread, "Deleting thread: " << name);
    ASSERT(this != kernel->currentThread);
    if (stack != NULL)
	    DeallocBoundedArray((char *) stack, StackSize * sizeof(int));

    // delete busrt;
}

//----------------------------------------------------------------------
// Thread::Fork
// 	Invoke (*func)(arg), allowing caller and callee to execute 
//	concurrently.
//
//	NOTE: although our definition allows only a single argument
//	to be passed to the procedure, it is possible to pass multiple
//	arguments by making them fields of a structure, and passing a pointer
//	to the structure as "arg".
//
// 	Implemented as the following steps:
//		1. Allocate a stack
//		2. Initialize the stack so that a call to SWITCH will
//		    cause it to run the procedure
//		3. Put the thread on the ready queue
// 	
//	"func" is the procedure to run concurrently.
//	"arg" is a single argument to be passed to the procedure.
//----------------------------------------------------------------------
// 23-0127[j]: Fork (VoidFunctionPtr func, void *arg)
//             傳入參數
//             (1)  VoidFunctionPtr func 是一個指向「1個 傳入參數」的函數 = void (*funPtr)(void *arg)
//             (2)  void *arg 是一個指向未定型態的指標
//                  可以透過 傳遞「打包好的Struct」來傳遞多個變數
//             主要功能
//             (1)	call StackAllocate(..)
//             	    -	幫 Thread 配置 Stack空間 & 初始化 Stack空間(將 ThreadRoot位址 Push 到 Stack中)
//             	    -	指定將來要執行的函數(func) & 參數(arg)
//             (2)	將此Thread 排進 ReadyQueue

// 23-0128[j]: Note
//             (1)  這個 Fork 僅用來配置/初始化 這個 Thread 的 Stack，並將其排入 ReadyQueue
//                  表示 此Fork() 與 課本Fork()的不同之處在於「後者會自動產生 Child Thread；前者只是初始化 Stack」
//             (2)  在 NachOS 要 Create Child Threat 的流程
//                  Thread* newThread = new Thread("New Thread");   // new 一個 Thread物件 = 有了 TCB(PCB)
//                  newThread->Fork(ThreadFunc,ThreadFuncArg);      
//                  // 配置 Stack空間、排入 Ready Queue、指定未來要執行的函數(func(arg))

void 
Thread::Fork(VoidFunctionPtr func, void *arg)
{
    Interrupt *interrupt = kernel->interrupt;
    Scheduler *scheduler = kernel->scheduler;
    IntStatus oldLevel;
    
    DEBUG(dbgThread, "Forking thread: " << name << " f(a): " << (int) func << " " << arg);
    StackAllocate(func, arg);

    oldLevel = interrupt->SetLevel(IntOff);
    scheduler->ReadyToRun(this);	// ReadyToRun assumes that interrupts are disabled!
    (void) interrupt->SetLevel(oldLevel);
}    

//----------------------------------------------------------------------
// Thread::CheckOverflow
// 	Check a thread's stack to see if it has overrun the space
//	that has been allocated for it.  If we had a smarter compiler,
//	we wouldn't need to worry about this, but we don't.
//
// 	NOTE: Nachos will not catch all stack overflow conditions.
//	In other words, your program may still crash because of an overflow.
//
// 	If you get bizarre results (such as seg faults where there is no code)
// 	then you *may* need to increase the stack size.  You can avoid stack
// 	overflows by not putting large data structures on the stack.
// 	Don't do this: void foo() { int bigArray[10000]; ... }
//----------------------------------------------------------------------

void
Thread::CheckOverflow()
{
    if (stack != NULL) {
#ifdef HPUX			// Stacks grow upward on the Snakes
	ASSERT(stack[StackSize - 1] == STACK_FENCEPOST);
#else
	ASSERT(*stack == STACK_FENCEPOST);
#endif
   }
}

//----------------------------------------------------------------------
// Thread::Begin
// 	Called by ThreadRoot when a thread is about to begin
//	executing the forked procedure.
//
// 	It's main responsibilities are:
//	1. deallocate the previously running thread if it finished 
//		(see Thread::Finish())
//	2. enable interrupts (so we can get time-sliced)
//----------------------------------------------------------------------
// 23-0128[j]: 等同 void ThreadBegin()
//             (1)  已經由 StackAllocate() 存在 TCB(PCB) 中，並在未來 由 ThreadRoot(..) 呼叫
//             (2)  刪去 toBeDestroyed 指向的 Thread
//             (3)  啟用 interrupt

void
Thread::Begin ()
{
    ASSERT(this == kernel->currentThread);
    DEBUG(dbgThread, "Beginning thread: " << name);
    
    kernel->scheduler->CheckToBeDestroyed();
    kernel->interrupt->Enable();
}

//----------------------------------------------------------------------
// Thread::Finish
// 	Called by ThreadRoot when a thread is done executing the 
//	forked procedure.
//
// 	NOTE: we can't immediately de-allocate the thread data structure 
//	or the execution stack, because we're still running in the thread 
//	and we're still on the stack!  Instead, we tell the scheduler
//	to call the destructor, once it is running in the context of a different thread.
//
// 	NOTE: we disable interrupts, because Sleep() assumes interrupts
//	are disabled.
//----------------------------------------------------------------------
// 23-0128[j]: 等同 void ThreadFinish()
//             (1)  已經由 StackAllocate() 存在 TCB(PCB) 中，並在未來 由 ThreadRoot(..) 呼叫
//             (2)  關閉 interrupt
//             (3)  將此 Thread 設為 Sleep

void
Thread::Finish ()
{
    (void) kernel->interrupt->SetLevel(IntOff);		
    ASSERT(this == kernel->currentThread);
    
    // 23-0306[j]:  MP3 新增 
    DEBUG(dbgSch," Ticks: " << kernel->stats->totalTicks << ", thread " << kernel->currentThread->getName() << " finish!");
    DEBUG(dbgSch," ========= Priority: " << kernel->currentThread->getPriority() << "=========");
    DEBUG(dbgThread, "Finishing thread: " << name);

    Sleep(TRUE);				// invokes SWITCH = 召喚 SWITCH(..)
    // not reached
}


//----------------------------------------------------------------------
// Thread::Yield
// 	Relinquish the CPU if any other thread is ready to run.
//	If so, put the thread on the end of the ready list, so that
//	it will eventually be re-scheduled.
//
//	NOTE: returns immediately if no other thread on the ready queue.
//	Otherwise returns when the thread eventually works its way
//	to the front of the ready list and gets re-scheduled.
//
//	NOTE: we disable interrupts, so that looking at the thread
//	on the front of the ready list, and switching to it, can be done
//	atomically.  On return, we re-set the interrupt level to its
//	original state, in case we are called with interrupts disabled. 
//
// 	Similar to Thread::Sleep(), but a little different.
//----------------------------------------------------------------------
// 23-0130[j]: Yield () [Run -> Ready]
            // -    主要功能：暫停 currentThread(未結束) 送回 RQ，先讓其他 Thread 執行
            // -    是不可中斷的「原子操作(Atomic operation)」= 要先關中斷才能執行
            // (1)  關中斷，並儲存「原本的中斷狀態」
            // (2)  Pop ReadyQueue = nextThread
            // (3)  將 currentThread 排入 ReadyQueue
            // (4)  進行 Context Switch 切到 nextThread 執行
            // (5)  從 其他Thread 切回 currentThread 後，設定回「原本的中斷狀態」

// 23-0303[j]:  OneTick() 的 Time Out 會呼叫此處
//              NachOS 預設的 RR排班，每 100 Ticks 會執行一次 Preempt = Yield

void
Thread::Yield ()
{
    Thread *nextThread;

    // 23-0128[j]: 先關閉中斷，並儲存「原本的中斷狀態(oldLevel)」
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
    
    ASSERT(this == kernel->currentThread);

    DEBUG(dbgThread, "Yielding thread: " << name);
    
    // 23-0128[j]: 從 ReadyQueue Pop out 1st Thread 存入 nextThread
    //             並將 當前的 Thread 排入 ReadyQueue
    //             再進行 Context Switch
    nextThread = kernel->scheduler->FindNextToRun();
    if (nextThread != NULL) {

        // 23-0306[j]:  這裏要計算 實際Burst Time & 紀錄 Context Switch 的發生時間
        // 23-0306[j]:  (1) currentThread 欲 [ Running -> Ready ]
        //                  需要 暫停 實際Burst Time 的計時
        if(this->getStatus() == RUNNING && this->busrt ){
            this->busrt->PauseNow(kernel->stats->userTicks);
        }

        kernel->scheduler->ReadyToRun(this);
        kernel->scheduler->Run(nextThread, FALSE);
    }

    (void) kernel->interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// Thread::Sleep
// 	Relinquish the CPU, because the current thread has either
//	finished or is blocked waiting on a synchronization 
//	variable (Semaphore, Lock, or Condition).  In the latter case,
//	eventually some thread will wake this thread up, and put it
//	back on the ready queue, so that it can be re-scheduled.
//
//	NOTE: if there are no threads on the ready queue, that means
//	we have no thread to run.  "Interrupt::Idle" is called
//	to signify that we should idle the CPU until the next I/O interrupt
//	occurs (the only thing that could cause a thread to become
//	ready to run).
//
//	NOTE: we assume interrupts are already disabled, because it
//	is called from the synchronization routines which must
//	disable interrupts for atomicity.   We need interrupts off 
//	so that there can't be a time slice between pulling the first thread
//	off the ready list, and switching to it.
//----------------------------------------------------------------------
// 23-0129[j]: Sleep(bool finishing) [Run -> Waiting(FALSE)、Terminate(TRUE)]
//             -    主要功能：自行設定 暫停/結束 currentThread，並從 RQ 取出 next Thread 執行
//             -    是不可中斷的「原子操作(Atomic operation)」= 要先關中斷才能執行
//             (1) 檢查中斷是否已關閉
//             (2) 將 currentThread 排入 Waiting Queue (設為 Blocked)
//             (3) Pop ReadyQueue = nextThread
//                 - 進入 while(nextThread) 迴圈
//                 - 若 Ready Queue 為空(nextThread=NULL)，則 等待interrupt (唯一會將 Thread放入ReadyQueue的手段)
//                 - 若 Ready Queue 不為空，則 進行 Context Switch 切到 nextThread 執行
 
//             - Note:  當 Thread 在Blocked狀態 = Waiting Queue
//                      最終會有其他 Thread 將 Waiting Queue 的 Thread 喚醒、放回 Ready Queue)
void
Thread::Sleep (bool finishing)
{
    Thread *nextThread;
    
    ASSERT(this == kernel->currentThread);
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    
    DEBUG(dbgThread, "Sleeping thread: " << name);
    DEBUG(dbgTraCode, "In Thread::Sleep, Sleeping thread: " << name << ", " << kernel->stats->totalTicks);

    // 23-0306[j]:  這裏要計算 實際Burst Time & 紀錄 Context Switch 的發生時間
    // 23-0306[j]:  (2) 若 currentThread 欲 [ Running -> Waiting/Terminate ]
    //                  需要 停止 實際Burst Time 的計時 並 更新 預測Burst Time
    if(this->getStatus() == RUNNING && this->busrt ){
        double prevNext = this->busrt->getNext();

        this->busrt->PauseNow(kernel->stats->userTicks);
        this->busrt->UpdateNext();

        DEBUG(dbgSch, "[D] Tick[" << kernel->stats->totalTicks << "]: Thread [ " << this->getID()
                    << " ] update approximate burst time, from: [ "
                    << prevNext << "], add [" << this->busrt->getNow() << "], to [ " << this->busrt->getNext() <<" ] ");

        this->busrt->ResetNow();   // 23-0303[j]:  本次 預測時間(NextBurst) 計算完畢，本次 實際Burst Time 歸零
    }

    status = BLOCKED;

    while ((nextThread = kernel->scheduler->FindNextToRun()) == NULL) {
		kernel->interrupt->Idle();	// no one to run, wait for an interrupt
	}    
    
    // returns when it's time for us to run
    kernel->scheduler->Run(nextThread, finishing); 
}

//----------------------------------------------------------------------
// ThreadBegin, ThreadFinish,  ThreadPrint
//	Dummy functions because C++ does not (easily) allow pointers to member
//	functions.  So we create a dummy C function
//	(which we can pass a pointer to), that then simply calls the 
//	member function.
//----------------------------------------------------------------------

static void ThreadFinish()    { kernel->currentThread->Finish(); }
static void ThreadBegin() { kernel->currentThread->Begin(); }
void ThreadPrint(Thread *t) { t->Print(); }

#ifdef PARISC

//----------------------------------------------------------------------
// PLabelToAddr
//	On HPUX, function pointers don't always directly point to code,
//	so we need to do the conversion.
//----------------------------------------------------------------------

static void *
PLabelToAddr(void *plabel)
{
    int funcPtr = (int) plabel;

    if (funcPtr & 0x02) {
        // L-Field is set.  This is a PLT pointer
        funcPtr -= 2;	// Get rid of the L bit
        return (*(void **)funcPtr);
    } else {
        // L-field not set.
        return plabel;
    }
}
#endif

//----------------------------------------------------------------------
// Thread::StackAllocate
//	Allocate and initialize an execution stack.  The stack is
//	initialized with an initial stack frame for ThreadRoot, which:
//		enables interrupts
//		calls (*func)(arg)
//		calls Thread::Finish
//
//	"func" is the procedure to be forked
//	"arg" is the parameter to be passed to the procedure
//----------------------------------------------------------------------
// 23-0129[j]: 用來配置 & 初始化 Thread 的 Kernel Stack
//             (1)  配置空間，大小=StackSize * sizeof(int)
//                  (StackSize 定義在 thread.h)
//             (2)  初始化 Kernel stack
//                  包含 Push ThreadRoot(..)位址 & 將 Magic Num 存入 Stack 端點
//             (3)	初始化「剛建立 Thread」的「起始Reg值」
//                  設定初始 TCB(PCB) 中 machineState[]的值
//                  包含：ThreadRoot()、ThreadBegin()、func()、Arg、ThreadFinish()
//                  當 Thread 在 Context Switch 後，將 machineState[] 所存 Reg值 載回 CPU Reg
//                  此時 CPU 即可根據 Reg值 來執行 剛建立的 Thread

void
Thread::StackAllocate (VoidFunctionPtr func, void *arg)
{
    stack = (int *) AllocBoundedArray(StackSize * sizeof(int));
    // 23-0131[j]: Kernel StackSize 定義在 thread.h
    //             Kernel stack大小 = 8*1024*4 Bytes (if sizeof(int)=4 Bytes)
    //             其中，AllocBoundedArray(int size) 定義在 sysdep.cc
    //             會跟 Host 索取 PageSize 並 動態配置一塊區域 當作 Thread Stack
#ifdef PARISC
    // HP stack works from low addresses to high addresses
    // everyone else works the other way: from high addresses to low addresses
    stackTop = stack + 16;	// HP requires 64-byte frame marker
    stack[StackSize - 1] = STACK_FENCEPOST;
#endif

#ifdef SPARC
    stackTop = stack + StackSize - 96; 	// SPARC stack must contains at 
					// least 1 activation record 
					// to start with.
    *stack = STACK_FENCEPOST;
#endif 

#ifdef PowerPC // RS6000
    stackTop = stack + StackSize - 16; 	// RS6000 requires 64-byte frame marker
    *stack = STACK_FENCEPOST;
#endif 

#ifdef DECMIPS
    stackTop = stack + StackSize - 4;	// -4 to be on the safe side!
    *stack = STACK_FENCEPOST;
#endif

#ifdef ALPHA
    stackTop = stack + StackSize - 8;	// -8 to be on the safe side!
    *stack = STACK_FENCEPOST;
#endif


#ifdef x86
    // the x86 passes the return address on the stack.  In order for SWITCH() 
    // to go to ThreadRoot when we switch to this thread, the return addres 
    // used in SWITCH() must be the starting address of ThreadRoot.
    // 23-0127[j]: 此區塊是用來 初始化 Kernel stack
    //             (1)  存入第一個呼叫的函數位址 = ThreadRoot(..)
    //             (2)  將 Magic Num 存入 Kernel Stack 端點，用來識別 Kernel Stack 是否 Overflow

    // 23-0127[j]: 在 thread.h 預設的 Kernel StackSize = 8*1024 words
    //             stackTop 自然等於 stack + StackSize - 4
    //             Stack 的資料從 stackTop 開始往下擺
    stackTop = stack + StackSize - 4;	// -4 to be on the safe side!

    // 23-0127[j]: stackTop是int指標，-1 就是 往前指 4 Bytes
    //             因為 Stack 從高位址往下長
    //             以下等同「將 ThreadRoot(..)的位址 Push 到 Stack」(塞在stackTop位址 -4 Bytes 的位址)
    *(--stackTop) = (int) ThreadRoot;   
    *stack = STACK_FENCEPOST;   // 23-0127[j]: 將 stack 存入 Magic Num 用來識別 Stack 是否 Overflow
#endif
    
#ifdef PARISC
    machineState[PCState] = PLabelToAddr(ThreadRoot);
    machineState[StartupPCState] = PLabelToAddr(ThreadBegin);
    machineState[InitialPCState] = PLabelToAddr(func);
    machineState[InitialArgState] = arg;
    machineState[WhenDonePCState] = PLabelToAddr(ThreadFinish);


#else
// 23-0129[j]: 這裡不論什麼架構的Host，都會編譯
//             主要功能：初始化「剛建立 Thread」的「起始Reg值」
//             (1)  Thread使用到的 Host CPU Reg值 護儲存在「Thread物件(TCB/PCB) 中的 machineState[]陣列中」
//                  ex. machineState[PCState] = machineState[7] 存放 Thread的PC值
//                  ex. machineState[StartupPCState] = machineState[2] 存放 Thread的ECX值
//             (2)  初始化 Reg值 = 為 machineState[] 設定初始值
//                  -   當 Thread 在 Context Switch 後，將 machineState[] 所存 Reg值 載回 CPU Reg
//                      此時 Host CPU 即可根據 Reg 來執行 剛建立的 Thread
//                  -   machineState[] 存入的初始 Reg值 =「未來要執行的函數 或 要處理的參數」的位址
//                      包含：ThreadRoot()、ThreadBegin()、func()、Arg、ThreadFinish()
//             (3)  務必搭配 switch.h 定義

    machineState[PCState] = (void*)ThreadRoot;          // 23-0127[j]: machineState[7](PC) 指向 ThreadRoot(..)
    machineState[StartupPCState] = (void*)ThreadBegin;  // 23-0127[j]: machineState[2](ECX) 指向 kernel->currentThread->Begin()
    machineState[InitialPCState] = (void*)func;         // 23-0127[j]: machineState[5](ESI) 指向 func()
    machineState[InitialArgState] = (void*)arg;         // 23-0127[j]: machineState[3](EDX) 指向 arg
    machineState[WhenDonePCState] = (void*)ThreadFinish;// 23-0127[j]: machineState[6](EDI) 指向 kernel->currentThread->Finish()
    // 23-0128[j]: machineState[0](ESP)、[1](EAX)、[4](EBP) 皆被初始化為 NULL
#endif
}

#include "machine.h"

//----------------------------------------------------------------------
// Thread::SaveUserState
//	Save the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers -- 
//	one for its state while executing user code, one for its state 
//	while executing kernel code.  This routine saves the former.
//----------------------------------------------------------------------
// 23-0130[j]: 在 User Mode 下暫存「模擬CPU Reg值」

void
Thread::SaveUserState()
{
    for (int i = 0; i < NumTotalRegs; i++)
	userRegisters[i] = kernel->machine->ReadRegister(i);
}

//----------------------------------------------------------------------
// Thread::RestoreUserState
//	Restore the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers -- 
//	one for its state while executing user code, one for its state 
//	while executing kernel code.  This routine restores the former.
//----------------------------------------------------------------------
// 23-0130[j]: 在 User Mode 下回復「模擬CPU Reg值」

void
Thread::RestoreUserState()
{
    for (int i = 0; i < NumTotalRegs; i++)
	kernel->machine->WriteRegister(i, userRegisters[i]);
}


//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------
// 23-0130[j]: 印出 指定ID(代表當前Thread) 並 切到其他 Thread執行，共執行5次

static void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	    cout << "*** thread " << which << " looped " << num << " times\n";
        kernel->currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// Thread::SelfTest
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------
// 23-0130[j]: 測試 Thread 的實作 是否正常運作 = Create & Run Thread 的示範!!!
//             -    主要流程
//             (1)  new 一個 Thread 名為"forked thread" & ID = 1
//             (2)	幫 newThread 配置/初始化 Stack，並排入 ReadyQueue
//                  -   指定未來執行 func(..) = SimpleThread(1) & 傳入參數 1
//                  -   表示 newThread 會 印出 ID=1 並 切到其他 Thread執行，總共迴圈5次
//             (3)	Yield()：暫停 currentThread 切到 其他Thread 執行
//             (4)	SimpleThread(0)
//                  印出 ID=0 並 切到其他 Thread執行，總共迴圈5次

void
Thread::SelfTest()
{
    DEBUG(dbgThread, "Entering Thread::SelfTest");

    Thread *t = new Thread("forked thread", 1);

    t->Fork((VoidFunctionPtr) SimpleThread, (void *) 1);
    kernel->currentThread->Yield();
    SimpleThread(0);
}



// 23-0303[j]:  計算 new預測時間(NextBurst) = 0.5 x 實際Burst Time(NowBurst) + 0.5 x old預測時間(NextBurst)
//              需要 紀錄開始時間(start)、結束時間(end)、實際Burst Time(NowBurst)、預測時間(NextBurst)

// 23-0306[j]:  建構子，順便紀錄 Thread 被創建的時間
Burst::Burst(){
    start = 0.0; end = 0.0; accumWaitTime = 0; total = 0.0; now = 0.0; next = 0.0; 
}

double 
Burst::PauseNow(int uTick){
    // 23-0303[j]:  若 now = 0 表示沒有被送回 Ready Queue，故沒有「上次未結算的時間」
    end = (double)uTick;
    if(!now){
        now = end - start;
        total += now;
    }
    // 23-0306[j]:  若 now != 0 表示此前待在 RQ，本次 Burst Time 未結算
    else{
        total -= now;
        now = end - start + now;    // 23-0303[j]:  加上「上次未結算的時間」
        total += now;
    }
    return now;  
}  

// 23-0306[j]:  更新 預測時間(NextBurst)
double 
Burst::UpdateNext(){
    next = 0.5*now + 0.5*next;
    return next;
}