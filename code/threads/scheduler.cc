// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    // 23-0303[j]:  MP3 始化 3條 RQ
    readyList_L3 = new List<Thread *>; 
    readyList_L2 = new SortedList<Thread *>(CompareForPriority); 
    readyList_L1 = new SortedList<Thread *>(CompareForSFJ);

    preempFlag = FALSE;

    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList_L3; 
    delete readyList_L2; 
    delete readyList_L1; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------
// 23-0128[j]: 功能：將 thread 排入 ReadyQueue
//             條件：kernel->interrupt->getLevel() == IntOff (禁止中斷時，才能做這件事)
// 23-0302[j]: [ Ready Queue Inqueue ]

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    
    ThreadStatus prevStat = thread->getStatus();

    thread->setStatus(READY);

    // 23-0303[j]:  MP3 根據 Priority 決定加入那個 RQ
    if(CheckThreadRQ(thread) == 3){
        DEBUG(dbgSch, "[A] Tick[" << kernel->stats->totalTicks 
            << "]: Thread [ " << thread->getID() << " ] is inserted into queue L[ 3 ]");
        readyList_L3->Append(thread);
    }
    else if(CheckThreadRQ(thread) == 2){
        DEBUG(dbgSch, "[A] Tick[" << kernel->stats->totalTicks 
            << "]: Thread [ " << thread->getID() << " ] is inserted into queue L[ 2 ]");
        readyList_L2->Insert(thread);
    }
    else if(CheckThreadRQ(thread) == 1){
        DEBUG(dbgSch, "[A] Tick[" << kernel->stats->totalTicks 
            << "]: Thread [ " << thread->getID() << " ] is inserted into queue L[ 1 ]");
        readyList_L1->Insert(thread);
    }

    // 23-0306[j]:  MP3 檢查剛 insert 到 RQ 者，是不是 可以 優先於 currentThread 執行 = 插隊 
    //              注意：此處僅檢查「不是從 Running 回來的人」
    //                    因為「Running 回來的人，表示剛被插隊，表示 currentThread 有插隊的優先權，故不必反覆比較 」
    if(prevStat != RUNNING)
        SetPreempt( CheckPreempt() );

    // 23-0306[j]:  MP3 首次進入 Ready Queue 時 = New Thread 剛被創建的時間
    if(prevStat == JUST_CREATED)
        thread->busrt->SetAccumWait(kernel->stats->totalTicks);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect: 
//	Thread is removed from the ready list.
//----------------------------------------------------------------------
// 23-0128[j]: 功能：從 ReadyQueue 取出(Dequeue) 1st Thread，並回傳位址
//             條件：kernel->interrupt->getLevel() == IntOff (禁止中斷時，才能做這件事)
// 23-0302[j]: [ Ready Queue Dequeue ]

Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    // 23-0303[j]:  MP3 從最高Level的 RQ 先取出 Thread
    //              Lv1 RQ 空了，再去 Lv2 RQ Dequeue，以此類推

    if (!readyList_L1->IsEmpty()){
        DEBUG(dbgSch, "[B] Tick[" << kernel->stats->totalTicks 
            << "]: Thread [ " << readyList_L1->Front()->getID() << " ] is removed from queue L[ 1 ]");
        return readyList_L1->RemoveFront();
    }
    else if (!readyList_L2->IsEmpty()){
        DEBUG(dbgSch, "[B] Tick[" << kernel->stats->totalTicks 
            << "]: Thread [ " << readyList_L2->Front()->getID() << " ] is removed from queue L[ 2 ]");
        return readyList_L2->RemoveFront();
    }
    else if (!readyList_L3->IsEmpty()){
        DEBUG(dbgSch, "[B] Tick[" << kernel->stats->totalTicks 
            << "]: Thread [ " << readyList_L3->Front()->getID() << " ] is removed from queue L[ 3 ]");
        return readyList_L3->RemoveFront();
    }
    else{ return NULL; }

    // if (readyList->IsEmpty()) {
	// 	return NULL;
    // } else {
    // 	return readyList->RemoveFront();
    // }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------
