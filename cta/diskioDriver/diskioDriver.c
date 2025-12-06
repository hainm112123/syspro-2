#include "networkCallout.h"
#include <fltkernel.h>

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = PTDBG_TRACE_ROUTINES | PTDBG_TRACE_OPERATION_STATUS;

#define PT_DBG_PRINT( _dbgLevel, _string ) (FlagOn(gTraceFlags,(_dbgLevel)) ? DbgPrint _string : ((int)0))

typedef struct _PROCESS_MESSAGE {
	ULONG ProcessId;
	BOOLEAN IsCreate;
} PROCESS_MESSAGE, *PPROCESS_MESSAGE;

typedef struct _PROCESS_REPLY {
  BOOLEAN IsTracked;
} PROCESS_REPLY, *PPROCESS_REPLY;

const enum TRACK_REQUEST_TYPE {
  TYPE_TRACK_PROCESS = 0,
  TYPE_UNTRACK_PROCESS,
  TYPE_REQUEST_STAT
};

typedef struct _TRACK_REQUEST {
  enum TRACK_REQUEST_TYPE type;
  ULONG ProcessId;
} TRACK_REQUEST, *PTRACK_REQUEST;

typedef struct _PROCESS_USAGE_ENTRY {
  ULONG ProcessId;
  ULONG64 IoBytes;
  ULONG64 NetBytes;
} PROCESS_USAGE_ENTRY, *PPROCESS_USAGE_ENTRY;

typedef struct _PROCESS_USAGE_LIST {
  ULONG Count;
  PROCESS_USAGE_ENTRY Entries[1];
} PROCESS_USAGE_LIST, *PPROCESS_USAGE_LIST;

LIST_ENTRY g_TrackedList, g_NetTrackedList;
KSPIN_LOCK g_TrackedLock, g_NetTrackedLock;
ULONG g_TrackedCount;
EX_RUNDOWN_REF g_RundownRef;
PDEVICE_OBJECT g_DeviceObject = NULL;

DRIVER_INITIALIZE DriverEntry;
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath);
NTSTATUS DiskIoUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags);
NTSTATUS DiskIoQueryTeardown(_In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags);

PFLT_FILTER FilterHandle;
PFLT_PORT ServerPort = NULL, ClientPort = NULL, UsageServerPort, UsageClientPort;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DiskIoUnload)
#pragma alloc_text(PAGE, DiskIoQueryTeardown)
#endif

FLT_POSTOP_CALLBACK_STATUS PostIoCallback(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags);

const FLT_OPERATION_REGISTRATION Callbacks[] = {
	{IRP_MJ_READ, 0, NULL, PostIoCallback},
	{IRP_MJ_WRITE, 0, NULL, PostIoCallback},
	{IRP_MJ_OPERATION_END}
};

const FLT_REGISTRATION FilterRegistration = {
	sizeof(FLT_REGISTRATION),
	FLT_REGISTRATION_VERSION,
	FLTFL_REGISTRATION_SUPPORT_NPFS_MSFS, NULL,
	Callbacks, DiskIoUnload,
	NULL,
	DiskIoQueryTeardown,
	NULL, NULL, NULL, NULL, NULL
};

VOID ProcessCreateCallbackEx(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo) {
	UNREFERENCED_PARAMETER(Process);
	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Process %ul dectected\n", HandleToUlong(ProcessId)));
	if (ClientPort != NULL) {
		PROCESS_MESSAGE msg;
		msg.ProcessId = HandleToUlong(ProcessId);
		if (CreateInfo == NULL) {
			msg.IsCreate = FALSE;
		} else {
			msg.IsCreate = TRUE;
		}
		NTSTATUS status;
		ULONG replyLength;
		status = FltSendMessage(FilterHandle, &ClientPort, &msg, sizeof(PROCESS_MESSAGE), NULL, &replyLength, 0);
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Process Message sent\n"));
	}
}

VOID TrackProcess(ULONG pid) {
  KIRQL oldIrql;
  KeAcquireSpinLock(&g_TrackedLock, &oldIrql);

  PPROCESS_STAT entry = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(PROCESS_STAT), 'uLsP');
  entry->ProcessId = pid;
  entry->IoBytes = 0;
  InsertTailList(&g_TrackedList, &entry->ListEntry);
  g_TrackedCount ++;
  KeReleaseSpinLock(&g_TrackedLock, oldIrql);
}

