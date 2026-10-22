// ================================
// 文件: EditorCommands.cpp
// 说明: 编辑器命令层的实现 —— 布局族命令(增 / 删 / 重排 / 改名) + 元素规格的取值 / 插入 / 移除
//   口径: 判据本体一律经承载者(doc\Document)的现有公开接口; 本模块不新增框架层接口
//         ⇒ 锁定面(MemoryManager / Layout / LayoutModel)零改动。
// ================================

#include <windows.h>					// RtlZeroMemory(布局重排的深拷缓冲清零)
#include "editor/EditorCommands.h"
#include "doc/Document.h"
#include "common/AllocatorHub.h"		// 池内字符串拷贝(CopyString; 布局名两处同写用)
#include "framework/base/BasicDesign.h"		// AutoSize(条目参考尺寸)
#include "framework/base/ControlSpec.h"		// 控件类型身份(CtrlType / GetTypeName; 新建布局的三结构件模板)
#include "framework/layout/GeometrySolver.h"	// 几何解算与模式/基准换算(条目窗口像素与内部参考重建)

// ================================
// 说明: 三件是被布局命令复用的判据本体(应用侧的同名件仍服务其其余调用点 ⇒ 两处并存,
// ================================

// 函数: DefaultRefSize
// 描述: 返回参考尺寸/经验参数默认样例(屏高模式 + 千分比) —— 取自布局模型(中间态)的默认值
static RefSize DefaultRefSize()
{
	return LayoutModel::DefaultRefSize();
}

// 函数: BuildEntryRef
// 描述: 依参考尺寸/经验参数(中间态 RefSize)重建条目窗口像素(wndW/wndH)与内部参考尺寸 ref
// 窗口化语义: 布局窗口客户区像素 = autosize width/height 千分比 x 基准/1000;
//       内部控件参考 ref 的 window 宽高 = 该窗口像素(控件按"窗口内"布局, 不相对屏幕画布)
static VOID BuildEntryRef(LayoutEntry& entry)
{
	// 1. 换算基准(mode 决定; WINDOW_* 模式在编辑器语境按屏兜底)
	CONST INT nMode = GeometrySolver::AutoModeIndex(entry.refSize.mode);
	DWORD base = GeometrySolver::EntryBasePx(nMode);
	if (base < 1) { base = 1; }
	// 2. 布局窗口客户区像素(千分比 x 基准/1000; 至少 16px)
	entry.wndW = (DWORD)((unsigned __int64)base * (DWORD)entry.refSize.width / 1000);
	entry.wndH = (DWORD)((unsigned __int64)base * (DWORD)entry.refSize.height / 1000);
	if (entry.wndW < 16) { entry.wndW = 16; }
	if (entry.wndH < 16) { entry.wndH = 16; }
	// 3. 内部参考尺寸(窗口宽高 = 该布局窗口像素)
	AutoSize::SizeInfo<DWORD> si = GeometrySolver::EntryRefSizeInfo(entry.refSize, base, entry.wndW, entry.wndH);
	entry.ref = AutoSize::CreateByFixedSize(si, (AutoSize::AutoSizeMode)nMode);
}

// 函数: FreeEntryLayout
// 描述: 断开会话条目对布局对象的引用(条目数组本身由内存池持有, 不需释放)
// 说明: 文档条目的布局对象 = 预览唯一基座(全部条目共享);
//       基座由 app\PreviewSession::FreePreviewBase 释放, 外壳快照条目由该模块的 FreeShellSnapshot 释放。
static VOID FreeEntryLayout(LayoutEntry& entry)
{
	entry.layout = NULL;
}

// 成员变量
Document* EditorCommands::pDoc = NULL;		// 文档承载者(宿主注入; NULL = 未注入 / 已复位)
// 会话面回调: 命令层不持会话状态 ⇒ 预览基座建立 / 预览同步 / 版本号递增一律经回调请求
pfnEnsurePreviewBase EditorCommands::pfnEnsureBase = NULL;
pfnSyncPreviewLayout EditorCommands::pfnSyncPreview = NULL;
pfnBumpDocRev EditorCommands::pfnBumpRev = NULL;

