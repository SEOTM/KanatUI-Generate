// ================================
// 文件: doc\DocConsistency.cpp
// 描述: 文档数据一致性的实现(口径、锁定面纪律与迁出动因见 DocConsistency.h 的文件头说明)
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <stdio.h>
#include <string.h>
#include "doc/DocConsistency.h"
#include "framework/layout/Layout.h"

// ================================
// 区域: 文件内静态工具区
// ================================

// 函数: AppendReport
// 描述: 向诊断文本追加一行(容量不足时按实现截断, 不越界)
// 参数:
//		OUT	szReport:	诊断文本(可空 ⇒ 本行丢弃)
//		IN	cbReport:	诊断文本容量(字节)
//		IN	szLine:		待追加的行(自带换行符)
static VOID AppendReport(LPSTR szReport, size_t cbReport, LPCSTR szLine)
{
	if (szReport != NULL && cbReport > 0 && szLine != NULL)
	{
		strncat_s(szReport, cbReport, szLine, _TRUNCATE);
	}
}

// ================================
// 区域: 接口实现区
// ================================
// 函数: CheckSaveData
// 描述: 保存前数据体检 —— 逐布局检查元素 ID 连续性与 Label 唯一性(只读)
BOOL DocConsistency::CheckSaveData(CONST FileMidState& state, BOOL* pbIdGap, BOOL* pbLabelDup, LPSTR szReport, size_t cbReport)
{
	// ID 体检位图上限: 单布局元素数上限(超出即判"不连续"并逐条点明)
	constexpr DWORD DOC_ID_CHECK_MAX = 1024;

	BOOL bIdGap = FALSE;
	BOOL bLabelDup = FALSE;
	BYTE aUsed[DOC_ID_CHECK_MAX]{};
	if (pbIdGap != NULL) { *pbIdGap = FALSE; }
	if (pbLabelDup != NULL) { *pbLabelDup = FALSE; }
	if (szReport != NULL && cbReport > 0) { szReport[0] = '\0'; }
	if (LayoutModel::LayoutFirst(state) == NULL)
	{
		return FALSE;
	}
	// 逐布局体检(ID = 位图查重 + 计数与最大值配平; Label = 双重循环; 元素数量级为百, 直接比较足够)
	for (LayoutNode* pLayout = LayoutModel::LayoutFirst(state); pLayout != NULL; pLayout = LayoutModel::LayoutNext(pLayout))
	{
		if (pLayout->info == NULL)
		{
			continue;
		}
		LPCSTR lpName = (pLayout->info->name[0] != '\0') ? pLayout->info->name : "(未命名布局)";
		// ① ID: 期望各元素 id 恰为 0..N-1(既不越界、不重复, 也不留空洞)
		DWORD dwCount = 0;
		DWORD dwMaxId = 0;
		BOOL bBadId = FALSE;
		memset(aUsed, 0, sizeof(aUsed));
		for (ElementNode* pElem = LayoutModel::ElementFirst(pLayout); pElem != NULL; pElem = LayoutModel::ElementNext(pElem))
		{
			CONST int nId = (pElem->data != NULL) ? pElem->data->id : -1;
			dwCount++;
			if (nId < 0 || (DWORD)nId >= DOC_ID_CHECK_MAX)
			{
				bBadId = TRUE;						// 负值 / 超出体检上限 ⇒ 不可能构成 0..N-1
				continue;
			}
			if ((DWORD)nId > dwMaxId) { dwMaxId = (DWORD)nId; }
			if (aUsed[nId] != 0) { bBadId = TRUE; }	// 重复 ID(按 ID 访问元素会产生歧义)
			aUsed[nId] = 1;
		}
		if (!bBadId && (dwCount > 0) && (dwMaxId + 1 != dwCount))
		{
			bBadId = TRUE;							// 无重复且最大值配平才等价于 0..N-1 的一个排列
		}
		if (bBadId)
		{
			bIdGap = TRUE;
			if (szReport != NULL && cbReport > 0)
			{
				CHAR szLine[320]{};
				snprintf(szLine, sizeof(szLine),
					"布局「%s」: 元素 %u 枚, ID 未构成 0..N-1(最大 %u)\n",
					lpName, (unsigned)dwCount, (unsigned)dwMaxId);
				strncat_s(szReport, cbReport, szLine, _TRUNCATE);
			}
		}
		// ② Label: 同一布局内重名(空 label = 未命名, 不参与判定; 跨布局同名允许)
		BOOL bDupInLayout = FALSE;
		for (ElementNode* pA = LayoutModel::ElementFirst(pLayout); (pA != NULL) && !bDupInLayout; pA = LayoutModel::ElementNext(pA))
		{
			if (pA->data == NULL || pA->data->label[0] == '\0')
			{
				continue;
			}
			for (ElementNode* pB = LayoutModel::ElementNext(pA); pB != NULL; pB = LayoutModel::ElementNext(pB))
			{
				if (pB->data == NULL || pB->data->label[0] == '\0')
				{
					continue;
				}
				if (lstrcmpA(pA->data->label, pB->data->label) == 0)
				{
					bDupInLayout = TRUE;
					if (szReport != NULL && cbReport > 0)
					{
						CHAR szLine[320]{};
						snprintf(szLine, sizeof(szLine),
							"布局「%s」: Label 重名「%s」\n", lpName, pA->data->label);
						strncat_s(szReport, cbReport, szLine, _TRUNCATE);
					}
					break;
				}
			}
		}
		if (bDupInLayout)
		{
			bLabelDup = TRUE;
		}
	}
	if (pbIdGap != NULL) { *pbIdGap = bIdGap; }
	if (pbLabelDup != NULL) { *pbLabelDup = bLabelDup; }
	return (!bIdGap && !bLabelDup);
}

