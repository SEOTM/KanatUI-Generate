// ================================
// 文件: DispatchScene.cpp
// 描述: Dispatch 的场景实现切片 —— 会话分配器建立 / 成员块与配置分配 / 布局来源分派(缺省加载链 ·
//   内存缓冲 · 布局模型 · 布局对象数组 · 延迟装配) / 布局槽装配 / 文档解析 / 展开布局对象 /
//   几何解算 / 位置定稿 / 自证日志 / 释放链。
// 说明: 本文件只实现 Dispatch 的私有静态函数(LoadScene / UnloadScene / 布局槽辅助), 不对外暴露任何类或函数;
//   装配算式与多文档应用的条目装配同法, 此处是"布局槽 + 无文档条目"的形态。
//   项目身份(缺省文档名 / 覆盖文档名的环境变量名 / 自动退出的环境变量名)一律取自
//   DispatchConfig 在成员块里的副本 ⇒ 本文件不含任何项目身份常量。
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>
#include <new>
#include <stdio.h>
#include <string.h>			// memcpy(内存缓冲来源的整体复制)

#include "framework/dispatch/Dispatch.h"
#include "framework/base/mem/AllocatorHub.h"		// 会话分配器(线性分配器 + 小对象池)
#include "framework/dispatch/ResourceLoader.h"		// exe 目录与路径拼接
#include "framework/base/ControlSpec.h"		// 事件名约定(委托父容器)与控件类型
#include "framework/layout/GeometrySolver.h"	// 几何解算与参考量换算
#include "framework/layout/LayoutBuilder.h"	// 中间态节点 → 布局对象展开
#include "framework/layout/LayoutSolver.h"	// 读方向解析器(JSON / bin)

// ================================
// 区域: 文件内常量区
// ================================
constexpr size_t DISPATCH_ARENA_SIZE = 32 * 1024 * 1024;	// 会话线性分配器容量(单布局场景 ⇒ 远小于多文档应用)
constexpr size_t ENV_NAME_CAPACITY = 64;			// 环境变量名缓冲容量(含结尾 NUL)
constexpr DWORD DISPATCH_SLOT_INIT = 8;			// 布局槽数组初始容量(延迟装配按枚提交 ⇒ 按需扩容)
constexpr DWORD DISPATCH_SLOT_STEP = 8;			// 布局槽数组扩容步进(线性分配器不回收旧块 ⇒ 步进取中)
constexpr DWORD DISPATCH_MEM_MAX = 8 * 1024 * 1024;	// 内存缓冲来源的字节上限(bin / JSON 文档远小于此值)

// ================================
// 区域: 文件内函数声明区
// ================================
// 函数: ReleaseOwnedLayout
// 描述: 回收一枚归调度层所有的布局对象(控件缓冲回池 → 析构; 不归还分配器内存)
static VOID ReleaseOwnedLayout(Layout* pLayout);

// ================================
// 区域: 场景装配实现区
// ================================

// 函数: ReleaseOwnedLayout
// 描述: 回收一枚归调度层所有的布局对象(控件缓冲回池 → 析构; 不归还分配器内存)
static VOID ReleaseOwnedLayout(Layout* pLayout)
{
	if (pLayout == NULL)
	{
		return;
	}
	LayoutBuilder::FreeLayoutCtrlBuffers(*pLayout);
	Layout::DestroyLayout(pLayout);
}

