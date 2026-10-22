// ================================
// 文件: PreviewSession.cpp
// 说明: 预览基座与帧间同步的实现(全静态成员; 见 PreviewSession.h 的文件头)
// ================================

#include <windows.h>
#include <stdio.h>
#include "app/Application.h"		// 装配共用件 BuildEntryRef / ApplyEntryGeometry(公开接口)
#include "doc/RawAttributes.h"		// 装配沿用的旁挂表 SetRawType 一族
#include "app/PreviewSession.h"
#include "app/HostContext.h"		// 预览内容控件点击处理器(装配时改绑)
#include "app/DocumentIO.h"			// 解析器工厂 NewSolver(外壳快照两入口)
#include "app/FlexHost.h"			// 内嵌成员装配与释放(外壳快照同源同生命周期)
#include "doc/Document.h"			// 承载者句柄解引用(条目数组 / 中间态 / 激活索引)
#include "common/AllocatorHub.h"	// 会话分配器(条目分配 / 串拷贝 / 池回收)
#include "framework/base/BasicDesign.h"
#include "framework/base/ControlSpec.h"	// 类型身份表(GetTypeEnum)
#include "framework/base/CtrlBuffer.h"		// 控件缓冲(数据结构 / 取址 / 回收)
#include "framework/dispatch/FrameEvent.h"	// 帧间事件队列(同步请求挂载 / 帧内时机判定)
#include "framework/layout/GeometrySolver.h"
#include "framework/layout/Layout.h"
#include "framework/layout/LayoutBuilder.h"	// 中间态节点 → 布局对象 + 控件装配 + 整枚布局缓冲回收
#include "framework/layout/LayoutModel.h"
#include "framework/layout/LayoutSolver.h"	// 解析器(外壳快照两入口的入参类型)

// 静态成员变量初始化
Layout* PreviewSession::pPreviewBase = NULL;
Document* PreviewSession::pDoc = NULL;
LONG* PreviewSession::pnDocRev = NULL;
Arena* PreviewSession::pArena = NULL;
LayoutEntry* PreviewSession::pShell = NULL;
BOOL PreviewSession::bShellValid = FALSE;

