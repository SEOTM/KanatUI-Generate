#pragma once
#include <minwindef.h>
#include <windef.h>		// RECT(宿主窗口矩形的出参类型)
#include <stdint.h>
#include "framework/base/ControlSpec.h"		// vec2 / CtrlInfo(渲染接口的几何与控制词语汇)
#include "framework/base/BasicDesign.h"		// AutoSize / ProjCR(控件上下文的类型语汇)

// ================================
// 文件: Render.h
// 描述: 渲染能力接口(函数指针表) —— "渲染一份布局"所需的全部 UI 设施能力的形式定义:
//       渲染目标窗口的取得、窗口与内容容器的开合、ID 命名空间压栈、光标定位与内容原点、逐控件命中判定、
//       窗口层级置顶; 另含控件库提供给布局渲染出口的绘制接口(CtrlApi)
//       与宿主提供给调度层的宿主接口(HostApi, 含样式描述 StyleDesc)。
// 说明: ① 本文件只定义接口形式(能力类型 + 状态位常量 + 能力表结构体), 不含任何具体 UI 库的类型、
//          头文件与实现 —— 能力由实现件(见 framework\render\imgui\ability\ImGuiRender)填表提供, 由宿主
//          (见 framework\render\imgui\ImGuiHost)在初始化期注入, 消费件(见 framework\layout\LayoutRender)
//          只经本表调用 ⇒ 更换 UI 实现时改为注入另一张表, 消费件零改动;
//       ② 接口形式 = "能力类型(using 定义的函数指针类型) + 成员全为该类型的结构体", 不用 C++
//          抽象基类 —— 本工程在无外部库要求时不引入虚函数与 RTTI, 解耦沿用"函数指针注入"的既有手段;
//          能力类型的命名沿用 framework\base\FuncPtr.h 的 pfn 前缀口径, 表中成员取语义裸名;
//       ③ 表中的几何量一律为框架自有语汇(像素坐标 vec2), 由实现件换算为目标 UI 库的类型;
//       ④ 失败口径与框架一致: 静默返回码(FALSE), 不弹窗、不打印;
//       ⑤ 调用时机: 一律在宿主已开启一帧之后(实现方的"当前上下文"之内)。
// ================================

// ================================
// 区域: 窗口状态位
// 说明: 开窗口时把"本窗口的框架级状态"告知实现方, 由实现方翻译成目标 UI 库的窗口标志
// ================================
constexpr DWORD RENDER_WND_BASE    = 0x00000001;	// 基座窗口标准状态: 无标题栏 / 禁缩放 / 禁移动 / 禁折叠 / 焦点不置顶 / 无滚动条 / 滚轮无效
constexpr DWORD RENDER_WND_NOINPUT = 0x00000002;	// 窗口级无输入(严格模态: 本窗口内控件与悬停一律不响应)

// ================================
// 区域: 渲染能力类型(函数指针)
// 说明: 逐能力的函数指针类型 —— 命名沿用 framework\base\FuncPtr.h 的 pfn 前缀; 各能力的完整语义
//       (参数含义与调用约束)见下方 RenderApi 对应成员的注释
// ================================

// 类型: pfnAcquireWnd
// 描述: 取得本次渲染目标窗口能力的函数指针类型(期望位置、客户区尺寸与窗口标题一并以参数给出)
using pfnAcquireWnd = BOOL(*)(CONST vec2& pos, CONST vec2& size, LPCSTR szTitle);

// 类型: pfnOpenWnd
// 描述: 开窗口能力的函数指针类型
using pfnOpenWnd = BOOL(*)(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD dwStates);

// 类型: pfnCloseWnd
// 描述: 关窗口能力的函数指针类型
using pfnCloseWnd = VOID(*)();

// 类型: pfnBringWndToFront
// 描述: 窗口置顶能力的函数指针类型
using pfnBringWndToFront = VOID(*)();

