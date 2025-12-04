#include "networkCallout.h"

#define NDIS61 1
#include <initguid.h>
#include <fwpsk.h>
#include <fwpmu.h>
#include <fwpmtypes.h>

// {0C88038E-08E3-4D87-9513-1B13CCFD99F4}
DEFINE_GUID(GUID_CALLOUT_LAYER_ESTABLISHED_V4, 0xc88038e, 0x8e3, 0x4d87, 0x95, 0x13, 0x1b, 0x13, 0xcc, 0xfd, 0x99, 0xf4);
// {076F1388-5F33-4645-B33B-EF01B0C81C2A}
DEFINE_GUID(GUID_CALLOUT_LAYER_ESTABLISHED_V6, 0x76f1388, 0x5f33, 0x4645, 0xb3, 0x3b, 0xef, 0x1, 0xb0, 0xc8, 0x1c, 0x2a);
// {C2D11D5F-1679-49B0-8CE1-4648C71F9291}
DEFINE_GUID(GUID_CALLOUT_LAYER_STREAM_V4, 0xc2d11d5f, 0x1679, 0x49b0, 0x8c, 0xe1, 0x46, 0x48, 0xc7, 0x1f, 0x92, 0x91);
// {7679C4DE-8D3A-4A31-AFCF-E60BCC34AC26}
DEFINE_GUID(GUID_CALLOUT_LAYER_STREAM_V6, 0x7679c4de, 0x8d3a, 0x4a31, 0xaf, 0xcf, 0xe6, 0xb, 0xcc, 0x34, 0xac, 0x26);
const int GUID_CALLOUT_COUNT = 4;

// {F2CE9337-2FF1-4A4D-B044-AE78B7207AB1}
DEFINE_GUID(GUID_WFP_NETWORK_PROVIDER, 0xf2ce9337, 0x2ff1, 0x4a4d, 0xb0, 0x44, 0xae, 0x78, 0xb7, 0x20, 0x7a, 0xb1);

// {208192BB-D1B7-49FE-AF8D-7442FB2B2945}
DEFINE_GUID(GUID_FILTER_LAYER_ESTABLISHED_V4, 0x208192bb, 0xd1b7, 0x49fe, 0xaf, 0x8d, 0x74, 0x42, 0xfb, 0x2b, 0x29, 0x45);
// {78213172-E71D-4506-A2D4-77F07727758F}
DEFINE_GUID(GUID_FILTER_LAYER_ESTABLISHED_V6, 0x78213172, 0xe71d, 0x4506, 0xa2, 0xd4, 0x77, 0xf0, 0x77, 0x27, 0x75, 0x8f);
// {D5CCF8D2-F402-4A0B-9167-3C1720A2E1C0}
DEFINE_GUID(GUID_FILTER_LAYER_STREAM_V4, 0xd5ccf8d2, 0xf402, 0x4a0b, 0x91, 0x67, 0x3c, 0x17, 0x20, 0xa2, 0xe1, 0xc0);
// {067B2394-DC6D-47DB-906C-9C02ED928A69}
DEFINE_GUID(GUID_FILTER_LAYER_STREAM_V6, 0x67b2394, 0xdc6d, 0x47db, 0x90, 0x6c, 0x9c, 0x2, 0xed, 0x92, 0x8a, 0x69);


typedef struct _NET_CALLOUT {
  const GUID *guid;
  const GUID *layer;
} NET_CALLOUT, *PNET_CALLOUT;

typedef struct _NET_FILTER {
  const GUID *guid;
  const GUID *layer;
  const GUID *callout;
} NET_FILTER, *PNET_FILTER;


VOID OnCalloutClassify(
  const FWPS_INCOMING_VALUES0* inFixedValues, const FWPS_INCOMING_METADATA_VALUES0* inMetaValues, 
  VOID* layerData, const FWPS_FILTER0* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT0* classifyOut
) {
  UNREFERENCED_PARAMETER(inFixedValues);
  UNREFERENCED_PARAMETER(layerData);
  UNREFERENCED_PARAMETER(filter);
  UNREFERENCED_PARAMETER(flowContext);
  UNREFERENCED_PARAMETER(classifyOut);
  // DbgPrint("Callout called\n");
  // if ((inMetaValues->currentMetadataValues & FWPS_METADATA_FIELD_PROCESS_ID) == 0) return;
  // DbgPrint("Callout for process id: %u", (ULONG)inMetaValues->processId);
  if (inFixedValues->layerId == FWPS_LAYER_ALE_FLOW_ESTABLISHED_V4) {

  }
  if (inFixedValues->layerId == FWPS_LAYER_ALE_FLOW_ESTABLISHED_V6) {

  }
  if (inFixedValues->layerId == FWPS_LAYER_STREAM_V4) {

  }
  if (inFixedValues->layerId == FWPS_LAYER_STREAM_V6) {

  }
}

