set moduleName arith_encode_Pipeline_Header
set isTopModule 0
set isCombinational 0
set isDatapathOnly 0
set isPipelined 1
set isPipelined_legacy 1
set pipeline_type loop_auto_rewind
set FunctionProtocol ap_ctrl_hs
set restart_counter_num 0
set isOneStateSeq 0
set ProfileFlag 0
set StallSigGenFlag 0
set isEnableWaveformDebug 1
set hasInterrupt 0
set DLRegFirstOffset 0
set DLRegItemOffset 0
set svuvm_can_support 1
set cdfgNum 16
set C_modelName {arith_encode_Pipeline_Header}
set C_modelType { void 0 }
set ap_memory_interface_dict [dict create]
set C_modelArgList {
	{ empty_37 int 16 regular  }
	{ empty_38 int 16 regular  }
	{ empty_39 int 16 regular  }
	{ empty_40 int 16 regular  }
	{ empty_41 int 16 regular  }
	{ empty_42 int 16 regular  }
	{ empty_43 int 16 regular  }
	{ empty int 16 regular  }
	{ shiftreg_out int 128 regular {pointer 1}  }
}
set hasAXIMCache 0
set l_AXIML2Cache [list]
set AXIMCacheInstDict [dict create]
set C_modelArgMapList {[ 
	{ "Name" : "empty_37", "interface" : "wire", "bitwidth" : 16, "direction" : "READONLY"} , 
 	{ "Name" : "empty_38", "interface" : "wire", "bitwidth" : 16, "direction" : "READONLY"} , 
 	{ "Name" : "empty_39", "interface" : "wire", "bitwidth" : 16, "direction" : "READONLY"} , 
 	{ "Name" : "empty_40", "interface" : "wire", "bitwidth" : 16, "direction" : "READONLY"} , 
 	{ "Name" : "empty_41", "interface" : "wire", "bitwidth" : 16, "direction" : "READONLY"} , 
 	{ "Name" : "empty_42", "interface" : "wire", "bitwidth" : 16, "direction" : "READONLY"} , 
 	{ "Name" : "empty_43", "interface" : "wire", "bitwidth" : 16, "direction" : "READONLY"} , 
 	{ "Name" : "empty", "interface" : "wire", "bitwidth" : 16, "direction" : "READONLY"} , 
 	{ "Name" : "shiftreg_out", "interface" : "wire", "bitwidth" : 128, "direction" : "WRITEONLY"} ]}
