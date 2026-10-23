// ================================
// 文件: AppHost.cpp
// 描述: 宿主层实现 —— 双窗口宿主(编辑窗口 + 预览窗口)的完整生命周期:
//   · 宿主状态(宿主投影配色 / 无人值守参数 / 提示模态标记)
//   · 启动初始化(①事件绑定表与预览点击记录器注入 + 应用文档 ②布局编辑器 ③声明两枚标准窗口
//     ④建立 ImGui 面与渲染能力 ⑤无人值守参数 + 外壳快照校验)
//   · 主循环(由 ImGuiHost 驱动; 每帧按渲染清单顺序渲染编辑窗口与预览窗口) / 同步提示展示器 / 窗口过程
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include "imgui.h"
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/AppHost.h"
#include "framework/base/AppEnv.h"
#include "framework/base/BasicDesign.h"
#include "app/Application.h"
#include "app/FlexHost.h"			// 内嵌(flex)窗口装配与查询
#include "app/DocumentIO.h"			// 文档文件 I/O 与格式分派(命令面回调接线在本文件)
#include "app/PreviewSession.h"		// 预览基座与帧间同步 + 外壳快照(会话回调接线在本文件)
#include "app/HostContext.h"			// 宿主能力与窗口句柄
#include "app/SessionContracts.h"		// 会话语义契约(提示展示 / 会话面回调 / 布局编辑命令)
#include "framework/render/imgui/control/Controls.h"
#include "framework/base/ControlSpec.h"
#include "framework/base/CtrlBuffer.h"			// CTRL_TEXT_BUF(提示布局按钮文本备份缓冲)
#include "framework/base/TextWidth.h"			// CalcTextWidth(提示按钮文本宽度自适应)
#include "framework/render/imgui/ImGuiHost.h"			// 窗口与 ImGui 面宿主(主循环 / 每窗口设施 / 圆角 / IME)
#include "framework/render/imgui/ability/ImGuiRender.h"		// ImGui 侧渲染能力提供者
#include "framework/layout/LayoutRender.h"	// 布局渲染出口
#include "framework/dispatch/UIEventHandler.h"	// 事件绑定表注入(表内容在本编辑器侧)
#include "editor/GenEditor.h"
#include "editor/EditorCommands.h"	// 命令层会话回调注入
#include "editor/SelectionOverlay.h"	// 预览选中态视图叠加件(链外绘制; 见其文件头说明)
#include "framework/dispatch/FrameEvent.h"

// ================================
// 区域: 宿主状态定义区
// 说明: 本区状态全部为宿主(进程)私有: 投影配色 / 无人值守参数 / 提示模态标记 ——
//       窗口句柄 / 每窗口渲染设施 / ImGui 上下文由 framework\ImGuiHost 持有。
// ================================
constexpr DWORD WND_EDIT = 0;			// 标准窗口序号: 编辑窗口
constexpr DWORD WND_PREVIEW = 1;		// 标准窗口序号: 预览窗口
constexpr DWORD WND_COUNT = 2;			// 本应用声明的标准窗口数量

static DWORD dwAutoCloseMs = 0;		// 无人值守自动退出毫秒数(0 = 不自动退出)
static ULONGLONG qwStartTick;		// 启动时刻
static BOOL bHintModalActive = FALSE;	// 同步「提示布局」展示中(重入守卫: 展示期间不再受理再次弹提示/WM_CLOSE)
static WNDPROC pfnPrevWndProc = NULL;	// 框架窗口过程(子类化前取到的原过程; 两窗口共用)

// ================================
// 区域: 投影配色口径(宿主侧)
// 说明: 一条投影 = 一套配色; 亮暗模式 = 采样方向(不对调底色/光色); 层号 1-based。
//       因"实际工程下沉多于上浮"由 5 抬高) ⇒ 调整整屏明暗只改那一枚常量。
//         "链序最近包含者层次 + 自身类型步进"递推: 域下沉 / 件上浮 / 其余继承)决定
//         ⇒ 本区常量只服务样式槽与无控件可依的绘制件(换算口径的单一真源见
//         ControlSpec.h 的"控件层次与层号换算"区)。
// ================================
static ProjCR* pProjCR = NULL;					// 宿主投影配色(启动期建立; 退出清理释放)
constexpr DWORD PROJ_LAYERS = 16;				// 层数(采样数量; 不少于 16 —— 8 层承载不了既有观感的层次密度)
constexpr DWORD PROJ_COLOR_BASE = 0xFF141414u;	// 底色端(轴的低/暗端 = 下嵌层色; 与亮暗模式无关)
constexpr DWORD PROJ_COLOR_LIGHT = 0xFFEFEFEFu;	// 光色端(轴的高/亮端; 与亮暗模式无关)
constexpr DWORD PROJ_L_WINDOW = PROJ_LAYER_BACKGROUND;			// 窗口底 / 背景层(单点常量 = PROJ_LAYER_BACKGROUND)
constexpr DWORD PROJ_L_CHILD = PROJ_LAYER_BACKGROUND + 1;		// 内容子窗 / 容器(样式槽缺省: 背景层之上)
constexpr DWORD PROJ_L_FRAME = PROJ_LAYER_BACKGROUND + 1;		// 帧类件底(输入框 / 按钮 / 表头; 同上)
constexpr DWORD PROJ_L_DISABLED = 10;			// 禁用文本 / 次级(中灰)

// ================================
// 区域: 函数声明区
// ================================
// 函数: BizWndProc
// 描述: 两个窗口共用的业务窗口过程(子类化挂在框架窗口过程之外): 只处理关闭前的未保存提醒与
//   提示模态期间的回车/ESC, 其余消息一律经 CallWindowProc 转交框架的原窗口过程
LRESULT WINAPI BizWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 函数: InstallBizWndProc
// 描述: 把业务窗口过程挂到指定窗口上(保存框架原过程; 重复调用幂等)
// 参数:
//		IN	hWnd:	目标窗口句柄(NULL 直接返回)
static VOID InstallBizWndProc(HWND hWnd);

// 函数: FrameOnce
// 描述: 每帧外部处理流程(由 ImGuiHost 每帧调用一次): 数据层更新 + 按渲染清单顺序渲染两窗口
// 参数:
//		IN	pUser:	宿主私有参数(本宿主不用)
static VOID FrameOnce(VOID* pUser);

// 函数: RenderPreviewFrame
// 描述: 渲染预览窗口一帧(active 布局 + 逐控件点击判定 + 视图叠加件)
static VOID RenderPreviewFrame();

