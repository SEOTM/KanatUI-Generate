// ================================
// 文件: DispatchScene.cpp
// 描述: Dispatch 的场景实现切片 —— 会话分配器建立 / 成员块与配置分配 / 文档解析 / 展开布局对象 /
//   几何解算 / 位置定稿 / 自证日志 / 释放链。
// 说明: 本文件只实现 Dispatch 的私有静态函数 LoadScene / UnloadScene, 不对外暴露任何类或函数;
//   装配算式与多文档应用的条目装配同法, 此处只是"单布局 + 无文档条目"的最小形态。
//   项目身份(缺省文档名 / 覆盖文档名的环境变量名 / 自动退出的环境变量名)一律取自
//   DispatchConfig 在成员块里的副本 ⇒ 本文件不含任何项目身份常量。
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>
#include <new>
#include <stdio.h>

#include "framework/dispatch/Dispatch.h"
#include "common/AllocatorHub.h"		// 会话分配器(线性分配器 + 小对象池)
#include "common/ResourceLoader.h"		// exe 目录与路径拼接
#include "framework/base/ControlSpec.h"		// 事件名约定(委托父容器)与控件类型
#include "framework/layout/GeometrySolver.h"	// 几何解算与参考量换算
#include "framework/layout/LayoutBuilder.h"	// 中间态节点 → 布局对象展开
#include "framework/layout/LayoutSolver.h"	// 读方向解析器(JSON / bin)

// ================================
// 区域: 文件内常量区
// ================================
constexpr size_t DISPATCH_ARENA_SIZE = 32 * 1024 * 1024;	// 会话线性分配器容量(单布局场景 ⇒ 远小于多文档应用)
constexpr size_t ENV_NAME_CAPACITY = 64;			// 环境变量名缓冲容量(含结尾 NUL)

// ================================
// 区域: 场景装配实现区
// ================================

