# ImGuiHost 模块 (src\framework\render\imgui\ImGuiHost.h)

## 概述

UI 宿主:纯静态类(构造、析构、拷贝均删除), 无实例 ⇒ 进程级单例语义。职责三块:
管理本应用声明的 1 到 8 个"标准窗口"(注册一个共用窗口类、建窗、圆角、成对销毁), 并独持整条渲染链(主消息循环、
窗口过程、每窗口一套 D3D11 设施与 ImGui 面、逐段渲染与呈现、遮挡判定、IME 开关、可见性策略),
以及在初始化期把 ImGui 侧的渲染能力表注入布局渲染出口。推荐入口为 `Start`:调用方只给"窗口数量 + 应用名",
默认几何(客户区 800x600、屏幕工作区居中、圆角 10)与已登记字号(未登记时 18)、窗口类名(`<APPNAME>_IMGUI_CLASS`)、
窗口标题(默认应用名)由本模块派生; 需要逐窗口定制时才自行填 `WndSpec` 表调 `CreateWnds`。

本模块属 **framework 层**(UI 宿主绑定层), 依赖 `framework/render/imgui/D3D11Manager`(每窗口的 D3D11 设施,
同目录)、`framework/render/imgui/ability/ImGuiRender`(被注入的能力表来源)、`framework/layout/LayoutRender`(注入点)、
`framework/base/AppEnv`(窗口圆角); 不认识布局、不做数据层、不读项目身份, 也不引 `app/`、`editor/`、`doc/` 头。
设备由本模块创建并释放 ⇒ 外部任何层面都不必持有 D3D 句柄; 窗口句柄只在模块内部使用, 外部经
`GetCurrentWnd` 按需取用而不持久持有。窗口过程由本模块内置(销毁窗口与 `ESC` 请求退出, 其余交系统默认过程),
不对外暴露、也不由调用方传入。

本模块包含以下编程接口:

### 类型

| 类型 | 说明 |
| --- | --- |
| `WndSpec` | 一个标准窗口的声明规格: 类名、标题、几何、字号、圆角半径(高级用法; 推荐用 `Start`)。 |
| `WndShowState` | 窗口显示状态(框架自持的可见性真相), 四态。 |
| `StyleFunc` | 样式槽填充回调: 在亮暗基础样式之后被调用。 |
| `FrameFunc` | 外部处理流程回调: 每帧被调用一次。 |
| `SegmentFunc` | 段前回调: 在目标窗口后端开帧之后、ImGui 帧开始之前被调用。 |

### 函数

| 函数 | 说明 |
| --- | --- |
| `EnableDpiAwareness` | 打开进程级 DPI 感知(启动首件事; 幂等)。 |
| `Start` | 启动渲染宿主: DPI 感知、默认几何构造、声明数量、建窗、建立渲染设施; 失败内部回退。 |
| `SetStandardWndCount` | 声明需要创建的窗口数量(超过上限按上限夹取)。 |
| `GetStandardWndCount` | 取已声明的窗口数量。 |
| `GetWndCount` | 取已声明的窗口数量(渲染侧按该数量顺序取窗口)。 |
| `SetActiveWndCount` | 声明布局激活所需的窗口数并同步各窗口可见性。 |
| `GetActiveWndCount` | 取当前的激活窗口数。 |
| `GetWndShowState` | 取指定窗口的显示状态; 序号越界返回 `WND_SHOW_NEVER`。 |
| `SetStyleFunc` | 注入样式槽填充回调(传 `NULL` 即清除注入)。 |
| `SetSegmentFunc` | 注入段前回调(在目标窗口后端开帧之后、ImGui 帧开始之前调用)。 |
| `SetFontSize` | 登记标准窗口的字号(像素; `Start` 建窗时消费; 无效值按缺省字号)。 |
| `SetDarkMode` | 设置亮暗基础样式(入参归一化为真/假)。 |
| `GetDarkMode` | 取当前亮暗基础样式。 |
| `RefreshStyle` | 把当前样式重新应用到全部已就绪窗口。 |
| `CreateWnds` | 按声明数量注册共用窗口类、建窗并施加圆角; 任一失败则全部回退。 |
| `Acquire` | 建立全部窗口的 D3D11 设施与 ImGui 面, 并注入渲染能力表; 失败则全部回退。 |
| `Run` | 主消息循环: 每帧调一次外部处理流程, 退出判据由外部持有。 |
| `AcquireNextWnd` | 结算上一段、取下一个渲染目标窗口、按期望几何与标题结算该窗口并开启新段。 |
| `GetCurrentWnd` | 取当前渲染段的窗口句柄(无段时 `NULL`)。 |
| `GetWndPos` | 取当前段窗口的桌面位置(真实窗口矩形)。 |
| `ResetWndCursor` | 把本帧的渲染目标取用游标归零(供宿主自定义渲染循环重走渲染清单)。 |
| `FinalizeFrame` | 结算当前已开启的渲染段(提交绘制、应用 IME、呈现并清理后台缓冲)。 |
| `SetIMEDisabled` | 设置本帧的 IME 覆盖(段结算时应用, 结算后失效)。 |
| `GetRenderStats` | 取绘制量自证计数: 段数、绘制命令数、顶点数、索引数。 |
| `GetResizeStats` | 取窗口尺寸调整自证计数与最近一次调整后的客户区尺寸。 |
| `Release` | 释放全部窗口的 ImGui 面与 D3D11 设施; 幂等且不动窗口。 |
| `DestroyWnds` | 销毁全部标准窗口并注销窗口类; 幂等。 |
| `GetWindow` | 取指定窗口的窗口句柄; 序号越界返回 `NULL`。 |
| `GetContext` | 取指定窗口的 ImGui 上下文; 序号越界或未就绪返回 `NULL`。 |
| `IsReady` | 查询指定窗口的 ImGui 面是否已就绪。 |

