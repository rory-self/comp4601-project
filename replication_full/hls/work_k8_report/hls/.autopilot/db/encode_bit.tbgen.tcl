set moduleName encode_bit
set isTopModule 0
set isCombinational 0
set isDatapathOnly 0
set isPipelined 0
set isPipelined_legacy 0
set pipeline_type none
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
set C_modelName {encode_bit}
set C_modelType { int 224 }
set ap_memory_interface_dict [dict create]
dict set ap_memory_interface_dict out_r { MEM_WIDTH 8 MEM_SIZE 576 MASTER_TYPE BRAM_CTRL MEM_ADDRESS_MODE WORD_ADDRESS PACKAGE_IO port READ_LATENCY 0 }
set C_modelArgList {
	{ low_read_6 int 32 regular  }
	{ low_read int 32 regular  }
	{ high_read_6 int 32 regular  }
	{ high_read int 32 regular  }
	{ pending_read int 32 regular  }
	{ ob_read int 32 regular  }
	{ nb_read int 32 regular  }
	{ out_r int 8 regular {array 576 { 0 3 } 0 1 }  }
	{ oi_read int 32 regular  }
	{ prob_read int 32 regular  }
	{ bit_r int 1 regular  }
}
set hasAXIMCache 0
set l_AXIML2Cache [list]
set AXIMCacheInstDict [dict create]
set C_modelArgMapList {[ 
	{ "Name" : "low_read_6", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "low_read", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "high_read_6", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "high_read", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "pending_read", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "ob_read", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "nb_read", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "out_r", "interface" : "memory", "bitwidth" : 8, "direction" : "WRITEONLY"} , 
 	{ "Name" : "oi_read", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "prob_read", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "bit_r", "interface" : "wire", "bitwidth" : 1, "direction" : "READONLY"} , 
 	{ "Name" : "ap_return", "interface" : "wire", "bitwidth" : 224} ]}