// 函数: SetDocument
// 描述: 注入文档承载者(NULL = 复位)
VOID EditorCommands::SetDocument(Document* pDocument)
{
	pDoc = pDocument;
}

// 函数: SetSessionHooks
// 描述: 注入会话面回调三件套(可分别传 NULL ⇒ 该项不驱动)
VOID EditorCommands::SetSessionHooks(pfnEnsurePreviewBase pfnEnsure, pfnSyncPreviewLayout pfnSync, pfnBumpDocRev pfnBumpRev)
{
	EditorCommands::pfnEnsureBase = pfnEnsure;
	EditorCommands::pfnSyncPreview = pfnSync;
	EditorCommands::pfnBumpRev = pfnBumpRev;
}

// 函数: SetDocLayoutName
// 描述: 写指定布局条目的显示名(label 与 name 同写; 中间态与条目副本两处)
// 说明: 版本号递增经 pfnBumpRev 回调驱动, 且位置原位保持(两处同写之后、返回之前)。
BOOL EditorCommands::SetDocLayoutName(DWORD docIndex, LPCSTR name)
{
	BOOL bRet = FALSE;
	LayoutNode* pNode = NULL;
	LayoutEntry* pEntry = NULL;
	if (pDoc == NULL || name == NULL || name[0] == '\0')
	{
		return bRet;
	}
	pEntry = pDoc->GetEntry(docIndex);
	if (pEntry == NULL)
	{
		return bRet;
	}
	pNode = pEntry->pNode;
	if (pNode == NULL || pNode->info == NULL)
	{
		return bRet;
	}
	// 布局名 = 条目字段与中间态布局信息同源(两处同写; 中间态才是保存写出物)
	LayoutModel::StrCopy(pNode->info->name, STR_NAME, name);
	LayoutModel::StrCopy(pNode->info->label, STR_LABEL, name);
	pEntry->name = AllocatorHub::CopyString(pNode->info->name);		// 条目副本跟随(内存显示一致)
	pEntry->label = AllocatorHub::CopyString(pNode->info->label);
	if (pfnBumpRev != NULL)
	{
		pfnBumpRev();		// 文档结构版本号递增
	}
	bRet = TRUE;
	return bRet;
}