// 函数: SetSession
// 描述: 注入文档承载者 / 文档版本号地址 / 会话分配器句柄(NULL = 复位)
VOID PreviewSession::SetSession(Document* pDocument, LONG* pnRev, Arena* pArenaHub)
{
	pDoc = pDocument;
	pnDocRev = pnRev;
	pArena = pArenaHub;
}
// 函数: GetPreviewBase
// 描述: 取预览唯一基座(未建立 ⇒ NULL)
Layout* PreviewSession::GetPreviewBase()
{
	return pPreviewBase;
}
// 函数: ResetPreviewBase
// 描述: 复位基座指针(只丢弃指针, 不释放对象)
VOID PreviewSession::ResetPreviewBase()
{
	pPreviewBase = NULL;
}
// 函数: EnsurePreviewBase
// 描述: 取预览布局唯一基座(不存在则建立空链式布局), 并把全部文档条目的 layout 指向它
// 说明: 基座 = 文档条目的布局对象(全部条目共享一个对象, 内容随 active 由 SyncPreviewLayout 装配);
//       建立成"空链式布局" —— 结构件与内容控件按中间态追加, 对象身份自始至终不变;
//       取用路径(GetDocLayout(index) / GetActiveLayout())保持"任意条目都返回有效对象"。
// 返回:
//		Layout*: 基座指针; NULL = 未初始化或分配失败
Layout* PreviewSession::EnsurePreviewBase()
{
	if (pDoc == NULL)
	{
		return NULL;
	}
	if (pPreviewBase == NULL)
	{
		pPreviewBase = Layout::CreateLayout(0, TRUE);
		if (pPreviewBase == NULL)
		{
			printf("[APP] 预览布局基座: 建立失败(链式布局分配失败)\n");
			return NULL;
		}
		printf("[APP] 预览布局基座: 已建立(链式空布局; 内容按中间态装配)\n");
	}
	// 全部文档条目共享同一基座(条目数组内存由内存池持有 ⇒ 只改指针)
	if (pDoc->entries != NULL)
	{
		for (DWORD i = 0; i < pDoc->dwEntries; i++)
		{
			pDoc->entries[i].layout = pPreviewBase;
		}
	}
	return pPreviewBase;
}
// 函数: FreePreviewBase
// 描述: 释放预览布局唯一基座(仅 Release 时调用)
// 说明: 基座对象由内存池持有, 此处只做析构(链节点回小对象池)并断开全部条目引用
VOID PreviewSession::FreePreviewBase()
{
	if (pDoc == NULL || pPreviewBase == NULL)
	{
		return;
	}
	LayoutBuilder::FreeLayoutCtrlBuffers(*pPreviewBase);		// 析构前回收全部控件的自身缓冲
	Layout::DestroyLayout(pPreviewBase);
	pPreviewBase = NULL;
	if (pDoc->entries != NULL)
	{
		for (DWORD i = 0; i < pDoc->dwEntries; i++)
		{
			pDoc->entries[i].layout = NULL;
		}
	}
}
// 函数: EnsurePreviewBaseHook
// 描述: 取预览唯一基座(不存在则建立) —— 命令层会话回调的实现
// 返回:
//		Layout*: 基座地址; NULL = 应用未就绪
Layout* PreviewSession::EnsurePreviewBaseHook()
{
	if (pDoc == NULL)
	{
		return NULL;
	}
	return EnsurePreviewBase();
}
// 函数: BumpDocRevHook
// 描述: 递增文档结构变更版本号 —— 命令层会话回调的实现
VOID PreviewSession::BumpDocRevHook()
{
	if (pnDocRev != NULL)
	{
		(*pnDocRev)++;
	}
}
// 函数: SyncPreviewLayout
// 描述: 把中间态当前内容部分更新到预览布局唯一基座(布局对象身份不变, 不重建对象)
// 说明: ⓪ 选中态表达件在链外(编辑器自持的视图叠加件, 见 src\editor\SelectionOverlay)
//          ⇒ 本函数不涉及它, 也无须"先摘除、后重建";
//       ① 结构件区(基座前 GetShellNum() 槽)就地刷新几何/标签/文本, 事件绑定原样保留;
//       ② 内容控件整体卸载后, 按中间态元素链重新全部追加(含缓冲分配/事件绑定/旁挂表);
//       ③ 每次调用都按当前中间态重装内容控件(调用时机由外部决定);
//       ④ 部分失败逐条诊断并继续 —— 已装配部分保留(尽力而为), 返回 FALSE 表示未完整完成。
// 返回:
//		BOOL: TRUE = 完整同步; FALSE = 无文档/无中间态节点/存在失败项
BOOL PreviewSession::SyncPreviewLayout()
{
	BOOL bRet = FALSE;
	DWORD dwDiag = 0;					// 失败项计数(逐条诊断; 最后据此定返回值)
	if (pDoc == NULL || pDoc->entries == NULL || pDoc->dwEntries == 0)
	{
		return bRet;
	}
	DWORD dwIndex = (pDoc->dwActive < pDoc->dwEntries) ? pDoc->dwActive : 0;
	LayoutEntry& entry = pDoc->entries[dwIndex];
	CONST LayoutNode* pNode = entry.pNode;
	if (pNode == NULL || pNode->info == NULL)
	{
		printf("[APP] 预览布局同步: 条目 %u 无中间态节点 ⇒ 跳过\n", (unsigned)dwIndex);
		return bRet;
	}
	Layout* pBase = EnsurePreviewBase();
	if (pBase == NULL || pBase->IsLinkMode() == FALSE)
	{
		printf("[APP] 预览布局同步: 预览基座不可用(须为链式布局) ⇒ 跳过\n");
		return bRet;
	}
	CONST AutoSize::SizeInfo<FLOAT> ref = entry.ref.GetSizeInfo<FLOAT>();
	CONST DWORD dwElems = pNode->info->dwElements;
	// 1. 结构件区 = 基座当前已装配的结构件槽位(首次为空链 ⇒ 0 ⇒ 结构件也随之走下方"追加"装配)
	size_t qwKeep = pBase->GetShellNum();
	if (qwKeep > (size_t)dwElems)
	{
		qwKeep = (size_t)dwElems;
	}
	// 2. 卸载内容区(自尾向头; 旁挂表条目随索引一并前移)
	{
		size_t qwTotal = pBase->GetTotalNum();
		for (size_t i = qwTotal; i > qwKeep; i--)
		{
			size_t idx = i - 1;
			// 卸载前回收该控件的自身缓冲(池块随卸载回池, 供后续装配复用)
			CtrlBuffer::FreeCtrlBuffer(pBase->GetCtrlInfo(idx));
			if (!pBase->RemoveCtrlInfo(idx))
			{
				printf("[APP] 预览布局同步: 卸载内容控件失败(索引 %zu)\n", idx);
				dwDiag++;
				break;
			}
			RawAttributes::RemoveRawTypeAt(pBase, idx);
			RawAttributes::RemoveRawTextAt(pBase, idx);
		}
	}
	// 3. 结构件刷新: 几何 + 标签 + 文本(事件绑定 pEventHandler 保留不动)
	{
		CONST ElementNode* pElem = pNode->elements;
		for (size_t j = 0; (j < qwKeep) && (pElem != NULL); j++, pElem = pElem->next)
		{
			CONST ElementSpec* es = pElem->data;
			CtrlInfo* pCtrl = pBase->GetCtrlInfo(j);
			if (es == NULL || pCtrl == NULL)
			{
				continue;
			}
			pCtrl->id = es->id;
			pCtrl->parentID = es->parentID;
			pCtrl->type = GetTypeEnum(es->type);
			pCtrl->label = es->label;			// 重指当前中间态池(池重建后旧指针失效)
			GeometrySolver::ApplySpecGeometry(*pCtrl, es, ref);
			CtrlBuffer::WriteCtrlTextToBuffer(pCtrl, es->text);
			RawAttributes::SetRawType(pBase, j, es->type);
			if (pCtrl->bufferInfo.buffer == NULL && es->text[0] != '\0')
			{
				RawAttributes::SetRawText(pBase, j, es->text);
			}
		}
	}
	// 4. 内容控件按中间态重新全部追加(共 dwElems 枚; 装配 = 缓冲分配 + 事件绑定 + 几何解算)
	{
		CONST ElementNode* pElem = pNode->elements;
		for (size_t j = 0; (j < qwKeep) && (pElem != NULL); j++)
		{
			pElem = pElem->next;				// 跳过结构件区(已在第 3 步就地刷新)
		}
		for (size_t j = qwKeep; (j < (size_t)dwElems) && (pElem != NULL); j++, pElem = pElem->next)
		{
			CONST ElementSpec* es = pElem->data;
			if (es == NULL)
			{
				continue;
			}
			CtrlInfo info{};
			if (!LayoutBuilder::BuildCtrlInfoFromSpec(info, es, TRUE))
			{
				CtrlBuffer::FreeCtrlBuffer(&info);		// 装配未完成时可能已分配部分缓冲(下拉类的选项数组) ⇒ 回池
				printf("[APP] 预览布局同步: 控件 %zu 装配失败(类型 %s) ⇒ 跳过\n", j, es->type);
				dwDiag++;
				continue;
			}
			// 注入"预览布局内容控件的点击事件处理器": 只改绑原本绑了真实事件(BuildCtrlInfoFromSpec
			// 按文档 event 名解析出的处理器非空)的内容控件 ⇒ 预览窗口里点它们只产生"点击记录",
			// 不触发真实业务事件(保存/新建/打开一类不得由预览驱动)。
			// 两条边界: ①结构件(WND/TITLE/CNTR)一律不覆盖 —— 首次装配时结构件也走本追加路径,
			// 故此处按类型排除(不能只看索引), 预览标题栏的拖动等既有绑定必须保留;
			// ②无事件绑定的控件(pEventHandler == NULL)保持 NULL —— 控件库的 TEXT/RECT
			// 只在绑定事件时才补"覆盖控件矩形的无绘制可交互项"(Controls::CtrlClickCode), 若给它们
			// 也挂上处理器, 那枚先提交的覆盖项会抢走其上方可交互控件(如 INPUT)的命中(见
			// 的实测缺陷) ⇒ 这类控件的点击由渲染出口判定(见 Render.cpp)
			CONST pfnCtrlEvent pfnInject = HostContext::GetInjectClickHandler();
			if (pfnInject != NULL && info.pEventHandler != NULL &&
				info.type != CTRL_TYPE_WND && info.type != CTRL_TYPE_TITLE && info.type != CTRL_TYPE_CNTR)
			{
				info.pEventHandler = pfnInject;
			}
			// 委托父容器: 预览环境不解析真实委托(避免点它触发业务事件), 按"有点击响应"处理 ⇒
			// 改绑点击记录器; 无记录器(空记录)时保持无响应
			if (CtrlEventIsEntrustParent(es->event) != FALSE && pfnInject != NULL)
			{
				info.pEventHandler = pfnInject;
			}
			info.position.x = GeometrySolver::FactorAbsPx(es->x, ref);
			info.position.y = GeometrySolver::FactorAbsPx(es->y, ref);
			info.size.x = GeometrySolver::FactorAbsPx(es->width, ref);
			info.size.y = GeometrySolver::FactorAbsPx(es->height, ref);
			info.posDeclared = info.position;	// 声明坐标 = 解算值; 有效坐标由父链重推(见 4-ter)
			// 追加到尾部(索引 = 当前控件数): 中间若有元素被跳过, 也不会越出可插入区间
			if (!pBase->AddCtrlInfo(pBase->GetTotalNum(), info))
			{
				CtrlBuffer::FreeCtrlBuffer(&info);		// 未入链的装配产物回池
				printf("[APP] 预览布局同步: 追加控件 %zu 失败 ⇒ 停止追加\n", j);
				dwDiag++;
				break;
			}
			RawAttributes::SetRawType(pBase, j, es->type);
			if (info.bufferInfo.buffer == NULL && es->text[0] != '\0')
			{
				RawAttributes::SetRawText(pBase, j, es->text);
			}
		}
		// 4-bis. 层次与坐标整理: 上一循环逐枚尾插 ⇒ 各控件的 nLayer / 有效坐标只按"插入当刻的前序"求值;
		//        这里在全部内容控件装配完成后统一重推一遍, 使同一批装配的结果与链序无关
		//        (口径见 ControlSpec.h 的"控件层次与层号换算"区; 幂等, 重复调用不漂移)。
		//        注: 即使本批出现诊断(dwDiag != 0)也必须执行 —— 已入链控件的有效坐标依赖本步
		//        (声明坐标已写、父链尚未累加 ⇒ 不重推会使这些控件停在未累加的坐标上)。
		pBase->DeriveCtrlLayers();
		pBase->DeriveCtrlPositions();
		// 4-ter. 窗口位置记录: 基座 WND 件的 position = 本条目的窗口桌面位置(wndX/wndY)
		//        说明: 结构件几何刷新与坐标重推都按"布局内的声明坐标"写 WND 件的 position(声明值恒为 0),
		//              但预览窗口的桌面位置来自条目字段(标题栏拖动经 SetDocWndPos 改写) ⇒ 此处以条目
		//              字段为唯一真源覆盖, 并把 posDeclared 同步为同值(重推坐标只读声明坐标, 不会把
		//              位置记录改回旧值)。渲染出口每帧以该记录为参数经 AcquireWnd 结算窗口几何。
		{
			CtrlInfo* pWnd = pBase->GetLayoutBase(CTRL_TYPE_WND);
			if (pWnd != NULL)
			{
				pWnd->position.x = (FLOAT)entry.wndX;
				pWnd->position.y = (FLOAT)entry.wndY;
				pWnd->posDeclared = pWnd->position;
			}
		}
	}
	// 5. 条目引用指向基座(取用路径 GetDocLayout(index) 恒返回有效对象)
	entry.layout = pBase;
	bRet = (dwDiag == 0);
	return bRet;
}


