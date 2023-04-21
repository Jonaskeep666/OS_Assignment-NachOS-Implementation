// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -n -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you are using the "stub" file system, you
//		don't need to do this last step)

// 23-0419[j]: 管理 Thread 的記憶體
//             ex. 將 User program (noff檔案=fileName) 載入到 主記憶體中

#include "copyright.h"
#include "main.h"
#include "addrspace.h"
#include "machine.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
#ifdef RDATA
    noffH->readonlyData.size = WordToHost(noffH->readonlyData.size);
    noffH->readonlyData.virtualAddr = 
           WordToHost(noffH->readonlyData.virtualAddr);
    noffH->readonlyData.inFileAddr = 
           WordToHost(noffH->readonlyData.inFileAddr);
#endif 
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);

#ifdef RDATA
    DEBUG(dbgAddr, "code = " << noffH->code.size <<  
                   " readonly = " << noffH->readonlyData.size <<
                   " init = " << noffH->initData.size <<
                   " uninit = " << noffH->uninitData.size << "\n");
#endif
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//----------------------------------------------------------------------
// 23-0131[j]: AddrSpace::AddrSpace() 是用來 建立/初始化 Page Table
//             ( Page Table 最多 128個Entry (定義在 machine.h) )
//             此處是尚未修改前的 AddrSpace()，只有一個 unsegmented page table

// 23-0127[j]: 注意 machine 內的 translate.h 有硬體支援的 Page Table 結構(不可更動)
//             包含欄位
            // (1) virtualPage(Page Num) - physicalPage(Frame Num)
            // (2) valid(Valid-invalid Bit)
            // (3) readOnly(不可修改)
            // (4) use(Ref. Bit): 是否被存取過
            // (5) dirty(Dirty Bit): 是否被修改過

AddrSpace::AddrSpace()
{
    pageTable = new TranslationEntry[NumPhysPages]; 
    // 23-0127[j]: Page Table 最多 128個Entry (定義在 machine.h)
    for (int i = 0; i < NumPhysPages; i++) {
        pageTable[i].virtualPage = i;	
        pageTable[i].physicalPage = -1;
        pageTable[i].valid = FALSE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;  
    }
    
    // zero out the entire address space
    bzero(kernel->machine->mainMemory, MemorySize);
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    // 23-0201[j]: 我自己加一個 回收 Free Frame 的機制
    for(int i=0;i<numPages;i++){
        if(pageTable[i].valid){
            kernel->PushFreeFrame(pageTable[i].physicalPage);
            pageTable[i].physicalPage = -1;
            pageTable[i].valid = FALSE;
        }
    }
   delete pageTable;
}


//----------------------------------------------------------------------
// AddrSpace::Load
// 	Load a user program into memory from a file.
//
//	Assumes that the page table has been initialized, and that
//	the object code file is in NOFF format.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------
// 23-0131[j]: 由 ForkExecute(..) 呼叫
// 23-0127[j]: 將 User program (noff檔案=fileName) 載入到 主記憶體中
//             -	決定 Program 的大小 = Code + Data + BSS + Stack
//             -	將 Program 各個 Segment 的內容 Load 到實體記憶體中
//                  (需分配 Free Frame 給相應的 Page)
//             -	若 檔案開啟失敗 return false
bool 
AddrSpace::Load(char *fileName) 
{
    OpenFile *executable = kernel->fileSystem->Open(fileName);
    NoffHeader noffH;
    unsigned int size;

    if (executable == NULL) {
        cerr << "Unable to open file " << fileName << "\n";
        return FALSE;
    }

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);   // 23-0127[j]: 讀出 noff header 存入 noffH

    // 23-0127[j]: 若檔案採用 Big Endian ，則轉成 Little Endian
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

#ifdef RDATA
// how big is address space?
    size = noffH.code.size + noffH.readonlyData.size + noffH.initData.size +
           noffH.uninitData.size + UserStackSize;	
                                                // we need to increase the size
						// to leave room for the stack
