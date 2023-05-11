// callback.h 
//	Data structure to allow an object to register a "callback".
//	On an asynchronous operation, the call to start the operation
//	returns immediately.  When the operation completes, the called 
//	object must somehow notify the caller of the completion.
//	In the general case, the called object doesn't know the type
//	of the caller.
//
//	We implement this using virtual functions in C++.  An object
//	that needs to register a callback is set up as a derived class of
//	the abstract base class "CallbackObj".  When we pass a
//	pointer to the object to a lower level module, that module
//	calls back via "obj->CallBack()", without knowing the
//	type of the object being called back.
//
// 	Note that this isn't a general-purpose mechanism, 
//	because a class can only register a single callback.
//
//  DO NOT CHANGE -- part of the machine emulation

#ifndef CALLBACK_H
#define CALLBACK_H 

#include "copyright.h"

// Abstract base class for objects that register callbacks

// 23-0102[j]: NachOS中，實現硬體中斷，是由 Callback 方式來實現
// 23-0102[j]: (1) 硬體中斷：在硬體完成工作時，引發中斷，呼叫 根據不同硬體 定義的Callback() 來執行相應動作
// 23-0102[j]: (2) Callback 回呼叫
// 23-0102[j]:    - Def: 將「函數」視為「傳遞參數」，來讓其他函式 控制、執行 傳入的函式
// 23-0102[j]:    - 目的: 夠過同一隻手機，請不同的員工做事

class CallBackObj {
   public:
      // 23-0102[j]: Pure virtual function 純虛函數，只能用來繼承
      // 23-0102[j]: 格式：virtual type Function(type) = 0
      virtual void CallBack() = 0; 
   protected:
      CallBackObj() {};	// to prevent anyone from creating
				// an instance of this class.  Only
				// allow creation of instances of
				// classes derived from this class.
      virtual ~CallBackObj() {};
};

#endif
