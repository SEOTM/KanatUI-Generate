// ================================
// 文件: ImGuiRender.cpp
// 描述: Dear ImGui 渲染能力提供者实现 —— 按 Render.h 的接口形式填表, 逐项把框架语汇翻译成
//   ImGui 调用: 渲染目标窗口的取得(反调宿主; 期望位置、客户区尺寸与窗口标题一并以参数交出)、窗口与内容容器的开合、
//   ID 命名空间压栈、光标定位与内容原点、逐控件命中判定(补无绘制命中项 / 末项点击 / 矩形兜底)、
//   窗口层级置顶。
// 说明: 本文件是渲染侧唯一包含 imgui 头文件的边界 —— 消费件(LayoutRender)只持 RenderApi 表,
//   因此"换 UI 实现"= 另写一份同形态的能力提供者并在宿主注入点替换, 消费件零改动。
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>

#include "imgui.h"
#include "imgui_internal.h"		// 末项表(LastItemData)与窗口层级置顶(BringWindowToDisplayFront)

#include "framework/render/imgui/ability/ImGuiRender.h"
#include "framework/render/imgui/ImGuiHost.h"		// 反调面: 渲染目标窗口由宿主按渲染顺序决定(见 AcquireWnd)

// ================================
// 区域: 模块级状态定义区
// 说明: 进程级静态 —— 与框架层既有口径一致(最近窗口不随窗口数扩张)
// ================================
static ImGuiWindow* pLastWnd = NULL;	// 最近一次开启的窗口(供帧末显式置顶)

// ================================
// 区域: 文件内函数声明区
// ================================
static ImVec2 toImVec2(CONST vec2& v);
static BOOL AcquireWnd(CONST vec2& pos, CONST vec2& size, LPCSTR szTitle);
static BOOL OpenWnd(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD dwStates);
static VOID CloseWnd();
static VOID BringWndToFront();
static BOOL OpenPanel(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD crBack);
static VOID ClosePanel();
static VOID SetCursor(CONST vec2& pos);
static vec2 GetContentOrigin();
static VOID PushId(INT32 nId);
static VOID PushPtrId(CONST VOID* pKey);
static VOID PopId();
static uint64_t TakeItemId();
static VOID AddHitArea(LPCSTR szLabel, CONST vec2& pos, CONST vec2& size);
static BOOL IsHitClicked();
static BOOL IsRectClicked(CONST vec2& mn, CONST vec2& mx);
static INT64 GetDrawVtxCount();

// ================================
// 区域: 接口表定义区
// 说明: 表的唯一实例(进程级静态) —— 成员用指定初始化列出, 使成员顺序调整不会静默错位;
//       各成员语义见 Render.h 的 RenderApi
// ================================
static CONST RenderApi RenderApiTable =
{
	.AcquireWnd = AcquireWnd,
	.OpenWnd = OpenWnd,
	.CloseWnd = CloseWnd,
	.BringWndToFront = BringWndToFront,
	.OpenPanel = OpenPanel,
	.ClosePanel = ClosePanel,
	.SetCursor = SetCursor,
	.GetContentOrigin = GetContentOrigin,
	.PushId = PushId,
	.PushPtrId = PushPtrId,
	.PopId = PopId,
	.TakeItemId = TakeItemId,
	.AddHitArea = AddHitArea,
	.IsHitClicked = IsHitClicked,
	.IsRectClicked = IsRectClicked,
	.GetDrawVtxCount = GetDrawVtxCount,
};

// ================================
// 区域: 文件内辅助函数区
// ================================

// 函数: toImVec2
// 描述: vec2 → ImVec2(ImGui 窗口/容器 API 的入参类型)
static ImVec2 toImVec2(CONST vec2& v)
{
	return ImVec2(v.x, v.y);
}

// ================================
// 区域: 窗口与内容容器实现区
// ================================

// 函数: AcquireWnd
// 描述: 取得本次渲染的目标窗口(转调宿主的反调面) —— 渲染目标由渲染器按渲染顺序决定, 布局侧报期望几何与标题
static BOOL AcquireWnd(CONST vec2& pos, CONST vec2& size, LPCSTR szTitle)
{
	if (size.x <= 0.0f || size.y <= 0.0f)
	{
		return FALSE;
	}
	return ImGuiHost::AcquireNextWnd(pos, size, szTitle);
}

