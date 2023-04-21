// kernel.h
//	Global variables for the Nachos kernel.
// 
#ifndef KERNEL_H
#define KERNEL_H

#include "copyright.h"
#include "debug.h"
#include "utility.h"
#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "stats.h"
#include "alarm.h"
#include "filesys.h"
#include "machine.h"

class PostOfficeInput;
class PostOfficeOutput;
class SynchConsoleInput;
class SynchConsoleOutput;
class SynchDisk;

typedef int OpenFileId;

class Kernel {
  public:
    // 23-0126[j]: 初始化函數
    Kernel(int argc, char **argv);  // 23-0126[j]: 根據命令列參數，設定初始化旗標
    				// Interpret command line arguments
    ~Kernel();		        // deallocate the kernel
    
    void Initialize(); 		// initialize the kernel -- separated
				                  // from constructor because 
				                  // refers to "kernel" as a global

    // 23-0126[j]: Execution/Thread 相關函數
    void ExecAll();
    
    // 23-0302[j]: MP3 ( 主要建立一個 TCB(PCB) & Page Table的地方 )
    int Exec(char* name, int pry);

    void ThreadSelfTest();	// self test of threads and synchronization
    Thread* getThread(int threadID){return t[threadID];} // 23-0126[j]: 查看 Thread ID 的方法

    // 23-0126[j]: 測試用函數
    void ConsoleTest();         // interactive console self test
    void NetworkTest();         // interactive 2-machine network test

    // 23-0126[j]: I/O & File 相關函數
    void PrintInt(int number); 	
    int CreateFile(char* filename); // fileSystem call

    // 23-0419[j]: kernel.h 定義了介面，前輩提到「真正的實作則是在filesys.h裡面，可使用grep -nr CloseFile 來查驗」
    OpenFileId OpenFile(char* name);                       
    int WriteFile(char* buffer, int size, OpenFileId id); 
    int ReadFile(char* buffer, int size, OpenFileId id);  
    int CloseFile(OpenFileId id);

    // 23-0131[j]: Pop/Push Free Frame from AVList
    int PopFreeFrame();
    void PushFreeFrame(int f);

// These are public for notational convenience; really, 
// they're global variables used everywhere.

// 23-0126[j]: 以下大多是指標，會在 Initialize(..) 中 指向「動態配置的物件(new Object)」

    Thread *currentThread;	// the thread holding the CPU
    Scheduler *scheduler;	// the ready list
    Interrupt *interrupt;	// interrupt status
    Statistics *stats;		// performance metrics
    Alarm *alarm;		// the software alarm clock    
    Machine *machine;           // the simulated CPU
    SynchConsoleInput *synchConsoleIn;
    SynchConsoleOutput *synchConsoleOut;
    SynchDisk *synchDisk;
    FileSystem *fileSystem;     
    PostOfficeInput *postOfficeIn;
    PostOfficeOutput *postOfficeOut;

    // 23-0131[j]: 透過一個 AV-List 來儲存 所有的 Free Frame
    List<int> *avList;

    int hostName;               // machine identifier

  private:

	Thread* t[10];                // 23-0126[j]: 指向 Thread 的指標陣列(共可指向 10條 Thread)
	char*   execfile[10];         // 23-0126[j]: 指向 開啟檔案名稱 的指標陣列(共可指向 10個 File)

  // 23-0302[j]: MP3
  int execfilePry[10];          // 23-0302[j]: 儲存開啟檔案的 Priority 

	int execfileNum;              // 23-0126[j]: 開啟檔案 數量
	int threadNum;                // 23-0126[j]: 正在使用的 Threads 數量 = 最後一個 Thread 的編號
    bool randomSlice;		        // enable pseudo-random time slicing
    bool debugUserProg;         // single step user program
    double reliability;         // likelihood messages are dropped
    char *consoleIn;            // file to read console input from
    char *consoleOut;           // file to send console output to
#ifndef FILESYS_STUB
    bool formatFlag;            // format the disk if this is true
#endif
};


#endif // KERNEL_H