# RTL Port declarations: 
set portNum 27
set portList { 
	{ ap_clk sc_in sc_logic 1 clock -1 } 
	{ ap_rst sc_in sc_logic 1 reset -1 active_high_sync } 
	{ ap_start sc_in sc_logic 1 start -1 } 
	{ ap_done sc_out sc_logic 1 predone -1 } 
	{ ap_idle sc_out sc_logic 1 done -1 } 
	{ ap_ready sc_out sc_logic 1 ready -1 } 
	{ low_read_6 sc_in sc_lv 32 signal 0 } 
	{ low_read sc_in sc_lv 32 signal 1 } 
	{ high_read_6 sc_in sc_lv 32 signal 2 } 
	{ high_read sc_in sc_lv 32 signal 3 } 
	{ pending_read sc_in sc_lv 32 signal 4 } 
	{ ob_read sc_in sc_lv 32 signal 5 } 
	{ nb_read sc_in sc_lv 32 signal 6 } 
	{ out_r_address0 sc_out sc_lv 10 signal 7 } 
	{ out_r_ce0 sc_out sc_logic 1 signal 7 } 
	{ out_r_we0 sc_out sc_logic 1 signal 7 } 
	{ out_r_d0 sc_out sc_lv 8 signal 7 } 
	{ oi_read sc_in sc_lv 32 signal 8 } 
	{ prob_read sc_in sc_lv 32 signal 9 } 
	{ bit_r sc_in sc_lv 1 signal 10 } 
	{ ap_return_0 sc_out sc_lv 32 signal -1 } 
	{ ap_return_1 sc_out sc_lv 32 signal -1 } 
	{ ap_return_2 sc_out sc_lv 32 signal -1 } 
	{ ap_return_3 sc_out sc_lv 32 signal -1 } 
	{ ap_return_4 sc_out sc_lv 32 signal -1 } 
	{ ap_return_5 sc_out sc_lv 32 signal -1 } 
	{ ap_return_6 sc_out sc_lv 32 signal -1 } 
}
set NewPortList {[ 
	{ "name": "ap_clk", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "clock", "bundle":{"name": "ap_clk", "role": "default" }} , 
 	{ "name": "ap_rst", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "reset", "bundle":{"name": "ap_rst", "role": "default" }} , 
 	{ "name": "ap_start", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "start", "bundle":{"name": "ap_start", "role": "default" }} , 
 	{ "name": "ap_done", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "predone", "bundle":{"name": "ap_done", "role": "default" }} , 
 	{ "name": "ap_idle", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "done", "bundle":{"name": "ap_idle", "role": "default" }} , 
 	{ "name": "ap_ready", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "ready", "bundle":{"name": "ap_ready", "role": "default" }} , 
 	{ "name": "low_read_6", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "low_read_6", "role": "default" }} , 
 	{ "name": "low_read", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "low_read", "role": "default" }} , 
 	{ "name": "high_read_6", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "high_read_6", "role": "default" }} , 
 	{ "name": "high_read", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "high_read", "role": "default" }} , 
 	{ "name": "pending_read", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "pending_read", "role": "default" }} , 
 	{ "name": "ob_read", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ob_read", "role": "default" }} , 
 	{ "name": "nb_read", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "nb_read", "role": "default" }} , 
 	{ "name": "out_r_address0", "direction": "out", "datatype": "sc_lv", "bitwidth":10, "type": "signal", "bundle":{"name": "out_r", "role": "address0" }} , 
 	{ "name": "out_r_ce0", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "out_r", "role": "ce0" }} , 
 	{ "name": "out_r_we0", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "out_r", "role": "we0" }} , 
 	{ "name": "out_r_d0", "direction": "out", "datatype": "sc_lv", "bitwidth":8, "type": "signal", "bundle":{"name": "out_r", "role": "d0" }} , 
 	{ "name": "oi_read", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "oi_read", "role": "default" }} , 
 	{ "name": "prob_read", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "prob_read", "role": "default" }} , 
 	{ "name": "bit_r", "direction": "in", "datatype": "sc_lv", "bitwidth":1, "type": "signal", "bundle":{"name": "bit_r", "role": "default" }} , 
 	{ "name": "ap_return_0", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ap_return_0", "role": "default" }} , 
 	{ "name": "ap_return_1", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ap_return_1", "role": "default" }} , 
 	{ "name": "ap_return_2", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ap_return_2", "role": "default" }} , 
 	{ "name": "ap_return_3", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ap_return_3", "role": "default" }} , 
 	{ "name": "ap_return_4", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ap_return_4", "role": "default" }} , 
 	{ "name": "ap_return_5", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ap_return_5", "role": "default" }} , 
 	{ "name": "ap_return_6", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ap_return_6", "role": "default" }}  ]}

set ArgLastReadFirstWriteLatency {
	encode_bit {
		low_read_6 {Type I LastRead 2 FirstWrite -1}
		low_read {Type I LastRead 0 FirstWrite -1}
		high_read_6 {Type I LastRead 2 FirstWrite -1}
		high_read {Type I LastRead 0 FirstWrite -1}
		pending_read {Type I LastRead 0 FirstWrite -1}
		ob_read {Type I LastRead 0 FirstWrite -1}
		nb_read {Type I LastRead 0 FirstWrite -1}
		out_r {Type O LastRead -1 FirstWrite 1}
		oi_read {Type I LastRead 0 FirstWrite -1}
		prob_read {Type I LastRead 1 FirstWrite -1}
		bit_r {Type I LastRead 2 FirstWrite -1}}
	encode_bit_Pipeline_VITIS_LOOP_53_2 {
		oi_5_ph {Type I LastRead 0 FirstWrite -1}
		nb_5_ph {Type I LastRead 0 FirstWrite -1}
		ob_5_ph {Type I LastRead 0 FirstWrite -1}
		pending_0 {Type I LastRead 0 FirstWrite -1}
		out_r {Type O LastRead -1 FirstWrite 1}
		oi_5_out {Type IO LastRead 1 FirstWrite 0}
		nb_5_out {Type IO LastRead 1 FirstWrite 0}
		ob_5_out {Type IO LastRead 1 FirstWrite 0}}
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
	, {"Name" : "Interval", "Min" : "-1", "Max" : "-1"}
]}

set PipelineEnableSignalInfo {[
]}

set Spec2ImplPortList { 
	low_read_6 { ap_none {  { low_read_6 in_data 0 32 } } }
	low_read { ap_none {  { low_read in_data 0 32 } } }
	high_read_6 { ap_none {  { high_read_6 in_data 0 32 } } }
	high_read { ap_none {  { high_read in_data 0 32 } } }
	pending_read { ap_none {  { pending_read in_data 0 32 } } }
	ob_read { ap_none {  { ob_read in_data 0 32 } } }
	nb_read { ap_none {  { nb_read in_data 0 32 } } }
	out_r { ap_memory {  { out_r_address0 mem_address 1 10 }  { out_r_ce0 mem_ce 1 1 }  { out_r_we0 mem_we 1 1 }  { out_r_d0 mem_din 1 8 } } }
	oi_read { ap_none {  { oi_read in_data 0 32 } } }
	prob_read { ap_none {  { prob_read in_data 0 32 } } }
	bit_r { ap_none {  { bit_r in_data 0 1 } } }
}
