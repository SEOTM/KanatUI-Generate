// ================================
// 文件: doc\DocConsistency.h
// 描述: 文档数据一致性 —— 保存前只读体检与 ID 归一。
// ================================
#pragma once
#include <windows.h>
#include "framework/layout/LayoutModel.h"

// 前向声明: 链式控件 id 同步的入参(实现处用完整类型)
class Layout;

// ================================
// 命名空间: DocConsistency
// 描述: 文档数据一致性(保存前体检 / ID 归一)
// ================================
namespace DocConsistency
{
	// 函数: CheckSaveData
	// 描述: 保存前数据体检 —— 逐布局检查元素 ID 连续性与 Label 唯一性(只读, 不改数据)
	// 说明: ① ID: 期望各元素 id 恰为 0..N-1(既不越界、不重复, 也不留空洞);
	//       ② Label: 同一布局内重名(空 label = 未命名, 不参与判定; 跨布局同名允许, 如 ##MAINWND);
	//       ③ 无布局(空文档) ⇒ 返回 FALSE(无数据可体检)。
	// 参数:
	//		IN	state:		文档中间态
	//		OUT	pbIdGap:	[可空] 是否存在 ID 未构成 0..N-1 的布局
	//		OUT	pbLabelDup:	[可空] 是否存在 Label 重名的布局
	//		OUT	szReport:	[可空] 诊断文本(逐条列出问题布局)
	//		IN	cbReport:	诊断文本容量(字节)
	// 返回:
	//		BOOL: TRUE = 两项检查均通过; FALSE = 至少一项不通过或文档为空
	BOOL CheckSaveData(CONST FileMidState& state, BOOL* pbIdGap, BOOL* pbLabelDup, LPSTR szReport, size_t cbReport);

	// 函数: CheckStructure
	// 描述: 结构件排布体检 —— 逐布局检查结构件是否占据布局头部连续槽位(只读, 不改数据)
	// 说明: 结构件 = 窗体区 CTRL_TYPE_WND(必需) + 窗体内容区 CTRL_TYPE_CNTR(必需) + 标题栏 CTRL_TYPE_TITLE(可选);
	//       三者须构成布局的头部连续槽位, 顺序为 WND → [TITLE] → CNTR, 其后不得再出现结构件。
	//       判据依据 = 展开侧按类型只在布局前三个槽位内查找结构件(GetLayoutBase), 内容控件自
	//       结构件计数处起遍历(GetShellNum) ⇒ 结构件缺失时该布局渲染为空, 结构件落在计数之外时
	//       会被当作内容控件渲染。
	//       本函数供载入链侧调用(不触及 Layout 锁定面): 违规 ⇒ 该布局无法正确渲染, 应由调用方拒载。
	// 参数:
	//		IN	state:		文档中间态
	//		OUT	szReport:	[可空] 诊断文本(逐条列出违规布局与违规形态)
	//		IN	cbReport:	诊断文本容量(字节)
	// 返回:
	//		BOOL: TRUE = 全部布局合规; FALSE = 至少一处违规或文档为空(无数据可体检)
	BOOL CheckStructure(CONST FileMidState& state, LPSTR szReport, size_t cbReport);

	// 函数: NormalizeElementIds
	// 描述: 逐布局把元素 ID 按链序(即文件数组序)重排为 0..N-1(只改 id 字段)
	// 说明: 与 CheckSaveData 的 ID 判据配对; 不改类型/标签/文本/几何。
	// 参数:
	//		IN	state:	文档中间态
	VOID NormalizeElementIds(CONST FileMidState& state);

	// 函数: SyncCtrlIdsToIndex
	// 描述: 把一枚已展开布局内控件的 id 与索引对齐(id = 索引)
	// 说明: 控件列表行文本按 id 显示 ⇒ 不同步会出现内存与文件不一致;
	//   链内只含文档控件 ⇒ 索引即文档索引, 无跳过项; 无效布局 ⇒ 无动作。
	// 参数:
	//		IN	layout:	目标布局对象
	VOID SyncCtrlIdsToIndex(Layout& layout);
}
