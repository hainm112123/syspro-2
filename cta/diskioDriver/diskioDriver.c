#include <fltkernel.h>
#include <ntifs.h>

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = PTDBG_TRACE_ROUTINES | PTDBG_TRACE_OPERATION_STATUS;

#define PT_DBG_PRINT( _dbgLevel, _string ) (FlagOn(gTraceFlags,(_dbgLevel)) ? DbgPrint _string : ((int)0))

typedef struct _IO_MESSAGE {
	FILTER_MESSAGE_HEADER MessageHeader;
	ULONG_PTR ProcessId;
	ULONG64 bytes;
} IO_MESSAGE, *PIO_MESSAGE;

typedef struct _PROCESS_MESSAGE {
	ULONG ProcessId;
	BOOLEAN IsCreate;
} PROCESS_MESSAGE, *PPROCESS_MESSAGE;

DRIVER_INITIALIZE DriverEntry;
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath);
NTSTATUS DiskIoUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags);
NTSTATUS DiskIoQueryTeardown(_In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags);

PFLT_FILTER FilterHandle;
PFLT_PORT ServerPort = NULL, ClientPort = NULL;

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

NTSTATUS DiskIoConnect(PFLT_PORT _ClientPort, PVOID ServerPortCookie, PVOID ConnectionContext, ULONG SizeOfContext, PVOID* ConnectionCookie) {
	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionCookie);
	ClientPort = _ClientPort;
	return STATUS_SUCCESS;
}

VOID DiskIoDisconnect(PVOID ConnectionCookie) {
	UNREFERENCED_PARAMETER(ConnectionCookie);
	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Communication port connected\n"));
	ClientPort = NULL;
}

NTSTATUS DiskIoMessage(PVOID ConnectionCookie, PVOID InputBuffer, ULONG InputBufferSize, PVOID OutputBuffer, ULONG OutputBufferSize, PULONG ReturnOutputBufferLength) {
	UNREFERENCED_PARAMETER(ConnectionCookie);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferSize);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(ReturnOutputBufferLength);
	return STATUS_SUCCESS;
}

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

VOID ProcessCreateCallback(HANDLE ParentId, HANDLE ProcessId, BOOLEAN Create) {
	UNREFERENCED_PARAMETER(ParentId);
	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Process %ul dectected\n", HandleToUlong(ProcessId)));
	if (ClientPort != NULL) {
		PROCESS_MESSAGE msg={0};
    RtlZeroMemory(&msg, sizeof(msg));
		msg.ProcessId = HandleToULong(ProcessId);
		msg.IsCreate = Create;
		NTSTATUS status;
		ULONG replyLength;
		status = FltSendMessage(FilterHandle, &ClientPort, &msg, sizeof(msg), NULL, &replyLength, 0);
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Process Message sent\n"));
	}
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING PortName = RTL_CONSTANT_STRING(L"\\DiskIoFilterPort");
	PSECURITY_DESCRIPTOR sd;

	status  = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
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
	status = FltRegisterFilter(DriverObject, &FilterRegistration, &FilterHandle);
	if (!NT_SUCCESS(status)) {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Registration failed\n"));
		return status;
	}

	status = FltCreateCommunicationPort(FilterHandle, &ServerPort, &oa, NULL, DiskIoConnect, DiskIoDisconnect, DiskIoMessage, 1);
  FltFreeSecurityDescriptor(sd);
	if (NT_SUCCESS(status)) {
		status = FltStartFiltering(FilterHandle);
		if (!NT_SUCCESS(status)) {
			PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Failed to start filtering\n"));
			FltUnregisterFilter(FilterHandle);
		}
		else {
			PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Start filtering\n"));
		}
	}
	else {
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Failed to create Communication port\n"));
	}
	return status;
}

NTSTATUS DiskIoUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags) {
	UNREFERENCED_PARAMETER(Flags);
	PAGED_CODE();
	if (ClientPort != NULL) {
		FltCloseClientPort(FilterHandle, &ClientPort);
		ClientPort = NULL;
	}
	FltCloseCommunicationPort(ServerPort);
	FltUnregisterFilter(FilterHandle);
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
	// HANDLE pid = PsGetCurrentProcessId();
	// ULONG64 bytes = Data->IoStatus.Information;
	// PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskioDriver: PostIo called"));
	// if (ClientPort != NULL) {
	// 	IO_MESSAGE msg;
	// 	msg.ProcessId = (ULONG_PTR)pid;
	// 	msg.bytes = bytes;
	// 	NTSTATUS status;
	// 	ULONG replyLength;
	// 	status = FltSendMessage(FilterHandle, &ClientPort, &msg, sizeof(IO_MESSAGE), NULL, &replyLength, 0);
	// 	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("DiskIoDriver: Message sent\n"));
	// }
	return FLT_POSTOP_FINISHED_PROCESSING;
}
