cmake_minimum_required(VERSION 3.28)

# ================================
# 工程: GenUI(通用布局编辑器, 基于 VideoCut 框架裁剪, 无 FFMPEG)
# 描述: 主布局 = 预览画布; child 布局 = 编辑器
#       编辑器支持向预览布局添加控件/改位置尺寸/打开 JSON(保存方向尚未实现)
# 构建(经根目录 IPCHost 通道; 旧 IPCBridge 已于 2026-10-09 退役):
#   cmake   -S GenUI -B GenUI/out -G "Visual Studio 18 2026" -A x64
#   msbuild GenUI/out/GenUI.slnx -p:Configuration=Release -p:Platform=x64 -m
# ================================
# RC: 需要资源编译(.rc)以把布局文档内置进 exe —— 加载链第 3/4 级(设计口径 docs\HANDOFF.md §0.2-1)
project(GenUI LANGUAGES CXX RC)

if(MSVC)
	# 源码注释为 UTF-8(无 BOM), 需 /utf-8 以免按代码页 936 误读中文
	# /Zc:char8_t-: 沿用工程惯例 u8"..." 作为 char*(兼容既有 LPCSTR 传参)
	add_compile_options(/utf-8 /Zc:char8_t-)
endif()

# 静态运行库 /MT(/MTd), 自包含
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 内置文档资源(加载链第 3/4 级 `resource.res\*`): 由 GenUI.json 生成资源脚本, 随 exe 内置。
# 生成期写入源指纹 ⇒ GenUI.json 一变, 生成的 .rc 内容即变 ⇒ 资源必然重编(不会保留旧副本);
# 运行期自检另有"内置资源 == 磁盘副本"逐字节断言(见 GenEditor.cpp 的 0n)。
#
# 第 3 级 = bin 形态: 构建期由 tools\json2bin.py 把工程源转成 GenUI.doc.bin 再内嵌。
# 说明: ① 格式真源仍是 C++ 的 LayoutWriter(写方向), 本脚本只是"构建期引导";
#         其产物由自检断言"内置 bin 资源 == 由当前中间态生成的 bin 字节"逐字节守卫;
#       ② 找不到 Python ⇒ 只嵌第 4 级 JSON(警告, 不阻断构建)。
set(GENUI_DOC_BIN "${CMAKE_CURRENT_BINARY_DIR}/GenUI.doc.bin")
find_program(GENUI_PYTHON NAMES python python3)
if(GENUI_PYTHON)
	add_custom_command(
		OUTPUT "${GENUI_DOC_BIN}"
		COMMAND "${GENUI_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/json2bin.py"
			"${CMAKE_CURRENT_SOURCE_DIR}/GenUI.json" "${GENUI_DOC_BIN}"
		DEPENDS
			"${CMAKE_CURRENT_SOURCE_DIR}/GenUI.json"
			"${CMAKE_CURRENT_SOURCE_DIR}/tools/json2bin.py"
		COMMENT "生成内置 bin 文档(GenUI.doc.bin)"
		VERBATIM
	)
else()
	message(WARNING "未找到 Python ⇒ 不内嵌 bin 文档资源(加载链第 3 级将未命中, 仅第 4 级 JSON 兜底)")
	set(GENUI_DOC_BIN "")
endif()

set(GENUI_DOC_RES_RC "${CMAKE_CURRENT_BINARY_DIR}/GenUI.docres.rc")
set(GENUI_DOC_RES_DEPS
	"${CMAKE_CURRENT_SOURCE_DIR}/GenUI.json"
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen-doc-res.cmake"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/app/resource.h"
)
if(GENUI_DOC_BIN)
	list(APPEND GENUI_DOC_RES_DEPS "${GENUI_DOC_BIN}")
endif()
add_custom_command(
	OUTPUT "${GENUI_DOC_RES_RC}"
	COMMAND ${CMAKE_COMMAND}
		"-DGENUI_DOC=${CMAKE_CURRENT_SOURCE_DIR}/GenUI.json"
		"-DGENUI_DOC_BIN=${GENUI_DOC_BIN}"
		"-DGENUI_RES_H=${CMAKE_CURRENT_SOURCE_DIR}/src/app/resource.h"
		"-DGENUI_RC=${GENUI_DOC_RES_RC}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen-doc-res.cmake"
	DEPENDS ${GENUI_DOC_RES_DEPS}
	COMMENT "生成内置文档资源脚本(GenUI.docres.rc)"
	VERBATIM
)

