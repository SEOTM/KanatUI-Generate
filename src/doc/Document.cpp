// ================================
// 文件: Document.cpp
//   说明: 三段实现(FindByLayout 的条目反查段 / SpecAt / MarkDirty+ClearDirty)均为纯数据操作,
//         不含会话面状态与策略; 迁移只改访问前缀, 语义逐条不变。
// ================================

#include <string.h>
#include "doc/Document.h"

// 函数: FindByLayout
// 描述: 按布局指针在文档条目数组内线性反查(未命中返回 NULL)
LayoutEntry* Document::FindByLayout(Layout* layout)
{
	LayoutEntry* pRet = NULL;
	if (layout == NULL || entries == NULL)
	{
		return pRet;
	}
	for (DWORD i = 0; i < dwEntries; i++)
	{
		if (entries[i].layout == layout)
		{
			pRet = &entries[i];
			break;
		}
	}
	return pRet;
}

// 函数: SpecAt
// 描述: 取条目中间态内指定索引的元素规格(不判几何系数有效标志; 越界/未挂接返回 NULL)
ElementSpec* Document::SpecAt(LayoutEntry& entry, size_t index)
{
	ElementSpec* pRet = NULL;
	if (entry.pNode == NULL)
	{
		return pRet;
	}
	ElementNode* pElem = entry.pNode->elements;
	for (size_t i = 0; (i < index) && (pElem != NULL); i++)
	{
		pElem = pElem->next;
	}
	if (pElem != NULL)
	{
		pRet = pElem->data;
	}
	return pRet;
}

// 函数: LayoutLabelAt
// 描述: 取指定条目的显示名(label 为空则回落 name)拷入调用方缓冲(越界/缓冲无效 ⇒ FALSE 并写空串)
BOOL Document::LayoutLabelAt(DWORD index, LPSTR buf, size_t cap)
{
	BOOL bRet = FALSE;
	LPCSTR lpSrc = NULL;
	LayoutEntry* pEntry = NULL;
	if (buf == NULL || cap == 0)
	{
		return bRet;
	}
	buf[0] = '\0';
	pEntry = GetEntry(index);
	if (pEntry != NULL)
	{
		CONST LayoutNode* pNode = pEntry->pNode;
		if (pNode != NULL && pNode->info != NULL)
		{
			// 显示名优先取 label; label 为空时回落 name
			lpSrc = (pNode->info->label[0] != '\0') ? pNode->info->label : pNode->info->name;
			bRet = TRUE;
		}
	}
	if (lpSrc != NULL)
	{
		strncpy_s(buf, cap, lpSrc, _TRUNCATE);
	}
	return bRet;
}

// 函数: RebindNodes
// 描述: 把全部条目的中间态布局节点按索引重新挂接到当前中间态布局链上
VOID Document::RebindNodes()
{
	if (entries == NULL)
	{
		return;
	}
	for (DWORD i = 0; i < dwEntries; i++)
	{
		entries[i].pNode = LayoutModel::LayoutAt(docState, i);
	}
}

// 函数: InsertSpecAt
// 描述: 在指定布局的指定文档索引处插入一枚元素规格(中间态就地链接; 其后元素索引整体后移)
// 说明: 布局→条目索引的反查由调用方完成
BOOL Document::InsertSpecAt(DWORD docIndex, Layout* layout, size_t index)
{
	BOOL bRet = FALSE;
	if (!LayoutModel::ElementInsert(docState, docIndex, (DWORD)index))
	{
		return bRet;
	}
	RebindNodes();		// 按索引对齐条目节点(插入后布局内元素索引整体后移)
	// 新元素几何 = 该控件当前像素(系数留零, 全部落在 offset 上)
	{
		ElementSpec* pSpec = LayoutModel::ElementAt(docState, docIndex, (DWORD)index);
		// 取同索引的布局控件(链内只含文档控件 ⇒ 文档索引 == 布局控件索引)
		CtrlInfo* pCtrl = (layout != NULL) ? layout->GetCtrlInfo(index) : NULL;
		if (pSpec != NULL && pCtrl != NULL)
		{
			pSpec->x.offset = (LONG)pCtrl->position.x;
			pSpec->y.offset = (LONG)pCtrl->position.y;
			pSpec->width.offset = (LONG)pCtrl->size.x;
			pSpec->height.offset = (LONG)pCtrl->size.y;
			pSpec->id = pCtrl->id;
		}
	}
	bRet = TRUE;
	return bRet;
}

// 函数: RemoveSpecAt
// 描述: 移除指定文档索引的元素规格(中间态就地解链; 其后元素索引整体前移)
BOOL Document::RemoveSpecAt(DWORD docIndex, size_t index)
{
	BOOL bRet = FALSE;
	if (!LayoutModel::ElementRemove(docState, docIndex, (DWORD)index))
	{
		return bRet;
	}
	RebindNodes();		// 按索引对齐条目节点(移除后布局内元素索引整体前移)
	bRet = TRUE;
	return bRet;
}

// 函数: ReplaceEntries
// 描述: 整体替换条目数组与激活索引(纯字段写入; 旧数组的释放归调用方)
VOID Document::ReplaceEntries(LayoutEntry* pEntries, DWORD dwCount, DWORD dwActive)
{
	entries = pEntries;
	dwEntries = dwCount;
	SetActive(dwActive);
}

// 函数: GetCount / GetActive / SetActive
// 描述: 条目数量与激活索引的读写原语(越界钳制与版本号递增归调用方策略)
DWORD Document::GetCount()
{
	return dwEntries;
}

DWORD Document::GetActive()
{
	return dwActive;
}

VOID Document::SetActive(DWORD index)
{
	dwActive = index;
}

// 函数: GetEntry
// 描述: 取指定索引的条目地址(条目数组未挂接或索引越界 ⇒ NULL)
LayoutEntry* Document::GetEntry(DWORD index)
{
	LayoutEntry* pRet = NULL;
	if (entries != NULL && index < dwEntries)
	{
		pRet = &entries[index];
	}
	return pRet;
}

// 函数: GetState
// 描述: 取文档中间态引用
FileMidState& Document::GetState()
{
	return docState;
}

// 函数: GetPath / SetPath
// 描述: 关联磁盘路径的读写(SetPath 只做字段赋值; 非空串须由调用方保证池内自持)
LPCSTR Document::GetPath()
{
	return lpDocPath;
}

VOID Document::SetPath(LPCSTR path)
{
	lpDocPath = path;
}

// 函数: IsDirty
// 描述: 当前文档是否有未保存改动
BOOL Document::IsDirty()
{
	return bDocDirty;
}

// 函数: MarkDirty
// 描述: 置"有未保存改动"
VOID Document::MarkDirty()
{
	bDocDirty = TRUE;
}

// 函数: ClearDirty
// 描述: 清"未保存改动"标记
VOID Document::ClearDirty()
{
	bDocDirty = FALSE;
}
