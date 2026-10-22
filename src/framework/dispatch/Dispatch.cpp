// ================================
// 文件: Dispatch.cpp
// 描述: 调度层实现 —— 事件绑定表注入 / 场景载入 / 投影配色与宿主样式登记 / 每帧处理流程 /
//   渲染自证 / 无人值守参数 / 退出判据 / 清理链。
// 说明: 场景装配的分配与释放见同模块实现切片 DispatchScene.cpp; 建窗与渲染设施归具体宿主实现
//   (见 framework\render 下的宿主件), 本层只经 Render.h 的 HostApi 访问宿主, 并向宿主交出
//   pfnFrame 与退出判据地址。日志一律经本文件的 Log 包装(前缀取 DispatchConfig.szLogPrefix)。
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>
#include <math.h>
#include <stdarg.h>			// va_list(Log 包装)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framework/dispatch/Dispatch.h"
#include "common/AppEnv.h"		// 控制台 UTF-8 / 系统亮暗 / 窗口圆角
#include "framework/base/ControlSpec.h"	// 投影层号常量(控件层次 → 层号)
#include "framework/layout/LayoutRender.h"	// 布局渲染出口

// ================================
// 区域: 文件内常量区
// ================================
constexpr DWORD PROJ_LAYERS = 16;				// 层数(采样数量)
constexpr DWORD PROJ_COLOR_BASE = 0xFF141414u;	// 底色端(轴的低/暗端; 与亮暗模式无关)
constexpr DWORD PROJ_COLOR_LIGHT = 0xFFEFEFEFu;	// 光色端(轴的高/亮端; 与亮暗模式无关)
constexpr DWORD PROJ_L_WINDOW = PROJ_LAYER_BACKGROUND;		// 窗口底 / 背景层
constexpr DWORD PROJ_L_CHILD = PROJ_LAYER_BACKGROUND + 1;	// 内容子窗 / 容器
constexpr DWORD PROJ_L_FRAME = PROJ_LAYER_BACKGROUND + 1;	// 帧类件底(输入框 / 按钮 / 表头)
constexpr DWORD PROJ_L_DISABLED = 10;			// 禁用文本 / 次级(中灰)
constexpr DWORD PROOF_EVERY = 60;				// 渲染自证打印周期(帧; 打印节流)
constexpr size_t LOG_PREFIX_CAPACITY = 32;		// 日志前缀缓冲容量(含结尾 NUL)
constexpr FLOAT FONT_SIZE_MIN = 12.0f;			// 建面字号有效下限(低于此值按缺省字号)
constexpr FLOAT FONT_SIZE_FALLBACK = 18.0f;		// 建面字号缺省值(无有效布局参考时)

// ================================
// 区域: 调度状态定义区
// ================================
Dispatch::DispatchData* Dispatch::data = NULL;		// 调度成员块(会话分配器 arena 内)
static CHAR szLogPrefix[LOG_PREFIX_CAPACITY] = "";	// 日志前缀(Init 期从配置复制; 须早于任何打印可用)

// ================================
// 区域: 文件内函数声明区
// ================================
// 函数: ProjSampleConvex
// 描述: 暗模式采样函数 —— 导数单调递增的单调递增函数(幂曲线: 前期缓、后期陡)
static FLOAT ProjSampleConvex(DWORD dwParam, DWORD dwLayers);

// 函数: ProjSampleConcave
// 描述: 亮模式采样函数 —— 上者的镜像形态(导数单调递减的单调递增函数)
static FLOAT ProjSampleConcave(DWORD dwParam, DWORD dwLayers);

// 函数: ShowStateName
// 描述: 取显示状态的中文短名(渲染自证用)
static LPCSTR ShowStateName(WndShowState state);

// ================================
// 区域: 生命周期实现区
// ================================

