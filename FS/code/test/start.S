/* Start.s 
 *	Assembly language assist for user programs running on top of Nachos.
 *
 *	Since we don't want to pull in the entire C library, we define
 *	what we need for a user program here, namely Start and the system
 *	calls.
 *
 */
//  22-1224[j]: Start.s 是 NachOS 的 System Call Interface

#define IN_ASM
#include "syscall.h"

        .text   
        .align  2

/* -------------------------------------------------------------
 * __start
 *	Initialize running a C program, by calling "main". 
 *
 * 	NOTE: This has to be first, so that it gets loaded at location 0.
 *	The Nachos kernel always starts a program by jumping to location 0.
 * -------------------------------------------------------------
 */

	.globl __start
	.ent	__start
__start:
	jal	main
	move	$4,$0		
	jal	Exit	 /* if we return from main, exit(0) */
	.end __start

/* -------------------------------------------------------------
 * System call stubs:
 *	Assembly language assist to make system calls to the Nachos kernel.
 *	There is one stub per system call, that places the code for the
 *	system call into register r2, and leaves the arguments to the
 *	system call alone (in other words, arg1 is in r4, arg2 is 
 *	in r5, arg3 is in r6, arg4 is in r7)
 * 	
 *  The return value is in r2. This follows the standard C calling
 * 	convention on the MIPS.
 * -------------------------------------------------------------
 */
 
// 22-1228[j]: NachOS 的 System Call Interface 在此(Start.s)
// 22-1228[j]: 功能：將 User program 提出 OS功能 對應的 System Call ID 存入 Reg[2]=r2
// 22-1228[j]: 		之後再由 OS/Kernel 的 Exception Handler 中的 System call handler (在userprog/exception.cc 中)
// 22-1228[j]: 		將 存於 r2 的 System Call ID 讀出，並執行 System Call Service Routine (在 xx.c/xx.cc 中)

// 22-1228[j]: 備註：Exception handler = Trap handler 中，包含 System call handler 的功能

// 22-1228[j]: 紀錄一下 呼叫 System call = Halt 的機制
// 22-1228[j]: 這裡宣告 全域函數 的 Label=Halt 即 .globl Halt & .ent Halt
// 22-1228[j]: 當 main.c 呼叫到 Halt() 時，由 main.s(組語) 可發現 程式會Jump 到 Label=Halt 處
// 22-1228[j]: 此時就會進入 Start.s 的「Halt:」處
// 22-1228[j]: (1) addiu：將在 system.h 定義的 System Call ID = SC_Halt = 0 寫入 Reg[2]=r2
// 22-1228[j]: (2) syscall：執行 RaiseException(..)->切到 Kernel Mode->ExceptionHandler(..) 並 跳去執行 真正的System Call Service Routine
// 22-1228[j]: (3) 完成 Service Routine後，Jump 到 Reg[31]=Program Counter 指向的記憶體位址 -> 結束

	.globl Halt
	.ent	Halt
Halt:
	addiu $2,$0,SC_Halt
	syscall
	j	$31
	.end Halt

	.globl  PrintInt
	.ent     PrintInt

PrintInt:
	addiu $2,$0,SC_PrintInt
	syscall
	j       $31
	.end  PrintInt

	.globl MSG
	.ent   MSG
MSG:
	addiu $2,$0,SC_MSG
	syscall
	j	$31
	.end MSG

	.globl Add
	.ent	Add
Add:
	addiu $2,$0,SC_Add
	syscall
	j 	$31
	.end Add

	.globl Exit
	.ent	Exit
Exit:
	addiu $2,$0,SC_Exit
	syscall
	j	$31
	.end Exit

	.globl Exec
	.ent	Exec
Exec:
	addiu $2,$0,SC_Exec
	syscall
	j	$31
	.end Exec

	.globl ExecV
	.ent	ExecV
ExecV:
	addiu $2,$0,SC_ExecV
	syscall
	j	$31
	.end ExecV

	.globl Join
	.ent	Join
Join:
	addiu $2,$0,SC_Join
	syscall
	j	$31
	.end Join

	.globl Create
	.ent	Create
Create:
	addiu $2,$0,SC_Create
	syscall
	j	$31
	.end Create

	.globl Remove
	.ent	Remove
Remove:
	addiu $2,$0,SC_Remove
	syscall
	j	$31
	.end Remove

	.globl Seek
	.ent	Seek
Seek:
	addiu $2,$0,SC_Seek
	syscall
	j	$31
	.end Seek

        .globl ThreadFork
        .ent    ThreadFork
ThreadFork:
        addiu $2,$0,SC_ThreadFork
        syscall
        j       $31
        .end ThreadFork

        .globl ThreadYield
        .ent    ThreadYield
ThreadYield:
        addiu $2,$0,SC_ThreadYield
        syscall
        j       $31
        .end ThreadYield

	.globl ThreadExit
	.ent    ThreadExit
ThreadExit:
	addiu $2, $0, SC_ThreadExit
	syscall
	j 	$31
	.end ThreadExit

	.globl ThreadJoin
	.ent    ThreadJoin
ThreadJoin:
	addiu $2, $0, SC_ThreadJoin
	syscall
	j 	$31
	.end ThreadJoin
 
	.globl Open
	.ent	Open
Open:
	addiu $2,$0,SC_Open
	syscall
	j	$31
	.end Open
 
	.globl Write
	.ent	Write
Write:
	addiu $2,$0,SC_Write
	syscall
	j	$31
	.end Write

	.globl Read
	.ent	Read
Read:
	addiu $2,$0,SC_Read
	syscall
	j	$31
	.end Read
 
	.globl Close
	.ent	Close
Close:
	addiu $2,$0,SC_Close
	syscall
	j	$31
	.end Close

/* dummy function to keep gcc happy */
        .globl  __main
        .ent    __main
__main:
        j       $31
        .end    __main

