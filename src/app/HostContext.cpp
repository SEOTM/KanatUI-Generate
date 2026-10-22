// ================================
// 文件: HostContext.cpp
// 描述: 宿主上下文的实现(口径、注入时序与复位口径见 HostContext.h 的文件头说明)
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include "app/HostContext.h"

// ================================
// 区域: 状态区
// ================================
static pfnHintSync sPfnHint = NULL;			// 同步提示展示器(宿主注入; NULL = 未注入)
static pfnCtrlEvent sPfnInjectClick = NULL;	// 预览内容控件的点击处理器(注入; NULL = 未注入)
static HWND sHwndEdit = NULL;				// 编辑窗口句柄(建窗后注册)
static HWND sHwndPreview = NULL;			// 预览窗口句柄(建窗后注册)

// ================================
// 区域: 接口实现区
// ================================
VOID HostContext::SetHintSyncHandler(pfnHintSync pfn)
{
	sPfnHint = pfn;
}
pfnHintSync HostContext::GetHintSyncHandler()
{
	return sPfnHint;
}
VOID HostContext::SetInjectClickHandler(pfnCtrlEvent pfn)
{
	sPfnInjectClick = pfn;
}
pfnCtrlEvent HostContext::GetInjectClickHandler()
{
	return sPfnInjectClick;
}
VOID HostContext::SetWindowHandle(HWND hwnd)
{
	sHwndEdit = hwnd;
}
HWND HostContext::GetWindowHandle()
{
	return sHwndEdit;
}
VOID HostContext::SetPreviewWindowHandle(HWND hwnd)
{
	sHwndPreview = hwnd;
}
HWND HostContext::GetPreviewWindowHandle()
{
	return sHwndPreview;
}
