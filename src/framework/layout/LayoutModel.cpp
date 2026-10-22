#include "framework/layout/LayoutModel.h"
#include "common/MemoryManager.h"
#include <Windows.h>
#include <string.h>

// ================================
// 文件: LayoutModel.cpp
// 描述: 布局数据模型(中间态)的字段工具与"块 + 链"操作实现(接口契约见同目录 LayoutModel.md)
// ================================

// ========================
// 静态成员变量初始化
// ========================
static Arena* s_memArena = NULL;	// 线性分配器(块超出池上限时的回落分配)
static MemPool* s_memPool = NULL;	// 小对象池(元素块 / 布局块的常规来源)

// ========================
// 区域: 块结构(文件内部)
// ========================

// 结构: ElementBlock
struct ElementBlock
{
	ElementNode node;	// 链节点(node.data 指向本块内的 spec)
	ElementSpec spec;	// 元素规格
};

// 结构: LayoutBlock
struct LayoutBlock
{
	LayoutNode node;	// 链节点(node.info 指向本块内的 info)
	LayoutInfo info;	// 布局信息
};

// ========================
// 区域: 块分配与回收(文件内部)
// ========================

// 函数: AllocBlock
// 描述: 从池分配块(超池上限或池分配失败时回落线性分配器)
static BufferInfo AllocBlock(size_t size)
{
	BufferInfo ret{ nullptr, 0 };
	if (s_memPool != nullptr && size <= MemPool::MAX_SMALL_SIZE)
	{
		ret = s_memPool->Allocate(size);
	}
	if (ret.buffer == nullptr && s_memArena != nullptr)
	{
		ret = s_memArena->Allocate(size);
	}
	return ret;
}

// 函数: FreeBlock
// 描述: 归还池内块(线性分配器承载的块不在此回收)
static VOID FreeBlock(BufferInfo info)
{
	if (info.buffer != nullptr && s_memPool != nullptr && s_memPool->Contains(info.buffer) == TRUE)
	{
		s_memPool->Free(info);
	}
}

// 函数: MakeElementBlock
// 描述: 构造一枚元素块(节点在块首, data 指向同块规格; 除父容器缺省 -1 外各字段为零值)
static ElementBlock* MakeElementBlock()
{
	BufferInfo buf = AllocBlock(sizeof(ElementBlock));
	if (buf.buffer == nullptr)
	{
		return nullptr;
	}
	ElementBlock* pBlock = (ElementBlock*)buf.buffer;
	pBlock->spec = ElementSpec{};
	pBlock->spec.parentID = -1;		// 无父容器(0 是合法 ID ⇒ 缺省不能取 0)
	pBlock->node.data = &pBlock->spec;
	pBlock->node.next = nullptr;
	return pBlock;
}

// 函数: MakeLayoutBlock
// 描述: 构造一枚零值布局块(节点在块首, info 指向同块信息)
static LayoutBlock* MakeLayoutBlock()
{
	BufferInfo buf = AllocBlock(sizeof(LayoutBlock));
	if (buf.buffer == nullptr)
	{
		return nullptr;
	}
	LayoutBlock* pBlock = (LayoutBlock*)buf.buffer;
	pBlock->info = LayoutInfo{};
	pBlock->node.info = &pBlock->info;
	pBlock->node.elements = nullptr;
	pBlock->node.next = nullptr;
	return pBlock;
}

// 函数: SetAllocators
// 描述: 注入块分配器(静态, 全进程一份; 传 NULL = 解绑)
VOID LayoutModel::SetAllocators(Arena* arena, MemPool* pool)
{
	s_memArena = arena;
	s_memPool = pool;
}

// ================================
// 区域: 字段工具(与解算无关的纯字段访问)
// ================================

// 函数: RefSizeValueAt
// 描述: 按参考量序号读经验参数值(序号越界返回 0)
INT LayoutModel::RefSizeValueAt(CONST RefSize& refSize, INT index)
{
	switch (index)
	{
	case 0:		return refSize.width;
	case 1:		return refSize.height;
	case 2:		return refSize.controlHeight;
	case 3:		return refSize.fontSize;
	case 4:		return refSize.space;
	case 5:		return refSize.padding;
	default:	return 0;
	}
}

// 函数: SetRefSizeValueAt
// 描述: 按参考量序号写经验参数值(序号越界不写入)
VOID LayoutModel::SetRefSizeValueAt(RefSize& refSize, INT index, INT value)
{
	switch (index)
	{
	case 0:		refSize.width = value;			break;
	case 1:		refSize.height = value;			break;
	case 2:		refSize.controlHeight = value;	break;
	case 3:		refSize.fontSize = value;		break;
	case 4:		refSize.space = value;			break;
	case 5:		refSize.padding = value;		break;
	default:									break;
	}
}