// 函数: OnSegmentBegin
// 描述: 段落开帧回调(预览窗口段: 在 ImGui::NewFrame 之前的预留钩子)
// 参数:
//		IN	index:	即将开帧的标准窗口序号
//		IN	pUser:	宿主私有参数(本宿主不用)
static VOID OnSegmentBegin(DWORD index, VOID* pUser);

// 函数: ProjSampleConvex
// 描述: 暗模式采样函数 —— 导数单调递增的单调递增函数(幂曲线: 前期缓、后期陡)
// 参数:
//		IN	dwParam:	采样参数(已按方向反射)
//		IN	dwLayers:	层数
// 返回:
//		FLOAT: 该参数对应的轴位置(0 = 底色端; 1 = 光色端)
static FLOAT ProjSampleConvex(DWORD dwParam, DWORD dwLayers);

// 函数: ProjSampleConcave
// 描述: 亮模式采样函数 —— 上者的镜像形态(导数单调递减的单调递增函数)
// 参数:
//		IN	dwParam:	采样参数(已按方向反射)
//		IN	dwLayers:	层数
// 返回:
//		FLOAT: 该参数对应的轴位置(0 = 底色端; 1 = 光色端)
static FLOAT ProjSampleConcave(DWORD dwParam, DWORD dwLayers);

// 函数: EnsureProjCR
// 描述: 确保宿主投影配色已建立(亮暗模式 = 采样方向; 反复调用幂等)
static VOID EnsureProjCR();

// 函数: ApplyProjStyle
// 描述: 用投影配色填充 ImGui 样式槽(17 项; 层号口径见本文件"投影配色口径"区)
// 参数:
//		IN/OUT	style:	目标 ImGui 样式(按当前 context 取)
//		IN		pUser:	宿主私有参数(本宿主不用)
static VOID ApplyProjStyle(ImGuiStyle& style, VOID* pUser);

// 函数: RenderEditFrame
// 描述: 渲染编辑窗口一帧(主循环与同步提示等待共用)
static VOID RenderEditFrame();

// 函数: FindHintCtrl
// 描述: 在指定布局内按 label 找控件(未命中返回 NULL)
// 参数:
//		IN	pLayout:	目标布局(可为 NULL)
//		IN	label:		控件标签(精确匹配; NULL 直接未命中)
// 返回:
//		CtrlInfo*: 命中的控件; 未命中返回 NULL
static CtrlInfo* FindHintCtrl(Layout* pLayout, LPCSTR label);

// 函数: SetHintCtrlText
// 描述: 把 UTF-8 文本写入指定控件的文本缓冲(超长按缓冲容量截断; 控件无缓冲则忽略)
// 参数:
//		IN	ctrl:	目标控件(可为 NULL)
//		IN	text:	新文本(UTF-8; NULL 按空串)
static VOID SetHintCtrlText(CtrlInfo* ctrl, LPCSTR text);

// 函数: BackupHintCtrlText
// 描述: 把指定控件的当前文本备份到调用方缓冲(用于"按钮文本被覆盖后恢复"; 无缓冲则备份为空串)
// 参数:
//		IN	ctrl:		目标控件(可为 NULL)
//		OUT	szBackup:	备份缓冲(写出以 '\0' 结尾的文本)
//		IN	cbBackup:	备份缓冲字节数
static VOID BackupHintCtrlText(CtrlInfo* ctrl, LPSTR szBackup, size_t cbBackup);

// 函数: HintCtrlTextOf
// 描述: 取控件当前文本(无缓冲返回空串)
// 参数:
//		IN	ctrl:	目标控件(可为 NULL)
// 返回:
//		LPCSTR: 控件文本(永不为 NULL)
static LPCSTR HintCtrlTextOf(CtrlInfo* ctrl);

// 函数: RunHintSync
// 描述: 同步展示「提示布局」并阻塞至用户选择(宿主注入给编辑器的同步提示展示器; 见 SessionContracts.h 的 pfnHintSync)
// 参数:
//		IN	spec:	展示参数(标题 / 正文 / 两个按钮文本 / 是否显示取消按钮; 见 SessionContracts.h 的 HintSpec)
// 返回:
//		BOOL: TRUE = 用户确认; FALSE = 用户取消(或提示布局不可用 / 重入 / 等待期间收到退出)
static BOOL RunHintSync(CONST HintSpec& spec);

// ================================
// 区域: 宿主生命周期实现区
// ================================

