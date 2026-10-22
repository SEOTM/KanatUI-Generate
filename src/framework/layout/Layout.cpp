#pragma once
#include <new>
#include <string.h>
#include "framework/layout/Layout.h"

// ================================
// 类名: Layout
// ================================

// ========================
// 静态成员变量初始化
// ========================
Arena* Layout::memArena = NULL;
MemPool* Layout::memPool = NULL;
Layout* Layout::NULLOBJ = NULL;
CtrlInfo Layout::NULLINFO = {};

// 函数: 构造函数
// 描述: 初始化布局对象(按 IsLink 选定储存模式, 并按 num 预置控件)
Layout::Layout(size_t num, BOOL IsLink) :
	IsLinkmode(FALSE),
	qwNumCtrl(0)
{
	ctrls.pCtrlInfos = nullptr;
	if (memArena == NULL)
	{
		return;						// 分配器未注入: 空布局
	}
	// 1. 线性模式: 控件信息连续数组(取自 Arena)
	if (IsLink == FALSE)
	{
		BufferInfo bufInfo = memArena->Allocate(sizeof(CtrlInfo) * num);
		ctrls.pCtrlInfos = static_cast<CtrlInfo*>(bufInfo.buffer);
		if (ctrls.pCtrlInfos == nullptr)
		{
			return;					// 数组分配失败: 空布局
		}
		for (size_t i = 0; i < num; ++i)
		{
			new (&ctrls.pCtrlInfos[i]) CtrlInfo();	// 定位 new 构造
		}
		qwNumCtrl = num;
		return;
	}
	// 2. 链式模式: 链视图(取自 Arena) + num 枚预建节点(取自 MemPool)
	BufferInfo vbuf = memArena->Allocate(sizeof(linkinfo));
	linkinfo* pLink = static_cast<linkinfo*>(vbuf.buffer);
	if (pLink == nullptr || vbuf.size < sizeof(linkinfo))
	{
		return;						// 链视图分配失败 ⇒ 构造失败(保持 FALSE, 由调用方判失败)
	}
	pLink->pFirst = NULL;
	pLink->pLast = NULL;
	pLink->pCursor = NULL;
	pLink->qwCursor = 0;
	ctrls.link = pLink;
	IsLinkmode = TRUE;
	CtrlInfo empty{};
	for (size_t i = 0; i < num; ++i)
	{
		linknode* pNode = NewNode(empty);
		if (pNode == NULL)
		{
			// 节点分配失败: 整链回卷(节点回池)并标为失败态
			linknode* p = ctrls.link->pFirst;
			while (p != NULL)
			{
				linknode* pNext = p->next;
				DeleteNode(p);
				p = pNext;
			}
			ctrls.pCtrlInfos = nullptr;
			qwNumCtrl = 0;
			IsLinkmode = FALSE;
			return;
		}
		pNode->prev = ctrls.link->pLast;
		if (ctrls.link->pLast != NULL)
		{
			ctrls.link->pLast->next = pNode;
		}
		else
		{
			ctrls.link->pFirst = pNode;
		}
		ctrls.link->pLast = pNode;
		qwNumCtrl++;
	}
}
// 函数: NULL 对象构造函数
// 描述: 构造空布局对象(储存视图置空, 模式与计数归零)
Layout::Layout()
{
	ctrls.pCtrlInfos = NULL;
	IsLinkmode = FALSE;
	qwNumCtrl = 0;
}
// 函数: SetAllocators
// 描述: 注入两个分配器(静态, 全进程一份)
VOID Layout::SetAllocators(Arena* arena, MemPool* pool)
{
	memArena = arena;
	memPool = pool;
}
// 函数: CreateLayout
// 描述: 创建布局对象(分配对象内存, 并校验预置数量是否达成)
Layout* Layout::CreateLayout(size_t num, BOOL IsLink)
{
	if (memArena == NULL)
	{
		return nullptr;
	}
	// 使用 Arena(线性分配器)分配布局对象自身
	BufferInfo bufInfo = memArena->Allocate(sizeof(Layout));
	if (bufInfo.buffer == nullptr || bufInfo.size < sizeof(Layout))
	{
		return nullptr; // 分配失败
	}
	// 使用定位 new 创建 Layout 对象(按 IsLink 选定模式)
	Layout* layout = new (bufInfo.buffer) Layout(num, IsLink);
	// 预置数量不足 ⇒ 视作分配失败(内部已回卷): 析构并返回 NULL
	if (layout->GetTotalNum() != num)
	{
		layout->~Layout();
		return nullptr;
	}
	return layout;
}
// 函数: GetNullObject
// 描述: 取空布局引用(静态实例, 首次调用时登记)
Layout& Layout::GetNullObject()
{
	static Layout nullobj;
	NULLOBJ = &nullobj;
	return nullobj;
}
// 函数: DestroyLayout
// 描述: 析构布局对象(入参 NULL 安全; 不归还分配器)
VOID Layout::DestroyLayout(Layout* layout)
{
	if (layout)
	{
		layout->~Layout();
	}
	return;
}