// 23-0130[j]: Scheduler::Run (nextThread, finishing)
/*
            -	主要功能
                -	依照 參數finishing 決定 暫存/終結() oldThread
                -	Context Switch：切換到 nextThread執行
                    (1)	切換 Thread 的 Host CPU Reg：SWITCH(..) in switch.s
                    (2)	切換 Thread 的 模擬 CPU Reg：Scheduler::Run() in scheduler.cc
            -   流程
            (1)	把 currentThread 存入 oldThread 
            (2)	判斷 oldThread 狀態
                -  若 oldThread is finishing(TRUE) 則 toBeDestroyed = oldThread
                    等 nextThread 執行 Begin()時，會刪去 toBeDestroyed 指向的 Thread

                -	儲存 oldThread 模擬CPU reg 狀態 (in User Mode 存在 userRegisters[i])
                    (等一下 Context Switch 會再存一次 在 machineState[i])
                    
            (3)	將 nextThread 設為 kernel->currentThread & 狀態設為為 RUNNING 
            (4)	Context switch: 呼叫 SWITCH(oldThread, nextThread) in switch.S
                -	將 oldThread 目前所使用的 CPU Reg(s) 暫存至 oldThread之TCB(PCB) 中
                -	將 nextThread 暫存在 nextThread之TCB(PCB)的 Reg(s)值 載入「目前 CPU 的 Reg(s)」
                -	回復 nextThread 之前的 PC值 給「目前的PC」
                    (1)	若 nextThread 剛建立：PC 指向 ThreadRoot(..)
                        -	nextThread 依序執行 ThreadBegin() -> ForkExecute(..) -> ThreadFinish()
						-	ForkExecute(..) 會執行 (0) 所述的工作
                    (2) 若 nextThread 執行到一半：PC 指向 之前的 t2 PC
                        -	會回到 nextThread 的 scheduler->Run(..) 執行[2]的「回復 模擬 Reg & Page Table」

            (5)	若 oldThread 已經 finish -> Run() 至此結束
                若 oldThread 尚未 finish，表示有其他 Thread 進行 Context switch 回到 oldThread
                會繼續執行 剩下的 Scheduler::Run(..)，包含 (6)、(7)

            (6) CheckToBeDestroyed() = 若 toBeDestroyed 有指向 Thread，則將其刪除
            (7)	回復 oldThread 模擬CPU reg 狀態 (in User Mode 存在 userRegisters[i])
*/
             
void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
	    toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	    oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running

    // 23-0306[j]:  這裏要計算 實際Burst Time & 紀錄 Context Switch 的發生時間
    // 23-0303[j]:  MP3 此處是 nextThread 進入 Running 的入口，需要開始紀錄 實際Burst Time
    if(nextThread->busrt){
        nextThread->busrt->Start(kernel->stats->userTicks);
    }

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());

    // 23-0303[j]:  MP3 新增 Debug 訊息
    double now = 0.0;
    if(oldThread->busrt) now = oldThread->busrt->getNow();
    DEBUG(dbgSch, "[E] Tick[" << kernel->stats->totalTicks 
            << "]: Thread [ " << nextThread->getID() << " ] is now selected for execution,"
            << "thread [" << oldThread->getID() << "] is replaced, and it has executed [" << now << "] ticks");    
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    // 23-0128[j]: 從 Scheduler::Run 呼叫 SWITCH(oldThread, nextThread) 的流程
    //             (1)  此行翻譯成組語如下
    //                  push Thread * nextThread
	//                  push Thread * oldThread	
	//                  call SWITCH			// 這裡會 Push oldThread的PC = oldThread的下個指令位址
	//             						       並且 Jmp to SWITCH
    //             (2) 進入 switch.S 中的 SWITCH: 並執行以下動作
    //                 -    儲存 oldThread 目前所使用的 CPU Reg值
    //                 -    載入 nextThread 存在 TCB(PCB)的 Reg值 到「目前 CPU 的 Reg(s)」
    //                 -    回復 nextThread 之前的 PC值 給「目前的PC」= 切到 nextThread執行
			

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
    // 23-0130[j]: 若 oldThread 沒有 finish，則會回到「下一道指令」繼續執行
    //             其中的流程如下
    //             (1) otherThread 呼叫 SWITCH(..)
    //             (2) SWITCH(..) 將 oldThread 的 Reg & PC 載回 CPU的Reg
    //             (3) 切回 oldThread，CPU的PC 指向此處 繼續執行

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    // 23-0130[j]: 若 toBeDestroyed 有指向 Thread，則將其刪除
    CheckToBeDestroyed();		// check if thread we were running
					            // before this one has finished
					            // and needs to be cleaned up
    
    // 23-0130[j]: 在 User Mode 下回復 CPU 的 Reg值
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	    oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------
// 23-0128[j]: 由 ThreadBegin() = Thread::Begin() & Scheduler::Run() 呼叫
//             若 toBeDestroyed 有指向 Thread，則將其刪除

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {

        // 23-0303[j]:  可以在此回收 Thread 的記憶體空間
        // cout << "Destroyed Thread: " << kernel->currentThread->getName() << endl;
        if(toBeDestroyed->space)
            toBeDestroyed->space->~AddrSpace();

        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    // 23-0303[j]:  MP3 改成 印出3條RQ

    cout << "Lv1 Ready Queue:\n";
    readyList_L1->Apply(ThreadPrint);
    cout << "\nLv2 Ready Queue:\n";
    readyList_L2->Apply(ThreadPrint);
    cout << "\nLv3 Ready Queue:\n";
    readyList_L3->Apply(ThreadPrint);

    // cout << "Ready list contents:\n";
    // readyList->Apply(ThreadPrint);
}