// 函数: Run
// 描述: 执行完整的宿主生命周期(启动初始化 → 主循环 → 清理链)并返回进程退出码
INT AppHost::Run()
{
	AppEnv::InitConsoleUtf8();
	INT ret = 0;
	LPCWSTR ERRTEXT = NULL;
	BOOL IsExit = FALSE;
	ImGuiHost::WndSpec specs[WND_COUNT]{};

	ImGuiHost::EnableDpiAwareness();

	// 1. 注入"事件绑定表"与"预览布局内容控件的点击事件处理器" + 初始化应用文档(GenUI.json) + 注入文档承载者
	//    ① 事件绑定表: 表内容是本编辑器的处理函数, 而 framework\UIEventHandler 不得反向依赖
	//       editor ⇒ 由宿主注入(见 UIEventHandler.h)。
	//       同样必须早于 Application::Init(Init 载入文档时即按 event 名解析处理器)。
	{
		size_t nBindings = 0;
		CONST UIEventHandler::EventBinding* pBindings = GenUIEditor::GetEventBindings(&nBindings);
		UIEventHandler::SetBindings(pBindings, nBindings);
	}
	//    ② 预览点击记录器: 注入必须早于 Application::Init(首次装配预览基座内容控件时就生效):
	//       装配点把每枚内容控件的 pEventHandler 一律改绑为该记录器(覆盖其文档 event 的原处理器,
	//       不转调) ⇒ 预览窗口里点控件只记录点击(索引 / 位置 / 大小), 不触发真实业务事件(预览是展示面,
	//       不得驱动保存 / 新建 / 打开一类动作); 结构件不覆盖(预览标题栏的拖动等既有绑定保留)。
	HostContext::SetInjectClickHandler(GenUIEditor::OnPreviewClick);
	if (!Application::Init())
	{
		ERRTEXT = L"应用程序初始化失败!";
		ret = 1;
		goto MAIN_END;
	}
	//    ③ 文档承载者(doc\Document): 本注入的时机与上面 ① ② 相反 —— 必须晚于 Application::Init():
	//       承载者按值内嵌在 AppData 内, 而 AppData 到 Application::Init() 内部才分配(见 Application.cpp
	//       "分配成员存储") ⇒ Init 之前 data == NULL, 取不到承载者(上面两处注入的是宿主侧函数与处理部件,
	//       不依赖 AppData ⇒ 可以早于 Init; 本处不可) ⇒ 不得按"注入一律放在 Init 之前"的惯例提前。
	//       复位口径: 必须在 Application::Release() 之前以 NULL 复位(见下方 MAIN_APP_RELEASE 标签处),
	//       否则承载者随 AppData 失效 ⇒ 编辑器侧留悬垂指针。
	GenUIEditor::SetDocument(Application::GetDocument());
	// 启动装配: 预览基座内容按当前 active 条目的中间态装配一次(此后由编辑动作经帧末同步更新)
	PreviewSession::SyncPreviewLayout();
	// 命令层会话面回调注入: 命令层不持会话面状态(预览唯一基座 / 文档版本号) ⇒ 布局族命令改完中间态后
	//   经本组回调"请求"驱动; 注入点与承载者同点(必须晚于 Application::Init)。
	EditorCommands::SetSessionHooks(&PreviewSession::EnsurePreviewBaseHook, &PreviewSession::SyncPreviewLayout, &PreviewSession::BumpDocRevHook);
	// 命令层命令面回调接线:
	//   DocumentIO::NewDocument 的"新建默认布局"一步 = 命令层 EditorCommands::AddDocLayout;
	//   应用侧的文档 I/O 模块不引编辑器命令头(避免 app → editor 反依赖) ⇒ 由宿主在本点把命令
	//   地址交给该模块(与上面会话回调同点、同注入时机)。承载者与会话回调三件套由 Application::Init
	//   就地注入(承载者按值内嵌在 AppData 内 ⇒ 必须晚于分配成员存储那一步)。
	DocumentIO::SetDocLayoutAdder(&EditorCommands::AddDocLayout);
	// 2. 初始化布局编辑器(注册回调)
	GenUIEditor::Init();

	// 3. 声明两枚标准窗口(编辑窗口 + 预览窗口)的几何与身份
	// 编辑窗口: 客户区尺寸 = GenUI.json 中外壳布局(GenUIEditor)的描述(autosize 千分比 × 换算基准 / 1000);
	// 快照无效(文档无 GenUIEditor)时无描述可依 ⇒ 按屏占比(34% 屏宽 × 94% 屏高)取尺寸;
	// 该情形随后由启动校验判定无效并中止(见第 5 步 PreviewSession::GetShellValid)
	{
		int iScreenW = GetSystemMetrics(SM_CXSCREEN);
		int iScreenH = GetSystemMetrics(SM_CYSCREEN);
		int iEditW = 0;
		int iEditH = 0;
		LPCSTR lpszSizeSrc = "屏占比兜底";
		{
			LONG wx = 0;
			LONG wy = 0;
			DWORD ww = 0;
			DWORD wh = 0;
			if (PreviewSession::GetShellWndRect(&wx, &wy, &ww, &wh) && ww >= 1 && wh >= 1)
			{
				iEditW = (int)ww;
				iEditH = (int)wh;
				lpszSizeSrc = "GenUI.json 外壳布局 autosize";
			}
			else
			{
				iEditW = (int)(iScreenW * 0.34f);
				iEditH = (int)(iScreenH * 0.94f);
				if (iEditW < 900) { iEditW = 900; }
				if (iEditH < 1200) { iEditH = 1200; }
			}
		}
		// 下限夹取: 描述值过小时按最小可用值
		if (iEditW < 200) { iEditW = 200; }
		if (iEditH < 200) { iEditH = 200; }
		int iEditX = iScreenW - iEditW - 24;
		if (iEditX < 0) { iEditX = 0; }		// 描述宽于屏宽时夹取到屏左缘(仍按"右上角留 24px 边距"摆放)
		int iEditY = (int)(iScreenH * 0.03f);
		printf("[MAIN] 编辑窗口尺寸: %dx%d (来源: %s)\n", iEditW, iEditH, lpszSizeSrc);
		specs[WND_EDIT].lpszClassName = L"GENUIEDITWND";
		specs[WND_EDIT].lpszTitle = L"GenUI - 布局编辑器";
		specs[WND_EDIT].x = iEditX;
		specs[WND_EDIT].y = iEditY;
		specs[WND_EDIT].dwWidth = (DWORD)iEditW;
		specs[WND_EDIT].dwHeight = (DWORD)iEditH;
		FLOAT fEditFont = PreviewSession::GetShellRef().GetFontSize<FLOAT>();
		if (fEditFont < 12.0f) { fEditFont = 18.0f; }
		specs[WND_EDIT].fFontSize = fEditFont;
		specs[WND_EDIT].dwRounding = PreviewSession::GetShellRef().GetSpace<DWORD>();
	}
	// 预览窗口: 尺寸/位置 = active 布局窗口几何(autosize 推导), POPUP 不可缩放
	{
		LONG wx = 0, wy = 0;
		DWORD ww = 0, wh = 0;
		Application::GetDocWndRect(Application::GetActiveIndex(), &wx, &wy, &ww, &wh);
		if (ww < 32) { ww = 32; }
		if (wh < 32) { wh = 32; }
		specs[WND_PREVIEW].lpszClassName = L"GENUIPREVWND";
		specs[WND_PREVIEW].lpszTitle = L"GenUI - 预览窗口";
		specs[WND_PREVIEW].x = (INT)wx;
		specs[WND_PREVIEW].y = (INT)wy;
		specs[WND_PREVIEW].dwWidth = ww;
		specs[WND_PREVIEW].dwHeight = wh;
		FLOAT fPrevFont = Application::GetActiveRef().GetFontSize<FLOAT>();
		if (fPrevFont < 12.0f) { fPrevFont = 18.0f; }
		specs[WND_PREVIEW].fFontSize = fPrevFont;
		specs[WND_PREVIEW].dwRounding = Application::GetActiveRef().GetSpace<DWORD>();
	}

	// 4. 建立窗口与 ImGui 面(每窗口 D3D11 / 上下文 / 字体 / 样式 / 圆角 / IME), 并注入渲染能力表
	EnsureProjCR();
	ImGuiHost::SetStandardWndCount(WND_COUNT);
	ImGuiHost::SetStyleFunc(ApplyProjStyle, NULL);
	ImGuiHost::SetSegmentFunc(OnSegmentBegin, NULL);
	ImGuiHost::SetDarkMode(AppEnv::IsSystemDarkMode());
	if (!ImGuiHost::CreateWnds(specs))
	{
		ERRTEXT = L"窗口创建失败!";
		ret = 1;
		goto MAIN_APP_RELEASE;
	}
	// 框架已收回窗口过程(内置退出处理); 本工程的两项业务消息改由子类化过程前置处理,
	// 其余消息原样转交框架过程(见 BizWndProc)
	InstallBizWndProc(ImGuiHost::GetWindow(WND_EDIT));
	InstallBizWndProc(ImGuiHost::GetWindow(WND_PREVIEW));
	HostContext::SetWindowHandle(ImGuiHost::GetWindow(WND_EDIT));
	HostContext::SetPreviewWindowHandle(ImGuiHost::GetWindow(WND_PREVIEW));
	// 外壳布局几何按编辑窗口实际客户区求解(WND 铺满客户区, 内部控件依客户区参考);
	// 实际客户区与建窗尺寸一致(WS_POPUP 无边框) ⇒ 此处为幂等校正
	{
		RECT rcClient{};
		if (GetClientRect(ImGuiHost::GetWindow(WND_EDIT), &rcClient))
		{
			PreviewSession::SetShellClientSize(rcClient.right - rcClient.left,
				rcClient.bottom - rcClient.top);
		}
		else
		{
			PreviewSession::SetShellClientSize(specs[WND_EDIT].dwWidth, specs[WND_EDIT].dwHeight);
		}
	}
	// 编辑窗口的位置记录: 外壳 WND 件的 position = 该窗口的桌面位置
	// 说明: 窗口几何由渲染出口每帧按 WND 件的 position/size 经 AcquireWnd 结算(绘制侧已不再由
	//       标题栏事件直接移动 HWND) ⇒ 建窗后必须把位置记录落成实际建窗位置, 否则首帧会被
	//       结算到布局声明坐标(0,0)。外壳快照是只读件、不参与预览基座的同步重装 ⇒ 该记录
	//       在整个会话内只由标题栏事件改写, 不会被几何解算覆盖。
	{
		CtrlInfo* pWnd = PreviewSession::GetShellLayout().GetLayoutBase(CTRL_TYPE_WND);
		if (pWnd != NULL)
		{
			pWnd->position.x = (FLOAT)specs[WND_EDIT].x;
			pWnd->position.y = (FLOAT)specs[WND_EDIT].y;
			pWnd->posDeclared = pWnd->position;
		}
	}
	if (!ImGuiHost::Acquire())
	{
		ERRTEXT = L"D3D 设备或 IMGUI 面初始化失败!";
		ret = 1;
		goto MAIN_CLN_IMGUI;
	}

	// 5. 无人值守参数 + 提示展示器 + 外壳快照校验
	{
		char szEnv[32]{};
		if (GetEnvironmentVariableA("GENUI_AUTOCLOSE_MS", szEnv, 32) > 0)
		{
			dwAutoCloseMs = (DWORD)atoi(szEnv);
		}
		// 注入同步提示展示器: 编辑器侧的弹窗统一入口据此把提示切到「提示布局」(同步等待)
		HostContext::SetHintSyncHandler(RunHintSync);
		// 编辑窗口内容 = 外壳布局(GenUIEditor 快照)的声明式渲染
		// 快照无效 ⇒ 打印诊断并中止启动(此处不设 ERRTEXT, 诊断走 stdout + 退出码)
		if (!PreviewSession::GetShellValid())
		{
			printf("[MAIN] 外壳布局快照无效: 文档缺少 name==GenUIEditor 的布局(或无结构件/解析失败)\n");
			printf("[MAIN] 编辑窗口无可渲染内容, 且已无内置回退(渲染回退已移除) ⇒ 启动中止\n");
			fflush(stdout);
			ret = 1;
			goto MAIN_CLN_IMGUI;
		}
		printf("[MAIN] 编辑窗口渲染: 外壳布局(GenUIEditor 快照)\n");
	}

	// 6. 主循环: 由 ImGuiHost 驱动(每帧消费消息后固定调用一次 FrameOnce)
	qwStartTick = GetTickCount64();
	ImGuiHost::Run(FrameOnce, NULL, &IsExit);

	// 清理流程
MAIN_CLN_IMGUI:
	ImGuiHost::Release();
	ImGuiHost::DestroyWnds();
MAIN_APP_RELEASE:
	ProjCR::Release(pProjCR);	// 投影配色: 先释放(轴位置表须在分配器解绑前归还小对象池)
	// 文档承载者随 Application::Release() 失效 ⇒ 先复位为 NULL(与 Init 内窗口句柄的复位同法;
	//   时序硬约束见上方 ③ 注入点说明 —— 顺序颠倒 ⇒ 编辑器侧留悬垂指针)
	GenUIEditor::SetDocument(NULL);
	EditorCommands::SetSessionHooks(NULL, NULL, NULL);	// 会话回调随承载者一并复位(须早于 Application::Release)
	DocumentIO::SetDocLayoutAdder(NULL);				// 命令面回调同点复位(须早于 Application::Release;
														//   承载者与 I/O 侧会话回调由 Application::Release 内复位)
	Application::Release();
MAIN_END:
	if (ERRTEXT)
	{
		MessageBox(NULL, ERRTEXT, L"GenUI 错误", MB_OK | MB_ICONERROR);
	}
	return ret;
}