// 函数: LoadScene
// 描述: 建立会话分配器与成员块, 并从 exe 目录载入布局文档(解析 → 展开 → 几何解算 → 位置定稿 → 自证)
BOOL Dispatch::LoadScene(CONST DispatchConfig& cfg)
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	Arena* arena = NULL;
	MemPool* pool = NULL;
	BufferInfo bufData{};
	BufferInfo bufSolver{};
	LayoutNode* pNode = NULL;
	LPCSTR lpszDocPath = NULL;
	INT nMode = 0;
	DWORD dwBase = 0;

	// 1. 会话分配器(线性分配器 + 小对象池)与三个领域分配器注入
	//    注: 成员块 / 中间态块 / 布局对象 / 控件缓冲分别取自这几处 ⇒ 注入必须早于任何分配
	if (!AllocatorHub::Init(DISPATCH_ARENA_SIZE))
	{
		Log("场景载入失败: 会话分配器建立失败\n");
		goto END;
	}
	arena = AllocatorHub::GetArena();
	pool = AllocatorHub::GetSmallPool();
	// 2. 成员块: 从线性分配器取内存并 placement new(Release 时显式析构; 内存随会话分配器整体释放)
	bufData = arena->Allocate(sizeof(DispatchData));
	if (bufData.buffer == NULL || bufData.size < sizeof(DispatchData))
	{
		Log("场景载入失败: 成员块分配失败\n");
		goto END;
	}
	data = new (bufData.buffer) DispatchData();
	// 2-bis. 配置复制: 项目身份字符串与运行开关进成员块(调度层不含项目身份常量 ⇒ 身份只来自配置)
	lstrcpynA(data->szDocName, cfg.szDocName, MAX_PATH);
	if (cfg.szDocEnv != NULL)
	{
		lstrcpynA(data->szDocEnv, cfg.szDocEnv, (INT)ENV_NAME_CAPACITY);
	}
	if (cfg.szAutoCloseEnv != NULL)
	{
		lstrcpynA(data->szAutoCloseEnv, cfg.szAutoCloseEnv, (INT)ENV_NAME_CAPACITY);
	}
	data->dwWndCount = (cfg.dwWndCount > 0) ? cfg.dwWndCount : 1;
	data->bDarkFollowSystem = (cfg.bDarkFollowSystem != FALSE) ? TRUE : FALSE;
	data->bDarkMode = FALSE;
	LayoutModel::SetAllocators(arena, pool);	// 中间态的元素块 / 布局块
	Layout::SetAllocators(arena, pool);		// 布局对象 / 控件数组 / 链节点
	ProjCR::SetAllocators(arena, pool);		// 投影配色的轴位置表(宿主创建配色前注入)
	// 3. 文档名解析: 缺省名取自配置, 可由配置指定的环境变量覆盖(多范例共用同一 exe)
	{
		CHAR szEnvDoc[MAX_PATH]{};
		DWORD dwDocLen = 0;
		if (data->szDocEnv[0] != '\0')
		{
			dwDocLen = GetEnvironmentVariableA(data->szDocEnv, szEnvDoc, (DWORD)MAX_PATH);
		}
		if (dwDocLen > 0 && dwDocLen < (DWORD)MAX_PATH)
		{
			lstrcpynA(data->szDocName, szEnvDoc, MAX_PATH);		// 已设置 / 取值合法 ⇒ 覆盖缺省名
		}
	}
	// 4. exe 目录缓存与文档路径拼接(目录缓存的串取自线性分配器 ⇒ 必须在分配器建立之后)
	ResourceLoader::Init();
	lpszDocPath = ResourceLoader::GetFilePath(data->szDocName);
	if (lpszDocPath == NULL)
	{
		Log("场景载入失败: 未能解析程序所在目录\n");
		goto END;
	}
	// 5. 解析文档(解析器由线性分配器持有 ⇒ 生命周期随会话分配器, 无需单独释放内存)
	bufSolver = arena->Allocate(sizeof(LayoutSolver));
	if (bufSolver.buffer == NULL)
	{
		Log("场景载入失败: 解析器分配失败\n");
		goto END;
	}
	data->pSolver = new (bufSolver.buffer) LayoutSolver(arena);
	if (!data->pSolver->LoadLayoutsAuto(lpszDocPath))
	{
		Log("场景载入失败: 文档解析失败(%s)\n", lpszDocPath);
		goto END;
	}
	// 6. 取首个布局(单窗口模式 ⇒ 一份文档只用一个布局; 多布局文档只展开第一个)
	pNode = LayoutModel::LayoutAt(data->pSolver->GetState(), 0);
	if (pNode == NULL || pNode->info == NULL || pNode->info->dwElements == 0)
	{
		Log("场景载入失败: 文档不含可用布局\n");
		goto END;
	}
	data->refSize = pNode->info->refSize;
	// 7. 窗口像素与内部参考尺寸(与文档条目的窗口几何口径一致):
	//    基准 = autosize 模式量(无外部宿主语境 ⇒ 窗口类模式按屏兜底);
	//    客户区像素 = 千分比 x 基准 / 1000(下限 16px); 内部参考的窗口宽高 = 该客户区像素
	nMode = GeometrySolver::AutoModeIndex(data->refSize.mode);
	dwBase = GeometrySolver::EntryBasePx(nMode);
	if (dwBase < 1) { dwBase = 1; }
	data->dwWndW = (DWORD)((unsigned __int64)dwBase * (DWORD)data->refSize.width / 1000);
	data->dwWndH = (DWORD)((unsigned __int64)dwBase * (DWORD)data->refSize.height / 1000);
	if (data->dwWndW < 16) { data->dwWndW = 16; }
	if (data->dwWndH < 16) { data->dwWndH = 16; }
	{
		AutoSize::SizeInfo<DWORD> si = GeometrySolver::EntryRefSizeInfo(data->refSize, dwBase,
			data->dwWndW, data->dwWndH);
		data->ref = AutoSize::CreateByFixedSize(si, (AutoSize::AutoSizeMode)nMode);
	}
	// 8. 展开布局对象(线性只读展开态: 单窗口无增删 ⇒ 不需要链式储存)
	data->pLayout = LayoutBuilder::BuildLayoutFromNode(pNode, FALSE);
	if (data->pLayout == NULL)
	{
		Log("场景载入失败: 布局对象展开失败(含不支持控件或分配失败)\n");
		goto END;
	}
	// 9. 逐控件解算像素几何 —— 控件对象只持像素几何, 其值 = 中间态元素规格(系数 + 偏移)的解析产物
	{
		AutoSize::SizeInfo<FLOAT> ref = data->ref.GetSizeInfo<FLOAT>();
		DWORD dwIndex = 0;
		for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, dwIndex++)
		{
			CtrlInfo* pCtrl = data->pLayout->GetCtrlInfo(dwIndex);
			if (pCtrl != NULL && pElem->data != NULL)
			{
				GeometrySolver::ApplySpecGeometry(*pCtrl, pElem->data, ref);
			}
			// 元素规格缺失 ⇒ 保持现状像素, 索引照常推进
		}
	}
	// 9-bis. 坐标推导: 有效坐标 = 声明坐标 + 父链声明坐标之和(结构件不作父基; 见 Layout::DeriveCtrlPositions)
	//        注: 必须在第 8 步之后 —— 推导只读声明坐标, 几何解算前声明值尚未落地
	data->pLayout->DeriveCtrlPositions();
	// 10. 窗口位置记录: 由声明位置解算(声明 = (-1,-1) 随机 / 在屏内直用 / 越屏降级随机; 见 ResolveWndPos)
	//     注: 位置必须在此定稿 —— 首帧的渲染目标结算(AcquireWnd 的 pos 参数)以它为准
	{
		CtrlInfo* pWnd = data->pLayout->GetLayoutBase(CTRL_TYPE_WND);
		GeometrySolver::ResolveWndPos(pWnd);
	}
	data->bReady = TRUE;
	bRet = TRUE;
	Log("场景载入: %s (布局=%s 控件=%zu 窗口=%ux%u)\n",
		lpszDocPath, pNode->info->name, data->pLayout->GetTotalNum(), data->dwWndW, data->dwWndH);
	// 11. 事件绑定自证: 统计已绑定处理器的控件数, 以及"委托父容器"的解析结果
	//     客观回答"事件是否已接到框架侧解析器与控件上"(绑定表稀疏 ⇒ 其余控件无响应);
	//     委托者的事件由父容器的事件提供 ⇒ 其处理器非空即证明委托已在装配期解析到落点
	{
		DWORD dwBound = 0;
		DWORD dwEntrust = 0;
		DWORD dwResolved = 0;
		for (size_t i = 0; i < data->pLayout->GetTotalNum(); i++)
		{
			CtrlInfo* pCtrl = data->pLayout->GetCtrlInfo(i);
			CONST ElementSpec* pSpec = LayoutModel::ElementAt(data->pSolver->GetState(), 0, (DWORD)i);
			if (pCtrl == NULL)
			{
				continue;
			}
			if (pCtrl->pEventHandler != NULL)
			{
				dwBound++;
			}
			if (pSpec != NULL && CtrlEventIsEntrustParent(pSpec->event) != FALSE)
			{
				dwEntrust++;
				if (pCtrl->pEventHandler != NULL)
				{
					dwResolved++;
				}
			}
		}
		Log("事件绑定: %u 枚控件已绑定处理器(其中委托父容器 %u 枚, 已解析 %u 枚)\n",
			(unsigned)dwBound, (unsigned)dwEntrust, (unsigned)dwResolved);
	}
	// 12. 层次自证: 逐枚打印控件层次(控件ID=层), 供核对"层序按 parentID 链推导"的结果
	//     判据: 结构件恒 0; 其余 = 父控件层次 + 自身类型步进(无父 / 父缺失 ⇒ 以窗口底档 0 为基)
	{
		Log("控件层次:");
		for (size_t i = 0; i < data->pLayout->GetTotalNum(); i++)
		{
			CtrlInfo* pCtrl = data->pLayout->GetCtrlInfo(i);
			printf(" %d=%d", (pCtrl != NULL) ? pCtrl->id : -1, (pCtrl != NULL) ? (int)pCtrl->nLayer : 0);
		}
		printf("\n");
	}
	// 13. 坐标自证: 逐枚打印有效坐标(控件ID=x,y), 供核对"内容控件坐标 = 声明坐标 + 父链声明坐标之和"
	//     判据: 结构件不受影响(WND = 桌面位置 / TITLE / CNTR = 窗口本地); 内容控件含 CNTR 的原点偏移
	{
		INT nOffset = 0;			// 有效坐标与声明坐标不同的枚数(= 真正吃到父链偏移的控件数)
		Log("控件坐标:");
		for (size_t i = 0; i < data->pLayout->GetTotalNum(); i++)
		{
			CtrlInfo* pCtrl = data->pLayout->GetCtrlInfo(i);
			if (pCtrl == NULL)
			{
				continue;
			}
			if ((pCtrl->position - pCtrl->posDeclared) != vec2(0.0f, 0.0f))
			{
				nOffset++;
			}
			printf(" %d=(%d,%d)", pCtrl->id, (int)pCtrl->position.x, (int)pCtrl->position.y);
		}
		printf("(其中含父链偏移 %d 枚)\n", (int)nOffset);
		fflush(stdout);
	}

