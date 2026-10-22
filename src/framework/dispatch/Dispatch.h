// ================================
// 文件: Dispatch.h
// 描述: 调度层(纯静态单例) —— 事件绑定注入 / 会话分配器与场景装配 / 投影配色与宿主样式登记 /
//   每帧处理流程 / 无人值守参数 / 退出判据 / 清理链。
// 说明: 形态 = 全静态成员 + 私有删除构造析构 + DispatchData 成员块; 成员块由会话分配器 arena
//   placement new 分配、Release 时显式析构; 场景装配的分配与释放收在同模块的实现切片
//   framework\dispatch\DispatchScene.cpp(LoadScene / UnloadScene, 不对外暴露场景类)。
//   本层不持有窗口句柄、不认识任何具体 UI 库: 建窗 / 帧流程 / 消息循环归宿主, 本层只经
//   framework\render\Render.h 的 HostApi 访问宿主, 并把 pfnFrame 与退出判据地址交回宿主;
//   事件绑定表经函数指针注入(不直接依赖演示层的事件表); 项目身份(文档名 / 环境变量名 / 日志前缀)
//   一律取自 DispatchConfig ⇒ 本层不 include 项目身份头, 也不 include 具体 UI 库或宿主实现的头。
// ================================

#pragma once
#include <windows.h>		// MAX_PATH / ULONGLONG(成员块字段)
#include <stddef.h>			// size_t(绑定表条目数)

#include "framework/base/BasicDesign.h"		// AutoSize / ProjCR
#include "framework/dispatch/UIEventHandler.h"		// EventBinding(事件绑定表条目)
#include "framework/layout/Layout.h"		// Layout(渲染源)
#include "framework/layout/LayoutModel.h"	// RefSize(文档 autosize 直映)
#include "framework/render/Render.h"		// HostApi / StyleDesc / WndShowState(宿主接口契约)

class LayoutSolver;

// ================================
// 结构: DispatchConfig
// 描述: 调度层配置 —— 项目身份与运行开关(由进程入口按本工程常量填好; 框架层不含任何项目身份)
// 说明: ① 字符串在 Init 期复制进成员块 ⇒ 调用方无需保持其生命周期(传 NULL 按空串处理);
//       ② 日志前缀缺省为空串; 宿主按自身形态填写(例如项目缩写) ⇒ 输出形如 "[PREFIX] ";
//       ③ bDarkFollowSystem = TRUE 时每帧跟随系统亮暗; FALSE 时用 SetDarkMode 登记的值。
// ================================
struct DispatchConfig
{
	LPCSTR szDocName;			// 缺省布局文档名(如 "App.json"; exe 同目录)
	LPCSTR szDocEnv;			// 覆盖文档名的环境变量名(如 "APP_DOC")
	LPCSTR szAutoCloseEnv;		// 自动退出毫秒数的环境变量名(如 "APP_AUTOCLOSE_MS")
	LPCSTR szLogPrefix;			// 日志前缀(如 "APP"; 打印为 "[APP] ")
	DWORD dwWndCount;			// 标准窗口数(本工程 = 1)
	BOOL bDarkFollowSystem;		// TRUE = 每帧跟随系统; FALSE = 用 SetDarkMode 的值
};

// ================================
// 类名: Dispatch
// 描述: 调度层(纯静态; 无实例 ⇒ 进程级单例语义)
// ================================
class Dispatch
{
private:
	// 结构: DispatchData
	// 描述: 调度成员块 —— 集中收纳调度状态, 由会话分配器 arena placement new 分配(见 DispatchScene.cpp)
	struct DispatchData
	{
		Layout* pLayout;			// 布局对象(渲染源)					| 8 字节
		LayoutSolver* pSolver;		// 布局解析器(会话分配器持有)		| 8 字节
		ProjCR* pProjCR;			// 投影配色(启动期建立)				| 8 字节
		CONST HostApi* pHost;		// 宿主接口表(Init 注入; NULL = 未注入)	| 8 字节
		AutoSize ref;				// 内部参考尺寸(几何解算基准)
		RefSize refSize;			// 参考尺寸/经验参数(文档 autosize 直映)
		LONG nWndX;					// 窗口桌面位置 X					| 4 字节
		LONG nWndY;					// 窗口桌面位置 Y					| 4 字节
		DWORD dwWndW;				// 窗口客户区宽						| 4 字节
		DWORD dwWndH;				// 窗口客户区高						| 4 字节
		DWORD dwWndCount;			// 标准窗口数(配置给出; 下限 1)		| 4 字节
		BOOL bReady;				// 场景就绪标志(布局已构建且几何已解算)	| 4 字节
		BOOL bExit;					// 退出判据(宿主主循环每帧只读其地址)	| 4 字节
		BOOL bDarkFollowSystem;		// 亮暗是否每帧跟随系统				| 4 字节
		BOOL bDarkMode;				// 不跟随时生效的亮暗值				| 4 字节
		DWORD dwFrameCount;			// 已执行帧数(渲染自证节流与收尾打印)	| 4 字节
		DWORD dwAutoCloseMs;		// 无人值守自动退出毫秒数(0 = 不自动退出)	| 4 字节
		ULONGLONG qwStartTick;		// 启动时刻(无人值守计时基准)		| 8 字节
		DWORD dwLastShowState;		// 最近一帧首窗显示状态(收尾自证缓存; 枚举值)	| 4 字节
		DWORD dwLastVisible;		// 最近一帧可见窗口数(收尾自证缓存)	| 4 字节
		CHAR szDocName[MAX_PATH];	// 布局文档名(缺省名; 载入期可被环境变量覆盖)
		CHAR szDocEnv[64];			// 覆盖文档名的环境变量名(空串 = 不覆盖)
		CHAR szAutoCloseEnv[64];	// 自动退出毫秒数的环境变量名(空串 = 不读取)
	};