// 函数: MakeBinName
// 描述: 由文档名派生同名 bin 名(扩展名替换为 .bin; 无扩展名时追加)
// 参数:
//		IN	docName:	文档文件名(如 Kanat.json)
//		OUT	szBin:		输出缓冲
//		IN	cbBin:		输出缓冲字节数(含结尾 NUL)
// 返回:
//		LPCSTR: 成功返回 szBin, 失败返回 NULL
static LPCSTR MakeBinName(LPCSTR docName, LPSTR szBin, size_t cbBin)
{
	if (docName == NULL || szBin == NULL || cbBin == 0)
	{
		return NULL;
	}
	size_t cbName = 0;
	while ((docName[cbName] != '\0') && (cbName + 1 < cbBin))
	{
		szBin[cbName] = docName[cbName];
		cbName++;
	}
	if (docName[cbName] != '\0')
	{
		return NULL;		// 名过长: 失败, 不静默截断
	}
	// 找最后一个扩展名分隔点(只认文件名内的 '.', 不跨越目录分隔符)
	size_t cbDot = cbName;
	for (size_t i = cbName; i > 0; i--)
	{
		CHAR c = szBin[i - 1];
		if (c == '\\' || c == '/')
		{
			break;
		}
		if (c == '.')
		{
			cbDot = i - 1;
			break;
		}
	}
	CONST CHAR BinExt[] = ".bin";
	if (cbDot + sizeof(BinExt) > cbBin)
	{
		return NULL;
	}
	szBin[cbDot] = '\0';
	for (size_t i = 0; i < sizeof(BinExt); i++)
	{
		szBin[cbDot + i] = BinExt[i];
	}
	return szBin;
}

// 函数: ResolveDocPath
// 描述: 按加载链解析布局文档路径(同名 bin 优先, 其次文档名本身)
// 说明: 形态由解析器按内容嗅探判定(见 LayoutSolver::LoadLayoutsAuto), 与扩展名无关;
//   bin 不存在时退回文档名本身(通常是工程源 JSON)。
// 参数:
//		IN	docName:	文档文件名
// 返回:
//		LPCSTR: 命中路径(内存池串); 目录解析失败返回 NULL
static LPCSTR ResolveDocPath(LPCSTR docName)
{
	LPCSTR lpszDoc = NULL;
	CHAR szBin[MAX_PATH]{};
	if (docName == NULL)
	{
		return lpszDoc;
	}
	// 第 1 级: 同名 .bin(构建期 tools\json2bin.py 生成的形态)
	if (MakeBinName(docName, szBin, sizeof(szBin)) != NULL)
	{
		LPCSTR lpszBin = ResourceLoader::GetFilePath(szBin);
		if (lpszBin != NULL && ResourceLoader::FileExists(lpszBin) != FALSE)
		{
			lpszDoc = lpszBin;
		}
	}
	// 第 2 级: 文档名本身(JSON 形态)
	if (lpszDoc == NULL)
	{
		lpszDoc = ResourceLoader::GetFilePath(docName);
	}
	return lpszDoc;
}

// ================================
// 区域: 布局槽实现区
// ================================

// 函数: ReserveSlots
// 描述: 保证槽数组至少容纳指定数量(不足时按步进扩容; 旧块随会话分配器整体释放)
BOOL Dispatch::ReserveSlots(DWORD dwCapacity)
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	DWORD dwWant = 0;
	BufferInfo bufSlots{};
	LayoutSlot* pNew = NULL;
	if (data == NULL || dwCapacity == 0)
	{
		return bRet;
	}
	if (dwCapacity <= data->dwSlotCapacity)
	{
		return TRUE;
	}
	if (dwCapacity > DISPATCH_SLOT_MAX)
	{
		return bRet;
	}
	// 1. 目标容量: 自当前容量起按步进递增(空数组从初始容量起)
	dwWant = (data->dwSlotCapacity > 0) ? data->dwSlotCapacity : DISPATCH_SLOT_INIT;
	while (dwWant < dwCapacity)
	{
		dwWant += DISPATCH_SLOT_STEP;
	}
	if (dwWant > DISPATCH_SLOT_MAX)
	{
		dwWant = DISPATCH_SLOT_MAX;
	}
	// 2. 新块取自会话分配器(旧块不回收, 随会话分配器整体释放)
	bufSlots = AllocatorHub::GetArena()->Allocate(sizeof(LayoutSlot) * dwWant);
	if (bufSlots.buffer == NULL || bufSlots.size < sizeof(LayoutSlot) * dwWant)
	{
		return bRet;
	}
	pNew = (LayoutSlot*)bufSlots.buffer;
	// 零初始化逐槽执行(槽内含 AutoSize 值类型 ⇒ 不按字节清零, 走默认构造)
	for (DWORD i = 0; i < dwWant; i++)
	{
		pNew[i] = LayoutSlot{};
	}
	// 3. 逐槽迁移(槽结构只含值与内嵌值类型, 浅拷贝安全)
	for (DWORD i = 0; i < data->dwSlotCount; i++)
	{
		pNew[i] = data->pSlots[i];
	}
	data->pSlots = pNew;
	data->dwSlotCapacity = dwWant;
	bRet = TRUE;
	return bRet;
}

