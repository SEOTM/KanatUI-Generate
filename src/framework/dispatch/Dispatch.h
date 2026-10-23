// ================================
// 文件: Dispatch.h
// 描述: 调度层(纯静态单例) —— 事件绑定注入 / 会话分配器与场景装配 / 布局来源五种入口 / 布局槽状态 /
//   投影配色与宿主样式登记 / 每帧处理流程 / 无人值守参数 / 退出判据 / 清理链。
// 说明: 形态 = 全静态成员 + 私有删除构造析构 + DispatchData 成员块; 成员块由会话分配器 arena
//   placement new 分配、Release 时显式析构; 场景装配的分配与释放收在同模块的实现切片
//   framework\dispatch\DispatchScene.cpp(LoadScene / UnloadScene 与布局槽辅助, 不对外暴露场景类)。
//   布局层对本层开放: 初始化可按 LayoutSource 的四种实体来源(缺省加载链 / 内存缓冲 / 布局模型 /
//   布局对象数组)装配, 也可声明为延迟装配(LAYOUT_SOURCE_DEFERRED) —— 此时只建立会话基础设施,
//   布局由业务代码在 Init 之后经 AttachLayouts / AddLayout 逐枚提交(含运行期替换)。
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
#include "framework/layout/LayoutModel.h"	// FileMidState / RefSize(布局模型与文档 autosize 直映)
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
// 枚举: LayoutSourceKind
// 描述: 布局来源种类 —— 初始化调度层时布局从何处来
// 说明: ① 缺省 = 文档加载链(同名 bin 优先, 其次文档名; 形态由内容嗅探判定);
//       ② 内存缓冲 = 调用方给出的 JSON / bin 字节; 解析失败回退缺省加载链(不视为初始化失败);
//       ③ 布局模型 = 调用方持有的中间态(FileMidState, 见 LayoutModel.h); 直接展开, 不再解析文档;
//       ④ 布局对象数组 = 调用方已展开并解算好的 Layout 对象; 直接使用, 本层不做解析与几何解算;
//       ⑤ 延迟装配 = 初始化不提交真实布局; 业务代码在 Init 之后经 AttachLayouts / AddLayout 逐枚提交。
// ================================
enum LayoutSourceKind
{
	LAYOUT_SOURCE_DEFAULT = 0,		// 缺省加载链(文档文件)
	LAYOUT_SOURCE_MEMORY,			// 内存缓冲(JSON / bin)
	LAYOUT_SOURCE_MODEL,			// 布局模型(中间态)
	LAYOUT_SOURCE_LAYOUTS,			// 布局对象数组 + 参考尺寸数组
	LAYOUT_SOURCE_DEFERRED			// 延迟装配(运行期由业务代码提交)
};

// ================================
// 结构: LayoutSource
// 描述: 布局来源描述 —— Init 的第四段入参; 一律经工厂函数构造(判别位与载荷由工厂成对写入)
// 说明: ① 载荷只在 Init 期被取用一次: 内存缓冲只读不改写(JSON 形态内部会补结尾符副本),
//          布局模型与布局对象数组按借用处理(所有权归调用方, 本层不释放);
//       ② 布局对象数组的元素须已就绪(结构件齐备、像素几何已解算、层次已推导) —— 本层直接使用,
//          不再解析文档也不重算几何;
//       ③ 数组长度合法域 = 1..DISPATCH_SLOT_MAX; 初始化时仅首枚可见(其余由业务代码经
//          SetLayoutVisible 打开), 与"默认渲染首枚"的口径一致。
// ================================
struct LayoutSource
{
	LayoutSourceKind kind;			// 来源种类(工厂函数写入)
	LPCVOID pMemData;				// 内存缓冲首址(仅 MEMORY)
	DWORD cbMemSize;				// 内存缓冲字节数(仅 MEMORY)
	CONST FileMidState* pModel;		// 布局模型(仅 MODEL; 中间态整份)
	Layout* CONST* ppLayouts;		// 布局对象数组(仅 LAYOUTS)
	CONST AutoSize* pRefs;			// 参考尺寸数组(仅 LAYOUTS; 与布局数组同长)
	DWORD dwLayoutCount;			// 数组长度(仅 LAYOUTS)

	// 函数: CreateDefault
	// 描述: 构造"缺省加载链"来源(不传任何内容时的形态)
	static LayoutSource CreateDefault()
	{
		LayoutSource src{};
		src.kind = LAYOUT_SOURCE_DEFAULT;
		return src;
	}

	// 函数: CreateMemory
	// 描述: 构造"内存缓冲"来源(JSON 或 bin 字节; 解析失败回退缺省加载链)
	static LayoutSource CreateMemory(LPCVOID pData, DWORD cbSize)
	{
		LayoutSource src{};
		src.kind = LAYOUT_SOURCE_MEMORY;
		src.pMemData = pData;
		src.cbMemSize = cbSize;
		return src;
	}