// 函数: 析构函数
// 描述: 释放布局对象(链式节点回池, 线性控件逐个析构; 数组与链视图不归还)
Layout::~Layout()
{
	if (IsLinkmode && ctrls.link != NULL)
	{
		// 链式模式: 逐节点析构控件信息, 并把节点交还链式专用分配器(MemPool; 见 DeleteNode)
		linknode* p = ctrls.link->pFirst;
		while (p != NULL)
		{
			linknode* pNext = p->next;
			DeleteNode(p);
			p = pNext;
		}
		ctrls.link->pFirst = NULL;
		ctrls.link->pLast = NULL;
		ctrls.link->pCursor = NULL;
		ctrls.link->qwCursor = 0;
		qwNumCtrl = 0;
	}
	else if (ctrls.pCtrlInfos != NULL)
	{
		// 线性模式: 调用控件信息数组中每个控件的析构函数
		for (size_t i = 0; i < qwNumCtrl; ++i)
		{
			ctrls.pCtrlInfos[i].~CtrlInfo();
		}
	}
	// 注意: 线性数组 / 链视图 / 布局对象自身均由 memArena 分配, 无需(也无法)单独归还
}

// 函数: NewNode
// 描述: 新建一枚链节点, 并按入参值构造内嵌的控件信息
Layout::linknode* Layout::NewNode(const CtrlInfo& src)
{
	linknode* pNode = NULL;
	BufferInfo buf{ nullptr, 0 };
	// 1. 分配: 取自链式模式专用分配器 MemPool
	if (memPool == NULL || sizeof(linknode) > MemPool::MAX_SMALL_SIZE)
	{
		return NULL;				// 池未绑定(注入缺失)或节点超出池上限 ⇒ 失败
	}
	buf = memPool->Allocate(sizeof(linknode));
	if (buf.buffer == NULL || buf.size < sizeof(linknode))
	{
		return NULL;				// 池分配失败 ⇒ 失败
	}
	// 2. 构造: 节点本身无构造函数 ⇒ 链接字段逐个置位; 控件信息用定位 new 按值拷贝构造
	pNode = (linknode*)buf.buffer;
	new (&pNode->info) CtrlInfo(src);
	pNode->prev = NULL;
	pNode->next = NULL;
	return pNode;
}
// 函数: DeleteNode
// 描述: 析构节点内嵌的控件信息, 并把节点交还小对象池
VOID Layout::DeleteNode(linknode* pNode)
{
	if (pNode == NULL)
	{
		return;
	}
	pNode->info.~CtrlInfo();		// 控件信息析构(纯数据, 无堆内资源)
	if (memPool != NULL && memPool->Contains(pNode) == TRUE)
	{
		memPool->Free((VOID*)pNode);	// 回池: 该块可被后续 NewNode 复用
	}
}
// 函数: UnlinkNode
// 描述: 把节点从链上摘下(只改链接, 不动节点内存)
VOID Layout::UnlinkNode(linknode* pNode)
{
	if (pNode == NULL)
	{
		return;
	}
	if (pNode->prev != NULL)
	{
		pNode->prev->next = pNode->next;
	}
	else
	{
		ctrls.link->pFirst = pNode->next;
	}
	if (pNode->next != NULL)
	{
		pNode->next->prev = pNode->prev;
	}
	else
	{
		ctrls.link->pLast = pNode->prev;
	}
	pNode->prev = NULL;
	pNode->next = NULL;
}

