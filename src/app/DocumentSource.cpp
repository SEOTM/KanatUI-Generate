// ================================
// 文件: DocumentSource.cpp
// 描述: 启动加载链(四级顺序探测)与内置文档资源取件的实现
// ================================
#include <windows.h>
#include <stdio.h>						// printf(逐级诊断行)
#include <string.h>						// memcpy(内置资源拷贝)
#include "app/DocumentSource.h"
#include "app/resource.h"				// IDR_KANATUI_GENERATE_JSON / IDR_KANATUI_GENERATE_BIN(内置文档资源 ID)
#include "framework/dispatch/ResourceLoader.h"		// exe 目录与路径拼接(GetFilePath)
#include "framework/base/mem/AllocatorHub.h"		// 会话分配器(内置资源副本)


// ================================
// 区域: 文件内静态工具区
// ================================

// ================================
// 函数: LoadEmbeddedResource
// 描述: 读取随 exe 内置的资源节字节并拷进会话分配器(末尾补一位 '\0')
// 说明: 框架层 ResourceLoader 只提供 exe 目录与路径拼接(2026-10-23 归位后不再承担资源节读取);
//   内置资源读取是本工程加载链第 3/4 级的应用侧设施, 故实现留在本文件。
//   资源字节不保证以 '\0' 结尾, 而 JSON 解析需要 NUL ⇒ 统一补一位;
//   补的字节不计入 pcbData(bin 解析按显式长度, 不受影响)。
// 参数:
//		IN	dwResId:	资源 ID(由 app\resource.h 定义)
//		IN	szResType:	资源类型串(如 "JSON" / "BIN")
//		OUT	ppData:		数据首址(会话分配器副本; 失败置 NULL)
//		OUT	pcbData:	字节数(不含补的 '\0')
// 返回:
//		BOOL: TRUE = 命中(ppData/pcbData 有效)
// ================================
static BOOL LoadEmbeddedResource(DWORD dwResId, LPCSTR szResType, LPVOID* ppData, DWORD* pcbData)
{
	BOOL bRet = FALSE;
	HRSRC hRes = NULL;
	HGLOBAL hGlobal = NULL;
	LPVOID pLock = NULL;
	DWORD dwSize = 0;
	Arena* pArena = AllocatorHub::GetArena();
	if (pArena == NULL || szResType == NULL || ppData == NULL || pcbData == NULL)
	{
		return bRet;
	}
	*ppData = NULL;
	*pcbData = 0;
	hRes = FindResourceA(NULL, MAKEINTRESOURCEA(dwResId), szResType);
	if (hRes == NULL)
	{
		return bRet;
	}
	hGlobal = LoadResource(NULL, hRes);
	if (hGlobal == NULL)
	{
		return bRet;
	}
	pLock = LockResource(hGlobal);
	dwSize = SizeofResource(NULL, hRes);
	if (pLock == NULL || dwSize == 0)
	{
		return bRet;
	}
	{
		BufferInfo buf = pArena->Allocate((size_t)dwSize + 1);
		if (buf.buffer == NULL || buf.size < (size_t)dwSize + 1)
		{
			return bRet;
		}
		memcpy(buf.buffer, pLock, dwSize);
		((CHAR*)buf.buffer)[dwSize] = '\0';
		*ppData = buf.buffer;
		*pcbData = dwSize;
	}
	bRet = TRUE;
	return bRet;
}

// ================================
// 区域: 启动加载链区(四级顺序探测)
// 说明: 顺序 = 第 1 级 .\KanatUIGenerate.bin → 第 2 级 .\KanatUIGenerate.json → 第 3 级 resource.res\KanatUIGenerate.bin
//       → 第 4 级 resource.res\KanatUIGenerate.json; 命中即用(命中后不再探测后续级别)。
//       第 1/2 级 = exe 目录下的外部文件; 第 3/4 级 = 随 exe 内置的资源(见 src\app\resource.h);
//       第 1/3 级为 bin 形态、第 2/4 级为 JSON 形态, 实际解析按文件内容嗅探分派
//       (见 LayoutSolver::IsBinDocument / LoadLayoutsAuto)⇒ 级序才是口径, 形态由内容决定。
// ================================