// 函数: ResetSlots
// 描述: 清空布局槽(归调度层所有的布局对象先回收控件缓冲再析构; 槽数归零)
BOOL Dispatch::ResetSlots()
{
	if (data == NULL)
	{
		return FALSE;
	}
	for (DWORD i = 0; i < data->dwSlotCount; i++)
	{
		LayoutSlot& slot = data->pSlots[i];
		// 1. 归本层所有的布局对象: 控件缓冲回池 → 析构(借用者只清指针, 所有权归调用方)
		if (slot.pLayout != NULL && slot.bOwned != FALSE)
		{
			ReleaseOwnedLayout(slot.pLayout);
		}
		slot.pLayout = NULL;
		slot.bVisible = FALSE;
		slot.bNested = FALSE;
		slot.bOwned = FALSE;
	}
	data->dwSlotCount = 0;
	data->bReady = FALSE;
	return TRUE;
}

// 函数: AppendSlot
// 描述: 在槽尾追加一枚布局(容量不足时先扩容)
BOOL Dispatch::AppendSlot(Layout* pLayout, CONST AutoSize& ref, BOOL bOwned, BOOL bVisible, BOOL bNested)
{
	if (data == NULL || pLayout == NULL)
	{
		return FALSE;
	}
	if (data->dwSlotCount >= DISPATCH_SLOT_MAX)
	{
		Log("布局提交失败: 已达槽位上限(%u 枚)\n", (unsigned)DISPATCH_SLOT_MAX);
		return FALSE;
	}
	if (!ReserveSlots(data->dwSlotCount + 1))
	{
		Log("布局提交失败: 布局槽扩容失败\n");
		return FALSE;
	}
	{
		LayoutSlot& slot = data->pSlots[data->dwSlotCount];
		slot.pLayout = pLayout;
		slot.ref = ref;
		slot.bVisible = (bVisible != FALSE) ? TRUE : FALSE;
		slot.bNested = (bNested != FALSE) ? TRUE : FALSE;
		slot.bOwned = (bOwned != FALSE) ? TRUE : FALSE;
	}
	data->dwSlotCount++;
	data->bReady = TRUE;
	return TRUE;
}