// 函数: ProjSampleConvex
// 描述: 暗模式采样函数 —— 导数单调递增的单调递增函数(幂曲线: 前期缓、后期陡)
// 说明: 口径 = "采样函数最好是导数单调递增的单调递增函数" ⇒ 低层号(下嵌层与
//       窗口底)缓升 ⇒ 基调保持暗; 抬升层之间的差异逐层加大。幂次越大低层越暗。
// 参数:
//		IN	dwParam:	采样参数(已按方向反射)
//		IN	dwLayers:	层数
// 返回:
//		FLOAT: 该参数对应的轴位置(0 = 底色端; 1 = 光色端)
static FLOAT ProjSampleConvex(DWORD dwParam, DWORD dwLayers)
{
	constexpr FLOAT fExponent = 3.0f;	// 幂次(> 1 ⇒ 导数单调递增)
	CONST FLOAT fRatio = (FLOAT)(dwParam - 1) / (FLOAT)(dwLayers - 1);
	return powf(fRatio, fExponent);
}

// 函数: ProjSampleConcave
// 描述: 亮模式采样函数 —— 上者的镜像形态(导数单调递减的单调递增函数)
// 说明: 幂次独立整定(暗模式 2.0 / 亮模式 3.0): 两种模式各自的目标是"窗口底贴近本模式的
//       端点"(暗端 / 亮端), 不以"互为镜像"为约束 —— 亮模式下"层号大 = 更暗", 幂次越大 ⇒
//       低层号越贴近亮端。
// 参数:
//		IN	dwParam:	采样参数(已按方向反射)
//		IN	dwLayers:	层数
// 返回:
//		FLOAT: 该参数对应的轴位置(0 = 底色端; 1 = 光色端)
static FLOAT ProjSampleConcave(DWORD dwParam, DWORD dwLayers)
{
	constexpr FLOAT fExponent = 3.0f;	// 亮模式幂次(独立整定; 增大 ⇒ 亮模式整体更亮)
	CONST FLOAT fRatio = (FLOAT)(dwParam - 1) / (FLOAT)(dwLayers - 1);
	return 1.0f - powf(1.0f - fRatio, fExponent);
}

