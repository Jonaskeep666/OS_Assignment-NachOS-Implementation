// main.cc 
//	Driver code to initialize, selftest, and run the 
//	operating system kernel.  
//
// Usage: nachos -d <debugflags> -rs <random seed #>
//              -s -x <nachos file> -ci <consoleIn> -co <consoleOut>
//              -f -cp <unix file> <nachos file>
//              -p <nachos file> -r <nachos file> -l -D
//              -n <network reliability> -m <machine id>
//              -z -K -C -N
//
//    -d causes certain debugging messages to be printed (see debug.h)
//    -rs causes Yield to occur at random (but repeatable) spots
//    -z prints the copyright message
//    -s causes user programs to be executed in single-step mode
//    -x runs a user program
//    -ci specify file for console input (stdin is the default)
//    -co specify file for console output (stdout is the default)
//    -n sets the network reliability
//    -m sets this machine's host id (needed for the network)
//    -K run a simple self test of kernel threads and synchronization
//    -C run an interactive console test
//    -N run a two-machine network test (see Kernel::NetworkTest)
//
// 23-0427[j]: 檔案系統相關的指令
//
//    Filesystem-related flags: 
//    -f forces the Nachos disk to be formatted
//    -cp copies a file from UNIX to Nachos
//    -p prints a Nachos file to stdout
//    -r removes a Nachos file from the file system
//    -l lists the contents of the Nachos directory
//    -D prints the contents of the entire file system 
//
//  Note: the file system flags are not used if the stub filesystem
//        is being used
//

#define MAIN
#include "copyright.h"
#undef MAIN

#include "main.h"
#include "filesys.h"
#include "openfile.h"
#include "sysdep.h"

// global variables
Kernel *kernel;
Debug *debug;


//----------------------------------------------------------------------
// Cleanup
//	Delete kernel data structures; called when user hits "ctl-C".
//----------------------------------------------------------------------
static void 
Cleanup(int x) 
{     
    cerr << "\nCleaning up after signal " << x << "\n";
    delete kernel; 
}

//-------------------------------------------------------------------
// Constant used by "Copy" and "Print"
//   It is the number of bytes read from the Unix file (for Copy)
//   or the Nachos file (for Print) by each read operation
//-------------------------------------------------------------------
// 23-0419[j]: File System 的 Buffer Size (in Bytes)
static const int TransferSize = 128; 


#ifndef FILESYS_STUB
//----------------------------------------------------------------------
// Copy
//      Copy the contents of the UNIX file "from" to the Nachos file "to"
//----------------------------------------------------------------------
// 23-0427[j]: 將 Host File (from) 複製「Name、Content」到 new NachOS File (to)


static void
Copy(char *from, char *to)
{
    int fd; // 23-0419[j]: Host File desriptor，用來指向 Process Open-File Table 的 Entry

    // 23-0427[j]: openFile 物件代表 NachOS File Header & SeekPosition
    OpenFile* openFile;
    int amountRead, fileLength;
    char *buffer;

    // cout << "Open UNIX file" << endl;
// Open UNIX file
    // 23-0427[j]: 直接呼叫 Linux POSIX API
    if ((fd = OpenForReadWrite(from,FALSE)) < 0) {       
        printf("Copy: couldn't open input file %s\n", from);
        return;
    }

    // cout << "Figure out length of UNIX file" << endl;
// Figure out length of UNIX file
    // 23-0427[j]: 直接呼叫 Linux POSIX API
    //             Lseek(fd,offset,Seek位置)：將 Current Pointer 從「Seek位置」移動 offset 距離
    //             -   SEEK_SET = 0 (檔案 頭)
    //             -   SEEK_CUR = 1 (目前位置)
    //             -   SEEK_END = 2 (檔案 尾)
    //             Tell(fd)：return Current Pointer 的位置
    Lseek(fd, 0, 2);            
    fileLength = Tell(fd);
    Lseek(fd, 0, 0);


// Create a Nachos file of the same length 
    DEBUG('f', "Copying file " << from << " of size " << fileLength <<  " to file " << to);

    // 23-0507[j]: 呼叫 kernel->fileSystem->Create() 來建立 new NachOS File
    //             修改 Dir & Bitmap 並建立 File Header
    if (!kernel->fileSystem->Create(to, fileLength,0)) {   // Create Nachos file
        printf("Copy: couldn't create output file %s\n", to);
        Close(fd);  // 23-0427[j]: 若 無法建立 new NachOS File -> 則 Close Host File
        return;
    }

    // cout << "Open NachOS File" <<endl;
    // 23-0507[j]: 呼叫 kernel->fileSystem->Open() 來開啟 new File
    //             = Load File Header 到 Memory
    //             之後才能操作 File 的 R/W
    openFile = kernel->fileSystem->Open(to);

    ASSERT(openFile != NULL);
    
// Copy the data in TransferSize chunks
    // 23-0427[j]: new 一個 char buffer[TransferSize]，TransferSize = 128 B
    //             因為一個「模擬 Disk」的 Sector = 128 B
    buffer = new char[TransferSize];

    // 23-0427[j]: 一次讀取 Host File 的 TransferSize Bytes 到 buffer
    //             並呼叫 openFile->Write(..) 將 buffer 中的 amountRead Bytes 寫入 NachOS File

    // cout << "Start to read Host File" << endl;
    while ((amountRead=ReadPartial(fd, buffer, sizeof(char)*TransferSize)) > 0)
        openFile->Write(buffer, amountRead);    
    delete [] buffer;

// Close the UNIX and the Nachos files
    // 23-0507[j]: 注意 Close NachOS File = delete openFile物件
    delete openFile;
    Close(fd);

    cout << "Copy in main.c done" << endl;
}