// 类型: pfnOpenPanel
// 描述: 开内容容器能力的函数指针类型
using pfnOpenPanel = BOOL(*)(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD crBack);

// 类型: pfnClosePanel
// 描述: 关内容容器能力的函数指针类型
using pfnClosePanel = VOID(*)();

// 类型: pfnSetCursor
// 描述: 绘制光标定位能力的函数指针类型
using pfnSetCursor = VOID(*)(CONST vec2& pos);

// 类型: pfnGetContentOrigin
// 描述: 取内容原点能力的函数指针类型
using pfnGetContentOrigin = vec2(*)();

// 类型: pfnPushId
// 描述: 压入整数 ID 命名空间能力的函数指针类型
using pfnPushId = VOID(*)(INT32 nId);

// 类型: pfnPushPtrId
// 描述: 压入指针 ID 命名空间能力的函数指针类型
using pfnPushPtrId = VOID(*)(CONST VOID* pKey);

// 类型: pfnPopId
// 描述: 弹出 ID 命名空间能力的函数指针类型
using pfnPopId = VOID(*)();

// 类型: pfnTakeItemId
// 描述: 取末项 ID 能力的函数指针类型
using pfnTakeItemId = uint64_t(*)();

// 类型: pfnAddHitArea
// 描述: 补无绘制命中项能力的函数指针类型
using pfnAddHitArea = VOID(*)(LPCSTR szLabel, CONST vec2& pos, CONST vec2& size);

// 类型: pfnIsHitClicked
// 描述: 末项点击判定能力的函数指针类型
using pfnIsHitClicked = BOOL(*)();

// 类型: pfnIsRectClicked
// 描述: 矩形点击判定能力的函数指针类型
using pfnIsRectClicked = BOOL(*)(CONST vec2& mn, CONST vec2& mx);

// 类型: pfnGetDrawVtxCount
// 描述: 取绘制顶点数能力的函数指针类型
using pfnGetDrawVtxCount = INT64(*)();

// ================================
// 结构: RenderApi
// 描述: 渲染能力表 —— 一次布局渲染所需的 UI 设施能力(成员顺序即典型使用顺序)
// 说明: 整表由实现方一次性填好(进程级静态), 消费方持只读指针; 逐成员语义与调用约束见其注释
// ================================
struct RenderApi
{
	// --------------------------------
	// 区域: 窗口与内容容器
	// --------------------------------

	// 成员: AcquireWnd
	// 描述: 取得本次渲染的目标窗口(把"渲染到哪个窗口"的决定权收归实现方), 并使它成为当前绘制目标;
	//   期望标题以 UTF-8 给出(为 NULL 表示不改标题)
	pfnAcquireWnd AcquireWnd;

	// 成员: OpenWnd
	// 描述: 开启一个窗口(基座窗口与无独立句柄的内嵌窗口同走本项), 并把它作为当前绘制目标
	pfnOpenWnd OpenWnd;

	// 成员: CloseWnd
	// 描述: 结束当前窗口(与 OpenWnd 成对)
	pfnCloseWnd CloseWnd;

	// 成员: BringWndToFront
	// 描述: 把"最近开启的窗口"置于绘制顺序最前(内嵌窗口须绘制在其宿主之上时, 在 CloseWnd 之后调用)
	pfnBringWndToFront BringWndToFront;

	// 成员: OpenPanel
	// 描述: 在当前窗口内开启一个内容容器(承载内容控件的子区域), 并把它作为当前绘制目标
	pfnOpenPanel OpenPanel;

	// 成员: ClosePanel
	// 描述: 结束当前内容容器(与 OpenPanel 成对)
	pfnClosePanel ClosePanel;

	// 成员: SetCursor
	// 描述: 把当前窗口/容器的绘制光标定位到指定位置(像素; 当前窗口或容器的本地坐标)
	pfnSetCursor SetCursor;

	// 成员: GetContentOrigin
	// 描述: 取当前容器的内容原点(屏幕坐标) —— 即"容器内控件声明坐标的零点"
	pfnGetContentOrigin GetContentOrigin;

