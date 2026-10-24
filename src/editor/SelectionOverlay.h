// ================================
// 文件: SelectionOverlay.h
// 描述: 编辑器"预览选中态"的视图叠加件 —— 在预览布局渲染帧内, 于被选中控件之前绘制一枚
//   仅显示矩形的 RECT。
// ================================
#pragma once
#include <minwindef.h>
#include "framework/layout/Layout.h"

// ================================
// 命名空间: SelectionOverlay
// 描述: 选中态视图叠加件(函数均为进程级单例语义, 与 KanatUIGenerateEditor 同风格)
// ================================
namespace SelectionOverlay
{
	// 常量: SEL_MARK_LABEL
	// 描述: 叠加件的固定 label(叠加件放置与识别共用的同一真源)
	// 说明: 该件不入布局控件链, 也不进中间态; 本标签只在编辑器侧使用
	inline constexpr LPCSTR SEL_MARK_LABEL = "##SEL_MARK";

	// 常量: SEL_MARK_ID
	// 描述: 叠加件的 ImGui ID(其 id 与文档控件 id 空间无关: 它不入布局)
	constexpr INT SEL_MARK_ID = 3;
	// 常量: SEL_MARK_LAYER
	// 描述: 叠加件的层次值: 固定 = 0(窗口底那一档)
	// 说明: 叠加件是派生附加件, 不参与层次推导(Layout 只对链内控件推导) ⇒ 其层次只能由
	constexpr INT32 SEL_MARK_LAYER = 0;

	// 函数: IsMarkCtrl
	// 描述: 判定一枚控件是否为"选中态表达件"(类型 RECT 且 label 等于 SEL_MARK_LABEL)
	// 参数:
	//		IN	pCtrl:	待判定的控件(可空)
	// 返回:
	//		BOOL: TRUE = 是选中态表达件; FALSE = 其它控件或入参为空
	BOOL IsMarkCtrl(CONST CtrlInfo* pCtrl);

	// 函数: SetSelectedIndex / GetSelectedIndex
	// 描述: 当前选中的文档索引读写(-1 = 无选中)
	// 说明: 链内只含文档控件 ⇒ 文档索引 == 布局控件索引(无需换算);
	//   结构件可被选中 ⇒ 本索引可指向结构件 —— 结构件不挂叠加件
	//   (判定见 OnBeforeCtrl), 这只是"叠加件"语义, 与"选中"语义无关。
	VOID SetSelectedIndex(INT docIndex);
	INT GetSelectedIndex();

	// 函数: OnBeforeCtrl
	// 描述: 渲染插入点 —— 在布局内索引 index 的控件绘制之前被调用一次
	//       (由 LayoutRender::RenderPreviewBase 逐枚回调; 见 FuncPtr.h 的 pfnOverlayBefore)
	// 说明: 仅当 index == 当前选中索引、且该控件非结构件时绘制一枚外扩矩形; 其余情形零动作
	//   (不提交任何绘制项、不改变 ImGui 状态)。
	// 参数:
	//		IN	layout:	正在渲染的预览布局对象(只读)
	//		IN	index:	即将绘制的控件索引
	VOID OnBeforeCtrl(Layout& layout, size_t index);

	// 函数: GetDrawnRect
	// 描述: 取最近一次已绘制叠加件的目标索引与几何(生产路径不消费该记录)
	// 说明: 记录在 OnBeforeCtrl 实际绘制之后写入 ⇒ "有记录"即证"渲染帧内的绘制路径确实走到";
	//   几何 = 绘制当刻填入控件描述的矩形(= 被选中控件按参考内距 padding 外扩的结果)。
	// 参数:
	//		OUT	pnDocIndex:	绘制时的目标控件索引(可空)
	//		OUT	pfX/pfY:	矩形左上角(可空)
	//		OUT	pfW/pfH:	矩形宽高(可空)
	// 返回:
	//		BOOL: TRUE = 已有绘制记录; FALSE = 尚无
	BOOL GetDrawnRect(INT* pnDocIndex, FLOAT* pfX, FLOAT* pfY, FLOAT* pfW, FLOAT* pfH);
}