// 函数: UseLayouts
// 描述: 整体替换布局槽(先清空), 逐枚接管调用方给出的布局对象(不取得所有权)
BOOL Dispatch::UseLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount, BOOL bFirstOnly)
{
	// 变量声明区(避免 goto 跳过变量声明)
	DWORD dwValid = 0;
	DWORD dwWndW = 0;
	DWORD dwWndH = 0;
	if (data == NULL || ppLayouts == NULL || pRefs == NULL)
	{
		return FALSE;
	}
	if (dwCount == 0 || dwCount > DISPATCH_SLOT_MAX)
	{
		Log("布局提交失败: 数量非法(%u, 允许 1..%u)\n", (unsigned)dwCount, (unsigned)DISPATCH_SLOT_MAX);
		return FALSE;
	}
	// 1. 入参校验: 空对象与空布局一律拒绝(不静默取用槽位); 借用对象不得与"归本层所有"的现有布局重叠
	//    (重置会析构归属槽的布局 ⇒ 随后入槽即悬空)
	for (DWORD i = 0; i < dwCount; i++)
	{
		if (ppLayouts[i] == NULL || ppLayouts[i]->GetTotalNum() == 0)
		{
			Log("布局提交失败: 第 %u 枚为空(空指针或零控件)\n", (unsigned)i);
			return FALSE;
		}
		for (DWORD j = 0; j < data->dwSlotCount; j++)
		{
			if (data->pSlots[j].bOwned != FALSE && data->pSlots[j].pLayout == ppLayouts[i])
			{
				Log("布局提交失败: 第 %u 枚与现有归属槽重叠(重置会析构该对象)\n", (unsigned)i);
				return FALSE;
			}
		}
	}
	// 2. 清空现有槽后逐枚接管: 首枚可见, 其余按"默认渲染首枚"的口径先隐藏
	if (!ResetSlots())
	{
		return FALSE;
	}
	for (DWORD i = 0; i < dwCount; i++)
	{
		CONST BOOL bVisible = ((bFirstOnly != FALSE) && (i > 0)) ? FALSE : TRUE;
		if (!AppendSlot(ppLayouts[i], pRefs[i], FALSE, bVisible, FALSE))
		{
			ResetSlots();
			return FALSE;
		}
	}
	// 3. 首枚窗口几何: 优先取窗口件(控件)的像素尺寸, 缺失时回退到参考尺寸的窗口量
	{
		CtrlInfo* pWnd = ppLayouts[0]->GetLayoutBase(CTRL_TYPE_WND);
		if (pWnd != NULL && pWnd->size.x > 0.0f && pWnd->size.y > 0.0f)
		{
			dwWndW = (DWORD)pWnd->size.x;
			dwWndH = (DWORD)pWnd->size.y;
		}
		if (dwWndW == 0 || dwWndH == 0)
		{
			dwWndW = pRefs[0].GetWindowWidth<DWORD>();
			dwWndH = pRefs[0].GetWindowHeight<DWORD>();
		}
		if (dwWndW < 16) { dwWndW = 16; }
		if (dwWndH < 16) { dwWndH = 16; }
		data->dwWndW = dwWndW;
		data->dwWndH = dwWndH;
	}
	// 4. 结构件自检(只诊断不拒绝): 结构件缺失或窗口件尺寸为零的布局在渲染期被静默跳过
	for (DWORD i = 0; i < dwCount; i++)
	{
		if (ppLayouts[i]->IsInvalid() == FALSE)
		{
			dwValid++;
		}
	}
	Log("布局校验: %u/%u 枚结构件齐备(无效布局在渲染期被跳过)\n", (unsigned)dwValid, (unsigned)dwCount);
	return TRUE;
}

// ================================
// 区域: 模型装配实现区
// ================================

// 函数: ApplyWndGeometry
// 描述: 按布局参考参数推导首枚窗口几何与该布局的参考尺寸(AutoSize)
BOOL Dispatch::ApplyWndGeometry(CONST RefSize& refSize, AutoSize& refOut, DWORD* pdwWndW, DWORD* pdwWndH)
{
	// 变量声明区(避免 goto 跳过变量声明)
	DWORD dwWndW = 0;
	DWORD dwWndH = 0;
	DWORD dwBase = 0;
	INT nMode = 0;
	// 1. 基准 = autosize 模式量(无外部宿主语境 ⇒ 窗口类模式按屏兜底)
	nMode = GeometrySolver::AutoModeIndex(refSize.mode);
	dwBase = GeometrySolver::EntryBasePx(nMode);
	if (dwBase < 1)
	{
		dwBase = 1;
	}
	// 2. 客户区像素 = 千分比 x 基准 / 1000(下限 16px)
	dwWndW = (DWORD)((unsigned __int64)dwBase * (DWORD)refSize.width / 1000);
	dwWndH = (DWORD)((unsigned __int64)dwBase * (DWORD)refSize.height / 1000);
	if (dwWndW < 16) { dwWndW = 16; }
	if (dwWndH < 16) { dwWndH = 16; }
	// 3. 参考尺寸(窗口化): 窗口宽高取上一步的客户区像素
	{
		AutoSize::SizeInfo<DWORD> si = GeometrySolver::EntryRefSizeInfo(refSize, dwBase, dwWndW, dwWndH);
		refOut = AutoSize::CreateByFixedSize(si, (AutoSize::AutoSizeMode)nMode);
	}
	if (pdwWndW != NULL) { *pdwWndW = dwWndW; }
	if (pdwWndH != NULL) { *pdwWndH = dwWndH; }
	return TRUE;
}

