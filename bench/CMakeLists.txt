cmake_minimum_required(VERSION 3.28)

# ================================
# 工装: MemBench(MemoryManager 基准与契约测试)
# 描述: 独立控制台工装, 直接编译 framework\base\mem\MemoryManager.cpp; 不依赖 vcpkg / NuGet,
#       不参与 GenUI 主产物构建(与主工程解耦 ⇒ 加基准不改主工程的生成期流程)
# 构建(经根目录 IPCHost 通道; 旧 IPCBridge 已于 2026-10-09 退役):
#   cmake   -S GenUI\bench -B GenUI\bench\out -G "Visual Studio 18 2026" -A x64
#   msbuild GenUI\bench\out\MemBench.slnx -p:Configuration=Release -p:Platform=x64 -m
# 运行(报告输出路径由调用方指定; 原示例指向的 docs\archive\recon-memman\ 已于 2026-10-14 清理):
#   GenUI\bench\out\Release\MemBench.exe <报告输出路径>.md
# ================================
project(MemBench LANGUAGES CXX)

if(MSVC)
	# 与主工程同口径: 源码注释为 UTF-8(无 BOM); 沿用 u8"..." 作为 char*
	add_compile_options(/utf-8 /Zc:char8_t-)
endif()

# 静态运行库 /MT(/MTd), 与主工程一致(自包含)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MemBench
	MemBench.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../src/framework/base/mem/MemoryManager.cpp
)

# 唯一包含根 = 主工程 src\(MemBench.cpp 与 MemoryManager.cpp 均以"子目录/文件"形式引用)
target_include_directories(MemBench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")

# 平台宏: x64 架构 + Unicode(与主工程同口径)
target_compile_definitions(MemBench PRIVATE _AMD64_ UNICODE _UNICODE)