END:
	return bRet;
}

// 函数: UnloadScene
// 描述: 释放场景资源(布局缓冲 → 布局对象 → 解析器 → 成员块显式析构 → 领域分配器解绑 → 会话分配器)
VOID Dispatch::UnloadScene()
{
	// 1. 布局控件缓冲先回池(布局对象析构之前; 否则池块泄漏)
	if (data != NULL && data->pLayout != NULL)
	{
		LayoutBuilder::FreeLayoutCtrlBuffers(*data->pLayout);
		Layout::DestroyLayout(data->pLayout);
		data->pLayout = NULL;
	}
	// 2. 解析器显式析构(内存随线性分配器整体释放 ⇒ 不单独回收)
	if (data != NULL && data->pSolver != NULL)
	{
		data->pSolver->~LayoutSolver();
		data->pSolver = NULL;
	}
	if (data != NULL)
	{
		data->bReady = FALSE;
	}
	// 3. 领域分配器解绑(此后任何分配一律失败, 不再静默取旧池)
	Layout::SetAllocators(NULL, NULL);
	LayoutModel::SetAllocators(NULL, NULL);
	ProjCR::SetAllocators(NULL, NULL);
	// 4. 成员块显式析构并置空(内存属会话分配器 ⇒ 不单独回收)
	if (data != NULL)
	{
		DispatchData* pData = data;
		data = NULL;
		pData->~DispatchData();
	}
	// 5. 会话分配器整体释放(中间态 / 布局对象 / 池块随之失效)
	AllocatorHub::Release();
}