// 函数: EnsureProjCR
// 描述: 确保宿主投影配色已建立并跟随系统亮暗(反复调用幂等)
// 说明: 亮暗模式 = 采样方向(不对调底色与光色) —— 亮模式 PROJ_LIGHT_TO_DARK(层 1 取光端 ⇒
//       层号增大更暗, 与旧观感同向) / 暗模式 PROJ_DARK_TO_LIGHT(层 1 取暗端 ⇒ 层号增大更亮);
//       方向在"喂给采样函数之前"反射层号 ⇒ 采样函数本身的单调性不受方向影响
static VOID EnsureProjCR()
{
	// 诊断开关(仅复核用; 不改变系统设置): GENUI_PROJCR_LIGHT=1 ⇒ 强制按亮色模式取色,
	// 便于无人值守下对两种模式各自取证(未设该变量时行为不变)
	BOOL bForceLight = FALSE;
	{
		CHAR szForce[32]{};
		if (GetEnvironmentVariableA("GENUI_PROJCR_LIGHT", szForce, 32) > 0 && atoi(szForce) != 0)
		{
			bForceLight = TRUE;
		}
	}
	CONST ProjCR::SAMPLE_DIRECTION eDir =
		((bForceLight != FALSE) || !AppEnv::IsSystemDarkMode()) ? ProjCR::PROJ_LIGHT_TO_DARK : ProjCR::PROJ_DARK_TO_LIGHT;
	// 采样函数按方向选形态(口径 = "导数单调递增的单调递增函数"):
	//   暗模式(层号大 = 亮) ⇒ 凸函数(前期缓 / 后期陡) ⇒ 窗口底与低层保持暗;
	//   亮模式(层号大 = 暗) ⇒ 其镜像(凹函数) ⇒ 窗口底与低层保持亮。
	CONST ProjCR::SampleFunc pfnSample =
		(eDir == ProjCR::PROJ_DARK_TO_LIGHT) ? ProjSampleConvex : ProjSampleConcave;
	if (pProjCR == NULL)
	{
		// 构造失败(层数非法 / 采样函数校验失败 / 表分配失败) ⇒ 保持 NULL, 取色处回退无色
		ProjCR::Create(pProjCR, PROJ_COLOR_BASE, PROJ_COLOR_LIGHT, eDir, PROJ_LAYERS, pfnSample);
		return;
	}
	pProjCR->SetDirection(eDir);
	pProjCR->SetSampleFunc(pfnSample);
}

// 函数: ApplyProjStyle
// 描述: 用投影配色填充 ImGui 样式槽(17 项; 层号口径见本文件"投影配色口径"区)
// 说明: 只覆盖 ImGui 内建控件(滚动条 / 弹窗 / 表头一类)的取色; 项目自绘件一律走控件库的
//       ProjColor(层号, 槽位) ⇒ 两处层号口径必须一致(改层次只改两侧各自的常量区)
// 参数:
//		IN/OUT	style:	目标 ImGui 样式(按当前 context 取)
//		IN		pUser:	宿主私有参数(本宿主不用)
static VOID ApplyProjStyle(ImGuiStyle& style, VOID* pUser)
{
	(void)pUser;
	if (pProjCR == NULL)
	{
		return;
	}
	style.Colors[ImGuiCol_WindowBg] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_WINDOW));
	style.Colors[ImGuiCol_ChildBg] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_CHILD));
	style.Colors[ImGuiCol_FrameBg] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME));
	style.Colors[ImGuiCol_FrameBgHovered] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_HOVER));
	style.Colors[ImGuiCol_FrameBgActive] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_ACTIVE));
	style.Colors[ImGuiCol_Button] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME));
	style.Colors[ImGuiCol_ButtonHovered] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_HOVER));
	style.Colors[ImGuiCol_ButtonActive] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_ACTIVE));
	style.Colors[ImGuiCol_Border] = MAKE_CR_IMVEC4(pProjCR->SampleBorder(PROJ_L_CHILD, PROJ_L_FRAME));
	style.Colors[ImGuiCol_Text] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_WINDOW, ProjCR::PROJ_SLOT_CONTRAST));
	style.Colors[ImGuiCol_TextDisabled] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_DISABLED));
	style.Colors[ImGuiCol_SliderGrab] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME));
	style.Colors[ImGuiCol_SliderGrabActive] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_CONTRAST));
	style.Colors[ImGuiCol_CheckMark] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_CONTRAST));
	style.Colors[ImGuiCol_Header] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME));
	style.Colors[ImGuiCol_HeaderHovered] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_HOVER));
	style.Colors[ImGuiCol_HeaderActive] = MAKE_CR_IMVEC4(pProjCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_ACTIVE));
}

