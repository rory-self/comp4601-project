set moduleName encode_chunk_Pipeline_VITIS_LOOP_33_5
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
set C_modelName {encode_chunk_Pipeline_VITIS_LOOP_33_5}
set C_modelType { void 0 }
set ap_memory_interface_dict [dict create]
dict set ap_memory_interface_dict out_r { MEM_WIDTH 8 MEM_SIZE 576 MASTER_TYPE BRAM_CTRL MEM_ADDRESS_MODE WORD_ADDRESS PACKAGE_IO port READ_LATENCY 0 }
set C_modelArgList {
	{ oi_453_ph int 32 regular  }
	{ nb_6_4_ph int 32 regular  }
	{ ob_8_4_ph int 8 regular  }
	{ pending_2 int 32 regular  }
	{ out_r int 8 regular {array 576 { 0 3 } 0 1 }  }
	{ oi_453_out int 32 regular {pointer 1}  }
	{ nb_6_4_out int 32 regular {pointer 1}  }
	{ ob_8_4_out int 8 regular {pointer 1}  }
}
set hasAXIMCache 0
set l_AXIML2Cache [list]
set AXIMCacheInstDict [dict create]
set C_modelArgMapList {[ 
	{ "Name" : "oi_453_ph", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "nb_6_4_ph", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "ob_8_4_ph", "interface" : "wire", "bitwidth" : 8, "direction" : "READONLY"} , 
 	{ "Name" : "pending_2", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "out_r", "interface" : "memory", "bitwidth" : 8, "direction" : "WRITEONLY"} , 
 	{ "Name" : "oi_453_out", "interface" : "wire", "bitwidth" : 32, "direction" : "WRITEONLY"} , 
 	{ "Name" : "nb_6_4_out", "interface" : "wire", "bitwidth" : 32, "direction" : "WRITEONLY"} , 
 	{ "Name" : "ob_8_4_out", "interface" : "wire", "bitwidth" : 8, "direction" : "WRITEONLY"} ]}
# RTL Port declarations: 
set portNum 20
set portList { 
	{ ap_clk sc_in sc_logic 1 clock -1 } 
	{ ap_rst sc_in sc_logic 1 reset -1 active_high_sync } 
	{ ap_start sc_in sc_logic 1 start -1 } 
	{ ap_done sc_out sc_logic 1 predone -1 } 
	{ ap_idle sc_out sc_logic 1 done -1 } 
	{ ap_ready sc_out sc_logic 1 ready -1 } 
	{ oi_453_ph sc_in sc_lv 32 signal 0 } 
	{ nb_6_4_ph sc_in sc_lv 32 signal 1 } 
	{ ob_8_4_ph sc_in sc_lv 8 signal 2 } 
	{ pending_2 sc_in sc_lv 32 signal 3 } 
	{ out_r_address0 sc_out sc_lv 10 signal 4 } 
	{ out_r_ce0 sc_out sc_logic 1 signal 4 } 
	{ out_r_we0 sc_out sc_logic 1 signal 4 } 
	{ out_r_d0 sc_out sc_lv 8 signal 4 } 
	{ oi_453_out sc_out sc_lv 32 signal 5 } 
	{ oi_453_out_ap_vld sc_out sc_logic 1 outvld 5 } 
	{ nb_6_4_out sc_out sc_lv 32 signal 6 } 
	{ nb_6_4_out_ap_vld sc_out sc_logic 1 outvld 6 } 
	{ ob_8_4_out sc_out sc_lv 8 signal 7 } 
	{ ob_8_4_out_ap_vld sc_out sc_logic 1 outvld 7 } 
}
set NewPortList {[ 
	{ "name": "ap_clk", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "clock", "bundle":{"name": "ap_clk", "role": "default" }} , 
 	{ "name": "ap_rst", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "reset", "bundle":{"name": "ap_rst", "role": "default" }} , 
 	{ "name": "ap_start", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "start", "bundle":{"name": "ap_start", "role": "default" }} , 
 	{ "name": "ap_done", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "predone", "bundle":{"name": "ap_done", "role": "default" }} , 
 	{ "name": "ap_idle", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "done", "bundle":{"name": "ap_idle", "role": "default" }} , 
 	{ "name": "ap_ready", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "ready", "bundle":{"name": "ap_ready", "role": "default" }} , 
 	{ "name": "oi_453_ph", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "oi_453_ph", "role": "default" }} , 
 	{ "name": "nb_6_4_ph", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "nb_6_4_ph", "role": "default" }} , 
 	{ "name": "ob_8_4_ph", "direction": "in", "datatype": "sc_lv", "bitwidth":8, "type": "signal", "bundle":{"name": "ob_8_4_ph", "role": "default" }} , 
 	{ "name": "pending_2", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "pending_2", "role": "default" }} , 
 	{ "name": "out_r_address0", "direction": "out", "datatype": "sc_lv", "bitwidth":10, "type": "signal", "bundle":{"name": "out_r", "role": "address0" }} , 
 	{ "name": "out_r_ce0", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "out_r", "role": "ce0" }} , 
 	{ "name": "out_r_we0", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "out_r", "role": "we0" }} , 
 	{ "name": "out_r_d0", "direction": "out", "datatype": "sc_lv", "bitwidth":8, "type": "signal", "bundle":{"name": "out_r", "role": "d0" }} , 
 	{ "name": "oi_453_out", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "oi_453_out", "role": "default" }} , 
 	{ "name": "oi_453_out_ap_vld", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "outvld", "bundle":{"name": "oi_453_out", "role": "ap_vld" }} , 
 	{ "name": "nb_6_4_out", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "nb_6_4_out", "role": "default" }} , 
 	{ "name": "nb_6_4_out_ap_vld", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "outvld", "bundle":{"name": "nb_6_4_out", "role": "ap_vld" }} , 
 	{ "name": "ob_8_4_out", "direction": "out", "datatype": "sc_lv", "bitwidth":8, "type": "signal", "bundle":{"name": "ob_8_4_out", "role": "default" }} , 
 	{ "name": "ob_8_4_out_ap_vld", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "outvld", "bundle":{"name": "ob_8_4_out", "role": "ap_vld" }}  ]}

