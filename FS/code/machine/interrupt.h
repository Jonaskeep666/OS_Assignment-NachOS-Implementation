// interrupt.h 
//	Data structures to emulate low-level interrupt hardware.
//
//	The hardware provides a routine (SetLevel) to enable or disable
//	interrupts.
//
//	In order to emulate the hardware, we need to keep track of all
//	interrupts the hardware devices would cause, and when they
//	are supposed to occur.  
//
//	This module also keeps track of simulated time.  Time advances
//	only when the following occur: 
//		interrupts are re-enabled
//		a user instruction is executed
//		there is nothing in the ready queue
//
//	As a result, unlike real hardware, interrupts (and thus time-slice 
//	context switches) cannot occur anywhere in the code where interrupts
//	are enabled, but rather only at those places in the code where 
//	simulated time advances (so that it becomes time to invoke an
//	interrupt in the hardware simulation).
//
//	NOTE: this means that incorrectly synchronized code may work
//	fine on this hardware simulation (even with randomized time slices),
//	but it wouldn't work on real hardware.
//
//  DO NOT CHANGE -- part of the machine emulation

// 23-0301[j]: 模擬電腦的中斷
/*
  - 模擬 硬體中斷
		-	可透過 SetLevel() 來開啟/關閉 中斷功能
		-	無法如真實硬體一樣，隨時發出中斷訊號，而是在特定時刻才會發出中斷
	-	模擬 時間前進（只有以下 3種情況 會 模擬時間 前進）
		(1)	interrupts 被重新啟用時，模擬時間前進
		(2)	執行一個 User Instruction 後，模擬時間前進
		(3)	Ready Queue 為空的時候，模擬時間前進
*/

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "copyright.h"
#include "list.h"
#include "callback.h"

// Interrupts can be disabled (IntOff) or enabled (IntOn)
enum IntStatus { IntOff, IntOn };

// Nachos can be running kernel code (SystemMode), user code (UserMode),
// or there can be no runnable thread, because the ready list 
// is empty (IdleMode).
enum MachineStatus {IdleMode, SystemMode, UserMode};

// IntType records which hardware device generated an interrupt.
// In Nachos, we support a hardware timer device, a disk, a console
// display and keyboard, and a network.
// 22-1231[j]: IntType 代表「中斷的類型」= 哪個硬體引發中斷
enum IntType { TimerInt, DiskInt, ConsoleWriteInt, ConsoleReadInt, 
			NetworkSendInt, NetworkRecvInt};

// The following class defines an interrupt that is scheduled
// to occur in the future.  The internal data structures are
// left public to make it simpler to manipulate.

typedef int OpenFileId;

// 23-0302[j]: 建立的 PendingInterrupt 物件 = 儲存一個「待執行中斷」的「ISR & 發生時間 & 中斷類型」
class PendingInterrupt {
  public:
    PendingInterrupt(CallBackObj *callOnInt, int time, IntType kind);
				// initialize an interrupt that will
				// occur in the future

    CallBackObj *callOnInterrupt;// The object (in the hardware device
				// emulator) to call when the interrupt occurs
    // 23-0302[j]: 指標 callOnInterrupt 指向「模擬硬體設備 的 物件」
    /*
                  -	callOnInterrupt->CallBack();	// 會指向 該硬體設備的 ISR
                  -	ex.	callOnInterrupt 指向 ConsoleOutput 建立的物件位址
                      callOnInterrupt->CallBack() 指向 ConsoleOutput->CallBack()
                      （當 ConsoleOutput(螢幕) 工作完成，可呼叫此 ISR 通知 CPU）
    */



    int when;			// When the interrupt is supposed to fire
    IntType type;		// for debugging
};

// The following class defines the data structures for the simulation
// of hardware interrupts.  We record whether interrupts are enabled
// or disabled, and any hardware interrupts that are scheduled to occur
// in the future.

class Interrupt {
  public:
    Interrupt();		// initialize the interrupt simulation
    ~Interrupt();		// de-allocate data structures
    
    IntStatus SetLevel(IntStatus level);
    				// Disable or enable interrupts and return previous setting.

    void Enable() { (void) SetLevel(IntOn); }
				// Enable interrupts.
    IntStatus getLevel() {return level;}
    				// Return whether interrupts are enabled or disabled
    
    void Idle(); 		// The ready queue is empty, roll 
				// simulated time forward until the 
				// next interrupt

    void Halt(); 		// quit and print out stats 

    void PrintInt(int number); // print int

    // 23-0419[j]: 以下在 interrupt.cc 本來就註解掉了(MP1 不需透過以下完成)
    int CreateFile(char *filename);
    OpenFileId OpenFile(char *name);
    int WriteFile(char *buffer, int size, OpenFileId id);
    int ReadFile(char *buffer, int size, OpenFileId id);
    int CloseFile(OpenFileId id);
 
    void YieldOnReturn();	// cause a context switch on return 
				// from an interrupt handler

    MachineStatus getStatus() { return status; } 
    void setStatus(MachineStatus st) { status = st; }
        			// idle, kernel, user

    void DumpState();		// Print interrupt state
    

    // NOTE: the following are internal to the hardware simulation code.
    // DO NOT call these directly.  I should make them "private",
    // but they need to be public since they are called by the
    // hardware device simulators.

    // 23-0419[j]: 安排 未來要發生的中斷(順序存在 SortedList* pending)
    void Schedule(CallBackObj *callTo, int when, IntType type);
    				// Schedule an interrupt to occur
				// at time "when".  This is called
    				// by the hardware device simulators.
    
    // 23-0127[j]: 模擬 時間快轉 10個 or 1個 Tick
    void OneTick();       	// Advance simulated time

  private:
    IntStatus level;		// are interrupts enabled or disabled?

    // 22-1231[j]: pending 是個 Sorted List，依序存放「待執行中斷」物件的位址
    SortedList<PendingInterrupt *> *pending;		
    				// the list of interrupts scheduled to occur in the future

    //int writeFileNo;            //UNIX file emulating the display
    bool inHandler;		// TRUE if we are running an interrupt handler
    //bool putBusy;               // Is a PrintInt operation in progress
                                  // If so, you cannoot do another one
    bool yieldOnReturn; 	// TRUE if we are to context switch on return from the interrupt handler
    MachineStatus status;	// idle, kernel mode, user mode

    // these functions are internal to the interrupt simulation code

    bool CheckIfDue(bool advanceClock); 
    				// Check if any interrupts are supposed
				// to occur now, and if so, do them

    void ChangeLevel(IntStatus old, 	// SetLevel, without advancing the simulated time
			IntStatus now);
};

#endif // INTERRRUPT_H