VOID UntrackedProcess(ULONG pid) {
  KIRQL oldIrql;
  KeAcquireSpinLock(&g_TrackedLock, &oldIrql);

  for (PLIST_ENTRY entry = g_TrackedList.Flink; entry != &g_TrackedList; entry = entry->Flink) {
    PPROCESS_STAT p = CONTAINING_RECORD(entry, PROCESS_STAT, ListEntry);
    if (p->ProcessId == pid) {
      RemoveEntryList(entry);
      ExFreePoolWithTag(p, 'pTsI');
      g_TrackedCount --;
      break;
    }
  }

  KeReleaseSpinLock(&g_TrackedLock, oldIrql);
}

VOID UpdateStat(ULONG pid, ULONG64 bytes) {
  KIRQL oldIrql = KeGetCurrentIrql();
	BOOLEAN atDpc = (KeGetCurrentIrql() >= DISPATCH_LEVEL);
	if (atDpc) {
		UNREFERENCED_PARAMETER(oldIrql);
		KeAcquireSpinLockAtDpcLevel(&g_TrackedLock);
	} else {
		KeAcquireSpinLock(&g_TrackedLock, &oldIrql);
	}

  for (PLIST_ENTRY entry = g_TrackedList.Flink; entry != &g_TrackedList; entry = entry->Flink) {
    PPROCESS_STAT p = CONTAINING_RECORD(entry, PROCESS_STAT, ListEntry);
    if (p->ProcessId == pid) {
      p->IoBytes += bytes;
      break;
    }
  }

	if (atDpc) {
		KeReleaseSpinLockFromDpcLevel(&g_TrackedLock);
	} else {
		KeReleaseSpinLock(&g_TrackedLock, oldIrql);
	}
}

VOID UpdateNetworkStat(ULONG pid, ULONG64 bytes) {
  KIRQL oldIrql = KeGetCurrentIrql();
	BOOLEAN atDpc = (KeGetCurrentIrql() >= DISPATCH_LEVEL);
	if (atDpc) {
		UNREFERENCED_PARAMETER(oldIrql);
		KeAcquireSpinLockAtDpcLevel(&g_TrackedLock);
	} else {
		KeAcquireSpinLock(&g_TrackedLock, &oldIrql);
	}

  for (PLIST_ENTRY entry = g_TrackedList.Flink; entry != &g_TrackedList; entry = entry->Flink) {
    PPROCESS_STAT p = CONTAINING_RECORD(entry, PROCESS_STAT, ListEntry);
    if (p->ProcessId == pid) {
      p->NetBytes += bytes;
      break;
    }
  }

	if (atDpc) {
		KeReleaseSpinLockFromDpcLevel(&g_TrackedLock);
	} else {
		KeReleaseSpinLock(&g_TrackedLock, oldIrql);
	}
}

VOID SendUsage(PVOID Context) {
  PWORK_QUEUE_ITEM WorkItem = (PWORK_QUEUE_ITEM)Context;

  ULONG bufSize = sizeof(PROCESS_USAGE_LIST) + (g_TrackedCount - 1) * sizeof(PROCESS_USAGE_ENTRY);
  PPROCESS_USAGE_LIST usage = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufSize, 'uLsP');
  if (!usage) {
    ExReleaseRundownProtection(&g_RundownRef);
    return;
  }
  usage->Count = g_TrackedCount;
  ULONG i = 0;

  KIRQL oldIrql;
  KeAcquireSpinLock(&g_TrackedLock, &oldIrql);
  for (PLIST_ENTRY entry = g_TrackedList.Flink; entry != &g_TrackedList; entry = entry->Flink) {
    PPROCESS_STAT p = CONTAINING_RECORD(entry, PROCESS_STAT, ListEntry);
    usage->Entries[i].ProcessId = p->ProcessId;
    usage->Entries[i].IoBytes = p->IoBytes;
    usage->Entries[i].NetBytes = p->NetBytes;
    p->IoBytes = 0;
    p->NetBytes = 0;
    i ++;
  }
  KeReleaseSpinLock(&g_TrackedLock, oldIrql);

  ULONG replyLength = 0;
  LARGE_INTEGER timeout;
  timeout.QuadPart = -(500LL * 10000);
  NTSTATUS status = FltSendMessage(FilterHandle, &UsageClientPort, usage, bufSize, NULL, &replyLength, &timeout);
  if (NT_SUCCESS(status)) {
    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Send usage succeeded\n"));
  } else {
    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Send usage failed\n"));
  }
  ExReleaseRundownProtection(&g_RundownRef);
  ExFreePoolWithTag(WorkItem, 'uQoW');
}

