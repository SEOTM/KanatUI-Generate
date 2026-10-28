// ================================
// 文件: ImGuiHost.cpp
// 描述: ImGui 宿主实现(接口契约见同目录 ImGuiHost.md)
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>			// DXGI_PRESENT_TEST / DXGI_STATUS_OCCLUDED(遮挡判定)
#include <imm.h>			// IME 开关(输入法上下文)
#include <float.h>
#include <string.h>			// strcmp / strncpy_s(标题变化判定)
#include <wchar.h>			// wcscat_s(窗口类名拼接)

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "framework/render/imgui/ImGuiHost.h"
#include "framework/render/imgui/ability/ImGuiRender.h"		// ImGui 侧渲染能力(初始化期注入布局渲染出口)
#include "framework/render/imgui/control/Controls.h"		// 控件绘制接口表(初始化期注入布局渲染出口)
#include "framework/layout/LayoutRender.h"		// 渲染能力注入点(布局渲染出口)
#include "framework/base/AppEnv.h"			// 窗口圆角(优先 DWM, 不可用时回退 RGN)

// 从 imgui_impl_win32.cpp 前向声明消息处理程序
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ================================
// 区域: 标准窗口单元定义区
// ================================

// 结构: WndUnit
struct WndUnit
{
	HWND                 hWnd = NULL;				// 窗口句柄(NULL = 未创建)
	ATOM                 atomClass = 0;				// 本窗口所用窗口类原子(所有窗口共用; 0 = 未注册)
	ImGuiContext*        ctx = NULL;				// 本窗口的 ImGui 上下文(NULL = 未就绪)
	D3D11Manager         d3d;						// 本窗口的 D3D11 设施(设备 / 交换链 / 渲染目标; 本模块自持)
	HIMC                 hIMC = NULL;				// IME 输入上下文(ImGui 面就绪后取得; 释放时归还)
	FLOAT                fFontSize = 0.0f;			// 本窗口字号(建窗时取自 WndSpec)
	DWORD                dwRounding = 0;			// 本窗口圆角半径(像素; 0 = 不设圆角)
	BOOL                 bRoundByRgn = FALSE;		// 本窗口圆角是否由 RGN 实现(TRUE = 尺寸变化后须重设)
	BOOL                 bRoundDone = FALSE;		// 本窗口圆角是否已设过(建窗时首设, 尺寸变化时重设)
	CHAR                 szTitle[256] = "";			// 上次应用的窗口标题(UTF-8; 供标题变化判定)
	ImGuiHost::WndShowState stateShow = ImGuiHost::WND_SHOW_NEVER;	// 本窗口显示状态(本模块自持的可见性真相)
	BOOL                 bShowChangeByUs = FALSE;	// 本次可见性变更是否由本模块发起(ShowWindow 调用期内有效)
};

// ================================
// 区域: 模块级状态定义区
// ================================
static WndUnit Units[ImGuiHost::MAX_WND];			// 标准窗口单元表
static DWORD dwStandardWnds = 0;					// 已声明的标准窗口数量
static DWORD dwActiveWnds = 0;						// 布局激活所需的窗口数
static ATOM atomSharedClass = 0;					// 共用窗口类原子(所有标准窗口共用一个类; 0 = 未注册)
static ImGuiHost::StyleFunc pfnStyleFunc = NULL;	// 宿主的样式槽填充回调(NULL = 不填充)
static VOID* pStyleUser = NULL;						// 传给样式回调的用户指针
static ImGuiHost::SegmentFunc pfnSegmentFunc = NULL;	// 宿主的段前回调(NULL = 不调用)
static VOID* pSegmentUser = NULL;					// 传给段前回调的用户指针
static BOOL bDarkBase = TRUE;						// 亮暗基础样式(默认暗色)
static BOOL bSegmentOpen = FALSE;					// 本帧是否已有已开启未结算的渲染段
static DWORD dwSegmentIndex = 0;					// 当前段的窗口序号(仅 bSegmentOpen 为真时有效)
static DWORD dwCursor = 0;							// 本帧的渲染目标取用游标(下一个可取用的标准窗口序号)
static BOOL bIMEDisabledOverride = FALSE;			// 本段 IME 覆盖(段结算时应用; 结算后失效)
static DWORD dwSegments = 0;						// 已结算的渲染段数(绘制量自证)
static DWORD dwCmdLists = 0;						// 已提交的 ImGui 绘制命令数累计(绘制量自证)
static INT64 llVtxCount = 0;						// 已提交的顶点数累计(绘制量自证)
static INT64 llIdxCount = 0;						// 已提交的索引数累计(绘制量自证)
static DWORD dwResizeCount = 0;						// 窗口尺寸调整次数(尺寸调整自证)
static DWORD dwResizeLastW = 0;						// 最近一次调整后的客户区宽(尺寸调整自证)
static DWORD dwResizeLastH = 0;						// 最近一次调整后的客户区高(尺寸调整自证)
static StyleDesc styleDesc{};						// 已登记的宿主样式(调度层经 ApplyStyle 登记)
static BOOL bStyleDescSet = FALSE;					// 宿主样式是否已登记(未登记 ⇒ 只用亮暗基础样式)
static FLOAT fWndFontSize = 0.0f;					// 已登记的建面字号(像素; 0 = 未登记 ⇒ Start 用缺省字号)
static HICON sAppIcon = NULL;						// 已登记的应用窗口图标(建窗与窗口类注册时消费)
static HostApi HostTable{};							// 宿主接口表(Register 填充; 进程级静态)

// ================================
// 区域: 默认建窗几何(Start 的推荐入口口径)
// ================================
constexpr DWORD DEFAULT_WND_WIDTH = 800;			// 默认客户区宽(像素)
constexpr DWORD DEFAULT_WND_HEIGHT = 600;			// 默认客户区高(像素)
constexpr FLOAT DEFAULT_FONT_SIZE = 18.0f;			// 默认字号(像素)
constexpr FLOAT FONT_SIZE_MIN = 8.0f;				// 字号下限(像素; 低于此值按默认字号)
constexpr DWORD DEFAULT_ROUNDING = 10;				// 默认窗口圆角半径(像素)
constexpr LPCWSTR CLASS_NAME_SUFFIX = L"_IMGUI_CLASS";	// 窗口类名后缀(类名 = <APPNAME>_IMGUI_CLASS)

// ================================
// 区域: 文件内函数声明区
// ================================

// 函数: FindUnitIndex
// 描述: 按窗口句柄反查标准窗口单元序号(未命中返回 -1)
static INT FindUnitIndex(HWND hWnd);

// 函数: ApplyUnitStyle
// 描述: 对指定窗口重放亮暗基础样式、高级样式回调与已登记的宿主样式
static VOID ApplyUnitStyle(DWORD index);

// 函数: LoadFonts
// 描述: 逐个试载中文字体候选并自校准 em 换算(全部失败退默认字体)
static BOOL LoadFonts(HWND hWnd, ImGuiContext* ctx, FLOAT fFontSize);

// 函数: InitImGuiUnit
// 描述: 建立单个窗口的 ImGui 面(上下文 / 字体 / 两端后端 / 样式 / IME / 能力注入)
static BOOL InitImGuiUnit(DWORD index);

// 函数: ShutdownImGuiUnit
// 描述: 关停单个窗口的 ImGui 面并归还 IME 上下文
static VOID ShutdownImGuiUnit(DWORD index);

// 函数: StartSegment
// 描述: 开启指定窗口的渲染段(两端后端开帧并结束当前 ImGui 帧的准备)
static BOOL StartSegment(DWORD index);