// ================================
// 区域: 控件层次推导
// ================================

// 常量: 层次未解析哨兵
// 说明: 推导期占位(父层尚未定出), 不出现在推导结果里
static constexpr INT32 LAYOUT_LAYER_UNRESOLVED = INT32_MIN;

// 函数: IsShellCtrl
// 描述: 判定是否为结构件(层次恒 0, 无视 parent 声明)
static BOOL IsShellCtrl(CtrlType type)
{
	if (type == CTRL_TYPE_WND || type == CTRL_TYPE_TITLE || type == CTRL_TYPE_CNTR)
	{
		return TRUE;
	}
	return FALSE;
}

// 函数: InitCtrlLayer
// 描述: 层次推导的逐枚初值(结构件归 0 / 其余标为未解析)
VOID Layout::InitCtrlLayer(CtrlInfo& info)
{
	if (IsShellCtrl(info.type) != FALSE)
	{
		info.nLayer = 0;
		return;
	}
	info.nLayer = LAYOUT_LAYER_UNRESOLVED;
}

// 函数: SettleCtrlLayer
// 描述: 为单枚未解析控件定层次(父已定或无父即定值; 父未解析则等下一趟)
BOOL Layout::SettleCtrlLayer(CtrlInfo& info)
{
	BOOL bRet = FALSE;
	CtrlInfo* pParent = NULL;
	INT32 nBase = 0;
	if (info.nLayer != LAYOUT_LAYER_UNRESOLVED)
	{
		return TRUE;			// 无需推导(结构件 / 已定)
	}
	// 1. 按父容器 ID 取父: 无声明(-1) / 父不存在 / 自指 ⇒ 以窗口底档(0)为基
	pParent = (info.parentID >= 0) ? GetCtrlInfoByID(info.parentID) : NULL;
	if (pParent == NULL || pParent == &info)
	{
		nBase = 0;
		bRet = TRUE;
	}
	// 2. 父尚未解析 ⇒ 本趟不定(等下一趟; 成环时永远等不到 ⇒ 由兜底收口)
	else if (pParent->nLayer == LAYOUT_LAYER_UNRESOLVED)
	{
		bRet = FALSE;
	}
	// 3. 父已定 ⇒ 继承父层
	else
	{
		nBase = pParent->nLayer;
		bRet = TRUE;
	}
	// 定值: 基值 + 自身类型步进(允许为负: 域逐级下沉; 层号钳制在换算侧 CtrlLayerOf)
	if (bRet != FALSE)
	{
		info.nLayer = nBase + CtrlLayerStep(info.type);
	}
	return bRet;
}