#else
// how big is address space?

// 23-0131[j]: 根據 Program 所需的 size = Code + Data + BSS + UserStackSize 來決定 Pages 的數量
//             (UserStackSize 定義在 addrspace.h)
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size to leave room for the stack
#endif

    numPages = divRoundUp(size, PageSize);  // 23-0127[j]: 設定 Page數量，總Page大小 ≥ Process 所需大小
    size = numPages * PageSize;

// 23-0127[j]: 在「實作 Virtual Memory 之前」Process空間 必須小於 實體空間
    ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG(dbgAddr, "Initializing address space: " << numPages << ", " << size);

// then, copy in the code and data segments into memory
// Note: this code assumes that virtual address = physical address

// 23-0127[j]: 將 檔案(executable)中「noffH.code.inFileAddr位置 noffH.code.size大小」的資料
//             存入 主記憶體 kernel->machine->mainMemory[noffH.code.virtualAddr]

// 23-0131[j]: MP2 的實作區域
//             -	修改前
//                  -   原本這裡直接將 Program資料 寫入 kernel->machine->mainMemory[virtualAddr]
//                  -	等同 PageTable 直接將 Virtual Memory 映射為 Physical Memory
//                  -	不採用 Paging 的概念，直接將 整個 Physical Memory 視為 Program 專屬的 Virtual Memory
//                      只是把 各個Section 按照 虛擬位址 連續存在實體記憶體中
//             -	造成的問題
//                  -   所有 Thread 的 Code seg、Data Seg、Stack 皆共用同一段 實體記憶體空間
//                      因為 所有 Thread 都是從 虛擬位址=0 開始執行程式碼
//             	        若虛擬位址0=實體位址0，那後面載入的 Program Code 都會覆蓋掉 前面載入的 Program Code
//             -	修改方式
//             		(1)	透過 維護 AV-List = Free Frame List，將 不同Free Frame 分配給 不同 Thread
//             			-	使得 Threads 彼此存放的實體空間不同，可獨立運作、存取期內的資料
//             		(2)	透過 設定 Page Table Entry 的 Valid Bit、ReadOnly Bit 等
//             			-	防止 Thread 自己讀寫到 Code Seg 區域的 Page

    int numFrame = 0;
    int p=0;    // 23-0131[j]: Page Number
    int f=0;    // 23-0131[j]: Frame Number

    if (noffH.code.size > 0) {
        DEBUG(dbgAddr, "Initializing code segment.");
	    DEBUG(dbgAddr, noffH.code.virtualAddr << ", " << noffH.code.size);

        // 23-0131[j]: 這裡是實作 Addr 轉換的地方之一

        numFrame = divRoundUp(noffH.code.size, PageSize);
        // cout << "numFrame for Code=" << numFrame <<endl;
        for(int i=0;i<numFrame;i++,p++){
            f = kernel->PopFreeFrame();
            ASSERT(f>=0);   // 23-0131[j]: Frame Number >=0
            if(i >= numFrame-1){
                executable->ReadAt(
		            &(kernel->machine->mainMemory[f*PageSize]), 
			        (noffH.code.size-i*PageSize), (noffH.code.inFileAddr+i*PageSize));
            }
            else{
                executable->ReadAt(
		        &(kernel->machine->mainMemory[f*PageSize]), 
			    PageSize, (noffH.code.inFileAddr+i*PageSize));
            }

            // 23-0131[j]: 每分配一個 Free Frame 給 Page，就 update Page Table
            pageTable[p].virtualPage = p;
            pageTable[p].physicalPage = f;
            pageTable[p].valid = TRUE;
            pageTable[p].use = FALSE;
            pageTable[p].dirty = FALSE;
            pageTable[p].readOnly = TRUE;   // 23-0131[j]: Code Seg 屬於ReadOnly的
        } 

        // executable->ReadAt(
		//     &(kernel->machine->mainMemory[noffH.code.virtualAddr]), 
		// 	noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG(dbgAddr, "Initializing data segment.");
	    DEBUG(dbgAddr, noffH.initData.virtualAddr << ", " << noffH.initData.size);

        // 23-0131[j]: 這裡是實作 Addr 轉換的地方之一

        numFrame = divRoundUp(noffH.initData.size, PageSize);
        // cout << "numFrame for Data=" << numFrame <<endl;

        for(int i=0;i<numFrame;i++,p++){
            f = kernel->PopFreeFrame();
            ASSERT(f>=0);   // 23-0131[j]: Frame Number >=0
            if(i >= numFrame-1){
                executable->ReadAt(
		            &(kernel->machine->mainMemory[f*PageSize]), 
			        (noffH.initData.size-i*PageSize), (noffH.initData.inFileAddr+i*PageSize));
            }
            else{
                executable->ReadAt(
		        &(kernel->machine->mainMemory[f*PageSize]), 
			    PageSize, (noffH.initData.inFileAddr+i*PageSize));
            }

            // 23-0131[j]: 每分配一個 Free Frame 給 Page，就 update Page Table
            pageTable[p].virtualPage = p;
            pageTable[p].physicalPage = f;
            pageTable[p].valid = TRUE;
            pageTable[p].use = FALSE;
            pageTable[p].dirty = FALSE;
            pageTable[p].readOnly = FALSE;  // 23-0131[j]: Data Seg 不屬於ReadOnly的
        } 

        // executable->ReadAt(
		//     &(kernel->machine->mainMemory[noffH.initData.virtualAddr]),
		// 	noffH.initData.size, noffH.initData.inFileAddr);
    }

        // 23-0131[j]: 除了 Code Seg & Data Seg 剩下應該是 Heap & Stack 的空間，目前暫且全部配置

        for(p;p<numPages;p++){
            f = kernel->PopFreeFrame();
            ASSERT(f>=0);   // 23-0131[j]: Frame Number >=0
            pageTable[p].virtualPage = p;
            pageTable[p].physicalPage = f;
            pageTable[p].valid = TRUE;
            pageTable[p].use = FALSE;
            pageTable[p].dirty = FALSE;
            pageTable[p].readOnly = FALSE;  // 23-0131[j]: Stack Seg 不屬於ReadOnly的
        }

        // cout << "Name=" << kernel->currentThread->getName() << endl;
        // for(int i=0;i<numPages;i++){
        //     cout << "p=" << pageTable[i].virtualPage << ", f=" << pageTable[i].physicalPage;
        //     cout << ", v=" << pageTable[i].valid << ", r=" << pageTable[i].readOnly << endl;
        // }

    
