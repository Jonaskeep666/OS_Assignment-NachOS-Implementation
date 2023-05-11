// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.

// 23-0419[j]: MP1 此處需要更動

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"
//----------------------------------------------------------------------
// ExceptionHandler
//
//  22-1224[j]: 這是 進入 NachOS kernel 的入口！
//
// 	**Entry point into the Nachos kernel**.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
//  22-1224[j]: Calling convention = 呼叫約定
//  22-1224[j]: 由於 硬體無法區分 哪些參數屬於哪些函數，所以在呼叫函數、參數傳遞時
//  22-1224[j]: 規定 傳入參數 & 回傳結果 要擺在「指定的暫存器」，該暫存器又稱「共享暫存器」
//  22-1228[j]: 結論: NachOS 的 System call 傳遞參數 採用「Register暫存」(不是 Table/Block 或 Stack)
//
// 	system call code -- r2	// 22-1224[j]: 存放 System Call ID 或 system call 的回傳(return)結果
//				      arg1 -- r4	// 22-1224[j]: 存放 傳入參數1 = arg1
//				      arg2 -- r5  // 22-1224[j]: 存放 傳入參數2 = arg2
//				      arg3 -- r6  // 22-1224[j]: 存放 傳入參數3 = arg3
//				      arg4 -- r7  // 22-1224[j]: 存放 傳入參數4 = arg4
//
//  22-1230[j]: (1) System Call ID 是由 System Call Interface(Start.S)傳遞
//  22-1230[j]: (2) System call 的 傳遞參數 會根據 Calling convention 自動依序放入 r4、r5、r6...
//
//	The result of the system call, if any, must be put back into r2. 
//
//  If you are handling a system call, don't forget to increment the pc
//  before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	is in machine.h.
//----------------------------------------------------------------------
// 22-1224[j]: 當 mipssim.cc 的 void Machine::OneInstruction(Instruction *instr) 出現 例外事件(Error & Syscall)
// 22-1228[j]: 也就是 CPU 運算時，運算出現錯誤、讀到需要請求 Syscall 時
// 22-1224[j]: 則會呼叫 void Machine::RaiseException(ExceptionType which, int badVAddr) 進入 Kernel Mode，並呼叫 ExceptionHandler
// 22-1224[j]: ExceptionHandler(ExceptionType which) 定義「例外事件(Trap)」要做的事
// 22-1224[j]: 其中，ExceptionType定義於Machine.h中，目前(MP1)只有 case SyscallException 一個 Trap
// 22-1224[j]: case SyscallException 又可分為不同的「type」(定義於 syscall.h 中)