// 函数: DeriveCtrlLayers
// 描述: 整体重推全部控件的层次(按 parentID 链求值; 两种储存模式都支持)
BOOL Layout::DeriveCtrlLayers()
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	BOOL bChanged = FALSE;
	DWORD dwPass = 0;
	size_t i = 0;
	// 1. 链式: 初值 → 按父链收敛 → 成环兜底
	if (IsLinkmode)
	{
		linknode* p = NULL;
		if (ctrls.link == NULL || ctrls.link->pFirst == NULL)
		{
			return bRet;			// 链视图缺失 / 空链: 无可整理对象
		}
		for (p = ctrls.link->pFirst; p != NULL; p = p->next)
		{
			InitCtrlLayer(p->info);
		}
		// 每趟定出"父已定或无父"的控件; 趟数由父链深度决定(至多走一遍控件总数)
		for (dwPass = 0; dwPass < (DWORD)qwNumCtrl; ++dwPass)
		{
			bChanged = FALSE;
			for (p = ctrls.link->pFirst; p != NULL; p = p->next)
			{
				if (p->info.nLayer != LAYOUT_LAYER_UNRESOLVED)
				{
					continue;
				}
				if (SettleCtrlLayer(p->info) != FALSE)
				{
					bChanged = TRUE;
				}
			}
			if (bChanged == FALSE)
			{
				break;
			}
		}
		// 仍未解析者 = 父链成环 ⇒ 视作无父容器, 以窗口底档为基
		for (p = ctrls.link->pFirst; p != NULL; p = p->next)
		{
			if (p->info.nLayer == LAYOUT_LAYER_UNRESOLVED)
			{
				p->info.nLayer = CtrlLayerStep(p->info.type);
			}
		}
		bRet = TRUE;
		return bRet;
	}
	// 2. 线性: 就地按下标推进(推导口径与链式相同, 仅遍历方式不同)
	if (ctrls.pCtrlInfos == NULL || qwNumCtrl == 0)
	{
		return bRet;			// 线性空数组: 无可整理对象
	}
	for (i = 0; i < qwNumCtrl; ++i)
	{
		InitCtrlLayer(ctrls.pCtrlInfos[i]);
	}
	for (dwPass = 0; dwPass < (DWORD)qwNumCtrl; ++dwPass)
	{
		bChanged = FALSE;
		for (i = 0; i < qwNumCtrl; ++i)
		{
			if (ctrls.pCtrlInfos[i].nLayer != LAYOUT_LAYER_UNRESOLVED)
			{
				continue;
			}
			if (SettleCtrlLayer(ctrls.pCtrlInfos[i]) != FALSE)
			{
				bChanged = TRUE;
			}
		}
		if (bChanged == FALSE)
		{
			break;
		}
	}
	for (i = 0; i < qwNumCtrl; ++i)
	{
		if (ctrls.pCtrlInfos[i].nLayer == LAYOUT_LAYER_UNRESOLVED)
		{
			ctrls.pCtrlInfos[i].nLayer = CtrlLayerStep(ctrls.pCtrlInfos[i].type);
		}
	}
	bRet = TRUE;
	return bRet;
}

// ================================
// 区域: 控件坐标推导
// 说明: 有效坐标 = 声明坐标 + 父链声明坐标之和 ——
//   ①累加只读声明坐标(CtrlInfo::posDeclared): 声明值一经几何解算即不再改写 ⇒ 本推导幂等, 可反复重推,
//     与链序无关(父可声明在子之后);
//   ②结构件不作父基: WND 的坐标是桌面位置(不是布局坐标) ⇒ 累加在 WND 处止步;
//     TITLE / CNTR 照常计入 —— 它们的坐标是窗口本地坐标, 而内容控件的声明坐标正是以内容容器原点为零点
//     (开内容容器时容器件已提供裁剪与偏移);
//   ③父缺失 / 自指 / 父链成环 ⇒ 累加到此为止(不追异常链, 与层次推导同口径);
//   ④结构件自身的有效坐标恒等于其声明坐标: 推导不写结构件(WND 的位置记录在运行期会被窗口级交互改写)。
// ================================