NTSTATUS OnCalloutNotify(FWPS_CALLOUT_NOTIFY_TYPE notifyType, const GUID* filterKey, FWPS_FILTER0* filter) {
  UNREFERENCED_PARAMETER(filter);
  UNICODE_STRING sguid = RTL_CONSTANT_STRING(L"<NoGuid>");
  if (filterKey) RtlStringFromGUID(filterKey, &sguid);
  if (notifyType == FWPS_CALLOUT_NOTIFY_ADD_FILTER) {
    DbgPrint("Filter added: %wZ\n", sguid);
  }
  else if (notifyType == FWPS_CALLOUT_NOTIFY_DELETE_FILTER) {
    DbgPrint("Filter deleted: %wZ\n", sguid);
  }
  if (filterKey) RtlFreeUnicodeString(&sguid);
  return STATUS_SUCCESS;
}

NTSTATUS RegisterNetworkCallout(PVOID devObj) {
  NTSTATUS status = STATUS_SUCCESS;
  const GUID* guids[] = {
    &GUID_CALLOUT_LAYER_ESTABLISHED_V4,
    &GUID_CALLOUT_LAYER_ESTABLISHED_V6,
    &GUID_CALLOUT_LAYER_STREAM_V4,
    &GUID_CALLOUT_LAYER_STREAM_V6
  };
  for (int i = 0; i < GUID_CALLOUT_COUNT; ++ i) {
    FWPS_CALLOUT0 callout = {0};
    RtlZeroMemory(&callout, sizeof(callout));
    callout.calloutKey = *guids[i];
    callout.notifyFn = OnCalloutNotify;
    callout.classifyFn = OnCalloutClassify;
    status |= FwpsCalloutRegister0(devObj, &callout, NULL);
  }

  return status;
}

BOOLEAN WfpKernelRegister(PVOID devObj) {
  NTSTATUS status;
  // UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\ProcessMonitoringFilter");
  // UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\ProcessMonitoringFilter");
  // BOOLEAN isSymlinkCreated = FALSE;
  do {
    // status = IoCreateSymbolicLink(&symLink, &devName);
    // if (!NT_SUCCESS(status)) break;
    // isSymlinkCreated = TRUE;
    status = RegisterNetworkCallout(devObj);
    if (!NT_SUCCESS(status)) break;
  } while (FALSE);
  if (!NT_SUCCESS(status)) {
    DbgPrint("WFP Kernel register failed with status=0x%X\n", status);
    return FALSE;
  }
  return TRUE;
}

VOID WfpKernelUnregister() {
  const GUID* guids[] = {
    &GUID_CALLOUT_LAYER_ESTABLISHED_V4,
    &GUID_CALLOUT_LAYER_ESTABLISHED_V6,
    &GUID_CALLOUT_LAYER_STREAM_V4,
    &GUID_CALLOUT_LAYER_STREAM_V6
  };
  for (int i = 0; i < GUID_CALLOUT_COUNT; ++ i) {
    FwpsCalloutUnregisterByKey0(guids[i]);
  }
  DbgPrint("Unregistered network callout.\n");
}

//....................................User...................................................

DWORD RegisterProvider() {
  HANDLE hEngine;
  DWORD error = FwpmEngineOpen0(NULL, RPC_C_AUTHN_DEFAULT, NULL, NULL, &hEngine);
  if (error) return error;

  FWPM_PROVIDER0 *provider;
  error = FwpmProviderGetByKey(NULL, &GUID_WFP_NETWORK_PROVIDER, &provider);
  if (error != 0) {
    FWPM_PROVIDER0 reg = {0};
    WCHAR name[] = L"WFP NETWORK";
    reg.displayData.name = name;
    reg.providerKey = GUID_WFP_NETWORK_PROVIDER;
    reg.flags = 0;

    error = FwpmProviderAdd0(hEngine, &reg, NULL);
  } else {
    FwpmFreeMemory0((void *)&provider);
  }

  FwpmEngineClose(hEngine);
  return error;
}

BOOLEAN AddCallout() {
  HANDLE hEngine;
  DWORD error = FwpmEngineOpen0(NULL, RPC_C_AUTHN_DEFAULT, NULL, NULL, &hEngine);
  if (error) return FALSE;
  do {
    {
      FWPM_CALLOUT0 *callout;
      if (FwpmCalloutGetByKey0(hEngine, &GUID_CALLOUT_LAYER_ESTABLISHED_V4, &callout) == 0) {
        FwpmFreeMemory0((void **)&callout);
        break;
      }
    }

    const NET_CALLOUT callouts[] = {
      {&GUID_CALLOUT_LAYER_ESTABLISHED_V4, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4},
      {&GUID_CALLOUT_LAYER_ESTABLISHED_V6, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6},
      {&GUID_CALLOUT_LAYER_STREAM_V4, &FWPM_LAYER_STREAM_V4},
      {&GUID_CALLOUT_LAYER_STREAM_V6, &FWPM_LAYER_STREAM_V6}
    };

    error = FwpmTransactionBegin0(hEngine, 0);
    if (error) break;
    for (int i = 0; i < GUID_CALLOUT_COUNT; ++ i) {
      FWPM_CALLOUT0 callout = {0};
      RtlZeroMemory(&callout, sizeof(callout));
      callout.applicableLayer = *callouts[i].layer;
      callout.calloutKey = *callouts[i].guid;
      WCHAR name[] = L"Network monitoring Callout";
      callout.displayData.name = name;
      callout.providerKey = (GUID*)&GUID_WFP_NETWORK_PROVIDER;
      FwpmCalloutAdd0(hEngine, &callout, NULL, NULL);
    }
    error = FwpmTransactionCommit0(hEngine);
  } while (FALSE);

  FwpmEngineClose0(hEngine);
  return error == 0;
}

