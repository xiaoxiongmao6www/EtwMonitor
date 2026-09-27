#pragma once
#include "Common.h"
//Etw控制器
//开启事件会话
BOOL StartEtwSession();
//在当前的 ETW 跟踪会话中，启用一个或多个内核态事件提供程序
BOOL EnableKernelProviders();
//停止事件会话
VOID StopEtwSession();