NTSTATUS ProcessPortConnect(PFLT_PORT _ClientPort, PVOID ServerPortCookie, PVOID ConnectionContext, ULONG SizeOfContext, PVOID* ConnectionCookie) {
	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionCookie);
	ClientPort = _ClientPort;
  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Communication port connected\n"));
	return STATUS_SUCCESS;
}

VOID ProcessPortDisconnect(PVOID ConnectionCookie) {
	UNREFERENCED_PARAMETER(ConnectionCookie);
	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Communication port disconnected\n"));
	ClientPort = NULL;
}

NTSTATUS ProcessPortMessage(PVOID ConnectionCookie, PVOID InputBuffer, ULONG InputBufferSize, PVOID OutputBuffer, ULONG OutputBufferSize, PULONG ReturnOutputBufferLength) {
	UNREFERENCED_PARAMETER(ConnectionCookie);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferSize);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(ReturnOutputBufferLength);
	return STATUS_SUCCESS;
}

NTSTATUS UsagePortConnect(PFLT_PORT _ClientPort, PVOID ServerPortCookie, PVOID ConnectionContext, ULONG SizeOfContext, PVOID* ConnectionCookie) {
	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionCookie);
  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Usage Communication port connected\n"));
	UsageClientPort = _ClientPort;
  InitializeListHead(&g_TrackedList);
  KeInitializeSpinLock(&g_TrackedLock);
  g_TrackedCount = 0;
	return STATUS_SUCCESS;
}

VOID UsagePortDisconnect(PVOID ConnectionCookie) {
	UNREFERENCED_PARAMETER(ConnectionCookie);
	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Usage Communication port disconnected\n"));
	UsageClientPort = NULL;
  while (!IsListEmpty(&g_TrackedList)) {
    PLIST_ENTRY entry = RemoveHeadList(&g_TrackedList);
    PPROCESS_STAT p = CONTAINING_RECORD(entry, PROCESS_STAT, ListEntry);
    ExFreePoolWithTag(p, 'pTsI');
  }
  g_TrackedCount = 0;
}

NTSTATUS UsagePortMessage(PVOID ConnectionCookie, PVOID InputBuffer, ULONG InputBufferSize, PVOID OutputBuffer, ULONG OutputBufferSize, PULONG ReturnOutputBufferLength) {
	UNREFERENCED_PARAMETER(ConnectionCookie);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(ReturnOutputBufferLength);

  if (InputBuffer == NULL || InputBufferSize < sizeof(TRACK_REQUEST)) return STATUS_INVALID_PARAMETER;
  PTRACK_REQUEST request = (PTRACK_REQUEST)(InputBuffer);
  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Received track requrest with type = %u, process = %u\n", request->type, request->ProcessId));
  if (request->type == TYPE_TRACK_PROCESS) {
    TrackProcess(request->ProcessId);
  } else if (request->type == TYPE_UNTRACK_PROCESS) {
    UntrackedProcess(request->ProcessId);
  } else {
    PWORK_QUEUE_ITEM WorkItem = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(WORK_QUEUE_ITEM), 'uQoW');
    if (WorkItem && ExAcquireRundownProtection(&g_RundownRef)) {
      ExInitializeWorkItem(WorkItem, SendUsage, WorkItem);
      ExQueueWorkItem(WorkItem, DelayedWorkQueue);
    } else {
      if (WorkItem) ExFreePoolWithTag(WorkItem, 'uQoW');
    }
  }

	return STATUS_SUCCESS;
}