// 函数: DefaultRefSize
// 描述: 取经验参数默认样例(屏高模式 + 千分比 622/432/25/17/10/4)
RefSize LayoutModel::DefaultRefSize()
{
	RefSize ret{};
	StrCopy(ret.mode, sizeof(ret.mode), "AUTO_SIZE_BY_SCREEN_HEIGHT");
	ret.unit = 0;			// 0 = 千分比(比例推导像素)
	ret.width = 622;		// 窗口宽度(千分比)
	ret.height = 432;		// 窗口高度
	ret.controlHeight = 25;	// 控件高度
	ret.fontSize = 17;		// 字体大小
	ret.space = 10;			// 控件间隙
	ret.padding = 4;		// 内部边距
	return ret;
}

// ================================
// 区域: 定长字符串
// ================================

// 函数: StrCopy
// 描述: 把字符串安全拷入定长数组(超长按容量截断)
BOOL LayoutModel::StrCopy(char* dst, size_t cap, LPCSTR src)
{
	if (dst == NULL || cap == 0)
	{
		return FALSE;
	}
	dst[0] = '\0';
	if (src == NULL)
	{
		return TRUE;
	}
	size_t len = strlen(src);
	if (len >= cap)
	{
		// 超长: 拷满 cap - 1 字节并补齐结尾符(截断)
		memcpy(dst, src, cap - 1);
		dst[cap - 1] = '\0';
		return FALSE;
	}
	memcpy(dst, src, len + 1);
	return TRUE;
}

// ================================
// 区域: 池与链
// ================================

// 函数: StateInit
// 描述: 初始化一份中间态(按布局数与各布局元素数逐块建立两条链)
BOOL LayoutModel::StateInit(FileMidState& state, DWORD dwLayouts, CONST DWORD* ElementCounts)
{
	state = FileMidState{};		// 先清零(失败时保持空态)
	for (DWORD i = 0; i < dwLayouts; i++)
	{
		if (LayoutAdd(state) == NULL)
		{
			StateClear(state);		// 回卷已建块
			return FALSE;
		}
		CONST DWORD dwCount = (ElementCounts != NULL) ? ElementCounts[i] : 0;
		for (DWORD j = 0; j < dwCount; j++)
		{
			if (!ElementInsert(state, i, j))
			{
				StateClear(state);	// 回卷已建块
				return FALSE;
			}
		}
	}
	return TRUE;
}

// 函数: StateClear
// 描述: 释放中间态持有的全部块并把 state 复位为空态
VOID LayoutModel::StateClear(FileMidState& state)
{
	LayoutNode* pLayout = state.layouts.next;
	while (pLayout != NULL)
	{
		LayoutNode* pNextLayout = pLayout->next;		// 先记后继(回收后不可再读)
		ElementNode* pElem = pLayout->elements;
		while (pElem != NULL)
		{
			ElementNode* pNextElem = pElem->next;
			FreeBlock(BufferInfo{ pElem, sizeof(ElementBlock) });
			pElem = pNextElem;
		}
		FreeBlock(BufferInfo{ pLayout, sizeof(LayoutBlock) });
		pLayout = pNextLayout;
	}
	state = FileMidState{};
}

// 函数: LayoutAdd
// 描述: 在布局链尾追加一个空布局(分配一个布局块并尾插)
LayoutNode* LayoutModel::LayoutAdd(FileMidState& state)
{
	LayoutBlock* pBlock = MakeLayoutBlock();
	if (pBlock == nullptr)
	{
		return NULL;
	}
	// 尾插: 自哨兵头逐枚推进到链尾
	LayoutNode* pTail = &state.layouts;
	while (pTail->next != NULL)
	{
		pTail = pTail->next;
	}
	pTail->next = &pBlock->node;
	state.dwLayouts++;
	return &pBlock->node;
}

// 函数: LayoutRemove
// 描述: 移除指定布局(解链后回收其元素块与布局块)
BOOL LayoutModel::LayoutRemove(FileMidState& state, DWORD layoutIndex)
{
	BOOL bRet = FALSE;
	// 1. 定位前驱与目标(越界 ⇒ 原状态不变)
	LayoutNode* pPrev = &state.layouts;
	for (DWORD i = 0; (i < layoutIndex) && (pPrev->next != NULL); i++)
	{
		pPrev = pPrev->next;
	}
	LayoutNode* pNode = pPrev->next;
	if (pNode == NULL)
	{
		return bRet;
	}
	// 2. 解链
	pPrev->next = pNode->next;
	// 3. 回收本布局的元素块与布局块
	CONST DWORD dwElems = (pNode->info != NULL) ? pNode->info->dwElements : 0;
	ElementNode* pElem = pNode->elements;
	while (pElem != NULL)
	{
		ElementNode* pNext = pElem->next;
		FreeBlock(BufferInfo{ pElem, sizeof(ElementBlock) });
		pElem = pNext;
	}
	FreeBlock(BufferInfo{ pNode, sizeof(LayoutBlock) });
	// 4. 更新计数
	if (state.dwLayouts > 0)
	{
		state.dwLayouts--;
	}
	state.dwElements -= (dwElems < state.dwElements) ? dwElems : state.dwElements;
	bRet = TRUE;
	return bRet;
}