// 函数: AddDocLayout
// 描述: 追加布局(默认三结构件 窗口/标题栏/容器 + 布局默认 autosize)
BOOL EditorCommands::AddDocLayout(LPCSTR name)
{
	BOOL bRet = FALSE;
	if (pDoc == NULL || AllocatorHub::GetArena() == NULL)
	{
		return bRet;
	}
	LPCSTR szName = (name && name[0]) ? name : u8"新布局";
	CONST DWORD dwNewIndex = pDoc->dwEntries;		// 新布局在文档中的索引
	// 1. 中间态: 追加一个空布局, 再补三枚结构件元素(池 + 链)
	LayoutNode* pNode = LayoutModel::LayoutAdd(pDoc->docState);
	if (pNode == NULL)
	{
		return bRet;
	}
	for (DWORD k = 0; k < 3; k++)
	{
		if (!LayoutModel::ElementInsert(pDoc->docState, dwNewIndex, k))
		{
			return bRet;			// 中间态半成品: 由后续载入/新建整体替换
		}
	}
	pNode = LayoutModel::LayoutAt(pDoc->docState, dwNewIndex);	// 取回新布局节点(随后填充其布局信息)
	if ((pNode == NULL) || (pNode->info == NULL))
	{
		return bRet;
	}
	LayoutInfo* pInfo = pNode->info;
	LayoutModel::StrCopy(pInfo->name, sizeof(pInfo->name), szName);
	LayoutModel::StrCopy(pInfo->label, sizeof(pInfo->label), szName);
	pInfo->IsMaxiable = FALSE;
	pInfo->IsResizeable = TRUE;
	pInfo->refSize = DefaultRefSize();
	// 默认窗口桌面位置: 自左上按 3 列网格错开
	pInfo->wndX = (LONG)(24 + (dwNewIndex % 3) * 48);
	pInfo->wndY = (LONG)(24 + (dwNewIndex / 3) * 48);
	// 结构件几何因子(各轴)
	CONST struct
	{
		CtrlType type;
		LPCSTR label;
		FLOAT wf[8];	// 宽度系数
		FLOAT hf[8];	// 高度系数
		LONG woff;
		LONG hoff;
	} aShell[3] = {
		// 窗口: 铺满(宽=1x窗口宽, 高=1x窗口高)
		{ CTRL_TYPE_WND,   u8"##MAINWND",   {0,0,1,0,0,0,0,0}, {0,0,0,1,0,0,0,0}, 0, 0 },
		// 标题栏: 宽=1x窗口宽, 高=1x控件高
		{ CTRL_TYPE_TITLE, u8"##TITLEBAR",  {0,0,1,0,0,0,0,0}, {0,0,0,0,1,0,0,0}, 0, 0 },
		// 容器: 宽=1x窗口宽, 高=1x窗口高 - 1x控件高(标题栏下方)
		{ CTRL_TYPE_CNTR,  u8"##CONTAINER", {0,0,1,0,0,0,0,0}, {0,0,0,1,-1,0,0,0}, 0, 0 },
	};
	// 三结构件写入中间态元素规格(类型串 / 标签 / 事件 / 宽高系数)
	for (DWORD k = 0; k < 3; k++)
	{
		ElementSpec* es = LayoutModel::ElementAt(pDoc->docState, dwNewIndex, k);
		if (es == NULL)
		{
			return bRet;
		}
		LayoutModel::StrCopy(es->type, sizeof(es->type), GetTypeName(aShell[k].type));
		LayoutModel::StrCopy(es->label, sizeof(es->label), aShell[k].label);
		// 结构件文本: 标题栏文本 = 该布局显示名(标题栏渲染以"文本缓冲"作交互项 ID ⇒ 空文本会触发
		// ImGui 空 ID 断言); 其余结构件无文本
		LayoutModel::StrCopy(es->text, sizeof(es->text),
			(aShell[k].type == CTRL_TYPE_TITLE) ? szName : "");
		// 标题栏事件声明: 新增布局由预览窗口渲染 ⇒ 用预览语义的事件名
		// (只移动并写回窗口位置; 关闭钮无行为), 与文档内既有布局的声明一致
		LayoutModel::StrCopy(es->event, sizeof(es->event),
			(aShell[k].type == CTRL_TYPE_TITLE) ? "PREVIEW_TITLEBAR_EVENT" : "");
		es->id = (int)k;
		for (int c = 0; c < GENFACTOR_NUM; c++)
		{
			es->width.coeff.RefCoeffs[c] = aShell[k].wf[c];
			es->height.coeff.RefCoeffs[c] = aShell[k].hf[c];
		}
		es->width.offset = aShell[k].woff;
		es->height.offset = aShell[k].hoff;
	}
	// 2. 条目数组扩容(内存池追加) + 新条目
	{
		BufferInfo arrbuf = AllocatorHub::GetArena()->Allocate(sizeof(LayoutEntry) * (pDoc->dwEntries + 1));
		if (arrbuf.buffer == NULL)
		{
			return bRet;
		}
		LayoutEntry* pNew = (LayoutEntry*)arrbuf.buffer;
		// 复制既有条目(平凡结构, 直接赋值)
		for (DWORD i = 0; i < pDoc->dwEntries; i++)
		{
			pNew[i] = pDoc->entries[i];
		}
		// 初始化新条目
		LayoutEntry& entry = pNew[pDoc->dwEntries];
		entry = LayoutEntry{};				// 零初始化
		entry.name = AllocatorHub::CopyString(szName);
		entry.label = AllocatorHub::CopyString(szName);
		entry.IsMaxiable = FALSE;
		entry.IsResizeable = TRUE;
		entry.refSize = pInfo->refSize;
		entry.wndX = pInfo->wndX;
		entry.wndY = pInfo->wndY;
		entry.pNode = pNode;				// 中间态节点(元素规格/几何系数)
		// 布局对象 = 预览唯一基座(内容按 active 装配, 见 SyncPreviewLayout)
		entry.layout = NULL;
		// 求解 ref(窗口像素/控件量基准; 控件几何在装配时按本 ref 解算)
		BuildEntryRef(entry);
		// 提交数组并切换激活到新布局
		pDoc->entries = pNew;
		pDoc->dwEntries++;
		pDoc->dwActive = pDoc->dwEntries - 1;
		if (pfnBumpRev != NULL)
		{
			pfnBumpRev();		// 文档结构版本号递增
		}
		// 建立预览基座(若尚未建立)并把新 active 布局的中间态更新进布局对象
		if (pfnEnsureBase != NULL)
		{
			pfnEnsureBase();	// 会话面: 取 / 建预览唯一基座
		}
		if (pfnSyncPreview != NULL)
		{
			pfnSyncPreview();	// 会话面: 中间态 → 预览基座同步
		}
	}
	bRet = TRUE;
	return bRet;
}
// 函数: RemoveDocLayout
// 描述: 删除指定布局(文档保底 1 个; 删除激活项时激活回 0)
BOOL EditorCommands::RemoveDocLayout(DWORD index)
{
	BOOL bRet = FALSE;
	if (pDoc == NULL || pDoc->entries == NULL || pDoc->dwEntries <= 1)
	{
		return bRet;			// 保底 1 个布局
	}
	if (index >= pDoc->dwEntries)
	{
		return bRet;
	}
	// 释放该条目的布局对象
	FreeEntryLayout(pDoc->entries[index]);
	// 中间态: 移除对应布局(池 + 链整体重建)
	if (!LayoutModel::LayoutRemove(pDoc->docState, index))
	{
		return bRet;
	}
	// 数组整体搬迁: 原数组内存保留在内存池(不再复用), 重建一份更短的数组
	{
		BufferInfo arrbuf = AllocatorHub::GetArena()->Allocate(sizeof(LayoutEntry) * (pDoc->dwEntries - 1));
		if (arrbuf.buffer == NULL)
		{
			return bRet;
		}
		LayoutEntry* pNew = (LayoutEntry*)arrbuf.buffer;
		DWORD dst = 0;
		for (DWORD i = 0; i < pDoc->dwEntries; i++)
		{
			if (i == index) { continue; }
			pNew[dst++] = pDoc->entries[i];
		}
		pDoc->entries = pNew;
		pDoc->dwEntries--;
	}
	// 中间态布局池已重建 ⇒ 条目节点按索引重挂
	pDoc->RebindNodes();
	// 激活索引修正(超出则回 0)
	if (pDoc->dwActive >= pDoc->dwEntries)
	{
		pDoc->dwActive = 0;
	}
	if (pfnBumpRev != NULL)
	{
		pfnBumpRev();		// 文档结构版本号递增
	}
	// 条目集合已变 ⇒ 预览基座内容按当前 active 重新装配(删除非 active 条目时结果等价)
	if (pfnSyncPreview != NULL)
	{
		pfnSyncPreview();	// 会话面: 中间态 → 预览基座同步
	}
	bRet = TRUE;
	return bRet;
}
// 函数: MoveDocLayout
// 描述: 拖拽重排: 把指定布局移到新的相对位置(布局顺序唯一真源 = 中间态布局链)
// 说明: 中间态只提供"追加(LayoutAdd) / 移除(LayoutRemove)"与元素级插入 ⇒ 任意重排按
//       ①深拷全部布局(布局信息 + 元素规格)并备好新顺序与条目数组缓冲 ②清空布局链
//       ③按新顺序重建(元素用 ElementInsert 逐个补齐) 实施 —— 全部池分配都在破坏性步骤之前完成,
//       以降低"半成品中间态"风险; 条目数组按同一顺序重排, 激活索引跟随移动。
// 参数: 见 EditorCommands.h
// 返回: BOOL TRUE = 已重排并重新装配预览基座; FALSE = 入参无效 / 索引越界 / 源目标同处 / 池分配失败
BOOL EditorCommands::MoveDocLayout(DWORD dwFrom, DWORD dwDest)
{
	// 变量声明区(深拷载体 + 新顺序 + 条目数组缓冲)
	struct LayoutCopy
	{
		LayoutInfo info;			// 布局信息(定长串 + 参考尺寸 + 窗口位置; 平凡可拷贝)
		ElementSpec* pElems;		// 元素规格数组(池内)
		DWORD dwElems;				// 元素数量
	};
	BOOL bRet = FALSE;
	DWORD dwCount = 0;
	LayoutCopy* pCopies = NULL;
	DWORD* aOrder = NULL;
	LayoutEntry* pNewEntries = NULL;
	if (pDoc == NULL || AllocatorHub::GetArena() == NULL || pDoc->entries == NULL)
	{
		return bRet;
	}
	dwCount = pDoc->dwEntries;
	if (dwCount <= 1 || dwFrom >= dwCount || dwDest >= dwCount || dwFrom == dwDest)
	{
		return bRet;
	}
	// 1. 深拷出: 每个布局的"信息 + 元素规格"(中间态随后被清空 ⇒ 必须先拷出)
	pCopies = (LayoutCopy*)AllocatorHub::GetArena()->Allocate(sizeof(LayoutCopy) * dwCount).buffer;
	if (pCopies == NULL)
	{
		return bRet;
	}
	RtlZeroMemory(pCopies, sizeof(LayoutCopy) * dwCount);
	for (DWORD i = 0; i < dwCount; i++)
	{
		LayoutNode* pNode = LayoutModel::LayoutAt(pDoc->docState, i);
		if (pNode == NULL || pNode->info == NULL)
		{
			return bRet;
		}
		pCopies[i].info = *pNode->info;
		pCopies[i].dwElems = pNode->info->dwElements;
		if (pCopies[i].dwElems > 0)
		{
			pCopies[i].pElems = (ElementSpec*)AllocatorHub::GetArena()->Allocate(sizeof(ElementSpec) * pCopies[i].dwElems).buffer;
			if (pCopies[i].pElems == NULL)
			{
				return bRet;
			}
			for (DWORD e = 0; e < pCopies[i].dwElems; e++)
			{
				ElementSpec* pEs = LayoutModel::ElementAt(pDoc->docState, i, e);
				if (pEs == NULL)
				{
					return bRet;
				}
				pCopies[i].pElems[e] = *pEs;
			}
		}
	}
	// 2. 新顺序 = 去掉源之后把源插到目标位置(其余布局保持相对次序)
	aOrder = (DWORD*)AllocatorHub::GetArena()->Allocate(sizeof(DWORD) * dwCount).buffer;
	pNewEntries = (LayoutEntry*)AllocatorHub::GetArena()->Allocate(sizeof(LayoutEntry) * dwCount).buffer;
	if (aOrder == NULL || pNewEntries == NULL)
	{
		return bRet;
	}
	{
		DWORD dwFill = 0;
		for (DWORD i = 0; i < dwCount; i++)
		{
			if (i == dwFrom) { continue; }
			aOrder[dwFill++] = i;
		}
		for (DWORD k = dwCount - 1; k > dwDest; k--)
		{
			aOrder[k] = aOrder[k - 1];
		}
		aOrder[dwDest] = dwFrom;
	}
	// 3. 清空布局链(逐个移除首个 ⇒ 布局块与其元素块一并回收)
	for (DWORD i = 0; i < dwCount; i++)
	{
		if (!LayoutModel::LayoutRemove(pDoc->docState, 0))
		{
			return bRet;
		}
	}
	// 4. 按新顺序重建布局链(元素用 ElementInsert 逐个补齐后回写规格)
	for (DWORD i = 0; i < dwCount; i++)
	{
		CONST DWORD dwSrc = aOrder[i];
		LayoutNode* pNode = LayoutModel::LayoutAdd(pDoc->docState);
		if (pNode == NULL || pNode->info == NULL)
		{
			return bRet;
		}
		*pNode->info = pCopies[dwSrc].info;
		pNode->info->dwElements = 0;			// 元素计数由 ElementInsert 维护
		for (DWORD e = 0; e < pCopies[dwSrc].dwElems; e++)
		{
			if (!LayoutModel::ElementInsert(pDoc->docState, i, e))
			{
				return bRet;
			}
			ElementSpec* pEs = LayoutModel::ElementAt(pDoc->docState, i, e);
			if (pEs == NULL)
			{
				return bRet;
			}
			*pEs = pCopies[dwSrc].pElems[e];
		}
	}
	// 5. 条目数组按同一顺序重排 + 激活索引跟随移动
	for (DWORD i = 0; i < dwCount; i++)
	{
		pNewEntries[i] = pDoc->entries[aOrder[i]];
	}
	pDoc->entries = pNewEntries;
	{
		CONST DWORD dwActive = pDoc->dwActive;
		DWORD dwNew = dwActive;
		if (dwActive == dwFrom)
		{
			dwNew = dwDest;
		}
		else if (dwFrom < dwActive)
		{
			dwNew = (dwActive > dwDest) ? dwActive : (dwActive - 1);
		}
		else
		{
			dwNew = (dwDest <= dwActive) ? (dwActive + 1) : dwActive;
		}
		pDoc->dwActive = dwNew;
	}
	// 6. 条目节点按索引重挂 + 版本号 + 预览基座按当前 active 重新装配
	pDoc->RebindNodes();
	if (pfnBumpRev != NULL)
	{
		pfnBumpRev();		// 文档结构版本号递增
	}
	if (pfnEnsureBase != NULL)
	{
		pfnEnsureBase();	// 会话面: 取 / 建预览唯一基座
	}
	if (pfnSyncPreview != NULL)
	{
		pfnSyncPreview();	// 会话面: 中间态 → 预览基座同步
	}
	bRet = TRUE;
	return bRet;
}
// 函数: DocIndexOfLayout
// 描述: 按布局指针反查文档索引(唯一基座 ⇒ 当前 active 条目; 未命中 ⇒ -1)
LONG EditorCommands::DocIndexOfLayout(Layout* layout)
{
	LONG nRet = -1;
	if (pDoc == NULL || layout == NULL)
	{
		return nRet;
	}
	// 唯一基座: 被全部条目共享 ⇒ 归属当前 active 条目(与 app 侧反查同法)
	if (pDoc->GetActive() < pDoc->GetCount())
	{
		LayoutEntry* pActive = pDoc->GetEntry(pDoc->GetActive());
		if (pActive != NULL && pActive->layout == layout)
		{
			return (LONG)pDoc->GetActive();
		}
	}
	for (DWORD i = 0; i < pDoc->GetCount(); i++)
	{
		LayoutEntry* pEntry = pDoc->GetEntry(i);
		if (pEntry != NULL && pEntry->layout == layout)
		{
			nRet = (LONG)i;
			break;
		}
	}
	return nRet;
}

