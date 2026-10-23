// ================================
// 文件: FlexHost.cpp
// 说明: 内嵌(flex)窗口装配与查询的实现(全静态成员; 见 FlexHost.h 的文件头)
// ================================

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "app/Application.h"		// 就绪判据 Application::IsReady()(原 data == NULL 判据的等价只读入口)
#include "app/FlexHost.h"
#include "framework/base/mem/AllocatorHub.h"	// 会话分配器(条目 / 活动指针缓存 / 字符串池化)
#include "framework/base/CtrlBuffer.h"		// 控件缓冲(容量口径 / 取址 / 回收)
#include "framework/layout/LayoutBuilder.h"	// 布局对象构建与整枚布局的缓冲回收
#include "framework/layout/LayoutSolver.h"	// 外壳源解析器(中间态 + 逐布局规格)

// 静态成员变量初始化
FlexEntry* FlexHost::pFlexes = NULL;
DWORD FlexHost::dwFlexes = 0;
Layout** FlexHost::ppFlexActive = NULL;
Layout* FlexHost::pShellBase = NULL;

// 内嵌(flex)窗口成员名单(应用层决定"谁是内嵌窗口")
// 说明: 名单内的布局 = 无独立 HWND 的内嵌成员, 由宿主(外壳布局)窗口的渲染帧内以 LayoutRender::RenderFlex
//       渲染; 名单外的布局照旧 = 各自拥有 HWND 的顶层窗口(或文档条目)。
//       成员来源 = 外壳源文档(装配点 = 外壳快照构建) ⇒ 不随被编辑文档的切换 / 新建而变化;
//       装配后默认非活动(按需显示, 见 BuildFlexMembersFromSolver)。
static CONST LPCSTR aFlexMemberNames[] =
{
	GENUI_HINT_LAYOUT_NAME,		// 提示布局(初版唯一内嵌成员; 按需显示 —— 展示提示时置活动)
};