// 函数: FinalizeSegment
// 描述: 结算当前渲染段(提交绘制 / 应用 IME / 呈现并清理后台缓冲)
static VOID FinalizeSegment();

// 函数: ResetSegmentState
// 描述: 复位段状态(段标记 / 段序号 / 渲染游标 / IME 覆盖)
static VOID ResetSegmentState();

// 函数: ShowWnd
// 描述: 发起指定窗口的显示或隐藏并标记本次变更的来源
static VOID ShowWnd(DWORD index, BOOL bShow);

// 函数: ApplyActiveVisibility
// 描述: 按激活窗口数应用可见性策略(不足者显示, 超出者隐藏)
static VOID ApplyActiveVisibility();

// 函数: HostWndProc
// 描述: 共用窗口过程: 记账显示状态与尺寸, 处理退出键与销毁消息, 其余交 DefWindowProcW
static LRESULT WINAPI HostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ================================
// 区域: 声明与注入实现区
// ================================

// 函数: EnableDpiAwareness
// 描述: 打开进程级 DPI 感知(启动首件事; 幂等)
VOID ImGuiHost::EnableDpiAwareness()
{
	ImGui_ImplWin32_EnableDpiAwareness();
}

// 函数: Register
// 描述: 填充宿主接口表并返回其只读指针(调度层取表入口; 重复调用幂等)
CONST HostApi* ImGuiHost::Register()
{
	// 填表: 表内全部为静态成员函数地址, 进程级恒定 ⇒ 重复调用等价于重置为同一组值
	HostTable.EnableDpiAwareness = &ImGuiHost::EnableDpiAwareness;
	HostTable.Start = &ImGuiHost::Start;
	HostTable.Run = &ImGuiHost::Run;
	HostTable.Release = &ImGuiHost::Release;
	HostTable.DestroyWnds = &ImGuiHost::DestroyWnds;
	HostTable.SetDarkMode = &ImGuiHost::SetDarkMode;
	HostTable.ApplyStyle = &ImGuiHost::ApplyStyle;
	HostTable.SetIMEDisabled = &ImGuiHost::SetIMEDisabled;
	HostTable.GetStandardWndCount = &ImGuiHost::GetStandardWndCount;
	HostTable.GetWndShowState = &ImGuiHost::GetWndShowState;
	HostTable.GetWndRect = &ImGuiHost::GetWndRect;
	HostTable.GetRenderStats = &ImGuiHost::GetRenderStats;
	HostTable.GetResizeStats = &ImGuiHost::GetResizeStats;
	HostTable.SetAppIcon = &ImGuiHost::SetAppIcon;
	HostTable.GetNativeDevice = &ImGuiHost::GetNativeDevice;
	HostTable.GetNativeDeviceContext = &ImGuiHost::GetNativeDeviceContext;
	return &HostTable;
}

// 函数: GetHostApi
// 描述: 取宿主接口表(与 Register 同源; 未填充时先填充)
CONST HostApi* ImGuiHost::GetHostApi()
{
	return Register();
}

// 函数: ApplyStyle
// 描述: 登记宿主样式并刷新已就绪窗口(返回 FALSE 只表示当前没有就绪窗口)
BOOL ImGuiHost::ApplyStyle(CONST StyleDesc& desc)
{
	// 1. 登记: 建面时按本描述应用(见 InitImGuiUnit → ApplyUnitStyle)
	styleDesc = desc;
	bStyleDescSet = TRUE;
	// 2. 已就绪窗口立即刷新; 一个都没有 ⇒ 返回 FALSE(未就绪不视为失败)
	BOOL bRet = FALSE;
	for (DWORD i = 0; i < MAX_WND; i++)
	{
		if (Units[i].ctx != NULL)
		{
			ApplyUnitStyle(i);
			bRet = TRUE;
		}
	}
	return bRet;
}

// 函数: Start
// 描述: 启动渲染宿主(DPI 感知 → 默认几何构造 → 声明数量 → 建窗 → 建立渲染设施; 失败内部回退)
BOOL ImGuiHost::Start(DWORD dwWndCount, LPCSTR lpszAppName, SegmentFunc pfnSegment, VOID* pUser)
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	WCHAR szClassName[128]{};	// 窗口类名(<APPNAME>_IMGUI_CLASS; 所有窗口共用)
	WCHAR szTitle[128]{};		// 窗口标题(默认 = APPNAME)
	WndSpec specs[MAX_WND]{};	// 默认窗口规格表(按数量构造)
	RECT rcWork{};				// 主显示器工作区(居中基准)
	INT nX = 0;
	INT nY = 0;
	INT nWideLen = 0;

	// 1. DPI 感知必须是启动首件事(须早于任何窗口创建与 ImGui 初始化)
	EnableDpiAwareness();
	if (lpszAppName == NULL || lpszAppName[0] == '\0' ||
		dwWndCount == 0 || dwWndCount > MAX_WND)
	{
		return bRet;
	}
	// 2. 应用名 UTF-8 → UTF-16(窗口标题; MultiByteToWideChar 的 len 含结尾 NUL)
	nWideLen = MultiByteToWideChar(CP_UTF8, 0, lpszAppName, -1, szTitle, (INT)(sizeof(szTitle) / sizeof(szTitle[0])));
	if (nWideLen <= 0)
	{
		return bRet;
	}
	// 3. 窗口类名 = <APPNAME>_IMGUI_CLASS(同样转宽字符后拼接; 所有窗口共用一个类)
	nWideLen = MultiByteToWideChar(CP_UTF8, 0, lpszAppName, -1, szClassName,
		(INT)(sizeof(szClassName) / sizeof(szClassName[0])));
	if (nWideLen <= 0)
	{
		return bRet;
	}
	if (wcscat_s(szClassName, sizeof(szClassName) / sizeof(szClassName[0]), CLASS_NAME_SUFFIX) != 0)
	{
		return bRet;
	}
	// 4. 默认几何: 客户区 800x600, 屏幕工作区居中(工作区查询失败 ⇒ 退主屏尺寸; 工作区小于窗口 ⇒ 靠左上)
	if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0) == FALSE)
	{
		rcWork.left = 0;
		rcWork.top = 0;
		rcWork.right = GetSystemMetrics(SM_CXSCREEN);
		rcWork.bottom = GetSystemMetrics(SM_CYSCREEN);
	}
	nX = rcWork.left + (((rcWork.right - rcWork.left) - (INT)DEFAULT_WND_WIDTH) / 2);
	nY = rcWork.top + (((rcWork.bottom - rcWork.top) - (INT)DEFAULT_WND_HEIGHT) / 2);
	if (nX < rcWork.left) { nX = rcWork.left; }
	if (nY < rcWork.top) { nY = rcWork.top; }
	// 5. 构造规格表: 全部窗口共用类名与标题, 几何/圆角取默认口径, 字号取已登记值(未登记时默认)
	for (DWORD i = 0; i < dwWndCount; i++)
	{
		specs[i].lpszClassName = szClassName;
		specs[i].lpszTitle = szTitle;
		specs[i].x = nX;
		specs[i].y = nY;
		specs[i].dwWidth = DEFAULT_WND_WIDTH;
		specs[i].dwHeight = DEFAULT_WND_HEIGHT;
		specs[i].fFontSize = (fWndFontSize > 0.0f) ? fWndFontSize : DEFAULT_FONT_SIZE;
		specs[i].dwRounding = DEFAULT_ROUNDING;
	}
	// 6. 声明数量 → 段前回调注入 → 建窗 → 建立渲染设施(任一失败即内部回退: 销毁已建窗口并清注入)
	SetStandardWndCount(dwWndCount);
	SetSegmentFunc(pfnSegment, pUser);
	if (!CreateWnds(specs))
	{
		goto START_CLN;
	}
	if (!Acquire())
	{
		goto START_CLN;
	}
	bRet = TRUE;
	return bRet;