// 函数: Init
// 描述: 初始化调度层(控制台 UTF-8 → DPI 感知 → 事件绑定表注入 → 场景载入 → 宿主注入与样式登记 → 无人值守参数)
BOOL Dispatch::Init(CONST HostApi* pHost, CONST DispatchConfig& cfg, BindingsFunc pfnBindings)
{
	// 变量声明区(避免 goto 跳过变量声明)
	BOOL bRet = FALSE;
	size_t nBindings = 0;
	if (pHost == NULL || pfnBindings == NULL || cfg.szDocName == NULL || cfg.dwWndCount == 0)
	{
		return bRet;
	}
	// 1. 控制台代码页切 UTF-8(日志全中文输出, 须先于任何打印)
	AppEnv::InitConsoleUtf8();
	// 2. 日志前缀(须先于任何打印; 缺省前缀按空串处理)
	lstrcpynA(szLogPrefix, (cfg.szLogPrefix != NULL) ? cfg.szLogPrefix : "", (INT)LOG_PREFIX_CAPACITY);
	// 3. DPI 感知须早于场景载入: 场景几何按屏幕像素推导, 未感知时系统会按缩放虚拟化屏幕尺寸
	//    注: 宿主 Start 首步仍会调用一次(幂等)
	pHost->EnableDpiAwareness();
	// 4. 注入事件绑定表(必须早于场景载入: 展开布局时即按 event 名解析处理器)
	//    表内容属应用侧, 框架层 UIEventHandler 不得反向依赖应用侧 ⇒ 由本层经函数指针取表再注入。
	{
		CONST UIEventHandler::EventBinding* pBindings = pfnBindings(&nBindings);
		UIEventHandler::SetBindings(pBindings, nBindings);
		Log("事件绑定表: %zu 条(拖拽窗口 · 事件委托演示)\n", nBindings);
	}
	// 5. 场景载入: 分配器 → 成员块(含配置复制) → 文档解析 → 展开 → 几何解算 → 位置定稿 → 自证
	if (!LoadScene(cfg))
	{
		goto INIT_END;
	}
	// 6. 宿主注入与样式登记(必须早于宿主建立 UI 面: 初始化期即应用一次宿主样式)
	//    投影配色(一条投影 = 一套配色)由本层持有; 亮暗口径与样式描述经 HostApi 登记给宿主。
	data->pHost = pHost;
	EnsureProjCR();
	data->bDarkMode = (data->bDarkFollowSystem != FALSE)
		? (AppEnv::IsSystemDarkMode() ? TRUE : FALSE) : data->bDarkMode;
	data->pHost->SetDarkMode(data->bDarkMode);
	{
		StyleDesc desc{};
		if (ApplyHostStyle(desc))
		{
			data->pHost->ApplyStyle(desc);
		}
	}
	// 6-bis. 建面字号自证: 字号由布局参考量决定(进程入口取本值交具体宿主登记, 宿主建窗时消费)
	Log("字号: %.1f (来源: 布局参考量)\n", GetFontSize());
	// 7. 无人值守参数: 自动退出毫秒数(无人值守启动的判据 = 退出码 0 + 日志行数)
	{
		CHAR szEnv[32]{};
		if (data->szAutoCloseEnv[0] != '\0' && GetEnvironmentVariableA(data->szAutoCloseEnv, szEnv, 32) > 0)
		{
			data->dwAutoCloseMs = (DWORD)atoi(szEnv);
		}
		Log("单窗口就绪: 渲染 = 声明式布局路径(LayoutRender::RenderBase; 目标窗口由渲染器决定)\n");
		Log("交互: 按住标题栏拖动窗口; ESC 或 Alt+F4 退出\n");
	}
	data->qwStartTick = GetTickCount64();
	data->bExit = FALSE;
	bRet = TRUE;

INIT_END:
	// 失败按阶段回退: Release 内按"投影配色 → 场景与成员块 → 会话分配器"逆序清理(部分状态亦安全)
	if (!bRet)
	{
		Release();
	}
	return bRet;
}

// 函数: Release
// 描述: 释放调度资源(投影配色 → 场景与成员块 → 会话分配器; 幂等)
VOID Dispatch::Release()
{
	// 收尾自证: 主循环退出后、场景释放前打印一次(显示状态取最近一帧缓存 ⇒ 不受窗口已销毁影响)
	if (data != NULL && data->bReady != FALSE && data->dwFrameCount > 0)
	{
		PrintRenderProof("收尾");
	}
	// 投影配色先释放: 其轴位置表须在会话分配器解绑之前归还小对象池
	if (data != NULL && data->pProjCR != NULL)
	{
		ProjCR::Release(data->pProjCR);
		data->pProjCR = NULL;
	}
	UnloadScene();
}

