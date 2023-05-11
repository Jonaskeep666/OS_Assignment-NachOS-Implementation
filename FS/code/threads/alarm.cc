// alarm.cc
//	Routines to use a hardware timer device to provide a
//	software alarm clock.  For now, we just provide time-slicing.
//
//	Not completely implemented.
//

#include "copyright.h"
#include "alarm.h"
#include "main.h"

//----------------------------------------------------------------------
// Alarm::Alarm
//      Initialize a software alarm clock.  Start up a timer device
//
//      "doRandom" -- if true, arrange for the hardware interrupts to 
//		occur at random, instead of fixed, intervals.
//----------------------------------------------------------------------

Alarm::Alarm(bool doRandom)
{
    timer = new Timer(doRandom, this);
}

//----------------------------------------------------------------------
// Alarm::CallBack
//	Software interrupt handler for the timer device. The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	Note that instead of calling Yield() directly (which would
//	suspend the interrupt handler, not the interrupted thread
//	which is what we wanted to context switch), we set a flag
//	so that once the interrupt handler is done, it will appear as 
//	if the interrupted thread called Yield at the point it is 
//	was interrupted.
//
//	For now, just provide time-slicing.  Only need to time slice 
//      if we're currently running something (in other words, not idle).
//----------------------------------------------------------------------
// 23-0302[j]: 每次 Time out 會呼叫的 ISR
/*
    主要功能：呼叫 interrupt->YieldOnReturn()，提示系統「可以開始 Context Switch」
    工作流程：
    -	在 Alarm(..) 時建構的 new Timer(..); 會呼叫 SetInterrupt() 設定 Time Out 發生時間 = 100 Ticks 之後
    -	當 100 Ticks 之後，Time Out 中斷發生
    -	呼叫 Timer->CallBack()，會做 2件事
        -	呼叫 Alarm->CallBack();	//	提示系統「可以開始 Context Switch」
        -	呼叫 SetInterrupt();	//	再次設定一次 100 Ticks 以後發生的中斷

    -	當 Machine::Run() 中 OneInstruction(instr) 結束，會呼叫 kernel->interrupt->OneTick()
        此時會檢查 是否要進行 Context Switch，若需要切換(yieldOnReturn = TRUE)
        則呼叫 kernel->currentThread->Yield() = 暫停 currentThread(未結束) 送回RQ，先讓其他 Thread 執行 
*/


void 
Alarm::CallBack() 
{
    Interrupt *interrupt = kernel->interrupt;
    MachineStatus status = interrupt->getStatus();

    // 23-0304[j]:  MP3 每 100 Ticks 作一次 Aging 調整
    kernel->scheduler->Aging();

    // 23-0306[j]:  MP3 檢查 RQ 中 有沒有適合 Preempt 的 Job
    //              若有，則設定 YieldOnReturn() 回到 OneTick() 後會執行 Yield() = Preempt
    if(kernel->scheduler->CheckPreempt()){
        DEBUG(dbgSch,"[#] Other Preempt!");
        kernel->scheduler->SetPreempt(FALSE);
        interrupt->YieldOnReturn();
    }
        
    // 23-0304[j]:  只有 currentThread 屬於 L3 RQ，才會在這裏執行插隊(Preempt)
    if (status != IdleMode && kernel->currentThread->getPriority() < 50) {
        DEBUG(dbgSch,"[#] Time Out Preempt!");
	    interrupt->YieldOnReturn();
    }
}
