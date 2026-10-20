add_library(Optimization STATIC 
		Manager.cpp
		Passes.cpp
		Transforms/DCE.cpp
		Transforms/Inline.cpp
		Transforms/Tiling.cpp
		Transforms/Unroll.cpp
)

target_link_libraries(Optimization
	PUBLIC
		${LLVM_LIBS}
		${MLIR_LIBS}
		${CLANG_LIBS}
		MLIRPass
		Logger
)

target_include_directories(Optimization
	PUBLIC
		${CMAKE_SOURCE_DIR}/smr
		${CMAKE_CURRENT_SOURCE_DIR}
)