// 函数: SetDarkMode
// 描述: 覆盖"跟随系统"的亮暗口径(调用后本进程不再跟随系统; 宿主已就绪窗口立即刷新)
VOID Dispatch::SetDarkMode(BOOL bDark)
{
	if (data == NULL)
	{
		return;
	}
	data->bDarkFollowSystem = FALSE;
	data->bDarkMode = (bDark != FALSE) ? TRUE : FALSE;
	// 投影配色立即按新口径重算方向, 再把亮暗基础样式与配色槽一起刷新(两者须同向, 否则底色与取色不一致)
	EnsureProjCR();
	if (data->pHost != NULL)
	{
		data->pHost->SetDarkMode(data->bDarkMode);
		StyleDesc desc{};
		if (ApplyHostStyle(desc))
		{
			data->pHost->ApplyStyle(desc);
		}
	}
}

// ================================
// 区域: 帧流程实现区
// ================================

// 函数: Frame
// 描述: 每帧处理流程(建窗几何自证 → 数据层更新 → 布局渲染出口 → 可见性缓存 → IME 覆盖 → 渲染自证 → 退出判据)
VOID Dispatch::Frame(VOID* pUser)
{
	UNREFERENCED_PARAMETER(pUser);
	if (data == NULL || data->pHost == NULL)
	{
		return;
	}
	// 1. 建窗几何自证: 首帧渲染之前窗口仍是宿主 Start 的默认几何(客户区 800x600, 屏幕工作区居中)
	if (data->dwFrameCount == 0)
	{
		PrintWndGeometry();
	}
	// 2. 数据层更新: 亮暗模式跟随系统(亮暗 = 采样方向 ⇒ 每帧确保)
	EnsureProjCR();
	// 3. 渲染清单(单窗口单布局): 布局对象 → 渲染出口; 目标窗口由渲染器按顺序取得
	//    说明: 出口返回 FALSE(未注入能力 / 布局无效 / 取不到目标窗口)时静默跳过本帧, 不中断主循环
	LayoutRender::RenderBase(GetLayout(), GetRef(), data->pProjCR);
	// 4. 窗口可见性缓存: 收尾自证在窗口销毁之后打印, 故先缓存本帧的显示状态与可见数
	RefreshWndShowCache();
	// 5. IME 覆盖: 控件库报告"正在编辑输入控件" ⇒ 本段关闭输入法(段结算后自动失效)
	data->pHost->SetIMEDisabled(LayoutRender::IsIMEDisabled());
	// 6. 渲染自证: 首帧与每 PROOF_EVERY 帧各打印一行(段数与绘制量证明本帧确实提交过绘制)
	data->dwFrameCount++;
	if (data->dwFrameCount == 1 || (data->dwFrameCount % PROOF_EVERY) == 0)
	{
		PrintRenderProof("周期");
	}
	// 7. 无人值守: 到点即置退出判据(宿主主循环每帧读其地址)
	if (data->dwAutoCloseMs > 0 && (GetTickCount64() - data->qwStartTick) >= data->dwAutoCloseMs)
	{
		data->bExit = TRUE;
	}
}

// 函数: OnSegment
// 描述: 段前回调范例挂载点(后端开帧之后、UI 库帧开始之前; 当前为空实现)
VOID Dispatch::OnSegment(DWORD index, VOID* pUser)
{
	// 本形态无合成输入注入需求 ⇒ 空实现; 需要时在此按窗口序号写入输入事件
	UNREFERENCED_PARAMETER(index);
	UNREFERENCED_PARAMETER(pUser);
}

// ================================
// 区域: 访问实现区
// ================================

// 函数: GetLayout
// 描述: 取布局对象(渲染源; 未就绪时返回空对象)
Layout& Dispatch::GetLayout()
{
	if (data != NULL && data->bReady != FALSE && data->pLayout != NULL)
	{
		return *data->pLayout;
	}
	return Layout::GetNullObject();
}