#ifdef RDATA
    if (noffH.readonlyData.size > 0) {
        DEBUG(dbgAddr, "Initializing read only data segment.");
	DEBUG(dbgAddr, noffH.readonlyData.virtualAddr << ", " << noffH.readonlyData.size);
        executable->ReadAt(
		&(kernel->machine->mainMemory[noffH.readonlyData.virtualAddr]),
			noffH.readonlyData.size, noffH.readonlyData.inFileAddr);
    }
#endif

    delete executable;			// close file
    return TRUE;			    // success
}

//----------------------------------------------------------------------
// AddrSpace::Execute
// 	Run a user program using the current thread
//
//      The program is assumed to have already been loaded into
//      the address space
//
//----------------------------------------------------------------------
// 23-0127[j]: Execute(char* fileName) 
//             - 背景：在 ForkExecute(..) 被呼叫
//                    & 假設 fileName 代表的 Program 已載入 Memory
//             - 主要功能：配置 Page Table & 將其位址 載入machine，並請 machine 開始執行指令
//             (1) kernel->currentThread->space = this; // 指派 currentTread 的 TCB(PCB) = this
//                 (AddrSpace物件 = Page Table = this 先前 已被 Kernel::Exec(char* name) 創建)
//             (2) this->InitRegisters();               // 重置 Register(s) = 設定 PC、Reg(s)
//             (3) this->RestoreState();                // 載入 Thread Page Table的位址 到 machine
//             (4) kernel->machine->Run();              // CPU 開始執行 User Program 的指令

