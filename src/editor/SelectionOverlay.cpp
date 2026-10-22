// ================================
// 文件: SelectionOverlay.cpp
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>	// 先于 Application.h(其接口含 HWND; minwindef.h 不定义 HWND)
#include <string.h>		// strcmp(选中态表达件标签比较)
#include "editor/SelectionOverlay.h"
#include "app/Application.h"
#include "framework/render/imgui/control/Controls.h"
#include "framework/base/ControlSpec.h"
#include "imgui.h"

// ================================
// 区域: 状态区
// ================================
static INT nSelDocIndex = -1;			// 当前选中的文档索引(-1 = 无选中)
// 叠加件的控件描述(无事件 / 无缓冲 / 不入中间态): 逐帧就地改写后交控件库绘制。
// 说明: 用持久对象而非栈上临时量, 避免绘制实现若暂存该
static CtrlInfo sMark{};
// 最近一次"已绘制"叠加件的记录(见 GetDrawnRect —— 生产路径不消费)
static BOOL bDrawnValid = FALSE;			// 是否已有绘制记录
static INT nDrawnDocIndex = -1;				// 绘制时的目标控件索引
static FLOAT fDrawnX = 0.0f;				// 绘制的矩形左上角 X
static FLOAT fDrawnY = 0.0f;				// 绘制的矩形左上角 Y
static FLOAT fDrawnW = 0.0f;				// 绘制的矩形宽
static FLOAT fDrawnH = 0.0f;				// 绘制的矩形高

// ================================
// 区域: 接口实现区
// ================================
// 函数: IsMarkCtrl
// 描述: 判定一枚控件是否为选中态表达件(见头文件)
BOOL SelectionOverlay::IsMarkCtrl(CONST CtrlInfo* pCtrl)
{
	if (pCtrl == NULL || pCtrl->type != CTRL_TYPE_RECT || pCtrl->label == NULL)
	{
		return FALSE;
	}
	return (strcmp(pCtrl->label, SEL_MARK_LABEL) == 0) ? TRUE : FALSE;
}
// 函数: SetSelectedIndex
// 描述: 置当前选中的文档索引(-1 = 无选中)
VOID SelectionOverlay::SetSelectedIndex(INT docIndex)
{
	nSelDocIndex = docIndex;
}
// 函数: GetSelectedIndex
// 描述: 取当前选中的文档索引
INT SelectionOverlay::GetSelectedIndex()
{
	return nSelDocIndex;
}
// 函数: OnBeforeCtrl
// 描述: 渲染插入点 —— 在索引 index 的控件绘制之前绘制选中叠加件(见头文件)
VOID SelectionOverlay::OnBeforeCtrl(Layout& layout, size_t index)
{
	if (nSelDocIndex < 0 || ((size_t)nSelDocIndex != index))
	{
		return;					// 非当前选中位置 ⇒ 零动作(本帧的其余插入点亦然)
	}
	if (index < layout.GetShellNum())
	{
		return;
	}
	CtrlInfo* pSel = layout.GetCtrlInfo(index);
	if (pSel == NULL)
	{
		return;					// 索引失效(文档结构已变) ⇒ 本帧不画(由编辑器侧 Update 校验选中)
	}
	CONST FLOAT fPad = Application::GetActiveRef().GetSizeInfo<FLOAT>().padding;
	sMark = CtrlInfo{};
	sMark.type = CTRL_TYPE_RECT;
	sMark.id = SEL_MARK_ID;
	sMark.label = SEL_MARK_LABEL;
	sMark.pEventHandler = NULL;
	sMark.nLayer = SEL_MARK_LAYER;
	sMark.position.x = pSel->position.x - fPad;
	sMark.position.y = pSel->position.y - fPad;
	sMark.size.x = pSel->size.x + fPad * 2.0f;
	sMark.size.y = pSel->size.y + fPad * 2.0f;
	ImGui::PushID(sMark.id);
	Controls::DrawCtrl(&sMark);
	ImGui::PopID();
	// 绘制记录("渲染帧内确实画过" + 目标索引与几何可核; 见 GetDrawnRect)
	bDrawnValid = TRUE;
	nDrawnDocIndex = (INT)index;
	fDrawnX = sMark.position.x;
	fDrawnY = sMark.position.y;
	fDrawnW = sMark.size.x;
	fDrawnH = sMark.size.y;
}
// 函数: GetDrawnRect
// 描述: 取最近一次已绘制叠加件的目标索引与几何(见头文件)
BOOL SelectionOverlay::GetDrawnRect(INT* pnDocIndex, FLOAT* pfX, FLOAT* pfY, FLOAT* pfW, FLOAT* pfH)
{
	if (bDrawnValid == FALSE)
	{
		return FALSE;
	}
	if (pnDocIndex != NULL) { *pnDocIndex = nDrawnDocIndex; }
	if (pfX != NULL) { *pfX = fDrawnX; }
	if (pfY != NULL) { *pfY = fDrawnY; }
	if (pfW != NULL) { *pfW = fDrawnW; }
	if (pfH != NULL) { *pfH = fDrawnH; }
	return TRUE;
}
