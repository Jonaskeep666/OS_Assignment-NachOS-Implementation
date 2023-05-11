// scheduler.h 
//	Data structures for the thread dispatcher and scheduler.
//	Primarily, the list of threads that are ready to run.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "copyright.h"
#include "list.h"
#include "thread.h"

// 23-0303[j]:  MP3 SortedList 用的 Compare 函數
int CompareForSFJ(Thread * newThread, Thread * currentThread);
int CompareForPriority(Thread * newThread, Thread * currentThread);

// The following class defines the scheduler/dispatcher abstraction -- 
// the data structures and operations needed to keep track of which 
// thread is running, and which threads are ready but not running.

class Scheduler {
  public:
    Scheduler();		// Initialize list of ready threads 
    ~Scheduler();		// De-allocate ready list

    void ReadyToRun(Thread* thread);	
    				// Thread can be dispatched.
    Thread* FindNextToRun();	// Dequeue first thread on the ready 
				// list, if any, and return thread.
    void Run(Thread* nextThread, bool finishing);
    				// Cause nextThread to start running
    void CheckToBeDestroyed();// Check if thread that had been
    				// running needs to be deleted
    void Print();		// Print contents of ready list
    
    // SelfTest for scheduler is implemented in class Thread
    
    // 23-0304[j]:  MP3 新增
    int CheckThreadRQ(Thread* thread);

    void Aging();

    bool CheckPreempt();
    void SetPreempt(bool flag) {preempFlag = flag;}

  private:
    // 23-0303[j]:  MP3 修改成 3條 RQ
    List<Thread *> *readyList_L3;         //  Round-Robin

    SortedList<Thread *> *readyList_L2;   //  Non-preemptive Priority
    SortedList<Thread *> *readyList_L1;   //  Approximated SFJ

    Thread *toBeDestroyed;	// finishing thread to be destroyed
    				// by the next thread that runs

    // 23-0306[j]:  用一個變數 紀錄需不需要在 下個 Time Out 作 Preempt
    //              注意：NachOS 目前允許實際呼叫 Yield() = Preempt 的地方只有 OneTick()
    //                    自行隨意呼叫，容易出現同步問題
    bool preempFlag;
};

#endif // SCHEDULER_H
