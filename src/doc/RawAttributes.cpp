// ================================
// 文件: doc\RawAttributes.cpp
// 说明: 数据保真旁挂表族的实现(全静态成员; 见 RawAttributes.h 的文件头)
// ================================

#include <windows.h>		// RtlZeroMemory
#include <string.h>			// strcmp
#include "doc/RawAttributes.h"
#include "doc/Document.h"			// 条目结构(旁挂表两字段与容量字段)
#include "framework/base/mem/AllocatorHub.h"	// 小对象池分配 / 回收与串拷贝
#include "framework/layout/Layout.h"		// 布局控件数 / 控件信息(有自身缓冲判据)

// 注入的条目反查(反查本体属会话面 ⇒ 由应用侧在"分配成员存储"之后注入; NULL = 未注入)
pfnFindLayoutEntry RawAttributes::pfnFindEntry = NULL;

// 函数: IsReady
// 描述: 本模块是否就绪(私有判据件) —— 应用未就绪判据的等价件
BOOL RawAttributes::IsReady()
{
	return (pfnFindEntry != NULL) ? TRUE : FALSE;
}
// 函数: SetEntryLookup
// 描述: 注入条目反查函数指针(NULL = 复位)
VOID RawAttributes::SetEntryLookup(pfnFindLayoutEntry pfnLookup)
{
	pfnFindEntry = pfnLookup;
}
// 函数: SetRawType / GetRawType / RemoveRawTypeAt
// 描述: 文档原类型串旁挂表(按文档索引; 池内自持) —— 控件对象不持有原类型串
// 说明: 原串只服务两个用途 —— ①编辑层可感知(编辑入口可读); ②按原串写回(数据无损),
//       故不驻控件对象。表惰性建立(容量 = 首次记录时的控件数,
//       只增不减: 删除控件时条目整体前移, 容量不变 ⇒ 与文档索引一一对应);
//       条目为空(NULL)表示"无原串" ⇒ 写回时回退规范类型名 GetTypeName(type)。
// 函数: EnsureRawTable
// 描述: 确保一张"字符串旁挂表"(原类型串表 / 声明文本表)的容量不小于 qwCap(不足则迁移扩容)
// 说明: 两表同构(条目类型均为 LPCSTR, 容量字段为 DWORD) ⇒ 共用一个扩容实现;
//       旧表确属小对象池时回收, 回落线性分配器的块安全忽略
// 参数: 见 RawAttributes.h
// 返回: BOOL TRUE = 容量已满足; FALSE = 入参无效 / 池分配失败
BOOL RawAttributes::EnsureRawTable(LPCSTR** ppTable, DWORD* pdwCap, size_t qwCap)
{
	if (ppTable == NULL || pdwCap == NULL || AllocatorHub::GetArena() == NULL)
	{
		return FALSE;
	}
	if (*ppTable != NULL && *pdwCap >= qwCap)
	{
		return TRUE;			// 容量已满足
	}
	size_t qwNew = (qwCap > 0) ? qwCap : 1;
	BufferInfo tbuf = AllocatorHub::AllocSmall(sizeof(LPCSTR) * qwNew);
	if (tbuf.buffer == NULL)
	{
		return FALSE;
	}
	LPCSTR* pNew = (LPCSTR*)tbuf.buffer;
	RtlZeroMemory(pNew, sizeof(LPCSTR) * qwNew);
	if (*ppTable != NULL)		// 迁移既有条目
	{
		for (DWORD k = 0; (k < *pdwCap) && (k < qwNew); k++)
		{
			pNew[k] = (*ppTable)[k];
		}
	}
	// 迁移完成后回收旧表(仅当旧表确属小对象池; 回落线性分配器的块安全忽略)
	if (*ppTable != NULL)
	{
		AllocatorHub::FreeSmall(BufferInfo{ (VOID*)*ppTable, sizeof(LPCSTR) * ((*pdwCap > 0) ? *pdwCap : 1) });
	}
	*ppTable = pNew;
	*pdwCap = (DWORD)qwNew;
	return TRUE;
}
// 返回: 见各函数注释(索引越界/布局无效/池不足 ⇒ FALSE)
BOOL RawAttributes::SetRawType(Layout* layout, size_t index, LPCSTR rawType)
{
	if (layout == NULL || !IsReady() || AllocatorHub::GetArena() == NULL)
	{
		return FALSE;
	}
	// 按布局指针反查条目(唯一基座 ⇒ 当前 active 条目; 外壳快照 ⇒ 快照条目)
	LayoutEntry* pEntry = pfnFindEntry(layout);
	// 文档面口径: 入参索引 = 文档索引(不含选中标记), 表容量按文档控件数
	CONST size_t qwDocCount = layout->GetTotalNum();
	if (pEntry == NULL || index >= qwDocCount)
	{
		return FALSE;
	}
	if (pEntry->aRawType == NULL || pEntry->qwRawType < qwDocCount)
	{
		if (!EnsureRawTable(&pEntry->aRawType, &pEntry->qwRawType, qwDocCount))
		{
			return FALSE;
		}
	}
	// 原串未变时复用既有池内串(逐帧重装的常态路径 ⇒ 不重复分配; 表内串内容与入参一致)
	if (rawType == NULL)
	{
		pEntry->aRawType[index] = NULL;
	}
	else if (pEntry->aRawType[index] == NULL || strcmp(pEntry->aRawType[index], rawType) != 0)
	{
		pEntry->aRawType[index] = AllocatorHub::CopyString(rawType);
	}
	return TRUE;
}
// 函数: GetRawType
// 描述: 取某布局内指定文档索引的"文档原类型串"(表内为空/无表 ⇒ 返回 NULL)
LPCSTR RawAttributes::GetRawType(Layout* layout, size_t index)
{
	if (layout == NULL || !IsReady())
	{
		return NULL;
	}
	// 按布局指针反查条目(唯一基座 ⇒ 当前 active 条目; 外壳快照 ⇒ 快照条目)
	LayoutEntry* pEntry = pfnFindEntry(layout);
	if (pEntry == NULL || pEntry->aRawType == NULL || index >= pEntry->qwRawType)
	{
		return NULL;
	}
	return pEntry->aRawType[index];
}
// 函数: RemoveRawTypeAt
// 描述: 移除某布局旁挂表中指定文档索引的条目(后续条目前移; 容量不变 ⇒ 与文档索引保持一致)
BOOL RawAttributes::RemoveRawTypeAt(Layout* layout, size_t index)
{
	if (layout == NULL || !IsReady())
	{
		return FALSE;
	}
	// 按布局指针反查条目(唯一基座 ⇒ 当前 active 条目; 外壳快照 ⇒ 快照条目)
	LayoutEntry* pEntry = pfnFindEntry(layout);
	if (pEntry == NULL || pEntry->aRawType == NULL || index >= pEntry->qwRawType)
	{
		return FALSE;
	}
	for (DWORD k = (DWORD)index; (k + 1) < pEntry->qwRawType; k++)
	{
		pEntry->aRawType[k] = pEntry->aRawType[k + 1];
	}
	pEntry->aRawType[pEntry->qwRawType - 1] = NULL;
	return TRUE;
}
// 函数: InsertRawTypeAt
// 描述: 在指定索引处插入一枚"文档原类型串"条目(其后条目整体后移; 与控件重排同步)
// 参数: 见 RawAttributes.h
// 返回: BOOL TRUE = 已插入; FALSE = 入参无效 / 索引越界 / 池分配失败
BOOL RawAttributes::InsertRawTypeAt(Layout* layout, size_t index, LPCSTR rawType)
{
	if (layout == NULL || !IsReady())
	{
		return FALSE;
	}
	// 按布局指针反查条目(唯一基座 ⇒ 当前 active 条目; 外壳快照 ⇒ 快照条目)
	LayoutEntry* pEntry = pfnFindEntry(layout);
	// 文档面口径: 入参索引 = 文档索引(不含选中标记)
	CONST size_t qwDocCount = layout->GetTotalNum();
	if (pEntry == NULL || index >= qwDocCount)
	{
		return FALSE;
	}
	if (!EnsureRawTable(&pEntry->aRawType, &pEntry->qwRawType, qwDocCount))
	{
		return FALSE;
	}
	// 其后条目整体后移(自尾向前搬, 避免覆盖)
	for (DWORD k = pEntry->qwRawType - 1; k > (DWORD)index; k--)
	{
		pEntry->aRawType[k] = pEntry->aRawType[k - 1];
	}
	pEntry->aRawType[index] = (rawType != NULL) ? AllocatorHub::CopyString(rawType) : NULL;
	return TRUE;
}
// 函数: SetRawText / GetRawText / RemoveRawTextAt
// 描述: 无缓冲控件的声明文本旁挂表(与 aRawType 同机制、同容量)
// 说明: 没有缓冲的类型(未知类型 / 未使用槽位 / 视频专用类型 LIST·TIMEBAR·RENDER)在控件对象里没有文本存储,
//       但其 JSON text 键仍原样往返 ⇒ 记在本表;
//       控件一旦有自身缓冲(正常文本类), 其文本即由缓冲承载, 本表条目不参与。
BOOL RawAttributes::SetRawText(Layout* layout, size_t index, LPCSTR text)
{
	if (layout == NULL || !IsReady() || AllocatorHub::GetArena() == NULL)
	{
		return FALSE;
	}
	// 按布局指针反查条目(唯一基座 ⇒ 当前 active 条目; 外壳快照 ⇒ 快照条目)
	LayoutEntry* pEntry = pfnFindEntry(layout);
	// 文档面口径: 入参索引 = 文档索引(不含选中标记), 表容量按文档控件数
	CONST size_t qwDocCount = layout->GetTotalNum();
	if (pEntry == NULL || index >= qwDocCount)
	{
		return FALSE;
	}
	if (pEntry->aRawText == NULL || pEntry->qwRawText < qwDocCount)
	{
		if (!EnsureRawTable(&pEntry->aRawText, &pEntry->qwRawText, qwDocCount))
		{
			return FALSE;
		}
	}
	// 文本未变时复用既有池内串(与类型串旁挂表同法: 逐帧重装的常态路径不重复分配)
	if (text == NULL)
	{
		pEntry->aRawText[index] = NULL;
	}
	else if (pEntry->aRawText[index] == NULL || strcmp(pEntry->aRawText[index], text) != 0)
	{
		pEntry->aRawText[index] = AllocatorHub::CopyString(text);
	}
	return TRUE;
}
// 函数: GetRawText
// 描述: 取某布局内指定文档索引的"无缓冲控件声明文本"(表内为空/无表/控件有自身缓冲 ⇒ 返回 NULL)
LPCSTR RawAttributes::GetRawText(Layout* layout, size_t index)
{
	if (layout == NULL || !IsReady())
	{
		return NULL;
	}
	// 按布局指针反查条目(唯一基座 ⇒ 当前 active 条目; 外壳快照 ⇒ 快照条目)
	LayoutEntry* pEntry = pfnFindEntry(layout);
	if (pEntry == NULL || pEntry->aRawText == NULL || index >= pEntry->qwRawText)
	{
		return NULL;
	}
	CtrlInfo* pCtrl = layout->GetCtrlInfo(index);	// 链内只含文档控件 ⇒ 文档索引 == 布局控件索引
	if (pCtrl != NULL && pCtrl->bufferInfo.buffer != NULL)
	{
		return NULL;		// 有自身缓冲的控件以缓冲为准(本表只服务"无文本存储"的控件)
	}
	return pEntry->aRawText[index];
}
// 函数: RemoveRawTextAt
// 描述: 移除某布局文本旁挂表中指定索引的条目(后续条目前移; 容量不变)
BOOL RawAttributes::RemoveRawTextAt(Layout* layout, size_t index)
{
	if (layout == NULL || !IsReady())
	{
		return FALSE;
	}
	// 按布局指针反查条目(唯一基座 ⇒ 当前 active 条目; 外壳快照 ⇒ 快照条目)
	LayoutEntry* pEntry = pfnFindEntry(layout);
	if (pEntry == NULL || pEntry->aRawText == NULL || index >= pEntry->qwRawText)
	{
		return FALSE;
	}
	for (DWORD k = (DWORD)index; (k + 1) < pEntry->qwRawText; k++)
	{
		pEntry->aRawText[k] = pEntry->aRawText[k + 1];
	}
	pEntry->aRawText[pEntry->qwRawText - 1] = NULL;
	return TRUE;
}
// 函数: InsertRawTextAt
// 描述: 在指定索引处插入一枚"无缓冲控件声明文本"条目(其后条目整体后移; 与控件重排同步)
// 参数: 见 RawAttributes.h
// 返回: BOOL TRUE = 已插入; FALSE = 入参无效 / 索引越界 / 池分配失败
BOOL RawAttributes::InsertRawTextAt(Layout* layout, size_t index, LPCSTR text)
{
	if (layout == NULL || !IsReady())
	{
		return FALSE;
	}
	// 按布局指针反查条目(唯一基座 ⇒ 当前 active 条目; 外壳快照 ⇒ 快照条目)
	LayoutEntry* pEntry = pfnFindEntry(layout);
	// 文档面口径: 入参索引 = 文档索引(不含选中标记)
	CONST size_t qwDocCount = layout->GetTotalNum();
	if (pEntry == NULL || index >= qwDocCount)
	{
		return FALSE;
	}
	if (!EnsureRawTable(&pEntry->aRawText, &pEntry->qwRawText, qwDocCount))
	{
		return FALSE;
	}
	// 其后条目整体后移(自尾向前搬, 避免覆盖)
	for (DWORD k = pEntry->qwRawText - 1; k > (DWORD)index; k--)
	{
		pEntry->aRawText[k] = pEntry->aRawText[k - 1];
	}
	pEntry->aRawText[index] = (text != NULL) ? AllocatorHub::CopyString(text) : NULL;
	return TRUE;
}