// 函数: SpecAt
// 描述: 取指定布局内某文档索引的条目中间态元素规格(未命中 ⇒ NULL)
ElementSpec* EditorCommands::SpecAt(Layout* layout, size_t index)
{
	ElementSpec* pRet = NULL;
	CONST LONG nDoc = DocIndexOfLayout(layout);
	if (nDoc >= 0)
	{
		LayoutEntry* pEntry = pDoc->GetEntry((DWORD)nDoc);
		if (pEntry != NULL)
		{
			pRet = Document::SpecAt(*pEntry, index);
		}
	}
	return pRet;
}

// 函数: InsertSpecAt
// 描述: 在指定布局内插入一枚中间态元素规格(与控件插入同步; 新元素以当前像素表达几何)
BOOL EditorCommands::InsertSpecAt(Layout* layout, size_t index)
{
	if (pDoc == NULL)
	{
		return FALSE;
	}
	CONST LONG nDoc = DocIndexOfLayout(layout);
	if (nDoc < 0)
	{
		return FALSE;
	}
	return pDoc->InsertSpecAt((DWORD)nDoc, layout, index);
}

// 函数: RemoveSpecAt
// 描述: 移除指定布局内一枚中间态元素规格(与控件删除同步)
BOOL EditorCommands::RemoveSpecAt(Layout* layout, size_t index)
{
	if (pDoc == NULL)
	{
		return FALSE;
	}
	CONST LONG nDoc = DocIndexOfLayout(layout);
	if (nDoc < 0)
	{
		return FALSE;
	}
	return pDoc->RemoveSpecAt((DWORD)nDoc, index);
}
