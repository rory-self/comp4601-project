// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2.1 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2026 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XARITH_ENCODE_H
#define XARITH_ENCODE_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#ifndef __linux__
#include "xil_types.h"
#include "xil_assert.h"
#include "xstatus.h"
#include "xil_io.h"
#else
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#endif
#include "xarith_encode_hw.h"

/**************************** Type Definitions ******************************/
#ifdef __linux__
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#else
typedef struct {
#ifdef SDT
    char *Name;
#else
    u16 DeviceId;
#endif
    u64 Control_BaseAddress;
} XArith_encode_Config;
#endif

typedef struct {
    u64 Control_BaseAddress;
    u32 IsReady;
} XArith_encode;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XArith_encode_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XArith_encode_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XArith_encode_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XArith_encode_ReadReg(BaseAddress, RegOffset) \
    *(volatile u32*)((BaseAddress) + (RegOffset))

#define Xil_AssertVoid(expr)    assert(expr)
#define Xil_AssertNonvoid(expr) assert(expr)

#define XST_SUCCESS             0
#define XST_DEVICE_NOT_FOUND    2
#define XST_OPEN_DEVICE_FAILED  3
#define XIL_COMPONENT_IS_READY  1
#endif

/************************** Function Prototypes *****************************/
#ifndef __linux__
#ifdef SDT
int XArith_encode_Initialize(XArith_encode *InstancePtr, UINTPTR BaseAddress);
XArith_encode_Config* XArith_encode_LookupConfig(UINTPTR BaseAddress);
#else
int XArith_encode_Initialize(XArith_encode *InstancePtr, u16 DeviceId);
XArith_encode_Config* XArith_encode_LookupConfig(u16 DeviceId);
#endif
int XArith_encode_CfgInitialize(XArith_encode *InstancePtr, XArith_encode_Config *ConfigPtr);
#else
int XArith_encode_Initialize(XArith_encode *InstancePtr, const char* InstanceName);
int XArith_encode_Release(XArith_encode *InstancePtr);
#endif

void XArith_encode_Start(XArith_encode *InstancePtr);
u32 XArith_encode_IsDone(XArith_encode *InstancePtr);
u32 XArith_encode_IsIdle(XArith_encode *InstancePtr);
u32 XArith_encode_IsReady(XArith_encode *InstancePtr);
void XArith_encode_Continue(XArith_encode *InstancePtr);
void XArith_encode_EnableAutoRestart(XArith_encode *InstancePtr);
void XArith_encode_DisableAutoRestart(XArith_encode *InstancePtr);
u32 XArith_encode_Get_return(XArith_encode *InstancePtr);

void XArith_encode_Set_in_r(XArith_encode *InstancePtr, u64 Data);
u64 XArith_encode_Get_in_r(XArith_encode *InstancePtr);
void XArith_encode_Set_n(XArith_encode *InstancePtr, u32 Data);
u32 XArith_encode_Get_n(XArith_encode *InstancePtr);
void XArith_encode_Set_out_r(XArith_encode *InstancePtr, u64 Data);
u64 XArith_encode_Get_out_r(XArith_encode *InstancePtr);

void XArith_encode_InterruptGlobalEnable(XArith_encode *InstancePtr);
void XArith_encode_InterruptGlobalDisable(XArith_encode *InstancePtr);
void XArith_encode_InterruptEnable(XArith_encode *InstancePtr, u32 Mask);
void XArith_encode_InterruptDisable(XArith_encode *InstancePtr, u32 Mask);
void XArith_encode_InterruptClear(XArith_encode *InstancePtr, u32 Mask);
u32 XArith_encode_InterruptGetEnabled(XArith_encode *InstancePtr);
u32 XArith_encode_InterruptGetStatus(XArith_encode *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
