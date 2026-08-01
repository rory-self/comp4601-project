set moduleName encode_bit_Pipeline_VITIS_LOOP_50_1
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
set C_modelName {encode_bit_Pipeline_VITIS_LOOP_50_1}
set C_modelType { void 0 }
set ap_memory_interface_dict [dict create]
dict set ap_memory_interface_dict out_r { MEM_WIDTH 8 MEM_SIZE 576 MASTER_TYPE BRAM_CTRL MEM_ADDRESS_MODE WORD_ADDRESS PACKAGE_IO port READ_LATENCY 0 }
set C_modelArgList {
	{ oi_2_ph int 32 regular  }
	{ nb_2_ph int 32 regular  }
	{ ob_2_ph int 32 regular  }
	{ pending_0 int 32 regular  }
	{ out_r int 8 regular {array 576 { 0 3 } 0 1 }  }
	{ oi_2_out int 32 regular {pointer 2}  }
	{ nb_2_out int 32 regular {pointer 2}  }
	{ ob_2_out int 32 regular {pointer 2}  }
}
set hasAXIMCache 0
set l_AXIML2Cache [list]
set AXIMCacheInstDict [dict create]
set C_modelArgMapList {[ 
	{ "Name" : "oi_2_ph", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "nb_2_ph", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "ob_2_ph", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "pending_0", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "out_r", "interface" : "memory", "bitwidth" : 8, "direction" : "WRITEONLY"} , 
 	{ "Name" : "oi_2_out", "interface" : "wire", "bitwidth" : 32, "direction" : "READWRITE"} , 
 	{ "Name" : "nb_2_out", "interface" : "wire", "bitwidth" : 32, "direction" : "READWRITE"} , 
 	{ "Name" : "ob_2_out", "interface" : "wire", "bitwidth" : 32, "direction" : "READWRITE"} ]}
# RTL Port declarations: 
set portNum 23
set portList { 
	{ ap_clk sc_in sc_logic 1 clock -1 } 
	{ ap_rst sc_in sc_logic 1 reset -1 active_high_sync } 
	{ ap_start sc_in sc_logic 1 start -1 } 
	{ ap_done sc_out sc_logic 1 predone -1 } 
	{ ap_idle sc_out sc_logic 1 done -1 } 
	{ ap_ready sc_out sc_logic 1 ready -1 } 
	{ oi_2_ph sc_in sc_lv 32 signal 0 } 
	{ nb_2_ph sc_in sc_lv 32 signal 1 } 
	{ ob_2_ph sc_in sc_lv 32 signal 2 } 
	{ pending_0 sc_in sc_lv 32 signal 3 } 
	{ out_r_address0 sc_out sc_lv 10 signal 4 } 
	{ out_r_ce0 sc_out sc_logic 1 signal 4 } 
	{ out_r_we0 sc_out sc_logic 1 signal 4 } 
	{ out_r_d0 sc_out sc_lv 8 signal 4 } 
	{ oi_2_out_i sc_in sc_lv 32 signal 5 } 
	{ oi_2_out_o sc_out sc_lv 32 signal 5 } 
	{ oi_2_out_o_ap_vld sc_out sc_logic 1 outvld 5 } 
	{ nb_2_out_i sc_in sc_lv 32 signal 6 } 
	{ nb_2_out_o sc_out sc_lv 32 signal 6 } 
	{ nb_2_out_o_ap_vld sc_out sc_logic 1 outvld 6 } 
	{ ob_2_out_i sc_in sc_lv 32 signal 7 } 
	{ ob_2_out_o sc_out sc_lv 32 signal 7 } 
	{ ob_2_out_o_ap_vld sc_out sc_logic 1 outvld 7 } 
}
set NewPortList {[ 
	{ "name": "ap_clk", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "clock", "bundle":{"name": "ap_clk", "role": "default" }} , 
 	{ "name": "ap_rst", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "reset", "bundle":{"name": "ap_rst", "role": "default" }} , 
 	{ "name": "ap_start", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "start", "bundle":{"name": "ap_start", "role": "default" }} , 
 	{ "name": "ap_done", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "predone", "bundle":{"name": "ap_done", "role": "default" }} , 
 	{ "name": "ap_idle", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "done", "bundle":{"name": "ap_idle", "role": "default" }} , 
 	{ "name": "ap_ready", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "ready", "bundle":{"name": "ap_ready", "role": "default" }} , 
 	{ "name": "oi_2_ph", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "oi_2_ph", "role": "default" }} , 
 	{ "name": "nb_2_ph", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "nb_2_ph", "role": "default" }} , 
 	{ "name": "ob_2_ph", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ob_2_ph", "role": "default" }} , 
 	{ "name": "pending_0", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "pending_0", "role": "default" }} , 
 	{ "name": "out_r_address0", "direction": "out", "datatype": "sc_lv", "bitwidth":10, "type": "signal", "bundle":{"name": "out_r", "role": "address0" }} , 
 	{ "name": "out_r_ce0", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "out_r", "role": "ce0" }} , 
 	{ "name": "out_r_we0", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "out_r", "role": "we0" }} , 
 	{ "name": "out_r_d0", "direction": "out", "datatype": "sc_lv", "bitwidth":8, "type": "signal", "bundle":{"name": "out_r", "role": "d0" }} , 
 	{ "name": "oi_2_out_i", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "oi_2_out", "role": "i" }} , 
 	{ "name": "oi_2_out_o", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "oi_2_out", "role": "o" }} , 
 	{ "name": "oi_2_out_o_ap_vld", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "outvld", "bundle":{"name": "oi_2_out", "role": "o_ap_vld" }} , 
 	{ "name": "nb_2_out_i", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "nb_2_out", "role": "i" }} , 
 	{ "name": "nb_2_out_o", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "nb_2_out", "role": "o" }} , 
 	{ "name": "nb_2_out_o_ap_vld", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "outvld", "bundle":{"name": "nb_2_out", "role": "o_ap_vld" }} , 
 	{ "name": "ob_2_out_i", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ob_2_out", "role": "i" }} , 
 	{ "name": "ob_2_out_o", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ob_2_out", "role": "o" }} , 
 	{ "name": "ob_2_out_o_ap_vld", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "outvld", "bundle":{"name": "ob_2_out", "role": "o_ap_vld" }}  ]}