// 函数: RequestPreviewSync
// 描述: 请求把中间态同步到预览布局唯一基座(编辑动作改完中间态后的统一入口)
// 说明: 渲染帧内 ⇒ 挂到帧间事件队列(帧末排空时执行; 同一帧内多次请求由队列合并为一枚);
//       帧末排空执行中 ⇒ 当场同步(此时两窗口均不在渲染帧内)。
BOOL PreviewSession::RequestPreviewSync()
{
	// 1. 渲染帧内: 挂载等待帧末排空(同一帧内多次请求被队列合并)
	if (FrameEvent::IsInFrame())
	{
		return FrameEvent::Post(PreviewSession::OnPreviewSyncRequest, NULL, 0, 0, NULL);
	}
	// 2. 帧末排空执行中: 当场同步
	return PreviewSession::SyncPreviewLayout();
}

// 函数: OnPreviewSyncRequest
// 描述: 帧间同步请求的处理函数(帧末排空时执行)
// 参数:
//		IN	obj/oper/param/data: 帧间事件四元组(本请求不使用: 同步作用于全局唯一基座)
VOID PreviewSession::OnPreviewSyncRequest(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	PreviewSession::SyncPreviewLayout();
}

// 函数: RecalcEntryGeometry
// 描述: 依条目 autosize 重建 ref 并(仅 active 条目)按中间态更新预览布局对象(autosize 变更后调用)
VOID PreviewSession::RecalcEntryGeometry(DWORD index)
{
	LayoutEntry* pEntry = (pDoc != NULL) ? pDoc->GetEntry(index) : NULL;
	if (pEntry == NULL)
	{
		return;
	}
	Application::BuildEntryRef(*pEntry);
	// 唯一基座只承载当前 active 条目的控件 ⇒ 仅当该条目为 active 时按新 ref 更新布局对象;
	// 非 active 条目在此只重建自身 ref(其几何在切为 active 时由 SyncPreviewLayout 求解)
	if (index == pDoc->GetActive())
	{
		SyncPreviewLayout();
	}
}

