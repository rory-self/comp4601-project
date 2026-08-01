// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2.1 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2026 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xarith_encode.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XArith_encode_CfgInitialize(XArith_encode *InstancePtr, XArith_encode_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Control_BaseAddress = ConfigPtr->Control_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XArith_encode_Start(XArith_encode *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_AP_CTRL) & 0x80;
    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XArith_encode_IsDone(XArith_encode *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XArith_encode_IsIdle(XArith_encode *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XArith_encode_IsReady(XArith_encode *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XArith_encode_Continue(XArith_encode *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_AP_CTRL) & 0x80;
    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_AP_CTRL, Data | 0x10);
}

void XArith_encode_EnableAutoRestart(XArith_encode *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_AP_CTRL, 0x80);
}

void XArith_encode_DisableAutoRestart(XArith_encode *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_AP_CTRL, 0);
}

u32 XArith_encode_Get_return(XArith_encode *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_AP_RETURN);
    return Data;
}
void XArith_encode_Set_in_r(XArith_encode *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_IN_R_DATA, (u32)(Data));
    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_IN_R_DATA + 4, (u32)(Data >> 32));
}

u64 XArith_encode_Get_in_r(XArith_encode *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_IN_R_DATA);
    Data += (u64)XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_IN_R_DATA + 4) << 32;
    return Data;
}

void XArith_encode_Set_n(XArith_encode *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_N_DATA, Data);
}

u32 XArith_encode_Get_n(XArith_encode *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_N_DATA);
    return Data;
}

void XArith_encode_Set_out_r(XArith_encode *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_OUT_R_DATA, (u32)(Data));
    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_OUT_R_DATA + 4, (u32)(Data >> 32));
}

u64 XArith_encode_Get_out_r(XArith_encode *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_OUT_R_DATA);
    Data += (u64)XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_OUT_R_DATA + 4) << 32;
    return Data;
}

void XArith_encode_InterruptGlobalEnable(XArith_encode *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_GIE, 1);
}

void XArith_encode_InterruptGlobalDisable(XArith_encode *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_GIE, 0);
}

void XArith_encode_InterruptEnable(XArith_encode *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_IER);
    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_IER, Register | Mask);
}

void XArith_encode_InterruptDisable(XArith_encode *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_IER);
    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_IER, Register & (~Mask));
}

void XArith_encode_InterruptClear(XArith_encode *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XArith_encode_WriteReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_ISR, Mask);
}

u32 XArith_encode_InterruptGetEnabled(XArith_encode *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_IER);
}

u32 XArith_encode_InterruptGetStatus(XArith_encode *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XArith_encode_ReadReg(InstancePtr->Control_BaseAddress, XARITH_ENCODE_CONTROL_ADDR_ISR);
}