### 常量

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `MAX_WND` | `8` | 标准窗口数量上限, 同时是模块内窗口单元表的容量。 |

文件内私有件不立条: 窗口单元表与全部模块级状态(声明数、激活数、共用窗口类原子、样式回调及其用户指针、
已登记建面字号、亮暗标志、段状态、渲染目标游标、IME 覆盖、四项绘制量计数与三项尺寸调整计数)均为文件内静态;
句柄反查、基础样式应用、中文字体加载、单窗口 ImGui 面的建立与关停、段开启与结算、段状态复位、
可见性发起与激活数策略、共用窗口过程均为文件内 `static` 函数。

---

## WndSpec 结构 (ImGuiHost)

一个标准窗口的声明规格, 由调用方逐项填写后整表交给 `CreateWnds`(推荐直接用 `Start` 生成默认表)。

### 语法

```cpp
struct WndSpec
{
    LPCWSTR lpszClassName;
    LPCWSTR lpszTitle;
    INT     x;
    INT     y;
    DWORD   dwWidth;
    DWORD   dwHeight;
    FLOAT   fFontSize;
    DWORD   dwRounding;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `lpszClassName` | 窗口类名, 由本模块注册; 进程内须唯一。 |
| `lpszTitle` | 窗口标题, 仅任务栏与窗口枚举可见。 |
| `x` / `y` | 建窗时的桌面位置(像素)。 |
| `dwWidth` / `dwHeight` | 建窗时的客户区尺寸(像素)。 |
| `fFontSize` | 本窗口字号(像素); 小于 `8` 时按 `18` 兜底。 |
| `dwRounding` | 本窗口圆角半径(像素); `0` = 不设圆角。 |

### 注解

`lpszClassName` 同名类会与同机其它程序冲突, 因此调用方须给出进程内唯一的类名; 类由本模块注册,
销毁时由 `DestroyWnds` 注销, 调用方不自行注册或注销。所有标准窗口共用**一个**窗口类(取规格表首项的
`lpszClassName`), 本模块只注册一次、只注销一次。

推荐入口是 `Start`:它按"数量 + 应用名"构造整张规格表 —— 类名 = `<APPNAME>_IMGUI_CLASS`, 标题 = 应用名,
客户区 800x600、屏幕工作区居中、字号取已登记值(见 `SetFontSize`; 未登记时 18)、圆角 10。只有需要逐窗口定制
几何 / 字号 / 圆角时才自行填本结构并调 `CreateWnds`(高级用法)。

建窗样式为 `WS_POPUP` 且不含 `WS_VISIBLE`, 无系统标题栏 ⇒ `dwWidth` / `dwHeight` 即客户区几何,
外框与客户区一致; `x` / `y` 只是建窗初值, 运行期的窗口位置与标题由 `AcquireNextWnd` 按布局记录结算。

`fFontSize` 由 `Acquire` 建 ImGui 面时消费, 小于 `8` 的值按 `18` 兜底(与字号域下限一致); `Start` 生成的
默认表取 `SetFontSize` 的登记值(未登记时 `18`)。

`dwRounding` 大于 `0` 时建窗即施加, 尺寸变化后由窗口过程按新尺寸重设。

---

## WndShowState 枚举 (ImGuiHost)

框架自持的窗口显示状态: 可见性判定的唯一真相, 不查系统可见性。

### 语法

```cpp
enum WndShowState
{
    WND_SHOW_NEVER = 0,
    WND_SHOW_VISIBLE,
    WND_SHOW_HIDDEN_BY_ACTIVE,
    WND_SHOW_HIDDEN_BY_HOST,
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `WND_SHOW_NEVER` | 建窗后从未显示过(首窗初值)。 |
| `WND_SHOW_VISIBLE` | 当前可见。 |
| `WND_SHOW_HIDDEN_BY_ACTIVE` | 当前不可见, 隐藏由激活数策略发起; 本模块可自行恢复。 |
| `WND_SHOW_HIDDEN_BY_HOST` | 当前不可见, 隐藏由宿主或系统发起; 本模块不擅自恢复。 |

### 注解

写点唯一: 窗口过程的 `WM_SHOWWINDOW` 分支。本模块自身发起的显示与隐藏同样经该分支记账, 发起时置
"本次变更由本模块发起"标记, 据此把隐藏归入可恢复的一类(`WND_SHOW_HIDDEN_BY_ACTIVE`);
其余来源的隐藏一律记为 `WND_SHOW_HIDDEN_BY_HOST`, 此后本模块不再对该窗口发起显示。

`SetActiveWndCount` 的可见性策略只认本枚举, 因此宿主直接 `ShowWindow` 造成的可见性变化同样被感知,
不会被下一次策略应用覆盖掉宿主意图。

---

## StyleFunc 类型 (ImGuiHost)

样式槽填充回调类型: 供宿主把项目口径的配色与圆角写进 ImGui 样式。

### 语法

```cpp
using StyleFunc = VOID (*)(ImGuiStyle& style, VOID* pUser);
```

### 参数

`[in, out] style`

类型: **ImGuiStyle&**

当前窗口的 ImGui 样式, 回调可就地改写其中的任意槽位。

`[in] pUser`

类型: **VOID\***

注册时经 `SetStyleFunc` 一并交付的用户指针, 原样回传。可为 `NULL`。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

调用时机 = 亮暗基础样式设置之后(`SetDarkMode` 的效果先落地), 因此回调只需覆盖需要项目口径的槽位,
不必重置整套样式。回调在 `Acquire` 建每个窗口的 ImGui 面时各调用一次, 运行期调整后由 `RefreshStyle`
对全部已就绪窗口重放。

回调运行在本模块的渲染线程内(即主循环线程), 不得在其中调用 `Run`、`Release`、`DestroyWnds`。

---

## FrameFunc 类型 (ImGuiHost)

外部处理流程回调类型: 主循环每帧调用一次, 全部数据层更新与渲染清单驱动都在其中完成。

### 语法

```cpp
using FrameFunc = VOID(*)(VOID* pUser);
```

### 参数

`[in] pUser`

类型: **VOID\***

注册时经 `Run` 一并交付的用户指针, 原样回传。可为 `NULL`。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

回调内须自行按渲染清单顺序驱动布局渲染出口; 每次"取渲染目标"都落到 `AcquireNextWnd`。本帧结束时
`Run` 会兜底结算未结算的段, 因此回调中途返回或中途失败不会留下未结束的 ImGui 帧。

回调内可调用本模块除 `Run`、`Release`、`DestroyWnds` 以外的全部接口(窗口级事件就在回调链内产生)。
回调运行在主循环线程, 不得跨线程并发进入。

---

## SegmentFunc 类型 (ImGuiHost)

段前回调: 在目标窗口后端开帧之后、ImGui 帧开始之前被调用。

### 语法

```cpp
using SegmentFunc = VOID(*)(DWORD index, VOID* pUser);
```

### 参数

`[in] index`

类型: **DWORD**

即将开帧的标准窗口序号(标准窗口表下标)。

`[in] pUser`

类型: **VOID\***

注入时给出的用户指针(见 `SetSegmentFunc`)。

### 返回值

类型: **VOID**

回调不返回值。

### 注解

典型用途 = 在帧开始前注入合成输入事件(后端已写完真实光标位置, 此处写入的事件才会进入本帧输入队列)。
回调不参与段是否开启的判定; 回调内不得再开帧或结算段。

---

## EnableDpiAwareness 函数 (ImGuiHost)

打开进程级 DPI 感知。

### 语法

```cpp
static VOID EnableDpiAwareness();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

调用时机 = 进程启动后第一件事, 须早于任何窗口创建与任何 ImGui 初始化, 否则首次取到的 DPI 与
客户区尺寸口径会与后续不一致。幂等: 重复调用无副作用。

本调用只影响本进程, 不改系统设置。

---

## Start 函数 (ImGuiHost)

启动渲染宿主:依次完成 DPI 感知、默认几何构造、窗口数量声明、建窗与渲染设施建立。

### 语法

```cpp
static BOOL Start(
  [in] DWORD       dwWndCount,
  [in] LPCSTR      lpszAppName,
  [in] SegmentFunc pfnSegment,
  [in] VOID*       pUser
);
```

### 参数

`[in] dwWndCount`

类型: **DWORD**

需要创建的标准窗口数量。合法域 = `1` 到 `MAX_WND`(8); 超出范围或为 `0` 时启动失败。

`[in] lpszAppName`

类型: **LPCSTR**

应用名(UTF-8)。用于派生窗口类名(`<APPNAME>_IMGUI_CLASS`)与默认窗口标题; 为空或 `NULL` 时启动失败。

`[in] pfnSegment`

类型: **SegmentFunc**

段前回调(见 `SetSegmentFunc`), 可为 `NULL`。本函数把它与 `pUser` 一并注入, 等价于启动前调用一次
`SetSegmentFunc`。

`[in] pUser`

类型: **VOID\***

原样回传给段前回调的用户指针, 可为 `NULL`。

### 返回值

类型: **BOOL**

`TRUE` = 全部窗口已建成且渲染设施已就绪; `FALSE` = 参数非法、窗口类注册或建窗失败、或渲染设施建立失败。
失败时本函数内部已销毁本次建成的窗口、关闭已建立的渲染设施并清除段前回调注入。

### 注解

**推荐入口**:调用方只需给出"窗口数量 + 应用名", 默认几何与窗口身份由本模块派生 ——
客户区 `800x600`、主显示器工作区居中、圆角 `10`、字号取 `SetFontSize` 的登记值(未登记时 `18`);
窗口类名 = `<APPNAME>_IMGUI_CLASS`(全部窗口共用一个类, 只注册一次), 窗口标题默认 = 应用名。
需要逐窗口定制几何时才改为自行构造 `WndSpec` 表并调 `CreateWnds` + `Acquire`(高级用法)。

**首步 = `EnableDpiAwareness`**:DPI 感知须早于任何窗口创建与 ImGui 初始化; 若场景几何在更早阶段已按屏幕
像素推导, 调用方应自行提前调用一次 `EnableDpiAwareness`(幂等)。

**内部次序**:`EnableDpiAwareness` → 应用名转宽字符并拼类名 → 构造默认规格表(字号取已登记值) →
`SetStandardWndCount` → `SetSegmentFunc` → `CreateWnds` → `Acquire`。成功即完成首次显示
(见 `Acquire` 的可见性策略), 之后可直接 `Run`。

**建窗样式**:`WS_POPUP` 且不含 `WS_VISIBLE` ⇒ 建窗时窗口不可见, 首次显示在 `Acquire` 末尾统一发起;
客户区尺寸即规格值, 运行期首帧由 `AcquireNextWnd` 按布局记录调整位置、尺寸与标题。

---

## SetStandardWndCount 函数 (ImGuiHost)

声明需要创建的标准窗口数量。

### 语法

```cpp
static VOID SetStandardWndCount(
  [in] DWORD dwCount
);
```

### 参数

`[in] dwCount`

类型: **DWORD**

期望的窗口数量。合法域 = `0` 到 `MAX_WND`(8); 大于上限的值按上限夹取后写入, 不报错。
`0` 是合法声明, 但此后的 `CreateWnds` 与 `Acquire` 都会失败返回。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

调用时机须早于 `CreateWnds`(建窗按本值逐个进行)。本值不会在失败路径上被清零: `CreateWnds` 或
`Acquire` 失败回退后, 声明数量保持原值, 便于按原声明重试; `DestroyWnds` 也不清声明数量。

激活窗口数在 `CreateWnds` 成功时被置为与本值相同; 若声明数下调到低于当前激活数, 须自行再调用
`SetActiveWndCount` 收紧, 否则激活数保持原值而实际窗口数已减。

---

## GetStandardWndCount 函数 (ImGuiHost)

取已声明的标准窗口数量。

### 语法

```cpp
static DWORD GetStandardWndCount();
```

### 参数

此函数无参数。

### 返回值

类型: **DWORD**

当前声明数量(`0` 到 `8`)。未声明时为 `0`。

### 注解

只读本模块的声明状态, 不反映实际建成的窗口数; 建窗失败时声明数量仍保留原值。

---

## GetWndCount 函数 (ImGuiHost)

取已声明的标准窗口数量(渲染侧按该数量顺序取窗口)。

### 语法

```cpp
static DWORD GetWndCount();
```

### 参数

此函数无参数。

### 返回值

类型: **DWORD**

当前声明数量, 与 `GetStandardWndCount` 同值。

### 注解

两枚取数函数的语义区别只在调用方语境: 本函数供渲染侧判断"本帧最多能取到几个窗口",
`GetStandardWndCount` 供装配期核对声明。二者读同一份状态, 不会出现不同值。

---

## SetActiveWndCount 函数 (ImGuiHost)

声明布局激活所需的窗口数, 并立即按该数同步各窗口可见性。

### 语法

```cpp
static VOID SetActiveWndCount(
  [in] DWORD dwActive
);
```

### 参数

`[in] dwActive`

类型: **DWORD**

布局激活所需的窗口数。大于当前声明数量时按声明数量夹取(不报错); `0` 表示全部标准窗口均不可见。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

可见性判定只取本模块自持的显示状态, 不查系统可见性:

- 序号小于 `dwActive` 的窗口保证可见。处于 `WND_SHOW_NEVER`(首次显示)或
  `WND_SHOW_HIDDEN_BY_ACTIVE`(由本策略隐藏过)的一组由本模块发起显示;
  `WND_SHOW_VISIBLE` 与 `WND_SHOW_HIDDEN_BY_HOST` 一律不动(不抢宿主的显示与隐藏控制)。
- 序号不小于 `dwActive` 的窗口在非"已隐藏"状态下由本模块发起隐藏。宿主若重新显示, 下一次应用会再次隐藏。

`Acquire` 末尾会按当前激活数应用一次本策略, 因此"首次显示"归本模块而非宿主: 建窗样式不含
`WS_VISIBLE`, 窗口只有在 ImGui 面就绪之后才首次出现, 不会出现"窗口先出现而内容未就绪"的空窗。
该顺序也保证了"声明多少激活窗口就有多少窗口可见"与宿主调用本函数与 `Acquire` 的先后无关。

被隐藏的窗口仍可被开帧与渲染: 渲染几段完全由外部处理流程的清单决定, 与可见性无关。

---

## GetActiveWndCount 函数 (ImGuiHost)

取当前的激活窗口数。

### 语法

```cpp
static DWORD GetActiveWndCount();
```

### 参数

此函数无参数。

### 返回值

类型: **DWORD**

当前激活窗口数。`CreateWnds` 成功时被置为声明数量, `DestroyWnds` 后归 `0`。

### 注解

本值只是"布局激活所需的窗口数"这一声明量, 不等于当前可见窗口数(宿主可能另行隐藏了某个窗口)。

---

## GetWndShowState 函数 (ImGuiHost)

取指定窗口的显示状态。

### 语法

```cpp
static WndShowState GetWndShowState(
  [in] DWORD index
);
```

### 参数

`[in] index`

类型: **DWORD**

标准窗口序号(从 `0` 起)。合法域 = `0` 到 `MAX_WND - 1`; 越界时直接返回 `WND_SHOW_NEVER`, 不读单元表。

### 返回值

类型: **WndShowState**

该窗口的显示状态。未建窗的序号返回 `WND_SHOW_NEVER`。

### 注解

返回值取自模块内部单元表的记录, 与系统当前可见性可能不一致(系统外的变更尚未经 `WM_SHOWWINDOW` 抵达时)。
宿主据此判断"本模块是否在该窗口上还有可见性主张", 不要用它替代 `IsWindowVisible`。

---

## SetStyleFunc 函数 (ImGuiHost)

注入样式槽填充回调。

### 语法

```cpp
static VOID SetStyleFunc(
  [in] StyleFunc pfnStyle,
  [in] VOID*     pUser
);
```

### 参数

`[in] pfnStyle`

类型: **StyleFunc**

样式回调; 传 `NULL` 即清除注入(此后只应用亮暗基础样式)。

`[in] pUser`

类型: **VOID\***

原样回传给回调的用户指针, 可为 `NULL`。清除注入时该指针一并被置空。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

初始化路径: 在 `Acquire` 之前调用即随每窗口的 ImGui 面建立生效一次。运行期再次调用只改注入值,
已就绪的窗口须再调 `RefreshStyle` 才会看到新样式。

叠加次序 = 亮暗基础样式(`SetDarkMode`)→ 宿主回调; 因此宿主回调只应覆盖需要项目口径的槽位。
回调与其用户指针的所有权归调用方: 本模块只存指针, 不复制也不释放, 指针须在两次调用之间保持有效。

---

## SetSegmentFunc 函数 (ImGuiHost)

注入段前回调。

### 语法

```cpp
static VOID SetSegmentFunc(
  [in] SegmentFunc pfnSegment,
  [in] VOID*       pUser
);
```

### 参数

`[in] pfnSegment`

类型: **SegmentFunc**

段前回调; 传 `NULL` 即清除注入。

`[in] pUser`

类型: **VOID\***

原样回传给回调的用户指针, 可为 `NULL`。清除注入时该指针一并被置空。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

调用时机 = 目标窗口的两端后端已开帧、`ImGui::NewFrame` 尚未调用; 入参含该窗口在标准窗口表中的序号。
该时机是注入合成输入事件的唯一正确位置: Win32 后端每次开帧都会先用真实光标位置写一次输入队列,
在开帧之前注入的事件会被它覆盖。

回调内不得再开帧或结算段; 回调抛出的失败本模块不感知, 渲染段照常开启。回调与其用户指针的所有权
归调用方: 本模块只存指针, 不复制也不释放。

---

## SetFontSize 函数 (ImGuiHost)

登记标准窗口的字号。

### 语法

```cpp
static VOID SetFontSize(
  [in] FLOAT fFontSize
);
```

### 参数

`[in] fFontSize`

类型: **FLOAT**

标准窗口的字号(像素)。合法域 = 大于等于 `8`; 小于 `8` 或为非数时按"未登记"处理(建窗回落到缺省字号 `18`)。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

登记值只在 `Start` 按默认几何构造规格表时被消费, 影响此后建立的 ImGui 面; 已就绪窗口不因本调用重建字体
(需要在建面之前登记, 或自行填 `WndSpec.fFontSize` 走 `CreateWnds` 高级用法)。重复调用以最后一次为准;
登记值与样式描述一样属进程级静态状态, `Release` / `DestroyWnds` 不清除。

字号是布局语义量(由布局文档 `autosize` 的参考字号推导), 本模块不认识布局 ⇒ 该值由调用方按布局算出后登记。

---

## SetDarkMode 函数 (ImGuiHost)

设置亮暗基础样式。

### 语法

```cpp
static VOID SetDarkMode(
  [in] BOOL bDark
);
```

### 参数

`[in] bDark`

类型: **BOOL**

非 `0` = 暗色基础样式, `0` = 亮色基础样式; 入参归一化为 `TRUE` / `FALSE` 后保存, 不做其它解释。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

基础样式只在"建 ImGui 面"与 `RefreshStyle` 两处被消费: 在 `Acquire` 之前调用即随建面生效,
运行期切换后须再调 `RefreshStyle`。默认值为暗色。

基础样式与宿主回调的叠加次序见 `SetStyleFunc` 的注解。

---

## GetDarkMode 函数 (ImGuiHost)

取当前亮暗基础样式。

### 语法

```cpp
static BOOL GetDarkMode();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 暗色, `FALSE` = 亮色。未设置时为默认的 `TRUE`。

### 注解

只读保存的标志, 不反映屏幕上已生效的样式(尚未 `RefreshStyle` 时二者可能不一致)。

---

## RefreshStyle 函数 (ImGuiHost)

把当前样式重新应用到全部已就绪窗口。

### 语法

```cpp
static BOOL RefreshStyle();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 至少有一个窗口被重新应用; `FALSE` = 没有任何已就绪的窗口(未 `Acquire` 或已 `Release`)。

### 注解

每个已就绪窗口按"亮暗基础样式 → 宿主回调"重放一次, 与建面时的次序一致; 未就绪窗口跳过。
本函数只改样式, 不动窗口几何、不触发行程, 可在运行期任意时刻调用(含外部处理流程内)。

---

## CreateWnds 函数 (ImGuiHost)

按声明的数量注册窗口类、建窗并施加圆角。

### 语法

```cpp
static BOOL CreateWnds(
  [in] CONST WndSpec* pSpecs
);
```

### 参数

`[in] pSpecs`

类型: **CONST WndSpec\***

窗口规格数组首址, 长度须不小于当前声明数量; 只有前"声明数量"项被消费。全部窗口共用首项的
`lpszClassName`(类只注册一次)。为 `NULL` 或首项类名为 `NULL` 时失败返回, 不建任何窗口。

### 返回值

类型: **BOOL**

`TRUE` = 全部声明窗口已建成; `FALSE` = 参数非法(规格或类名为空)、声明数量为 `0` 或超过 `MAX_WND`、
或任一步失败。失败时已注册的窗口类与已建窗口全部回退, 声明数量保留原值。

### 注解

调用顺序: `SetStandardWndCount` → `CreateWnds` → `Acquire` → `Run` → `Release` → `DestroyWnds`,
本函数是第二步, 早于任何 D3D 与 ImGui 初始化。推荐入口 `Start` 已把前两步与 `Acquire` 聚合为一次调用。

每个窗口依次做三件事: 以首项类名注册**一个共用窗口类**(类 DC, 无游标、无背景、无菜单, 外观全由控件库自绘)、
以 `WS_POPUP`(不含 `WS_VISIBLE`)建窗、在 `dwRounding` 大于 `0` 时施加圆角(优先系统合成,
不可用时回退区域裁剪)。建窗后窗口仍不可见, 首次显示由 `Acquire` 末尾的可见性策略统一发起。

成功时把激活窗口数置为声明数量, 并复位段状态(不残留上一轮未结算的段)。窗口过程由本模块内置, 不需要也不允许
调用方传入。

---

## Acquire 函数 (ImGuiHost)

建立全部窗口的 D3D11 设施与 ImGui 面, 并把渲染能力表注入布局渲染出口。

### 语法

```cpp
static BOOL Acquire();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 全部窗口的两套设施已就绪且能力表已注入; `FALSE` = 声明数量为 `0` 或超过上限、未建窗、
设备或上下文建立失败、字体或任一端后端失败。失败时已建设施全部回退(先关 ImGui 面, 再关 D3D 设施,
次序与释放路径一致), 段状态复位。

### 注解

调用顺序: 须晚于 `CreateWnds`、早于 `Run`。成功时依次完成: 逐窗口建立 D3D11 设施(设备、交换链、
渲染目标; 交换链尺寸即客户区尺寸)、逐窗口建立 ImGui 面(上下文 → IO 配置 → 中文字体 → Win32 后端 →
D3D11 后端 → 样式 → IME 上下文)、按激活数应用可见性(完成首次显示)、复位段状态。

IO 配置的口径: 不落盘布局文件、开启键盘导航、关闭错误恢复弹窗。

中文字体按三个系统字体候选依次尝试(带 em 换算自校准, 使一个全宽汉字恰为一个字号宽); 三者全部失败时
退回默认字体并弹一条系统警告框, 不阻断启动(这是本模块唯一的弹窗)。

能力注入: 每个窗口的 ImGui 面就绪后执行一次
`LayoutRender::SetRenderApi(ImGuiRender::GetApi())`。表为进程级静态、与窗口存续无关 ⇒ 多窗口重复注入
同一张表无副作用; 注入后布局渲染出口的各渲染入口方可工作, 未注入时它们静默失败(返回 `FALSE`)。
换用其它 UI 实现时只需在注入点改为注入另一份同形态的能力表, 消费件零改动。

释放路径: `Release` 须早于 `DestroyWnds`(避免 D3D 资源与窗口销毁次序倒置); `Release` 之后不得再 `Run`。

---

## Run 函数 (ImGuiHost)

运行主消息循环: 每帧调一次外部处理流程, 退出判据由外部持有。

### 语法

```cpp
static VOID Run(
  [in] FrameFunc   pfnFrame,
  [in] VOID*       pUser,
  [in] CONST BOOL* pbExit
);
```

### 参数

`[in] pfnFrame`

类型: **FrameFunc**

每帧调用一次的外部处理流程。为 `NULL` 时不进入循环, 静默返回。

`[in] pUser`

类型: **VOID\***

原样回传给 `pfnFrame` 的用户指针, 可为 `NULL`。

`[in] pbExit`

类型: **CONST BOOL\***

外部持有的退出标志地址。为 `NULL` 时不进入循环, 静默返回。本模块只读该地址, 每帧读一次,
从不写它, 也不持有它的所有权; 变量须在循环存续期内有效。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

每帧顺序: 渲染目标游标归零 → 消息泵(`PeekMessage` 逐个派发, 遇 `WM_QUIT` 即退出) → 再读一次退出标志 →
调一次 `pfnFrame` → 兜底结算本帧最后一段。循环退出后还会再兜底结算一次, 任何返回路径都不会留下
未结算的 ImGui 帧。

退出判据只有两个: 消息泵收到 `WM_QUIT`, 或 `*pbExit` 非 `0`。本模块不修改退出标志, 也不自行终止循环。

窗口过程在消息泵内被调用: 进入时切换到本窗口的 ImGui 上下文并转交后端消息处理, 未被消费的消息由本模块
内置处理(`WM_DESTROY` 与 `WM_KEYDOWN` 的 `ESC` 请求退出, 其余交 `DefWindowProcW`), 返回前还原进入前的
上下文。窗口级事件需要窗口句柄时经 `GetCurrentWnd` 取。

---

## AcquireNextWnd 函数 (ImGuiHost)

结算上一段、取下一个渲染目标窗口、按期望几何结算该窗口并开启新段。

### 语法

```cpp
static BOOL AcquireNextWnd(
  [in] CONST vec2& pos,
  [in] CONST vec2& size,
  [in] LPCSTR      szTitle
);
```

### 参数

`[in] pos`

类型: **CONST vec2&**

期望的窗口左上角桌面位置(像素)。与当前窗口矩形不符时本模块移动该窗口; 一致时不调窗。
来源是布局的窗口位置记录, 本模块不解释其来源, 只按值结算。

`[in] size`

类型: **CONST vec2&**

期望的客户区尺寸(像素)。任一分量非正时本函数直接失败, 不结算上一段, 也不改窗口。与当前客户区不符时
本模块调整窗口尺寸并同步交换链与圆角。

`[in] szTitle`

类型: **LPCSTR**

期望的窗口标题(UTF-8)。与上次应用的标题不同时, 本模块把它转成 UTF-16 并调 `SetWindowTextW`; 相同则不动;
为 `NULL` 表示不改标题。标题只影响任务栏与窗口枚举(窗口无系统标题栏)。

### 返回值

类型: **BOOL**

`TRUE` = 新段已开启, 调用方可以开始绘制; `FALSE` = 本次不得继续渲染(静默失败), 具体情形:
尺寸非正、本帧渲染清单已走满(游标越界)、该窗口未就绪(句柄、上下文或交换链缺失)、窗口被遮挡、
取窗口矩形失败、移动或调整尺寸失败、开启新段失败。

### 注解

反调面: 布局渲染出口每次取渲染目标都落到本函数, 因此调用顺序天然由渲染清单决定, 本模块不持位置状态、
也不认识布局语义。

每次成功调用按固定次序完成七步: 结算上一段(同一帧内下一段必须在前一段的 ImGui 帧结束之后才开帧)、
取下一个标准窗口、遮挡判定、位置结算、尺寸结算、标题同步、开启本段并推进游标。

- 遮挡判定: 先用"测试呈现"向驱动问一次(不提交任何内容); 判为遮挡时本段直接失败, 因此遮挡期间不提交
  也不呈现, 几何结算随之推迟到下一次未被遮挡的调用。
- 位置结算、尺寸结算与标题同步都在本窗口开帧之前完成 ⇒ 绘制期内不再改几何与标题; 移动与调整都只改一项,
  不动 Z 序、不激活窗口。尺寸调整会累计尺寸调整计数, 并立即同步交换链(不等窗口过程送达)。
- 标题同步按"上次应用值"缓存比较, 稳态每帧只做一次字符串比较, 不重复调窗口 API。
- 失败口径与尺寸纠偏同源: 任一窗口操作失败即整体失败, 调用方不得继续渲染。

---

## GetCurrentWnd 函数 (ImGuiHost)

取当前渲染段的窗口句柄。

### 语法

```cpp
static HWND GetCurrentWnd();
```

### 参数

此函数无参数。

### 返回值

类型: **HWND**

当前段的窗口句柄; 无已开启的段或段序号越界时返回 `NULL`。句柄由本模块持有, 调用方不得销毁它。

### 注解

窗口级事件(鼠标、键盘、标题栏交互)在消息链内处理时经本函数把目标窗口带给处理器; 段未开启时返回
`NULL`, 调用方须自行判定。

---

## GetWndPos 函数 (ImGuiHost)

取当前段窗口的桌面位置。

### 语法

```cpp
static BOOL GetWndPos(
  [out] vec2* pPos
);
```

### 参数

`[out] pPos`

类型: **vec2\***

接收窗口左上角的桌面坐标(像素)。可为 `NULL`: 此时只做可用性判定, 不写回。失败时该缓冲保持原值
(实现不触碰它)。

### 返回值

类型: **BOOL**

`TRUE` = 已取到真实窗口矩形(位置可能未写回, 见 `pPos` 可空); `FALSE` = 无已开启的段、窗口句柄为空,
或取窗口矩形失败。

### 注解

返回值是**真实窗口矩形**的左上角, 不是布局的位置记录。标题栏拖动时建立抓取偏移的现行口径读布局记录,
本函数是其回退口径:需要"窗口此刻实际在哪"时才用它, 需要"窗口应该在哪"时读布局记录。

只读系统窗口矩形, 不改任何状态。

---

## ResetWndCursor 函数 (ImGuiHost)

把本帧的渲染目标取用游标归零。

### 语法

```cpp
static VOID ResetWndCursor();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

`Run` 在每帧消息泵之后自动归零; 本函数供宿主在**自定义渲染循环**里重走渲染清单(例如同步提示等待
期间只渲染编辑窗口)。只改游标, 不结算也不开启任何段。

---

## FinalizeFrame 函数 (ImGuiHost)

结算当前已开启的渲染段。

### 语法

```cpp
static VOID FinalizeFrame();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

动作 = `ImGui::Render` + 后端提交绘制 + 应用本段 IME 覆盖 + 呈现 + 清理后台缓冲; 无段时安静返回。
本函数只清段状态, 不清渲染目标游标 —— 游标归帧级复位(`Run` 的帧末兜底与 `ResetWndCursor`), 因此
同一帧内连续取多个窗口时不会被反复拉回 0。

---

## SetIMEDisabled 函数 (ImGuiHost)

设置本帧的 IME 覆盖。

### 语法

```cpp
static VOID SetIMEDisabled(
  [in] BOOL bDisabled
);
```

### 参数

`[in] bDisabled`

类型: **BOOL**

非 `0` = 本段结算时关闭输入法; `0` = 不覆盖, 回落到 ImGui 的输入请求判定。入参归一化为 `TRUE` / `FALSE`。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

覆盖只对"本帧下一段的结算"有效, 段结算后自动失效, 不跨帧保留。未设置覆盖值时, 段结算按
ImGui 的输入请求判定(有编辑中的输入项 ⇒ 关闭输入法, 否则打开)。

调用时机 = 外部处理流程内、在布局渲染出口取渲染目标之前; 不存 IME 上下文的窗口(未就绪)不受影响。

---

## GetRenderStats 函数 (ImGuiHost)

取绘制量自证计数。

### 语法

```cpp
static VOID GetRenderStats(
  [out] DWORD* pdwSegments,
  [out] DWORD* pdwCmdLists,
  [out] INT64* pllVtx,
  [out] INT64* pllIdx
);
```

### 参数

`[out] pdwSegments`

类型: **DWORD\***

接收已结算的渲染段数; 该值随每次成功的段结算累加。可为 `NULL`(不写回)。

`[out] pdwCmdLists`

类型: **DWORD\***

接收已提交的绘制命令数累计。可为 `NULL`(不写回)。

`[out] pllVtx`

类型: **INT64\***

接收已提交的顶点数累计。可为 `NULL`(不写回)。

`[out] pllIdx`

类型: **INT64\***

接收已提交的索引数累计。可为 `NULL`(不写回)。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

计数为进程启动起累计, 只在段结算时增长(遮挡或失败而未结算的段不计数); 本模块不清零、不提供复位入口。
四个出参相互独立, 传 `NULL` 的项只是不被写回, 其余照常写回。

---

## GetResizeStats 函数 (ImGuiHost)

取窗口尺寸调整自证计数与最近一次调整后的客户区尺寸。

### 语法

```cpp
static VOID GetResizeStats(
  [out] DWORD* pdwCount,
  [out] DWORD* pdwLastW,
  [out] DWORD* pdwLastH
);
```

### 参数

`[out] pdwCount`

类型: **DWORD\***

接收尺寸调整次数(进程启动起累计, 只计入 `AcquireNextWnd` 在尺寸不符时发起的调整, 窗口过程侧的
尺寸记账不计入)。可为 `NULL`(不写回)。

`[out] pdwLastW`

类型: **DWORD\***

接收最近一次调整后的客户区宽(像素); 从未调整过时为 `0`。可为 `NULL`(不写回)。

`[out] pdwLastH`

类型: **DWORD\***

接收最近一次调整后的客户区高(像素); 从未调整过时为 `0`。可为 `NULL`(不写回)。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

计数与最近尺寸同源于同一次调整, 因此三个值可互相印证(次数为 `0` 时两个尺寸必为 `0`)。本模块不清零。

---

## Release 函数 (ImGuiHost)

释放全部窗口的 ImGui 面与 D3D11 设施。

### 语法

```cpp
static VOID Release();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

幂等: 每个窗口逐项关停, 已释放的项跳过; 重复调用无副作用。

次序: 先关 ImGui 面(后端须在 D3D 设施有效期内关停, 因其纹理与设备上下文挂在窗口上下文内, 随后销毁
上下文并归还 IME 上下文), 再关 D3D 设施; 最后复位段状态。本函数**不动窗口**: 窗口仍存在且仍可见,
只是内容不再更新。

调用时机: 须早于 `DestroyWnds`; 调用后不得再 `Run`, 也不得再取渲染目标。释放后 `IsReady` 一律为 `FALSE`,
`RefreshStyle` 返回 `FALSE`。

---

## DestroyWnds 函数 (ImGuiHost)

销毁全部标准窗口并注销窗口类。

### 语法

```cpp
static VOID DestroyWnds();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

幂等: 逐窗口销毁句柄并复位单元字段(句柄、类原子、字号、圆角与圆角实现方式、标题缓存、显示状态),
全部窗口销毁后**只注销一次**共用窗口类, 激活窗口数清零, 段状态复位。

**不**清声明数量: 声明归 `SetStandardWndCount`, 保留它便于失败后按原声明重试。

销毁会触发 `WM_DESTROY` 等消息并照常由内置窗口过程处理(`WM_DESTROY` 会 `PostQuitMessage`)。因此调用方
须在调用本函数前自行退出主循环并已调用 `Release`; `DestroyWnds` 之后的 `WM_QUIT` 已无主循环消费,
不影响进程退出码。

---

## GetWindow 函数 (ImGuiHost)

取指定窗口的窗口句柄。

### 语法

```cpp
static HWND GetWindow(
  [in] DWORD index
);
```

### 参数

`[in] index`

类型: **DWORD**

标准窗口序号(从 `0` 起)。越界(`index >= MAX_WND`)时返回 `NULL`。

### 返回值

类型: **HWND**

该序号的窗口句柄; 未建窗时为 `NULL`。句柄由本模块持有, 调用方不得销毁。

### 注解

供需要时取用, 外部不得持久持有句柄(窗口可能在 `DestroyWnds` 后被销毁)。常规渲染路径不需要句柄,
窗口级事件经 `GetCurrentWnd` 取当前段的那个。

---

## GetContext 函数 (ImGuiHost)

取指定窗口的 ImGui 上下文。

### 语法

```cpp
static ImGuiContext* GetContext(
  [in] DWORD index
);
```

### 参数

`[in] index`

类型: **DWORD**

标准窗口序号(从 `0` 起)。越界时返回 `NULL`。

### 返回值

类型: **ImGuiContext\***

该窗口的 ImGui 上下文指针; 未 `Acquire`、已 `Release` 或该窗口建面失败时返回 `NULL`。
上下文由本模块创建并销毁, 调用方不得销毁它。

### 注解

需要"在该窗口上下文内做 ImGui 操作"时, 调用方须自行 `SetCurrentContext` 并在返回前还原;
常规渲染路径不经过本函数(段开启与结算内部切换上下文)。

---

## IsReady 函数 (ImGuiHost)

查询指定窗口的 ImGui 面是否已就绪。

### 语法

```cpp
static BOOL IsReady(
  [in] DWORD index
);
```

### 参数

`[in] index`

类型: **DWORD**

标准窗口序号(从 `0` 起)。越界时返回 `FALSE`。

### 返回值

类型: **BOOL**

`TRUE` = 该窗口的 ImGui 上下文已建立; `FALSE` = 序号越界, 或该窗口未就绪。

### 注解

判据只有"上下文是否已建立": 已建窗但未 `Acquire`、以及 `Release` 之后都为 `FALSE`。
本函数不做任何初始化, 不改变状态。

---

## MAX_WND 常量 (ImGuiHost)

标准窗口数量上限。

### 语法

```cpp
static constexpr DWORD MAX_WND = 8;
```

### 值

`8`。

### 注解

本值同时是模块内窗口单元表的容量:序号 `0` 到 `MAX_WND - 1` 是合法序号域, 各接口对越界序号的处置都以它
为界(越界回空值或 `WND_SHOW_NEVER`, 不做夹取)。`SetStandardWndCount` 与 `SetActiveWndCount` 收到的更大
声明值会被夹取到本值或更低, 因此实际建成的窗口数不超过本值。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/render/imgui/ImGuiHost.h`(`src\framework\render\imgui\ImGuiHost.h`) |
| 实现 | `src\framework\render\imgui\ImGuiHost.cpp`;`WndSpec` 等结构定义在头文件内 |
| 命名空间 | `ImGuiHost`(命名空间内含 `WndSpec` 结构与状态枚举) |
| 依赖 | `minwindef.h` · `imgui.h` · `imgui_impl_win32` · `imgui_impl_dx11` · `user32.dll` · `framework/base/AppEnv.h` · `dwmapi.dll` · `framework/render/imgui/D3D11Manager` · `imgui` · `framework/render/Render.h` · `framework/render/imgui/ability/ImGuiRender` · `framework/render/imgui/control/Controls` · `framework/layout/LayoutRender` · `imm32.dll` · `dxgi.dll` · `framework/base/ControlSpec.h` |
| 平台 | Windows 全版本 |
| 线程安全 | 否; 主消息循环、每窗口 ImGui 上下文与 D3D 设施须在进程主线程串行使用 |