	// --------------------------------
	// 区域: ID 命名空间
	// --------------------------------

	// 成员: PushId
	// 描述: 压入一个整数 ID 命名空间(逐控件绘制前压入控件 id, 使同名控件互不冲突)
	pfnPushId PushId;

	// 成员: PushPtrId
	// 描述: 压入一个指针 ID 命名空间(内嵌窗口按布局对象地址隔离, 使多枚内嵌窗口的同名控件互不冲突)
	pfnPushPtrId PushPtrId;

	// 成员: PopId
	// 描述: 弹出最近压入的 ID 命名空间(与 PushId / PushPtrId 成对)
	pfnPopId PopId;

	// --------------------------------
	// 区域: 命中判定
	// --------------------------------

	// 成员: TakeItemId
	// 描述: 取"当前窗口最近一次提交的可交互项 ID"(逐控件绘制前后各取一次, 用于判断本控件是否自行提交了项)
	pfnTakeItemId TakeItemId;

	// 成员: AddHitArea
	// 描述: 在指定矩形上补一枚"无绘制命中项"(供纯图元控件与未自行提交项的控件)
	pfnAddHitArea AddHitArea;

	// 成员: IsHitClicked
	// 描述: 判定"最近提交的项"本帧是否被左键点击(须紧接绘制或 AddHitArea 之后调用)
	pfnIsHitClicked IsHitClicked;

	// 成员: IsRectClicked
	// 描述: 判定"指定矩形内"本帧是否发生左键按下(含当前窗口裁剪) —— 复合控件的矩形兜底判定用
	pfnIsRectClicked IsRectClicked;

	// 成员: GetDrawVtxCount
	// 描述: 取当前窗口累计的绘制顶点数
	pfnGetDrawVtxCount GetDrawVtxCount;
};

// ================================
// 区域: 控件绘制接口类型(函数指针)
// 说明: 控件库向布局渲染出口暴露的绘制能力 —— 逐能力的完整语义见下方 CtrlApi 对应成员的注释;
//       命名沿用 framework\base\FuncPtr.h 的 pfn 前缀口径
// ================================

// 类型: pfnUpdateCtrlContext
// 描述: 更新控件绘制上下文能力的函数指针类型(参考尺寸 / 投影配色 / 窗口位置记录槽 / 内容容器原点)
using pfnUpdateCtrlContext = BOOL(*)(CONST AutoSize& refsize, CONST ProjCR* pProj,
	CONST vec2* pWndPos, CONST vec2& orgPanel);

// 类型: pfnDrawCtrl
// 描述: 逐控件绘制能力的函数指针类型(按控件类型在实现内部分派)
using pfnDrawCtrl = BOOL(*)(CtrlInfo* ctrl);

// 类型: pfnHasActiveInput
// 描述: 查询"是否存在编辑中的输入控件"能力的函数指针类型
using pfnHasActiveInput = BOOL(*)();

// ================================
// 结构: CtrlApi
// 描述: 控件绘制接口表 —— 布局渲染出口逐帧所需的控件库能力(3 项)
// 说明: 整表由控件库一次性填好(进程级静态), 消费方持只读指针; 由宿主在 UI 设施初始化期
//       注入布局渲染出口(见 framework\layout\LayoutRender.h 的 SetCtrlApi), 未注入时渲染入口
//       静默失败(与 RenderApi 同口径)
// ================================
struct CtrlApi
{
	// 成员: UpdateContext
	// 描述: 每次渲染前设一次控件上下文(参考尺寸 / 投影配色 / 窗口位置记录槽 / 内容容器原点)
	pfnUpdateCtrlContext UpdateContext;

	// 成员: DrawCtrl
	// 描述: 按 CtrlType 内部分派绘制一枚控件(含 TITLE, 不再单列 TitleBar);
	//   返回 FALSE 表示该类型无绘制实现(未知类型仍按矩形占位绘制)
	pfnDrawCtrl DrawCtrl;