START_CLN:
	DestroyWnds();
	SetSegmentFunc(NULL, NULL);
	return bRet;
}

// 函数: SetStandardWndCount
// 描述: 声明需要创建的标准窗口数量(超过上限按上限夹取)
VOID ImGuiHost::SetStandardWndCount(DWORD dwCount)
{
	if (dwCount > MAX_WND)
	{
		dwCount = MAX_WND;
	}
	dwStandardWnds = dwCount;
}

// 函数: GetStandardWndCount
// 描述: 取已声明的标准窗口数量
DWORD ImGuiHost::GetStandardWndCount()
{
	return dwStandardWnds;
}

// 函数: GetWndCount
// 描述: 取已声明的标准窗口数量(供渲染侧按数量顺序取窗口)
DWORD ImGuiHost::GetWndCount()
{
	return dwStandardWnds;
}

// 函数: SetActiveWndCount
// 描述: 声明布局激活所需的窗口数并同步各窗口可见性
VOID ImGuiHost::SetActiveWndCount(DWORD dwActive)
{
	if (dwActive > dwStandardWnds)
	{
		dwActive = dwStandardWnds;
	}
	dwActiveWnds = dwActive;
	ApplyActiveVisibility();
}

// 函数: GetActiveWndCount
// 描述: 取当前的激活窗口数
DWORD ImGuiHost::GetActiveWndCount()
{
	return dwActiveWnds;
}

// 函数: GetWndShowState
// 描述: 取指定窗口的显示状态(序号越界按从未显示处理)
ImGuiHost::WndShowState ImGuiHost::GetWndShowState(DWORD index)
{
	if (index >= MAX_WND)
	{
		return WND_SHOW_NEVER;
	}
	return Units[index].stateShow;
}

// 函数: GetWndRect
// 描述: 取指定窗口的窗口矩形与客户区矩形(客户区矩形左上恒为 0,0); 窗口未建时返回 FALSE
BOOL ImGuiHost::GetWndRect(DWORD index, RECT* prcWnd, RECT* prcClient)
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	RECT rcWnd{};
	RECT rcClient{};
	if (index >= MAX_WND || Units[index].hWnd == NULL)
	{
		return bRet;
	}
	// 窗口矩形 = 桌面坐标; 客户区矩形 = 窗口内部坐标(左上为 0,0)
	if (GetWindowRect(Units[index].hWnd, &rcWnd) == FALSE ||
		GetClientRect(Units[index].hWnd, &rcClient) == FALSE)
	{
		return bRet;
	}
	if (prcWnd != NULL) { *prcWnd = rcWnd; }
	if (prcClient != NULL) { *prcClient = rcClient; }
	bRet = TRUE;
	return bRet;
}

// 函数: SetStyleFunc
// 描述: 注入样式槽填充回调(传 NULL 即清除注入)
VOID ImGuiHost::SetStyleFunc(StyleFunc pfnStyle, VOID* pUser)
{
	pfnStyleFunc = pfnStyle;
	pStyleUser = pUser;
}

// 函数: SetSegmentFunc
// 描述: 注入段前回调(在目标窗口后端开帧之后、ImGui 帧开始之前调用; 传 NULL 即清除注入)
VOID ImGuiHost::SetSegmentFunc(SegmentFunc pfnSegment, VOID* pUser)
{
	pfnSegmentFunc = pfnSegment;
	pSegmentUser = pUser;
}

// 函数: SetFontSize
// 描述: 登记标准窗口的字号(像素; Start 按默认几何建窗时消费; 无效值按缺省字号)
VOID ImGuiHost::SetFontSize(FLOAT fFontSize)
{
	// 无效值(非数 / 低于字体加载下限)一律记为"未登记" ⇒ Start 用缺省字号(与 LoadFonts 的下限同口径)
	if (!(fFontSize >= FONT_SIZE_MIN))
	{
		fWndFontSize = 0.0f;
		return;
	}
	fWndFontSize = fFontSize;
}

// 函数: SetAppIcon
// 描述: 登记应用窗口图标(建窗前调用; 传 NULL 清除)
VOID ImGuiHost::SetAppIcon(VOID* hIcon)
{
	sAppIcon = (HICON)hIcon;
}

// 函数: GetNativeDevice
// 描述: 取首个标准窗口的 D3D11 设备(渲染设施未就绪时返回 NULL)
VOID* ImGuiHost::GetNativeDevice()
{
	return (VOID*)Units[0].d3d.GetDevice();
}

// 函数: GetNativeDeviceContext
// 描述: 取首个标准窗口的 D3D11 设备上下文(渲染设施未就绪时返回 NULL)
VOID* ImGuiHost::GetNativeDeviceContext()
{
	return (VOID*)Units[0].d3d.GetDeviceContext();
}

// 函数: SetDarkMode
// 描述: 设置亮暗基础样式(入参归一化为真/假; 已就绪窗口立即刷新)
VOID ImGuiHost::SetDarkMode(BOOL bDark)
{
	CONST BOOL bNewBase = (bDark != FALSE) ? TRUE : FALSE;
	if (bNewBase == bDarkBase)
	{
		return;
	}
	bDarkBase = bNewBase;
	RefreshStyle();
}

// 函数: GetDarkMode
// 描述: 取当前亮暗基础样式
BOOL ImGuiHost::GetDarkMode()
{
	return bDarkBase;
}

// 函数: RefreshStyle
// 描述: 把当前样式重新应用到全部已就绪窗口
BOOL ImGuiHost::RefreshStyle()
{
	BOOL bRet = FALSE;
	for (DWORD i = 0; i < MAX_WND; i++)
	{
		if (Units[i].ctx != NULL)
		{
			ApplyUnitStyle(i);
			bRet = TRUE;
		}
	}
	return bRet;
}

// ================================
// 区域: 窗口与画面生命周期实现区
// ================================

