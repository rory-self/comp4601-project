set moduleName encode_chunk
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
set C_modelName {encode_chunk}
set C_modelType { int 32 }
set ap_memory_interface_dict [dict create]
dict set ap_memory_interface_dict in_r { MEM_WIDTH 8 MEM_SIZE 576 MASTER_TYPE BRAM_CTRL MEM_ADDRESS_MODE WORD_ADDRESS PACKAGE_IO port READ_LATENCY 1 }
dict set ap_memory_interface_dict out_r { MEM_WIDTH 8 MEM_SIZE 576 MASTER_TYPE BRAM_CTRL MEM_ADDRESS_MODE WORD_ADDRESS PACKAGE_IO port READ_LATENCY 0 }
set C_modelArgList {
	{ in_r int 8 regular {array 576 { 1 3 } 1 1 }  }
	{ n int 32 regular  }
	{ out_r int 8 regular {array 576 { 0 3 } 0 1 }  }
}
set hasAXIMCache 0
set l_AXIML2Cache [list]
set AXIMCacheInstDict [dict create]
set C_modelArgMapList {[ 
	{ "Name" : "in_r", "interface" : "memory", "bitwidth" : 8, "direction" : "READONLY"} , 
 	{ "Name" : "n", "interface" : "wire", "bitwidth" : 32, "direction" : "READONLY"} , 
 	{ "Name" : "out_r", "interface" : "memory", "bitwidth" : 8, "direction" : "WRITEONLY"} , 
 	{ "Name" : "ap_return", "interface" : "wire", "bitwidth" : 32} ]}
