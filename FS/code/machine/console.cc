// console.cc 
//	Routines to simulate a serial port to a console device.
//	A console has input (a keyboard) and output (a display).
//	These are each simulated by operations on UNIX files.
//	The simulated device is asynchronous, so we have to invoke 
//	the interrupt handler (after a simulated delay), to signal that 
//	a byte has arrived and/or that a written byte has departed.
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "console.h"
#include "main.h"
#include "stdio.h"
//----------------------------------------------------------------------
// ConsoleInput::ConsoleInput
// 	Initialize the simulation of the input for a hardware console device.
//
//	"readFile" -- UNIX file simulating the keyboard (NULL -> use stdin)
// 	"toCall" is the interrupt handler to call when a character arrives
//		from the keyboard
//----------------------------------------------------------------------

ConsoleInput::ConsoleInput(char *readFile, CallBackObj *toCall)
{
    if (readFile == NULL)
	readFileNo = 0;					// keyboard = stdin
    else
    	readFileNo = OpenForReadWrite(readFile, TRUE);	// should be read-only

    // set up the stuff to emulate asynchronous interrupts
    callWhenAvail = toCall;
    incoming = EOF;

    // start polling for incoming keystrokes
    kernel->interrupt->Schedule(this, ConsoleTime, ConsoleReadInt);
}

//----------------------------------------------------------------------
// ConsoleInput::~ConsoleInput
// 	Clean up console input emulation
//----------------------------------------------------------------------

ConsoleInput::~ConsoleInput()
{
    if (readFileNo != 0)
	Close(readFileNo);
}


//----------------------------------------------------------------------
// ConsoleInput::CallBack()
// 	Simulator calls this when a character may be available to be
//	read in from the simulated keyboard (eg, the user typed something).
//
//	First check to make sure character is available.
//	Then invoke the "callBack" registered by whoever wants the character.
//----------------------------------------------------------------------

void
ConsoleInput::CallBack()
{
  char c;
  int readCount;

    ASSERT(incoming == EOF);
    if (!PollFile(readFileNo)) { // nothing to be read
        // schedule the next time to poll for a packet
        kernel->interrupt->Schedule(this, ConsoleTime, ConsoleReadInt);
    } else { 
    	// otherwise, try to read a character
    	readCount = ReadPartial(readFileNo, &c, sizeof(char));
	if (readCount == 0) {
	   // this seems to happen at end of file, when the
	   // console input is a regular file
	   // don't schedule an interrupt, since there will never
	   // be any more input
	   // just do nothing....
	}
	else {
	  // save the character and notify the OS that
	  // it is available
	  ASSERT(readCount == sizeof(char));
	  incoming = c;
	  kernel->stats->numConsoleCharsRead++;
	}
	callWhenAvail->CallBack();
    }
}

//----------------------------------------------------------------------
// ConsoleInput::GetChar()
// 	Read a character from the input buffer, if there is any there.
//	Either return the character, or EOF if none buffered.
//----------------------------------------------------------------------

char
ConsoleInput::GetChar()
{
   char ch = incoming;

   if (incoming != EOF) {	// schedule when next char will arrive
       kernel->interrupt->Schedule(this, ConsoleTime, ConsoleReadInt);
   }
   incoming = EOF;
   return ch;
}



//----------------------------------------------------------------------
// ConsoleOutput::ConsoleOutput
// 	Initialize the simulation of the output for a hardware console device.
//
//	"writeFile" -- UNIX file simulating the display (NULL -> use stdout)
// 	"toCall" is the interrupt handler to call when a write to 
//	the display completes.
//----------------------------------------------------------------------
/*
// 23-0302[j]:  建構子：ConsoleOutput(char *writeFile, CallBackObj *toCall)
				-	功能：初始化「寫入檔案」、寫入完成時呼叫的ISR
				-	呼叫時機：在 class SynchConsoleOutput 建構時，一起建構 class ConsoleOutput
					-	consoleOutput = new ConsoleOutput(outputFile, this);
					-	this = SynchConsoleOutput物件
*/
ConsoleOutput::ConsoleOutput(char *writeFile, CallBackObj *toCall)
{
    if (writeFile == NULL)
	    writeFileNo = 1;				// display = stdout
    else
    	writeFileNo = OpenForWrite(writeFile);

    callWhenDone = toCall;
    putBusy = FALSE;
}

//----------------------------------------------------------------------
// ConsoleOutput::~ConsoleOutput
// 	Clean up console output emulation
//----------------------------------------------------------------------

ConsoleOutput::~ConsoleOutput()
{
    if (writeFileNo != 1)
	Close(writeFileNo);
}

//----------------------------------------------------------------------
// ConsoleOutput::CallBack()
// 	Simulator calls this when the next character can be output to the
//	display.
//----------------------------------------------------------------------
// 23-0103[j]: 硬體工作完成後，會呼叫「中斷服務程式 ISR」(由Timer每個Tick，去檢查有沒有「待執行中斷」)
// 23-0103[j]: 而「某個硬體的ISR」在 NachOS 稱為 class 硬體 中的 CallBack()方法
void
ConsoleOutput::CallBack()
{
	DEBUG(dbgTraCode, "In ConsoleOutput::CallBack(), " << kernel->stats->totalTicks);
    putBusy = FALSE;
    kernel->stats->numConsoleCharsWritten++; // 23-0101[j]: 在 stats 紀錄 寫入的Bytes數+1
    callWhenDone->CallBack();   
    // 23-0102[j]: callWhenDone = tocall = this = SynchConsoleOutput物件的位址
    // 23-0102[j]: 呼叫 SynchConsoleOutput物件的CallBack()方法，是為了 waitFor->V() 即宣布 目前自己 已不佔用 ConsoleOutput
}

//----------------------------------------------------------------------
// ConsoleOutput::PutChar()
// 	Write a character to the simulated display, schedule an interrupt 
//	to occur in the future, and return.
//----------------------------------------------------------------------
/*
// 23-0302[j]:  ConsoleOutput::PutChar(char ch)

*/
void
ConsoleOutput::PutChar(char ch)
{
    ASSERT(putBusy == FALSE);
    WriteFile(writeFileNo, &ch, sizeof(char));
    putBusy = TRUE;
    kernel->interrupt->Schedule(this, ConsoleTime, ConsoleWriteInt);
}
