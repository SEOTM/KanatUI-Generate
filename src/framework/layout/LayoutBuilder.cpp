// ================================
// 文件: LayoutBuilder.cpp
// 描述: 布局对象构建与深拷贝的实现(接口契约见同目录 LayoutBuilder.md)
// ================================
#include <windows.h>
#include <string.h>						// memcpy(类型缓冲的逐字节复制)
#include "framework/layout/LayoutBuilder.h"
#include "common/AllocatorHub.h"		// 池内分配与字符串拷贝原语(AllocBuffer / AllocSmall / CopyString)
#include "framework/base/BasicDesign.h"
#include "framework/base/CtrlBuffer.h"		// 控件缓冲约定与构造回收(PrepareCtrlBuffer / FreeCtrlBuffer)
#include "framework/dispatch/UIEventHandler.h"	// event 名 → 处理函数绑定表

// ================================
// 区域: 文件内部辅助
// ================================

// 函数: ElementSpecAt
// 描述: 按索引取元素链上的元素规格(越界返回 NULL)
static CONST ElementSpec* ElementSpecAt(CONST LayoutNode* pNode, DWORD index)
{
	CONST ElementSpec* pRet = NULL;
	DWORD dwIndex = 0;
	for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, dwIndex++)
	{
		if (dwIndex == index)
		{
			pRet = pElem->data;
			break;
		}
	}
	return pRet;
}

// 函数: ElementIndexByID
// 描述: 按元素 ID 在元素链上取链序索引(未找到返回 FALSE)
static BOOL ElementIndexByID(CONST LayoutNode* pNode, int id, DWORD* pIndex)
{
	BOOL bRet = FALSE;
	DWORD dwIndex = 0;
	for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, dwIndex++)
	{
		CONST ElementSpec* es = pElem->data;
		if (es != NULL && es->id == id)
		{
			if (pIndex != NULL)
			{
				*pIndex = dwIndex;
			}
			bRet = TRUE;
			break;
		}
	}
	return bRet;
}

// ================================
// 区域: 公开接口实现区
// ================================

// 函数: BuildLayoutFromNode
// 描述: 依中间态布局节点构建一枚布局对象(逐元素装配控件信息与类型缓冲)
Layout* LayoutBuilder::BuildLayoutFromNode(CONST LayoutNode* pNode, BOOL IsLink)
{
	Layout* ret = NULL;
	Layout* layout = NULL;
	// 校验中间态节点
	if (pNode == NULL || pNode->info == NULL || pNode->info->dwElements == 0)
	{
		goto END;
	}
	// 创建布局对象(储存模式在此确定)
	layout = Layout::CreateLayout(pNode->info->dwElements, IsLink);
	if (layout == NULL)
	{
		goto END;
	}
	// 逐个元素设置基本信息与缓冲
	{
		DWORD j = 0;
		for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, j++)
		{
			CONST ElementSpec* es = pElem->data;
			if (es == NULL)
			{
				continue;
			}
			CtrlInfo info{};
			// 控件信息装配(标识 / 标签 / 类型 / 事件绑定 / 类型缓冲) —— 口径与布局层的单点装配实现同源
			if (!LayoutBuilder::BuildCtrlInfoFromSpec(info, es, TRUE))
			{
				CtrlBuffer::FreeCtrlBuffer(&info);				// 装配未完成时可能已分配部分缓冲(下拉类的选项数组) ⇒ 回池
				LayoutBuilder::FreeLayoutCtrlBuffers(*layout);	// 已装配控件的缓冲回池(析构之前)
				Layout::DestroyLayout(layout);
				layout = NULL;
				goto END;
			}
			// 注: 几何系数不驻控件(CtrlInfo 只有像素几何) —— 其值存于中间态元素规格
			//     (ElementSpec 的 x/y/width/height), 由 ApplyEntryGeometry 解算。
			// 注: 文本内容即控件缓冲自身(CtrlTextBuffer 取址); 载入期只需把文本写进缓冲
			//     (见上方 BuildCtrlInfoFromSpec 转调的 CtrlBuffer::PrepareCtrlBuffer)。
			layout->SetCtrlInfo(j, info);
		}
	}
	// 层次整理: 两种储存模式都要做(本路径不过 AddCtrlInfo ⇒ 层次不自动推导; 幂等; 线性分支同样可写)
	// 口径: 按 parentID 链逐级求值(父层 + 自身类型步进), 不再做几何包含推导
	layout->DeriveCtrlLayers();
	// 坐标整理: 同一口径按 parentID 链累加声明坐标(有效坐标 = 声明坐标 + 父链声明坐标之和; 幂等)
	// 说明: 装配期尚未解算像素几何(声明坐标全 0) ⇒ 本次推导结果也是 0; 真正的累加发生在几何解算之后
	//       (调用方解算完几何须再调一次, 见 Layout::DeriveCtrlPositions 的说明)
	layout->DeriveCtrlPositions();
	// 事件委托: 声明为 CtrlEventIsEntrustParent 的控件把事件绑定到父容器的事件(按 parentID 上溯)
	// 说明: 委托未解析(父容器缺失 / 父无事件且无处理函数 / 祖先未登记事件 / 成环)时该控件保持无响应,
	//       不影响装配结果
	LayoutBuilder::ResolveEntrustedEvents(*layout, pNode);
	// 成功
	ret = layout;
