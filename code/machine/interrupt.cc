// interrupt.cc 
//	Routines to simulate hardware interrupts.
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
//  DO NOT CHANGE -- part of the machine emulation

#include "copyright.h"
#include "interrupt.h"
#include "main.h"

// String definitions for debugging messages

static char *intLevelNames[] = { "off", "on"};
static char *intTypeNames[] = { "timer", "disk", "console write", 
			"console read", "network send", 
			"network recv"};

//----------------------------------------------------------------------
// PendingInterrupt::PendingInterrupt
// 	Initialize a hardware device interrupt that is to be scheduled 
//	to occur in the near future.
//
//	"callOnInt" is the object to call when the interrupt occurs
//	"time" is when (in simulated time) the interrupt is to occur
//	"kind" is the hardware device that generated the interrupt
//----------------------------------------------------------------------

PendingInterrupt::PendingInterrupt(CallBackObj *callOnInt, 
					int time, IntType kind)
{
    callOnInterrupt = callOnInt;
    when = time;
    type = kind;
}

//----------------------------------------------------------------------
// PendingCompare
//	Compare to interrupts based on which should occur first.
//----------------------------------------------------------------------

static int
PendingCompare (PendingInterrupt *x, PendingInterrupt *y)
{
    if (x->when < y->when) { return -1; }
    else if (x->when > y->when) { return 1; }
    else { return 0; }
}

//----------------------------------------------------------------------
// Interrupt::Interrupt
// 	Initialize the simulation of hardware device interrupts.
//	
//	Interrupts start disabled, with no interrupts pending, etc.
//----------------------------------------------------------------------

Interrupt::Interrupt()
{
    level = IntOff;
    pending = new SortedList<PendingInterrupt *>(PendingCompare);
    inHandler = FALSE;
    yieldOnReturn = FALSE;
    status = SystemMode;
}

//----------------------------------------------------------------------
// Interrupt::~Interrupt
// 	De-allocate the data structures needed by the interrupt simulation.
//----------------------------------------------------------------------

Interrupt::~Interrupt()
{
    while (!pending->IsEmpty()) {
	    delete pending->RemoveFront();
    }
    delete pending;
}

//----------------------------------------------------------------------
// Interrupt::ChangeLevel
// 	Change interrupts to be enabled or disabled, without advancing 
//	the simulated time (normally, enabling interrupts advances the time).
//
//	Used internally.
//
//	"old" -- the old interrupt status
//	"now" -- the new interrupt status
//----------------------------------------------------------------------

void
Interrupt::ChangeLevel(IntStatus old, IntStatus now)
{
    level = now;
    DEBUG(dbgInt, "\tinterrupts: " << intLevelNames[old] << " -> " << intLevelNames[now]);
}

//----------------------------------------------------------------------
// Interrupt::SetLevel
// 	Change interrupts to be enabled or disabled, and if interrupts
//	are being enabled, advance simulated time by calling OneTick().
//
// Returns:
//	The old interrupt status.
// Parameters:
//	"now" -- the new interrupt status
//----------------------------------------------------------------------
// 23-0128[j]: SetLevel(IntStatus now)
//             (1)  儲存「原本的 中斷狀態(ON/OFF)=old」
//             (2)  更新中斷狀態 = now，並回傳 原本的中斷狀態(old)
//             (3)  若中斷狀態 由 OFF(old)->ON(now)，則 前進一個 Tick

IntStatus
Interrupt::SetLevel(IntStatus now)
{
    IntStatus old = level;
    
    // interrupt handlers are prohibited from enabling interrupts
    ASSERT((now == IntOff) || (inHandler == FALSE));

    ChangeLevel(old, now);			// change to new state
    if ((now == IntOn) && (old == IntOff)) {
	    OneTick();				// advance simulated time
    }
    return old;
}