#endif // FILESYS_STUB

//----------------------------------------------------------------------
// Print
//      Print the contents of the Nachos file "name".
//----------------------------------------------------------------------
// 23-0507[j]: 印出 名為 name 的 NachOS File Content
void
Print(char *name)
{
    OpenFile *openFile;    
    int i, amountRead;
    char *buffer;

    // 23-0507[j]: Load NachOS File Header 到 Memory
    if ((openFile = kernel->fileSystem->Open(name)) == NULL) { 
        printf("Print: unable to open file %s\n", name);
        return;
    }
    
    // 23-0507[j]: 依序讀取每個 Data Blocks，並以 char 格式印出
    buffer = new char[TransferSize];
    while ((amountRead = openFile->Read(buffer, TransferSize)) > 0)
        for (i = 0; i < amountRead; i++)
            printf("%c", buffer[i]);
    delete [] buffer;

    delete openFile;            // close the Nachos file

    cout << " Print done!!" << endl;
    return;
}



//----------------------------------------------------------------------
// main
// 	Bootstrap the operating system kernel.  
//	
//	Initialize kernel data structures
//	Call some test routines
//	Call "Run" to start an initial user program running
//
//	"argc" is the number of command line arguments (including the name
//		of the command) -- ex: "nachos -d +" -> argc = 3 
//	"argv" is an array of strings, one for each command line argument
//		ex: "nachos -d +" -> argv = {"nachos", "-d", "+"}
//----------------------------------------------------------------------

// 23-0130[j]: main.cc = NachOS 主程式
//             
int
main(int argc, char **argv)
{
    int i;
    char *debugArg = "";
    char *userProgName = NULL;        // default is not to execute a user prog
    bool threadTestFlag = false;
    bool consoleTestFlag = false;
    bool networkTestFlag = false;

// 23-0507[j]: 若採用 Real NachOS File System
#ifndef FILESYS_STUB

    // 23-0507[j]: 宣告一些會用到的工具
    char *copyUnixFileName = NULL;    // UNIX file to be copied into Nachos
    char *copyNachosFileName = NULL;  // name of copied file in Nachos
    char *printFileName = NULL; 
    char *removeFileName = NULL;
    bool dirListFlag = false;
    bool dumpFlag = false;

    // 23-0510[j]: MP4 實作 Subdirectory 用到的工具
    char *subDirPath = NULL;
    char *dirPath = NULL;
    bool recurListFlag = false;
    bool recurRemove = false;
    
#endif //FILESYS_STUB

    // some command line arguments are handled here.
    // those that set kernel parameters are handled in
    // the Kernel constructor
    // 23-0419[j]: 定義 Command line 指令 的動作

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
      	    ASSERT(i + 1 < argc);   // next argument is debug string
            debugArg = argv[i + 1];
	        i++;
	      }
      	else if (strcmp(argv[i], "-z") == 0) {
            cout << copyright << "\n";
      	}
      	else if (strcmp(argv[i], "-x") == 0) {
            ASSERT(i + 1 < argc);
       	    userProgName = argv[i + 1];
       	    i++;
      	}
      	else if (strcmp(argv[i], "-K") == 0) {
      	    threadTestFlag = TRUE;
      	}
      	else if (strcmp(argv[i], "-C") == 0) {
      	    consoleTestFlag = TRUE;
      	}
      	else if (strcmp(argv[i], "-N") == 0) {
      	    networkTestFlag = TRUE;
      	}