// 函数: RenderEditFrame
// 描述: 渲染编辑窗口的内容段(外壳布局 + 内嵌(flex)成员 + IME 口径)
// 说明: 帧的开合与呈现由 ImGuiHost 的段结算负责; 本函数只提交"本窗口要画什么"
static VOID RenderEditFrame()
{
	// 投影配色(宿主唯一持有; 编辑帧与预览帧共用同一投影 ⇒ 同屏层次口径一致)
	EnsureProjCR();
	// 渲染前联动(选中 / 缓冲)
	GenUIEditor::Update();
	// 编辑窗口内容 = 外壳布局的声明式渲染(快照有效性已在启动期校验)
	// 严格模态: 「提示布局」展示期间(bHintModalActive)
	// 编辑窗口传 bNoInputs = TRUE ⇒ 窗口级无输入; 提示布局是内嵌(flex)成员、走 RenderFlex ⇒ 不受影响
	// (仍可点击); 时机由本处以显式参数给出 —— 渲染入口不做全局状态查询。
	LayoutRender::RenderBase(PreviewSession::GetShellLayout(), PreviewSession::GetShellRef(), pProjCR, bHintModalActive);
	// 编辑窗口的内嵌(flex)窗口: 向应用查询"该基座当前活动的内嵌窗口集合",
	// 有则逐枚渲染(无独立 HWND; 同一 context 同一帧, 在基座之后渲染 ⇒ 位于基座上层)
	{
		DWORD dwFlex = 0;
		Layout** ppFlex = FlexHost::GetActiveFlexLayouts(PreviewSession::GetShellLayout(), &dwFlex);
		for (DWORD i = 0; i < dwFlex; i++)
		{
			LayoutRender::RenderFlex(*ppFlex[i], FlexHost::GetFlexRef(ppFlex[i]), pProjCR);
		}
	}
	// IME(编辑窗口输入): 本帧覆盖值由段结算应用(ImGuiHost 统一处理输入法开关)
	ImGuiHost::SetIMEDisabled(LayoutRender::IsIMEDisabled());
}

// 函数: OnSegmentBegin
// 描述: 段落开帧回调(预览窗口段: 在 ImGui::NewFrame 之前预留钩子)
// 参数:
//		IN	index:	即将开帧的标准窗口序号
//		IN	pUser:	宿主私有参数(本宿主不用)
static VOID OnSegmentBegin(DWORD index, VOID* pUser)
{
	(void)index;
	(void)pUser;
}

// 函数: RenderPreviewFrame
// 描述: 渲染预览窗口的内容段(active 布局 + 逐控件点击判定 + 视图叠加件)
static VOID RenderPreviewFrame()
{
	EnsureProjCR();
	Layout& active = Application::GetActiveLayout();
	if (!active.IsInvalid())
	{
		// 预览布局专属入口: 逐枚内容控件做点击判定(命中即派发给编辑器侧点击记录器) +
		// 编辑器选中态视图叠加件(链外绘制; 见 editor\SelectionOverlay)
		LayoutRender::RenderPreviewBase(active, Application::GetActiveRef(), pProjCR, GenUIEditor::OnPreviewClick,
			SelectionOverlay::OnBeforeCtrl);
	}
}

// 函数: FrameOnce
// 描述: 每帧外部处理流程(由 ImGuiHost 每帧调用一次): 数据层更新 + 按渲染清单顺序渲染两窗口
// 参数:
//		IN	pUser:	宿主私有参数(本宿主不用)
static VOID FrameOnce(VOID* pUser)
{
	(void)pUser;
	// 1. 无人值守: 到点即请求退出(退出消息交回主循环)
	if (dwAutoCloseMs > 0 && (GetTickCount64() - qwStartTick) >= dwAutoCloseMs)
	{
		PostQuitMessage(0);
		return;
	}
	// 2. 投影配色(宿主唯一持有; 亮暗模式 = 采样方向 ⇒ 每帧确保跟随系统亮暗)
	EnsureProjCR();

	// 3. 编辑窗口(渲染清单第 1 段)
	// 说明: 段与段之间不手动结算 —— AcquireWnd 取下一窗口时会先结算上一段(窗口切换的唯一口径);
	//       本帧最后一段由 ImGuiHost::Run 的帧末兜底结算
	ImGuiHost::ResetWndCursor();
	RenderEditFrame();

	// 4. 预览窗口(渲染清单第 2 段; 渲染 active 布局)
	RenderPreviewFrame();

	// 5. 帧间事件: 两窗口的渲染帧均已结束 ⇒ 执行本帧挂载的事件(时机分离, 见 FrameEvent.h)。
	// 处理函数在帧内只负责挂载, 真正的业务(重装布局/重建文档一类会改动控件对象与缓冲的操作)
	// 在此执行; 队列为空时本调用无任何影响。
	FrameEvent::Drain();
}

// ================================
// 区域: 同步提示展示器实现区(「提示布局」)
// 说明: 展示提示需要"渲染帧 + 消息泵"(宿主职责) ⇒ 本区实现宿主侧的同步展示器并经
//       HostContext::SetHintSyncHandler 注入; 编辑器侧的弹窗统一入口按注入结果分流(见 GenEditor.cpp)。
// ================================

// 函数: FindHintCtrl
// 描述: 在指定布局内按 label 找控件(未命中返回 NULL)
// 参数:
//		IN	pLayout:	目标布局(可为 NULL)
//		IN	label:		控件标签(精确匹配; NULL 直接未命中)
// 返回:
//		CtrlInfo*: 命中的控件; 未命中返回 NULL
static CtrlInfo* FindHintCtrl(Layout* pLayout, LPCSTR label)
{
	CtrlInfo* pRet = NULL;
	if (pLayout == NULL || label == NULL)
	{
		return pRet;
	}
	size_t num = pLayout->GetTotalNum();
	for (size_t i = 0; i < num && pRet == NULL; i++)
	{
		CtrlInfo* ctrl = pLayout->GetCtrlInfo(i);
		if (ctrl != NULL && ctrl->label != NULL && strcmp(ctrl->label, label) == 0)
		{
			pRet = ctrl;
		}
	}
	return pRet;
}

// 函数: SetHintCtrlText
// 描述: 把 UTF-8 文本写入指定控件的文本缓冲(超长按缓冲容量截断; 控件无缓冲则忽略)
// 参数:
//		IN	ctrl:	目标控件(可为 NULL)
//		IN	text:	新文本(UTF-8; NULL 按空串)
static VOID SetHintCtrlText(CtrlInfo* ctrl, LPCSTR text)
{
	if (ctrl == NULL || ctrl->bufferInfo.buffer == NULL || ctrl->bufferInfo.size == 0)
	{
		return;
	}
	strncpy_s((CHAR*)ctrl->bufferInfo.buffer, ctrl->bufferInfo.size, (text != NULL) ? text : "", _TRUNCATE);
}