// 函数: BuildFromModel
// 描述: 从布局模型逐枚展开布局对象并解算几何(前 dwWant 枚; 产物归调度层所有)
BOOL Dispatch::BuildFromModel(CONST FileMidState& state, DWORD dwWant)
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	DWORD dwTotal = 0;
	DWORD dwUse = 0;
	if (data == NULL)
	{
		return bRet;
	}
	dwTotal = LayoutModel::LayoutCount(state);
	if (dwTotal == 0)
	{
		Log("场景装配失败: 布局模型不含布局\n");
		goto END;
	}
	// 1. 装配枚数 = min(模型布局数, 窗口数): 单窗口模式仍只用首枚(与既有行为一致)
	dwUse = (dwTotal < dwWant) ? dwTotal : dwWant;
	if (dwUse < dwTotal)
	{
		Log("布局模型: %u 枚布局, 本会话仅装配前 %u 枚(受窗口数 %u 限制)\n",
			(unsigned)dwTotal, (unsigned)dwUse, (unsigned)dwWant);
	}
	for (DWORD i = 0; i < dwUse; i++)
	{
		LayoutNode* pNode = LayoutModel::LayoutAt(state, i);
		Layout* pLayout = NULL;
		AutoSize ref;
		if (pNode == NULL || pNode->info == NULL || pNode->info->dwElements == 0)
		{
			Log("场景装配失败: 第 %u 枚布局不含可用布局信息\n", (unsigned)i);
			goto END;
		}
		// 2. 参考尺寸与窗口几何(首枚的窗口像素记进成员块, 供建窗自证与 GetWndRect 使用)
		if (!ApplyWndGeometry(pNode->info->refSize, ref,
			(i == 0) ? &data->dwWndW : NULL, (i == 0) ? &data->dwWndH : NULL))
		{
			goto END;
		}
		if (i == 0)
		{
			data->refSize = pNode->info->refSize;
		}
		// 3. 展开布局对象(线性只读展开态: 单窗口无增删 ⇒ 不需要链式储存)
		pLayout = LayoutBuilder::BuildLayoutFromNode(pNode, FALSE);
		if (pLayout == NULL)
		{
			Log("场景装配失败: 第 %u 枚布局对象展开失败(含不支持控件或分配失败)\n", (unsigned)i);
			goto END;
		}
		// 4. 逐控件解算像素几何 —— 控件对象只持像素几何, 其值 = 中间态元素规格(系数 + 偏移)的解析产物
		{
			AutoSize::SizeInfo<FLOAT> refF = ref.GetSizeInfo<FLOAT>();
			DWORD dwIndex = 0;
			for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, dwIndex++)
			{
				CtrlInfo* pCtrl = pLayout->GetCtrlInfo(dwIndex);
				if (pCtrl != NULL && pElem->data != NULL)
				{
					GeometrySolver::ApplySpecGeometry(*pCtrl, pElem->data, refF);
				}
				// 元素规格缺失 ⇒ 保持现状像素, 索引照常推进
			}
		}
		// 5. 坐标推导: 有效坐标 = 声明坐标 + 父链声明坐标之和(结构件不作父基; 见 Layout::DeriveCtrlPositions)
		pLayout->DeriveCtrlPositions();
		// 6. 窗口位置记录: 由声明位置解算(声明 = (-1,-1) 随机 / 在屏内直用 / 越屏降级随机; 见 ResolveWndPos)
		{
			CtrlInfo* pWnd = pLayout->GetLayoutBase(CTRL_TYPE_WND);
			GeometrySolver::ResolveWndPos(pWnd);
		}
		// 7. 入槽(归本层所有: Release / 替换时由 ResetSlots 回收)
		if (!AppendSlot(pLayout, ref, TRUE, TRUE, FALSE))
		{
			ReleaseOwnedLayout(pLayout);
			goto END;
		}
	}
	bRet = TRUE;
END:
	return bRet;
}

