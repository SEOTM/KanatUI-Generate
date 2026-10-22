#pragma once
#include <minwindef.h>
#include <stddef.h>
#include "framework/layout/LayoutModel.h"

// ================================
// 区域: bin 文件格式契约的来源
// 说明: 格式契约(文件头 / 段头 / 头部区 / 控件记录、版本号位域、规模上限与段大小公式 BinSegBytes)
//       统一声明在 src\framework\LayoutModel.h 的"bin 文件格式契约"区(与结构体布局同源) ⇒ 本模块按该契约
//       写出字节流; 校验码算法取 FileCodec::Crc32。
// ================================
// ================================
// 类名: LayoutWriter
// 描述: 布局写入器 —— 把中间态(布局数据模型)生成规范 JSON 文本或 bin 字节流, 并写盘
// 说明: JSON 规范排版 = 2 空格缩进 / LF 换行 / 末尾换行 / 无 BOM / 因子对象单行 /
//       整值不带小数点(其余取最短往返十进制) / 字符串按 UTF-8 字节原样(中文不转义);
//       写出的键序固定(布局: name label IsMaxiable IsResizeable wndX wndY width height autosize ctrls;
//       控件: id parent type label text event x y width height), 与工程源 GenUI.json 逐字节一致。
// ================================
class LayoutWriter
{
private:
	// 私有化构造/析构(纯静态工具类)
	LayoutWriter() = delete;
	~LayoutWriter() = delete;
public:
	// 函数: BuildJsonText
	// 描述: 把整份中间态生成规范 JSON 文本(两趟: 先计数再分配写入; 所有权交调用方)
	// 参数:
	//		IN	state:		中间态(布局链 + 元素链)
	//		OUT	ppszText:	文本首址(调用方负责 delete[]; 失败置 NULL)
	//		OUT	pcbText:	文本字节数(含末尾换行)
	// 返回:
	//		BOOL: TRUE = 已生成; FALSE = 入参非法 / 分配失败 / 写出越界
	static BOOL BuildJsonText(CONST FileMidState& state, LPSTR* ppszText, size_t* pcbText);

	// 函数: SaveToFile
	// 描述: 生成规范 JSON 文本并按"备份 + 临时文件 + 原子替换"写入指定路径
	// 说明: ① 目标已存在且 <目标>.bak 不存在 ⇒ 先备份(不覆盖既有备份; 备份失败不阻断写入, 只打印诊断);
	//       ② 正文写同目录 <目标>.tmp, FlushFileBuffers 后 MoveFileEx(REPLACE_EXISTING) 原子替换;
	//       ③ 任一步失败 ⇒ 删除残留 .tmp, 打印 [APP] 诊断并返回 FALSE(原文件保持不变)。
	// 参数:
	//		IN	state:	中间态(布局链 + 元素链)
	//		IN	szPath:	目标文件路径
	// 返回:
	//		BOOL: TRUE = 已写入; FALSE = 路径非法 / 文本生成失败 / 备份或写盘或替换失败
	static BOOL SaveToFile(CONST FileMidState& state, LPCSTR szPath);

	// 函数: BuildBinBytes
	// 描述: 把整份中间态生成 bin 字节流(两趟: 先计数再分配写入; 所有权交调用方)
	// 说明: 记录区整块 memcpy 自中间态(LayoutInfo / ElementSpec), 不对字段逐个编码;
	//       末尾回填 CRC32(校验码)。
	// 参数:
	//		IN	state:		中间态(布局链 + 元素链)
	//		OUT	ppBytes:	字节流首址(调用方负责 delete[]; 失败置 NULL)
	//		OUT	pcbBytes:	字节数
	// 返回:
	//		BOOL: TRUE = 已生成(校验码已写入文件头首 4 字节);
	//		      FALSE = 入参非法 / 段数或控件数超上限 / 分配失败
	static BOOL BuildBinBytes(CONST FileMidState& state, LPBYTE* ppBytes, size_t* pcbBytes);

	// 函数: SaveBinToFile
	// 描述: 生成 bin 字节流并按"备份 + 临时文件 + 原子替换"写入指定路径(写盘骨架与 JSON 侧共用)
	// 参数:
	//		IN	state:	中间态(布局链 + 元素链)
	//		IN	szPath:	目标文件路径
	// 返回:
	//		BOOL: TRUE = 已写入; FALSE = 生成失败 / 路径非法 / 备份或写盘或替换失败
	static BOOL SaveBinToFile(CONST FileMidState& state, LPCSTR szPath);

	// 函数: SaveRawToFile
	// 描述: 把一段内存字节按"备份 + 临时文件 + 原子替换"写入指定路径(JSON 与 bin 两条写盘链共用)
	// 说明: 写盘口径与 SaveToFile 完全一致(见上), 只是正文由调用方给出(不再自行生成文本)。
	// 参数:
	//		IN	pData:	正文字节首址
	//		IN	cbData: 正文字节数
	//		IN	szPath:	目标文件路径
	// 返回:
	//		BOOL: TRUE = 已写入; FALSE = 入参非法 / 路径过长 / 备份或写盘或替换失败
	static BOOL SaveRawToFile(CONST BYTE* pData, size_t cbData, LPCSTR szPath);
};