# ================================
# 源码清单(按层分组)
#   framework\base           : 基础设计量 / 控件类型契约 / 控件缓冲(数据结构 · 容量 · 取址 · 装配与回收) / 函数指针 / 文本宽度
#                              / 平台环境件(AppEnv; 2026-10-23 自 common 归位)
#   framework\base\mem      : 内存设施(MemoryManager 分配器 + AllocatorHub 会话分配器; 2026-10-23 自 common 归位)
#   framework\base\util     : 与框架语义无关的纯算法件(FileCodec 校验; 2026-10-23 自 common 归位)
#   framework\render         : 渲染能力接口(Render.h)与具体渲染技术的实现(按技术名分目录)
#   framework\render\imgui         : ImGui 宿主与图形后端设施(窗口 / 上下文 / 帧流程 / 消息循环 / D3D11)
#   framework\render\imgui\ability : ImGui 侧渲染能力提供者(能力表的唯一填表处)
#   framework\render\imgui\control : 控件库与部件层(逐控件绘制)
#   framework\layout         : 布局子系统(布局对象 / 中间态模型 / 构建原语 / 读方向解算 / 渲染出口 / 几何解算)
#   framework\dispatch       : 调度层(帧间事件 / 事件绑定表 / 调度与场景实现切片 / 资源与路径解析)
#   app / doc / editor       : 应用层(多布局文档 + 编辑器 + 预览与会话)
# 说明: framework\dispatch 的 Dispatch / DispatchScene 随框架就位(与 Kanat 快照一致), 但本工程的宿主
#       生命周期仍由 app\AppHost 驱动(多文档与编辑器协调面不走单文档调度层); 该两件不参与运行时接线。
# 说明: 原 src\common 层已于 2026-10-23 取消(五件全部归位 framework; 与 Kanat 母本同构),
#       应用层不再有 common 目录。
# ================================
set(GENUI_FRAMEWORK_SRC
	src/framework/base/AppEnv.cpp
	src/framework/base/BasicDesign.cpp
	src/framework/base/ControlSpec.cpp
	src/framework/base/CtrlBuffer.cpp
	src/framework/base/TextWidth.cpp
	src/framework/base/mem/AllocatorHub.cpp
	src/framework/base/mem/MemoryManager.cpp
	src/framework/base/util/FileCodec.cpp

	src/framework/render/imgui/D3D11Manager.cpp
	src/framework/render/imgui/ImGuiHost.cpp
	src/framework/render/imgui/ability/ImGuiRender.cpp
	src/framework/render/imgui/control/Controls.cpp
	src/framework/render/imgui/control/Widgets.cpp

	src/framework/layout/GeometrySolver.cpp
	src/framework/layout/Layout.cpp
	src/framework/layout/LayoutBuilder.cpp
	src/framework/layout/LayoutModel.cpp
	src/framework/layout/LayoutRender.cpp
	src/framework/layout/LayoutSolver.cpp

	src/framework/dispatch/Dispatch.cpp
	src/framework/dispatch/DispatchScene.cpp
	src/framework/dispatch/FrameEvent.cpp
	src/framework/dispatch/ResourceLoader.cpp
	src/framework/dispatch/UIEventHandler.cpp
)

set(GENUI_APP_SRC
	src/main.cpp
	src/app/AppHost.cpp
	src/app/Application.cpp
	src/app/HostContext.cpp
	src/app/DocumentSource.cpp
	src/app/DocumentIO.cpp
	src/app/FlexHost.cpp
	src/app/PreviewSession.cpp
	src/doc/DocConsistency.cpp
	src/doc/Document.cpp
	src/doc/RawAttributes.cpp
	src/editor/LayoutWriter.cpp
	src/editor/GenEditor.cpp
	src/editor/EditorCommands.cpp
	src/editor/SelectionOverlay.cpp
)

add_executable(GenUI
	${GENUI_APP_SRC}
	${GENUI_FRAMEWORK_SRC}
	"${GENUI_DOC_RES_RC}"
)

# 唯一包含根 = src\(源码内一律以"子目录/文件"形式引用, 见各文件 include 区)
target_include_directories(GenUI PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Windows SDK 库; imgui/rapidjson 头与 imgui 库由 NuGet 包 GenUI.Deps.Native 注入
target_link_libraries(GenUI PRIVATE d3d11 dxgi dwmapi imm32 comdlg32)

# 平台宏: x64 架构 + Unicode(与 windows.h 窄宽 API 语义一致)
target_compile_definitions(GenUI PRIVATE _AMD64_ UNICODE _UNICODE)

# 运行时把布局文档投放到 exe 同目录(加载链第 1 级 .\GenUI.bin)。
# 优先投放构建期生成的 bin(与内嵌第 3 级同一份产物); 找不到 Python ⇒ 退回复制工程源 JSON(第 2 级)。
if(GENUI_DOC_BIN)
	add_custom_command(TARGET GenUI POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${GENUI_DOC_BIN}"
			"$<TARGET_FILE_DIR:GenUI>/GenUI.bin"
	)
else()
	add_custom_command(TARGET GenUI POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/GenUI.json"
			"$<TARGET_FILE_DIR:GenUI>/GenUI.json"
	)
endif()
