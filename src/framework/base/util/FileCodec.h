#pragma once
#include <minwindef.h>
#include <stddef.h>

// ================================
// 类名: FileCodec
// 描述: 文件处理 —— 对"一段字节"做纯校验 / 生成摘要(现为 CRC32), 以及后续的压缩与加密
// 说明: ① 本模块不感知文档结构(不引用 LayoutModel), 只处理字节;
//       ② 校验码的字段位置与覆盖范围属文件格式契约(见 src\framework\layout\LayoutModel.h 的格式契约区),
//          本模块只提供算法 ⇒ 由调用方按契约决定对哪些字节、以何顺序调用;
//       ③ 版本号低 8 位标记的压缩 / 加密尚未实现, 实现时在本模块内新增(不占用其它模块)。
// ================================
class FileCodec
{
private:
	// 私有化构造/析构(纯静态工具类)
	FileCodec() = delete;
	~FileCodec() = delete;
public:
	// 函数: Crc32Begin
	// 描述: 取 CRC32 的初始状态(流式计算入口)
	static DWORD Crc32Begin();

	// 函数: Crc32Update
	// 描述: 用一段字节推进 CRC32 状态(可连续多次调用, 以覆盖不连续的字节区间)
	static DWORD Crc32Update(DWORD dwCrc, LPCVOID data, size_t cbData);

	// 函数: Crc32Finish
	// 描述: 取 CRC32 的最终值(对状态取反)
	static DWORD Crc32Finish(DWORD dwCrc);
};
