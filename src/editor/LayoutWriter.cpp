#include "editor/LayoutWriter.h"
#include "common/FileCodec.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <charconv>
#include <new>

// ================================
// 区域: 输出缓冲(计数趟与写入趟共用同一套写出函数)
// ================================

// 结构: JsonOut
// 描述: 规范 JSON 文本的输出目标
// 说明: lpBuf == NULL 时只累加 cbLen(计数趟); 否则边写边累加,
//       越界置 bOverflow(调用方据此判定失败)。
struct JsonOut
{
	CHAR*	lpBuf;		// 目标缓冲(NULL = 只计数)
	size_t	cbCap;		// 目标缓冲字节数
	size_t	cbLen;		// 已写出/已计数的字节数
	BOOL	bOverflow;	// 写入趟是否越界(计数趟恒 FALSE)
};

// ================================
// 区域: 辅助函数声明区
// ================================

// 函数: OutPut
// 描述: 写出一个字符(计数趟只累加长度)
VOID OutPut(JsonOut& out, CHAR c);
// 函数: OutStr
// 描述: 写出一个以 '\0' 结尾的串(逐字符经 OutPut)
VOID OutStr(JsonOut& out, LPCSTR s);
// 函数: OutIndent
// 描述: 写出 nLevel 级缩进(每级 2 空格)
VOID OutIndent(JsonOut& out, INT nLevel);
// 函数: OutJsonString
// 描述: 写出 JSON 字符串字面量(转义引号/反斜杠/控制符; UTF-8 中文按字节原样)
VOID OutJsonString(JsonOut& out, LPCSTR s);
// 函数: OutInt
// 描述: 写出整数(LONG)
VOID OutInt(JsonOut& out, LONG v);
// 函数: OutFloat
// 描述: 写出系数值: 整值写整数形式; 其余取最短往返十进制
VOID OutFloat(JsonOut& out, FLOAT v);
// 函数: OutFactor
// 描述: 写出单行因子对象: "key": { "coeff": [c0, c1, ..., c7], "offset": N }
VOID OutFactor(JsonOut& out, LPCSTR key, CONST SizeFactor& f);
// 函数: BuildCore
// 描述: 按规范排版把整份中间态写出到 out(计数趟与写入趟走同一实现)
BOOL BuildCore(CONST FileMidState& state, JsonOut& out);

// ================================
// 类名: LayoutWriter
// 描述: 布局写入器 —— 把中间态(布局数据模型)生成规范 JSON 文本或 bin 字节流
// ================================

// 函数: BuildJsonText
// 描述: 把整份中间态生成规范 JSON 文本(两趟: 先计数再分配写入; 所有权交调用方)
// 参数:
//		IN	state:		中间态(布局链 + 元素链)
//		OUT	ppszText:	文本首址(调用方负责 delete[]; 失败置 NULL)
//		OUT	pcbText:	文本字节数(含末尾换行)
// 返回:
//		BOOL: TRUE = 已生成; FALSE = 入参非法 / 分配失败 / 写出越界
BOOL LayoutWriter::BuildJsonText(CONST FileMidState& state, LPSTR* ppszText, size_t* pcbText)
{
	BOOL bRet = FALSE;
	size_t cbNeed = 0;
	LPSTR lpText = NULL;
	if (ppszText == NULL || pcbText == NULL)
	{
		return bRet;
	}
	*ppszText = NULL;
	*pcbText = 0;
	// 1. 计数趟: 只累加所需字节数(不写任何存储)
	{
		JsonOut cnt{};
		cnt.lpBuf = NULL;
		cnt.cbCap = 0;
		cnt.cbLen = 0;
		cnt.bOverflow = FALSE;
		if (!BuildCore(state, cnt))
		{
			return bRet;
		}
		cbNeed = cnt.cbLen;
		if (cbNeed == 0)
		{
			return bRet;
		}
	}
	// 2. 写入趟: 一次性分配并写出(禁用异常的工程约定 ⇒ 用 nothrow 形式)
	lpText = new (std::nothrow) CHAR[cbNeed];
	if (lpText == NULL)
	{
		return bRet;
	}
	{
		JsonOut out{};
		out.lpBuf = lpText;
		out.cbCap = cbNeed;
		out.cbLen = 0;
		out.bOverflow = FALSE;
		if (!BuildCore(state, out) || out.bOverflow || (out.cbLen != cbNeed))
		{
			delete[] lpText;
			return bRet;
		}
	}
	*ppszText = lpText;
	*pcbText = cbNeed;
	bRet = TRUE;
	return bRet;
}