// 函数: SetShellBase
// 描述: 注入"内嵌成员的宿主基座"的外壳布局指针(应用侧在外壳快照构建 / 释放点原位设置; NULL = 复位)
VOID FlexHost::SetShellBase(Layout* pShell)
{
	pShellBase = pShell;
}
// 函数: FindFlexEntry
// 描述: 按窗口布局对象取内嵌成员条目的公开只读投影(未命中返回 NULL; 供条目族统一反查编排)
LayoutEntry* FlexHost::FindFlexEntry(Layout* layout)
{
	LayoutEntry* pRet = NULL;
	// 内嵌(flex)窗口成员: 与外壳快照同性质(独立条目; 几何系数同样存于中间态)
	if (pFlexes != NULL)
	{
		for (DWORD i = 0; i < dwFlexes; i++)
		{
			if (pFlexes[i].entry.layout == layout)
			{
				pRet = &pFlexes[i].entry;
				break;
			}
		}
	}
	return pRet;
}
// 函数: BuildFlexMembersFromSolver
// 描述: 由外壳源文档的解析结果装配全部内嵌(flex)成员(每个成员一份线性只读展开态)
// 说明: ① 成员名单 = 文件内 aFlexMemberNames(应用层决定); 来源 = 外壳源(与外壳快照同一份解析产物)
//          ⇒ 成员不随被编辑文档的切换 / 新建而变化(提示布局不依赖用户正在编辑的数据);
//          节点取自 solver 的中间态(生存期 = 应用线性分配器) ⇒ 文档替换不会使其悬垂;
//       ② 外壳源缺少某成员 ⇒ 跳过并逐条诊断, 不算失败(成员数随之减少);
//       ③ 装配后 bActive 恒 FALSE —— 按需显示(由 SetFlexActive 控制: 显示前置真, 用毕复位);
//       ④ 成员条目/活动指针缓存取自应用线性分配器(随分配器整体释放)。
// 参数:
//		IN	solver:	外壳源的解析器(与外壳快照同一份解析产物)
// 返回:
//		BOOL: TRUE = 装配完成(允许缺少部分或全部成员); FALSE = 应用未就绪 / 分配失败
BOOL FlexHost::BuildFlexMembersFromSolver(LayoutSolver* solver)
{
	// 变量声明(避免 goto 跳过声明)
	BOOL bRet = FALSE;
	FlexEntry* pEntries = NULL;
	Layout** ppActive = NULL;
	CONST FileMidState* pState = NULL;
	DWORD dwLayouts = 0;
	DWORD dwBuilt = 0;
	size_t nNum = 0;
	if (!Application::IsReady() || AllocatorHub::GetArena() == NULL || solver == NULL)
	{
		return bRet;
	}
	FreeFlexLayouts();
	pState = &solver->GetState();		// 外壳源中间态(节点生存期 = 应用线性分配器)
	dwLayouts = solver->GetLayoutsCount();
	nNum = sizeof(aFlexMemberNames) / sizeof(aFlexMemberNames[0]);
	if (nNum == 0)
	{
		return TRUE;		// 名单为空 = 无内嵌成员(合法)
	}
	// 1. 条目数组与活动指针缓存(一次分配; 池内自持)
	{
		BufferInfo buf = AllocatorHub::GetArena()->Allocate(sizeof(FlexEntry) * nNum);
		if (buf.buffer == NULL)
		{
			goto END;
		}
		pEntries = (FlexEntry*)buf.buffer;
		RtlZeroMemory(pEntries, sizeof(FlexEntry) * nNum);
		BufferInfo pbuf = AllocatorHub::GetArena()->Allocate(sizeof(Layout*) * nNum);
		if (pbuf.buffer == NULL)
		{
			goto END;
		}
		ppActive = (Layout**)pbuf.buffer;
		RtlZeroMemory(ppActive, sizeof(Layout*) * nNum);
	}
	// 2. 逐成员在外壳源的中间态布局链上按名命中 ⇒ 线性展开态 + 窗口像素 + 逐控件几何解算
	for (size_t m = 0; m < nNum; m++)
	{
		CONST LayoutNode* pNode = NULL;
		for (DWORD i = 0; i < dwLayouts && pNode == NULL; i++)
		{
			CONST LayoutInfo* spec = solver->GetLayout(i);
			if (spec != NULL && pState != NULL && strcmp(spec->name, aFlexMemberNames[m]) == 0)
			{
				pNode = LayoutModel::LayoutAt(*pState, i);
			}
		}
		if (pNode == NULL || pNode->info == NULL)
		{
			printf("[APP] 内嵌窗口: 外壳源缺少成员 \"%s\" ⇒ 跳过\n", aFlexMemberNames[m]);
			continue;
		}
		// 3. 建成员条目(与外壳快照同法: 线性只读展开态 + 自身参考尺寸 + 中间态节点绑定)
		FlexEntry& fe = pEntries[dwBuilt];
		fe.entry.name = AllocatorHub::CopyString(pNode->info->name[0] ? pNode->info->name : aFlexMemberNames[m]);
		fe.entry.label = AllocatorHub::CopyString(pNode->info->label[0] ? pNode->info->label : aFlexMemberNames[m]);
		fe.entry.IsMaxiable = pNode->info->IsMaxiable;
		fe.entry.IsResizeable = pNode->info->IsResizeable;
		fe.entry.refSize = pNode->info->refSize;
		fe.entry.wndX = pNode->info->wndX;
		fe.entry.wndY = pNode->info->wndY;
		fe.entry.pNode = (LayoutNode*)pNode;					// 中间态节点 = 几何系数存放处(成员只读)
		fe.entry.layout = LayoutBuilder::BuildLayoutFromNode(pNode, FALSE);	// 内嵌成员 = 线性只读展开态
		if (fe.entry.layout == NULL)
		{
			printf("[APP] 内嵌窗口: 成员 \"%s\" 展开失败(含不支持控件 / 分配失败) ⇒ 跳过\n", aFlexMemberNames[m]);
			continue;
		}
		Application::BuildEntryRef(fe.entry);			// 窗口像素 = 该布局 autosize 推导(与顶层窗口同法)
		Application::ApplyEntryGeometry(fe.entry);		// 逐控件几何解算(像素)
			// 提示正文控件(##TEXT_HINT)缓冲扩容: 正文可能较长(文件名 / 路径) ⇒ 按 HINT_BODY_BUF 装配
			// 注: 通用装配给文本类控件的容量 = CTRL_TEXT_BUF(256); 扩容前的旧缓冲在替换时回池
			for (size_t k = 0; k < fe.entry.layout->GetTotalNum(); k++)
			{
				CtrlInfo* pBody = fe.entry.layout->GetCtrlInfo(k);
				if (pBody == NULL || pBody->label == NULL || strcmp(pBody->label, "##TEXT_HINT") != 0)
				{
					continue;
				}
				if (pBody->bufferInfo.size >= HINT_BODY_BUF)
				{
					continue;
				}
				BufferInfo bufBody = AllocatorHub::AllocSmall(HINT_BODY_BUF);
				if (bufBody.buffer == NULL || bufBody.size < HINT_BODY_BUF)
				{
					continue;
				}
				RtlZeroMemory(bufBody.buffer, HINT_BODY_BUF);
				if (pBody->bufferInfo.buffer != NULL)
				{
					strncpy_s((LPSTR)bufBody.buffer, HINT_BODY_BUF, (LPCSTR)pBody->bufferInfo.buffer, _TRUNCATE);
				}
				CtrlBuffer::FreeCtrlBuffer(pBody);			// 旧缓冲回池(新缓冲已取好内容)
				pBody->bufferInfo = bufBody;
			}
		fe.bActive = FALSE;					// 按需显示: 默认不渲染(展示器经 SetFlexActive 置真)
		dwBuilt++;
		printf("[APP] 内嵌窗口: 成员 \"%s\" 已装配(控件=%zu 窗口=%ux%u)\n",
			fe.entry.name, fe.entry.layout->GetTotalNum(), (unsigned)fe.entry.wndW, (unsigned)fe.entry.wndH);
	}
	pFlexes = pEntries;
	ppFlexActive = ppActive;
	dwFlexes = dwBuilt;
	bRet = TRUE;
END:
	return bRet;
}
// 函数: FreeFlexLayouts
// 描述: 释放全部内嵌(flex)成员(布局对象显式析构; 条目与缓存内存由线性分配器持有)
// 说明: 成员为线性展开态 ⇒ 析构只逐控件析构, 不涉及链节点池; 释放后活动集合为空
VOID FlexHost::FreeFlexLayouts()
{
	if (!Application::IsReady())
	{
		return;
	}
	if (pFlexes != NULL)
	{
		for (DWORD i = 0; i < dwFlexes; i++)
		{
			if (pFlexes[i].entry.layout != NULL)
			{
				LayoutBuilder::FreeLayoutCtrlBuffers(*pFlexes[i].entry.layout);	// 析构前回收控件缓冲
				Layout::DestroyLayout(pFlexes[i].entry.layout);
				pFlexes[i].entry.layout = NULL;
			}
		}
		pFlexes = NULL;
	}
	dwFlexes = 0;
	ppFlexActive = NULL;
}
// 函数: GetActiveFlexLayouts
// 描述: 查询指定基座布局当前活动的内嵌(flex)窗口集合(渲染接线入口)
// 说明: ① 归属过滤: 初版内嵌成员的宿主恒为外壳布局(编辑窗口宿主) ⇒ 传入其它基座视为"无活动成员";
//       ② 收集结果写入条目级的指针缓存(渲染期只读; 每次查询重算, 元素有效至下次查询);
//       ③ 无活动成员(或全部非活动) ⇒ 返回 NULL 且数量置 0(调用方据此跳过渲染)。
// 参数:
//		IN	base:		基座(宿主)布局引用
//		OUT	pdwCount:	活动成员数量(无成员置 0; 可为 NULL)
// 返回:
//		Layout: 活动成员的布局对象指针数组(二级指针); 无活动成员返回 NULL
Layout** FlexHost::GetActiveFlexLayouts(Layout& base, DWORD* pdwCount)
{
	Layout** ppRet = NULL;
	DWORD dwCnt = 0;
	if (pdwCount != NULL)
	{
		*pdwCount = 0;
	}
	if (pFlexes == NULL || ppFlexActive == NULL)
	{
		return ppRet;
	}
	if (pShellBase == NULL || &base != pShellBase)
	{
		return ppRet;
	}
	for (DWORD i = 0; i < dwFlexes; i++)
	{
		if (pFlexes[i].bActive && pFlexes[i].entry.layout != NULL)
		{
			ppFlexActive[dwCnt] = pFlexes[i].entry.layout;
			dwCnt++;
		}
	}
	if (dwCnt == 0)
	{
		return ppRet;
	}
	if (pdwCount != NULL)
	{
		*pdwCount = dwCnt;
	}
	ppRet = ppFlexActive;
	return ppRet;
}
// 函数: GetFlexRef
// 描述: 取指定内嵌成员的参考尺寸(渲染需要; 未命中返回缺省样例)
CONST AutoSize& FlexHost::GetFlexRef(Layout* layout)
{
	static AutoSize sNull = AutoSize::CreateByRatio(
		622, 432, 25, 17, 10, 4, AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT);
	if (pFlexes != NULL && layout != NULL)
	{
		for (DWORD i = 0; i < dwFlexes; i++)
		{
			if (pFlexes[i].entry.layout == layout)
			{
				return pFlexes[i].entry.ref;
			}
		}
	}
	return sNull;
}
// 函数: GetFlexCount
// 描述: 内嵌成员总数(装配结果; 供诊断)
DWORD FlexHost::GetFlexCount()
{
	return Application::IsReady() ? dwFlexes : 0;
}
// 函数: SetFlexActive
// 描述: 置某内嵌成员的活动状态(业务段驱动入口; 按布局名命中)
// 参数:
//		IN	name:		成员布局名(装配时取自中间态 name)
//		IN	bActive:	TRUE = 活动(渲染) / FALSE = 非活动(不渲染)
// 返回:
//		BOOL: TRUE = 命中并已设置; FALSE = 未命中 / 入参无效 / 应用未就绪
BOOL FlexHost::SetFlexActive(LPCSTR name, BOOL bActive)
{
	BOOL bRet = FALSE;
	if (pFlexes == NULL || name == NULL)
	{
		return bRet;
	}
	for (DWORD i = 0; i < dwFlexes; i++)
	{
		LPCSTR lpName = pFlexes[i].entry.name;
		if (lpName != NULL && strcmp(lpName, name) == 0)
		{
			pFlexes[i].bActive = bActive;
			bRet = TRUE;
			break;
		}
	}
	return bRet;
}