// 函数: ParentPosSum
// 描述: 求一枚控件父链上的声明坐标之和(跳过 WND; 自指与成环一律按无父处理)
static vec2 ParentPosSum(Layout& layout, CONST CtrlInfo& info)
{
	vec2 sum(0.0f, 0.0f);
	CONST size_t qwTotal = layout.GetTotalNum();
	size_t qwGuard = 0;
	BOOL IsCyclic = FALSE;
	int nParent = info.parentID;
	while (nParent >= 0)
	{
		CtrlInfo* pParent = layout.GetCtrlInfoByID(nParent);
		if (pParent == NULL || pParent->type == CTRL_TYPE_WND)
		{
			break;					// 父缺失 / 桌面坐标(WND) ⇒ 累加止步
		}
		if (pParent == (CONST CtrlInfo*)&info || qwGuard >= qwTotal)
		{
			IsCyclic = TRUE;		// 回到自身 / 上溯步数越界 ⇒ 成环(不取链上半截的偏和)
			break;
		}
		sum = sum + pParent->posDeclared;
		nParent = pParent->parentID;
		qwGuard++;
	}
	if (IsCyclic != FALSE)
	{
		sum = vec2(0.0f, 0.0f);		// 成环 ⇒ 视同无父容器(与层次推导的兜底同口径)
	}
	return sum;
}

// 函数: DeriveCtrlPositions
// 描述: 整体重推全部内容控件的有效坐标(按 parentID 链累加声明坐标; 两种储存模式同一口径)
BOOL Layout::DeriveCtrlPositions()
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	size_t i = 0;
	// 1. 链式: 逐节点写有效坐标(结构件跳过)
	if (IsLinkmode)
	{
		linknode* p = NULL;
		if (ctrls.link == NULL || ctrls.link->pFirst == NULL)
		{
			return bRet;			// 链视图缺失 / 空链: 无可整理对象
		}
		for (p = ctrls.link->pFirst; p != NULL; p = p->next)
		{
			if (IsShellCtrl(p->info.type) != FALSE)
			{
				continue;			// 结构件: 有效坐标 = 声明坐标, 不参与累加
			}
			p->info.position = p->info.posDeclared + ParentPosSum(*this, p->info);
		}
		bRet = TRUE;
		return bRet;
	}
	// 2. 线性: 就地按下标推进(口径与链式相同, 仅遍历方式不同)
	if (ctrls.pCtrlInfos == NULL || qwNumCtrl == 0)
	{
		return bRet;				// 线性空数组: 无可整理对象
	}
	for (i = 0; i < qwNumCtrl; ++i)
	{
		if (IsShellCtrl(ctrls.pCtrlInfos[i].type) != FALSE)
		{
			continue;				// 结构件: 有效坐标 = 声明坐标, 不参与累加
		}
		ctrls.pCtrlInfos[i].position = ctrls.pCtrlInfos[i].posDeclared + ParentPosSum(*this, ctrls.pCtrlInfos[i]);
	}
	bRet = TRUE;
	return bRet;
}

// 函数: NodeAt
// 描述: 取指定索引处的链节点(按游标就近定位)
Layout::linknode* Layout::NodeAt(size_t index)
{
	linknode* p = NULL;
	if (IsLinkmode == FALSE || index >= qwNumCtrl)
	{
		return NULL;
	}
	// 1. 选定起点并就近走到目标
	if (ctrls.link->pCursor != NULL)
	{
		p = ctrls.link->pCursor;
		size_t i = ctrls.link->qwCursor;
		while ((i < index) && (p != NULL))
		{
			p = p->next;
			i++;
		}
		while ((i > index) && (p != NULL))
		{
			p = p->prev;
			i--;
		}
	}
	else if (index < (qwNumCtrl / 2))
	{
		p = ctrls.link->pFirst;
		for (size_t i = 0; (i < index) && (p != NULL); ++i)
		{
			p = p->next;
		}
	}
	else
	{
		p = ctrls.link->pLast;
		for (size_t i = qwNumCtrl - 1; (i > index) && (p != NULL); --i)
		{
			p = p->prev;
		}
	}
	// 2. 命中即更新游标(未命中保持原游标, 不影响正确性)
	if (p != NULL)
	{
		ctrls.link->pCursor = p;
		ctrls.link->qwCursor = index;
	}
	return p;
}

// 函数: IsLinkMode
// 描述: 查询是否链式储存模式
BOOL Layout::IsLinkMode() const
{
	return IsLinkmode;
}