// 函数: CheckStructure
// 描述: 结构件排布体检 —— 逐布局检查结构件是否占据布局头部连续槽位(只读)
BOOL DocConsistency::CheckStructure(CONST FileMidState& state, LPSTR szReport, size_t cbReport)
{
	BOOL bOk = TRUE;
	if (szReport != NULL && cbReport > 0) { szReport[0] = '\0'; }
	if (LayoutModel::LayoutFirst(state) == NULL)
	{
		return FALSE;
	}
	for (LayoutNode* pLayout = LayoutModel::LayoutFirst(state); pLayout != NULL; pLayout = LayoutModel::LayoutNext(pLayout))
	{
		CHAR szLine[320]{};
		if (pLayout->info == NULL)
		{
			continue;
		}
		LPCSTR lpName = (pLayout->info->name[0] != '\0') ? pLayout->info->name : "(未命名布局)";
		BOOL bWnd = FALSE;
		BOOL bTitle = FALSE;
		BOOL bCntr = FALSE;
		BOOL bContent = FALSE;			// 已出现内容控件 ⇒ 结构件区已结束
		BOOL bBadStruct = FALSE;
		DWORD dwIndex = 0;
		for (ElementNode* pElem = LayoutModel::ElementFirst(pLayout); pElem != NULL;
			pElem = LayoutModel::ElementNext(pElem), dwIndex++)
		{
			CONST CtrlType type = (pElem->data != NULL) ? GetTypeEnum(pElem->data->type) : CTRL_TYPE_UNKNOWN;
			CONST BOOL bStructCtrl = (type == CTRL_TYPE_WND) || (type == CTRL_TYPE_TITLE) || (type == CTRL_TYPE_CNTR);
			if (!bStructCtrl)
			{
				bContent = TRUE;
				continue;
			}
			// 结构件落在内容控件之后: 展开侧只在布局前三个槽位内查找结构件 ⇒ 该枚会被当作内容控件渲染
			if (bContent)
			{
				snprintf(szLine, sizeof(szLine),
					"布局「%s」: 第 %u 枚控件为结构件 %s, 却排在内容控件之后(结构件须占据布局头部连续槽位)\n",
					lpName, (unsigned)dwIndex, GetTypeName(type));
				AppendReport(szReport, cbReport, szLine);
				bBadStruct = TRUE;
				continue;
			}
			// 头部区内: 顺序须为 WND → [TITLE] → CNTR, 且各至多一枚
			if (type == CTRL_TYPE_WND)
			{
				if (bWnd || bTitle || bCntr)
				{
					snprintf(szLine, sizeof(szLine),
						"布局「%s」: 第 %u 枚控件为重复或错序的窗体区 CTRL_TYPE_WND(结构件顺序须为 WND → [TITLE] → CNTR)\n",
						lpName, (unsigned)dwIndex);
					AppendReport(szReport, cbReport, szLine);
					bBadStruct = TRUE;
					continue;
				}
				bWnd = TRUE;
			}
			else if (type == CTRL_TYPE_TITLE)
			{
				if (!bWnd || bTitle || bCntr)
				{
					snprintf(szLine, sizeof(szLine),
						"布局「%s」: 第 %u 枚控件为重复或错序的标题栏 CTRL_TYPE_TITLE(结构件顺序须为 WND → [TITLE] → CNTR)\n",
						lpName, (unsigned)dwIndex);
					AppendReport(szReport, cbReport, szLine);
					bBadStruct = TRUE;
					continue;
				}
				bTitle = TRUE;
			}
			else
			{
				if (!bWnd || bCntr)
				{
					snprintf(szLine, sizeof(szLine),
						"布局「%s」: 第 %u 枚控件为重复或错序的窗体内容区 CTRL_TYPE_CNTR(结构件顺序须为 WND → [TITLE] → CNTR)\n",
						lpName, (unsigned)dwIndex);
					AppendReport(szReport, cbReport, szLine);
					bBadStruct = TRUE;
					continue;
				}
				bCntr = TRUE;
			}
		}
		// 必需结构件缺项(缺 WND 或 CNTR 时该布局被判无效 ⇒ 渲染为空)
		if (!bWnd)
		{
			snprintf(szLine, sizeof(szLine), "布局「%s」: 缺少必需结构件窗体区 CTRL_TYPE_WND\n", lpName);
			AppendReport(szReport, cbReport, szLine);
			bBadStruct = TRUE;
		}
		if (!bCntr)
		{
			snprintf(szLine, sizeof(szLine), "布局「%s」: 缺少必需结构件窗体内容区 CTRL_TYPE_CNTR\n", lpName);
			AppendReport(szReport, cbReport, szLine);
			bBadStruct = TRUE;
		}
		if (bBadStruct)
		{
			bOk = FALSE;
		}
	}
	return bOk;
}

