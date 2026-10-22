# ================================
# 脚本: gen-doc-res.cmake
# 说明: 生成"内置文档资源脚本"(GenUI.docres.rc) —— 加载链第 3/4 级: 文档随 exe 内置。
#       由 CMakeLists.txt 的 add_custom_command 调用; 依赖 GenUI.json(第 4 级 JSON)、可选的
#       生成期 bin 文档(第 3 级 bin)与 resource.h, 并把源文件指纹(sha256/字节数)写进注释
#       ⇒ 源一变, 生成的 .rc 内容即变 ⇒ rc 重编(不会留旧副本)。
# 用法: cmake -DGENUI_DOC=<JSON 绝对路径> [-DGENUI_DOC_BIN=<bin 绝对路径>]
#              -DGENUI_RES_H=<resource.h 绝对路径> -DGENUI_RC=<输出 .rc 绝对路径> -P gen-doc-res.cmake
# 注意: 路径一律转成正斜杠形式写入 .rc —— rc.exe 的字符串里反斜杠是转义字符, 正斜杠可避免歧义。
# ================================

foreach(v GENUI_DOC GENUI_RES_H GENUI_RC)
	if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
		message(FATAL_ERROR "缺少必需变量 ${v}")
	endif()
endforeach()

file(TO_CMAKE_PATH "${GENUI_DOC}"   DOC_FWD)
file(TO_CMAKE_PATH "${GENUI_RES_H}" RESH_FWD)
file(SHA256 "${GENUI_DOC}" DOC_SHA)
file(SIZE   "${GENUI_DOC}" DOC_SIZE)

# 第 3 级 bin 资源(可选): 由构建期 tools\json2bin.py 生成; 未提供(如找不到 Python)时只嵌 JSON
if(DEFINED GENUI_DOC_BIN AND NOT "${GENUI_DOC_BIN}" STREQUAL "")
	file(TO_CMAKE_PATH "${GENUI_DOC_BIN}" DOCBIN_FWD)
	file(SHA256 "${GENUI_DOC_BIN}" BIN_SHA)
	file(SIZE   "${GENUI_DOC_BIN}" BIN_SIZE)
	set(BIN_INFO "// bin 源: ${DOCBIN_FWD}
// bin 指纹: sha256=${BIN_SHA} size=${BIN_SIZE}")
	set(BIN_LINE "IDR_GENUI_BIN BIN \"${DOCBIN_FWD}\"")
	message(STATUS "内置 bin 文档: ${GENUI_DOC_BIN} (${BIN_SIZE} 字节, sha256 ${BIN_SHA})")
else()
	set(BIN_INFO "// bin 源: (未提供 ⇒ 不内嵌第 3 级资源)")
	set(BIN_LINE "")
endif()

file(WRITE "${GENUI_RC}"
"// 本文件由 cmake/gen-doc-res.cmake 生成, 请勿手改(改 GenUI.json 后构建时自动重生成)。
// 作用: 把布局文档作为自定义资源编译进 exe = 加载链第 3/4 级(`resource.res\\*`)。
// JSON 源文件: ${DOC_FWD}
// JSON 源指纹: sha256=${DOC_SHA} size=${DOC_SIZE}
${BIN_INFO}
#pragma code_page(65001)
#include \"${RESH_FWD}\"

IDR_GENUI_JSON JSON \"${DOC_FWD}\"
${BIN_LINE}
")

message(STATUS "生成内置文档资源脚本: ${GENUI_RC} (源 ${DOC_SIZE} 字节, sha256 ${DOC_SHA})")

