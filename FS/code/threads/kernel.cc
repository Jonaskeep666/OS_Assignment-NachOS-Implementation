// kernel.cc 
//	Initialization and cleanup routines for the Nachos kernel.
#include "copyright.h"
#include "debug.h"
#include "main.h"
#include "kernel.h"
#include "sysdep.h"
#include "synch.h"
#include "synchlist.h"
#include "libtest.h"
#include "string.h"
#include "synchdisk.h"
#include "post.h"
#include "synchconsole.h" 

// 23-0419[j]: 本檔案 主要進行 kernel 物件的初始化 & 測試
//             更多細節功能的實作 分散在其他 Source Code File

//----------------------------------------------------------------------
// Kernel::Kernel
// 	Interpret command line arguments in order to determine flags 
//	for the initialization (see also comments in main.cc)  
//----------------------------------------------------------------------
// 23-0126[j]: 根據命令列參數，設定初始化旗標

Kernel::Kernel(int argc, char **argv)
{
    randomSlice = FALSE; 
    debugUserProg = FALSE;
    consoleIn = NULL;          // default is stdin
    consoleOut = NULL;         // default is stdout
#ifndef FILESYS_STUB
    formatFlag = FALSE;
#endif
    reliability = 1;            // network reliability, default is 1.0
    hostName = 0;               // machine id, also UNIX socket name
                                // 0 is the default machine id

    // 23-0126[j]: -rs -s -e ... 等指令的功能，請參考 main.cc
    // 23-0125[j]: 根據 argc(傳入參數個數) & argv[1]、argv[2]... (傳入參數) 做相應動作

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-rs") == 0) {
 	    	ASSERT(i + 1 < argc);           // 23-0126[j]: 若讀取參數個數 超過總個數 則發出警告
                                            // 23-0126[j]: atoi("str")是將 字串str轉為 int
	    	RandomInit(atoi(argv[i + 1]));  // initialize pseudo-random 
			// number generator
	    	randomSlice = TRUE;
	    	i++;
        } else if (strcmp(argv[i], "-s") == 0) {
            debugUserProg = TRUE;
		} else if (strcmp(argv[i], "-e") == 0) {
            // 23-0126[j]: execfile 是 class kernel 的成員 char* execfile[10] in kernel.h
            // 23-0126[j]: 字元指標 execfile 指向 檔案名稱
        	execfile[++execfileNum]= argv[++i];
			cout << execfile[execfileNum] << "\n";
        
        // 23-0301[j]: MP3 實作，在此處新增「命令列參數 "-ep"」來初始化 Process 的優先權
        } else if (strcmp(argv[i], "-ep") == 0) {
            execfile[++execfileNum]= argv[++i];

            execfilePry[execfileNum] = atoi(argv[i + 1]);
            cout << execfile[execfileNum] << ", Priority(" << execfilePry[execfileNum] << ")\n";

		} else if (strcmp(argv[i], "-ci") == 0) {
	    	ASSERT(i + 1 < argc);
	    	consoleIn = argv[i + 1];
	    	i++;
		} else if (strcmp(argv[i], "-co") == 0) {
	    	ASSERT(i + 1 < argc);
	    	consoleOut = argv[i + 1];
	    	i++;
#ifndef FILESYS_STUB
		} else if (strcmp(argv[i], "-f") == 0) {
	    	formatFlag = TRUE;
#endif
        } else if (strcmp(argv[i], "-n") == 0) {
            ASSERT(i + 1 < argc);   // next argument is float
            reliability = atof(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-m") == 0) {
            ASSERT(i + 1 < argc);   // next argument is int
            hostName = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-u") == 0) {
            cout << "Partial usage: nachos [-rs randomSeed]\n";
	   		cout << "Partial usage: nachos [-s]\n";
            cout << "Partial usage: nachos [-ci consoleIn] [-co consoleOut]\n";
#ifndef FILESYS_STUB
	    	cout << "Partial usage: nachos [-nf]\n";
#endif
            cout << "Partial usage: nachos [-n #] [-m #]\n";
		}
    }
}

