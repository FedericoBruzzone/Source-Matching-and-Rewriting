add_library(Optimization STATIC 
		Manager.cpp
)

target_link_libraries(Optimization
	PUBLIC
		${LLVM_LIBS}
		${MLIR_LIBS}
		Logger
)

target_include_directories(Optimization
	PUBLIC
		${CMAKE_SOURCE_DIR}/smr
		${CMAKE_CURRENT_SOURCE_DIR}
)