void 
AddrSpace::Execute(char* fileName) 
{

    kernel->currentThread->space = this;

    this->InitRegisters();		// set the initial register values
    this->RestoreState();		// load page table register

    kernel->machine->Run();		// jump to the user progam

    ASSERTNOTREACHED();			// machine->Run never returns;
                                // the address space exits
                                // by doing the syscall "exit"
}


//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------
// 23-0130[j]: 重置 模擬 Register(s) = 設定 PC、Reg(s)
// 23-0131[j]: 這裏很重要！！
//             指示了 Thread 在 Virtual Space 的空間規劃
//             (1)  程式碼的位址 從 0 開始
//             (2)  Stack 底部 從 numPages * PageSize - 16 開始
//                  多-16 只是為了防止 Overflow 
void
AddrSpace::InitRegisters()
{
    Machine *machine = kernel->machine;
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start", which
    //  is assumed to be virtual address zero
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    // Since instructions occupy four bytes each, the next instruction
    // after start will be at virtual address four.
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG(dbgAddr, "Initializing stack pointer: " << numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, don't need to save anything!
//----------------------------------------------------------------------
// 23-0127[j]: 在 context switch時，儲存 Thread 的狀態、Page Table的位址

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------
// 23-0127[j]: 在Context switch時，回復 Thread 的 Page Table的位址/資訊
//             (我的感覺 是告訴 模擬MMU Thread 的 Page Table的位址/資訊)

void AddrSpace::RestoreState() 
{
    kernel->machine->pageTable = pageTable;
    kernel->machine->pageTableSize = numPages;
}

//----------------------------------------------------------------------
// AddrSpace::Translate
//  Translate the virtual address in _vaddr_ to a physical address
//  and store the physical address in _paddr_.
//  The flag _isReadWrite_ is false (0) for read-only access; true (1)
//  for read-write access.
//  Return any exceptions caused by the address translation.
//---------------------------------------------------------------------- 
// 23-0127[j]: 實作 1種翻譯方案的地方(Page Table)
// 23-0127[j]: 檢查是否異常 & 翻譯位址 存入 *paddr

ExceptionType
AddrSpace::Translate(unsigned int vaddr, unsigned int *paddr, int isReadWrite)
{
    TranslationEntry *pte;
    int               pfn;
    unsigned int      vpn    = vaddr / PageSize;    // 23-0127[j]: Page Num(p)
    unsigned int      offset = vaddr % PageSize;    // 23-0127[j]: Offset(d)

    if(vpn >= numPages) {       // 23-0127[j]: 若 p > Page總數 = 虛擬位址錯誤
        return AddressErrorException;
    }

    pte = &pageTable[vpn];      // 23-0127[j]: pte 指向 vaddr 所在的 Page Entry

    if(isReadWrite && pte->readOnly) {
        return ReadOnlyException;
    }

    pfn = pte->physicalPage;    // 23-0127[j]: ptn 是 p 對應的 Frame Num(f)

    // if the pageFrame is too big, there is something really wrong!
    // An invalid translation was loaded into the page table or TLB.
    // 23-0127[j]: 若 f > frame總數 = 實體頁框錯誤
    if (pfn >= NumPhysPages) {
        DEBUG(dbgAddr, "Illegal physical page " << pfn);
        return BusErrorException;
    }

    // 23-0127[j]: 設定 use(Ref. bit) & dirty(Dirty bit)
    pte->use = TRUE;            // set the use, dirty bits

    if(isReadWrite) pte->dirty = TRUE;

    // 23-0127[j]: 翻譯位址，實體位址(*paddr) = f(pfn) * PageSize + offset
    *paddr = pfn*PageSize + offset;

    ASSERT((*paddr < MemorySize));

    //cerr << " -- AddrSpace::Translate(): vaddr: " << vaddr <<
    //  ", paddr: " << *paddr << "\n";

    return NoException;
}