// 函数: SaveToFile
// 描述: 生成规范 JSON 文本并按"备份 + 临时文件 + 原子替换"写入指定路径
// 参数:
//		IN	state:	中间态(布局链 + 元素链)
//		IN	szPath:	目标文件路径
// 返回:
//		BOOL: TRUE = 已写入; FALSE = 路径非法 / 文本生成失败 / 备份或写盘或替换失败
BOOL LayoutWriter::SaveToFile(CONST FileMidState& state, LPCSTR szPath)
{
	BOOL bRet = FALSE;
	LPSTR lpText = NULL;
	size_t cbText = 0;
	if (szPath == NULL || szPath[0] == '\0')
	{
		return bRet;
	}
	if (!BuildJsonText(state, &lpText, &cbText))
	{
		printf("[APP] 保存失败: 规范 JSON 文本生成失败\n");
		return bRet;
	}
	bRet = SaveRawToFile((CONST BYTE*)lpText, cbText, szPath);
	delete[] lpText;
	return bRet;
}

// 函数: SaveRawToFile
// 描述: 把一段内存字节按"备份 + 临时文件 + 原子替换"写入指定路径(JSON 与 bin 两条写盘链共用)
// 参数:
//		IN	pData:	正文字节首址
//		IN	cbData: 正文字节数
//		IN	szPath:	目标文件路径
// 返回:
//		BOOL: TRUE = 已写入; FALSE = 入参非法 / 路径过长 / 备份或写盘或替换失败
BOOL LayoutWriter::SaveRawToFile(CONST BYTE* pData, size_t cbData, LPCSTR szPath)
{
	BOOL bRet = FALSE;
	CHAR szTmp[MAX_PATH]{};
	CHAR szBak[MAX_PATH]{};
	if ((pData == NULL) || (cbData == 0) || (szPath == NULL) || (szPath[0] == '\0'))
	{
		return bRet;
	}
	// 1. 同目录临时文件与备份路径(路径过长即拒绝)
	if ((sprintf_s(szTmp, sizeof(szTmp), "%s.tmp", szPath) <= 0) ||
		(sprintf_s(szBak, sizeof(szBak), "%s.bak", szPath) <= 0))
	{
		printf("[APP] 保存失败: 目标路径过长(%s)\n", szPath);
		goto END;
	}
	// 2. 目标已存在 ⇒ 先备份(不覆盖既有 .bak; 备份失败只诊断, 不阻断写入)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		if (GetFileAttributesExA(szPath, GetFileExInfoStandard, &fad))
		{
			if (!CopyFileA(szPath, szBak, FALSE) && (GetLastError() != ERROR_FILE_EXISTS))
			{
				printf("[APP] 保存: 备份未成功(%s; GetLastError=%u), 继续写入\n",
					szBak, (unsigned)GetLastError());
			}
		}
	}
	// 3. 正文写临时文件(写满 + 落盘)
	{
		HANDLE hFile = CreateFileA(szTmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		DWORD dwWrote = 0;
		DWORD dwErr = 0;
		BOOL bWrite = FALSE;
		if (hFile == INVALID_HANDLE_VALUE)
		{
			dwErr = GetLastError();
		}
		else
		{
			bWrite = WriteFile(hFile, pData, (DWORD)cbData, &dwWrote, NULL) && (dwWrote == (DWORD)cbData);
			if (bWrite)
			{
				bWrite = FlushFileBuffers(hFile);
			}
			dwErr = bWrite ? 0 : GetLastError();
			CloseHandle(hFile);
		}
		if (!bWrite)
		{
			printf("[APP] 保存失败: 写入临时文件未完成(%s; GetLastError=%u)\n", szTmp, (unsigned)dwErr);
			DeleteFileA(szTmp);
			goto END;
		}
	}
	// 4. 原子替换目标文件
	if (!MoveFileExA(szTmp, szPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		printf("[APP] 保存失败: 替换目标文件失败(%s; GetLastError=%u)\n", szPath, (unsigned)GetLastError());
		DeleteFileA(szTmp);
		goto END;
	}
	printf("[APP] 保存: %s (%u 字节)\n", szPath, (unsigned)cbData);
	bRet = TRUE;
END:
	return bRet;
}

// 函数: OutPut
// 描述: 写出一个字符(计数趟只累加长度)
VOID OutPut(JsonOut& out, CHAR c)
{
	if (out.lpBuf != NULL)
	{
		if (out.cbLen < out.cbCap)
		{
			out.lpBuf[out.cbLen] = c;
		}
		else
		{
			out.bOverflow = TRUE;
		}
	}
	out.cbLen++;
}
// 函数: OutStr
// 描述: 写出一段文本(逐字符经 OutPut)
VOID OutStr(JsonOut& out, LPCSTR s)
{
	if (s == NULL)
	{
		return;
	}
	for (LPCSTR p = s; *p != '\0'; p++)
	{
		OutPut(out, *p);
	}
}
// 函数: OutIndent
// 描述: 写出 nLevel 级缩进(每级 2 空格)
VOID OutIndent(JsonOut& out, INT nLevel)
{
	for (INT i = 0; i < nLevel; i++)
	{
		OutPut(out, ' ');
		OutPut(out, ' ');
	}
}
// 函数: OutJsonString
// 描述: 写出 JSON 字符串字面量(转义引号/反斜杠/控制符; 其余字节含 UTF-8 中文原样)
VOID OutJsonString(JsonOut& out, LPCSTR s)
{
	OutPut(out, '"');
	if (s != NULL)
	{
		for (LPCSTR p = s; *p != '\0'; p++)
		{
			unsigned char c = (unsigned char)*p;
			switch (c)
			{
			case '"':	OutStr(out, "\\\"");	break;
			case '\\':	OutStr(out, "\\\\");	break;
			case '\n':	OutStr(out, "\\n");		break;
			case '\r':	OutStr(out, "\\r");		break;
			case '\t':	OutStr(out, "\\t");		break;
			default:
				if (c < 0x20)
				{
					CHAR szEsc[8]{};
					sprintf_s(szEsc, sizeof(szEsc), "\\u%04x", (unsigned)c);
					OutStr(out, szEsc);
				}
				else
				{
					OutPut(out, (CHAR)c);
				}
				break;
			}
		}
	}
	OutPut(out, '"');
}
// 函数: OutInt
// 描述: 写出整数(LONG)
VOID OutInt(JsonOut& out, LONG v)
{
	CHAR szBuf[32]{};
	sprintf_s(szBuf, sizeof(szBuf), "%ld", v);
	OutStr(out, szBuf);
}
// 函数: OutFloat
// 描述: 写出系数值 —— 整值写整数形式(无小数点); 其余取最短往返十进制
// 说明: 最短往返由 std::to_chars 的 FLOAT 重载提供(0.1f 写回 "0.1");
//       该重载失败时回落 "%.9g"(往返仍无损, 但非最短表示)。
VOID OutFloat(JsonOut& out, FLOAT v)
{
	CHAR szBuf[64]{};
	// 非有限值(NaN / 正负无穷)不是合法 JSON 数字 ⇒ 按 0 写出(避免生成无法解析的文档)
	if (!(v == v) || v > 3.402823466e+38f || v < -3.402823466e+38f)
	{
		OutStr(out, "0");
		return;
	}
	if (v == 0.0f)
	{
		OutStr(out, "0");		// 零(含 -0.0f)统一写 "0"
		return;
	}
	{
		LONG n = (LONG)v;
		if (((FLOAT)n == v) && (n > -100000000L) && (n < 100000000L))
		{
			sprintf_s(szBuf, sizeof(szBuf), "%ld", n);
			OutStr(out, szBuf);
			return;
		}
	}
	{
		std::to_chars_result res = std::to_chars(szBuf, szBuf + sizeof(szBuf) - 1, v);
		if (res.ec == std::errc())
		{
			*res.ptr = '\0';
			OutStr(out, szBuf);
		}
		else
		{
			sprintf_s(szBuf, sizeof(szBuf), "%.9g", (DOUBLE)v);
			OutStr(out, szBuf);
		}
	}
}
// 函数: OutFactor
// 描述: 写出单行因子对象: "key": { "coeff": [c0, c1, ..., c7], "offset": N }
VOID OutFactor(JsonOut& out, LPCSTR key, CONST SizeFactor& f)
{
	OutPut(out, '"');
	OutStr(out, key);
	OutStr(out, "\": { \"coeff\": [");
	for (INT i = 0; i < GENFACTOR_NUM; i++)
	{
		if (i > 0)
		{
			OutStr(out, ", ");
		}
		OutFloat(out, f.coeff.RefCoeffs[i]);
	}
	OutStr(out, "], \"offset\": ");
	OutInt(out, (LONG)f.offset);
	OutStr(out, " }");
}
// 函数: BuildCore
// 描述: 按规范排版把整份中间态写出到 out(计数趟与写入趟共用)
// 说明: 布局链与元素链即文件顺序; 键序固定(见 LayoutWriter.h 说明)。
BOOL BuildCore(CONST FileMidState& state, JsonOut& out)
{
	BOOL bRet = FALSE;
	static CONST LPCSTR aszRefFld[6] = {
		"width", "height", "controlHeight", "fontSize", "space", "padding"
	};
	OutStr(out, "{\n");
	OutIndent(out, 1); OutStr(out, "\"layouts\": [\n");
	{
		BOOL bAnyLayout = FALSE;
		for (LayoutNode* pLay = LayoutModel::LayoutFirst(state);
			pLay != NULL; pLay = LayoutModel::LayoutNext(pLay))
		{
			LayoutInfo* info = pLay->info;
			if (info == NULL)
			{
				goto END;
			}
			if (bAnyLayout)
			{
				OutStr(out, ",\n");
			}
			bAnyLayout = TRUE;
			OutIndent(out, 2); OutStr(out, "{\n");
			OutIndent(out, 3); OutStr(out, "\"name\": "); OutJsonString(out, info->name); OutStr(out, ",\n");
			OutIndent(out, 3); OutStr(out, "\"label\": "); OutJsonString(out, info->label); OutStr(out, ",\n");
			OutIndent(out, 3); OutStr(out, "\"IsMaxiable\": "); OutStr(out, info->IsMaxiable ? "true" : "false"); OutStr(out, ",\n");
			OutIndent(out, 3); OutStr(out, "\"IsResizeable\": "); OutStr(out, info->IsResizeable ? "true" : "false"); OutStr(out, ",\n");
			OutIndent(out, 3); OutStr(out, "\"wndX\": "); OutInt(out, info->wndX); OutStr(out, ",\n");
			OutIndent(out, 3); OutStr(out, "\"wndY\": "); OutInt(out, info->wndY); OutStr(out, ",\n");
			OutIndent(out, 3); OutFactor(out, "width", info->width); OutStr(out, ",\n");
			OutIndent(out, 3); OutFactor(out, "height", info->height); OutStr(out, ",\n");
			// 布局级 autosize(各布局独立经验参数; 多行对象)
			OutIndent(out, 3); OutStr(out, "\"autosize\": {\n");
			OutIndent(out, 4); OutStr(out, "\"mode\": "); OutJsonString(out, info->refSize.mode); OutStr(out, ",\n");
			OutIndent(out, 4); OutStr(out, "\"unit\": "); OutInt(out, (LONG)info->refSize.unit); OutStr(out, ",\n");
			for (INT k = 0; k < 6; k++)
			{
				OutIndent(out, 4);
				OutPut(out, '"'); OutStr(out, aszRefFld[k]); OutStr(out, "\": ");
				OutInt(out, (LONG)LayoutModel::RefSizeValueAt(info->refSize, k));
				OutStr(out, (k < 5) ? ",\n" : "\n");
			}
			OutIndent(out, 3); OutStr(out, "},\n");
			// 控件数组(元素链顺序 = 文件顺序)
			OutIndent(out, 3); OutStr(out, "\"ctrls\": [\n");
			{
				BOOL bAnyCtrl = FALSE;
				for (ElementNode* pElem = LayoutModel::ElementFirst(pLay);
					pElem != NULL; pElem = LayoutModel::ElementNext(pElem))
				{
					ElementSpec* es = pElem->data;
					if (es == NULL)
					{
						goto END;
					}
					if (bAnyCtrl)
					{
						OutStr(out, ",\n");
					}
					bAnyCtrl = TRUE;
					OutIndent(out, 4); OutStr(out, "{\n");
					OutIndent(out, 5); OutStr(out, "\"id\": "); OutInt(out, (LONG)es->id); OutStr(out, ",\n");
					OutIndent(out, 5); OutStr(out, "\"parent\": "); OutInt(out, (LONG)es->parentID); OutStr(out, ",\n");
					OutIndent(out, 5); OutStr(out, "\"type\": "); OutJsonString(out, es->type); OutStr(out, ",\n");
					OutIndent(out, 5); OutStr(out, "\"label\": "); OutJsonString(out, es->label); OutStr(out, ",\n");
					OutIndent(out, 5); OutStr(out, "\"text\": "); OutJsonString(out, es->text); OutStr(out, ",\n");
					OutIndent(out, 5); OutStr(out, "\"event\": "); OutJsonString(out, es->event); OutStr(out, ",\n");
					OutIndent(out, 5); OutFactor(out, "x", es->x); OutStr(out, ",\n");
					OutIndent(out, 5); OutFactor(out, "y", es->y); OutStr(out, ",\n");
					OutIndent(out, 5); OutFactor(out, "width", es->width); OutStr(out, ",\n");
					OutIndent(out, 5); OutFactor(out, "height", es->height); OutStr(out, "\n");
					OutIndent(out, 4); OutPut(out, '}');
				}
				if (bAnyCtrl)
				{
					OutStr(out, "\n");
				}
			}
			OutIndent(out, 3); OutStr(out, "]\n");
			OutIndent(out, 2); OutPut(out, '}');
		}
		if (bAnyLayout)
		{
			OutStr(out, "\n");
		}
	}
	OutIndent(out, 1); OutStr(out, "]\n");
	OutStr(out, "}\n");
	bRet = TRUE;
END:
	return bRet;
}

// ================================
// 区域: bin 写方向实现(格式 v2; 契约常量与段尺寸公式 BinSegBytes 见 src\framework\LayoutModel.h)
// 说明: 记录区 = 中间态结构体的内存镜像 —— 头部区整块 memcpy 一枚 LayoutInfo,
//       控件区逐枚 memcpy ElementSpec(不做逐字段编码); 只有文件头三项与段头一项按 4 B 写入;
//       校验码 = 对整个文件算 CRC32(首 4 B 即校验码字段, 写入时先按 0 参与, 算完回填)。
// ================================

// 函数: BuildBinBytes
// 描述: 把整份中间态生成 bin 字节流(两趟: 先计数再分配写入; 所有权交调用方)
// 参数:
//		IN	state:		中间态(布局链 + 元素链)
//		OUT	ppBytes:	字节流首址(调用方负责 delete[]; 失败置 NULL)
//		OUT	pcbBytes:	字节数
// 返回:
//		BOOL: TRUE = 已生成(校验码已写入文件头首 4 字节); FALSE = 入参非法 / 段数或控件数超上限 / 分配失败
BOOL LayoutWriter::BuildBinBytes(CONST FileMidState& state, LPBYTE* ppBytes, size_t* pcbBytes)
{
	BOOL bRet = FALSE;
	size_t cbTotal = 0;
	LPBYTE lpBytes = NULL;
	DWORD dwLayouts = 0;
	if (ppBytes == NULL || pcbBytes == NULL)
	{
		return bRet;
	}
	*ppBytes = NULL;
	*pcbBytes = 0;
	// 1. 第一趟: 统计总长(并校验段数/控件数上限)
	dwLayouts = LayoutModel::LayoutCount(state);
	if (dwLayouts == 0 || dwLayouts > (DWORD)LAYOUT_MAX)
	{
		printf("[BIN] 段数量非法(%u, 允许 1..%u): 生成中止\n", (unsigned)dwLayouts, (unsigned)LAYOUT_MAX);
		return bRet;
	}
	cbTotal = (size_t)BIN_FILE_HEAD;
	for (CONST LayoutNode* pLay = LayoutModel::LayoutFirst(state);
		pLay != NULL; pLay = LayoutModel::LayoutNext(pLay))
	{
		if (pLay->info == NULL)
		{
			return bRet;
		}
		if (pLay->info->dwElements > (DWORD)CTRL_MAX)
		{
			printf("[BIN] 段控件数非法(%u, 上限 %u): 生成中止\n",
				(unsigned)pLay->info->dwElements, (unsigned)CTRL_MAX);
			return bRet;
		}
		cbTotal += BinSegBytes(pLay->info->dwElements);
	}
	// 2. 第二趟: 一次性分配并写出(禁用异常的工程约定 ⇒ nothrow 形式)
	lpBytes = new (std::nothrow) BYTE[cbTotal];
	if (lpBytes == NULL)
	{
		return bRet;
	}
	memset(lpBytes, 0, cbTotal);
	{
		DWORD dwZero = 0;
		DWORD dwVer = BIN_VERSION;
		size_t cbOff = (size_t)BIN_FILE_HEAD;
		// 文件头: 校验码先置 0(全文写完后统一计算并回填)
		memcpy(lpBytes + 0, &dwZero, 4);
		memcpy(lpBytes + 4, &dwVer, 4);
		memcpy(lpBytes + 8, &dwLayouts, 4);
		for (CONST LayoutNode* pLay = LayoutModel::LayoutFirst(state);
			pLay != NULL; pLay = LayoutModel::LayoutNext(pLay))
		{
			CONST LayoutInfo* info = pLay->info;
			DWORD dwSegSize = (DWORD)BinSegBytes(info->dwElements);
			BYTE* pSeg = lpBytes + cbOff;
			// 段头: 本段大小(含本字段自身)
			memcpy(pSeg + 0, &dwSegSize, 4);
			// 头部区: 整块 memcpy 一枚 LayoutInfo(其中含尾部 dwElements = 文件格式的"控件数量")
			memcpy(pSeg + BIN_SEG_HEAD, info, sizeof(LayoutInfo));
			// 控件记录区: 逐枚 memcpy 一枚 ElementSpec(顺序 = 元素链顺序)
			{
				BYTE* pCtrl = pSeg + BIN_SEG_HEAD + BIN_LAYOUT_SIZE;
				for (CONST ElementNode* pElem = LayoutModel::ElementFirst(pLay);
					pElem != NULL; pElem = LayoutModel::ElementNext(pElem))
				{
					if (pElem->data == NULL)
					{
						delete[] lpBytes;
						return bRet;
					}
					memcpy(pCtrl, pElem->data, sizeof(ElementSpec));
					pCtrl += BIN_CTRL_SIZE;
				}
			}
			cbOff += dwSegSize;
		}
		// 3. 校验码 = CRC32(整个文件; 首 4 字节即校验码字段, 此刻为 0 ⇒ 按 0 参与), 回填文件头首 4 字节
		{
			DWORD dwCrc = FileCodec::Crc32((LPCVOID)lpBytes, cbTotal);
			memcpy(lpBytes + 0, &dwCrc, 4);
		}
	}
	*ppBytes = lpBytes;
	*pcbBytes = cbTotal;
	bRet = TRUE;
	return bRet;
}

// 函数: SaveBinToFile
// 描述: 生成 bin 字节流并按"备份 + 临时文件 + 原子替换"写盘(写盘骨架与 JSON 侧共用)
// 参数:
//		IN	state:	中间态(布局链 + 元素链)
//		IN	szPath:	目标文件路径
// 返回:
//		BOOL: TRUE = 已写入; FALSE = 生成失败 / 路径非法 / 备份或写盘或替换失败
BOOL LayoutWriter::SaveBinToFile(CONST FileMidState& state, LPCSTR szPath)
{
	BOOL bRet = FALSE;
	LPBYTE lpBytes = NULL;
	size_t cbBytes = 0;
	if (szPath == NULL || szPath[0] == '\0')
	{
		return bRet;
	}
	if (!BuildBinBytes(state, &lpBytes, &cbBytes))
	{
		printf("[BIN] 保存失败: bin 字节流生成失败\n");
		return bRet;
	}
	bRet = SaveRawToFile((CONST BYTE*)lpBytes, cbBytes, szPath);
	delete[] lpBytes;
	return bRet;
}
