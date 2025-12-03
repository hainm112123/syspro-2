#include "networkCallout.h"

#define NDIS61 1
#include <initguid.h>
#include <fwpsk.h>
#include <fwpmu.h>

// {15152CBA-1EB8-4CC7-B2F7-7E6DA55C3448}
static const GUID NETWORK_CALLOUT_GUID = { 0x15152cba, 0x1eb8, 0x4cc7, { 0xb2, 0xf7, 0x7e, 0x6d, 0xa5, 0x5c, 0x34, 0x48 } };
PDEVICE_OBJECT g_DeviceObject = NULL;
UINT32 g_CalloutId = 0;
HANDLE g_EngineHandle = NULL;
UINT64 g_FilterId = 0;

VOID NTAPI ClassifyFn(
  const FWPS_INCOMING_VALUES* inFixedValues, const FWPS_INCOMING_METADATA_VALUES* inMetaValues, 
  VOID* layerData, const VOID* classifyContext, const FWPS_FILTER* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT* classifyOut
);

VOID NTAPI ClassifyFn(
  const FWPS_INCOMING_VALUES* inFixedValues, const FWPS_INCOMING_METADATA_VALUES* inMetaValues, 
  VOID* layerData, const VOID* classifyContext, const FWPS_FILTER* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT* classifyOut
) {
  UNREFERENCED_PARAMETER(inFixedValues);
  UNREFERENCED_PARAMETER(layerData);
  UNREFERENCED_PARAMETER(classifyContext);
  UNREFERENCED_PARAMETER(filter);
  UNREFERENCED_PARAMETER(flowContext);

  ULONG64 pid = inMetaValues->processId;
  UNREFERENCED_PARAMETER(pid);
  classifyOut->actionType = FWP_ACTION_PERMIT;
  classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
}

BOOLEAN RegisterNetworkCallout() {
  NTSTATUS status;
  FWPS_CALLOUT callout = {0};
  FWPM_CALLOUT mCallout = {0};
  FWPM_FILTER filter = {0};

  status = FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &g_EngineHandle);
  if (!NT_SUCCESS(status)) {
    DbgPrint("Engine open failed.\n");
    return FALSE;
  }

  callout.calloutKey = NETWORK_CALLOUT_GUID;
  callout.classifyFn = ClassifyFn;
  callout.notifyFn = NULL;
  callout.flowDeleteFn = NULL;
  status = FwpsCalloutRegister(g_DeviceObject, &callout, &g_CalloutId);
  if (!NT_SUCCESS(status)) {
    DbgPrint("Callout registration failed.\n");
    return FALSE;
  }

  mCallout.calloutKey = NETWORK_CALLOUT_GUID;
  mCallout.displayData.name = L"Network monitoring callout";
  mCallout.applicableLayer = FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4;
  status = FwpmCalloutAdd(g_EngineHandle, &mCallout, NULL, &g_CalloutId);
  if (!NT_SUCCESS(status)) {
    DbgPrint("Callout add failed.\n");
    return FALSE;
  }

  filter.layerKey = FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4;
  filter.displayData.name = L"Network flow filter";
  filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
  filter.action.calloutKey = NETWORK_CALLOUT_GUID;
  filter.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
  filter.weight.type = FWP_EMPTY;

  status = FwpmFilterAdd(g_EngineHandle, &filter, NULL, &g_FilterId);
  if (!NT_SUCCESS(status)) {
    DbgPrint("Filter add failed.\n");
    return FALSE;
  }

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
}