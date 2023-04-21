// main.h 
//	This file defines the Nachos global variables

#ifndef MAIN_H
#define MAIN_H

#include "copyright.h"
#include "debug.h"
#include "kernel.h"

// 23-0419[j]: extern 表示 Kernel 在其他地方被宣告
extern Kernel *kernel; 
extern Debug *debug;

#endif // MAIN_H

