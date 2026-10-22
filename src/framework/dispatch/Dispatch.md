# Dispatch 模块 (src\framework\dispatch\Dispatch.h)

## 概述

调度层:纯静态单例(全静态成员, 构造与析构均删除), 无实例 ⇒ 进程级单例语义。职责 = 事件绑定表注入、
会话分配器与场景装配、投影配色与宿主样式登记、每帧处理流程、无人值守参数、退出判据与清理链。

本模块属**调度层**(`src\framework\dispatch`), 依赖 `framework` 与 `common` 的公开接口, 不反向被框架层依赖。它不认识窗口句柄与
渲染设施: 建窗、UI 上下文、图形设施、消息循环与窗口过程一律归宿主实现(见 `framework\render` 下的宿主件); 本模块只经
`framework\render\Render.h` 的宿主接口表(`HostApi`)访问宿主, 并向宿主交出 `pfnFrame` 与退出判据地址。事件绑定表经函数指针
注入(不直接依赖演示层的事件表), 以保证绑定表内容属应用侧; 项目身份(文档名 / 环境变量名 / 日志前缀)取自 `DispatchConfig`。

场景装配的分配与释放(会话分配器 / 成员块 / 文档解析 / 布局展开 / 几何解算 / 位置定稿 / 自证日志)收在同模块的
实现切片 `src\framework\dispatch\DispatchScene.cpp`(私有静态函数 `LoadScene` / `UnloadScene`), 不对外暴露任何类。

本模块包含以下编程接口:

### 类型

| 类型 | 说明 |
| --- | --- |
| `BindingsFunc` | 事件绑定表取得函数指针类型: 启动期由 `Init` 调用一次, 取得"事件名 → 处理函数"表。 |

### 结构

| 结构 | 说明 |
| --- | --- |
| `DispatchConfig` | 调度层配置(6 字段): 布局文档名 / 文档名环境变量 / 自动退出环境变量 / 日志前缀 / 标准窗口数 / 亮暗是否跟随系统。 |

### 类

| 类 | 说明 |
| --- | --- |
| `Dispatch` | 调度层: 初始化 / 亮暗覆盖 / 每帧流程 / 段前挂载点 / 释放 / 布局与参考尺寸访问 / 建面字号 / 窗口数量与矩形 / 退出判据地址 / 就绪查询。 |

---

## BindingsFunc 类型 (Dispatch)

事件绑定表取得函数的类型: 把应用侧的事件绑定表交给调度层, 再由调度层注入框架侧解析器。

### 语法

```cpp
using BindingsFunc = CONST UIEventHandler::EventBinding* (*)(size_t* pnCount);
```

### 参数

`[out] pnCount`

类型: **size_t\***

接收绑定表条目数。被调函数可为 `NULL`(不写回); 调度层只把该值用于启动日志与空表注入。

### 返回值

类型: **CONST UIEventHandler::EventBinding\***

绑定表首址。表由应用侧持有, 进程级常驻; 调度层只把指针交给 `UIEventHandler::SetBindings`, 不复制、不释放。

### 注解

函数由 `Dispatch::Init` 在场景载入之前调用一次, 因此被调函数须已可用(不得延迟初始化)。返回 `NULL` 或条目数为 `0`
都按空表注入 ⇒ 所有 `event` 名解析为空绑定(控件无响应), 不报错、不中断启动。

---

## DispatchConfig 结构 (Dispatch)

调度层配置:项目身份与运行形态经此注入, 调度层本身不含任何项目身份常量。

### 语法

