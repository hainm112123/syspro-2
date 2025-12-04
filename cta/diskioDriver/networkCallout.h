#pragma once
#ifndef __NETWORK_CALLOUT_H__
#define __NETWORK_CALLOUT_H__

#include <ntdef.h>

typedef struct _PROCESS_STAT {
  ULONG ProcessId;
  ULONG64 IoBytes;
  ULONG64 NetBytes;
  LIST_ENTRY ListEntry;
} PROCESS_STAT, *PPROCESS_STAT;

BOOLEAN WfpKernelRegister(PVOID devObj);
VOID WfpKernelUnregister();
BOOLEAN WfpUserMain();
BOOLEAN WfpUserExit();
VOID UpdateTrackedListLock(PVOID _TrackedList, PVOID _TrackedLock);

#endif