//----------------------------------------------------------------------
// Interrupt::OneTick
// 	Advance simulated time and check if there are any pending 
//	interrupts to be called. 
//
//	Two things can cause OneTick to be called:
//		interrupts are re-enabled
//		a user instruction is executed
//----------------------------------------------------------------------
/*
// 23-0302[j]:  void OneTick();
                -	功能
                    (1)	模擬 時間前進 1個 or 10個 Tick
                        -	User Mode：前進 1 Tick
                        -	Kernel Mode：前進 10 Tick
                    (2)	檢查是否有「待執行中斷」可以「現在」執行(若無，不必快轉)
                        -	若有，則執行該中斷
                        -	若無，不必快轉，因為 advanceClock = FALSE)
                    (3)	檢查是否需要 Context Switch
                        -	若有(yieldOnReturn = TRUE)，則進行 Context Switch
                            -	切到 Kernel Mode，呼叫 kernel->currentThread->Yield();
                            -	暫停 currentThread(未結束) 送回RQ，先讓其他 Thread 執行 

                        -	若無(yieldOnReturn = FALSE)，則跳過

                -	被呼叫的時機
                    (1)	中斷「重新啟用」= SetLevel(IntOn);
                    (2)	模擬 CPU 執行一個 User Instruction 後 (Kernel 不算)
                        (在 Machine::Run() 中呼叫)
*/
void
Interrupt::OneTick()
{
    MachineStatus oldStatus = status;   // 23-0101[j]: enum MachineStatus 分為 IdleMode, SystemMode, UserMode
    Statistics *stats = kernel->stats;

// advance simulated time
// 23-0101[j]: 往後快轉一個「Tick」＝「OneTick」並需更新以下參數
// 23-0101[j]: (1) stats->totalTicks 開始運作 NachOS 到現在的 Tick數
// 23-0101[j]: (2) stats->SystemTick/UserTick 開始運作 SystemMode/UserMode 到現在的 Tick數
    if (status == SystemMode) {
        stats->totalTicks += SystemTick;
	    stats->systemTicks += SystemTick;
    } else {
	stats->totalTicks += UserTick;
	stats->userTicks += UserTick;
    }
    DEBUG(dbgInt, "== Tick " << stats->totalTicks << " ==");

// check any pending interrupts are now ready to fire

    ChangeLevel(IntOn, IntOff);	// first, turn off interrupts (ON->OFF)
				                // (interrupt handlers run with interrupts disabled)
    // 23-0302[j]: 檢查是否有「待執行中斷」可以「現在」執行
    //             若有，則執行該中斷
    //             若無，不必快轉，因為 advanceClock = FALSE)
    CheckIfDue(FALSE);		    // check for pending interrupts

    // 23-0302[j]: 注意 ChangeLevel(..) 不會讓時間前進 / SetLevel(..) 才會
    ChangeLevel(IntOff, IntOn);	// re-enable interrupts (OFF->ON)

    if (yieldOnReturn) {	    // if the timer device handler asked for a context switch, ok to do it now

        yieldOnReturn = FALSE;
        status = SystemMode;		// yield is a kernel routine
        kernel->currentThread->Yield();
        status = oldStatus;
    }
}

//----------------------------------------------------------------------
// Interrupt::YieldOnReturn
// 	Called from within an interrupt handler, to cause a context switch
//	(for example, on a time slice) in the interrupted thread,
//	when the handler returns.
//
//	We can't do the context switch here, because that would switch
//	out the interrupt handler, and we want to switch out the 
//	interrupted thread.
//----------------------------------------------------------------------

void
Interrupt::YieldOnReturn()
{ 
    ASSERT(inHandler == TRUE);  
    yieldOnReturn = TRUE; 
}

