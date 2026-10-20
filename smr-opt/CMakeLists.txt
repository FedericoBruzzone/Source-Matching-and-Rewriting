add_executable(smr-opt smr-opt.cpp)

target_link_libraries(smr-opt
	PUBLIC
	${LLVM_LIBS}
	${MLIR_LIBS}
	${CLANG_LIBS}
	MLIRDLTIDialect
	MLIRLLVMDialect
	MLIROptLib
	Optimization
)