// 函数: CountTotalCtrls
// 描述: 统计全部布局槽的控件数(自证用)
DWORD Dispatch::CountTotalCtrls()
{
	DWORD dwTotal = 0;
	if (data == NULL)
	{
		return dwTotal;
	}
	for (DWORD i = 0; i < data->dwSlotCount; i++)
	{
		if (data->pSlots[i].pLayout != NULL)
		{
			dwTotal += (DWORD)data->pSlots[i].pLayout->GetTotalNum();
		}
	}
	return dwTotal;
}

// 函数: CountBoundCtrls
// 描述: 统计全部布局槽内已绑定事件处理器的控件数(自证用)
DWORD Dispatch::CountBoundCtrls()
{
	DWORD dwBound = 0;
	if (data == NULL)
	{
		return dwBound;
	}
	for (DWORD i = 0; i < data->dwSlotCount; i++)
	{
		Layout* pLayout = data->pSlots[i].pLayout;
		if (pLayout == NULL)
		{
			continue;
		}
		for (size_t j = 0; j < pLayout->GetTotalNum(); j++)
		{
			CtrlInfo* pCtrl = pLayout->GetCtrlInfo(j);
			if (pCtrl != NULL && pCtrl->pEventHandler != NULL)
			{
				dwBound++;
			}
		}
	}
	return dwBound;
}

// ================================
// 区域: 场景载入与释放实现区
// ================================

