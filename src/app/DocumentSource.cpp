// ================================
// 文件: DocumentSource.cpp
// 描述: 启动加载链(四级顺序探测)与内置文档资源取件的实现
// ================================
#include <windows.h>
#include <stdio.h>						// printf(逐级诊断行)
#include "app/DocumentSource.h"
#include "app/resource.h"				// IDR_GENUI_JSON / IDR_GENUI_BIN(内置文档资源 ID)
#include "common/ResourceLoader.h"		// 通用加载原语: GetFilePath / LoadResource(exe 目录 + 资源节 → 池)

// ================================
// 区域: 启动加载链区(四级顺序探测)
// 说明: 顺序 = 第 1 级 .\GenUI.bin → 第 2 级 .\GenUI.json → 第 3 级 resource.res\GenUI.bin
//       → 第 4 级 resource.res\GenUI.json; 命中即用(命中后不再探测后续级别)。
//       第 1/2 级 = exe 目录下的外部文件; 第 3/4 级 = 随 exe 内置的资源(见 src\app\resource.h);
//       第 1/3 级为 bin 形态、第 2/4 级为 JSON 形态, 实际解析按文件内容嗅探分派
//       (见 LayoutSolver::IsBinDocument / LoadLayoutsAuto)⇒ 级序才是口径, 形态由内容决定。
// ================================


// 函数: GetEmbeddedDoc
// 描述: 取随 exe 内置的文档资源(资源节的 JSON 文档; 资源 ID/类型见 src\app\resource.h)
// 返回: TRUE 命中(ppData/pcbData 有效; pcbData 不含补的 '\0')
BOOL DocumentSource::GetEmbeddedDoc(LPVOID* ppData, DWORD* pcbData)
{
	return ResourceLoader::LoadResource(IDR_GENUI_JSON, "JSON", ppData, pcbData);
}
// 函数: GetEmbeddedDocBin
// 描述: 取随 exe 内置的 bin 文档资源(加载链第 3 级 resource.res\GenUI.bin; 资源 ID/类型见 src\app\resource.h)
// 返回: TRUE 命中(ppData/pcbData 有效; pcbData 不含补的 '\0')
BOOL DocumentSource::GetEmbeddedDocBin(LPVOID* ppData, DWORD* pcbData)
{
	return ResourceLoader::LoadResource(IDR_GENUI_BIN, "BIN", ppData, pcbData);
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
	// ---- 第 1 级: .\GenUI.bin(exe 目录外部文件)----
	if (dwLevelMask & 0x1u)
	{
		lpBin = ResourceLoader::GetFilePath("GenUI.bin");
		if (lpBin != NULL && GetFileAttributesA(lpBin) != INVALID_FILE_ATTRIBUTES)
		{
			// 文件在 ⇒ 真命中(形态由文件内容嗅探判定; 畸形文件在载入时被拒 ⇒ 启动中止, 不越级)
			pOut->nLevel = 1;
			pOut->bBin = TRUE;
			pOut->bEmbedded = FALSE;
			pOut->lpPath = lpBin;
			pOut->lpDesc = ".\\GenUI.bin(第 1 级: exe 目录外部文件)";
			printf("[APP] 加载链: 第 1 级 .\\GenUI.bin — 命中\n");
			bRet = TRUE;
			goto END;
		}
		printf("[APP] 加载链: 第 1 级 .\\GenUI.bin — 未命中(文件不存在)\n");
	}
	// ---- 第 2 级: .\GenUI.json(exe 目录外部文件)----
	if (dwLevelMask & 0x2u)
	{
		lpJson = ResourceLoader::GetFilePath("GenUI.json");
		if (lpJson != NULL && GetFileAttributesA(lpJson) != INVALID_FILE_ATTRIBUTES)
		{
			pOut->nLevel = 2;
			pOut->bBin = FALSE;
			pOut->bEmbedded = FALSE;
			pOut->lpPath = lpJson;
			pOut->lpDesc = ".\\GenUI.json(第 2 级: exe 目录外部文件)";
			printf("[APP] 加载链: 第 2 级 .\\GenUI.json — 命中\n");
			bRet = TRUE;
			goto END;
		}
		printf("[APP] 加载链: 第 2 级 .\\GenUI.json — 未命中(文件不存在)\n");
	}
	// ---- 第 3 级: resource.res\GenUI.bin(随 exe 内置资源)----
	if (dwLevelMask & 0x4u)
	{
		if (GetEmbeddedDocBin(&pData, &cbData))
		{
			pOut->nLevel = 3;
			pOut->bBin = TRUE;
			pOut->bEmbedded = TRUE;
			pOut->pData = pData;
			pOut->cbData = cbData;
			pOut->lpDesc = "resource.res\\GenUI.bin(第 3 级: exe 内置资源)";
			printf("[APP] 加载链: 第 3 级 resource.res\\GenUI.bin — 命中(%u 字节)\n", (unsigned)cbData);
			bRet = TRUE;
			goto END;
		}
		printf("[APP] 加载链: 第 3 级 resource.res\\GenUI.bin — 未命中(未内嵌)\n");
	}
	// ---- 第 4 级: resource.res\GenUI.json(随 exe 内置资源; JSON 形态)----
	if (dwLevelMask & 0x8u)
	{
		if (GetEmbeddedDoc(&pData, &cbData))
		{
			pOut->nLevel = 4;
			pOut->bBin = FALSE;
			pOut->bEmbedded = TRUE;
			pOut->pData = pData;
			pOut->cbData = cbData;
			pOut->lpDesc = "resource.res\\GenUI.json(第 4 级: exe 内置资源)";
			printf("[APP] 加载链: 第 4 级 resource.res\\GenUI.json — 命中(%u 字节)\n", (unsigned)cbData);
			bRet = TRUE;
			goto END;
		}
		printf("[APP] 加载链: 第 4 级 resource.res\\GenUI.json — 未命中(未内嵌)\n");
	}
END:
	if (!bRet)
	{
		printf("[APP] 加载链: 四级均未命中 ⇒ 无可用文档(不内置代码回退)\n");
	}
	return bRet;
}