	// 成员: HasActiveInput
	// 描述: 是否存在编辑中的输入控件(供 IME 开关判定)
	pfnHasActiveInput HasActiveInput;
};

// ================================
// 区域: 窗口显示状态
// 说明: 宿主按"标准窗口序号"报告可见性; 具体 UI 实现把它翻译为自身窗口状态的真相
// ================================
enum WndShowState
{
	WND_SHOW_NEVER = 0,				// 建窗后从未显示
	WND_SHOW_VISIBLE,				// 当前可见
	WND_SHOW_HIDDEN_BY_ACTIVE,		// 当前不可见, 隐藏由"激活窗口数不足"策略发起(实现方可自行恢复)
	WND_SHOW_HIDDEN_BY_HOST,		// 当前不可见, 隐藏由宿主发起(实现方不擅自恢复)
};

// ================================
// 区域: 宿主回调类型(函数指针)
// 说明: 宿主主循环与调度层之间的回调签名 —— 调度层只提供流程本体, 循环与退出判据归宿主
// ================================

// 类型: pfnFrame
// 描述: 每帧处理流程的函数指针类型(宿主主循环每帧调用一次)
using pfnFrame = VOID(*)(VOID* pUser);

// 类型: pfnSegment
// 描述: 段前回调的函数指针类型(目标窗口后端开帧之后、UI 库帧开始之前调用)
using pfnSegment = VOID(*)(DWORD index, VOID* pUser);

// ================================
// 结构: StyleDesc
// 描述: 宿主样式描述 —— 圆角口径(4 项)与配色槽(GDI 色值 17 项), 由调度层计算、由宿主映射到具体 UI 库
// 说明: ① 色值口径 = GDI 打包值(0x00BBGGRR; 低字节 = R), 与 BasicDesign.h 的取色正门同源;
//       ② 宿主在建立每个窗口的 UI 面时把本描述写成该 UI 库的样式; 已就绪窗口在登记时立即刷新;
//       ③ 本结构只含框架自有语汇, 不含任何具体 UI 库的类型 ⇒ 调度层无需认识 UI 库。
// ================================
struct StyleDesc
{
	FLOAT fWindowRounding;			// 窗口圆角(根窗口 = 设计间隔)
	FLOAT fFrameRounding;			// 帧类件圆角(设计内边距)
	FLOAT fChildRounding;			// 内容子窗圆角(同上)
	FLOAT fPopupRounding;			// 弹出层圆角(同上)
	DWORD crWindowBg;				// 窗口底
	DWORD crChildBg;				// 内容子窗底
	DWORD crFrameBg;				// 帧类件底(输入框 / 按钮 / 表头)
	DWORD crFrameBgHovered;			// 帧类件底(悬停)
	DWORD crFrameBgActive;			// 帧类件底(激活)
	DWORD crButton;					// 按钮底
	DWORD crButtonHovered;			// 按钮底(悬停)
	DWORD crButtonActive;			// 按钮底(激活)
	DWORD crBorder;					// 边框
	DWORD crText;					// 文本
	DWORD crTextDisabled;			// 文本(禁用 / 次级)
	DWORD crSliderGrab;				// 滑块把手
	DWORD crSliderGrabActive;		// 滑块把手(激活)
	DWORD crCheckMark;				// 勾选标记
	DWORD crHeader;					// 表头底
	DWORD crHeaderHovered;			// 表头底(悬停)
	DWORD crHeaderActive;			// 表头底(激活)
};