void
ExceptionHandler(ExceptionType which)
{
    char ch;
    int val;
    int type = kernel->machine->ReadRegister(2);
    int status, exit, threadID, programID, fileID, numChar;
    DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");
    DEBUG(dbgTraCode, "In ExceptionHandler(), Received Exception " << which << " type: " << type << ", " << kernel->stats->totalTicks);
    
    switch (which) {
    case SyscallException:
    
      switch(type) {
        case SC_Halt:
        {
          DEBUG(dbgSys, "Shutdown, initiated by user program.\n");
          SysHalt();
          cout<<"in exception\n";
          ASSERTNOTREACHED();	// 22-1224[j]: 印出 檔案 & 行數
          break;
        } 
        case SC_PrintInt:
        {
          DEBUG(dbgSys, "Print Int\n");
          val = kernel->machine->ReadRegister(4);
          DEBUG(dbgTraCode, "In ExceptionHandler(), into SysPrintInt, " << kernel->stats->totalTicks);    
          SysPrintInt(val); 	
          DEBUG(dbgTraCode, "In ExceptionHandler(), return from SysPrintInt, " << kernel->stats->totalTicks);
          // Set Program Counter
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
          return;	// 22-1224[j]: 若需要return回User program，則需要 更新 Program counter，不然會重複呼叫 System call 
          ASSERTNOTREACHED();
          break;
        }
        case SC_MSG:
        {
          DEBUG(dbgSys, "Message received.\n");
          val = kernel->machine->ReadRegister(4);
          char *msg = &(kernel->machine->mainMemory[val]);
          cout << msg << endl;
          SysHalt(); // 22-1224[j]: 印出系統訊息後，直接終止整個OS，故不使用 return，也不需要 更新 Program counter
          ASSERTNOTREACHED();
          break;
        }
        // 23-0104[j]: *重要的範例*
        case SC_Create: 
        {
          #ifdef FILESYS_STUB

          // 23-0104[j]: 將傳入System Call的參數，從Reg[4]讀出、存入 val中
          val = kernel->machine->ReadRegister(4);
          char *filename = &(kernel->machine->mainMemory[val]);
          //cout << filename << endl;
          
          status = SysCreate(filename); // 23-0419[j]: 實際上在 ksyscall 中實作

          #else // FILESYS
          // 23-0507[j]: MP4 這裏參數個數目不同，須另外處理

          val = kernel->machine->ReadRegister(4);
          char *filename = &(kernel->machine->mainMemory[val]);
          status = SysCreate(filename, (int)kernel->machine->ReadRegister(5)); 

          #endif // FILESYS

          // 23-0104[j]: 將回傳的參數，存入Reg[2]
          kernel->machine->WriteRegister(2, (int) status);
          // Set Program Counter
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
          return; 
          ASSERTNOTREACHED();
          break;
        }
        case SC_Add:
        {
          DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");
          /* Process SysAdd Systemcall*/
          int result;
          result = SysAdd(/* int op1 */(int)kernel->machine->ReadRegister(4),
                  /* int op2 */(int)kernel->machine->ReadRegister(5));
          DEBUG(dbgSys, "Add returning with " << result << "\n");
          /* Prepare Result */
          kernel->machine->WriteRegister(2, (int)result);	
          /* Modify return point */
          
          /* set previous programm counter (debugging only)*/
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          /* set next programm counter for brach execution */
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
          
          cout << "result is " << result << "\n";	
          return;	
          ASSERTNOTREACHED();
          break;
        }

      // 23-0103[j]: MP1 作業練習
      // 23-0104[j]: 先瞭解規格 syscall.h 
      // 23-0104[j]: 修改順序 exception.cc -> ksyscall.h -> filesys.h
      // ---------------------------------------------------------------
        case SC_Open: 
        {
          DEBUG(dbgSys, "Open\n");
          val = kernel->machine->ReadRegister(4);
          char *filename = &(kernel->machine->mainMemory[val]);
          //cout << filename << endl;
          fileID = SysOpen(filename); // 23-0103[j]: 細節在ksyscall & filesys.h 

          DEBUG(dbgSys, "Open returning with " << fileID << "\n");
          kernel->machine->WriteRegister(2, (int) fileID);
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
          return;
          ASSERTNOTREACHED();
          break;
        }

        case SC_Close:
        {
          DEBUG(dbgSys, "Close\n");
          fileID = kernel->machine->ReadRegister(4);
          // cout << "fileID" << fileID << endl;
          status = SysClose(fileID);

          DEBUG(dbgSys, "Close returning with " << fileID << "\n");
          kernel->machine->WriteRegister(2, (int) status);
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
          return;
          ASSERTNOTREACHED();
          break;
        }

        case SC_Write:
        {
          DEBUG(dbgSys, "Write\n");
          val = kernel->machine->ReadRegister(4);
          char *buffer = &(kernel->machine->mainMemory[val]);
          numChar = kernel->machine->ReadRegister(5);
          fileID = kernel->machine->ReadRegister(6);
          status = SysWrite(fileID,buffer,numChar);

          // DEBUG(dbgSys, "Write returning with " << fileID << "\n");
          kernel->machine->WriteRegister(2, (int) status);
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
          return;
          ASSERTNOTREACHED();
          break;
        }

        case SC_Read:
        {
          DEBUG(dbgSys, "Read\n");
          val = kernel->machine->ReadRegister(4);
          char *buffer = &(kernel->machine->mainMemory[val]);
          numChar = kernel->machine->ReadRegister(5);
          fileID = kernel->machine->ReadRegister(6);
          status = SysRead(fileID,buffer,numChar);

          // DEBUG(dbgSys, "Read returning with " << fileID << "\n");
          kernel->machine->WriteRegister(2, (int) status);
          kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
          kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
          kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
          return;
          ASSERTNOTREACHED();
          break;
        }
      // ---------------------------------------------------------------
            
        case SC_Exit:
        {
          DEBUG(dbgAddr, "Program exit\n");
          val=kernel->machine->ReadRegister(4);
          cout << "return value:" << val << endl;
          kernel->currentThread->Finish();
          break;
        }
      default:
      {
        cerr << "Unexpected system call " << type << "\n";
        break;
          }
    }	// 22-1224[j]: case SyscallException: switch(type) {...}

	  break;
    
    // case PageFaultException:
    //   // 23-0131[j]: 判斷 是 invalid-ref 或 Not-in-memory

    //   // PC-4 = Restart Last inst. 
    //   // int PrevPC = kernel->machine->ReadRegister(PrevPCReg);
    //   // int PC = kernel->machine->ReadRegister(PCReg);
    //   // int NextPC = kernel->machine->ReadRegister(NextPCReg);

    //   // kernel->machine->WriteRegister(PCReg,PrevPC);
    //   // kernel->machine->WriteRegister(NextPCReg,PC);
 
    // break;

	  default:
		  cerr << "Unexpected user mode exception " << (int)which << "\n";
		break;

  }	// 22-1224[j]:  for switch (which) {...}
    
    ASSERTNOTREACHED();
}

