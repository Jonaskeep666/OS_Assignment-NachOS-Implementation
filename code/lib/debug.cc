// debug.cc 
//	Debugging routines.  Allows users to control whether to 
//	print DEBUG statements, based on a command line argument.

#include "copyright.h"
#include "utility.h"
#include "debug.h" 
#include "string.h"

//----------------------------------------------------------------------
// Debug::Debug
//      Initialize so that only DEBUG messages with a flag in flagList 
//	will be printed.
//
//	If the flag is "+", we enable all DEBUG messages.
//
// 	"flagList" is a string of characters for whose DEBUG messages are 
//		to be enabled.
//----------------------------------------------------------------------

Debug::Debug(char *flagList)
{
    enableFlags = flagList;
}


//----------------------------------------------------------------------
// Debug::IsEnabled
//      Return TRUE if DEBUG messages with "flag" are to be printed.
//
// 22-1223[j]: char* strchr(const char* str, char c) 搜尋 str中 第一個 = c 的字元，並 return 位址
//----------------------------------------------------------------------
// 22-1223[j]: bool IsEnabled(char flag): 檢查 debug 的 "flag"代表之功能是否開啟，若開啟 則回傳 True       

bool
Debug::IsEnabled(char flag)
{
    if (enableFlags != NULL) {
	return ((strchr(enableFlags, flag) != 0) 
		|| (strchr(enableFlags, '+') != 0));
    } else {
    	return FALSE;
    }
}