	// 函数: CreateModel
	// 描述: 构造"布局模型"来源(整份中间态; 直接展开)
	static LayoutSource CreateModel(CONST FileMidState* pModel)
	{
		LayoutSource src{};
		src.kind = LAYOUT_SOURCE_MODEL;
		src.pModel = pModel;
		return src;
	}

	// 函数: CreateLayouts
	// 描述: 构造"布局对象数组"来源(布局数组 + 参考尺寸数组; 直接使用)
	static LayoutSource CreateLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount)
	{
		LayoutSource src{};
		src.kind = LAYOUT_SOURCE_LAYOUTS;
		src.ppLayouts = ppLayouts;
		src.pRefs = pRefs;
		src.dwLayoutCount = dwCount;
		return src;
	}

	// 函数: CreateDeferred
	// 描述: 构造"延迟装配"来源(初始化不提交真实布局; 运行期由业务代码逐枚提交)
	static LayoutSource CreateDeferred()
	{
		LayoutSource src{};
		src.kind = LAYOUT_SOURCE_DEFERRED;
		return src;
	}
};

// ================================
// 类名: Dispatch
// 描述: 调度层(纯静态; 无实例 ⇒ 进程级单例语义)
// ================================
class Dispatch
{
private:
	// 结构: LayoutSlot
	// 描述: 布局槽 —— 一枚布局对象及其参考尺寸与渲染状态(槽数组由会话分配器持有)
	struct LayoutSlot
	{
		Layout* pLayout;		// 布局对象(渲染源; NULL = 空槽)			| 8 字节
		AutoSize ref;			// 参考尺寸(该布局的几何解算与绘制基准)
		BOOL bVisible;			// 是否参与渲染							| 4 字节
		BOOL bNested;			// 是否按嵌套(内嵌)渲染 —— 即 Flex 渲染, 沿用前一枚基座布局的窗口段	| 4 字节
		BOOL bOwned;			// 布局对象是否归调度层(TRUE = 释放时析构)	| 4 字节
	};

	// 结构: DispatchData
	// 描述: 调度成员块 —— 集中收纳调度状态, 由会话分配器 arena placement new 分配(见 DispatchScene.cpp)
	struct DispatchData
	{
		LayoutSlot* pSlots;			// 布局槽数组(会话分配器持有)		| 8 字节
		LayoutSolver* pSolver;		// 布局解析器(会话分配器持有)		| 8 字节
		ProjCR* pProjCR;			// 投影配色(启动期建立)				| 8 字节
		CONST HostApi* pHost;		// 宿主接口表(Init 注入; NULL = 未注入)	| 8 字节
		DWORD dwSlotCount;			// 已用槽数(布局数量)				| 4 字节
		DWORD dwSlotCapacity;		// 槽数组容量(按需扩容)				| 4 字节
		RefSize refSize;			// 参考尺寸/经验参数(首枚布局的文档 autosize 直映)
		LONG nWndX;					// 窗口桌面位置 X					| 4 字节
		LONG nWndY;					// 窗口桌面位置 Y					| 4 字节
		DWORD dwWndW;				// 窗口客户区宽(首枚布局)			| 4 字节
		DWORD dwWndH;				// 窗口客户区高(首枚布局)			| 4 字节
		DWORD dwWndCount;			// 标准窗口数(配置给出; 下限 1)		| 4 字节
		BOOL bReady;				// 场景就绪标志(至少一枚布局已装配)	| 4 字节
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
	// 描述: 建立会话分配器与成员块, 并按布局来源装配场景(解析 / 展开 / 几何解算 / 位置定稿 / 自证)
	static BOOL LoadScene(CONST DispatchConfig& cfg, CONST LayoutSource& source);

	// 函数: UnloadScene
	// 描述: 释放场景资源(布局槽与布局对象 → 解析器 → 成员块显式析构 → 领域分配器解绑 → 会话分配器)
	static VOID UnloadScene();

	// 函数: ResetSlots
	// 描述: 清空布局槽(归调度层所有的布局对象先回收控件缓冲再析构; 槽数归零)
	static BOOL ResetSlots();

	// 函数: ReserveSlots
	// 描述: 保证槽数组至少容纳指定数量(不足时按步进扩容; 旧块随会话分配器整体释放)
	static BOOL ReserveSlots(DWORD dwCapacity);

	// 函数: AppendSlot
	// 描述: 在槽尾追加一枚布局(容量不足时先扩容)
	static BOOL AppendSlot(Layout* pLayout, CONST AutoSize& ref, BOOL bOwned, BOOL bVisible, BOOL bNested);

	// 函数: UseLayouts
	// 描述: 整体替换布局槽(先清空), 逐枚接管调用方给出的布局对象(不取得所有权)
	static BOOL UseLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount, BOOL bFirstOnly);

	// 函数: BuildFromModel
	// 描述: 从布局模型逐枚展开布局对象并解算几何(前 dwWant 枚; 产物归调度层所有)
	static BOOL BuildFromModel(CONST FileMidState& state, DWORD dwWant);

