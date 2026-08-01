set ModuleHierarchy {[{
"Name" : "arith_encode", "RefName" : "arith_encode","ID" : "0","Type" : "sequential",
"SubInsts" : [
	{"Name" : "grp_encode_chunk_fu_264", "RefName" : "encode_chunk","ID" : "1","Type" : "sequential",
		"SubInsts" : [
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_17_1_fu_319", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_17_1","ID" : "2","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_17_1","RefName" : "VITIS_LOOP_17_1","ID" : "3","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_33_5_fu_357", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_33_5","ID" : "4","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_33_5","RefName" : "VITIS_LOOP_33_5","ID" : "5","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_32_4_fu_373", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_32_4","ID" : "6","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_32_4","RefName" : "VITIS_LOOP_32_4","ID" : "7","Type" : "pipeline"},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_21_2","RefName" : "VITIS_LOOP_21_2","ID" : "8","Type" : "no",
		"SubInsts" : [
		{"Name" : "grp_encode_bit_fu_325", "RefName" : "encode_bit","ID" : "9","Type" : "sequential",
				"SubLoops" : [
				{"Name" : "Renorm","RefName" : "Renorm","ID" : "10","Type" : "no",
				"SubInsts" : [
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_53_2_fu_305", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_53_2","ID" : "11","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "12","Type" : "pipeline"},]},
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_50_1_fu_321", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_50_1","ID" : "13","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "14","Type" : "pipeline"},]},]},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_24_3","RefName" : "VITIS_LOOP_24_3","ID" : "15","Type" : "no",
			"SubInsts" : [
			{"Name" : "grp_encode_bit_1_fu_343", "RefName" : "encode_bit_1","ID" : "16","Type" : "sequential",
					"SubLoops" : [
					{"Name" : "Renorm","RefName" : "Renorm","ID" : "17","Type" : "no",
					"SubInsts" : [
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_53_2_fu_281", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_53_2","ID" : "18","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "19","Type" : "pipeline"},]},
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_50_1_fu_297", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_50_1","ID" : "20","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "21","Type" : "pipeline"},]},]},]},]},]},]},
	{"Name" : "grp_encode_chunk_fu_273", "RefName" : "encode_chunk","ID" : "22","Type" : "sequential",
		"SubInsts" : [
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_17_1_fu_319", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_17_1","ID" : "23","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_17_1","RefName" : "VITIS_LOOP_17_1","ID" : "24","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_33_5_fu_357", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_33_5","ID" : "25","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_33_5","RefName" : "VITIS_LOOP_33_5","ID" : "26","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_32_4_fu_373", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_32_4","ID" : "27","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_32_4","RefName" : "VITIS_LOOP_32_4","ID" : "28","Type" : "pipeline"},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_21_2","RefName" : "VITIS_LOOP_21_2","ID" : "29","Type" : "no",
		"SubInsts" : [
		{"Name" : "grp_encode_bit_fu_325", "RefName" : "encode_bit","ID" : "30","Type" : "sequential",
				"SubLoops" : [
				{"Name" : "Renorm","RefName" : "Renorm","ID" : "31","Type" : "no",
				"SubInsts" : [
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_53_2_fu_305", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_53_2","ID" : "32","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "33","Type" : "pipeline"},]},
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_50_1_fu_321", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_50_1","ID" : "34","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "35","Type" : "pipeline"},]},]},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_24_3","RefName" : "VITIS_LOOP_24_3","ID" : "36","Type" : "no",
			"SubInsts" : [
			{"Name" : "grp_encode_bit_1_fu_343", "RefName" : "encode_bit_1","ID" : "37","Type" : "sequential",
					"SubLoops" : [
					{"Name" : "Renorm","RefName" : "Renorm","ID" : "38","Type" : "no",
					"SubInsts" : [
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_53_2_fu_281", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_53_2","ID" : "39","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "40","Type" : "pipeline"},]},
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_50_1_fu_297", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_50_1","ID" : "41","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "42","Type" : "pipeline"},]},]},]},]},]},]},
	{"Name" : "grp_encode_chunk_fu_282", "RefName" : "encode_chunk","ID" : "43","Type" : "sequential",
		"SubInsts" : [
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_17_1_fu_319", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_17_1","ID" : "44","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_17_1","RefName" : "VITIS_LOOP_17_1","ID" : "45","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_33_5_fu_357", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_33_5","ID" : "46","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_33_5","RefName" : "VITIS_LOOP_33_5","ID" : "47","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_32_4_fu_373", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_32_4","ID" : "48","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_32_4","RefName" : "VITIS_LOOP_32_4","ID" : "49","Type" : "pipeline"},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_21_2","RefName" : "VITIS_LOOP_21_2","ID" : "50","Type" : "no",
		"SubInsts" : [
		{"Name" : "grp_encode_bit_fu_325", "RefName" : "encode_bit","ID" : "51","Type" : "sequential",
				"SubLoops" : [
				{"Name" : "Renorm","RefName" : "Renorm","ID" : "52","Type" : "no",
				"SubInsts" : [
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_53_2_fu_305", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_53_2","ID" : "53","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "54","Type" : "pipeline"},]},
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_50_1_fu_321", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_50_1","ID" : "55","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "56","Type" : "pipeline"},]},]},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_24_3","RefName" : "VITIS_LOOP_24_3","ID" : "57","Type" : "no",
			"SubInsts" : [
			{"Name" : "grp_encode_bit_1_fu_343", "RefName" : "encode_bit_1","ID" : "58","Type" : "sequential",
					"SubLoops" : [
					{"Name" : "Renorm","RefName" : "Renorm","ID" : "59","Type" : "no",
					"SubInsts" : [
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_53_2_fu_281", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_53_2","ID" : "60","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "61","Type" : "pipeline"},]},
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_50_1_fu_297", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_50_1","ID" : "62","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "63","Type" : "pipeline"},]},]},]},]},]},]},
	{"Name" : "grp_encode_chunk_fu_291", "RefName" : "encode_chunk","ID" : "64","Type" : "sequential",
		"SubInsts" : [
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_17_1_fu_319", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_17_1","ID" : "65","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_17_1","RefName" : "VITIS_LOOP_17_1","ID" : "66","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_33_5_fu_357", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_33_5","ID" : "67","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_33_5","RefName" : "VITIS_LOOP_33_5","ID" : "68","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_32_4_fu_373", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_32_4","ID" : "69","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_32_4","RefName" : "VITIS_LOOP_32_4","ID" : "70","Type" : "pipeline"},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_21_2","RefName" : "VITIS_LOOP_21_2","ID" : "71","Type" : "no",
		"SubInsts" : [
		{"Name" : "grp_encode_bit_fu_325", "RefName" : "encode_bit","ID" : "72","Type" : "sequential",
				"SubLoops" : [
				{"Name" : "Renorm","RefName" : "Renorm","ID" : "73","Type" : "no",
				"SubInsts" : [
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_53_2_fu_305", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_53_2","ID" : "74","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "75","Type" : "pipeline"},]},
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_50_1_fu_321", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_50_1","ID" : "76","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "77","Type" : "pipeline"},]},]},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_24_3","RefName" : "VITIS_LOOP_24_3","ID" : "78","Type" : "no",
			"SubInsts" : [
			{"Name" : "grp_encode_bit_1_fu_343", "RefName" : "encode_bit_1","ID" : "79","Type" : "sequential",
					"SubLoops" : [
					{"Name" : "Renorm","RefName" : "Renorm","ID" : "80","Type" : "no",
					"SubInsts" : [
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_53_2_fu_281", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_53_2","ID" : "81","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "82","Type" : "pipeline"},]},
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_50_1_fu_297", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_50_1","ID" : "83","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "84","Type" : "pipeline"},]},]},]},]},]},]},
	{"Name" : "grp_encode_chunk_fu_300", "RefName" : "encode_chunk","ID" : "85","Type" : "sequential",
		"SubInsts" : [
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_17_1_fu_319", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_17_1","ID" : "86","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_17_1","RefName" : "VITIS_LOOP_17_1","ID" : "87","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_33_5_fu_357", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_33_5","ID" : "88","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_33_5","RefName" : "VITIS_LOOP_33_5","ID" : "89","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_32_4_fu_373", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_32_4","ID" : "90","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_32_4","RefName" : "VITIS_LOOP_32_4","ID" : "91","Type" : "pipeline"},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_21_2","RefName" : "VITIS_LOOP_21_2","ID" : "92","Type" : "no",
		"SubInsts" : [
		{"Name" : "grp_encode_bit_fu_325", "RefName" : "encode_bit","ID" : "93","Type" : "sequential",
				"SubLoops" : [
				{"Name" : "Renorm","RefName" : "Renorm","ID" : "94","Type" : "no",
				"SubInsts" : [
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_53_2_fu_305", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_53_2","ID" : "95","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "96","Type" : "pipeline"},]},
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_50_1_fu_321", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_50_1","ID" : "97","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "98","Type" : "pipeline"},]},]},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_24_3","RefName" : "VITIS_LOOP_24_3","ID" : "99","Type" : "no",
			"SubInsts" : [
			{"Name" : "grp_encode_bit_1_fu_343", "RefName" : "encode_bit_1","ID" : "100","Type" : "sequential",
					"SubLoops" : [
					{"Name" : "Renorm","RefName" : "Renorm","ID" : "101","Type" : "no",
					"SubInsts" : [
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_53_2_fu_281", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_53_2","ID" : "102","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "103","Type" : "pipeline"},]},
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_50_1_fu_297", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_50_1","ID" : "104","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "105","Type" : "pipeline"},]},]},]},]},]},]},
	{"Name" : "grp_encode_chunk_fu_309", "RefName" : "encode_chunk","ID" : "106","Type" : "sequential",
		"SubInsts" : [
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_17_1_fu_319", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_17_1","ID" : "107","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_17_1","RefName" : "VITIS_LOOP_17_1","ID" : "108","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_33_5_fu_357", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_33_5","ID" : "109","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_33_5","RefName" : "VITIS_LOOP_33_5","ID" : "110","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_32_4_fu_373", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_32_4","ID" : "111","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_32_4","RefName" : "VITIS_LOOP_32_4","ID" : "112","Type" : "pipeline"},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_21_2","RefName" : "VITIS_LOOP_21_2","ID" : "113","Type" : "no",
		"SubInsts" : [
		{"Name" : "grp_encode_bit_fu_325", "RefName" : "encode_bit","ID" : "114","Type" : "sequential",
				"SubLoops" : [
				{"Name" : "Renorm","RefName" : "Renorm","ID" : "115","Type" : "no",
				"SubInsts" : [
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_53_2_fu_305", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_53_2","ID" : "116","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "117","Type" : "pipeline"},]},
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_50_1_fu_321", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_50_1","ID" : "118","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "119","Type" : "pipeline"},]},]},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_24_3","RefName" : "VITIS_LOOP_24_3","ID" : "120","Type" : "no",
			"SubInsts" : [
			{"Name" : "grp_encode_bit_1_fu_343", "RefName" : "encode_bit_1","ID" : "121","Type" : "sequential",
					"SubLoops" : [
					{"Name" : "Renorm","RefName" : "Renorm","ID" : "122","Type" : "no",
					"SubInsts" : [
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_53_2_fu_281", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_53_2","ID" : "123","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "124","Type" : "pipeline"},]},
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_50_1_fu_297", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_50_1","ID" : "125","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "126","Type" : "pipeline"},]},]},]},]},]},]},
	{"Name" : "grp_encode_chunk_fu_318", "RefName" : "encode_chunk","ID" : "127","Type" : "sequential",
		"SubInsts" : [
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_17_1_fu_319", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_17_1","ID" : "128","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_17_1","RefName" : "VITIS_LOOP_17_1","ID" : "129","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_33_5_fu_357", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_33_5","ID" : "130","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_33_5","RefName" : "VITIS_LOOP_33_5","ID" : "131","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_32_4_fu_373", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_32_4","ID" : "132","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_32_4","RefName" : "VITIS_LOOP_32_4","ID" : "133","Type" : "pipeline"},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_21_2","RefName" : "VITIS_LOOP_21_2","ID" : "134","Type" : "no",
		"SubInsts" : [
		{"Name" : "grp_encode_bit_fu_325", "RefName" : "encode_bit","ID" : "135","Type" : "sequential",
				"SubLoops" : [
				{"Name" : "Renorm","RefName" : "Renorm","ID" : "136","Type" : "no",
				"SubInsts" : [
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_53_2_fu_305", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_53_2","ID" : "137","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "138","Type" : "pipeline"},]},
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_50_1_fu_321", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_50_1","ID" : "139","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "140","Type" : "pipeline"},]},]},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_24_3","RefName" : "VITIS_LOOP_24_3","ID" : "141","Type" : "no",
			"SubInsts" : [
			{"Name" : "grp_encode_bit_1_fu_343", "RefName" : "encode_bit_1","ID" : "142","Type" : "sequential",
					"SubLoops" : [
					{"Name" : "Renorm","RefName" : "Renorm","ID" : "143","Type" : "no",
					"SubInsts" : [
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_53_2_fu_281", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_53_2","ID" : "144","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "145","Type" : "pipeline"},]},
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_50_1_fu_297", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_50_1","ID" : "146","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "147","Type" : "pipeline"},]},]},]},]},]},]},
	{"Name" : "grp_encode_chunk_fu_327", "RefName" : "encode_chunk","ID" : "148","Type" : "sequential",
		"SubInsts" : [
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_17_1_fu_319", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_17_1","ID" : "149","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_17_1","RefName" : "VITIS_LOOP_17_1","ID" : "150","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_33_5_fu_357", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_33_5","ID" : "151","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_33_5","RefName" : "VITIS_LOOP_33_5","ID" : "152","Type" : "pipeline"},]},
		{"Name" : "grp_encode_chunk_Pipeline_VITIS_LOOP_32_4_fu_373", "RefName" : "encode_chunk_Pipeline_VITIS_LOOP_32_4","ID" : "153","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_32_4","RefName" : "VITIS_LOOP_32_4","ID" : "154","Type" : "pipeline"},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_21_2","RefName" : "VITIS_LOOP_21_2","ID" : "155","Type" : "no",
		"SubInsts" : [
		{"Name" : "grp_encode_bit_fu_325", "RefName" : "encode_bit","ID" : "156","Type" : "sequential",
				"SubLoops" : [
				{"Name" : "Renorm","RefName" : "Renorm","ID" : "157","Type" : "no",
				"SubInsts" : [
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_53_2_fu_305", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_53_2","ID" : "158","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "159","Type" : "pipeline"},]},
				{"Name" : "grp_encode_bit_Pipeline_VITIS_LOOP_50_1_fu_321", "RefName" : "encode_bit_Pipeline_VITIS_LOOP_50_1","ID" : "160","Type" : "sequential",
						"SubLoops" : [
						{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "161","Type" : "pipeline"},]},]},]},],
		"SubLoops" : [
		{"Name" : "VITIS_LOOP_24_3","RefName" : "VITIS_LOOP_24_3","ID" : "162","Type" : "no",
			"SubInsts" : [
			{"Name" : "grp_encode_bit_1_fu_343", "RefName" : "encode_bit_1","ID" : "163","Type" : "sequential",
					"SubLoops" : [
					{"Name" : "Renorm","RefName" : "Renorm","ID" : "164","Type" : "no",
					"SubInsts" : [
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_53_2_fu_281", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_53_2","ID" : "165","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_53_2","RefName" : "VITIS_LOOP_53_2","ID" : "166","Type" : "pipeline"},]},
					{"Name" : "grp_encode_bit_1_Pipeline_VITIS_LOOP_50_1_fu_297", "RefName" : "encode_bit_1_Pipeline_VITIS_LOOP_50_1","ID" : "167","Type" : "sequential",
							"SubLoops" : [
							{"Name" : "VITIS_LOOP_50_1","RefName" : "VITIS_LOOP_50_1","ID" : "168","Type" : "pipeline"},]},]},]},]},]},]},
	{"Name" : "grp_arith_encode_Pipeline_Header_fu_361", "RefName" : "arith_encode_Pipeline_Header","ID" : "169","Type" : "sequential",
		"SubLoops" : [
		{"Name" : "Header","RefName" : "Header","ID" : "170","Type" : "pipeline"},]},],
"SubLoops" : [
	{"Name" : "Split","RefName" : "Split","ID" : "171","Type" : "no",
	"SubInsts" : [
	{"Name" : "grp_arith_encode_Pipeline_VITIS_LOOP_66_1_fu_336", "RefName" : "arith_encode_Pipeline_VITIS_LOOP_66_1","ID" : "172","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_66_1","RefName" : "VITIS_LOOP_66_1","ID" : "173","Type" : "pipeline"},]},]},
	{"Name" : "Concat","RefName" : "Concat","ID" : "174","Type" : "no",
	"SubInsts" : [
	{"Name" : "grp_arith_encode_Pipeline_VITIS_LOOP_89_2_fu_374", "RefName" : "arith_encode_Pipeline_VITIS_LOOP_89_2","ID" : "175","Type" : "sequential",
			"SubLoops" : [
			{"Name" : "VITIS_LOOP_89_2","RefName" : "VITIS_LOOP_89_2","ID" : "176","Type" : "pipeline"},]},]},]
}]}