VOID ProcessCreateCallback(HANDLE ParentId, HANDLE ProcessId, BOOLEAN Create) {
	UNREFERENCED_PARAMETER(ParentId);
	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Process %u dectected\n", HandleToUlong(ProcessId)));
  if (!ExAcquireRundownProtection(&g_RundownRef)) return;
	if (ClientPort != NULL) {
		PROCESS_MESSAGE msg={0};
    RtlZeroMemory(&msg, sizeof(msg));
		msg.ProcessId = HandleToULong(ProcessId);
		msg.IsCreate = Create;
		NTSTATUS status;
		ULONG replyLength;
    LARGE_INTEGER timeout;
    timeout.QuadPart = -(500LL * 10000);
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Process Message sent\n"));
    status = FltSendMessage(FilterHandle, &ClientPort, &msg, sizeof(msg), NULL, &replyLength, &timeout);
  }
  ExReleaseRundownProtection(&g_RundownRef);
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status;
	OBJECT_ATTRIBUTES oa, usageOa;
	UNICODE_STRING PortName = RTL_CONSTANT_STRING(L"\\DiskIoFilterPort");
	UNICODE_STRING UsagePortName = RTL_CONSTANT_STRING(L"\\UsageDiskIoFilterPort");
  UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\ProcessMonitoringFilter");
	PSECURITY_DESCRIPTOR sd;
	PSECURITY_DESCRIPTOR usageSd;

  InitializeListHead(&g_TrackedList);
  KeInitializeSpinLock(&g_TrackedLock);
  ExInitializeRundownProtection(&g_RundownRef);

	status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
	status = FltBuildDefaultSecurityDescriptor(&usageSd, FLT_PORT_ALL_ACCESS);
	if (!NT_SUCCESS(status)) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Build sd failed\n"));
		return status;
	}

	// status = PsSetCreateProcessNotifyRoutineEx(ProcessCreateCallback, FALSE);
	status = PsSetCreateProcessNotifyRoutine(ProcessCreateCallback, FALSE);
	if (!NT_SUCCESS(status)) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Register process callback failed\n"));
		return status;
	}

	InitializeObjectAttributes(&oa, &PortName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, sd);
	InitializeObjectAttributes(&usageOa, &UsagePortName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, usageSd);
	status = FltRegisterFilter(DriverObject, &FilterRegistration, &FilterHandle);
	if (!NT_SUCCESS(status)) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Registration failed\n"));
		return status;
	}

	status = FltCreateCommunicationPort(FilterHandle, &ServerPort, &oa, NULL, ProcessPortConnect, ProcessPortDisconnect, ProcessPortMessage, 1);
	status = FltCreateCommunicationPort(FilterHandle, &UsageServerPort, &usageOa, NULL, UsagePortConnect, UsagePortDisconnect, UsagePortMessage, 1);
  FltFreeSecurityDescriptor(sd);
  FltFreeSecurityDescriptor(usageSd);
	if (!NT_SUCCESS(status)) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Failed to create Communication port\n"));
    return status;
	}

  status = FltStartFiltering(FilterHandle);
  if (!NT_SUCCESS(status)) {
    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Failed to start filtering\n"));
    FltUnregisterFilter(FilterHandle);
    return status;
  }
  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Start filtering\n"));

  status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_DeviceObject);
  if (!NT_SUCCESS(status)) {
    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Create device failed\n"));
    return status;
  }

  if (!WfpKernelRegister(g_DeviceObject)) {
    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: WFP Kernel registration failed.\n"));
  }
  if (!WfpUserMain()) {
    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: WFP User registration failed.\n"));
  }

  return status;
}

NTSTATUS DiskIoUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags) {
	UNREFERENCED_PARAMETER(Flags);
	PAGED_CODE();

  PsSetCreateProcessNotifyRoutine(ProcessCreateCallback, TRUE);
  ExWaitForRundownProtectionRelease(&g_RundownRef);

  if (!WfpUserExit()) {
    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: WFP User exit failed.\n"));
  }
  WfpKernelUnregister();
	if (ClientPort) {
		FltCloseClientPort(FilterHandle, &ClientPort);
		ClientPort = NULL;
	}
  if (UsageClientPort) {
    FltCloseClientPort(FilterHandle, &UsageClientPort);
    UsageClientPort = NULL;
  }
  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("Closed client port.\n"));
	FltCloseCommunicationPort(ServerPort);
  FltCloseCommunicationPort(UsageServerPort);
  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("Closed server port.\n"));

	FltUnregisterFilter(FilterHandle);
  if (g_DeviceObject) IoDeleteDevice(g_DeviceObject);
  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("Unregistered filter.\n"));

	return STATUS_SUCCESS;
}

NTSTATUS DiskIoQueryTeardown(_In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags) {
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	PAGED_CODE();
	return STATUS_SUCCESS;
}

FLT_POSTOP_CALLBACK_STATUS PostIoCallback(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags) {
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(Data);
	ULONG pid = FltGetRequestorProcessId(Data);
	ULONG64 bytes = Data->IoStatus.Information;
  UpdateStat(pid, bytes);
	return FLT_POSTOP_FINISHED_PROCESSING;
}