// 函数: SetCtrlInfo
// 描述: 按索引按值覆盖控件信息
BOOL Layout::SetCtrlInfo(size_t index, const CtrlInfo& info)
{
	BOOL ret = FALSE;
	CtrlInfo* p = NULL;
	if (index < qwNumCtrl)
	{
		if (IsLinkmode)
		{
			linknode* pNode = NodeAt(index);
			p = (pNode != NULL) ? &pNode->info : NULL;
		}
		else
		{
			p = &ctrls.pCtrlInfos[index];
		}
		if (p != NULL)
		{
			*p = info;
			ret = TRUE;
		}
	}
	return ret;
}

// 函数: GetCtrlInfo
// 描述: 按索引取控件信息指针
CtrlInfo* Layout::GetCtrlInfo(size_t index)
{
	CtrlInfo* ret = nullptr;
	if (index < qwNumCtrl)
	{
		if (IsLinkmode)
		{
			linknode* pNode = NodeAt(index);
			ret = (pNode != NULL) ? &pNode->info : nullptr;
		}
		else
		{
			ret = &ctrls.pCtrlInfos[index];
		}
	}
	return ret;
}

// 函数: GetTotalNum
// 描述: 取控件总数
size_t Layout::GetTotalNum() const
{
	return qwNumCtrl;
}
// 函数: AddCtrlInfo
// 描述: 在指定位置插入一枚控件(链式专属; 线性模式恒失败)
BOOL Layout::AddCtrlInfo(size_t index, const CtrlInfo& info)
{
	BOOL bRet = FALSE;
	// 非链式模式: 始终返回 FALSE(线性模式不支持插入/删除)
	if (IsLinkmode && index <= qwNumCtrl)
	{
		// 插入参数按"声明坐标"解释: 调用方(应用层/预览)历来只写 position ⇒ 入链前把它同步为声明坐标,
		// 有效坐标随后由 DeriveCtrlPositions 按父链重算(见下方"坐标推导")
		CtrlInfo node = info;
		node.posDeclared = info.position;
		linknode* pNode = NewNode(node);
		if (pNode != NULL)
		{
			// 挂接: index==0 头插; index==qwNumCtrl 尾插; 否则插到 index 处节点之前(双向链 O(1))
			if (index == 0)
			{
				pNode->next = ctrls.link->pFirst;
				if (ctrls.link->pFirst != NULL)
				{
					ctrls.link->pFirst->prev = pNode;
				}
				ctrls.link->pFirst = pNode;
				if (ctrls.link->pLast == NULL)
				{
					ctrls.link->pLast = pNode;
				}
			}
			else if (index == qwNumCtrl)
			{
				pNode->prev = ctrls.link->pLast;
				if (ctrls.link->pLast != NULL)
				{
					ctrls.link->pLast->next = pNode;
				}
				ctrls.link->pLast = pNode;
				if (ctrls.link->pFirst == NULL)
				{
					ctrls.link->pFirst = pNode;
				}
			}
			else
			{
				linknode* pAt = NodeAt(index);
				if (pAt == NULL)
				{
					DeleteNode(pNode);		// 理论不可达(索引已校验): 回卷刚建的节点
					return FALSE;
				}
				pNode->next = pAt;
				pNode->prev = pAt->prev;
				if (pAt->prev != NULL)
				{
					pAt->prev->next = pNode;
				}
				else
				{
					ctrls.link->pFirst = pNode;
				}
				pAt->prev = pNode;
			}
			// 游标修正: 新节点落在游标之前/处 ⇒ 游标节点在链上的下标后移一位
			if ((ctrls.link->pCursor != NULL) && (index <= ctrls.link->qwCursor))
			{
				ctrls.link->qwCursor++;
			}
			qwNumCtrl++;
			// 层次推导: 层次按父容器 ID 链求值, 与链序无关 ⇒ 插入后整体重推
			//   (新节点可能成为链上任意位置控件的父; 与 DeriveCtrlLayers 同一口径, 幂等)
			DeriveCtrlLayers();
			// 坐标推导: 同一条父链决定坐标累加 ⇒ 与层次一起整体重推(幂等: 只读声明坐标)
			DeriveCtrlPositions();
			bRet = TRUE;
		}
	}
	return bRet;
}
// 函数: RemoveCtrlInfo
// 描述: 移除指定位置的控件(链式专属; 线性模式恒失败)
BOOL Layout::RemoveCtrlInfo(size_t index)
{
	BOOL bRet = FALSE;
	// 非链式模式: 始终返回 FALSE
	if (IsLinkmode && index < qwNumCtrl)
	{
		linknode* pNode = NodeAt(index);
		if (pNode != NULL)
		{
			// 游标修正(在摘链之前按旧下标判定)
			if (ctrls.link->pCursor == pNode)
			{
				ctrls.link->pCursor = NULL;
				ctrls.link->qwCursor = 0;
			}
			else if ((ctrls.link->pCursor != NULL) && (index < ctrls.link->qwCursor))
			{
				ctrls.link->qwCursor--;
			}
			UnlinkNode(pNode);
			DeleteNode(pNode);			// 析构控件信息 + 节点回池
			qwNumCtrl--;
			bRet = TRUE;
		}
	}
	return bRet;
}
// 函数: GetCtrlInfoRef
// 描述: 按索引取控件信息引用(越界返回静态空信息)
CtrlInfo& Layout::GetCtrlInfoRef(size_t index)
{
	if (index < qwNumCtrl)
	{
		if (IsLinkmode)
		{
			linknode* pNode = NodeAt(index);
			if (pNode != NULL)
			{
				return pNode->info;
			}
			return NULLINFO;
		}
		return ctrls.pCtrlInfos[index];
	}
	else
	{
		return NULLINFO;
	}
}
// 函数: GetCtrlInfoByID
// 描述: 按控件 ID 取可写控件指针(链序首个命中; 未找到返回 NULL)
CtrlInfo* Layout::GetCtrlInfoByID(int id)
{
	for (size_t i = 0; i < qwNumCtrl; i++)
	{
		CtrlInfo* pCtrl = GetCtrlInfo(i);
		if (pCtrl != NULL && pCtrl->id == id)
		{
			return pCtrl;
		}
	}
	return NULL;
}
// 函数: GetLayoutBase
// 描述: 按类型取结构件(只在前三个槽位内查找)
CtrlInfo* Layout::GetLayoutBase(CtrlType type)
{
	CtrlInfo* ret = nullptr;
	const size_t qwScan = (qwNumCtrl < 3) ? qwNumCtrl : 3;	// 只扫前三个
	for (size_t i = 0; (i < qwScan) && (ret == nullptr); ++i)
	{
		CtrlInfo* p = GetCtrlInfo(i);
		if (p != NULL && p->type == type)
		{
			ret = p;
		}
	}
	return ret;
}
// 函数: GetShellNum
// 描述: 统计结构件个数(WND / TITLE / CNTR 中存在者)
size_t Layout::GetShellNum()
{
	size_t ret = 0;
	if (this->GetLayoutBase(CTRL_TYPE_WND))
	{
		ret++;
	}
	if (this->GetLayoutBase(CTRL_TYPE_TITLE))
	{
		ret++;
	}
	if (this->GetLayoutBase(CTRL_TYPE_CNTR))
	{
		ret++;
	}
	return ret;
}
// 函数: IsInvalid
// 描述: 判定布局是否为无效引用
BOOL Layout::IsInvalid()
{
	BOOL ret = FALSE;
	CtrlInfo* pWnd = this->GetLayoutBase(CTRL_TYPE_WND);
	if (this == NULLOBJ)
	{
		ret = TRUE;
	}
	else if (!(pWnd && this->GetLayoutBase(CTRL_TYPE_CNTR)))
	{
		ret = TRUE;
	}
	else if (pWnd->size == vec2{ 0,0 })
	{
		ret = TRUE;
	}
	return ret;
}