// 函数: BackupHintCtrlText
// 描述: 把指定控件的当前文本备份到调用方缓冲(用于"按钮文本被覆盖后恢复"; 无缓冲则备份为空串)
// 参数:
//		IN	ctrl:		目标控件(可为 NULL)
//		OUT	szBackup:	备份缓冲(写出以 '\0' 结尾的文本)
//		IN	cbBackup:	备份缓冲字节数
static VOID BackupHintCtrlText(CtrlInfo* ctrl, LPSTR szBackup, size_t cbBackup)
{
	if (szBackup == NULL || cbBackup == 0)
	{
		return;
	}
	szBackup[0] = '\0';
	if (ctrl == NULL || ctrl->bufferInfo.buffer == NULL)
	{
		return;
	}
	strncpy_s(szBackup, cbBackup, (LPCSTR)ctrl->bufferInfo.buffer, _TRUNCATE);
}

// 函数: HintCtrlTextOf
// 描述: 取控件当前文本(无缓冲返回空串)
// 参数:
//		IN	ctrl:	目标控件(可为 NULL)
// 返回:
//		LPCSTR: 控件文本(永不为 NULL)
static LPCSTR HintCtrlTextOf(CtrlInfo* ctrl)
{
	if (ctrl == NULL || ctrl->bufferInfo.buffer == NULL)
	{
		return "";
	}
	return (LPCSTR)ctrl->bufferInfo.buffer;
}

// 函数: RunHintSync
// 描述: 同步展示「提示布局」并阻塞至用户选择(宿主注入给编辑器的同步提示展示器; 见 SessionContracts.h 的 pfnHintSync)
// 说明: 提示布局 = 编辑窗口内的内嵌(flex)成员(来源 = 外壳源文档 ⇒ 与当前编辑的文档无关);
//       同步等待 = 循环"泵消息 + 渲染编辑帧", 直到结论落定。
//       ①等待期间不排空帧间事件队列(避免重入编辑器动作);
//       ②文案全可按次定制: 标题 / 正文 / "确认"按钮文本 / "取消"按钮文本(见 HintSpec; 传 NULL 的项保留
//         布局默认文本); 被覆盖的按钮文本在退出前恢复原值(免污染下次展示);
//       ③按钮自适应: 按钮宽 = max(2 × 控件高, 文本宽 × 字号 + 2 × 间隔), 取消.x = 窗宽 − 间隔 − 取消宽,
//         确认.x = 窗宽 − 2 × 间隔 − 取消宽 − 确认宽(每次展示重算 ⇒ 无需恢复);
//       ④按钮可见性: bHasCancel = FALSE 时把"取消"按钮的控件类型临时置 CTRL_TYPE_INVALID
//         (渲染层对该类型只提交空白项 ⇒ 零像素、不可交互), 退出前恢复原类型;
//       ⑤每次展示都把提示窗口摆到宿主客户区正中(展示后仍可拖动标题栏改变它, 下次展示重新居中);
//       ⑥重入守卫: 已有提示在展示中 ⇒ 直接返回 FALSE(调用方据此回落/取消);
//       ⑦按需显示: 提示布局装配后默认非活动(平时不渲染) ⇒ 本函数展示前置为活动、用毕复位。
// 参数:
//		IN	spec:	展示参数(标题 / 正文 / 两个按钮文本 / 是否显示取消按钮; 见 SessionContracts.h 的 HintSpec)
// 返回:
//		BOOL: TRUE = 用户确认; FALSE = 用户取消(或提示布局不可用 / 重入 / 等待期间收到退出)
static BOOL RunHintSync(CONST HintSpec& spec)
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	DWORD dwFlex = 0;
	Layout** ppFlex = NULL;
	Layout* pHint = NULL;
	CtrlInfo* pOk = NULL;
	CtrlInfo* pCancel = NULL;
	CtrlType typeCancel = CTRL_TYPE_BUTTON;
	BOOL bOkOverridden = FALSE;					// "确认"按钮文本是否被本次展示覆盖
	BOOL bCancelOverridden = FALSE;				// "取消"按钮文本是否被本次展示覆盖
	CHAR szOkBack[CTRL_TEXT_BUF] = "";
	CHAR szCancelBack[CTRL_TEXT_BUF] = "";
	if (bHintModalActive)
	{
		return bRet;		// 重入: 已有提示在展示
	}
	// 按需显示: 提示布局装配后默认非活动(平时不渲染) ⇒ 展示前置为活动, 用毕复位(见函数末)
	FlexHost::SetFlexActive(GENUI_HINT_LAYOUT_NAME, TRUE);
	ppFlex = FlexHost::GetActiveFlexLayouts(PreviewSession::GetShellLayout(), &dwFlex);
	if (ppFlex == NULL || dwFlex == 0)
	{
		FlexHost::SetFlexActive(GENUI_HINT_LAYOUT_NAME, FALSE);	// 未装配 ⇒ 复位并交调用方回落
		return bRet;
	}
	pHint = ppFlex[0];
	// 1. 文案(标题 / 正文 / 两个按钮; 传 NULL 的项保留布局默认文本)与按钮可见性
	SetHintCtrlText(FindHintCtrl(pHint, "##TITLEBAR"), spec.szTitle);
	SetHintCtrlText(FindHintCtrl(pHint, "##TEXT_HINT"), spec.szText);
	pOk = FindHintCtrl(pHint, "##BTN_OK");
	if (pOk != NULL && spec.szOkText != NULL)
	{
		BackupHintCtrlText(pOk, szOkBack, sizeof(szOkBack));
		SetHintCtrlText(pOk, spec.szOkText);
		bOkOverridden = TRUE;
	}
	pCancel = FindHintCtrl(pHint, "##BTN_CANCEL");
	if (pCancel != NULL)
	{
		if (spec.szCancelText != NULL)
		{
			BackupHintCtrlText(pCancel, szCancelBack, sizeof(szCancelBack));
			SetHintCtrlText(pCancel, spec.szCancelText);
			bCancelOverridden = TRUE;
		}
		typeCancel = pCancel->type;
		pCancel->type = spec.bHasCancel ? CTRL_TYPE_BUTTON : CTRL_TYPE_INVALID;
	}
	// 2. 按钮自适应: 按钮宽 = max(2 × 控件高, 文本宽 × 字号 + 2 × 间隔)
	//    取消.x = 1 × 窗宽 − 1 × 间隔 − 取消宽; 确认.x = 1 × 窗宽 − 2 × 间隔 − 取消宽 − 确认宽
	//    (两按钮文字不同 ⇒ 宽度可不同; 确认的让位 = 间隔 + 两按钮宽度之和, 与"2 × 按钮宽"在等宽时一致)
	//    注: 只改宽度与 x 位置(y 与高保持布局声明); 每次展示都重算 ⇒ 无需恢复
	{
		CONST AutoSize& ref = FlexHost::GetFlexRef(pHint);
		FLOAT cs = ref.GetControlHeight<FLOAT>();
		FLOAT fs = ref.GetFontSize<FLOAT>();
		FLOAT sp = ref.GetSpace<FLOAT>();
		CtrlInfo* pWnd = pHint->GetLayoutBase(CTRL_TYPE_WND);
		FLOAT ww = (pWnd != NULL) ? pWnd->size.x : 0.0f;
		FLOAT wOk = 2.0f * cs;
		FLOAT wCancel = 2.0f * cs;
		if (pOk != NULL)
		{
			FLOAT need = (FLOAT)CalcTextWidth(HintCtrlTextOf(pOk)) * fs + 2.0f * sp;
			if (need > wOk) { wOk = need; }
		}
		if (pCancel != NULL)
		{
			FLOAT need = (FLOAT)CalcTextWidth(HintCtrlTextOf(pCancel)) * fs + 2.0f * sp;
			if (need > wCancel) { wCancel = need; }
		}
		if (pCancel != NULL)
		{
			pCancel->size.x = wCancel;
			pCancel->position.x = ww - sp - wCancel;
			pCancel->posDeclared.x = pCancel->position.x;	// 声明坐标同值(有效坐标按父链重推)
		}
		if (pOk != NULL)
		{
			pOk->size.x = wOk;
			pOk->position.x = ww - 2.0f * sp - wCancel - wOk;
			pOk->posDeclared.x = pOk->position.x;		// 同上
		}
	}
	// 3. 位置: 摆到宿主客户区正中(位置基准 = 宿主客户区)
	{
		RECT rc{};
		CtrlInfo* pWnd = pHint->GetLayoutBase(CTRL_TYPE_WND);
		if (pWnd != NULL && GetClientRect(ImGuiHost::GetWindow(WND_EDIT), &rc))
		{
			pWnd->position.x = (FLOAT)(((rc.right - rc.left) - (LONG)pWnd->size.x) / 2);
			pWnd->position.y = (FLOAT)(((rc.bottom - rc.top) - (LONG)pWnd->size.y) / 2);
		}
	}
	// 4. 同步等待: 泵消息 + 渲染编辑帧, 直到结论落定(确认 / 取消 / 退出)
	GenUIEditor::ResetHintResult();
	bHintModalActive = TRUE;
	while (GenUIEditor::GetHintResult() == GenUIEditor::HINT_RESULT_NONE)
	{
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				// 等待期间收到退出(Alt+F4 / ESC): 视为取消, 并把退出消息交还主循环
				::PostQuitMessage(0);
				goto HINT_END;
			}
		}
		ImGuiHost::ResetWndCursor();
		RenderEditFrame();
		ImGuiHost::FinalizeFrame();
	}
	bRet = (GenUIEditor::GetHintResult() == GenUIEditor::HINT_RESULT_OK) ? TRUE : FALSE;
