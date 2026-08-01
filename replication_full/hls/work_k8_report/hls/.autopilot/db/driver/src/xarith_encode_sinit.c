// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2.1 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2026 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef __linux__

#include "xstatus.h"
#ifdef SDT
#include "xparameters.h"
#endif
#include "xarith_encode.h"

extern XArith_encode_Config XArith_encode_ConfigTable[];

#ifdef SDT
XArith_encode_Config *XArith_encode_LookupConfig(UINTPTR BaseAddress) {
	XArith_encode_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XArith_encode_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XArith_encode_ConfigTable[Index].Control_BaseAddress == BaseAddress) {
			ConfigPtr = &XArith_encode_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XArith_encode_Initialize(XArith_encode *InstancePtr, UINTPTR BaseAddress) {
	XArith_encode_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XArith_encode_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XArith_encode_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XArith_encode_Config *XArith_encode_LookupConfig(u16 DeviceId) {
	XArith_encode_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XARITH_ENCODE_NUM_INSTANCES; Index++) {
		if (XArith_encode_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XArith_encode_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XArith_encode_Initialize(XArith_encode *InstancePtr, u16 DeviceId) {
	XArith_encode_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XArith_encode_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XArith_encode_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