//----------------------------------------------------------------------
// Kernel::Initialize
// 	Initialize Nachos global data structures.  Separate from the 
//	constructor because some of these refer to earlier initialized
//	data via the "kernel" global variable.
//----------------------------------------------------------------------
// 23-0126[j]: 對 Object kernel 進行初始化
// 23-0126[j]: 此方法 不整合在 Constructor kernel(..) 
//             是因為 此方法 需要使用 已建立之kernel物件中的變數

void
Kernel::Initialize()
{
    // We didn't explicitly allocate the current thread we are running in.
    // But if it ever tries to give up the CPU, we better have a Thread
    // object to save its state. 

	// 23-0126[j]: 將正在使用CPU的Thread 設為「new Thread = main主程式」
    currentThread = new Thread("main", threadNum++);	
    currentThread->setStatus(RUNNING);     

    // 23-0304[j]:  MP3 幫 main 設定為最高優先
    currentThread->setPriority(149);  

    stats = new Statistics();		// collect statistics
    interrupt = new Interrupt;		// start up interrupt handling
    scheduler = new Scheduler();	// initialize the ready queue
    alarm = new Alarm(randomSlice);	// start up time slicing
    machine = new Machine(debugUserProg);
    synchConsoleIn = new SynchConsoleInput(consoleIn); // input from stdin
    synchConsoleOut = new SynchConsoleOutput(consoleOut); // output to stdout
    synchDisk = new SynchDisk();    //

    // 23-0131[j]: 建立一個 AV List
    avList = new List<int>();
    for(int i=0;i<NumPhysPages;i++) 
        avList->Append(i);

#ifdef FILESYS_STUB
    fileSystem = new FileSystem();
#else
    fileSystem = new FileSystem(formatFlag);
#endif // FILESYS_STUB
    // 23-0301[j]: 應 MP3 要求，將以下註解掉
    // postOfficeIn = new PostOfficeInput(10);
    // postOfficeOut = new PostOfficeOutput(reliability);

    interrupt->Enable();
}

//----------------------------------------------------------------------
// Kernel::~Kernel
// 	Nachos is halting.  De-allocate global data structures.
//----------------------------------------------------------------------
Kernel::~Kernel()
{
    delete stats;
    delete interrupt;
    delete scheduler;
    delete alarm;
    delete machine;
    delete synchConsoleIn;
    delete synchConsoleOut;
    delete synchDisk;
    delete fileSystem;
    // 23-0301[j]: 應 MP3 要求，將以下註解掉
    // delete postOfficeIn;
    // delete postOfficeOut;
    
    Exit(0);
}

//----------------------------------------------------------------------
// Kernel::ThreadSelfTest
//      Test threads, semaphores, synchlists
//----------------------------------------------------------------------
// 23-0419[j]: 測試 thread 的 Semaphore 功能
void
Kernel::ThreadSelfTest() {
   Semaphore *semaphore;
   SynchList<int> *synchList;
   
   LibSelfTest();		// test library routines
   
   currentThread->SelfTest();	// test thread switching
   
   				// test semaphore operation
   semaphore = new Semaphore("test", 0);
   semaphore->SelfTest();
   delete semaphore;
   
   				// test locks, condition variables
				// using synchronized lists
   synchList = new SynchList<int>;
   synchList->SelfTest(9);
   delete synchList;

   cout << "Test done!" <<endl;

}

//----------------------------------------------------------------------
// Kernel::ConsoleTest
//      Test the synchconsole
//----------------------------------------------------------------------
// 23-0419[j]: 測試「模擬的 輸出輸入 機器」
void
Kernel::ConsoleTest() {
    char ch;

    cout << "Testing the console device.\n" 
        << "Typed characters will be echoed, until ^D is typed.\n"
        << "Note newlines are needed to flush input through UNIX.\n";
    cout.flush();

    do {
        ch = synchConsoleIn->GetChar();
        if(ch != EOF) synchConsoleOut->PutChar(ch);   // echo it!
    } while (ch != EOF);

    cout << "\n";

}