// 函数: NormalizeElementIds
// 描述: 逐布局把元素 ID 按链序重排为 0..N-1
VOID DocConsistency::NormalizeElementIds(CONST FileMidState& state)
{
	// 中间态: 全部布局逐元素重排(遍历顺序即链序 = 文件数组序)
	for (LayoutNode* pLayout = LayoutModel::LayoutFirst(state); pLayout != NULL; pLayout = LayoutModel::LayoutNext(pLayout))
	{
		int nId = 0;
		for (ElementNode* pElem = LayoutModel::ElementFirst(pLayout); pElem != NULL; pElem = LayoutModel::ElementNext(pElem))
		{
			if (pElem->data != NULL)
			{
				pElem->data->id = nId;
			}
			nId++;
		}
	}
}

// 函数: SyncCtrlIdsToIndex
// 描述: 把已展开布局内控件的 id 与索引对齐
VOID DocConsistency::SyncCtrlIdsToIndex(Layout& layout)
{
	if (layout.IsInvalid())
	{
		return;
	}
	// 按索引对齐 id(链内只含文档控件 ⇒ 索引即文档索引, 无跳过项)
	for (size_t i = 0; i < layout.GetTotalNum(); i++)
	{
		CtrlInfo* pCtrl = layout.GetCtrlInfo(i);
		if (pCtrl == NULL)
		{
			continue;
		}
		pCtrl->id = (int)i;
	}
}
