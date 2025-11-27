#include <fltkernel.h>

typedef struct _IO_MESSAGE {
	FILTER_MESSAGE_HEADER MessageHeader;
	ULONG_PTR ProcessId;
	ULONG64 bytes;
} IO_MESSAGE, *PIO_MESSAGE;

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
	0, NULL,
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

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING PortName = RTL_CONSTANT_STRING(L"\\DiskIoFilterPort");
	InitializeObjectAttributes(&oa, &PortName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = FltRegisterFilter(DriverObject, &FilterRegistration, &FilterHandle);
	if (!NT_SUCCESS(status)) return status;

	status = FltCreateCommunicationPort(FilterHandle, &ServerPort, &oa, NULL, DiskIoConnect, DiskIoDisconnect, DiskIoMessage, 1);
	if (NT_SUCCESS(status)) {
		status = FltStartFiltering(FilterHandle);
		if (!NT_SUCCESS(status)) {
			FltUnregisterFilter(FilterHandle);
		}
	}
	return status;
}

NTSTATUS DiskIoUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags) {
	UNREFERENCED_PARAMETER(Flags);
	PAGED_CODE();
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
	HANDLE pid = PsGetCurrentProcessId();
	ULONG64 bytes = Data->IoStatus.Information;
	if (ClientPort != NULL) {
		IO_MESSAGE msg;
		msg.ProcessId = (ULONG_PTR)pid;
		msg.bytes = bytes;
		NTSTATUS status;
		ULONG replyLength;
		status = FltSendMessage(FilterHandle, &ClientPort, &msg, sizeof(IO_MESSAGE), NULL, &replyLength, 0);
	}
	return FLT_POSTOP_FINISHED_PROCESSING;
}