	// 函数: ApplyWndGeometry
	// 描述: 按布局参考参数推导首枚窗口几何与该布局的参考尺寸(AutoSize)
	static BOOL ApplyWndGeometry(CONST RefSize& refSize, AutoSize& refOut, DWORD* pdwWndW, DWORD* pdwWndH);

	// 函数: CountTotalCtrls
	// 描述: 统计全部布局槽的控件数(自证用)
	static DWORD CountTotalCtrls();

	// 函数: CountBoundCtrls
	// 描述: 统计全部布局槽内已绑定事件处理器的控件数(自证用)
	static DWORD CountBoundCtrls();

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

	// 常量: DISPATCH_SLOT_MAX
	// 描述: 布局槽数量上限(单会话可装配的布局枚数; 超出即拒绝, 不静默截断)
	static constexpr DWORD DISPATCH_SLOT_MAX = LAYOUT_MAX;

	// 函数: Init
	// 描述: 初始化调度层(控制台 UTF-8 → DPI 感知 → 事件绑定表注入 → 场景装配 → 宿主注入与样式登记 → 无人值守参数)
	static BOOL Init(CONST HostApi* pHost, CONST DispatchConfig& cfg, BindingsFunc pfnBindings,
		CONST LayoutSource& source = LayoutSource::CreateDefault());

	// 函数: AttachLayouts
	// 描述: 整体替换布局(清空现有槽后逐枚接管; 含运行期替换; 布局对象所有权归调用方)
	static BOOL AttachLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount);

	// 函数: AddLayout
	// 描述: 追加一枚布局(逐枚提交; 布局对象所有权归调用方; 新槽默认可见且按基座渲染)
	static BOOL AddLayout(Layout* pLayout, CONST AutoSize& ref);

	// 函数: SetLayoutVisible
	// 描述: 设置指定槽的可见性(不可见的布局不参与本帧渲染)
	static BOOL SetLayoutVisible(DWORD index, BOOL bVisible);

	// 函数: SetLayoutNested
	// 描述: 设置指定槽的嵌套状态(TRUE = 按 Flex 渲染, 沿用前一枚基座布局的窗口段; FALSE = 基座渲染)
	static BOOL SetLayoutNested(DWORD index, BOOL bNested);

	// 函数: GetLayoutState
	// 描述: 取指定槽的可见性与嵌套状态(越界返回 FALSE)
	static BOOL GetLayoutState(DWORD index, BOOL* pbVisible, BOOL* pbNested);

	// 函数: GetLayoutCount
	// 描述: 取布局数量(延迟装配且未提交时返回 0)
	static DWORD GetLayoutCount();

	// 函数: GetLayoutAt
	// 描述: 取指定槽的布局对象(越界返回空对象)
	static Layout& GetLayoutAt(DWORD index);

	// 函数: GetRefAt
	// 描述: 取指定槽的参考尺寸(越界返回缺省参考量)
	static CONST AutoSize& GetRefAt(DWORD index);

	// 函数: SetDarkMode
	// 描述: 覆盖"跟随系统"的亮暗口径(调用后本进程不再跟随系统; 宿主已就绪窗口立即刷新)
	static VOID SetDarkMode(BOOL bDark);

	// 函数: Frame
	// 描述: 每帧处理流程(建窗几何自证 → 数据层更新 → 布局渲染清单 → 可见性缓存 → IME 覆盖 → 渲染自证 → 退出判据)
	static VOID Frame(VOID* pUser);

	// 函数: OnSegment
	// 描述: 段前回调范例挂载点(后端开帧之后、UI 库帧开始之前; 当前为空实现)
	static VOID OnSegment(DWORD index, VOID* pUser);

	// 函数: Release
	// 描述: 释放调度资源(投影配色 → 场景与成员块 → 会话分配器; 幂等)
	static VOID Release();

	// 函数: GetLayout
	// 描述: 取首枚布局对象(渲染源; 未就绪时返回空对象)
	static Layout& GetLayout();

	// 函数: GetRef
	// 描述: 取首枚参考尺寸(控件几何解算与绘制取值基准; 未就绪时返回缺省参考量)
	static CONST AutoSize& GetRef();

	// 函数: GetFontSize
	// 描述: 取建面字号(由首枚布局的参考字号推导; 无效时按缺省字号兜底)
	static FLOAT GetFontSize();

	// 函数: GetWndCount
	// 描述: 取本调度需要创建的标准窗口数量(未就绪时按单窗口兜底)
	static DWORD GetWndCount();

	// 函数: GetWndRect
	// 描述: 取主布局窗口矩形(桌面位置取自首枚布局的窗口件位置记录; 客户区尺寸取推导值)
	static BOOL GetWndRect(LONG* x, LONG* y, DWORD* w, DWORD* h);

	// 函数: GetExitAddress
	// 描述: 取退出判据地址(宿主主循环每帧只读; 未就绪时返回 NULL)
	static CONST BOOL* GetExitAddress();

	// 函数: IsReady
	// 描述: 调度层是否就绪(成员块已分配且至少一枚布局已装配)
	static BOOL IsReady();
};
