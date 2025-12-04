#include "networkCallout.h"

#define NDIS61 1
#include <initguid.h>
#include <fwpsk.h>
#include <fwpmu.h>

// {0C88038E-08E3-4D87-9513-1B13CCFD99F4}
DEFINE_GUID(NETWORK_CALLOUT_GUID, 0xc88038e, 0x8e3, 0x4d87, 0x95, 0x13, 0x1b, 0x13, 0xcc, 0xfd, 0x99, 0xf4);
PDEVICE_OBJECT g_DeviceObject = NULL;
UINT32 g_CalloutId = 0;
HANDLE g_EngineHandle = NULL;
UINT64 g_FilterId = 0;

VOID NTAPI ClassifyFn(
  const FWPS_INCOMING_VALUES0* inFixedValues, const FWPS_INCOMING_METADATA_VALUES0* inMetaValues, 
  VOID* layerData, const FWPS_FILTER0* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT0* classifyOut
) {
  UNREFERENCED_PARAMETER(inFixedValues);
  UNREFERENCED_PARAMETER(layerData);
  // UNREFERENCED_PARAMETER(classifyContext);
  UNREFERENCED_PARAMETER(filter);
  UNREFERENCED_PARAMETER(flowContext);

  ULONG64 pid = inMetaValues->processId;
  UNREFERENCED_PARAMETER(pid);
  classifyOut->actionType = FWP_ACTION_PERMIT;
  classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
}

BOOLEAN RegisterNetworkCallout() {
  NTSTATUS status;
  FWPS_CALLOUT0 callout = {0};
  FWPM_CALLOUT0 mCallout = {0};
  FWPM_FILTER0 filter = {0};
  RtlZeroMemory(&callout, sizeof(callout));
  RtlZeroMemory(&mCallout, sizeof(mCallout));
  RtlZeroMemory(&filter, sizeof(filter));

  callout.calloutKey = NETWORK_CALLOUT_GUID;
  callout.classifyFn = ClassifyFn;
  callout.notifyFn = NULL;
  callout.flowDeleteFn = NULL;
  if (!callout.classifyFn) {
    DbgPrint("Classify function is null...\n");
  }

  status = FwpsCalloutRegister0(g_DeviceObject, &callout, &g_CalloutId);
  if (!NT_SUCCESS(status)) {
    DbgPrint("Callout registration failed. Status = 0x%X\n", status);
    // return FALSE;
  }

  status = FwpmEngineOpen0(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &g_EngineHandle);
  if (!NT_SUCCESS(status)) {
    DbgPrint("Engine open failed.\n");
    // return FALSE;
  }

  status = FwpmTransactionBegin(g_EngineHandle, 0);
  if (!NT_SUCCESS(status)) {
    DbgPrint("FwpmTransactionBegin failed: 0x%08X\n", status);
    FwpsCalloutUnregisterById0(g_CalloutId);
    FwpmEngineClose(g_EngineHandle);
    g_EngineHandle = NULL;
    return FALSE;
  }

  mCallout.calloutKey = NETWORK_CALLOUT_GUID;
  mCallout.displayData.name = L"Network monitoring callout";
  mCallout.applicableLayer = FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4;
  status = FwpmCalloutAdd0(g_EngineHandle, &mCallout, NULL, &g_CalloutId);
  if (!NT_SUCCESS(status)) {
    DbgPrint("Callout add failed. Status = 0x%X\n", status);
    // return FALSE;
  }

  filter.layerKey = FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4;
  filter.displayData.name = L"Network flow filter";
  filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
  filter.action.calloutKey = NETWORK_CALLOUT_GUID;
  filter.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
  filter.weight.type = FWP_EMPTY;

  status = FwpmFilterAdd0(g_EngineHandle, &filter, NULL, &g_FilterId);
  if (!NT_SUCCESS(status)) {
    DbgPrint("Filter add. Status = 0x%X\n", status);
    // return FALSE;
  }

  status = FwpmTransactionCommit(g_EngineHandle);

  return TRUE;
}

VOID UnregisterNetworkCallout() {
  if (g_FilterId && g_EngineHandle) {
    FwpmFilterDeleteById(g_EngineHandle, g_FilterId);
    g_FilterId = 0;
  }

  if (g_CalloutId) {
    FwpsCalloutUnregisterById(g_CalloutId);
    g_CalloutId = 0;
  }

  if (g_EngineHandle) {
    FwpmEngineClose(g_EngineHandle);
    g_EngineHandle = NULL;
  }
  DbgPrint("Unregistered network callout.\n");
}