#ifndef FILESYS_STUB
// 23-0507[j]: 若採用 Real NachOS File System

        // 23-0507[j]: '-cp' 將 Host File 複製到 NachOS FS 中
        else if (strcmp(argv[i], "-cp") == 0) {
            ASSERT(i + 2 < argc);
            copyUnixFileName = argv[i + 1];
            copyNachosFileName = argv[i + 2];
            i += 2;
        }
        // 23-0507[j]: '-p' 印出 NachOS File Content
        else if (strcmp(argv[i], "-p") == 0) {
            ASSERT(i + 1 < argc);
            printFileName = argv[i + 1];
            i++;
        }
        // 23-0507[j]: '-r' 刪除 NachOS File
        else if (strcmp(argv[i], "-r") == 0) {
            ASSERT(i + 1 < argc);
            removeFileName = argv[i + 1];
            i++;
        }
        // 23-0507[j]: '-l' List Nachos Directory 下的 子目錄 & 檔案  
        else if (strcmp(argv[i], "-l") == 0) {
            dirListFlag = true;
            // 23-0511[j]: MP4
            dirPath = argv[i + 1];
            i++;
        }
        // 23-0507[j]: '-D' 印出 整個 Nachos File System 的內容
        else if (strcmp(argv[i], "-D") == 0) {
            dumpFlag = true;
        }

        // 23-0510[j]: '-mkdir' 建立一個 新資料夾(new Dir)
        else if (strcmp(argv[i], "-mkdir") == 0) {
            subDirPath = argv[i + 1];
            i++;
        }
        // 23-0511[j]: '-lr' 印出 dirPath 下的 子目錄/File 及其下的所有 子目錄/File
        else if (strcmp(argv[i], "-lr") == 0) {
            recurListFlag = true;
            dirPath = argv[i + 1];
            i++;
        }
        // 23-0511[j]: '-rr' 刪除 某個 dirPath 下的 子目錄/File 及其下的所有 子目錄/File
        else if (strcmp(argv[i], "-rr") == 0) {
            recurRemove = true;
            dirPath = argv[i + 1];
            i++;
        }
#endif //FILESYS_STUB

    	  else if (strcmp(argv[i], "-u") == 0) {
            cout << "Partial usage: nachos [-z -d debugFlags]\n";
            cout << "Partial usage: nachos [-x programName]\n";
	          cout << "Partial usage: nachos [-K] [-C] [-N]\n";
#ifndef FILESYS_STUB
            cout << "Partial usage: nachos [-cp UnixFile NachosFile]\n";
            cout << "Partial usage: nachos [-p fileName] [-r fileName]\n";
            cout << "Partial usage: nachos [-l] [-D]\n";
#endif //FILESYS_STUB
	      }  
    }
    
    debug = new Debug(debugArg);
    
    DEBUG(dbgThread, "Entering main");

    kernel = new Kernel(argc, argv);

    kernel->Initialize(); 

    CallOnUserAbort(Cleanup);		// if user hits ctl-C

    // at this point, the kernel is ready to do something
    // run some tests, if requested
    if (threadTestFlag) {
      kernel->ThreadSelfTest();  // test threads and synchronization
    }
    if (consoleTestFlag) {
      kernel->ConsoleTest();   // interactive test of the synchronized console
    }
    if (networkTestFlag) {
      kernel->NetworkTest();   // two-machine test of the network
    }

#ifndef FILESYS_STUB
// 23-0507[j]: 若採用 Real NachOS File System
//             根據 Command line 來操作 NachOS File

    if (removeFileName != NULL) {
      kernel->fileSystem->Remove(removeFileName);
    }
    if (copyUnixFileName != NULL && copyNachosFileName != NULL) {
      Copy(copyUnixFileName,copyNachosFileName);
    }
    if (dumpFlag) {
      kernel->fileSystem->Print();
    }
    if (dirListFlag) {
      // kernel->fileSystem->List();
      // 23-0511[j]: MP4
      kernel->fileSystem->List(dirPath,FALSE);
    }
    if (printFileName != NULL) {
      Print(printFileName);
    }
    // 23-0511[j]: MP4
    if(subDirPath != NULL) {
        kernel->fileSystem->Create(subDirPath,0,1);
    }
    if(recurListFlag) {
       kernel->fileSystem->List(dirPath,TRUE);
    }
    if(recurRemove) {
        kernel->fileSystem->RecursiveRemove(dirPath);
    }
#endif // FILESYS_STUB

    // finally, run an initial user program if requested to do so

// 23-0507[j]: 執行 User Program
//             -    kernel->ExecAll(); -> Fork() -> secheduler->Run()
//                  -> ForkExecute() -> Load() -> Open()
		kernel->ExecAll();
    // If we don't run a user program, we may get here.
    // Calling "return" would terminate the program.
    // Instead, call Halt, which will first clean up, then
    //  terminate.
//    kernel->interrupt->Halt(); 
    
    ASSERTNOTREACHED();
}

