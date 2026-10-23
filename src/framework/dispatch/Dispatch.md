# Dispatch 模块 (src\framework\dispatch\Dispatch.h)

## 概述

调度层:纯静态单例(全静态成员, 构造与析构均删除), 无实例 ⇒ 进程级单例语义。职责 = 事件绑定表注入、
会话分配器与场景装配、布局来源分派(缺省加载链 / 内存缓冲 / 布局模型 / 布局对象数组 / 延迟装配)、
布局槽状态(可见性与嵌套)、投影配色与宿主样式登记、每帧处理流程、无人值守参数、退出判据与清理链。

本模块属**调度层**(`src\framework\dispatch`), 依赖 `framework` 的公开接口, 不反向被框架层依赖。它不认识窗口句柄与
渲染设施: 建窗、UI 上下文、图形设施、消息循环与窗口过程一律归宿主实现(见 `framework\render` 下的宿主件); 本模块只经
`framework\render\Render.h` 的宿主接口表(`HostApi`)访问宿主, 并向宿主交出 `pfnFrame` 与退出判据地址。事件绑定表经函数指针
注入(不直接依赖演示层的事件表), 以保证绑定表内容属应用侧; 项目身份(文档名 / 环境变量名 / 日志前缀)取自 `DispatchConfig`。

布局以**槽**为单位持有: 每个槽 = 一枚布局对象 + 一份参考尺寸 + 可见性 + 嵌套状态。槽数组由会话分配器按需扩容,
初始化时由 `LayoutSource` 指定来源装配, 运行期可经 `AttachLayouts`(整体替换)与 `AddLayout`(逐枚追加)提交,
并经 `SetLayoutVisible` / `SetLayoutNested` 控制渲染方式。

场景装配的分配与释放(会话分配器 / 成员块 / 布局槽 / 文档解析 / 布局展开 / 几何解算 / 位置定稿 / 自证日志)收在同模块的
实现切片 `src\framework\dispatch\DispatchScene.cpp`(私有静态函数 `LoadScene` / `UnloadScene` 与布局槽辅助), 不对外暴露任何类。

本模块包含以下编程接口:

### 类型

| 类型 | 说明 |
| --- | --- |
| `BindingsFunc` | 事件绑定表取得函数指针类型: 启动期由 `Init` 调用一次, 取得"事件名 → 处理函数"表。 |
| `LayoutSourceKind` | 布局来源种类: 缺省加载链 / 内存缓冲 / 布局模型 / 布局对象数组 / 延迟装配。 |

### 结构

| 结构 | 说明 |
| --- | --- |
| `DispatchConfig` | 调度层配置(6 字段): 布局文档名 / 文档名环境变量 / 自动退出环境变量 / 日志前缀 / 标准窗口数 / 亮暗是否跟随系统。 |
| `LayoutSource` | 布局来源描述: 判别位与载荷; 一律经工厂函数构造(缺省 / 内存 / 模型 / 布局数组 / 延迟)。 |

### 类

| 类 | 说明 |
| --- | --- |
| `Dispatch` | 调度层: 初始化 / 亮暗覆盖 / 每帧流程 / 段前挂载点 / 释放 / 布局提交与槽状态 / 布局与参考尺寸访问 / 建面字号 / 窗口数量与矩形 / 退出判据地址 / 就绪查询。 |

### 常量

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `Dispatch::DISPATCH_SLOT_MAX` | `LAYOUT_MAX`(`1024`) | 单会话布局槽数量上限; 超出即拒绝提交, 不静默截断。 |

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

函数由 `Dispatch::Init` 在场景装配之前调用一次, 因此被调函数须已可用(不得延迟初始化)。返回 `NULL` 或条目数为 `0`
都按空表注入 ⇒ 所有 `event` 名解析为空绑定(控件无响应), 不报错、不中断启动。