//----------------------------------------------------------------------
// Interrupt::Idle
// 	Routine called when there is nothing in the ready queue.
//
//	Since something has to be running in order to put a thread
//	on the ready queue, the only thing to do is to advance 
//	simulated time until the next scheduled hardware interrupt.
//
//	If there are no pending interrupts, stop.  There's nothing
//	more for us to do.
//----------------------------------------------------------------------
/*
// 23-0302[j]:  Interrupt::Idle()
			    -	時機：當 Ready Queue = 空，由其他函示呼叫 Idle()
                -	功能：
                    -	設定 系統狀態 = IdleMode
                    -	檢查有無「待執行中斷」(pending List 是否為空)
                        -	若有，則快轉到中斷發生的時刻，並執行該中斷
                        -	若無，則關閉機器(delete kernel)
*/
void
Interrupt::Idle()
{
    DEBUG(dbgInt, "Machine idling; checking for interrupts.");
    status = IdleMode;
	DEBUG(dbgTraCode, "In Interrupt::Idle, into CheckIfDue, " << kernel->stats->totalTicks);
    if (CheckIfDue(TRUE)) {	// check for any pending interrupts
        DEBUG(dbgTraCode, "In Interrupt::Idle, return true from CheckIfDue, " << kernel->stats->totalTicks);
        status = SystemMode;
        return;			// return in case there's now
                    // a runnable thread
    }
	DEBUG(dbgTraCode, "In Interrupt::Idle, return false from CheckIfDue, " << kernel->stats->totalTicks);

    // if there are no pending interrupts, and nothing is on the ready
    // queue, it is time to stop.   If the console or the network is 
    // operating, there are *always* pending interrupts, so this code
    // is not reached.  Instead, the halt must be invoked by the user program.

    DEBUG(dbgInt, "Machine idle.  No interrupts to do.");
    cout << "No threads ready or runnable, and no pending interrupts.\n";
    cout << "Assuming the program completed.\n";
    Halt();
}

//----------------------------------------------------------------------
// Interrupt::Halt
// 	Shut down Nachos cleanly, printing out performance statistics.
//----------------------------------------------------------------------

void
Interrupt::Halt()
{
    cout << "Machine halting!\n\n";
    cout << "This is halt\n";
    kernel->stats->Print();
    delete kernel;	// Never returns. // 23-0419[j]: Delete kernel 物件 -> Thread 停止運作
}


// 23-0419[j]: 以下本來就註解掉了(MP1 不需透過以下完成)
/*
void 
Interrupt::PrintInt(int number)
{
    kernel->PrintInt(number);
    //kernel->synchConsoleOut->consoleOutput->PutInt(number);   
}

int
Interrupt::CreateFile(char *filename)
{
    return kernel->CreateFile(filename);
}

OpenFileId
Interrupt::OpenFile(char *name)
{
    return kernel->OpenFile(name);
}

int
Interrupt::WriteFile(char *buffer, int size, OpenFileId id)
{
    return kernel->WriteFile(buffer, size, id);
}

int
Interrupt::ReadFile(char *buffer, int size, OpenFileId id)
{
    return kernel->ReadFile(buffer, size, id);
}

int
Interrupt::CloseFile(OpenFileId id)
{
        return kernel->CloseFile(id);
}
*/
//----------------------------------------------------------------------
// Interrupt::Schedule
// 	Arrange for the CPU to be interrupted when simulated time
//	reaches "now + when".
//
//	Implementation: just put it on a sorted list.
//
//	**NOTE: the Nachos kernel should not call this routine directly.
//	Instead, it is only called by the hardware device simulators.**
//
//	"toCall" is the object to call when the interrupt occurs
//	"fromNow" is how far in the future (in simulated time) the 
//		 interrupt is to occur
//	"type" is the hardware device that generated the interrupt
//----------------------------------------------------------------------
// 22-1231[j]: 功能：(安排未來要發生的中斷) 從現在起，經過 fromNow 時間，引發中斷，並呼叫 *toCall函數
// 22-1231[j]: 參數：
// 22-1231[j]: CallBackObj *toCall  代表 中斷發生時 要呼叫的對象
// 22-1231[j]: int fromNow          代表 從現在起，經過「fromNow」時間後，引發中斷
// 22-1231[j]: IntType type         代表「中斷的類型」= 哪個硬體引發中斷
// 22-1231[j]: 機制：
// 22-1231[j]: (1) 計算 現在時間+fromNow = when = 要發生中斷的時刻
// 22-1231[j]: (2) 並在 PendingInterrupt 的 Sorted List = pending 中，新增「待執行中斷，發生時刻 = when時刻，中斷時呼叫toCall」

void
Interrupt::Schedule(CallBackObj *toCall, int fromNow, IntType type)
{
    int when = kernel->stats->totalTicks + fromNow;
    PendingInterrupt *toOccur = new PendingInterrupt(toCall, when, type);

    DEBUG(dbgInt, "Scheduling interrupt handler the " << intTypeNames[type] << " at time = " << when);
    ASSERT(fromNow > 0);

    // 22-1231[j]: pending 是個 Sorted List，依序存放「待執行中斷 類別」
    pending->Insert(toOccur); 
}