// 23-0303[j]:  MP3 SortedList 用的 Compare 函數

//	   int Compare(T x, T y) 
//		returns -1 if x < y (小的會往前排)
//		returns 0 if x == y
//		returns 1 if x > y

int CompareForSFJ(Thread * newThread, Thread * currentThread){
    if(!(newThread->busrt && currentThread->busrt)) return 0;

    if(newThread->busrt->getNext() < currentThread->busrt->getNext()) return -1;
    else if(newThread->busrt->getNext() == currentThread->busrt->getNext()) return 0;
    else return 1;
}
int CompareForPriority(Thread * newThread, Thread * currentThread){
    if(newThread->getPriority() > currentThread->getPriority()) return -1;
    else if(newThread->getPriority() == currentThread->getPriority()) return 0;
    else return 1;
}

// 23-0306[j]:  根據 Priority 判斷 Thread 應該待在那個 Ready Queue
int 
Scheduler::CheckThreadRQ(Thread* thread){
    int thread_pry = thread->getPriority();
    if(thread_pry < 50 && thread_pry >=0) return 3;
    else if(thread_pry < 100 && thread_pry >=50) return 2;
    else if(thread_pry < 150 && thread_pry >=100) return 1;
}

// 23-0304[j]:  MP3 Aging
void 
Scheduler::Aging(){

    // 23-0304[j]:  重排 RQ
    ListIterator<Thread*> *iter3 = new ListIterator<Thread*>(readyList_L3);
    ListIterator<Thread*> *iter2 = new ListIterator<Thread*>(readyList_L2);
    ListIterator<Thread*> *iter1 = new ListIterator<Thread*>(readyList_L1);

    Thread* t = NULL;

    if(!readyList_L3->IsEmpty()){
        for(; !iter3->IsDone(); iter3->Next()){
            t = iter3->Item();

            if((t->getStatus() == READY) && t->busrt){
                int waitTime = kernel->stats->totalTicks - (int)t->busrt->getTotal() - t->busrt->GetAccumWait();

                if(waitTime > 1500){
                    int oldPriority = t->getPriority();
                    t->setPriority( oldPriority + 10 );

                    t->busrt->SetAccumWait( waitTime + t->busrt->GetAccumWait());

                    DEBUG(dbgSch, "[C] Tick [" << kernel->stats->totalTicks 
                    << "]: Thread [ " << t->getID() 
                    << " ] changes its priority from [ "<< oldPriority <<" ] to [ "<< t->getPriority() <<" ]");

                    readyList_L3->Remove(t);

                    if(CheckThreadRQ(t)==3) readyList_L3->Append(t);
                    else
                        readyList_L2->Insert(t);
                }
            }
        }
    }

    if(!readyList_L2->IsEmpty()){
        for(; !iter2->IsDone(); iter2->Next()){
            t = iter2->Item();

            if((t->getStatus() == READY) && t->busrt){
                int waitTime = kernel->stats->totalTicks - (int)t->busrt->getTotal() - t->busrt->GetAccumWait();

                if(waitTime > 1500){
                    int oldPriority = t->getPriority();
                    t->setPriority( oldPriority + 10 );

                    t->busrt->SetAccumWait( waitTime + t->busrt->GetAccumWait());

                    DEBUG(dbgSch, "[C] Tick [" << kernel->stats->totalTicks 
                    << "]: Thread [ " << t->getID() 
                    << " ] changes its priority from [ "<< oldPriority <<" ] to [ "<< t->getPriority() <<" ]");

                    readyList_L2->Remove(t);

                    if(CheckThreadRQ(t)==2) readyList_L2->Insert(t);
                    else
                        readyList_L1->Insert(t);
                }
            }
        }
    }

    if(!readyList_L1->IsEmpty()){
        for(; !iter1->IsDone(); iter1->Next()){
            t = iter1->Item();

            if((t->getStatus() != RUNNING) && t->busrt){
                int waitTime = kernel->stats->totalTicks - (int)t->busrt->getTotal() - t->busrt->GetAccumWait();

                if(waitTime > 1500){
                    int oldPriority = t->getPriority();
                    if(oldPriority<140) t->setPriority( oldPriority + 10 );
                    else t->setPriority(149);

                    t->busrt->SetAccumWait( waitTime + t->busrt->GetAccumWait());

                    DEBUG(dbgSch, "[C] Tick [" << kernel->stats->totalTicks 
                    << "]: Thread [ " << t->getID() 
                    << " ] changes its priority from [ "<< oldPriority <<" ] to [ "<< t->getPriority() <<" ]");
                }
            }
        }
    }

    // delete iter1,iter2,iter3;
}

