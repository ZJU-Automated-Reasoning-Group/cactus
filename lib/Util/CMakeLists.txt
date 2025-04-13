set (UtilCodes
	CommandLine/CommandLineParser.cpp
	CommandLine/TypedCommandLineParser.cpp
	IO/ReadIR.cpp
	IO/WriteIR.cpp
	SparrowUtils/AnalysisUtil.cpp
	SparrowUtils/CPPUtil.cpp
	SparrowUtils/Common.cpp
	SparrowUtils/ExtAPI.cpp
	SparrowUtils/Profiler.cpp
	SparrowUtils/ProgressBar.cpp
)

add_library (Util STATIC ${UtilCodes})
target_link_libraries (Util LLVMIRReader LLVMAsmParser LLVMBitWriter LLVMSupport LLVMCore)