// 函数: CreateWnds
// 描述: 按声明数量注册共用窗口类并建窗(含圆角; 任一失败则全部回退)
BOOL ImGuiHost::CreateWnds(CONST WndSpec* pSpecs)
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	if (pSpecs == NULL || pSpecs[0].lpszClassName == NULL ||
		dwStandardWnds == 0 || dwStandardWnds > MAX_WND)
	{
		return bRet;
	}
	// 1. 共用窗口类只注册一次(类 DC; 无游标 / 背景 / 菜单 —— 外观全由控件库自绘)
	{
		WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, HostWndProc, 0L, 0L,
			hInstance, sAppIcon, NULL, NULL, NULL, pSpecs[0].lpszClassName, sAppIcon };
		atomSharedClass = RegisterClassEx(&wc);
		if (atomSharedClass == 0)
		{
			goto CREATE_CLN;
		}
	}
	for (DWORD i = 0; i < dwStandardWnds; i++)
	{
		WndUnit& unit = Units[i];
		// 2. 建窗: WS_POPUP 无系统标题栏(自绘标题带); 尺寸 = 规格给定的客户区几何
		unit.atomClass = atomSharedClass;
		unit.fFontSize = pSpecs[i].fFontSize;
		unit.dwRounding = pSpecs[i].dwRounding;
		unit.bRoundByRgn = FALSE;
		unit.bRoundDone = FALSE;
		unit.szTitle[0] = '\0';
		// 显示状态: 建窗样式不含 WS_VISIBLE ⇒ 建窗后窗口不可见, 首次显示由可见性策略发起
		unit.stateShow = ImGuiHost::WND_SHOW_NEVER;
		unit.bShowChangeByUs = FALSE;
		unit.hWnd = CreateWindow(MAKEINTATOM(atomSharedClass), pSpecs[i].lpszTitle,
			WS_POPUP, (INT)pSpecs[i].x, (INT)pSpecs[i].y, (INT)pSpecs[i].dwWidth, (INT)pSpecs[i].dwHeight,
			NULL, NULL, hInstance, NULL);
		if (unit.hWnd == NULL)
		{
			goto CREATE_CLN;
		}
		// 2-bis. 应用图标: 窗口类已带图标, 建窗后再显式发一次 WM_SETICON(大 / 小两档)
		if (sAppIcon != NULL)
		{
			SendMessageW(unit.hWnd, WM_SETICON, ICON_BIG, (LPARAM)sAppIcon);
			SendMessageW(unit.hWnd, WM_SETICON, ICON_SMALL, (LPARAM)sAppIcon);
		}
		// 3. 窗口圆角: 优先 DWM, 不可用时回退 RGN(半径 = 规格传入的设计间隔)
		//    说明: 由本模块统一施加 ⇒ 宿主不必持有窗口句柄; 尺寸变化后按新尺寸重设(见 WM_SIZE)
		if (unit.dwRounding > 0)
		{
			AppEnv::ApplyWindowCornerRounding(unit.hWnd, pSpecs[i].dwWidth, pSpecs[i].dwHeight,
				unit.dwRounding, &unit.bRoundByRgn);
			unit.bRoundDone = TRUE;
		}
	}
	// 4. 激活数默认 = 标准数量(全部可见); 宿主随后可按激活布局数下调
	// 说明: 建窗后窗口仍不可见(样式不含 WS_VISIBLE) —— 首次显示在 Acquire 末尾统一发起(面就绪后)
	dwActiveWnds = dwStandardWnds;
	ResetSegmentState();
	bRet = TRUE;
	return bRet;
CREATE_CLN:
	DestroyWnds();
	return bRet;
}

// 函数: Acquire
// 描述: 建立全部窗口的 D3D11 设施与 ImGui 面并注入渲染能力表
BOOL ImGuiHost::Acquire()
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	DWORD dwUnit = 0;
	if (dwStandardWnds == 0 || dwStandardWnds > MAX_WND)
	{
		return bRet;
	}
	// 1. 逐窗口建 D3D11 设备 / 交换链 / 渲染目标(窗口客户区尺寸即交换链尺寸)
	for (dwUnit = 0; dwUnit < dwStandardWnds; dwUnit++)
	{
		WndUnit& unit = Units[dwUnit];
		if (unit.hWnd == NULL || !unit.d3d.Initialize(unit.hWnd))
		{
			goto ACQUIRE_CLN;
		}
	}
	// 2. 逐窗口建 ImGui 面(上下文 → IO 配置 → 中文字体 → 两端后端 → 样式 → IME 上下文)
	for (dwUnit = 0; dwUnit < dwStandardWnds; dwUnit++)
	{
		if (!InitImGuiUnit(dwUnit))
		{
			goto ACQUIRE_CLN;
		}
	}
	// 3. 应用可见性策略: 建窗样式不含 WS_VISIBLE ⇒ 显示必须显式发起; 本处完成首次显示(此时面已就绪,
	//    不会出现"窗口先出现而内容未就绪"的空窗), 并保证可见性与宿主调用 SetActiveWndCount 的顺序无关
	ApplyActiveVisibility();
	ResetSegmentState();
	bRet = TRUE;
	return bRet;
ACQUIRE_CLN:
	// 回退: 先关 ImGui 面(须在 D3D 设施有效期内), 再关 D3D 设施
	for (dwUnit = 0; dwUnit < MAX_WND; dwUnit++)
	{
		ShutdownImGuiUnit(dwUnit);
	}
	for (dwUnit = 0; dwUnit < MAX_WND; dwUnit++)
	{
		Units[dwUnit].d3d.Shutdown();
	}
	ResetSegmentState();
	return bRet;
}

// 函数: Release
// 描述: 释放全部窗口的 ImGui 面与 D3D11 设施(幂等, 不动窗口)
VOID ImGuiHost::Release()
{
	for (DWORD i = 0; i < MAX_WND; i++)
	{
		ShutdownImGuiUnit(i);
		Units[i].d3d.Shutdown();
	}
	ResetSegmentState();
}

// 函数: DestroyWnds
// 描述: 销毁全部标准窗口并注销共用窗口类(幂等)
VOID ImGuiHost::DestroyWnds()
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	// 1. 先销毁全部窗口(窗口类在仍被窗口使用时不能注销)
	for (DWORD i = 0; i < MAX_WND; i++)
	{
		WndUnit& unit = Units[i];
		if (unit.hWnd != NULL)
		{
			DestroyWindow(unit.hWnd);
			unit.hWnd = NULL;
		}
		unit.atomClass = 0;
		unit.fFontSize = 0.0f;
		unit.dwRounding = 0;
		unit.bRoundByRgn = FALSE;
		unit.bRoundDone = FALSE;
		unit.szTitle[0] = '\0';
		unit.stateShow = ImGuiHost::WND_SHOW_NEVER;
		unit.bShowChangeByUs = FALSE;
	}
	// 2. 共用窗口类只注销一次
	if (atomSharedClass != 0)
	{
		UnregisterClassW(MAKEINTATOM(atomSharedClass), hInstance);
		atomSharedClass = 0;
	}
	// 说明: 已声明的标准窗口数量不清(声明归 SetStandardWndCount, 便于失败后按原声明重试)
	dwActiveWnds = 0;
	ResetSegmentState();
}

// 函数: GetWindow
// 描述: 取指定窗口的窗口句柄(序号越界返回空)
HWND ImGuiHost::GetWindow(DWORD index)
{
	if (index >= MAX_WND)
	{
		return NULL;
	}
	return Units[index].hWnd;
}

// 函数: GetContext
// 描述: 取指定窗口的 ImGui 上下文(序号越界或未就绪返回空)
ImGuiContext* ImGuiHost::GetContext(DWORD index)
{
	if (index >= MAX_WND)
	{
		return NULL;
	}
	return Units[index].ctx;
}

// 函数: IsReady
// 描述: 查询指定窗口的 ImGui 面是否已就绪
BOOL ImGuiHost::IsReady(DWORD index)
{
	if (index >= MAX_WND)
	{
		return FALSE;
	}
	return (Units[index].ctx != NULL) ? TRUE : FALSE;
}

// ================================
// 区域: 帧流程实现区
// ================================

// 函数: Run
// 描述: 运行主消息循环(每帧调一次外部处理流程; 退出判据由外部持有)
VOID ImGuiHost::Run(FrameFunc pfnFrame, VOID* pUser, CONST BOOL* pbExit)
{
	if (pfnFrame == NULL || pbExit == NULL)
	{
		return;
	}
	// 外部退出判据(每帧只读其地址; 本模块不持有也不修改该变量)
	CONST BOOL* pbExternalExit = pbExit;
	BOOL bQuit = FALSE;					// 本次消息泵是否遇到 WM_QUIT
	while (bQuit == FALSE && *pbExternalExit == FALSE)
	{
		// 1. 消息泵(本帧的渲染目标取用游标随之归零)
		dwCursor = 0;
		{
			MSG msg;
			while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
				{
					bQuit = TRUE;
				}
			}
		}
		if (bQuit == TRUE || *pbExternalExit != FALSE)
		{
			break;
		}
		// 2. 外部处理流程(每帧固定一次): 数据层更新 + 按渲染清单顺序调布局渲染出口
		pfnFrame(pUser);
		// 3. 兜底结算: 处理流程未走满渲染清单(或中途失败)时, 本帧不得留下未结算的 ImGui 帧
		FinalizeSegment();
	}
	// 退出前兜底: 任何路径都不得留下未结算的段
	FinalizeSegment();
}