END:
	return ret;
}

// ================================
// 区域: 控件信息装配与控件缓冲回收实现
// ================================

// 函数: BuildCtrlInfoFromSpec
// 描述: 依中间态元素规格装配一枚控件信息(控件字段 / 事件绑定 / 类型缓冲; 见 LayoutBuilder.h)
BOOL LayoutBuilder::BuildCtrlInfoFromSpec(CtrlInfo& info, CONST ElementSpec* es, BOOL bBindEvent)
{
	BOOL bRet = FALSE;
	if (es == NULL)
	{
		return bRet;
	}
	info.id = es->id;
	info.parentID = es->parentID;
	info.type = GetTypeEnum(es->type);
	info.label = es->label;			// 定长字段内嵌于中间态池, 可长期持有
	// 文档原类型串不驻控件(归条目级旁挂表); 未命中串 ⇒ CTRL_TYPE_UNKNOWN, 渲染前按矩形占位
	// 事件绑定: 按布局文件声明的 event 名查绑定表(绑定表稀疏 ⇒ 未命中者 pfnEvent == NULL)
	// 注: "委托父容器"是特殊事件值 ⇒ 本步不查表, 须在布局级装配后由 LayoutBuilder::ResolveEntrustedEvents 解析
	if (bBindEvent && es->event[0] && CtrlEventIsEntrustParent(es->event) == FALSE)
	{
		EventInfo ev = UIEventHandler::BindEventHandler(es->event);
		info.pEventHandler = ev.pfnEvent;
		if (ev.bufferSize > 0)
		{
			info.bufferInfo = AllocatorHub::AllocBuffer(ev.bufferSize);
			if (info.bufferInfo.buffer)
			{
				RtlZeroMemory(info.bufferInfo.buffer, info.bufferInfo.size);
			}
		}
	}
	// 类型缓冲初始化(仅内存池分配失败返回 FALSE; 未知类型不建缓冲但返回 TRUE)
	if (!CtrlBuffer::PrepareCtrlBuffer(&info, es->text))
	{
		return bRet;
	}
	bRet = TRUE;
	return bRet;
}

// 函数: CopyLayout
// 描述: 深拷贝一枚布局对象(缓冲与标签另建副本, 储存模式随源)
Layout* LayoutBuilder::CopyLayout(Layout& src)
{
	Layout* dst = NULL;
	if (src.IsInvalid() || src.GetTotalNum() == 0)
	{
		return NULL;
	}
	// 储存模式随源(模式在创建时确定): 源为链式(可增删)时目标也是链式;
	// 否则 SetCtrlInfo 会写到新对象的线性数组上(容量不符时索引越界)。
	dst = Layout::CreateLayout(src.GetTotalNum(), src.IsLinkMode());
	if (dst == NULL)
	{
		return NULL;
	}
	for (size_t i = 0; i < src.GetTotalNum(); i++)
	{
		CtrlInfo& s = src.GetCtrlInfoRef(i);
		CtrlInfo d = s;						// 浅拷贝: 指针暂共享, 下面逐个替换为池内独立副本
		// 1. 类型缓冲: 按原容量重新分配并逐字节复制 ⇒ 保留文本/行数据等内容
		if (s.bufferInfo.buffer != NULL && s.bufferInfo.size > 0)
		{
			d.bufferInfo = AllocatorHub::AllocSmall(s.bufferInfo.size);
			if (d.bufferInfo.buffer == NULL)
			{
				LayoutBuilder::FreeLayoutCtrlBuffers(*dst);		// 已拷贝控件的缓冲回池(析构之前)
				Layout::DestroyLayout(dst);
				return NULL;
			}
			memcpy(d.bufferInfo.buffer, s.bufferInfo.buffer, s.bufferInfo.size);
		}
		else
		{
			d.bufferInfo = BufferInfo{ NULL, 0 };
		}
		// 2. 几何: 像素几何随值复制(position / posDeclared / size); 几何系数不随拷贝建立
		//    (新对象尚未登记条目 ⇒ 若之后发生 ref 变化, 该副本保持现状像素)
		// 3. 标签: 复制为池内新串(保持 NULL 语义, 不与源共享)
		d.label = (s.label != NULL) ? AllocatorHub::CopyString(s.label) : NULL;
		// 4. 事件处理器与父容器 ID 为值: 按值复制(副本与源指向同一处理函数 / 同一父 ID)
		if (dst->SetCtrlInfo(i, d) == FALSE)
		{
			CtrlBuffer::FreeCtrlBuffer(&d);					// 本枚未入目标布局的副本缓冲回池
			LayoutBuilder::FreeLayoutCtrlBuffers(*dst);
			Layout::DestroyLayout(dst);
			return NULL;
		}
	}
	return dst;
}

