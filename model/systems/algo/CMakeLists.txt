# OBJECT 库：对象经 model/CMakeLists.txt 的 PreCessBase 聚合导出（依赖方向仍由此处强制）
add_library(AlgorithmSystem OBJECT AlgorithmSystem.cpp AlgorithmSystemRegister.cpp)
target_link_libraries(AlgorithmSystem PRIVATE 
	Data 
	IOSystem # 只使用头文件；OBJECT 库间仅传递用法需求不并入对象，等价原 $<COMPILE_ONLY:> 语义
	Qt6::Core spdlog::spdlog
)
target_link_libraries(AlgorithmSystem PUBLIC
    SystemCore Core    
)
target_include_directories(AlgorithmSystem PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}   # 当前文件所在路径
)