// 函数: AcquireNextWnd
// 描述: 结算上一段并取得下一个渲染目标窗口(含几何结算、标题同步与遮挡判定)
BOOL ImGuiHost::AcquireNextWnd(CONST vec2& pos, CONST vec2& size, LPCSTR szTitle)
{
	if (size.x <= 0.0f || size.y <= 0.0f)
	{
		return FALSE;
	}
	// 1. 结算上一段: 同一帧内的下一个目标窗口必须在前一段的 ImGui 帧结束之后才开帧
	FinalizeSegment();
	// 2. 取下一个标准窗口(游标越界 ⇒ 本帧的渲染清单已走满)
	if (dwCursor >= dwStandardWnds || dwCursor >= MAX_WND)
	{
		return FALSE;
	}
	WndUnit& unit = Units[dwCursor];
	if (unit.hWnd == NULL || unit.ctx == NULL || unit.d3d.GetSwapChain() == NULL)
	{
		return FALSE;
	}
	// 3. 遮挡判定: 交换链被遮挡(如整窗被覆盖)时本段不提交、不呈现
	//    说明: 用"测试呈现"问一次 DXGI(不提交任何内容); 遮挡期间本段直接失败 ⇒ 渲染出口静默跳过本帧
	if (unit.d3d.IsSwapChainOccluded() &&
		unit.d3d.Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
	{
		return FALSE;
	}
	unit.d3d.SetSwapChainOccluded(FALSE);
	// 4. 位置结算: 期望位置与当前不符即移动本窗口(期望值由调用方以参数给出 ⇒ 本模块不持位置状态)
	//    说明: 与下一步的尺寸纠偏同属"窗口几何结算", 都在本窗口开帧之前完成 ⇒ 绘制期内不再改几何;
	//          期望位置来自布局的窗口位置记录(基座布局 = WND 控件的 position), 本模块不解释其来源
	{
		RECT rcWnd{};
		if (GetWindowRect(unit.hWnd, &rcWnd) == FALSE)
		{
			return FALSE;
		}
		CONST INT nWantX = (INT)pos.x;
		CONST INT nWantY = (INT)pos.y;
		if (rcWnd.left != nWantX || rcWnd.top != nWantY)
		{
			// 只改位置: 尺寸与 Z 序均不动(WS_POPUP 无边框 ⇒ 外框即客户区)
			if (SetWindowPos(unit.hWnd, NULL, nWantX, nWantY, 0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) == FALSE)
			{
				return FALSE;
			}
		}
	}
	// 5. 尺寸同步: 客户区与期望尺寸不符时主动调整本窗口(位置不动; 含交换链与圆角同步)
	{
		CONST DWORD dwWantW = (DWORD)size.x;
		CONST DWORD dwWantH = (DWORD)size.y;
		RECT rcClient{};
		if (!GetClientRect(unit.hWnd, &rcClient))
		{
			return FALSE;
		}
		CONST DWORD dwCurW = (DWORD)(rcClient.right - rcClient.left);
		CONST DWORD dwCurH = (DWORD)(rcClient.bottom - rcClient.top);
		if (dwCurW != dwWantW || dwCurH != dwWantH)
		{
			// 只改尺寸: 位置与 Z 序均不动(WS_POPUP 无边框 ⇒ 外框即客户区);
			// 不加 SWP_NOSENDCHANGING ⇒ WM_SIZE 照常送达(尺寸同步后圆角按新尺寸重设)
			if (SetWindowPos(unit.hWnd, NULL, 0, 0, (INT)dwWantW, (INT)dwWantH,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) == FALSE)
			{
				return FALSE;
			}
			// 交换链尺寸随客户区更新(下一段结算前生效; WM_SIZE 未送达时由此兜底)
			unit.d3d.SetResizeDimensions(dwWantW, dwWantH);
			unit.d3d.ProcessResize();
			dwResizeCount++;
			dwResizeLastW = dwWantW;
			dwResizeLastH = dwWantH;
		}
	}
	// 6. 标题同步: 期望标题与上次应用的不同即转 UTF-16 调 SetWindowTextW(title 为 NULL ⇒ 不改)
	//    说明: 标题只影响任务栏与窗口枚举(无系统标题栏); 缓存上次应用值 ⇒ 稳态每帧只做一次比较
	if (szTitle != NULL && strcmp(unit.szTitle, szTitle) != 0)
	{
		WCHAR szWideTitle[256]{};
		if (MultiByteToWideChar(CP_UTF8, 0, szTitle, -1, szWideTitle,
			(INT)(sizeof(szWideTitle) / sizeof(szWideTitle[0]))) > 0)
		{
			SetWindowTextW(unit.hWnd, szWideTitle);
		}
		strncpy_s(unit.szTitle, sizeof(unit.szTitle), szTitle, _TRUNCATE);
	}
	// 7. 开启本窗口的段(两端后端 NewFrame + ImGui::NewFrame)并推进游标
	if (!StartSegment(dwCursor))
	{
		return FALSE;
	}
	dwCursor++;
	return TRUE;
}

// 函数: GetCurrentWnd
// 描述: 取当前渲染段的窗口句柄(无段时为空)
HWND ImGuiHost::GetCurrentWnd()
{
	if (bSegmentOpen == FALSE || dwSegmentIndex >= MAX_WND)
	{
		return NULL;
	}
	return Units[dwSegmentIndex].hWnd;
}

// 函数: GetWndPos
// 描述: 取当前段窗口的桌面位置(真实窗口矩形)
BOOL ImGuiHost::GetWndPos(vec2* pPos)
{
	if (bSegmentOpen == FALSE || dwSegmentIndex >= MAX_WND)
	{
		return FALSE;
	}
	RECT rcWnd{};
	if (Units[dwSegmentIndex].hWnd == NULL || GetWindowRect(Units[dwSegmentIndex].hWnd, &rcWnd) == FALSE)
	{
		return FALSE;
	}
	if (pPos != NULL)
	{
		pPos->x = (FLOAT)rcWnd.left;
		pPos->y = (FLOAT)rcWnd.top;
	}
	return TRUE;
}

// 函数: ResetWndCursor
// 描述: 把本帧的渲染目标取用游标归零(供宿主在同步等待等自渲染循环中重走渲染清单)
VOID ImGuiHost::ResetWndCursor()
{
	dwCursor = 0;
}

// 函数: FinalizeFrame
// 描述: 结算当前已开启的渲染段(提交绘制 / 应用 IME / 呈现并清理后台缓冲)
VOID ImGuiHost::FinalizeFrame()
{
	FinalizeSegment();
}

// 函数: SetIMEDisabled
// 描述: 设置本帧的 IME 覆盖(段结算时应用, 结算后失效)
VOID ImGuiHost::SetIMEDisabled(BOOL bDisabled)
{
	bIMEDisabledOverride = (bDisabled != FALSE) ? TRUE : FALSE;
}

// 函数: GetRenderStats
// 描述: 取绘制量自证计数(段数 / 绘制命令数 / 顶点数 / 索引数)
VOID ImGuiHost::GetRenderStats(DWORD* pdwSegments, DWORD* pdwCmdLists, INT64* pllVtx, INT64* pllIdx)
{
	if (pdwSegments != NULL) { *pdwSegments = dwSegments; }
	if (pdwCmdLists != NULL) { *pdwCmdLists = dwCmdLists; }
	if (pllVtx != NULL) { *pllVtx = llVtxCount; }
	if (pllIdx != NULL) { *pllIdx = llIdxCount; }
}

// 函数: GetResizeStats
// 描述: 取窗口尺寸调整计数与最近一次调整后的客户区尺寸
VOID ImGuiHost::GetResizeStats(DWORD* pdwCount, DWORD* pdwLastW, DWORD* pdwLastH)
{
	if (pdwCount != NULL) { *pdwCount = dwResizeCount; }
	if (pdwLastW != NULL) { *pdwLastW = dwResizeLastW; }
	if (pdwLastH != NULL) { *pdwLastH = dwResizeLastH; }
}

// ================================
// 区域: 文件内函数实现区
// ================================

// 函数: ShowWnd
// 描述: 发起指定窗口的显示或隐藏并标记本次变更的来源
static VOID ShowWnd(DWORD index, BOOL bShow)
{
	if (index >= ImGuiHost::MAX_WND)
	{
		return;
	}
	WndUnit& unit = Units[index];
	if (unit.hWnd == NULL)
	{
		return;
	}
	// 标记"本次可见性变更由本模块发起": WM_SHOWWINDOW 随后同步到达, 据此把隐藏记为可恢复的一类
	unit.bShowChangeByUs = TRUE;
	ShowWindow(unit.hWnd, (bShow != FALSE) ? SW_SHOW : SW_HIDE);
	unit.bShowChangeByUs = FALSE;
}

// 函数: ApplyActiveVisibility
// 描述: 按激活窗口数应用可见性策略(不足者显示, 超出者隐藏)
static VOID ApplyActiveVisibility()
{
	for (DWORD i = 0; i < dwStandardWnds; i++)
	{
		WndUnit& unit = Units[i];
		if (unit.hWnd == NULL)
		{
			continue;
		}
		if (i < dwActiveWnds)
		{
			// 本窗口应可见: 从未显示过(首次显示)或被本策略隐藏过 ⇒ 由本模块负责显示
			if (unit.stateShow == ImGuiHost::WND_SHOW_NEVER ||
				unit.stateShow == ImGuiHost::WND_SHOW_HIDDEN_BY_ACTIVE)
			{
				ShowWnd(i, TRUE);
			}
		}
		else
		{
			// 超出激活范围 ⇒ 多余的标准窗口隐藏(宿主若重新显示, 下一次应用会再次隐藏)
			if (unit.stateShow != ImGuiHost::WND_SHOW_HIDDEN_BY_ACTIVE &&
				unit.stateShow != ImGuiHost::WND_SHOW_HIDDEN_BY_HOST)
			{
				ShowWnd(i, FALSE);
			}
		}
	}
}

// 函数: FindUnitIndex
// 描述: 按窗口句柄反查标准窗口单元序号(未命中返回 -1)
static INT FindUnitIndex(HWND hWnd)
{
	if (hWnd == NULL)
	{
		return -1;
	}
	for (DWORD i = 0; i < ImGuiHost::MAX_WND; i++)
	{
		if (Units[i].hWnd == hWnd)
		{
			return (INT)i;
		}
	}
	return -1;
}

// 函数: ApplyUnitStyle
// 描述: 对指定窗口重放亮暗基础样式、高级样式回调与已登记的宿主样式
static VOID ApplyUnitStyle(DWORD index)
{
	if (index >= ImGuiHost::MAX_WND)
	{
		return;
	}
	WndUnit& unit = Units[index];
	if (unit.ctx == NULL)
	{
		return;
	}
	ImGui::SetCurrentContext(unit.ctx);
	// 1. 亮暗基础样式(ImGui 内建控件的底色 / 文本 / 边框一套)
	if (bDarkBase)
	{
		ImGui::StyleColorsDark();
	}
	else
	{
		ImGui::StyleColorsLight();
	}
	// 2. 高级样式回调(宿主接口表之外的直调用法; 逐槽填充, 在本层样式之前叠加)
	if (pfnStyleFunc != NULL)
	{
		pfnStyleFunc(ImGui::GetStyle(), pStyleUser);
	}
	// 3. 已登记的宿主样式(框架口径: 圆角与配色槽; 后于高级回调应用 ⇒ 框架契约优先)
	//    映射口径 = 逐槽取 GDI 色值经 MAKE_CR_IMVEC4 转 ImVec4, 与调度层填入 StyleDesc 的取值同源
	if (bStyleDescSet)
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = styleDesc.fWindowRounding;
		style.FrameRounding = styleDesc.fFrameRounding;
		style.ChildRounding = styleDesc.fChildRounding;
		style.PopupRounding = styleDesc.fPopupRounding;
		style.Colors[ImGuiCol_WindowBg] = MAKE_CR_IMVEC4(styleDesc.crWindowBg);
		style.Colors[ImGuiCol_ChildBg] = MAKE_CR_IMVEC4(styleDesc.crChildBg);
		style.Colors[ImGuiCol_FrameBg] = MAKE_CR_IMVEC4(styleDesc.crFrameBg);
		style.Colors[ImGuiCol_FrameBgHovered] = MAKE_CR_IMVEC4(styleDesc.crFrameBgHovered);
		style.Colors[ImGuiCol_FrameBgActive] = MAKE_CR_IMVEC4(styleDesc.crFrameBgActive);
		style.Colors[ImGuiCol_Button] = MAKE_CR_IMVEC4(styleDesc.crButton);
		style.Colors[ImGuiCol_ButtonHovered] = MAKE_CR_IMVEC4(styleDesc.crButtonHovered);
		style.Colors[ImGuiCol_ButtonActive] = MAKE_CR_IMVEC4(styleDesc.crButtonActive);
		style.Colors[ImGuiCol_Border] = MAKE_CR_IMVEC4(styleDesc.crBorder);
		style.Colors[ImGuiCol_Text] = MAKE_CR_IMVEC4(styleDesc.crText);
		style.Colors[ImGuiCol_TextDisabled] = MAKE_CR_IMVEC4(styleDesc.crTextDisabled);
		style.Colors[ImGuiCol_SliderGrab] = MAKE_CR_IMVEC4(styleDesc.crSliderGrab);
		style.Colors[ImGuiCol_SliderGrabActive] = MAKE_CR_IMVEC4(styleDesc.crSliderGrabActive);
		style.Colors[ImGuiCol_CheckMark] = MAKE_CR_IMVEC4(styleDesc.crCheckMark);
		style.Colors[ImGuiCol_Header] = MAKE_CR_IMVEC4(styleDesc.crHeader);
		style.Colors[ImGuiCol_HeaderHovered] = MAKE_CR_IMVEC4(styleDesc.crHeaderHovered);
		style.Colors[ImGuiCol_HeaderActive] = MAKE_CR_IMVEC4(styleDesc.crHeaderActive);
	}
}