布局在 `Init` 之前构建(布局对象数组来源)时, 调用方须自行先把同一张表交给 `UIEventHandler::SetBindings` ——
布局展开期即按 `event` 名解析处理器, 否则该次展开得到空绑定。

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
| `szDocName` | 缺省布局文档名(如 `"App.json"`); 按 exe 同目录解释, 构建期由 CMake 复制。缺省加载链与内存缓冲(回退链)来源必需; 布局模型 / 布局对象数组 / 延迟装配三种来源可传 `NULL`。 |
| `szDocEnv` | 覆盖文档名的环境变量名(如 `"APP_DOC"`); 空指针或取值为空 ⇒ 用 `szDocName`。 |
| `szAutoCloseEnv` | 自动退出毫秒数的环境变量名(如 `"APP_AUTOCLOSE_MS"`); 空指针 ⇒ 不设自动退出。 |
| `szLogPrefix` | 日志前缀(如 `"APP"`; 每行打印为 `[<前缀>] `)。 |
| `dwWndCount` | 标准窗口数量(由宿主按自身形态填写); 为 0 时按 1 兜底。 |
| `bDarkFollowSystem` | `TRUE` = 每帧跟随系统亮暗; `FALSE` = 用 `SetDarkMode` 的登记值。 |

### 注解

`szDocName` / `szDocEnv` / `szAutoCloseEnv` / `szLogPrefix` 指向的字符串由调用方持有且须进程级常驻(调度层只存
指针, 不复制 —— 场景切片按 `MAX_PATH` 内的缓冲使用它们)。`Identity.h` 的 `IDENTITY_LOG_PREFIX` 即经 `szLogPrefix`
交给调度层, 因此日志前缀可配置, 派生新项目不必改实现。配置在 `Init` 时被完整取用一次, 之后不再读取。

`dwWndCount` 同时是"模型来源最多装配几枚布局"的上界: 装配枚数 = min(模型布局数, `dwWndCount`)。

---

## LayoutSourceKind 枚举 (Dispatch)

布局来源种类: 说明初始化调度层时布局从何处来。

### 语法

```cpp
enum LayoutSourceKind
{
    LAYOUT_SOURCE_DEFAULT = 0,
    LAYOUT_SOURCE_MEMORY,
    LAYOUT_SOURCE_MODEL,
    LAYOUT_SOURCE_LAYOUTS,
    LAYOUT_SOURCE_DEFERRED
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `LAYOUT_SOURCE_DEFAULT` | 缺省加载链: 同名 `.bin` 优先, 其次文档名本身; 形态由解析器按内容嗅探。 |
| `LAYOUT_SOURCE_MEMORY` | 内存缓冲: 调用方给出的 JSON / bin 字节; 解析失败回退缺省加载链(不视为初始化失败)。 |
| `LAYOUT_SOURCE_MODEL` | 布局模型: 调用方持有的中间态(`FileMidState`); 直接展开, 不再解析文档。 |
| `LAYOUT_SOURCE_LAYOUTS` | 布局对象数组: 调用方已展开并解算好的 `Layout` 对象; 直接使用。 |
| `LAYOUT_SOURCE_DEFERRED` | 延迟装配: 初始化不提交真实布局; 业务代码在 `Init` 之后逐枚提交。 |

### 注解

`LAYOUT_SOURCE_DEFAULT` / `LAYOUT_SOURCE_MEMORY` / `LAYOUT_SOURCE_MODEL` 三支的产出一致 —— 装配后逐枚展开布局对象、
解算像素几何、按父链重推坐标并定稿窗口位置。`LAYOUT_SOURCE_LAYOUTS` 与 `LAYOUT_SOURCE_DEFERRED` 不触达解析器与几何解算:
前者直接接管调用方给出的布局对象, 后者只建立会话基础设施。

---

## LayoutSource 结构 (Dispatch)

布局来源描述:`Init` 的第四段入参; 判别位与载荷由工厂函数成对写入, 调用方无需手填字段。

### 语法

```cpp
struct LayoutSource
{
    LayoutSourceKind   kind;
    LPCVOID            pMemData;
    DWORD              cbMemSize;
    CONST FileMidState* pModel;
    Layout* CONST*     ppLayouts;
    CONST AutoSize*    pRefs;
    DWORD              dwLayoutCount;

    static LayoutSource CreateDefault();
    static LayoutSource CreateMemory(LPCVOID pData, DWORD cbSize);
    static LayoutSource CreateModel(CONST FileMidState* pModel);
    static LayoutSource CreateLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount);
    static LayoutSource CreateDeferred();
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `kind` | 来源种类; 由工厂函数写入。 |
| `pMemData` | 内存缓冲首址(仅 `LAYOUT_SOURCE_MEMORY`)。 |
| `cbMemSize` | 内存缓冲字节数(仅 `LAYOUT_SOURCE_MEMORY`); 合法域 = `1` 到 8 MiB。 |
| `pModel` | 布局模型(仅 `LAYOUT_SOURCE_MODEL`); 指向整份中间态。 |
| `ppLayouts` | 布局对象数组(仅 `LAYOUT_SOURCE_LAYOUTS`)。 |
| `pRefs` | 参考尺寸数组(仅 `LAYOUT_SOURCE_LAYOUTS`); 与布局数组同长。 |
| `dwLayoutCount` | 数组长度(仅 `LAYOUT_SOURCE_LAYOUTS`); 合法域 = `1` 到 `DISPATCH_SLOT_MAX`。 |

