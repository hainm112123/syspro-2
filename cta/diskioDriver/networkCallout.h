#pragma once
#ifndef __NETWORK_CALLOUT_H__
#define __NETWORK_CALLOUT_H__

#include <ntdef.h>

BOOLEAN WfpKernelRegister(PVOID devObj);
VOID WfpKernelUnregister();
BOOLEAN WfpUserMain();
BOOLEAN WfpUserExit();

#endif