# RTL Port declarations: 
set portNum 15
set portList { 
	{ ap_clk sc_in sc_logic 1 clock -1 } 
	{ ap_rst sc_in sc_logic 1 reset -1 active_high_sync } 
	{ ap_start sc_in sc_logic 1 start -1 } 
	{ ap_done sc_out sc_logic 1 predone -1 } 
	{ ap_idle sc_out sc_logic 1 done -1 } 
	{ ap_ready sc_out sc_logic 1 ready -1 } 
	{ in_r_address0 sc_out sc_lv 10 signal 0 } 
	{ in_r_ce0 sc_out sc_logic 1 signal 0 } 
	{ in_r_q0 sc_in sc_lv 8 signal 0 } 
	{ n sc_in sc_lv 32 signal 1 } 
	{ out_r_address0 sc_out sc_lv 10 signal 2 } 
	{ out_r_ce0 sc_out sc_logic 1 signal 2 } 
	{ out_r_we0 sc_out sc_logic 1 signal 2 } 
	{ out_r_d0 sc_out sc_lv 8 signal 2 } 
	{ ap_return sc_out sc_lv 32 signal -1 } 
}
set NewPortList {[ 
	{ "name": "ap_clk", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "clock", "bundle":{"name": "ap_clk", "role": "default" }} , 
 	{ "name": "ap_rst", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "reset", "bundle":{"name": "ap_rst", "role": "default" }} , 
 	{ "name": "ap_start", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "start", "bundle":{"name": "ap_start", "role": "default" }} , 
 	{ "name": "ap_done", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "predone", "bundle":{"name": "ap_done", "role": "default" }} , 
 	{ "name": "ap_idle", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "done", "bundle":{"name": "ap_idle", "role": "default" }} , 
 	{ "name": "ap_ready", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "ready", "bundle":{"name": "ap_ready", "role": "default" }} , 
 	{ "name": "in_r_address0", "direction": "out", "datatype": "sc_lv", "bitwidth":10, "type": "signal", "bundle":{"name": "in_r", "role": "address0" }} , 
 	{ "name": "in_r_ce0", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "in_r", "role": "ce0" }} , 
 	{ "name": "in_r_q0", "direction": "in", "datatype": "sc_lv", "bitwidth":8, "type": "signal", "bundle":{"name": "in_r", "role": "q0" }} , 
 	{ "name": "n", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "n", "role": "default" }} , 
 	{ "name": "out_r_address0", "direction": "out", "datatype": "sc_lv", "bitwidth":10, "type": "signal", "bundle":{"name": "out_r", "role": "address0" }} , 
 	{ "name": "out_r_ce0", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "out_r", "role": "ce0" }} , 
 	{ "name": "out_r_we0", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "out_r", "role": "we0" }} , 
 	{ "name": "out_r_d0", "direction": "out", "datatype": "sc_lv", "bitwidth":8, "type": "signal", "bundle":{"name": "out_r", "role": "d0" }} , 
 	{ "name": "ap_return", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ap_return", "role": "default" }}  ]}

set ArgLastReadFirstWriteLatency {
	encode_chunk {
		in_r {Type I LastRead 3 FirstWrite -1}
		n {Type I LastRead 1 FirstWrite -1}
		out_r {Type O LastRead -1 FirstWrite 1}}
	encode_chunk_Pipeline_VITIS_LOOP_17_1 {
		tree {Type O LastRead -1 FirstWrite 0}}
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
		ob_2_out {Type IO LastRead 1 FirstWrite 0}}
	encode_bit_1 {
		low_read {Type I LastRead 0 FirstWrite -1}
		high_read {Type I LastRead 0 FirstWrite -1}
		pending_read {Type I LastRead 0 FirstWrite -1}
		ob_read {Type I LastRead 0 FirstWrite -1}
		nb_read {Type I LastRead 0 FirstWrite -1}
		out_r {Type O LastRead -1 FirstWrite 1}
		oi_read {Type I LastRead 0 FirstWrite -1}
		prob_read {Type I LastRead 1 FirstWrite -1}
		bit_r {Type I LastRead 2 FirstWrite -1}}
	encode_bit_1_Pipeline_VITIS_LOOP_53_2 {
		oi_5_ph {Type I LastRead 0 FirstWrite -1}
		nb_5_ph {Type I LastRead 0 FirstWrite -1}
		ob_5_ph {Type I LastRead 0 FirstWrite -1}
		pending_0 {Type I LastRead 0 FirstWrite -1}
		out_r {Type O LastRead -1 FirstWrite 1}
		oi_5_out {Type IO LastRead 1 FirstWrite 0}
		nb_5_out {Type IO LastRead 1 FirstWrite 0}
		ob_5_out {Type IO LastRead 1 FirstWrite 0}}
	encode_bit_1_Pipeline_VITIS_LOOP_50_1 {
		oi_2_ph {Type I LastRead 0 FirstWrite -1}
		nb_2_ph {Type I LastRead 0 FirstWrite -1}
		ob_2_ph {Type I LastRead 0 FirstWrite -1}
		pending_0 {Type I LastRead 0 FirstWrite -1}
		out_r {Type O LastRead -1 FirstWrite 1}
		oi_2_out {Type IO LastRead 1 FirstWrite 0}
		nb_2_out {Type IO LastRead 1 FirstWrite 0}
		ob_2_out {Type IO LastRead 1 FirstWrite 0}}
	encode_chunk_Pipeline_VITIS_LOOP_33_5 {
		oi_453_ph {Type I LastRead 0 FirstWrite -1}
		nb_6_4_ph {Type I LastRead 0 FirstWrite -1}
		ob_8_4_ph {Type I LastRead 0 FirstWrite -1}
		pending_2 {Type I LastRead 0 FirstWrite -1}
		out_r {Type O LastRead -1 FirstWrite 1}
		oi_453_out {Type O LastRead -1 FirstWrite 0}
		nb_6_4_out {Type O LastRead -1 FirstWrite 0}
		ob_8_4_out {Type O LastRead -1 FirstWrite 0}}
	encode_chunk_Pipeline_VITIS_LOOP_32_4 {
		oi_251_ph {Type I LastRead 0 FirstWrite -1}
		nb_6_2_ph {Type I LastRead 0 FirstWrite -1}
		ob_8_2_ph {Type I LastRead 0 FirstWrite -1}
		pending_2 {Type I LastRead 0 FirstWrite -1}
		out_r {Type O LastRead -1 FirstWrite 1}
		oi_251_out {Type O LastRead -1 FirstWrite 0}
		nb_6_2_out {Type O LastRead -1 FirstWrite 0}
		ob_8_2_out {Type O LastRead -1 FirstWrite 0}}}

set hasDtUnsupportedChannel 0

set PerformanceInfo {[
	{"Name" : "Latency", "Min" : "-1", "Max" : "-1"}
	, {"Name" : "Interval", "Min" : "-1", "Max" : "-1"}
]}

set PipelineEnableSignalInfo {[
]}

set Spec2ImplPortList { 
	in_r { ap_memory {  { in_r_address0 mem_address 1 10 }  { in_r_ce0 mem_ce 1 1 }  { in_r_q0 mem_dout 0 8 } } }
	n { ap_none {  { n in_data 0 32 } } }
	out_r { ap_memory {  { out_r_address0 mem_address 1 10 }  { out_r_ce0 mem_ce 1 1 }  { out_r_we0 mem_we 1 1 }  { out_r_d0 mem_din 1 8 } } }
}