//----------------------------------------------------------------------
// Kernel::NetworkTest
//      Test whether the post office is working. On machines #0 and #1, do:
//
//      1. send a message to the other machine at mail box #0
//      2. wait for the other machine's message to arrive (in our mailbox #0)
//      3. send an acknowledgment for the other machine's message
//      4. wait for an acknowledgement from the other machine to our 
//          original message
//
//  This test works best if each Nachos machine has its own window
//----------------------------------------------------------------------
// 23-0419[j]: 測試網路
void
Kernel::NetworkTest() {

    if (hostName == 0 || hostName == 1) {
        // if we're machine 1, send to 0 and vice versa
        int farHost = (hostName == 0 ? 1 : 0); 
        PacketHeader outPktHdr, inPktHdr;
        MailHeader outMailHdr, inMailHdr;
        char *data = "Hello there!";
        char *ack = "Got it!";
        char buffer[MaxMailSize];

        // construct packet, mail header for original message
        // To: destination machine, mailbox 0
        // From: our machine, reply to: mailbox 1
        outPktHdr.to = farHost;         
        outMailHdr.to = 0;
        outMailHdr.from = 1;
        outMailHdr.length = strlen(data) + 1;

        // Send the first message
        postOfficeOut->Send(outPktHdr, outMailHdr, data); 

        // Wait for the first message from the other machine
        postOfficeIn->Receive(0, &inPktHdr, &inMailHdr, buffer);
        cout << "Got: " << buffer << " : from " << inPktHdr.from << ", box " 
                                                << inMailHdr.from << "\n";
        cout.flush();

        // Send acknowledgement to the other machine (using "reply to" mailbox
        // in the message that just arrived
        outPktHdr.to = inPktHdr.from;
        outMailHdr.to = inMailHdr.from;
        outMailHdr.length = strlen(ack) + 1;
        postOfficeOut->Send(outPktHdr, outMailHdr, ack); 

        // Wait for the ack from the other machine to the first message we sent
	postOfficeIn->Receive(1, &inPktHdr, &inMailHdr, buffer);
        cout << "Got: " << buffer << " : from " << inPktHdr.from << ", box " 
                                                << inMailHdr.from << "\n";
        cout.flush();
    }

    // Then we're done!
}


// 23-0130[j]: ForkExecute(Thread *t)
//             - 被Exec(..)呼叫
            // - 主要功能：將 Thread(t) 對應的 Program 載入記憶體 & 配置 Page Table & 請 machine 開始執行指令
            // (1)  將 名稱為 t->getName() 的 noff檔案 = Program = Thread要執行的程式 打開
            // (2)  將 noff檔案(Program) 各個 Segment 的內容 Load 到實體記憶體中
            // (3)  若 檔案開啟失敗 return
            // (4)  若 檔案開啟成功，則
            //      配置 Page Table & 將其位址 載入machine，並請 machine 開始執行指令
void ForkExecute(Thread *t)
{
    // 23-0130[j]: 若 Thread(t) 載入失敗，則直接return
	if ( !t->space->Load(t->getName()) ) {
    	return;             // executable not found
    }
    t->space->Execute(t->getName());
}
// 23-0130[j]: Note：new Thread 的啟動流程 
//             (1)  當 有人呼叫 Scheduler->Run() 切到 new Thread 執行時
//             (2)  PC會被初始化，指向 ThreadRoot(..)
//             (3)  ThreadRoot(..) 會依序呼叫 Begin()、func()=ForkExecute(..)、Finish()
//             (4)  當 new Thread 開始執行 ForkExecute(..)
//                  會將 對應Program載入記憶體 & 分配 Page Table 
//                  並請 machine 開始執行 User Program 指令 = machine::Run() = 提取 PC指向的指令 & 執行