### 注解

**所有权与生命周期**: 内存缓冲只在 `Init` 期被读取(JSON 形态内部复制一份并补结尾符, 原缓冲不改写); 布局模型与布局对象
数组按**借用**处理 —— 调度层不复制、不释放。模型内的布局标签(`ElementSpec::label`)会被展开态的控件直接引用 ⇒ 模型须存活到
`Dispatch::Release` 之后; 布局对象数组的元素由调用方持有, `Release` 只清指针, 不析构、不回收控件缓冲。

**布局对象数组的前置条件**: 元素须已就绪 —— 结构件(`WND` / `TITLE` / `CNTR`)落在前三个槽位、像素几何已解算、层次与坐标
已推导、事件已按绑定表解析(绑定表须在布局展开之前注入)。调度层不再解析文档, 也不重算几何; 结构件缺失或窗口件尺寸为零的
元素在渲染期被静默跳过(装配期打印一次"布局校验"行作诊断)。

**延迟装配的时序**: `Init` 只建立会话分配器、成员块、宿主注入与样式登记。此后的装配须在宿主 `Start` 之前完成, 否则建面字号
与窗口数量已按缺省值登记; 运行期替换(再次 `AttachLayouts`)不改变已登记的建面字号。

**会话分配器复用**: 若调用方在 `Init` 之前已建立会话分配器(`AllocatorHub::Init`), `Init` 复用而不重建 —— 这是"布局对象
数组来源在 `Init` 之前构建"的前提; 重建会令先前构建的布局与其控件缓冲整体悬空。会话分配器统一由 `Dispatch::Release` 释放。

**DPI 感知**: 参考尺寸按屏幕像素推导 ⇒ 预建布局前须已打开进程 DPI 感知(`HostApi::EnableDpiAwareness`, 幂等), 否则参考量
按系统虚拟化的屏幕尺寸推导(缩放比例不为 1 时整体偏小)。

---

## Dispatch 类 (Dispatch)

调度层:集中持有调度状态(成员块 `DispatchData`)并编排启动、每帧与释放三个阶段。

### 语法