// 23-0306[j]:  MP3 實現 Preemptive 的函數
/*              修改過的 NachOS 有 2 種 Preemptive
                1>   L3：RR排班的 Time Out 中斷 會呼叫 Yield() = Preempt
                2>  藉由 CheckPreempt() 確認「需要呼叫 Yield() = Preempt」
                    -   呼叫時機
                        -   每 100 Ticks Time Out 中斷，會檢查/執行一次 Aging + CheckPreempt
                        -   每次有 Thread 被 insert 到 Ready Queue = ReadyToRun() 時
                    -   需要 Preempt 的情形有3種
                    (1) L3 Job Running & 目前 RQ_L1、RQ_L2 非空：有 L1、L2 Job 要打斷 L3 Job
                    (2) L2 Job Running & 目前 RQ_L1 非空：有 L1 Job 要打斷 L2 Job
                    (3) L1 Job Running & 目前 RQ_L1非空 且 RQ_L1中的 Job 更短：有 L1 更段 Job 要打斷 L1 Job
*/
bool 
Scheduler::CheckPreempt(){
    if(preempFlag) return TRUE;

    
    // 23-0305[j]:  L3 Job Running & 目前 RQ_L1、RQ_L2 非空
    if((CheckThreadRQ(kernel->currentThread)==3) 
        && kernel->currentThread->getStatus()==RUNNING
        && !(readyList_L1->IsEmpty() && readyList_L2->IsEmpty())){

        preempFlag = TRUE;
    }
    // 23-0305[j]:  L2 Job Running & 目前 RQ_L1 非空
    else if((CheckThreadRQ(kernel->currentThread)==2) 
            && kernel->currentThread->getStatus()==RUNNING
            && !readyList_L1->IsEmpty()){

            preempFlag = TRUE;
    }
    // 23-0305[j]:  L1 Job Running & 目前 RQ_L1非空 且 RQ_L1中的 Job 更短！
    else if((CheckThreadRQ(kernel->currentThread)==1) 
            && kernel->currentThread->getStatus()==RUNNING
            && !readyList_L1->IsEmpty() && readyList_L1->Front()->busrt){

            if(readyList_L1->Front()->busrt->getNext() < kernel->currentThread->busrt->getNext())
                return TRUE;        
    }

    return FALSE;
}