// 函数: LoadFonts
// 描述: 逐个试载中文字体候选并自校准 em 换算(全部失败退默认字体)
static BOOL LoadFonts(HWND hWnd, ImGuiContext* ctx, FLOAT fFontSize)
{
	BOOL bRet = FALSE;
	ImGui::SetCurrentContext(ctx);
	ImGuiIO& io = ImGui::GetIO();
	// 1. 尝试加载系统常见中文字体(含 em 换算, 见下)
	//    em 换算: 本工程的几何口径是"1 个全宽汉字 = 1 个字号", 而 ImGui 的 SizePixels 语义是
	//    "字体行高(Ascent - Descent)" ⇒ 1 个 em 只占 SizePixels x em/(Ascent - Descent) 宽。
	//    处置 = 用 ImFontConfig::ExtraSizeScale 把字形等比放大 k 倍(k = (Ascent - Descent)/unitsPerEm),
	//    不动字号 ⇒ 字号/行高/帧高等 ImGui 固有度量零变化;
	//    k 由自校准求得(不依赖字体度量表): 先以 k = 1 试载, 量 4 个全宽汉字的实宽, 再复算。
	CONST char* FontFiles[] = {
		"C:\\Windows\\Fonts\\msyh.ttc",
		"C:\\Windows\\Fonts\\simhei.ttf",
		"C:\\Windows\\Fonts\\simsun.ttc",
		NULL
	};
	io.Fonts->Clear();
	io.FontDefault = NULL;				// 上一步已销毁原字体 ⇒ 先断开缺省字体指针(循环内还会再 Clear 一次)
	ImFont* pFont = NULL;
	if (fFontSize < FONT_SIZE_MIN) { fFontSize = DEFAULT_FONT_SIZE; }
	for (INT i = 0; FontFiles[i] != NULL; i++)
	{
		// 1.1 自校准: 试载本候选字体, 量 4 个全宽汉字的精确实宽, 求 em 换算系数 k
		//     (试载结果随后由 Clear 丢弃; 此处帧外即可度量 —— 后端尚未 Init, 起不了帧也不需要帧)
		FLOAT fEmScale = 1.0f;
		{
			ImFontConfig cfgProbe;			// ExtraSizeScale 取默认 1.0(即换算前的原始字形度量)
			ImFont* pProbe = io.Fonts->AddFontFromFileTTF(FontFiles[i], fFontSize, &cfgProbe, io.Fonts->GetGlyphRangesChineseFull());
			if (pProbe == NULL)
			{
				continue;					// 本候选字体不可用: 试下一个候选
			}
			CONST ImVec2 tsProbe = pProbe->CalcTextSizeA(fFontSize, FLT_MAX, 0.0f, u8"汉字宽度");
			if (tsProbe.x > 0.0f)
			{
				fEmScale = fFontSize / (tsProbe.x / 4.0f);	// 使 1 个全宽汉字 = fFontSize 像素
			}
			io.Fonts->Clear();				// 丢弃探测用字体(正式加载见 1.2)
		}
		// 1.2 正式加载: 按 k 放大字形(字号本身不变 ⇒ 行高/固有帧高不变)
		ImFontConfig cfg;
		cfg.ExtraSizeScale = fEmScale;
		pFont = io.Fonts->AddFontFromFileTTF(FontFiles[i], fFontSize, &cfg, io.Fonts->GetGlyphRangesChineseFull());
		if (pFont) { break; }
	}
	if (pFont == NULL)
	{
		io.Fonts->AddFontDefault();
		// 宽字面量 + W 版弹窗: 源码以 /utf-8 编译, 窄字面量直喂 ANSI 版会中文乱码
		MessageBoxW(hWnd, L"未能加载中文字体, 将使用默认字体。", L"字体警告", MB_OK | MB_ICONWARNING);
	}
	else
	{
		io.FontDefault = pFont;
	}
	bRet = TRUE;
	return bRet;
}