```cpp
struct DispatchConfig
{
    LPCSTR szDocName;
    LPCSTR szDocEnv;
    LPCSTR szAutoCloseEnv;
    LPCSTR szLogPrefix;
    DWORD  dwWndCount;
    BOOL   bDarkFollowSystem;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `szDocName` | 缺省布局文档名(如 `"App.json"`); 按 exe 同目录解释, 构建期由 CMake 复制。 |
| `szDocEnv` | 覆盖文档名的环境变量名(如 `"APP_DOC"`); 空指针或取值为空 ⇒ 用 `szDocName`。 |
| `szAutoCloseEnv` | 自动退出毫秒数的环境变量名(如 `"APP_AUTOCLOSE_MS"`); 空指针 ⇒ 不设自动退出。 |
| `szLogPrefix` | 日志前缀(如 `"APP"`; 每行打印为 `[<前缀>] `)。 |
| `dwWndCount` | 标准窗口数量(由宿主按自身形态填写); 为 0 时按 1 兜底。 |
| `bDarkFollowSystem` | `TRUE` = 每帧跟随系统亮暗; `FALSE` = 用 `SetDarkMode` 的登记值。 |

### 注解

`szDocName` / `szDocEnv` / `szAutoCloseEnv` / `szLogPrefix` 指向的字符串由调用方持有且须进程级常驻(调度层只存
指针, 不复制 —— 场景切片按 `MAX_PATH` 内的缓冲使用它们)。`Identity.h` 的 `IDENTITY_LOG_PREFIX` 即经 `szLogPrefix`
交给调度层, 因此日志前缀可配置, 派生新项目不必改实现。配置在 `Init` 时被完整取用一次, 之后不再读取。

---

## Dispatch 类 (Dispatch)

调度层:集中持有调度状态(成员块 `DispatchData`)并编排启动、每帧与释放三个阶段。

### 语法

```cpp
class Dispatch
{
public:
    using BindingsFunc = CONST UIEventHandler::EventBinding* (*)(size_t* pnCount);