# RTL Port declarations: 
set portNum 16
set portList { 
	{ ap_clk sc_in sc_logic 1 clock -1 } 
	{ ap_rst sc_in sc_logic 1 reset -1 active_high_sync } 
	{ ap_start sc_in sc_logic 1 start -1 } 
	{ ap_done sc_out sc_logic 1 predone -1 } 
	{ ap_idle sc_out sc_logic 1 done -1 } 
	{ ap_ready sc_out sc_logic 1 ready -1 } 
	{ empty_37 sc_in sc_lv 16 signal 0 } 
	{ empty_38 sc_in sc_lv 16 signal 1 } 
	{ empty_39 sc_in sc_lv 16 signal 2 } 
	{ empty_40 sc_in sc_lv 16 signal 3 } 
	{ empty_41 sc_in sc_lv 16 signal 4 } 
	{ empty_42 sc_in sc_lv 16 signal 5 } 
	{ empty_43 sc_in sc_lv 16 signal 6 } 
	{ empty sc_in sc_lv 16 signal 7 } 
	{ shiftreg_out sc_out sc_lv 128 signal 8 } 
	{ shiftreg_out_ap_vld sc_out sc_logic 1 outvld 8 } 
}
set NewPortList {[ 
	{ "name": "ap_clk", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "clock", "bundle":{"name": "ap_clk", "role": "default" }} , 
 	{ "name": "ap_rst", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "reset", "bundle":{"name": "ap_rst", "role": "default" }} , 
 	{ "name": "ap_start", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "start", "bundle":{"name": "ap_start", "role": "default" }} , 
 	{ "name": "ap_done", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "predone", "bundle":{"name": "ap_done", "role": "default" }} , 
 	{ "name": "ap_idle", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "done", "bundle":{"name": "ap_idle", "role": "default" }} , 
 	{ "name": "ap_ready", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "ready", "bundle":{"name": "ap_ready", "role": "default" }} , 
 	{ "name": "empty_37", "direction": "in", "datatype": "sc_lv", "bitwidth":16, "type": "signal", "bundle":{"name": "empty_37", "role": "default" }} , 
 	{ "name": "empty_38", "direction": "in", "datatype": "sc_lv", "bitwidth":16, "type": "signal", "bundle":{"name": "empty_38", "role": "default" }} , 
 	{ "name": "empty_39", "direction": "in", "datatype": "sc_lv", "bitwidth":16, "type": "signal", "bundle":{"name": "empty_39", "role": "default" }} , 
 	{ "name": "empty_40", "direction": "in", "datatype": "sc_lv", "bitwidth":16, "type": "signal", "bundle":{"name": "empty_40", "role": "default" }} , 
 	{ "name": "empty_41", "direction": "in", "datatype": "sc_lv", "bitwidth":16, "type": "signal", "bundle":{"name": "empty_41", "role": "default" }} , 
 	{ "name": "empty_42", "direction": "in", "datatype": "sc_lv", "bitwidth":16, "type": "signal", "bundle":{"name": "empty_42", "role": "default" }} , 
 	{ "name": "empty_43", "direction": "in", "datatype": "sc_lv", "bitwidth":16, "type": "signal", "bundle":{"name": "empty_43", "role": "default" }} , 
 	{ "name": "empty", "direction": "in", "datatype": "sc_lv", "bitwidth":16, "type": "signal", "bundle":{"name": "empty", "role": "default" }} , 
 	{ "name": "shiftreg_out", "direction": "out", "datatype": "sc_lv", "bitwidth":128, "type": "signal", "bundle":{"name": "shiftreg_out", "role": "default" }} , 
 	{ "name": "shiftreg_out_ap_vld", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "outvld", "bundle":{"name": "shiftreg_out", "role": "ap_vld" }}  ]}

set ArgLastReadFirstWriteLatency {
	arith_encode_Pipeline_Header {
		empty_37 {Type I LastRead 0 FirstWrite -1}
		empty_38 {Type I LastRead 0 FirstWrite -1}
		empty_39 {Type I LastRead 0 FirstWrite -1}
		empty_40 {Type I LastRead 0 FirstWrite -1}
		empty_41 {Type I LastRead 0 FirstWrite -1}
		empty_42 {Type I LastRead 0 FirstWrite -1}
		empty_43 {Type I LastRead 0 FirstWrite -1}
		empty {Type I LastRead 0 FirstWrite -1}
		shiftreg_out {Type O LastRead -1 FirstWrite 1}}}

set hasDtUnsupportedChannel 0

set PerformanceInfo {[
	{"Name" : "Latency", "Min" : "10", "Max" : "10"}
	, {"Name" : "Interval", "Min" : "9", "Max" : "9"}
]}

set PipelineEnableSignalInfo {[
	{"Pipeline" : "0", "EnableSignal" : "ap_enable_pp0"}
]}

set Spec2ImplPortList { 
	empty_37 { ap_none {  { empty_37 in_data 0 16 } } }
	empty_38 { ap_none {  { empty_38 in_data 0 16 } } }
	empty_39 { ap_none {  { empty_39 in_data 0 16 } } }
	empty_40 { ap_none {  { empty_40 in_data 0 16 } } }
	empty_41 { ap_none {  { empty_41 in_data 0 16 } } }
	empty_42 { ap_none {  { empty_42 in_data 0 16 } } }
	empty_43 { ap_none {  { empty_43 in_data 0 16 } } }
	empty { ap_none {  { empty in_data 0 16 } } }
	shiftreg_out { ap_vld {  { shiftreg_out out_data 1 128 }  { shiftreg_out_ap_vld out_vld 1 1 } } }
}