BOOLEAN AddFilter() {
  HANDLE hEngine;
  DWORD error = FwpmEngineOpen0(NULL, RPC_C_AUTHN_DEFAULT, NULL, NULL, &hEngine);
  if (error) return FALSE;

  do {
    {
      FWPM_FILTER0* filter;
      if (FwpmFilterGetByKey0(hEngine, &GUID_FILTER_LAYER_ESTABLISHED_V4, &filter) == 0) {
        FwpmFreeMemory0((void **)&filter);
        break;
      }
    }

    const NET_FILTER filters[] = {
      {&GUID_FILTER_LAYER_ESTABLISHED_V4, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4, &GUID_CALLOUT_LAYER_ESTABLISHED_V4},
      {&GUID_FILTER_LAYER_ESTABLISHED_V6, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6, &GUID_CALLOUT_LAYER_ESTABLISHED_V6},
      {&GUID_FILTER_LAYER_STREAM_V4, &FWPM_LAYER_STREAM_V4, &GUID_CALLOUT_LAYER_STREAM_V4},
      {&GUID_FILTER_LAYER_STREAM_V6, &FWPM_LAYER_STREAM_V6, &GUID_CALLOUT_LAYER_STREAM_V6}
    };

    error = FwpmTransactionBegin0(hEngine, 0);
    if (error) break;
    for (int i = 0; i < GUID_CALLOUT_COUNT; ++ i) {
      FWPM_FILTER0 filter = {0};
      RtlZeroMemory(&filter, sizeof(filter));
      filter.filterKey = *filters[i].guid;
      filter.providerKey = (GUID*)&GUID_WFP_NETWORK_PROVIDER;
      WCHAR name[] = L"Network monitoring filter";
      filter.displayData.name = name;
      filter.weight.uint8 = 8;
      filter.weight.type = FWP_UINT8;
      filter.layerKey = *filters[i].layer;
      filter.action.type = FWP_ACTION_CALLOUT_UNKNOWN;
      filter.action.calloutKey = *filters[i].callout;
      FwpmFilterAdd0(hEngine, &filter, NULL, NULL);
    }
    error = FwpmTransactionCommit0(hEngine);
  } while (FALSE);

  FwpmEngineClose0(hEngine);
  return error == 0;
}

BOOLEAN DeleteCallout() {
  HANDLE hEngine;
  DWORD error = FwpmEngineOpen0(NULL, RPC_C_AUTHN_DEFAULT, NULL, NULL, &hEngine);
  if (error) return FALSE;

  FwpmCalloutDeleteByKey0(hEngine, &GUID_CALLOUT_LAYER_ESTABLISHED_V4);
  FwpmCalloutDeleteByKey0(hEngine, &GUID_CALLOUT_LAYER_ESTABLISHED_V6);
  FwpmCalloutDeleteByKey0(hEngine, &GUID_CALLOUT_LAYER_STREAM_V4);
  FwpmCalloutDeleteByKey0(hEngine, &GUID_CALLOUT_LAYER_STREAM_V6);

  FwpmEngineClose0(hEngine);
  return TRUE;
}

BOOLEAN DeleteFilter() {
  HANDLE hEngine;
  DWORD error = FwpmEngineOpen0(NULL, RPC_C_AUTHN_DEFAULT, NULL, NULL, &hEngine);
  if (error) return FALSE;

  FwpmFilterDeleteByKey0(hEngine, &GUID_FILTER_LAYER_ESTABLISHED_V4);
  FwpmFilterDeleteByKey0(hEngine, &GUID_FILTER_LAYER_ESTABLISHED_V6);
  FwpmFilterDeleteByKey0(hEngine, &GUID_FILTER_LAYER_STREAM_V4);
  FwpmFilterDeleteByKey0(hEngine, &GUID_FILTER_LAYER_STREAM_V6);

  FwpmEngineClose0(hEngine);
  return TRUE;
}

BOOLEAN WfpUserExit() {
  return DeleteCallout() && DeleteFilter();
}

BOOLEAN WfpUserMain() {
  DWORD error = RegisterProvider();
  if (error) {
    DbgPrint("Error registering provider %u\n", error);
    return FALSE;
  }

  if (!AddCallout()) {
    DbgPrint("Error adding callouts\n");
    return FALSE;
  }

  if (!AddFilter()) {
    DbgPrint("Error adding filters\n");
    return FALSE;
  }

  return TRUE;
}