    static BOOL Init(CONST HostApi* pHost, CONST DispatchConfig& cfg, BindingsFunc pfnBindings);
    static VOID SetDarkMode(BOOL bDark);
    static VOID Frame(VOID* pUser);
    static VOID OnSegment(DWORD index, VOID* pUser);
    static VOID Release();
    static Layout& GetLayout();
    static CONST AutoSize& GetRef();
    static FLOAT GetFontSize();
    static DWORD GetWndCount();
    static BOOL GetWndRect(LONG* x, LONG* y, DWORD* w, DWORD* h);
    static CONST BOOL* GetExitAddress();
    static BOOL IsReady();
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `Init(pHost, cfg, pfnBindings)` | 初始化调度层: 控制台 UTF-8 → 绑定注入 → 场景载入 → 宿主注入与样式登记 → 无人值守参数。`pHost` 为空或 `pfnBindings` 为空 ⇒ 直接失败。 |
| `SetDarkMode(bDark)` | 覆盖"跟随系统"的亮暗口径(调用后本进程不再跟随系统; 宿主已就绪窗口立即刷新)。 |
| `Frame(pUser)` | 每帧处理流程: 数据层更新 → 布局渲染出口 → 可见性缓存 → IME 覆盖 → 渲染自证 → 无人值守退出判据。 |
| `OnSegment(index, pUser)` | 段前回调范例挂载点; 当前为空实现(本形态无合成输入注入需求)。 |
| `Release()` | 释放调度资源: 收尾自证 → 投影配色 → 场景与成员块 → 会话分配器。幂等。 |
| `GetLayout()` | 取布局对象(渲染源); 未就绪时返回空对象。 |
| `GetRef()` | 取内部参考尺寸; 未就绪时返回缺省参考量。 |
| `GetFontSize()` | 取建面字号: 布局参考字号(像素), 无效时按缺省字号 `18` 兜底。 |
| `GetWndCount()` | 取需要创建的标准窗口数量; 单窗口模式恒为 `1`。 |
| `GetWndRect(x, y, w, h)` | 取窗口矩形: 桌面位置取自窗口件位置记录, 客户区尺寸取文档 `autosize` 推导值。 |
| `GetExitAddress()` | 取退出判据地址; 未就绪时返回 `NULL`。 |
| `IsReady()` | 调度层是否就绪(成员块已分配且场景已载入)。 |

### 注解

**日志**: 全部输出统一经内部静态 `Log(fmt, ...)` 包装(前缀 `[<cfg.szLogPrefix>] `, 逐行刷新); 调度层不直接
调 `printf`, 因此前缀可配置而日志行结构不变。

**成员块**: 调度状态集中在一个私有 `DispatchData` 结构里, 由会话分配器的线性分配器以 placement new 分配
(在 `AllocatorHub::Init` 之后), `Release` 时显式析构并置空。成员块包含布局对象与解析器指针、参考尺寸与
经验参数、窗口矩形、投影配色指针、宿主接口表指针、就绪/退出标志、亮暗口径、帧计数、自动退出毫秒数、启动 tick、
最近一帧窗口显示缓存、布局文档名缓冲与环境变量名缓冲。调度层未就绪时成员块为空, 全部访问接口回退到空对象 /
缺省值 / `NULL`。

**`Init` 的次序不可交换**: DPI 感知须早于场景载入(场景几何按屏幕像素推导, 未感知时系统会按缩放虚拟化屏幕
尺寸); 事件绑定注入须早于场景载入(展开布局时即按 `event` 名解析处理器); 投影配色与样式登记须早于宿主建 UI 面
(初始化期即应用一次宿主样式)。成功时依次打印事件绑定表、场景载入、事件绑定、控件层次、控件坐标、字号与就绪日志。

**字号**: 建面字号 = 布局文档 `autosize` 的参考字号经换算基准推导的像素值(与控件几何同源), 由
`GetFontSize` 只读投影; 进程入口在建窗之前把它交给具体宿主登记。参考字号无效(小于 `12`)时按缺省 `18` 兜底。

**失败回退**: `Init` 内不弹窗, 错误只以 `FALSE` 上报(由进程入口弹中文错误框)。任一阶段失败都调用
`Release` 逆序清理已建立的部分, 因此 `Init` 失败后不要求调用方再调 `Release`。传入空的绑定表取得函数时
直接失败, 不进入场景载入。

**每帧流程**: `Frame` 只在首帧打印一次建窗几何(此时窗口仍是宿主 `Start` 的默认几何, 尚未按文档几何
结算); 随后每帧确保投影配色跟随当前亮暗口径、调用 `LayoutRender::RenderBase`、刷新窗口可见性缓存、按控件库
报告设置 IME 覆盖, 并在首帧与每 60 帧打印一次渲染自证。无人值守到点时置退出判据, 由宿主主循环在下一帧
开头读取并退出。

**收尾自证**: 主循环退出后, `Release` 在场景释放之前打印一次 `渲染自证(收尾)`。窗口可见状态取最近一帧的
缓存, 因此即使进程入口已先调用宿主的 `DestroyWnds`(窗口显示状态已复位), 收尾行仍报告运行期的真实
可见性; 绘制量计数由宿主跨释放保留。

**退出判据**: 成员块内的 `BOOL bExit` 是唯一判据, `GetExitAddress` 返回其地址, 宿主主循环每帧只读。`ESC`、
系统关闭与无人值守到点三条路径最终都使其生效(前两者经窗口过程的 `WM_QUIT`, 后者由 `Frame` 置位)。

**线程与所有权**: 全部接口须在进程主线程串行调用。`GetLayout` / `GetRef` 返回的引用在 `Release` 之后失效;
`GetExitAddress` 返回的地址在 `Release` 之后失效。`OnSegment` 只作为宿主 `Start` 的段前回调挂载点,
在其中不得开帧、结算段或调用 `Release`。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/dispatch/Dispatch.h`(`src\framework\dispatch\Dispatch.h`) |
| 实现 | `src\framework\dispatch\Dispatch.cpp`;场景装配切片 = `src\framework\dispatch\DispatchScene.cpp` |
| 命名空间 | 无(全局类 `Dispatch`) |
| 依赖 | `framework/base/BasicDesign.h` · `framework/dispatch/UIEventHandler.h` · `framework/layout/Layout.h` · `framework/layout/LayoutModel.h` · `framework/layout/LayoutSolver.h` · `framework/layout/LayoutBuilder.h` · `framework/layout/LayoutRender.h` · `framework/layout/GeometrySolver.h` · `framework/base/ControlSpec.h` · `framework/render/Render.h` · `common/AppEnv.h` · `common/AllocatorHub.h` · `common/ResourceLoader.h` · `user32.dll` · `dwmapi.dll` |
| 平台 | Windows 全版本(控制台子系统; 启动首件事为控制台代码页切 UTF-8) |
| 线程安全 | 否;启动、每帧流程与释放须在进程主线程串行调用 |
