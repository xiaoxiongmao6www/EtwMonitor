#pragma once
#include "Common.h"

BOOL InitProcessContext();
PPROCESS_CONTEXT FindProcessContext(ULONG pid);
BOOL AddProcessContext(ULONG pid, ULONG parentPid, PCWSTR image);
BOOL RemoveProcessContext(ULONG pid);
VOID DumpProcessTable();