// 函数: GetRef
// 描述: 取内部参考尺寸(控件几何解算与绘制取值基准; 未就绪时返回缺省参考量)
CONST AutoSize& Dispatch::GetRef()
{
	static AutoSize sNull = AutoSize::CreateByRatio(
		570, 430, 25, 13, 10, 4, AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT);
	if (data != NULL && data->bReady != FALSE)
	{
		return data->ref;
	}
	return sNull;
}

// 函数: GetFontSize
// 描述: 取建面字号(文档 autosize 的参考字号; 无效时按缺省字号兜底)
FLOAT Dispatch::GetFontSize()
{
	CONST FLOAT fFontSize = GetRef().GetFontSize<FLOAT>();
	if (fFontSize < FONT_SIZE_MIN)
	{
		return FONT_SIZE_FALLBACK;
	}
	return fFontSize;
}

// 函数: GetWndCount
// 描述: 取本调度需要创建的标准窗口数量(未就绪时按单窗口兜底)
DWORD Dispatch::GetWndCount()
{
	if (data != NULL && data->dwWndCount > 0)
	{
		return data->dwWndCount;
	}
	return 1;
}

// 函数: GetWndRect
// 描述: 取布局窗口矩形(桌面位置取自窗口位置记录; 客户区尺寸取文档 autosize 推导值)
BOOL Dispatch::GetWndRect(LONG* x, LONG* y, DWORD* w, DWORD* h)
{
	if (data == NULL || data->bReady == FALSE)
	{
		return FALSE;
	}
	// 位置随窗口件同步(窗口件缺失时保持上次值): 建窗几何与渲染目标结算共用同一真源
	if (data->pLayout != NULL)
	{
		CtrlInfo* pWnd = data->pLayout->GetLayoutBase(CTRL_TYPE_WND);
		if (pWnd != NULL)
		{
			data->nWndX = (LONG)pWnd->position.x;
			data->nWndY = (LONG)pWnd->position.y;
		}
	}
	if (x != NULL) { *x = data->nWndX; }
	if (y != NULL) { *y = data->nWndY; }
	if (w != NULL) { *w = data->dwWndW; }
	if (h != NULL) { *h = data->dwWndH; }
	return TRUE;
}

// 函数: GetExitAddress
// 描述: 取退出判据地址(宿主主循环每帧只读; 未就绪时返回 NULL)
CONST BOOL* Dispatch::GetExitAddress()
{
	if (data == NULL)
	{
		return NULL;
	}
	return &data->bExit;
}

// 函数: IsReady
// 描述: 调度层是否就绪(成员块已分配且场景已载入)
BOOL Dispatch::IsReady()
{
	if (data != NULL && data->bReady != FALSE)
	{
		return TRUE;
	}
	return FALSE;
}

// ================================
// 区域: 投影配色实现区
// ================================

// 函数: ProjSampleConvex
// 描述: 暗模式采样函数 —— 导数单调递增的单调递增函数(幂曲线)
static FLOAT ProjSampleConvex(DWORD dwParam, DWORD dwLayers)
{
	constexpr FLOAT fExponent = 3.0f;	// 幂次(> 1 ⇒ 导数单调递增)
	CONST FLOAT fRatio = (FLOAT)(dwParam - 1) / (FLOAT)(dwLayers - 1);
	return powf(fRatio, fExponent);
}

// 函数: ProjSampleConcave
// 描述: 亮模式采样函数 —— 上者的镜像形态(导数单调递减的单调递增函数)
static FLOAT ProjSampleConcave(DWORD dwParam, DWORD dwLayers)
{
	constexpr FLOAT fExponent = 3.0f;	// 亮模式幂次(独立整定; 增大 ⇒ 亮模式整体更亮)
	CONST FLOAT fRatio = (FLOAT)(dwParam - 1) / (FLOAT)(dwLayers - 1);
	return 1.0f - powf(1.0f - fRatio, fExponent);
}