HINT_END:
	bHintModalActive = FALSE;
	FlexHost::SetFlexActive(GENUI_HINT_LAYOUT_NAME, FALSE);	// 按需显示: 用毕复位(不再渲染提示)
	if (pOk != NULL && bOkOverridden)
	{
		SetHintCtrlText(pOk, szOkBack);			// 恢复"确认"按钮原文本(免污染下次展示)
	}
	if (pCancel != NULL)
	{
		pCancel->type = typeCancel;				// 恢复"取消"按钮可见性
		if (bCancelOverridden)
		{
			SetHintCtrlText(pCancel, szCancelBack);	// 恢复"取消"按钮原文本
		}
	}
	GenUIEditor::ResetHintResult();
	return bRet;
}

// ================================
// 区域: 窗口过程实现区
// ================================

// 函数: BizWndProc
// 描述: 两个窗口共用的业务窗口过程(子类化挂在框架窗口过程之外): 只处理关闭前的未保存提醒与
//   提示模态期间的回车/ESC, 其余消息一律经 CallWindowProc 转交框架的原窗口过程
// 说明: 框架件已收回窗口过程(内置 WM_DESTROY 与 ESC 退出), 本过程不重复实现退出判据:
//   WM_CLOSE 未拦截时交框架原过程 ⇒ DefWindowProc ⇒ 销毁 ⇒ 框架过程请求退出;
//   WM_KEYDOWN 非提示期一律透传(ESC 退出由框架原过程处理)。
LRESULT WINAPI BizWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CONST HWND hWndEdit = ImGuiHost::GetWindow(WND_EDIT);
	switch (msg)
	{
	case WM_CLOSE:
	{
		// 关闭窗口(Alt+F4): 有未保存改动 ⇒ 同步「提示布局」提醒; 用户取消 ⇒ 不销毁窗口
		// (统一直走编辑器的提醒入口; 无改动时零提示、零日志)
		// 注: 提示展示期间(bHintModalActive)不再受理关闭请求(重入守卫)
		if (hWnd == hWndEdit && !bHintModalActive)
		{
			if (!GenUIEditor::ConfirmDiscardDirtyChanges("关闭窗口"))
			{
				return 0;
			}
		}
		break;
	}
	case WM_KEYDOWN:
	{
		// 严格模态: 提示展示期间
		// Enter = 确认 / ESC = 取消当前提示 —— 不再退出程序
		// (提示期间编辑窗口不接收 ImGui 层输入; 键盘结论一律在 Win32 消息层给出 ⇒ 走编辑器 OnHintKey)
		if (hWnd == hWndEdit && bHintModalActive)
		{
			if (wParam == VK_RETURN || wParam == VK_ESCAPE)
			{
				GenUIEditor::OnHintKey((wParam == VK_RETURN) ? TRUE : FALSE);
				return 0;
			}
		}
		break;
	}
	default:
		break;
	}
	return (pfnPrevWndProc != NULL) ?
		::CallWindowProcW(pfnPrevWndProc, hWnd, msg, wParam, lParam) :
		::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// 函数: InstallBizWndProc
// 描述: 把业务窗口过程挂到指定窗口上(保存框架原过程; 重复调用幂等)
// 参数:
//		IN	hWnd:	目标窗口句柄(NULL 直接返回)
static VOID InstallBizWndProc(HWND hWnd)
{
	if (hWnd == NULL)
	{
		return;
	}
	if (pfnPrevWndProc == NULL)
	{
		pfnPrevWndProc = (WNDPROC)::GetWindowLongPtrW(hWnd, GWLP_WNDPROC);
	}
	::SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)BizWndProc);
}