//----------------------------------------------------------------------
// Interrupt::CheckIfDue
// 	Check if any interrupts are scheduled to occur, and if so, 
//	fire them off.
//
// Returns:
//	TRUE, if we fired off any interrupt handlers
// Params:
//	"advanceClock" -- if TRUE, there is nothing in the ready queue,
//		so we should simply advance the clock to when the next 
//		pending interrupt would occur (if any).
//----------------------------------------------------------------------

/*  23-0301[j]: 檢查是否有「待執行中斷」可以在「現在」執行
                -	若有，則執行下個中斷
				-	若無 = 下個中斷發生的時間未到
					-	advanceClock = FALSE：直接 reutrn FALSE，表示沒有「可發生中斷」
					-	advanceClock = TRUE：快轉時間 + 執行下個中斷
*/

bool
Interrupt::CheckIfDue(bool advanceClock)
{
    PendingInterrupt *next;
    Statistics *stats = kernel->stats;

    ASSERT(level == IntOff);		// interrupts need to be disabled,
    
    // to invoke an interrupt handler

    // 23-0103[j]: Debug功能開啟時，輸出 Interrupt 的狀態
    if (debug->IsEnabled(dbgInt)) {
        DumpState();
    }
    
    // 23-0101[j]: 注意!! pending = Sorted List
    if (pending->IsEmpty()) {   	// no pending interrupts
        return FALSE;	
    }	

    next = pending->Front();

    // 23-0301[j]: 若下個待執行中斷「應發生的時間」還沒到
    //             則依據 advanceClock 參數 決定要不要「快轉」模擬時間 到「應發生時間」
    //             (1)  advanceClock = TRUE：快轉 模擬時間 到「應發生下個中斷的時間」
    //             (2)  advanceClock = FALSE：不快轉 模擬時間，Return False
    if (next->when > stats->totalTicks) {
        if (!advanceClock) {		// not time yet
            return FALSE;
        }
        else {      		// advance the clock to next interrupt
	        stats->idleTicks += (next->when - stats->totalTicks);
	        stats->totalTicks = next->when;
        // UDelay(1000L); // rcgood - to stop nachos from spinning.
	    }
    }

    DEBUG(dbgInt, "Invoking interrupt handler for the ");
    DEBUG(dbgInt, intTypeNames[next->type] << " at time " << next->when);

    if (kernel->machine != NULL) {
    	kernel->machine->DelayedLoad(0, 0);
    }
    
    
    inHandler = TRUE;
    
    do {
        // 23-0101[j]: Pop 最優先的「待執行中斷」
        next = pending->RemoveFront();    // pull interrupt off list 
		
        DEBUG(dbgTraCode, "In Interrupt::CheckIfDue, into callOnInterrupt->CallBack, " << stats->totalTicks);
         
        // 23-0103[j]: 執行「最優先」的「待執行中斷 的 ISR(中斷服務程式)」
        // 23-0103[j]: = 呼叫 *callOnInterrupt物件中的方法 = callOnInterrupt->CallBack()
        next->callOnInterrupt->CallBack();// call the interrupt handler 
		
        DEBUG(dbgTraCode, "In Interrupt::CheckIfDue, return from callOnInterrupt->CallBack, " << stats->totalTicks);
	    delete next;
    } while ( !pending->IsEmpty() && (pending->Front()->when <= stats->totalTicks) );
    
    inHandler = FALSE;
    return TRUE;
}

//----------------------------------------------------------------------
// PrintPending
// 	Print information about an interrupt that is scheduled to occur.
//	When, where, why, etc.
//----------------------------------------------------------------------

static void
PrintPending (PendingInterrupt *pending)
{
    cout << "Interrupt handler "<< intTypeNames[pending->type];
    cout << ", scheduled at " << pending->when;
}

//----------------------------------------------------------------------
// DumpState
// 	Print the complete interrupt state - the status, and all interrupts
//	that are scheduled to occur in the future.
//----------------------------------------------------------------------

void
Interrupt::DumpState()
{
    cout << "Time: " << kernel->stats->totalTicks;
    cout << ", interrupts " << intLevelNames[level] << "\n";
    cout << "Pending interrupts:\n";
    pending->Apply(PrintPending);
    cout << "\nEnd of pending interrupts\n";
}