set ArgLastReadFirstWriteLatency {
	encode_bit_Pipeline_VITIS_LOOP_50_1 {
		oi_2_ph {Type I LastRead 0 FirstWrite -1}
		nb_2_ph {Type I LastRead 0 FirstWrite -1}
		ob_2_ph {Type I LastRead 0 FirstWrite -1}
		pending_0 {Type I LastRead 0 FirstWrite -1}
		out_r {Type O LastRead -1 FirstWrite 1}
		oi_2_out {Type IO LastRead 1 FirstWrite 0}
		nb_2_out {Type IO LastRead 1 FirstWrite 0}
		ob_2_out {Type IO LastRead 1 FirstWrite 0}}}

set hasDtUnsupportedChannel 0

set PerformanceInfo {[
	{"Name" : "Latency", "Min" : "-1", "Max" : "-1"}
	, {"Name" : "Interval", "Min" : "0", "Max" : "0"}
]}

set PipelineEnableSignalInfo {[
	{"Pipeline" : "0", "EnableSignal" : "ap_enable_pp0"}
]}

set Spec2ImplPortList { 
	oi_2_ph { ap_none {  { oi_2_ph in_data 0 32 } } }
	nb_2_ph { ap_none {  { nb_2_ph in_data 0 32 } } }
	ob_2_ph { ap_none {  { ob_2_ph in_data 0 32 } } }
	pending_0 { ap_none {  { pending_0 in_data 0 32 } } }
	out_r { ap_memory {  { out_r_address0 mem_address 1 10 }  { out_r_ce0 mem_ce 1 1 }  { out_r_we0 mem_we 1 1 }  { out_r_d0 mem_din 1 8 } } }
	oi_2_out { ap_ovld {  { oi_2_out_i in_data 0 32 }  { oi_2_out_o out_data 1 32 }  { oi_2_out_o_ap_vld out_vld 1 1 } } }
	nb_2_out { ap_ovld {  { nb_2_out_i in_data 0 32 }  { nb_2_out_o out_data 1 32 }  { nb_2_out_o_ap_vld out_vld 1 1 } } }
	ob_2_out { ap_ovld {  { ob_2_out_i in_data 0 32 }  { ob_2_out_o out_data 1 32 }  { ob_2_out_o_ap_vld out_vld 1 1 } } }
}