// 函数: OpenWnd
// 描述: 开启一个窗口(基座窗口与无独立句柄的内嵌窗口同走本项), 并记为当前绘制目标
static BOOL OpenWnd(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD dwStates)
{
	ImGuiWindowFlags flags = 0;
	if ((dwStates & RENDER_WND_BASE) != 0)
	{
		flags |= ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse;
	}
	if ((dwStates & RENDER_WND_NOINPUT) != 0)
	{
		flags |= ImGuiWindowFlags_NoInputs;
	}
	ImGui::SetNextWindowPos(toImVec2(pos));
	ImGui::SetNextWindowSize(toImVec2(size));
	ImGui::Begin(szName, NULL, flags);
	pLastWnd = ImGui::GetCurrentWindow();
	return TRUE;
}

// 函数: CloseWnd
// 描述: 结束当前窗口(与 OpenWnd 成对)
static VOID CloseWnd()
{
	ImGui::End();
}

// 函数: BringWndToFront
// 描述: 把最近开启的窗口置于绘制顺序最前
static VOID BringWndToFront()
{
	if (pLastWnd != NULL)
	{
		ImGui::BringWindowToDisplayFront(pLastWnd);
	}
}

// 函数: OpenPanel
// 描述: 在当前窗口内开启一个内容容器(承载内容控件的子区域), 并把它作为当前绘制目标
static BOOL OpenPanel(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD crBack)
{
	ImGui::SetCursorPos(toImVec2(pos));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, (ImU32)crBack);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
	ImGui::BeginChild(szName, toImVec2(size), true, 0);
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	return TRUE;
}

// 函数: ClosePanel
// 描述: 结束当前内容容器(与 OpenPanel 成对)
static VOID ClosePanel()
{
	ImGui::EndChild();
}

// 函数: SetCursor
// 描述: 把当前窗口/容器的绘制光标定位到指定位置
static VOID SetCursor(CONST vec2& pos)
{
	ImGui::SetCursorPos(toImVec2(pos));
}

// 函数: GetContentOrigin
// 描述: 取当前容器的内容原点(屏幕坐标) —— 即"容器内控件声明坐标的零点"
static vec2 GetContentOrigin()
{
	CONST ImVec2 origin = ImGui::GetCursorScreenPos();
	return vec2(origin.x, origin.y);
}

// ================================
// 区域: ID 命名空间实现区
// ================================

// 函数: PushId
// 描述: 压入一个整数 ID 命名空间(逐控件绘制前压入控件 id)
static VOID PushId(INT32 nId)
{
	ImGui::PushID((int)nId);
}

// 函数: PushPtrId
// 描述: 压入一个指针 ID 命名空间(内嵌窗口按布局对象地址隔离)
static VOID PushPtrId(CONST VOID* pKey)
{
	ImGui::PushID(pKey);
}

// 函数: PopId
// 描述: 弹出最近压入的 ID 命名空间(与 PushId / PushPtrId 成对)
static VOID PopId()
{
	ImGui::PopID();
}

// ================================
// 区域: 命中判定实现区
// ================================

// 函数: TakeItemId
// 描述: 取当前窗口最近一次提交的可交互项 ID
static uint64_t TakeItemId()
{
	return (uint64_t)ImGui::GetCurrentContext()->LastItemData.ID;
}

// 函数: AddHitArea
// 描述: 在指定矩形上补一枚无绘制命中项(InvisibleButton)
static VOID AddHitArea(LPCSTR szLabel, CONST vec2& pos, CONST vec2& size)
{
	ImGui::SetCursorPos(toImVec2(pos));
	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton((szLabel != NULL) ? szLabel : "##CLICKPROBE", toImVec2(size));
}

// 函数: IsHitClicked
// 描述: 判定最近提交的项本帧是否被左键点击
static BOOL IsHitClicked()
{
	return ImGui::IsItemClicked() ? TRUE : FALSE;
}

// 函数: IsRectClicked
// 描述: 判定指定矩形内本帧是否发生左键按下(含当前窗口裁剪)
static BOOL IsRectClicked(CONST vec2& mn, CONST vec2& mx)
{
	if (ImGui::IsMouseHoveringRect(toImVec2(mn), toImVec2(mx)) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		return TRUE;
	}
	return FALSE;
}

// 函数: GetDrawVtxCount
// 描述: 取当前窗口累计的绘制顶点数
static INT64 GetDrawVtxCount()
{
	return (INT64)ImGui::GetWindowDrawList()->VtxBuffer.Size;
}

// ================================
// 区域: 能力表出口实现区
// ================================

// 函数: GetApi
// 描述: 取本实现填好的渲染能力表(函数指针表的唯一真源)
CONST RenderApi* ImGuiRender::GetApi()
{
	return &RenderApiTable;
}
