#pragma once
#ifndef COMMON_H
#define COMMON_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <evntprov.h>
#include <tdh.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"tdh.lib")

#define MINI_EDR_SESSION_NAME L"MiniEDR_KernelSession"
//#define MINI_EDR_SESSION_NAME L"NT Kernel Logger"

#define MAX_PROCESS_COUNT 4096
#define MAX_IMAGE_PATH    512
#define MAX_MODULE_COUNT  8192
#define MAX_MODULE_PATH   512

VOID PrintGUID(const GUID* Guid);

// Provider GUID：只在 Common.cpp 中定义
extern const GUID GUID_KERNEL_PROCESS;
extern const GUID GUID_KERNEL_IMAGE;

typedef enum _PROCESS_EVENT_TYPE
{
    PROCESS_CREATE,
    PROCESS_EXIT,
    IMAGE_LOAD
} PROCESS_EVENT_TYPE;

typedef struct _EDR_EVENT
{
    PROCESS_EVENT_TYPE Type;
    ULONG  ProcessId;
    ULONG  ParentProcessId;
    WCHAR  Image[MAX_IMAGE_PATH];
    WCHAR  Module[MAX_IMAGE_PATH];
} EDR_EVENT;

typedef struct _PROCESS_CONTEXT
{
    BOOLEAN Active;
    ULONG   ProcessId;
    ULONG   ParentProcessId;
    WCHAR   Image[MAX_IMAGE_PATH];
    ULONG64 CreateTime;
} PROCESS_CONTEXT, * PPROCESS_CONTEXT;

typedef struct _MODULE_CONTEXT
{
    BOOLEAN Active;
    ULONG   ProcessId;
    WCHAR   ModuleName[MAX_MODULE_PATH];
    PVOID   BaseAddress;
    ULONG   Size;
    ULONG64 LoadTime;
} MODULE_CONTEXT;

#endif // COMMON_H