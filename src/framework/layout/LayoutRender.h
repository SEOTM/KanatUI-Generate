#pragma once
#include <minwindef.h>
#include "framework/base/BasicDesign.h"
#include "framework/layout/Layout.h"
#include "framework/base/FuncPtr.h"
#include "framework/render/Render.h"

// ================================
// 库名: LayoutRender
// 描述: 布局级渲染出口(对外调用面) —— 基座窗口/内容容器的编排 + 逐控件向控件库分派 +
//       逐控件命中判定; 一切 UI 设施动作只经注入的渲染能力表(Render.h)发起,
//       控件绘制只经注入的控件绘制接口表(Render.h 的 CtrlApi)发起。
// 说明: ① 本件不依赖任何具体 UI 库, 也不认识任何窗口句柄: 布局对象是纯描述, "渲染到哪个窗口"
//          由渲染能力表的 AcquireWnd 决定(基座布局路径显式调用, 内嵌(flex)路径沿用当前窗口);
//          能力表由宿主(见 framework\render\imgui\ImGuiHost)在 UI 设施初始化期注入, 未注入时各渲染入口
//          静默失败(返回 FALSE), 不弹窗、不打印;
//       ② 实现见同目录 LayoutRender.cpp; ImGui 侧的能力提供方见 framework\render\imgui\ability\ImGuiRender,
//          控件绘制能力由控件库(见 framework\render\imgui\control\Controls 的 GetCtrlApi)提供。
// ================================
namespace LayoutRender
{
	// ================================
	// 区域: 渲染能力注入
	// ================================

	// 函数: SetRenderApi
	// 描述: 注入渲染能力表(幂等; 传 NULL 清除注入)
	VOID SetRenderApi(CONST RenderApi* pApi);

	// 函数: GetRenderApi
	// 描述: 取当前注入的渲染能力表
	CONST RenderApi* GetRenderApi();

	// 函数: SetCtrlApi
	// 描述: 注入控件绘制接口表(幂等; 传 NULL 清除注入)
	VOID SetCtrlApi(CONST CtrlApi* pApi);

	// 函数: GetCtrlApi
	// 描述: 取当前注入的控件绘制接口表
	CONST CtrlApi* GetCtrlApi();

	// ================================
	// 区域: 渲染入口
	// ================================

	// 函数: RenderBase
	// 描述: 基础渲染: 渲染占一个标准窗口的基座布局
	BOOL RenderBase(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, BOOL bNoInputs = FALSE);

	// 函数: RenderPreviewBase
	// 描述: 基础渲染之上为内容控件逐枚做点击判定, 并可插入视图叠加件
	BOOL RenderPreviewBase(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, pfnCtrlEvent pfnClick,
		pfnOverlayBefore pfnOverlay = NULL);

	// 函数: RenderFlex
	// 描述: 渲染无独立窗口句柄的内嵌(flex)布局
	BOOL RenderFlex(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj);

	// 函数: IsIMEDisabled
	// 描述: 检查当前是否需要禁用 IME 输入法
	BOOL IsIMEDisabled();
}