	// 成员: data
	// 描述: 调度成员块(会话分配器 arena 内; NULL = 未就绪)
	static DispatchData* data;

	// 私有化构造/析构(纯静态工具类)
	Dispatch() = delete;
	~Dispatch() = delete;
	Dispatch(const Dispatch&) = delete;
	Dispatch& operator=(const Dispatch&) = delete;

	// 函数: LoadScene
	// 描述: 建立会话分配器与成员块, 并从 exe 目录载入布局文档(解析 → 展开 → 几何解算 → 位置定稿 → 自证)
	static BOOL LoadScene(CONST DispatchConfig& cfg);

	// 函数: UnloadScene
	// 描述: 释放场景资源(布局缓冲 → 布局对象 → 解析器 → 成员块显式析构 → 领域分配器解绑 → 会话分配器)
	static VOID UnloadScene();

	// 函数: EnsureProjCR
	// 描述: 确保投影配色已建立并跟随当前亮暗口径(跟随系统或 SetDarkMode 登记值; 反复调用幂等)
	static VOID EnsureProjCR();

	// 函数: ApplyHostStyle
	// 描述: 按参考尺寸与投影配色填宿主样式描述(圆角与 17 个配色槽); 配色未就绪时返回 FALSE
	static BOOL ApplyHostStyle(StyleDesc& desc);

	// 函数: RefreshWndShowCache
	// 描述: 刷新最近一帧的窗口显示状态缓存(收尾自证在窗口销毁后仍可打印当时的可见性)
	static VOID RefreshWndShowCache();

	// 函数: PrintRenderProof
	// 描述: 渲染自证 —— 打印绘制量、窗口尺寸调整次数、控件数与窗口显示状态
	static VOID PrintRenderProof(LPCSTR lpszTag);

	// 函数: PrintWndGeometry
	// 描述: 打印首帧前的建窗几何(Start 默认几何的实测值; 首帧渲染前窗口尚未按文档几何结算)
	static VOID PrintWndGeometry();

	// 函数: Log
	// 描述: 日志输出 —— 统一前缀 [<配置前缀>] 并按行刷新
	static VOID Log(LPCSTR lpszFormat, ...);

public:
	// 类型: BindingsFunc
	// 描述: 事件绑定表取得函数(创建期注入; 绑定点在应用侧, 框架层不反向依赖)
	using BindingsFunc = CONST UIEventHandler::EventBinding* (*)(size_t* pnCount);

	// 函数: Init
	// 描述: 初始化调度层(控制台 UTF-8 → DPI 感知 → 事件绑定表注入 → 场景载入 → 宿主注入与样式登记 → 无人值守参数)
	static BOOL Init(CONST HostApi* pHost, CONST DispatchConfig& cfg, BindingsFunc pfnBindings);

	// 函数: SetDarkMode
	// 描述: 覆盖"跟随系统"的亮暗口径(调用后本进程不再跟随系统; 宿主已就绪窗口立即刷新)
	static VOID SetDarkMode(BOOL bDark);

	// 函数: Frame
	// 描述: 每帧处理流程(建窗几何自证 → 数据层更新 → 布局渲染出口 → 可见性缓存 → IME 覆盖 → 渲染自证 → 退出判据)
	static VOID Frame(VOID* pUser);

	// 函数: OnSegment
	// 描述: 段前回调范例挂载点(后端开帧之后、UI 库帧开始之前; 当前为空实现)
	static VOID OnSegment(DWORD index, VOID* pUser);

	// 函数: Release
	// 描述: 释放调度资源(投影配色 → 场景与成员块 → 会话分配器; 幂等)
	static VOID Release();

	// 函数: GetLayout
	// 描述: 取布局对象(渲染源; 未就绪时返回空对象)
	static Layout& GetLayout();

	// 函数: GetRef
	// 描述: 取内部参考尺寸(控件几何解算与绘制取值基准; 未就绪时返回缺省参考量)
	static CONST AutoSize& GetRef();

	// 函数: GetFontSize
	// 描述: 取建面字号(由文档 autosize 的参考字号推导; 无效时按缺省字号兜底)
	static FLOAT GetFontSize();

	// 函数: GetWndCount
	// 描述: 取本调度需要创建的标准窗口数量(未就绪时按单窗口兜底)
	static DWORD GetWndCount();

	// 函数: GetWndRect
	// 描述: 取布局窗口矩形(桌面位置取自窗口位置记录; 客户区尺寸取文档 autosize 推导值)
	static BOOL GetWndRect(LONG* x, LONG* y, DWORD* w, DWORD* h);

	// 函数: GetExitAddress
	// 描述: 取退出判据地址(宿主主循环每帧只读; 未就绪时返回 NULL)
	static CONST BOOL* GetExitAddress();

	// 函数: IsReady
	// 描述: 调度层是否就绪(成员块已分配且场景已载入)
	static BOOL IsReady();
};
