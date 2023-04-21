// debug.h 
//	Data structures for debugging routines.
//
//	The debugging routines allow the user to turn on selected
//	debugging messages, controllable from the command line arguments
//	passed to Nachos (-d).  You are encouraged to add your own
//	debugging flags.  Please.... 

#ifndef DEBUG_H
#define DEBUG_H

#include "copyright.h"		
#include "utility.h"
#include "sysdep.h"

// The pre-defined debugging flags are:

const char dbgAll = '+';		// turn on all debug messages
const char dbgThread = 't';		// threads
const char dbgSynch = 's';		// locks, semaphores, condition vars
const char dbgInt = 'i'; 		// interrupt emulation
const char dbgMach = 'm'; 		// machine emulation
const char dbgDisk = 'd'; 		// disk emulation
const char dbgFile = 'f'; 		// file system
const char dbgAddr = 'a'; 		// address spaces
const char dbgNet = 'n'; 		// network emulation
const char dbgSys = 'u';        // systemcall
const char dbgTraCode = 'c';

// 23-0302[j]: MP3 新增
const char dbgSch = 'z';

class Debug {
  public:
    Debug(char *flagList);

    bool IsEnabled(char flag);

  private:
    char *enableFlags;		// controls which DEBUG messages are printed
};

extern Debug *debug;


//----------------------------------------------------------------------
// DEBUG
//      If flag is enabled, print a message.
//
// 22-1223[j]: #define 中分行 要使用 '\'
//----------------------------------------------------------------------
// 22-1223[j]: DEBUG(flag,expr) 
//             即 IsEnabled(flag) 的延伸 -> 檢查 debug 的 "flag"代表之功能是否開啟，若開啟 則印出

#define DEBUG(flag,expr)                                                     \
    if (!debug->IsEnabled(flag)) {} else { 				\
        cerr << expr << "\n";   				        \
    }


//----------------------------------------------------------------------
// ASSERT
//      If condition is false,  print a message and dump core.
//	Useful for documenting assumptions in the code.
//
//	NOTE: needs to be a #define, to be able to print the location 
//	where the error occurred.
//----------------------------------------------------------------------
// 22-1224[j]: 若 condition 不成立，則印出 錯誤訊息 __FILE__ (ex. main.cc) & __LINE__ (行數)
#define ASSERT(condition)                                               \
    if (condition) {} else { 						\
	cerr << "Assertion failed: line " << __LINE__ << " file " << __FILE__ << "\n";      \
        Abort();                                                              \
    }

//----------------------------------------------------------------------
// ASSERTNOTREACHED
//      Print a message and dump core (equivalent to ASSERT(FALSE) without
//	making the compiler whine).  Useful for documenting when
//	code should not be reached.
//
//	NOTE: needs to be a #define, to be able to print the location 
//	where the error occurred.
//----------------------------------------------------------------------
// 22-1224[j]: 印出 錯誤訊息 __FILE__ (ex. main.cc) & __LINE__ (行數)

#define ASSERTNOTREACHED()                                             \
    { 						\
	cerr << "Assertion failed: line " << __LINE__ << " file " << __FILE__ << "\n";      \
        Abort();                                                              \
    }

//----------------------------------------------------------------------
// ASSERTUNIMPLEMENTED
//     Print a message that unimplemented code is executed and dump core
//----------------------------------------------------------------------
#define UNIMPLEMENTED()                                                      \
{                                                                            \
  cerr << "Reached UNIMPLEMENTED function " << __FUNCTION__ << " in file: "  \
       << __FILE__ << " line: " << __LINE__ << ".\n";                        \
}

#endif // DEBUG_H