// 函数: EnsureProjCR
// 描述: 确保投影配色已建立并跟随当前亮暗口径(跟随系统或 SetDarkMode 登记值; 反复调用幂等)
VOID Dispatch::EnsureProjCR()
{
	if (data == NULL)
	{
		return;
	}
	CONST BOOL bDark = (data->bDarkFollowSystem != FALSE)
		? (AppEnv::IsSystemDarkMode() ? TRUE : FALSE) : data->bDarkMode;
	CONST ProjCR::SAMPLE_DIRECTION eDir =
		(bDark != FALSE) ? ProjCR::PROJ_DARK_TO_LIGHT : ProjCR::PROJ_LIGHT_TO_DARK;
	// 采样函数按方向选形态: 暗模式(层号大 = 亮) ⇒ 凸函数; 亮模式(层号大 = 暗) ⇒ 其镜像(凹函数)
	CONST ProjCR::SampleFunc pfnSample =
		(eDir == ProjCR::PROJ_DARK_TO_LIGHT) ? ProjSampleConvex : ProjSampleConcave;
	if (data->pProjCR == NULL)
	{
		// 构造失败(层数非法 / 采样函数校验失败 / 表分配失败) ⇒ 保持 NULL, 取色处回退无色
		ProjCR::Create(data->pProjCR, PROJ_COLOR_BASE, PROJ_COLOR_LIGHT, eDir, PROJ_LAYERS, pfnSample);
		return;
	}
	data->pProjCR->SetDirection(eDir);
	data->pProjCR->SetSampleFunc(pfnSample);
}

// 函数: ApplyHostStyle
// 描述: 按参考尺寸与投影配色填宿主样式描述(圆角与 17 个配色槽); 配色未就绪时返回 FALSE
BOOL Dispatch::ApplyHostStyle(StyleDesc& desc)
{
	// 1. 圆角: 根窗口(窗口圆角)取设计间隔; 帧 / 子窗 / 弹出层等非根窗口级圆角取设计内边距
	//    注: 非根窗口级与控件层同口径(Controls::UpdateContext 的 fRounding 亦取内边距), 两处须同步
	CONST FLOAT fSpace = GetRef().GetSpace<FLOAT>();
	CONST FLOAT fPadding = GetRef().GetPadding<FLOAT>();
	desc.fWindowRounding = fSpace;
	desc.fFrameRounding = fPadding;
	desc.fChildRounding = fPadding;
	desc.fPopupRounding = fPadding;
	// 2. 投影配色(一条投影 = 一套配色): 逐槽取 GDI 色值, 由宿主映射到具体 UI 库的样式槽
	//    注: 取色路径与改前同源(ProjCR::Sample 的 DWORD 返回) ⇒ 颜色逐值不变
	ProjCR* pCR = data->pProjCR;
	if (pCR == NULL)
	{
		return FALSE;
	}
	desc.crWindowBg = pCR->Sample(PROJ_L_WINDOW);
	desc.crChildBg = pCR->Sample(PROJ_L_CHILD);
	desc.crFrameBg = pCR->Sample(PROJ_L_FRAME);
	desc.crFrameBgHovered = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_HOVER);
	desc.crFrameBgActive = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_ACTIVE);
	desc.crButton = pCR->Sample(PROJ_L_FRAME);
	desc.crButtonHovered = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_HOVER);
	desc.crButtonActive = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_ACTIVE);
	desc.crBorder = pCR->SampleBorder(PROJ_L_CHILD, PROJ_L_FRAME);
	desc.crText = pCR->Sample(PROJ_L_WINDOW, ProjCR::PROJ_SLOT_CONTRAST);
	desc.crTextDisabled = pCR->Sample(PROJ_L_DISABLED);
	desc.crSliderGrab = pCR->Sample(PROJ_L_FRAME);
	desc.crSliderGrabActive = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_CONTRAST);
	desc.crCheckMark = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_CONTRAST);
	desc.crHeader = pCR->Sample(PROJ_L_FRAME);
	desc.crHeaderHovered = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_HOVER);
	desc.crHeaderActive = pCR->Sample(PROJ_L_FRAME, ProjCR::PROJ_SLOT_ACTIVE);
	return TRUE;
}

// ================================
// 区域: 渲染自证实现区
// ================================