// 函数: LoadScene
// 描述: 建立会话分配器与成员块, 并按布局来源装配场景(缺省加载链 / 内存缓冲 / 布局模型 /
//   布局对象数组 / 延迟装配), 完毕后打印来源与场景自证
BOOL Dispatch::LoadScene(CONST DispatchConfig& cfg, CONST LayoutSource& source)
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	Arena* arena = NULL;
	MemPool* pool = NULL;
	BufferInfo bufData{};
	BufferInfo bufSolver{};
	BufferInfo bufMem{};
	LPCSTR lpszDocPath = NULL;
	CONST FileMidState* pModel = NULL;

	// 1. 会话分配器(线性分配器 + 小对象池)与三个领域分配器注入
	//    注: 成员块 / 中间态块 / 布局对象 / 控件缓冲分别取自这几处 ⇒ 注入必须早于任何分配
	//    注: 调用方已预先建立会话分配器(布局对象在 Init 之前构建的场景)时复用, 不重建 ——
	//        重建会令先前构建的布局与其控件缓冲整体悬空
	if (AllocatorHub::GetArena() == NULL && !AllocatorHub::Init(DISPATCH_ARENA_SIZE))
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
	//    注: 文档名可为空(布局模型 / 布局对象数组 / 延迟装配三种来源不需要文档)
	if (cfg.szDocName != NULL)
	{
		lstrcpynA(data->szDocName, cfg.szDocName, MAX_PATH);
	}
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
	// 4. exe 目录缓存(目录缓存的串取自线性分配器 ⇒ 必须在分配器建立之后)
	ResourceLoader::Init();
	// 5. 布局槽数组初始容量(全部来源共用; 延迟装配按枚提交 ⇒ 之后按需扩容)
	if (!ReserveSlots(DISPATCH_SLOT_INIT))
	{
		Log("场景载入失败: 布局槽分配失败\n");
		goto END;
	}
	// 6. 按来源装配 ----------------------------------------------------------------
	switch (source.kind)
	{
	case LAYOUT_SOURCE_DEFAULT:
		{
			// 6-a. 缺省加载链: 同名 bin 优先, 其次文档名本身(形态由解析器按内容嗅探)
			Log("布局来源: 缺省加载链\n");
			lpszDocPath = ResolveDocPath(data->szDocName);
			if (lpszDocPath == NULL)
			{
				Log("场景载入失败: 未能解析程序所在目录\n");
				goto END;
			}
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
			pModel = &data->pSolver->GetState();
		}
		break;
	case LAYOUT_SOURCE_MEMORY:
		{
			// 6-b. 内存缓冲: 复制进会话分配器并补结尾符(JSON 解析按 C 串读), 再按内容嗅探分派;
			//      解析失败不视为初始化失败 ⇒ 回退缺省加载链
			BOOL bParsed = FALSE;
			Log("布局来源: 内存缓冲(%u 字节)\n", (unsigned)source.cbMemSize);
			if (source.pMemData != NULL && source.cbMemSize > 0 && source.cbMemSize <= DISPATCH_MEM_MAX)
			{
				bufMem = arena->Allocate((size_t)source.cbMemSize + 1);
				if (bufMem.buffer != NULL && bufMem.size >= (size_t)source.cbMemSize + 1)
				{
					bufSolver = arena->Allocate(sizeof(LayoutSolver));
					if (bufSolver.buffer != NULL)
					{
						data->pSolver = new (bufSolver.buffer) LayoutSolver(arena);
						memcpy(bufMem.buffer, source.pMemData, (size_t)source.cbMemSize);
						((LPSTR)bufMem.buffer)[source.cbMemSize] = '\0';
						bParsed = data->pSolver->LoadLayoutsAuto(bufMem.buffer, source.cbMemSize);
					}
				}
			}
			if (!bParsed)
			{
				Log("内存缓冲解析失败: 回退缺省加载链\n");
				lpszDocPath = ResolveDocPath(data->szDocName);
				if (lpszDocPath == NULL)
				{
					Log("场景载入失败: 未能解析程序所在目录\n");
					goto END;
				}
				if (data->pSolver == NULL)
				{
					bufSolver = arena->Allocate(sizeof(LayoutSolver));
					if (bufSolver.buffer == NULL)
					{
						Log("场景载入失败: 解析器分配失败\n");
						goto END;
					}
					data->pSolver = new (bufSolver.buffer) LayoutSolver(arena);
				}
				if (!data->pSolver->LoadLayoutsAuto(lpszDocPath))
				{
					Log("场景载入失败: 文档解析失败(%s)\n", lpszDocPath);
					goto END;
				}
			}
			else
			{
				lpszDocPath = "(内存缓冲)";
			}
			pModel = &data->pSolver->GetState();
		}
		break;
	case LAYOUT_SOURCE_MODEL:
		{
			// 6-c. 布局模型: 直接展开(不再解析文档; 模型按借用处理, 所有权归调用方)
			Log("布局来源: 布局模型\n");
			if (source.pModel == NULL || LayoutModel::LayoutCount(*source.pModel) == 0)
			{
				Log("场景载入失败: 布局模型为空\n");
				goto END;
			}
			pModel = source.pModel;
			lpszDocPath = "(布局模型)";
		}
		break;
	case LAYOUT_SOURCE_LAYOUTS:
		{
			// 6-d. 布局对象数组: 直接使用(不解析文档、不重算几何); 首枚可见, 其余由业务代码打开
			Log("布局来源: 布局对象数组(%u 枚)\n", (unsigned)source.dwLayoutCount);
			if (!UseLayouts(source.ppLayouts, source.pRefs, source.dwLayoutCount, TRUE))
			{
				Log("场景载入失败: 布局对象数组不可用\n");
				goto END;
			}
			lpszDocPath = "(布局对象数组)";
		}
		break;
	case LAYOUT_SOURCE_DEFERRED:
		{
			// 6-e. 延迟装配: 不提交真实布局, 只建立会话基础设施; 布局由业务代码经 AttachLayouts / AddLayout 提交
			Log("布局来源: 延迟装配(待业务代码提交)\n");
			lpszDocPath = "(延迟装配)";
		}
		break;
	default:
		{
			// 判别位非法(枚举被越界赋值) ⇒ 拒绝, 不静默按延迟装配处理
			Log("场景载入失败: 未知的布局来源(%d)\n", (int)source.kind);
			goto END;
		}
	}
	// 7. 模型路径的统一装配(缺省 / 内存 / 模型三支共用)
	if (pModel != NULL)
	{
		if (!BuildFromModel(*pModel, data->dwWndCount))
		{
			goto END;
		}
	}
	// 8. 场景自证(仅在已装配布局时打印; 延迟装配在业务代码提交后另行自证)
	if (data->dwSlotCount == 0)
	{
		bRet = TRUE;
		goto END;
	}
	{
		Layout* pFirst = data->pSlots[0].pLayout;
		LPCSTR lpszLayoutName = "(布局对象)";
		if (pModel != NULL)
		{
			CONST LayoutNode* pNode = LayoutModel::LayoutAt(*pModel, 0);
			if (pNode != NULL && pNode->info != NULL)
			{
				lpszLayoutName = pNode->info->name;
			}
		}
		Log("场景载入: %s (布局=%s 控件=%zu 窗口=%ux%u)\n",
			(lpszDocPath != NULL) ? lpszDocPath : "(未知)",
			(lpszLayoutName != NULL) ? lpszLayoutName : "(未命名)",
			(size_t)CountTotalCtrls(), data->dwWndW, data->dwWndH);
	}
	// 9. 事件绑定自证: 统计已绑定处理器的控件数, 以及"委托父容器"的解析结果
	//     客观回答"事件是否已接到框架侧解析器与控件上"(绑定表稀疏 ⇒ 其余控件无响应);
	//     委托者的事件由父容器的事件提供 ⇒ 其处理器非空即证明委托已在装配期解析到落点
	{
		DWORD dwBound = 0;
		DWORD dwEntrust = 0;
		DWORD dwResolved = 0;
		Layout* pFirst = data->pSlots[0].pLayout;
		for (size_t i = 0; i < pFirst->GetTotalNum(); i++)
		{
			CtrlInfo* pCtrl = pFirst->GetCtrlInfo(i);
			CONST ElementSpec* pSpec = (pModel != NULL)
				? LayoutModel::ElementAt(*pModel, 0, (DWORD)i) : NULL;
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
		// 布局模型缺失(纯布局对象来源)时无法按 event 声明统计委托父容器 ⇒ 该分支只报已绑定数
		if (pModel == NULL)
		{
			Log("事件绑定: %u 枚控件已绑定处理器(无布局模型, 委托父容器计数不适用)\n", (unsigned)dwBound);
		}
		else
		{
			Log("事件绑定: %u 枚控件已绑定处理器(其中委托父容器 %u 枚, 已解析 %u 枚)\n",
				(unsigned)dwBound, (unsigned)dwEntrust, (unsigned)dwResolved);
		}
	}
	// 10. 层次自证: 逐枚打印控件层次(控件ID=层), 供核对"层序按 parentID 链推导"的结果
	//     判据: 结构件恒 0; 其余 = 父控件层次 + 自身类型步进(无父 / 父缺失 ⇒ 以窗口底档 0 为基)
	{
		Layout* pFirst = data->pSlots[0].pLayout;
		Log("控件层次:");
		for (size_t i = 0; i < pFirst->GetTotalNum(); i++)
		{
			CtrlInfo* pCtrl = pFirst->GetCtrlInfo(i);
			printf(" %d=%d", (pCtrl != NULL) ? pCtrl->id : -1, (pCtrl != NULL) ? (int)pCtrl->nLayer : 0);
		}
		printf("\n");
	}
	// 11. 坐标自证: 逐枚打印有效坐标(控件ID=x,y), 供核对"内容控件坐标 = 声明坐标 + 父链声明坐标之和"
	//     判据: 结构件不受影响(WND = 桌面位置 / TITLE / CNTR = 窗口本地); 内容控件含 CNTR 的原点偏移
	{
		Layout* pFirst = data->pSlots[0].pLayout;
		INT nOffset = 0;			// 有效坐标与声明坐标不同的枚数(= 真正吃到父链偏移的控件数)
		Log("控件坐标:");
		for (size_t i = 0; i < pFirst->GetTotalNum(); i++)
		{
			CtrlInfo* pCtrl = pFirst->GetCtrlInfo(i);
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
	bRet = TRUE;

END:
	return bRet;
}

// 函数: UnloadScene
// 描述: 释放场景资源(布局槽与布局对象 → 解析器 → 成员块显式析构 → 领域分配器解绑 → 会话分配器)
VOID Dispatch::UnloadScene()
{
	// 1. 布局槽: 归调度层所有的布局对象先回收控件缓冲再析构(借用者只清指针); 槽数组随分配器释放
	if (data != NULL)
	{
		ResetSlots();
		data->pSlots = NULL;
		data->dwSlotCapacity = 0;
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
