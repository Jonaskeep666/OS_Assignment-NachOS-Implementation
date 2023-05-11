#include "syscall.h"

int main()
{

  char test[] = "abcdefghijklmnopqrstuvwxyz";

	int success = Create("file1.test");
	OpenFileId fid;
	int i;
	if (success != 1) MSG("Failed on creating file");
	fid = Open("file1.test");
	
	if (fid < 0) MSG("Failed on opening file");

	for (i = 0; i < 26; ++i) {
		int count = Write(test + i, 1, fid);
		if (count != 1) MSG("Failed on writing file");
	}

	success = Close(fid);
	if (success != 1) MSG("Failed on closing file");
	MSG("Success on creating file1.test");
 
	Halt();
}

// [fileIO.c]
// for (i = 0; i < 26; ++i) {
// 	int count = Write(test + i, 1, fid);
// 	if (count != 1) MSG("Failed on writing file");
// }

// [exception.cc - void ExceptionHandler(ExceptionType which)]
// case SC_Write: 
// {
// 	val = kernel->machine->ReadRegister(4); // 傳入參數
// 	char *buffer = &(kernel->machine->mainMemory[val]);
// 	numChar = kernel->machine->ReadRegister(5);
// 	fileID = kernel->machine->ReadRegister(6);
// 	status = SysWrite(fileID,buffer,numChar);

// 	kernel->machine->WriteRegister(2, (int) status); //回傳值
// 	kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
// 	kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
// 	kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
// 	return;
// }

// [ksyscall.h]
// int SysWrite(OpenFileId fd,char *buffer, int nBytes){
// 	return kernel->fileSystem->WriteF(fd,buffer,nBytes);
// }

// [filesys.h]
// class FileSystem {
// 	public:
// 	...
// 	int WriteF(OpenFileId fd,char *buffer, int nBytes){
// 		int before = Tell(fd);
// 		WriteFile(fd,buffer,nBytes);
// 		int after = Tell(fd);
// 		if(before <0 || after<0) return -1;
// 		else return ((after - before));
//     }
// 	...
// };