// 函数: ShowStateName
// 描述: 取显示状态的中文短名(渲染自证用)
static LPCSTR ShowStateName(WndShowState state)
{
	switch (state)
	{
	case WND_SHOW_VISIBLE:			return "可见";
	case WND_SHOW_HIDDEN_BY_ACTIVE:	return "被激活策略隐藏";
	case WND_SHOW_HIDDEN_BY_HOST:	return "被宿主隐藏";
	default:						return "从未显示";
	}
}

// 函数: PrintWndGeometry
// 描述: 打印首帧前的建窗几何(宿主 Start 默认几何的实测值; 首帧渲染前窗口尚未按文档几何结算)
VOID Dispatch::PrintWndGeometry()
{
	RECT rcWnd{};
	RECT rcClient{};
	if (data == NULL || data->pHost == NULL || data->pHost->GetWndRect(0, &rcWnd, &rcClient) == FALSE)
	{
		return;
	}
	Log("建窗几何: %ld,%ld %ldx%ld (来源: Start 默认几何)\n",
		(LONG)rcWnd.left, (LONG)rcWnd.top,
		(LONG)(rcClient.right - rcClient.left), (LONG)(rcClient.bottom - rcClient.top));
}

// 函数: RefreshWndShowCache
// 描述: 刷新最近一帧的窗口显示状态缓存(收尾自证在窗口销毁后仍可打印当时的可见性)
VOID Dispatch::RefreshWndShowCache()
{
	if (data == NULL || data->pHost == NULL)
	{
		return;
	}
	DWORD dwStandard = data->pHost->GetStandardWndCount();
	DWORD dwVisible = 0;
	for (DWORD i = 0; i < dwStandard; i++)
	{
		if (data->pHost->GetWndShowState(i) == WND_SHOW_VISIBLE)
		{
			dwVisible++;
		}
	}
	data->dwLastShowState = (DWORD)data->pHost->GetWndShowState(0);
	data->dwLastVisible = dwVisible;
}

// 函数: PrintRenderProof
// 描述: 渲染自证 —— 打印绘制量(渲染段数 / 绘制命令数 / 顶点数 / 索引数)、窗口尺寸调整次数、控件数
//   与窗口显示状态(显示状态取最近一帧缓存, 见 RefreshWndShowCache)
VOID Dispatch::PrintRenderProof(LPCSTR lpszTag)
{
	DWORD dwSegments = 0;
	DWORD dwCmdLists = 0;
	INT64 llVtx = 0;
	INT64 llIdx = 0;
	DWORD dwResizes = 0;
	DWORD dwLastW = 0;
	DWORD dwLastH = 0;
	DWORD dwStandard = 0;
	if (data != NULL && data->pHost != NULL)
	{
		dwStandard = data->pHost->GetStandardWndCount();
		data->pHost->GetRenderStats(&dwSegments, &dwCmdLists, &llVtx, &llIdx);
		data->pHost->GetResizeStats(&dwResizes, &dwLastW, &dwLastH);
	}
	Log("渲染自证(%s): 帧=%u 段=%u 绘制命令=%u 顶点=%lld 索引=%lld 窗口调整=%u(最近 %ux%u) 控件=%zu 窗口显示=%s 可见=%u/%u\n",
		(lpszTag != NULL) ? lpszTag : "-",
		(unsigned)((data != NULL) ? data->dwFrameCount : 0), (unsigned)dwSegments,
		(unsigned)dwCmdLists, (long long)llVtx, (long long)llIdx,
		(unsigned)dwResizes, (unsigned)dwLastW, (unsigned)dwLastH, GetLayout().GetTotalNum(),
		ShowStateName((WndShowState)((data != NULL) ? data->dwLastShowState : 0)),
		(unsigned)((data != NULL) ? data->dwLastVisible : 0), (unsigned)dwStandard);
}

// ================================
// 区域: 日志实现区
// ================================

// 函数: Log
// 描述: 日志输出 —— 统一前缀 [<配置前缀>] 并按行刷新
VOID Dispatch::Log(LPCSTR lpszFormat, ...)
{
	va_list args;
	printf("[%s] ", szLogPrefix);
	va_start(args, lpszFormat);
	vprintf(lpszFormat, args);
	va_end(args);
	fflush(stdout);
}