// 函数: LayoutCount
// 描述: 遍历布局链取布局数量
DWORD LayoutModel::LayoutCount(CONST FileMidState& state)
{
	DWORD dwRet = 0;
	CONST LayoutNode* p = state.layouts.next;
	while (p != NULL)
	{
		dwRet++;
		p = p->next;
	}
	return dwRet;
}

// 函数: LayoutAt
// 描述: 按布局索引取布局节点(越界返回 NULL)
LayoutNode* LayoutModel::LayoutAt(CONST FileMidState& state, DWORD layoutIndex)
{
	LayoutNode* pRet = state.layouts.next;
	for (DWORD i = 0; (i < layoutIndex) && (pRet != NULL); i++)
	{
		pRet = pRet->next;
	}
	return pRet;
}

// 函数: LayoutFirst
// 描述: 遍历入口: 取首个布局节点(空文档返回 NULL)
LayoutNode* LayoutModel::LayoutFirst(CONST FileMidState& state)
{
	return state.layouts.next;
}

// 函数: LayoutNext
// 描述: 遍历推进: 取下一布局节点(链尾或入参 NULL 返回 NULL)
LayoutNode* LayoutModel::LayoutNext(CONST LayoutNode* pNode)
{
	return (pNode != NULL) ? pNode->next : NULL;
}

// 函数: ElementFirst
// 描述: 遍历入口: 取指定布局的元素链首(无元素或入参 NULL 返回 NULL)
ElementNode* LayoutModel::ElementFirst(CONST LayoutNode* pNode)
{
	return (pNode != NULL) ? pNode->elements : NULL;
}

// 函数: ElementNext
// 描述: 遍历推进: 取下一元素节点(链尾或入参 NULL 返回 NULL)
ElementNode* LayoutModel::ElementNext(CONST ElementNode* pElem)
{
	return (pElem != NULL) ? pElem->next : NULL;
}

// 函数: ElementAt
// 描述: 按布局索引与元素索引取元素规格(越界返回 NULL)
ElementSpec* LayoutModel::ElementAt(CONST FileMidState& state, DWORD layoutIndex, DWORD elementIndex)
{
	ElementSpec* pRet = NULL;
	LayoutNode* pNode = LayoutAt(state, layoutIndex);
	if (pNode != NULL)
	{
		ElementNode* pElem = pNode->elements;
		for (DWORD i = 0; (i < elementIndex) && (pElem != NULL); i++)
		{
			pElem = pElem->next;
		}
		if (pElem != NULL)
		{
			pRet = pElem->data;
		}
	}
	return pRet;
}

// 函数: ElementInsert
// 描述: 在指定位置插入一枚空元素(分配元素块并就地链接)
BOOL LayoutModel::ElementInsert(FileMidState& state, DWORD layoutIndex, DWORD elementIndex)
{
	BOOL bRet = FALSE;
	LayoutNode* pNode = LayoutAt(state, layoutIndex);
	if ((pNode == NULL) || (pNode->info == NULL))
	{
		return bRet;
	}
	if (elementIndex > pNode->info->dwElements)
	{
		return bRet;			// 插入位置越界(合法范围 0..元素数)
	}
	// 1. 先分配新块(失败 ⇒ 原状态不变)
	ElementBlock* pBlock = MakeElementBlock();
	if (pBlock == nullptr)
	{
		return bRet;
	}
	// 2. 定位插入点(ppLink 指向"待改写的链指针存储位置"; 位置 0 时即布局头的元素链头)
	ElementNode** ppLink = &pNode->elements;
	for (DWORD i = 0; (i < elementIndex) && (*ppLink != NULL); i++)
	{
		ppLink = &((*ppLink)->next);
	}
	// 3. 链接并更新计数
	pBlock->node.next = *ppLink;
	*ppLink = &pBlock->node;
	pNode->info->dwElements++;
	state.dwElements++;
	bRet = TRUE;
	return bRet;
}

// 函数: ElementRemove
// 描述: 移除指定元素(解链后回收其元素块)
BOOL LayoutModel::ElementRemove(FileMidState& state, DWORD layoutIndex, DWORD elementIndex)
{
	BOOL bRet = FALSE;
	LayoutNode* pNode = LayoutAt(state, layoutIndex);
	if ((pNode == NULL) || (pNode->info == NULL) || (elementIndex >= pNode->info->dwElements))
	{
		return bRet;
	}
	// 1. 定位待移除节点的前驱链指针
	ElementNode** ppLink = &pNode->elements;
	for (DWORD i = 0; (i < elementIndex) && (*ppLink != NULL); i++)
	{
		ppLink = &((*ppLink)->next);
	}
	ElementNode* pElem = *ppLink;
	if (pElem == NULL)
	{
		return bRet;
	}
	// 2. 解链并更新计数
	*ppLink = pElem->next;
	pNode->info->dwElements--;
	if (state.dwElements > 0)
	{
		state.dwElements--;
	}
	// 3. 回收块(块地址即节点地址)
	FreeBlock(BufferInfo{ pElem, sizeof(ElementBlock) });
	bRet = TRUE;
	return bRet;
}
