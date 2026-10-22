// ================================
// 文件: LayoutBuilder.h
// 描述: 布局对象构建原语 —— 中间态节点(布局模型) → Layout 对象展开, 布局对象的池内深拷贝,
//       以及控件信息装配与"整枚布局"的控件缓冲回收(接口契约见同目录 LayoutBuilder.md)
// ================================
#pragma once
#include <minwindef.h>
#include "framework/layout/Layout.h"
#include "framework/layout/LayoutModel.h"

// ================================
// 命名空间: LayoutBuilder
// ================================
namespace LayoutBuilder
{
	// 函数: BuildLayoutFromNode
	// 描述: 依中间态布局节点构建一枚布局对象(控件缓冲 / 标签 / 事件处理器)
	Layout* BuildLayoutFromNode(CONST LayoutNode* pNode, BOOL IsLink);

	// 函数: BuildCtrlInfoFromSpec
	// 描述: 依中间态元素规格装配一枚控件信息(控件字段 / 事件绑定 / 类型缓冲)
	BOOL BuildCtrlInfoFromSpec(CtrlInfo& info, CONST ElementSpec* es, BOOL bBindEvent);

	// 函数: CopyLayout
	// 描述: 深拷贝一枚布局对象(储存模式随源, 可写存储不共享)
	Layout* CopyLayout(Layout& src);

	// 函数: FreeLayoutCtrlBuffers
	// 描述: 回收一枚布局内全部控件的自身缓冲(布局对象析构之前调用; 逐控件转调 base 的 CtrlBuffer::FreeCtrlBuffer)
	VOID FreeLayoutCtrlBuffers(Layout& layout);

	// 函数: ResolveEntrustedEvents
	// 描述: 解析"委托父容器"的事件绑定(按父容器 ID 上溯到最近一个持有处理器的祖先)
	BOOL ResolveEntrustedEvents(Layout& layout, CONST LayoutNode* pNode);
}