```cpp
class Dispatch
{
public:
    using BindingsFunc = CONST UIEventHandler::EventBinding* (*)(size_t* pnCount);
    static constexpr DWORD DISPATCH_SLOT_MAX = LAYOUT_MAX;

    static BOOL Init(CONST HostApi* pHost, CONST DispatchConfig& cfg, BindingsFunc pfnBindings,
        CONST LayoutSource& source = LayoutSource::CreateDefault());
    static BOOL AttachLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount);
    static BOOL AddLayout(Layout* pLayout, CONST AutoSize& ref);
    static BOOL SetLayoutVisible(DWORD index, BOOL bVisible);
    static BOOL SetLayoutNested(DWORD index, BOOL bNested);
    static BOOL GetLayoutState(DWORD index, BOOL* pbVisible, BOOL* pbNested);
    static DWORD GetLayoutCount();
    static Layout& GetLayoutAt(DWORD index);
    static CONST AutoSize& GetRefAt(DWORD index);
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
| `Init(pHost, cfg, pfnBindings, source)` | 初始化调度层: 控制台 UTF-8 → DPI 感知 → 绑定注入 → 场景装配(按来源) → 宿主注入与样式登记 → 无人值守参数。`source` 省略即缺省加载链。 |
| `AttachLayouts(ppLayouts, pRefs, dwCount)` | 整体替换布局(清空现有槽后逐枚接管); 首枚可见, 其余先隐藏; 不取得所有权。 |
| `AddLayout(pLayout, ref)` | 追加一枚布局(逐枚提交); 新槽默认可见且按基座渲染; 不取得所有权。 |
| `SetLayoutVisible(index, bVisible)` | 设置指定槽的可见性; 不可见的布局不参与本帧渲染。 |
| `SetLayoutNested(index, bNested)` | 设置指定槽的嵌套状态; `TRUE` = 按 Flex 渲染(沿用前一枚基座布局的窗口段)。 |
| `GetLayoutState(index, pbVisible, pbNested)` | 取指定槽的可见性与嵌套状态(输出参数可传 `NULL`)。 |
| `GetLayoutCount()` | 取已装配的布局数量(延迟装配且未提交时为 `0`)。 |
| `GetLayoutAt(index)` | 取指定槽的布局对象; 越界或空槽返回空对象。 |
| `GetRefAt(index)` | 取指定槽的参考尺寸; 越界返回缺省参考量。 |
| `SetDarkMode(bDark)` | 覆盖"跟随系统"的亮暗口径(调用后本进程不再跟随系统; 宿主已就绪窗口立即刷新)。 |
| `Frame(pUser)` | 每帧处理流程: 数据层更新 → 布局渲染清单(按槽顺序) → 可见性缓存 → IME 覆盖 → 渲染自证 → 无人值守退出判据。 |
| `OnSegment(index, pUser)` | 段前回调范例挂载点; 当前为空实现(本形态无合成输入注入需求)。 |
| `Release()` | 释放调度资源: 收尾自证 → 投影配色 → 布局槽与场景 → 成员块 → 会话分配器。幂等。 |
| `GetLayout()` | 取首枚布局对象(渲染源); 未就绪时返回空对象。 |
| `GetRef()` | 取首枚参考尺寸; 未就绪时返回缺省参考量。 |
| `GetFontSize()` | 取建面字号: 首枚布局参考字号(像素), 无效时按缺省字号 `18` 兜底。 |
| `GetWndCount()` | 取需要创建的标准窗口数量(配置值, 下限 `1`)。 |
| `GetWndRect(x, y, w, h)` | 取主布局窗口矩形: 桌面位置取自首枚布局的窗口件位置记录, 客户区尺寸取装配期推导值。 |
| `GetExitAddress()` | 取退出判据地址; 未就绪时返回 `NULL`。 |
| `IsReady()` | 调度层是否就绪(成员块已分配且至少一枚布局已装配)。 |

### 注解

**日志**: 全部输出统一经内部静态 `Log(fmt, ...)` 包装(前缀 `[<cfg.szLogPrefix>] `, 逐行刷新); 调度层不直接
调 `printf`, 因此前缀可配置而日志行结构不变。装配期依次打印"布局来源"、"场景载入 / 布局校验"、"事件绑定"、
"控件层次"与"控件坐标"; 运行期提交打印"布局提交"。

**成员块**: 调度状态集中在一个私有 `DispatchData` 结构里, 由会话分配器的线性分配器以 placement new 分配
(在 `AllocatorHub::Init` 之后), `Release` 时显式析构并置空。成员块包含布局槽数组(容量 / 数量)、解析器指针、
参考尺寸与经验参数、窗口矩形、投影配色指针、宿主接口表指针、就绪/退出标志、亮暗口径、帧计数、自动退出毫秒数、
启动 tick、最近一帧窗口显示缓存、布局文档名缓冲与环境变量名缓冲。调度层未就绪时成员块为空, 全部访问接口回退到
空对象 / 缺省值 / `NULL`。

**`Init` 的次序不可交换**: DPI 感知须早于场景装配(场景几何按屏幕像素推导, 未感知时系统会按缩放虚拟化屏幕
尺寸); 事件绑定注入须早于场景装配(展开布局时即按 `event` 名解析处理器); 投影配色与样式登记须早于宿主建 UI 面
(初始化期即应用一次宿主样式)。成功时依次打印事件绑定表、布局来源、场景载入、事件绑定、控件层次、控件坐标、
字号与就绪日志。

**入口校验**: `pHost` 为空、`pfnBindings` 为空或 `dwWndCount` 为 `0` ⇒ 直接失败; 文档名为空只在"缺省加载链"
与"内存缓冲"两种来源下视为失败(其余来源不需要文档)。

**布局来源分支**:

| 来源 | 装配行为 | 失败口径 |
| --- | --- | --- |
| 缺省加载链 | 解析文档(bin 优先) → 展开前 `min(布局数, 窗口数)` 枚 → 几何解算 → 坐标推导 → 位置定稿 | 目录解析失败或文档解析失败 ⇒ `Init` 失败 |
| 内存缓冲 | 复制进会话分配器并补结尾符 → 嗅探形态 → 解析 | 缓冲为空 / 超上限 / 解析失败 ⇒ 回退缺省加载链(打印一行回退日志) |
| 布局模型 | 直接取自调用方的 `FileMidState`, 逐枚展开与解算 | 模型为空或无可用布局 ⇒ `Init` 失败 |
| 布局对象数组 | 直接接管; 首枚可见, 其余隐藏 | 数组为空 / 超上限 / 元素为空或零控件 ⇒ `Init` 失败 |
| 延迟装配 | 不装配; 只建立会话基础设施 | 不适用(装配失败由 `AttachLayouts` / `AddLayout` 逐次上报) |

**字号**: 建面字号 = 首枚布局参考字号经换算基准推导的像素值(与控件几何同源), 由 `GetFontSize` 只读投影;
进程入口在建窗之前把它交给具体宿主登记。参考字号无效(小于 `12`)时按缺省 `18` 兜底。延迟装配须在 `Start` 之前
完成首次提交, 否则宿主按缺省字号建面。

**每帧流程**: `Frame` 只在首帧打印一次建窗几何; 随后每帧确保投影配色跟随当前亮暗口径、按槽顺序渲染布局、
刷新窗口可见性缓存、按控件库报告设置 IME 覆盖, 并在首帧与每 60 帧打印一次渲染自证。渲染清单的规则:
按槽位顺序遍历, 不可见槽跳过; 基座槽(`bNested == FALSE`)经 `LayoutRender::RenderBase` 独占一个标准窗口并开启
新的窗口段; 嵌套槽(`bNested == TRUE`)经 `LayoutRender::RenderFlex` 沿用**最近一次成功的基座槽**已开启的窗口段,
排在首个基座槽之前的嵌套槽在本帧被跳过。无人值守到点时置退出判据, 由宿主主循环在下一帧开头读取并退出。

**布局提交的所有权**: `AttachLayouts` 与 `AddLayout` 不取得布局对象所有权 —— `Release` 只清槽指针。若布局对象由
会话分配器分配(可在 `Init` 之后经 `AllocatorHub::GetArena` 构建), 其内存随 `Release` 整体失效; 若由调用方自有
分配器构建, 调用方负责其析构与控件缓冲回收(布局对象内保存的标签指针指向构建期的内存, 回收次序应与源数据一致)。

**收尾自证**: 主循环退出后, `Release` 在场景释放之前打印一次 `渲染自证(收尾)`。窗口可见状态取最近一帧的
缓存, 因此即使进程入口已先调用宿主的 `DestroyWnds`(窗口显示状态已复位), 收尾行仍报告运行期的真实
可见性; 绘制量计数由宿主跨释放保留。

**退出判据**: 成员块内的 `BOOL bExit` 是唯一判据, `GetExitAddress` 返回其地址, 宿主主循环每帧只读。`ESC`、
系统关闭与无人值守到点三条路径最终都使其生效(前两者经窗口过程的 `WM_QUIT`, 后者由 `Frame` 置位)。

**线程与所有权**: 全部接口须在进程主线程串行调用。`GetLayout` / `GetRef` / `GetLayoutAt` / `GetRefAt` 返回的
引用在 `Release` 之后失效; `GetExitAddress` 返回的地址在 `Release` 之后失效。`OnSegment` 只作为宿主 `Start` 的段前
回调挂载点, 在其中不得开帧、结算段或调用 `Release`。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/dispatch/Dispatch.h`(`src\framework\dispatch\Dispatch.h`) |
| 实现 | `src\framework\dispatch\Dispatch.cpp`;场景装配切片 = `src\framework\dispatch\DispatchScene.cpp` |
| 命名空间 | 无(全局类 `Dispatch`) |
| 依赖 | `framework/base/BasicDesign.h` · `framework/base/AppEnv.h` · `framework/base/mem/AllocatorHub.h` · `framework/dispatch/ResourceLoader.h` · `framework/dispatch/UIEventHandler.h` · `framework/layout/Layout.h` · `framework/layout/LayoutModel.h` · `framework/layout/LayoutSolver.h` · `framework/layout/LayoutBuilder.h` · `framework/layout/LayoutRender.h` · `framework/layout/GeometrySolver.h` · `framework/base/ControlSpec.h` · `framework/render/Render.h` · `user32.dll` · `dwmapi.dll` |
| 平台 | Windows 全版本(控制台子系统; 启动首件事为控制台代码页切 UTF-8) |
| 线程安全 | 否;启动、每帧流程与释放须在进程主线程串行调用 |