// 函数: GetEmbeddedDoc
// 描述: 取随 exe 内置的文档资源(资源节的 JSON 文档; 资源 ID/类型见 src\app\resource.h)
// 返回: TRUE 命中(ppData/pcbData 有效; pcbData 不含补的 '\0')
BOOL DocumentSource::GetEmbeddedDoc(LPVOID* ppData, DWORD* pcbData)
{
	return LoadEmbeddedResource(IDR_KANATUI_GENERATE_JSON, "JSON", ppData, pcbData);
}
// 函数: GetEmbeddedDocBin
// 描述: 取随 exe 内置的 bin 文档资源(加载链第 3 级 resource.res\KanatUIGenerate.bin; 资源 ID/类型见 src\app\resource.h)
// 返回: TRUE 命中(ppData/pcbData 有效; pcbData 不含补的 '\0')
BOOL DocumentSource::GetEmbeddedDocBin(LPVOID* ppData, DWORD* pcbData)
{
	return LoadEmbeddedResource(IDR_KANATUI_GENERATE_BIN, "BIN", ppData, pcbData);
}
// 函数: ResolveDocumentSource
// 描述: 按四级顺序探测可用文档源(命中即用), 逐级打印诊断行; 四级均不可用返回 FALSE
// 参数: dwLevelMask = 允许尝试的级别掩码(bit0..bit3 = 第 1..4 级; 默认 0xF = 全部)
BOOL DocumentSource::ResolveDocumentSource(DocSource* pOut, DWORD dwLevelMask)
{
	BOOL bRet = FALSE;
	LPCSTR lpBin = NULL;
	LPCSTR lpJson = NULL;
	LPVOID pData = NULL;
	DWORD cbData = 0;
	if (pOut == NULL)
	{
		return bRet;
	}
	*pOut = DocSource{};
	// ---- 第 1 级: .\KanatUIGenerate.bin(exe 目录外部文件)----
	if (dwLevelMask & 0x1u)
	{
		lpBin = ResourceLoader::GetFilePath("KanatUIGenerate.bin");
		if (lpBin != NULL && GetFileAttributesA(lpBin) != INVALID_FILE_ATTRIBUTES)
		{
			// 文件在 ⇒ 真命中(形态由文件内容嗅探判定; 畸形文件在载入时被拒 ⇒ 启动中止, 不越级)
			pOut->nLevel = 1;
			pOut->bBin = TRUE;
			pOut->bEmbedded = FALSE;
			pOut->lpPath = lpBin;
			pOut->lpDesc = ".\\KanatUIGenerate.bin(第 1 级: exe 目录外部文件)";
			printf("[APP] 加载链: 第 1 级 .\\KanatUIGenerate.bin — 命中\n");
			bRet = TRUE;
			goto END;
		}
		printf("[APP] 加载链: 第 1 级 .\\KanatUIGenerate.bin — 未命中(文件不存在)\n");
	}
	// ---- 第 2 级: .\KanatUIGenerate.json(exe 目录外部文件)----
	if (dwLevelMask & 0x2u)
	{
		lpJson = ResourceLoader::GetFilePath("KanatUIGenerate.json");
		if (lpJson != NULL && GetFileAttributesA(lpJson) != INVALID_FILE_ATTRIBUTES)
		{
			pOut->nLevel = 2;
			pOut->bBin = FALSE;
			pOut->bEmbedded = FALSE;
			pOut->lpPath = lpJson;
			pOut->lpDesc = ".\\KanatUIGenerate.json(第 2 级: exe 目录外部文件)";
			printf("[APP] 加载链: 第 2 级 .\\KanatUIGenerate.json — 命中\n");
			bRet = TRUE;
			goto END;
		}
		printf("[APP] 加载链: 第 2 级 .\\KanatUIGenerate.json — 未命中(文件不存在)\n");
	}
	// ---- 第 3 级: resource.res\KanatUIGenerate.bin(随 exe 内置资源)----
	if (dwLevelMask & 0x4u)
	{
		if (GetEmbeddedDocBin(&pData, &cbData))
		{
			pOut->nLevel = 3;
			pOut->bBin = TRUE;
			pOut->bEmbedded = TRUE;
			pOut->pData = pData;
			pOut->cbData = cbData;
			pOut->lpDesc = "resource.res\\KanatUIGenerate.bin(第 3 级: exe 内置资源)";
			printf("[APP] 加载链: 第 3 级 resource.res\\KanatUIGenerate.bin — 命中(%u 字节)\n", (unsigned)cbData);
			bRet = TRUE;
			goto END;
		}
		printf("[APP] 加载链: 第 3 级 resource.res\\KanatUIGenerate.bin — 未命中(未内嵌)\n");
	}
	// ---- 第 4 级: resource.res\KanatUIGenerate.json(随 exe 内置资源; JSON 形态)----
	if (dwLevelMask & 0x8u)
	{
		if (GetEmbeddedDoc(&pData, &cbData))
		{
			pOut->nLevel = 4;
			pOut->bBin = FALSE;
			pOut->bEmbedded = TRUE;
			pOut->pData = pData;
			pOut->cbData = cbData;
			pOut->lpDesc = "resource.res\\KanatUIGenerate.json(第 4 级: exe 内置资源)";
			printf("[APP] 加载链: 第 4 级 resource.res\\KanatUIGenerate.json — 命中(%u 字节)\n", (unsigned)cbData);
			bRet = TRUE;
			goto END;
		}
		printf("[APP] 加载链: 第 4 级 resource.res\\KanatUIGenerate.json — 未命中(未内嵌)\n");
	}
END:
	if (!bRet)
	{
		printf("[APP] 加载链: 四级均未命中 ⇒ 无可用文档(不内置代码回退)\n");
	}
	return bRet;
}