set ArgLastReadFirstWriteLatency {
	encode_chunk_Pipeline_VITIS_LOOP_33_5 {
		oi_453_ph {Type I LastRead 0 FirstWrite -1}
		nb_6_4_ph {Type I LastRead 0 FirstWrite -1}
		ob_8_4_ph {Type I LastRead 0 FirstWrite -1}
		pending_2 {Type I LastRead 0 FirstWrite -1}
		out_r {Type O LastRead -1 FirstWrite 1}
		oi_453_out {Type O LastRead -1 FirstWrite 0}
		nb_6_4_out {Type O LastRead -1 FirstWrite 0}
		ob_8_4_out {Type O LastRead -1 FirstWrite 0}}}

set hasDtUnsupportedChannel 0

set PerformanceInfo {[
	{"Name" : "Latency", "Min" : "-1", "Max" : "-1"}
	, {"Name" : "Interval", "Min" : "0", "Max" : "0"}
]}

set PipelineEnableSignalInfo {[
	{"Pipeline" : "0", "EnableSignal" : "ap_enable_pp0"}
]}

set Spec2ImplPortList { 
	oi_453_ph { ap_none {  { oi_453_ph in_data 0 32 } } }
	nb_6_4_ph { ap_none {  { nb_6_4_ph in_data 0 32 } } }
	ob_8_4_ph { ap_none {  { ob_8_4_ph in_data 0 8 } } }
	pending_2 { ap_none {  { pending_2 in_data 0 32 } } }
	out_r { ap_memory {  { out_r_address0 mem_address 1 10 }  { out_r_ce0 mem_ce 1 1 }  { out_r_we0 mem_we 1 1 }  { out_r_d0 mem_din 1 8 } } }
	oi_453_out { ap_vld {  { oi_453_out out_data 1 32 }  { oi_453_out_ap_vld out_vld 1 1 } } }
	nb_6_4_out { ap_vld {  { nb_6_4_out out_data 1 32 }  { nb_6_4_out_ap_vld out_vld 1 1 } } }
	ob_8_4_out { ap_vld {  { ob_8_4_out out_data 1 8 }  { ob_8_4_out_ap_vld out_vld 1 1 } } }
}
