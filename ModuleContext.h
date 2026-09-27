#pragma once
#include "Common.h"

BOOL InitModuleContext();
BOOL AddModuleContext(ULONG pid, PCWSTR moduleName, PVOID base, ULONG size);
BOOL RemoveModuleByProcess(ULONG pid);
VOID DumpModuleTable();