// ================================
// 结构: HostApi
// 描述: 宿主接口表 —— 调度层所需的宿主能力(窗口生命周期 / 帧循环 / 样式 / 可见性 / 窗口矩形 / 自证统计)
// 说明: ① 整表由具体宿主一次性填好(进程级静态), 调度层持只读指针; 推荐经宿主的 Register 取得;
//       ② SetDarkMode 与 ApplyStyle 的语义是"登记": 宿主在建立每个窗口的 UI 面时应用, 已就绪窗口立即刷新;
//          ApplyStyle 返回 FALSE 只表示当时没有任何已就绪窗口(未就绪不视为失败);
//       ③ GetWndRect 取代调度层直接接触窗口句柄: 几何自证只经本项取窗口矩形与客户区矩形;
//       ④ 未列出的高级用法仍可直调具体宿主的静态接口(本表只是其函数指针化投影)。
// ================================
struct HostApi
{
	// 成员: EnableDpiAwareness
	// 描述: 打开进程级 DPI 感知(启动首件事; 幂等)
	VOID (*EnableDpiAwareness)();

	// 成员: Start
	// 描述: 启动宿主(建窗 → 建立 UI 设施), 并以段前回调挂载调度层提供的挂载点
	BOOL (*Start)(DWORD dwWndCount, LPCSTR szAppName, pfnSegment pfn, VOID* pUser);

	// 成员: Run
	// 描述: 运行主消息循环(每帧调一次调度层流程; 退出判据由调度层持有, 此处只读其地址)
	VOID (*Run)(pfnFrame pfnFrame, VOID* pUser, CONST BOOL* pbExit);

	// 成员: Release
	// 描述: 释放全部窗口的 UI 面与图形设施(幂等, 不动窗口)
	VOID (*Release)();

	// 成员: DestroyWnds
	// 描述: 销毁全部标准窗口并注销窗口类(幂等)
	VOID (*DestroyWnds)();

	// 成员: SetDarkMode
	// 描述: 登记亮暗基础样式(已就绪窗口立即刷新)
	VOID (*SetDarkMode)(BOOL bDark);

	// 成员: ApplyStyle
	// 描述: 登记宿主样式(圆角与配色); 返回 TRUE 表示已应用到至少一个就绪窗口, FALSE 表示当前没有就绪窗口
	BOOL (*ApplyStyle)(CONST StyleDesc& desc);

	// 成员: SetIMEDisabled
	// 描述: 设置本帧的 IME 覆盖(段结算时应用, 结算后失效)
	VOID (*SetIMEDisabled)(BOOL bDisabled);

	// 成员: GetStandardWndCount
	// 描述: 取已声明的标准窗口数量(调度层按此遍历可见性)
	DWORD (*GetStandardWndCount)();

	// 成员: GetWndShowState
	// 描述: 取指定窗口的显示状态(序号越界按从未显示处理)
	WndShowState (*GetWndShowState)(DWORD index);

	// 成员: GetWndRect
	// 描述: 取指定窗口的窗口矩形与客户区矩形(客户区矩形左上恒为 0,0); 窗口未建时返回 FALSE
	BOOL (*GetWndRect)(DWORD index, RECT* prcWnd, RECT* prcClient);

	// 成员: GetRenderStats
	// 描述: 取绘制量自证计数(段数 / 绘制命令数 / 顶点数 / 索引数)
	VOID (*GetRenderStats)(DWORD* pdwSegments, DWORD* pdwCmdLists, INT64* pllVtx, INT64* pllIdx);

	// 成员: GetResizeStats
	// 描述: 取窗口尺寸调整计数与最近一次调整后的客户区尺寸
	VOID (*GetResizeStats)(DWORD* pdwCount, DWORD* pdwLastW, DWORD* pdwLastH);

	// 成员: SetAppIcon
	// 描述: 登记应用窗口图标(HICON; 建窗前调用)
	VOID (*SetAppIcon)(VOID* hIcon);

	// 成员: GetNativeDevice
	// 描述: 取首个标准窗口的原生图形设备(未就绪时返回 NULL; 类型擦除)
	VOID* (*GetNativeDevice)();

	// 成员: GetNativeDeviceContext
	// 描述: 取首个标准窗口的原生图形设备上下文(未就绪时返回 NULL; 类型擦除)
	VOID* (*GetNativeDeviceContext)();
};
