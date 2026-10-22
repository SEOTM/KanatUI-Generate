// ================================
// 文件: HostContext.h
// 描述: 宿主上下文 —— 宿主(进程入口 main.cpp)注入给"应用 / 编辑器"的外部能力与窗口句柄的
//   单一持有处。
// ================================
#pragma once
#include <windows.h>
#include "framework/base/FuncPtr.h"
#include "app/SessionContracts.h"

// ================================
// 命名空间: HostContext
// 描述: 宿主能力与窗口句柄的读写(进程级单例语义; 与 GenUIEditor / SelectionOverlay 同风格)
// ================================
namespace HostContext
{
	// 函数: SetHintSyncHandler / GetHintSyncHandler
	// 描述: 注入 / 读取同步提示展示器(宿主启动期调用一次; NULL = 未注入 ⇒ 编辑器回落 MessageBoxW)
	// 说明: 展示提示需要"渲染帧 + 消息泵"(宿主 main 才具备) ⇒ 展示器由宿主注入; 编辑器在弹窗前查它:
	//   已注入且「提示布局」已装配 ⇒ 走展示器(同步阻塞至用户选择), 否则回落 MessageBoxW。
	// 参数 / 返回: 见 app\SessionContracts.h 的 pfnHintSync
	VOID SetHintSyncHandler(pfnHintSync pfn);
	pfnHintSync GetHintSyncHandler();

	// 函数: SetInjectClickHandler / GetInjectClickHandler
	// 描述: 注入 / 读取预览布局内容控件的点击事件处理器(须早于首次装配; NULL = 未注入)
	// 说明: 装配点(应用侧 SyncPreviewLayout)按它把每枚内容控件的 pEventHandler 改绑为记录器
	//   (覆盖文档 event 的原处理器、不转调) ⇒ 预览窗口里点控件只产生"点击记录", 不触发真实业务事件;
	//   结构件不覆盖(按类型排除), 无事件绑定的控件保持 NULL。
	VOID SetInjectClickHandler(pfnCtrlEvent pfn);
	pfnCtrlEvent GetInjectClickHandler();

	// 函数: SetWindowHandle / GetWindowHandle
	// 描述: 编辑窗口(外壳宿主)句柄读写 —— 供文件对话框 hwndOwner / 消息框属主 / 内嵌窗口位置基准取用
	VOID SetWindowHandle(HWND hwnd);
	HWND GetWindowHandle();

	// 函数: SetPreviewWindowHandle / GetPreviewWindowHandle
	// 描述: 预览窗口句柄读写(建窗后注册; 供窗口级操作读取)
	VOID SetPreviewWindowHandle(HWND hwnd);
	HWND GetPreviewWindowHandle();
}