// 函数: InitImGuiUnit
// 描述: 建立单个窗口的 ImGui 面(上下文 / 字体 / 两端后端 / 样式 / IME / 能力注入)
static BOOL InitImGuiUnit(DWORD index)
{
	BOOL bRet = FALSE;
	if (index >= ImGuiHost::MAX_WND)
	{
		return bRet;
	}
	WndUnit& unit = Units[index];
	if (unit.hWnd == NULL || unit.d3d.GetDevice() == NULL || unit.d3d.GetDeviceContext() == NULL)
	{
		return bRet;
	}
	// 1. 本窗口的 ImGui 上下文与 IO 配置(不落盘 imgui.ini; 键盘导航开; 关闭窗口栈自动恢复)
	unit.ctx = ImGui::CreateContext();
	if (unit.ctx == NULL)
	{
		return bRet;
	}
	ImGui::SetCurrentContext(unit.ctx);
	{
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = NULL;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigErrorRecovery = false;
	}
	// 2. 中文字体(须在两端后端之前加载: 后端 Init 时会把字体图集上传为纹理)
	if (!LoadFonts(unit.hWnd, unit.ctx, unit.fFontSize))
	{
		goto INIT_CLN;
	}
	// 3. 两端后端(顺序与官方示例一致: Win32 先、DX11 后; 释放时逆序)
	if (!ImGui_ImplWin32_Init(unit.hWnd))
	{
		goto INIT_CLN;
	}
	if (!ImGui_ImplDX11_Init(unit.d3d.GetDevice(), unit.d3d.GetDeviceContext()))
	{
		goto INIT_CLN;
	}
	// 4. 样式: 亮暗基础样式 + 高级样式回调 + 已登记的宿主样式
	ApplyUnitStyle(index);
	// 5. IME 上下文(输入法开关归本模块; 须在窗口有效期内取得)
	unit.hIMC = ImmGetContext(unit.hWnd);
	// 6. 能力注入(幂等): 把 ImGui 侧的渲染能力表与控件库的绘制接口表交给布局渲染出口(LayoutRender)
	//    说明: ① 两张表均为进程级静态、与窗口存续无关 ⇒ 多窗口重复注入同一张表无副作用;
	//          ② 换用其它 UI 实现时, 只需在此处改为注入另一份同形态的能力表, 消费件零改动;
	//          ③ 注入后 LayoutRender 的各渲染入口方可工作(未注入 ⇒ 静默失败)。
	LayoutRender::SetRenderApi(ImGuiRender::GetApi());
	LayoutRender::SetCtrlApi(Controls::GetCtrlApi());
	bRet = TRUE;
	return bRet;
INIT_CLN:
	// 回退: 已建后端逐项关停, 最后销毁上下文(与 ShutdownImGuiUnit 同序)
	ImGui::SetCurrentContext(unit.ctx);
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext(unit.ctx);
	unit.ctx = NULL;
	return bRet;
}

// 函数: ShutdownImGuiUnit
// 描述: 关停单个窗口的 ImGui 面并归还 IME 上下文
static VOID ShutdownImGuiUnit(DWORD index)
{
	if (index >= ImGuiHost::MAX_WND)
	{
		return;
	}
	WndUnit& unit = Units[index];
	if (unit.ctx != NULL)
	{
		// 后端须在 D3D 设施有效期内关停(其纹理与设备上下文挂在本窗口的上下文中)
		ImGui::SetCurrentContext(unit.ctx);
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext(unit.ctx);
		unit.ctx = NULL;
	}
	// IME 上下文须在窗口有效期内归还
	if (unit.hIMC != NULL && unit.hWnd != NULL)
	{
		ImmReleaseContext(unit.hWnd, unit.hIMC);
	}
	unit.hIMC = NULL;
}

// 函数: StartSegment
// 描述: 开启指定窗口的渲染段(两端后端开帧并结束当前 ImGui 帧的准备)
static BOOL StartSegment(DWORD index)
{
	if (index >= ImGuiHost::MAX_WND || bSegmentOpen == TRUE)
	{
		return FALSE;
	}
	WndUnit& unit = Units[index];
	if (unit.ctx == NULL)
	{
		return FALSE;
	}
	// 交换链尺寸与本窗口客户区同步(WM_SIZE 未送达时的兜底; 无待处理尺寸时本调用无副作用)
	unit.d3d.ProcessResize();
	ImGui::SetCurrentContext(unit.ctx);
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	// 段前回调: 后端开帧之后再交给宿主(可在此注入合成输入事件 —— 后端每帧会先覆盖真实光标位置)
	if (pfnSegmentFunc != NULL)
	{
		pfnSegmentFunc(index, pSegmentUser);
	}
	ImGui::NewFrame();
	bSegmentOpen = TRUE;
	dwSegmentIndex = index;
	return TRUE;
}

