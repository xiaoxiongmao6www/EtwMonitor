#pragma once
#include "Common.h"

VOID ParseEtwEvent(PEVENT_RECORD Record);

BOOL GetUInt32Property(PEVENT_RECORD Record, LPCWSTR Name, ULONG* Value);
BOOL GetUInt64Property(PEVENT_RECORD Record, LPCWSTR Name, ULONG64* Value);
BOOL GetStringProperty(PEVENT_RECORD Record, LPCWSTR Name, PWCHAR Buffer, ULONG BufferSize);

VOID DumpEventPayload(PEVENT_RECORD Record);