// 23-0130[j]: 依序對 所有待執行(讀入)的 檔案=Program 建立各自的 TCB(PCB) & Page Table
//             (最多建立 10個TCB，因為一個 Kernel物件中，只宣告 Thread* t[10])
//             之後 將 main函式 所在 Thread = mainThread Finish掉
//             = 切到 其他在ReadyQueue的 Thread
void Kernel::ExecAll()
{
	for (int i=1;i<=execfileNum;i++) {
		int a = Exec(execfile[i],execfilePry[i]);
	}
	currentThread->Finish();
}

// 23-0130[j]: Exec(char* name)
            // - 主要功能：Create 一個 TCB(PCB) & Page Table，並回傳 其ID
            // (1)  new 一個 Thread Control Block(TCB/PCB) = Thread物件
            // (2)  new 一個 Page Table = AddrSpace物件
            // (3)  配置 & 初始化 new Thread，並將其 排入 ReadyQueue
            //      (包含 未來要執行 ForkExecute(..) & 傳入參數 = new Thread位址 )
            // (4)  回傳 new Thread ID
int Kernel::Exec(char* name,int pry)
{
    // 23-0126[j]: 重要！！！主要建立一個 TCB(PCB) & Page Table的地方
    //             (1)  新建一個 Thread Control Block(TCB/PCB)
    //                  儲存 名稱為name & ID=threadNum 的 Thread資訊
    //                  並將 TCB物件的 位址 放入 t[threadNum]中 
    //             (2)  新建一個 Page Table = AddrSpace物件
    //                  未來會在 ForkExecute(..) 中呼叫 AddrSpace::Execute(..)
    //                  將 new Page Table 指定為 new Thread 的 Page Table

	t[threadNum] = new Thread(name, threadNum);
	t[threadNum]->space = new AddrSpace();
    
    // 23-0302[j]: MP3 新增
    t[threadNum]->setPriority(pry);

    // 23-0130[j]: 配置 & 初始化 t[threadNum]代表的Thread物件，並將其 排入 ReadyQueue
    //             未來要執行 ForkExecute(..) & 傳入參數=t[threadNum]
	t[threadNum]->Fork((VoidFunctionPtr) &ForkExecute, (void *)t[threadNum]);

	threadNum++;            // 23-0130[j]: kernel 管理的 Thread數量+1

	return threadNum-1;     // 23-0130[j]: 回傳新建Thread的ID

/*
    cout << "Total threads number is " << execfileNum << endl;
    for (int n=1;n<=execfileNum;n++) {
		t[n] = new Thread(execfile[n]);
		t[n]->space = new AddrSpace();
		t[n]->Fork((VoidFunctionPtr) &ForkExecute, (void *)t[n]);
		cout << "Thread " << execfile[n] << " is executing." << endl;
	}
	cout << "debug Kernel::Run finished.\n";	
*/
//  Thread *t1 = new Thread(execfile[1]);
//  Thread *t1 = new Thread("../test/test1");
//  Thread *t2 = new Thread("../test/test2");

//    AddrSpace *halt = new AddrSpace();
//  t1->space = new AddrSpace();
//  t2->space = new AddrSpace();

//    halt->Execute("../test/halt");
//  t1->Fork((VoidFunctionPtr) &ForkExecute, (void *)t1);
//  t2->Fork((VoidFunctionPtr) &ForkExecute, (void *)t2);

//	currentThread->Finish();
//    Kernel::Run();
//  cout << "after ThreadedKernel:Run();" << endl;  // unreachable
}

// 23-0131[j]: 實作 PopFreeFrame()
int Kernel::PopFreeFrame(){
    if(avList->IsEmpty())
        return -1;
    return avList->RemoveFront();
}
// 23-0201[j]: 實作 PushFreeFrame()
void Kernel::PushFreeFrame(int f){
    if(f<0) return;
    avList->Prepend(f);
    return;
}