// 函数: FinalizeSegment
// 描述: 结算当前渲染段(提交绘制 / 应用 IME / 呈现并清理后台缓冲)
// 说明: 本函数只清"段"状态, 不清本帧的渲染目标取用游标 —— 游标归帧级复位
//       (Run 的帧末兜底与 ResetWndCursor); 否则同一帧内连续取多个窗口时会被反复拉回 0
static VOID FinalizeSegment()
{
	if (bSegmentOpen == FALSE || dwSegmentIndex >= ImGuiHost::MAX_WND)
	{
		return;
	}
	CONST DWORD dwIndex = dwSegmentIndex;
	WndUnit& unit = Units[dwIndex];
	// 先清段状态: 本函数内任何时候返回都不得留下"已开帧未结束"的假状态
	bSegmentOpen = FALSE;
	dwSegmentIndex = 0;
	bIMEDisabledOverride = FALSE;
	if (unit.ctx == NULL || unit.d3d.GetDeviceContext() == NULL)
	{
		return;
	}
	ImGui::SetCurrentContext(unit.ctx);
	// 1. 结束本帧(ImGui::Render 与后端 RenderDrawData 成对)
	ImGui::Render();
	ImDrawData* pDrawData = ImGui::GetDrawData();
	// 2. 绑定本窗口渲染目标并提交绘制
	ID3D11RenderTargetView* pRtv = unit.d3d.GetMainRenderTargetView();
	if (pRtv != NULL)
	{
		unit.d3d.GetDeviceContext()->OMSetRenderTargets(1, &pRtv, NULL);
	}
	if (pDrawData != NULL)
	{
		dwCmdLists += (DWORD)pDrawData->CmdListsCount;
		llVtxCount += (INT64)pDrawData->TotalVtxCount;
		llIdxCount += (INT64)pDrawData->TotalIdxCount;
	}
	ImGui_ImplDX11_RenderDrawData(pDrawData);
	dwSegments++;
	// 3. IME: 本段覆盖值优先, 否则取 ImGui 的输入请求(编辑中的输入控件 ⇒ 关闭输入法)
	{
		CONST BOOL bDisable = (bIMEDisabledOverride != FALSE) ?
			TRUE : ((ImGui::GetIO().WantTextInput != false) ? TRUE : FALSE);
		if (unit.hIMC != NULL)
		{
			if (bDisable != FALSE)
			{
				if (ImmGetOpenStatus(unit.hIMC)) { ImmSetOpenStatus(unit.hIMC, FALSE); }
			}
			else
			{
				if (!ImmGetOpenStatus(unit.hIMC)) { ImmSetOpenStatus(unit.hIMC, TRUE); }
			}
		}
	}
	// 4. 呈现(同步间隔 1; 呈现结果自带遮挡状态, 供下一段的遮挡判定使用)
	unit.d3d.Present(1, 0);
	// 5. 后台缓冲清理(与既有时序一致: 呈现之后清空, 下一帧重绘)
	{
		CONST FLOAT clearMain[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		unit.d3d.ClearRenderTargetView(clearMain);
	}
}

// 函数: ResetSegmentState
// 描述: 复位段状态(段标记 / 段序号 / 渲染游标 / IME 覆盖)
static VOID ResetSegmentState()
{
	bSegmentOpen = FALSE;
	dwSegmentIndex = 0;
	dwCursor = 0;
	bIMEDisabledOverride = FALSE;
}

// ================================
// 区域: 窗口过程实现区
// ================================

// 函数: HostWndProc
// 描述: 共用窗口过程: 记账显示状态与尺寸, 处理退出键与销毁消息, 其余交 DefWindowProcW
static LRESULT WINAPI HostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	INT index = FindUnitIndex(hWnd);
	// WM_SHOWWINDOW: 可见性发生变化 ⇒ 更新本模块自持的显示状态(本模块发起与外部发起分别记账)
	// 说明: ShowWindow 同步发出本消息 ⇒ 本模块发起的显示 / 隐藏也走此处记账, 状态只有这一个写点;
	//       本分支不消费消息(仍交 ImGui 后端与宿主过程), 只同步本模块对可见性的认知
	if (msg == WM_SHOWWINDOW && index >= 0 && index < (INT)ImGuiHost::MAX_WND)
	{
		WndUnit& unit = Units[index];
		if (wParam != 0)
		{
			unit.stateShow = ImGuiHost::WND_SHOW_VISIBLE;
		}
		else
		{
			// 由本模块隐藏 ⇒ 记为可恢复的一类; 其余(宿主 / 系统)⇒ 记为宿主隐藏, 本模块不擅自恢复
			unit.stateShow = (unit.bShowChangeByUs != FALSE) ?
				ImGuiHost::WND_SHOW_HIDDEN_BY_ACTIVE : ImGuiHost::WND_SHOW_HIDDEN_BY_HOST;
		}
	}
	// WM_SIZE: 客户区尺寸变化 ⇒ 记账给本窗口的交换链(下一段开帧时重设缓冲), 并按新尺寸重设圆角
	// 说明: 本分支不消费消息(仍交宿主与 DefWindowProc), 只做本模块自持设施的同步
	if (msg == WM_SIZE && wParam != SIZE_MINIMIZED && index >= 0 && index < (INT)ImGuiHost::MAX_WND)
	{
		WndUnit& unit = Units[index];
		CONST DWORD dwW = (DWORD)LOWORD(lParam);
		CONST DWORD dwH = (DWORD)HIWORD(lParam);
		unit.d3d.SetResizeDimensions(dwW, dwH);
		// RGN 路径的圆角按新尺寸重设(DWM 路径由系统跟随); 半径 0 表示本窗口不设圆角
		if (unit.dwRounding > 0 && unit.bRoundByRgn != FALSE && unit.bRoundDone != FALSE)
		{
			AppEnv::ApplyWindowCornerRounding(hWnd, dwW, dwH, unit.dwRounding, &unit.bRoundByRgn);
		}
	}
	// 先把当前上下文切到本窗口(后端数据按上下文挂载); 返回前还原进入前的上下文
	// 注: 建窗期与已释放期单元上下文为空 ⇒ 不切换、不转后端, 直接交宿主过程
	ImGuiContext* pPrevCtx = ImGui::GetCurrentContext();
	BOOL bSwitched = FALSE;
	if (index >= 0 && Units[index].ctx != NULL)
	{
		ImGui::SetCurrentContext(Units[index].ctx);
		bSwitched = TRUE;
	}
	if (bSwitched && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
	{
		ImGui::SetCurrentContext(pPrevCtx);
		return TRUE;
	}
	LRESULT lrRet = 0;
	// 内置退出处理: 销毁消息与 ESC 一律请求退出主循环; 其余消息交系统默认过程
	// 说明: 本模块已收回窗口过程 ⇒ 不再对外暴露业务窗口过程, 也不再转调宿主过程
	switch (msg)
	{
	case WM_DESTROY:
	{
		::PostQuitMessage(0);
		lrRet = 0;
		break;
	}
	case WM_KEYDOWN:
	{
		// ESC 退出(无应用交互 ⇒ 直接退出)
		if (wParam == VK_ESCAPE)
		{
			::PostQuitMessage(0);
			lrRet = 0;
		}
		else
		{
			lrRet = ::DefWindowProcW(hWnd, msg, wParam, lParam);
		}
		break;
	}
	default:
	{
		lrRet = ::DefWindowProcW(hWnd, msg, wParam, lParam);
		break;
	}
	}
	if (bSwitched)
	{
		ImGui::SetCurrentContext(pPrevCtx);
	}
	return lrRet;
}