// 函数: ResolveEntrustedEvents
// 描述: 解析"委托父容器"的事件绑定(沿父容器 ID 上溯: 委托父继续上溯; 无事件且无处理函数的父即中断)
BOOL LayoutBuilder::ResolveEntrustedEvents(Layout& layout, CONST LayoutNode* pNode)
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	CONST DWORD dwTotal = (pNode != NULL && pNode->info != NULL) ? pNode->info->dwElements : 0;
	DWORD j = 0;
	if ((dwTotal == 0) || (layout.GetTotalNum() < dwTotal))
	{
		return bRet;
	}
	// 1. 逐枚检查事件声明: 只有"委托父容器"的控件才需要解析(其余控件的绑定在装配时已定)
	bRet = TRUE;
	for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, j++)
	{
		CONST ElementSpec* es = pElem->data;
		CtrlInfo* pCtrl = NULL;
		INT nParent = -1;
		DWORD dwGuard = 0;
		if (es == NULL || j >= dwTotal || CtrlEventIsEntrustParent(es->event) == FALSE)
		{
			continue;
		}
		pCtrl = layout.GetCtrlInfo(j);
		if (pCtrl == NULL)
		{
			bRet = FALSE;
			continue;
		}
		// 2. 沿父容器 ID 上溯: 委托父继续上溯; 无事件且无处理函数的父即中断
		//    (最多走一遍控件总数 ⇒ 成环时自然止步)
		pCtrl->pEventHandler = NULL;
		nParent = es->parentID;
		while (nParent >= 0 && dwGuard < dwTotal)
		{
			DWORD dwParentIndex = 0;
			CONST ElementSpec* pParentSpec = NULL;
			CtrlInfo* pParentCtrl = NULL;
			if (ElementIndexByID(pNode, nParent, &dwParentIndex) == FALSE || dwParentIndex >= dwTotal)
			{
				break;						// 父容器不存在 ⇒ 保持无响应(静默降级)
			}
			pParentSpec = ElementSpecAt(pNode, dwParentIndex);
			if (pParentSpec == NULL)
			{
				break;
			}
			// ① 父自己也声明"委托父容器" ⇒ 向上委托(取它的父继续上溯)
			if (CtrlEventIsEntrustParent(pParentSpec->event) != FALSE)
			{
				nParent = pParentSpec->parentID;
				dwGuard++;
				continue;
			}
			// ② 父是无事件控件(事件字段为空)且无独立处理函数 ⇒ 委托中断, 不再上溯
			pParentCtrl = layout.GetCtrlInfo(dwParentIndex);
			if (pParentCtrl == NULL)
			{
				break;
			}
			if (pParentCtrl->pEventHandler == NULL && pParentSpec->event[0] == '\0')
			{
				break;
			}
			// ③ 父有自己的事件: 继承其处理函数(未登记 ⇒ 继承到空 = 无响应, 同样不再上溯)
			pCtrl->pEventHandler = pParentCtrl->pEventHandler;
			break;
		}
		// 3. 未解析到处理器 = 该控件无响应(不影响装配结果, 只记录在返回值)
		if (pCtrl->pEventHandler == NULL)
		{
			bRet = FALSE;
		}
	}
	return bRet;
}

// 函数: FreeLayoutCtrlBuffers
// 描述: 回收一枚布局内全部控件的自身缓冲(布局对象析构之前调用; 见 LayoutBuilder.h)
VOID LayoutBuilder::FreeLayoutCtrlBuffers(Layout& layout)
{
	if (layout.IsInvalid())
	{
		return;
	}
	CONST size_t qwTotal = layout.GetTotalNum();
	for (size_t i = 0; i < qwTotal; i++)
	{
		CtrlBuffer::FreeCtrlBuffer(layout.GetCtrlInfo(i));
	}
}
