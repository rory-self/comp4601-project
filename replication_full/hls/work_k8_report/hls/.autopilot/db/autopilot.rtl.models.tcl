set SynModuleInfo {
  {SRCNAME arith_encode_Pipeline_VITIS_LOOP_66_1 MODELNAME arith_encode_Pipeline_VITIS_LOOP_66_1 RTLNAME arith_encode_arith_encode_Pipeline_VITIS_LOOP_66_1
    SUBMODULES {
      {MODELNAME arith_encode_flow_control_loop_pipe_sequential_init RTLNAME arith_encode_flow_control_loop_pipe_sequential_init BINDTYPE interface TYPE internal_upc_flow_control INSTNAME arith_encode_flow_control_loop_pipe_sequential_init_U}
    }
  }
  {SRCNAME encode_chunk_Pipeline_VITIS_LOOP_17_1 MODELNAME encode_chunk_Pipeline_VITIS_LOOP_17_1 RTLNAME arith_encode_encode_chunk_Pipeline_VITIS_LOOP_17_1}
  {SRCNAME encode_bit_Pipeline_VITIS_LOOP_53_2 MODELNAME encode_bit_Pipeline_VITIS_LOOP_53_2 RTLNAME arith_encode_encode_bit_Pipeline_VITIS_LOOP_53_2}
  {SRCNAME encode_bit_Pipeline_VITIS_LOOP_50_1 MODELNAME encode_bit_Pipeline_VITIS_LOOP_50_1 RTLNAME arith_encode_encode_bit_Pipeline_VITIS_LOOP_50_1}
  {SRCNAME encode_bit MODELNAME encode_bit RTLNAME arith_encode_encode_bit
    SUBMODULES {
      {MODELNAME arith_encode_mul_32s_32s_32_1_1 RTLNAME arith_encode_mul_32s_32s_32_1_1 BINDTYPE op TYPE mul IMPL auto LATENCY 0 ALLOW_PRAGMA 1}
    }
  }
  {SRCNAME encode_bit.1_Pipeline_VITIS_LOOP_53_2 MODELNAME encode_bit_1_Pipeline_VITIS_LOOP_53_2 RTLNAME arith_encode_encode_bit_1_Pipeline_VITIS_LOOP_53_2}
  {SRCNAME encode_bit.1_Pipeline_VITIS_LOOP_50_1 MODELNAME encode_bit_1_Pipeline_VITIS_LOOP_50_1 RTLNAME arith_encode_encode_bit_1_Pipeline_VITIS_LOOP_50_1}
  {SRCNAME encode_bit.1 MODELNAME encode_bit_1 RTLNAME arith_encode_encode_bit_1}
  {SRCNAME encode_chunk_Pipeline_VITIS_LOOP_33_5 MODELNAME encode_chunk_Pipeline_VITIS_LOOP_33_5 RTLNAME arith_encode_encode_chunk_Pipeline_VITIS_LOOP_33_5}
  {SRCNAME encode_chunk_Pipeline_VITIS_LOOP_32_4 MODELNAME encode_chunk_Pipeline_VITIS_LOOP_32_4 RTLNAME arith_encode_encode_chunk_Pipeline_VITIS_LOOP_32_4}
  {SRCNAME encode_chunk MODELNAME encode_chunk RTLNAME arith_encode_encode_chunk
    SUBMODULES {
      {MODELNAME arith_encode_encode_chunk_tree_RAM_AUTO_1R1W RTLNAME arith_encode_encode_chunk_tree_RAM_AUTO_1R1W BINDTYPE storage TYPE ram IMPL auto LATENCY 2 ALLOW_PRAGMA 1}
    }
  }
  {SRCNAME arith_encode_Pipeline_Header MODELNAME arith_encode_Pipeline_Header RTLNAME arith_encode_arith_encode_Pipeline_Header
    SUBMODULES {
      {MODELNAME arith_encode_sparsemux_17_3_16_1_1 RTLNAME arith_encode_sparsemux_17_3_16_1_1 BINDTYPE op TYPE sparsemux IMPL compactencoding_dontcare}
    }
  }
  {SRCNAME arith_encode_Pipeline_VITIS_LOOP_89_2 MODELNAME arith_encode_Pipeline_VITIS_LOOP_89_2 RTLNAME arith_encode_arith_encode_Pipeline_VITIS_LOOP_89_2
    SUBMODULES {
      {MODELNAME arith_encode_sparsemux_17_3_8_1_1 RTLNAME arith_encode_sparsemux_17_3_8_1_1 BINDTYPE op TYPE sparsemux IMPL compactencoding_dontcare}
    }
  }
  {SRCNAME arith_encode MODELNAME arith_encode RTLNAME arith_encode IS_TOP 1
    SUBMODULES {
      {MODELNAME arith_encode_mul_4ns_30s_33_1_1 RTLNAME arith_encode_mul_4ns_30s_33_1_1 BINDTYPE op TYPE mul IMPL auto LATENCY 0 ALLOW_PRAGMA 1}
      {MODELNAME arith_encode_sparsemux_17_3_32_1_1 RTLNAME arith_encode_sparsemux_17_3_32_1_1 BINDTYPE op TYPE sparsemux IMPL compactencoding_dontcare}
      {MODELNAME arith_encode_arith_encode_unsigned_char_const_int_unsigned_char_buf_RAM_AUTO_1R1W RTLNAME arith_encode_arith_encode_unsigned_char_const_int_unsigned_char_buf_RAM_AUTO_1R1W BINDTYPE storage TYPE ram IMPL auto LATENCY 2 ALLOW_PRAGMA 1}
      {MODELNAME arith_encode_gmem0_m_axi RTLNAME arith_encode_gmem0_m_axi BINDTYPE interface TYPE adapter IMPL m_axi}
      {MODELNAME arith_encode_gmem1_m_axi RTLNAME arith_encode_gmem1_m_axi BINDTYPE interface TYPE adapter IMPL m_axi}
      {MODELNAME arith_encode_control_s_axi RTLNAME arith_encode_control_s_axi BINDTYPE interface TYPE interface_s_axilite}
    }
  }
}