// ================================
// ================================

// 函数: LoadShellSnapshot
// 描述: 从布局文件解析 name==GenUIEditor 布局为"外壳快照"(编辑窗口渲染源, 会话内固定)
//       文件缺失/无该名/解析失败均不致命: 快照无效(调用方 main 据此诊断 + 启动中止)
BOOL PreviewSession::LoadShellSnapshot(LPCSTR path)
{
	BOOL bRet = FALSE;
	LayoutSolver* solver = NULL;
	if (!IsReady() || path == NULL)
	{
		return bRet;
	}
	// 解析文件(与 LoadDocument 同款临时解析器, 规格字符串进内存池可长期持有; JSON / bin 由嗅探分派)
	solver = DocumentIO::NewSolver();
	if (solver == NULL || !solver->LoadLayoutsAuto(path))
	{
		return bRet;
	}
	return BuildShellSnapshotFromSolver(solver);
}
// 函数: LoadShellSnapshotFromMemory
// 描述: 同 LoadShellSnapshot, 但数据来自内存缓冲(启动加载链命中内置资源时走此入口)
BOOL PreviewSession::LoadShellSnapshotFromMemory(LPVOID pData, DWORD cbData)
{
	BOOL bRet = FALSE;
	LayoutSolver* solver = NULL;
	if (!IsReady() || pData == NULL || cbData == 0)
	{
		return bRet;
	}
	solver = DocumentIO::NewSolver();
	if (solver == NULL || !solver->LoadLayoutsAuto((LPCVOID)pData, cbData))
	{
		return bRet;
	}
	return BuildShellSnapshotFromSolver(solver);
}
// 函数: BuildShellSnapshotFromSolver
// 描述: 由已解析的布局规格构建外壳快照(取 name==GenUIEditor 的布局)
// 说明: 只解析并持有快照条目的规格->布局(与编辑文档条目相互独立, 互不串改)
BOOL PreviewSession::BuildShellSnapshotFromSolver(LayoutSolver* solver)
{
	BOOL bRet = FALSE;
	DWORD dwNum = 0;
	LayoutEntry* pNew = NULL;
	if (!IsReady() || solver == NULL)
	{
		goto END;
	}
	// 1. 释放既有快照(重复载入场景)
	FreeShellSnapshot();
	dwNum = solver->GetLayoutsCount();
	// 2. 查找保留名布局 GenUIEditor(并取其中间态布局节点)
	{
		CONST LayoutNode* pNode = NULL;
		for (DWORD i = 0; i < dwNum; i++)
		{
			CONST LayoutInfo* spec = solver->GetLayout(i);
			if (spec != NULL && lstrcmpA(spec->name, GENUI_SHELL_LAYOUT_NAME) == 0)
			{
				pNode = LayoutModel::LayoutAt(solver->GetState(), i);
				break;
			}
		}
		if (pNode == NULL || pNode->info == NULL)
		{
			goto END;		// 无 GenUIEditor: 快照无效
		}
		// 3. 构建快照条目(独立 LayoutEntry, 由内存池持有)
		BufferInfo ebuf = AllocatorHub::GetArena()->Allocate(sizeof(LayoutEntry));
		if (ebuf.buffer == NULL)
		{
			goto END;
		}
		pNew = (LayoutEntry*)ebuf.buffer;
		*pNew = LayoutEntry{};		// 零初始化
		pNew->name = AllocatorHub::CopyString(pNode->info->name[0] ? pNode->info->name : GENUI_SHELL_LAYOUT_NAME);
		pNew->label = AllocatorHub::CopyString(pNode->info->label[0] ? pNode->info->label : GENUI_SHELL_LAYOUT_NAME);
		pNew->IsMaxiable = pNode->info->IsMaxiable;
		pNew->IsResizeable = pNode->info->IsResizeable;
		pNew->refSize = pNode->info->refSize;
		pNew->wndX = pNode->info->wndX;
		pNew->wndY = pNode->info->wndY;
		pNew->pNode = (LayoutNode*)pNode;	// 中间态节点 = 几何系数存放处(外壳只读, 不参与增删)
		pNew->layout = LayoutBuilder::BuildLayoutFromNode(pNode, FALSE);	// 外壳快照 = 只读展开态(线性)
		if (pNew->layout == NULL)
		{
			goto END;		// 构建失败(如含不支持控件): 快照无效
		}
		// 注: 几何系数已在中间态元素规格内(ElementSpec 的 x/y/width/height) ⇒ 无需逐枚写入
		// 4. 窗口像素 = 该布局 autosize 推导(与文档条目/预览窗口同法), 并即时求解全部控件几何 ——
		//    编辑窗口(hWndEdit)的建窗尺寸即取此值(窗口大小符合 GenUI.json 描述);
		//    建窗后 SetShellClientSize 再按实际客户区幂等校正(WS_POPUP 无边框, 二者一致)
		Application::BuildEntryRef(*pNew);
		Application::ApplyEntryGeometry(*pNew);
	}
	pShell = pNew;
	bShellValid = TRUE;
	printf("[APP] 外壳快照: GenUIEditor 已载入 (布局=%s 控件=%zu)\n",
		pShell->name, pShell->layout->GetTotalNum());
	// 内嵌(flex)窗口成员: 由同一份外壳源装配(与外壳快照同源同生命周期; 会话级固定)
	// ⇒ 不随被编辑文档的切换 / 新建变化; 装配后默认非活动 —— 按需显示(SetFlexActive)。
	FlexHost::SetShellBase(pShell->layout);
	FlexHost::BuildFlexMembersFromSolver(solver);
	bRet = TRUE;
END:
	return bRet;
}
// 函数: IsReady
// 描述: 会话面是否就绪(私有判据件) —— 外壳快照区的"应用未就绪"判据
BOOL PreviewSession::IsReady()
{
	return (pArena != NULL && AllocatorHub::GetArena() != NULL) ? TRUE : FALSE;
}
// 函数: FreeShellSnapshot
// 描述: 释放外壳快照条目(布局对象销毁; 条目内存仍由内存池持有)
VOID PreviewSession::FreeShellSnapshot()
{
	if (!IsReady())
	{
		return;
	}
	if (pShell)
	{
		// 快照布局对象随应用线性分配器释放 ⇒ 其控件缓冲先回池(重载外壳快照时不遗留池块)
		if (pShell->layout != NULL)
		{
			LayoutBuilder::FreeLayoutCtrlBuffers(*pShell->layout);
		}
		pShell->layout = NULL;
		pShell = NULL;
	}
	// 内嵌(flex)窗口成员与外壳快照同生命周期 ⇒ 一并释放(重复载入场景)
	//   (原位复位外壳(基座)布局指针)
	FlexHost::SetShellBase(NULL);
	FlexHost::FreeFlexLayouts();
	bShellValid = FALSE;
}
// 函数: ResetShellSnapshot
// 描述: 复位外壳快照状态(释放 + 清有效位; 应用侧 Release 原位调用)
VOID PreviewSession::ResetShellSnapshot()
{
	FreeShellSnapshot();
}
// 函数: GetShellEntry
// 描述: 取外壳快照条目指针(未载入 ⇒ NULL)
LayoutEntry* PreviewSession::GetShellEntry()
{
	return pShell;
}
// 函数: GetShellValid
// 描述: 外壳快照是否有效(GenUI.json 载入且含 GenUIEditor 布局)
// 注意: 判据只看"已成功解析出 GenUIEditor 布局", 与几何是否已求解(SetShellClientSize)无关
BOOL PreviewSession::GetShellValid()
{
	BOOL bRet = FALSE;
	if (pArena && bShellValid && pShell && pShell->layout)
	{
		bRet = TRUE;
	}
	return bRet;
}
// 函数: GetShellLayout
// 描述: 获取外壳布局(编辑窗口渲染源); 无效时返回空对象
Layout& PreviewSession::GetShellLayout()
{
	if (GetShellValid())
	{
		return *(pShell->layout);
	}
	return Layout::GetNullObject();
}
// 函数: GetShellRef
// 描述: 获取外壳布局参考尺寸; 无效时返回缺省样例
CONST AutoSize& PreviewSession::GetShellRef()
{
	static AutoSize sNull = AutoSize::CreateByRatio(
		622, 432, 25, 17, 10, 4, AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT);
	if (GetShellValid())
	{
		return pShell->ref;
	}
	return sNull;
}
// 函数: GetShellRefSize
// 描述: 获取外壳布局参考尺寸/经验参数; 无效时返回缺省样例
CONST RefSize& PreviewSession::GetShellRefSize()
{
	static RefSize sNull = LayoutModel::DefaultRefSize();
	if (GetShellValid())
	{
		return pShell->refSize;
	}
	return sNull;
}
// 函数: GetShellWndRect
// 描述: 取外壳快照布局的窗口矩形(尺寸 = 该布局 autosize 推导)
// 说明: 与 GetDocWndRect(预览窗口)同法 —— 宿主 main 据此创建编辑窗口;
//       快照无效(文件无 GenUIEditor)时返回 FALSE, 由调用方兜底
BOOL PreviewSession::GetShellWndRect(LONG* x, LONG* y, DWORD* w, DWORD* h)
{
	BOOL bRet = FALSE;
	if (GetShellValid())
	{
		if (x) { *x = pShell->wndX; }
		if (y) { *y = pShell->wndY; }
		if (w) { *w = pShell->wndW; }
		if (h) { *h = pShell->wndH; }
		bRet = TRUE;
	}
	return bRet;
}
// 函数: SetShellClientSize
// 描述: 编辑窗口建窗后调用: 以实际客户区像素重建外壳 ref 并重算外壳布局几何
// 说明: 窗口尺寸来源 = 该布局 autosize(GenUI.json 描述), 本函数为"按实际客户区校正"一步
//       (WS_POPUP 无边框 ⇒ 与建窗尺寸相同, 幂等); 系统夹取/DPI 致二者不等时以实际客户区为准
VOID PreviewSession::SetShellClientSize(LONG width, LONG height)
{
	if (pShell == NULL || pShell->layout == NULL)
	{
		return;
	}
	if (width < 16) { width = 16; }
	if (height < 16) { height = 16; }
	LayoutEntry& shell = *pShell;
	CONST INT nMode = GeometrySolver::AutoModeIndex(shell.refSize.mode);
	DWORD base = GeometrySolver::EntryBasePx(nMode);
	if (base < 1) { base = 1; }
	// 窗口客户区像素 = 实际客户区(记录 wndW/wndH 供窗口几何使用)
	shell.wndW = (DWORD)width;
	shell.wndH = (DWORD)height;
	// 内部参考尺寸: 窗口宽高 = 实际客户区; 经验量按 refSize x 基准推导
	AutoSize::SizeInfo<DWORD> si = GeometrySolver::EntryRefSizeInfo(shell.refSize, base, shell.wndW, shell.wndH);
	shell.ref = AutoSize::CreateByFixedSize(si, (AutoSize::AutoSizeMode)nMode);
	// 重算外壳布局全部控件几何
	Application::ApplyEntryGeometry(shell);
}
