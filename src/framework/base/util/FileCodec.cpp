#include "framework/base/util/FileCodec.h"

// ================================
// 库名: FileCodec
// 描述: 文件处理(纯校验 / 摘要; 压缩与加密留待版本号低 8 位标记启用时实现)
// 说明: 算法为 CRC32(IEEE 802.3; 反射, 多项式 0xEDB88320, 初值 0xFFFFFFFF, 结果取反);
//       流式三件套(Crc32Begin / Crc32Update / Crc32Finish)供"覆盖不连续区间"的调用方使用
//       (如文件校验码字段自身按 0 参与计算: 先推进 4 个零字节, 再推进其余正文)。
// ================================

// 函数: Crc32Begin
// 描述: 取 CRC32 的初始状态(流式计算入口)
DWORD FileCodec::Crc32Begin()
{
	return 0xFFFFFFFFu;
}

// 函数: Crc32Update
// 描述: 用一段字节推进 CRC32 状态(可连续多次调用)
DWORD FileCodec::Crc32Update(DWORD dwCrc, LPCVOID data, size_t cbData)
{
	CONST BYTE* pData = (CONST BYTE*)data;
	if (pData == NULL)
	{
		return dwCrc;
	}
	for (size_t i = 0; i < cbData; i++)
	{
		dwCrc ^= (DWORD)pData[i];
		for (INT b = 0; b < 8; b++)
		{
			dwCrc = (dwCrc >> 1) ^ (0xEDB88320u & (0u - (dwCrc & 1u)));
		}
	}
	return dwCrc;
}

// 函数: Crc32Finish
// 描述: 取 CRC32 的最终值(对状态取反)
DWORD FileCodec::Crc32Finish(DWORD dwCrc)
{
	return ~dwCrc;
}
