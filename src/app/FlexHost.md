# FlexHost 模块 (src\app\FlexHost.h)

## 概述

`FlexHost` 是内嵌(flex)窗口成员的会话面持有者: 由外壳源文档的解析结果装配成员条目, 提供活动开关、渲染
查询与释放。成员是"一个窗口内的多个内嵌窗口"这一布局情形的载体: 不拥有独立 `HWND`, 由宿主(基座)窗口的
渲染帧内以 `LayoutRender::RenderFlex` 同帧绘制。

两条硬契约:

1. 成员来源 = 外壳源: 成员取自外壳源文档的解析产物, 与外壳快照同源同生命周期 ⇒ 不随被编辑文档的切换或
   新建变化; 中间态节点取自解析器, 生存期等于会话分配器 ⇒ 文档替换不会使成员悬垂。
2. 按需显示: 装配完成后全部成员恒为非活动; 需要显示时经 `SetFlexActive` 置真, 用毕复位。忘记复位会让该
   成员在后续每帧继续被渲染。

"谁是内嵌窗口"由本模块承载(布局文件不承载内嵌关系): 名单是文件内静态数组, 名单内的布局是无独立 `HWND`
的内嵌成员, 名单外的布局照旧各自拥有 `HWND`。基座归属判据的外壳布局指针由应用侧注入(`SetShellBase`),
`NULL` 表示无外壳基座 ⇒ 查询一律返回"无活动成员"。

本模块只承载内嵌成员: 外壳快照本体归 `app\PreviewSession`, 条目表与中间态归 `doc\Document`, 实际绘制归
`framework\render\Render.h`。状态全为静态成员、无内部同步; 分配一律走 `framework\base\mem\AllocatorHub` 的公开接口, 本模块不做
堆分配, 也不释放分配器持有的存储。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `SetShellBase` | 注入内嵌成员宿主基座的外壳布局指针。 |
| `FindFlexEntry` | 按窗口布局对象取成员条目的只读投影。 |
| `BuildFlexMembersFromSolver` | 由外壳源解析结果装配全部内嵌成员。 |
| `FreeFlexLayouts` | 释放全部内嵌成员并清空活动集合。 |
| `GetActiveFlexLayouts` | 查询指定基座当前活动的成员集合(渲染接线入口)。 |
| `GetFlexRef` | 取指定成员的参考尺寸。 |
| `GetFlexCount` | 取内嵌成员总数。 |
| `SetFlexActive` | 按布局名置某成员的活动状态。 |

### 类型

| 类型 | 说明 |
| --- | --- |
| `FlexEntry` | 一个内嵌成员条目: 条目本体 + 活动位。 |
| `FlexHost` | 全静态类, 装配 / 开关 / 查询 / 释放的唯一入口。 |

### 常量

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `GENUI_HINT_LAYOUT_NAME` | `u8"提示布局"` | 名单内的初版唯一成员名, 也是成员缺席时的回落名。 |
| `HINT_BODY_BUF` | `512` | 提示正文控件缓冲容量(替代通用文本容量的专用口径)。 |

---

## SetShellBase 函数 (FlexHost)

注入"内嵌成员的宿主基座"的外壳布局指针。

### 语法

```cpp
static VOID SetShellBase(
  [in] Layout* pShell
);
```

### 参数

`[in] pShell`

类型: **Layout\***

外壳(基座)布局对象指针, 不是条目指针 —— 传入 `LayoutEntry*` 的形态即误用。`NULL` = 复位(此后查询一律视为
"无活动成员")。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

该指针是查询侧"归属过滤"判据的唯一来源: 只有与它同一对象的基座才会得到非空的活动集合。

注入与复位必须与外壳快照的建立 / 释放成对: 注入点是外壳快照构建成功之后、装配内嵌成员之前; 复位点是外壳
快照释放点以及应用侧成员初始化与释放的同点。内嵌成员与外壳快照同生命周期, 单独复位基座指针会让已装配成员
永远查不出来(但内存仍由分配器持有, 直到装配或释放被再次触发)。

本函数只改判据, 不改动任何成员条目、也不释放任何对象; 重复注入同一指针是幂等的。

---

## FindFlexEntry 函数 (FlexHost)

按窗口布局对象取内嵌成员条目的只读投影。

### 语法

```cpp
static LayoutEntry* FindFlexEntry(
  [in] Layout* layout
);
```

### 参数

`[in] layout`

类型: **Layout\***

待查的窗口布局对象。允许为 `NULL`(返回 `NULL`)。

### 返回值

类型: **LayoutEntry\***

命中返回成员条目本体指针; 未命中或未装配返回 `NULL`。

### 注解

线性扫描成员条目数组, 首个 `entry.layout == layout` 者命中, 不做指针排序或缓存, 因此每次调用为 `O(成员数)`。

返回的是条目本体而非副本: 写它等于改成员条目, 调用方按只读使用。指针随下一次装配或释放失效 ⇒ 不得跨帧
持久化持有; 需要长期引用时改用"成员名"作为键。

用途是条目族反查: 判定某个布局对象是不是某个内嵌成员, 与 `app\PreviewSession` 的 `GetShellEntry` /
`GetPreviewBase` 同族(分别判定外壳快照与预览基座)。

---

## BuildFlexMembersFromSolver 函数 (FlexHost)

由外壳源文档的解析结果装配全部内嵌成员。

### 语法

```cpp
static BOOL BuildFlexMembersFromSolver(
  [in] LayoutSolver* solver
);
```

### 参数

`[in] solver`

类型: **LayoutSolver\***

外壳源的解析器, 与外壳快照是同一份解析产物。传 `NULL` 返回 `FALSE`。

### 返回值

类型: **BOOL**

`TRUE` = 装配完成, 允许缺少部分或全部成员; `FALSE` = 应用未就绪 / 分配器未就绪 / `solver` 为 `NULL` /
条目数组或活动指针缓存分配失败(此时既有成员与三个状态字段保持原状)。

### 注解

顺序固定, 逐段不可交换:

1. 入口判应用就绪、分配器就绪与 `solver` 非空, 任一不满足即返回 `FALSE`, 不触碰既有成员。
2. 先 `FreeFlexLayouts()` 释放既有成员(重复装配场景)。
3. 取外壳源中间态与布局数; 名单为空则直接返回 `TRUE`(合法: 无内嵌成员), 该分支不再提交状态字段。
4. 条目数组与活动指针缓存一次分配, 任一步失败即整体放弃且不提交(不留下半装配状态)。
5. 逐成员在外壳源的中间态布局链上按名查找; 缺员或展开失败(含不支持控件 / 分配失败)跳过并逐条诊断, 不算
   失败, 其余成员照常装配。
6. 建条目: 名称与标签(中间态为空串时回落名单名)、可最大化、可缩放、参考尺寸、窗体区像素位置、中间态节点
   绑定、线性只读展开态; 随后解算窗口像素与逐控件几何。
7. 正文控件 `##TEXT_HINT` 的缓冲按 `HINT_BODY_BUF` 扩容, 旧缓冲回池, 新缓冲先拷贝旧内容再替换。
8. 活动位一律置 `FALSE`。
9. 全部完成后一次性提交三个状态字段(条目数组 / 数量 / 活动指针缓存)并返回 `TRUE`。

成员来源是外壳源, 不随被编辑文档的切换或新建变化; 条目与活动指针缓存取自会话线性分配器, 随池整体释放,
本模块不单独归还它们。部分装配成功同样返回 `TRUE` ⇒ 判"是否成功"看返回值, 判"装了几枚"看
`GetFlexCount`。

名单策略由本模块承载: 名单是文件内静态数组, 不对外公开, 需要增减成员时改名单重编; 布局文件不表达内嵌关系。

---

## FreeFlexLayouts 函数 (FlexHost)

释放全部内嵌成员。

### 语法

```cpp
static VOID FreeFlexLayouts();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

逐成员处理: 先回收该布局全部控件的自身缓冲, 再析构布局对象(成员是线性只读展开态 ⇒ 析构只逐控件析构, 不
涉及链节点池), 然后断开条目里的布局指针。全部处理完后条目数组指针与活动指针缓存置空、成员数量归零。

条目与缓存的内存归会话线性分配器, 此处不归还, 只断引用; 释放后活动集合为空 ⇒ 查询返回 `NULL` 与数量 0。

未就绪时直接返回, 不改动状态(应用未就绪时三个状态字段本就为空, 见 `FlexHost` 类注解的不变式)。幂等:
重复调用无副作用。

---

## GetActiveFlexLayouts 函数 (FlexHost)

查询指定基座当前活动的内嵌成员集合, 渲染接线入口。

### 语法

```cpp
static Layout** GetActiveFlexLayouts(
  [in]  Layout& base,
  [out] DWORD*  pdwCount
);
```

### 参数

`[in] base`

类型: **Layout&**

基座(宿主)布局引用。只有与 `SetShellBase` 注入值同一对象时才可能得到非空集合; 其它基座一律视为"无活动
成员"(归属过滤: 内嵌成员的宿主恒为外壳布局)。

`[out] pdwCount`

类型: **DWORD\***

活动成员数量的写回处, 可为 `NULL`。函数入口先置 `0`, 任何提前返回都不留脏值。

### 返回值

类型: **Layout\*\***

活动成员的布局对象指针数组(二级指针); 无活动成员(含全部非活动、未装配、非外壳基座)返回 `NULL` 且
`*pdwCount == 0`, 调用方据此跳过渲染。

### 注解

每次查询重算, 结果写入条目级的指针缓存(容量 = 名单长度), 元素有效至下次查询 ⇒ 渲染期只读, 不得跨帧缓存
该数组。缓存复用意味着返回的数组地址可能不变, 但其内容在下一次查询时被覆盖。

成员的活动位为真但布局对象为空(装配中途被跳过)时不入集合; 因此集合长度可能小于成员总数, 小于等于名单
长度。

未装配或已释放(数组或缓存为空)⇒ 直接 `NULL`; 该判据先于归属过滤。

---

## GetFlexRef 函数 (FlexHost)

取指定内嵌成员的参考尺寸。

### 语法

```cpp
static CONST AutoSize& GetFlexRef(
  [in] Layout* layout
);
```

### 参数

`[in] layout`

类型: **Layout\***

成员的窗口布局对象。`NULL` 或未命中按缺省样例处理。

### 返回值

类型: **CONST AutoSize&**

命中返回该成员的参考尺寸(条目内字段的引用); 未命中返回函数内静态缺省样例 —— 比例 `622×432`、经验量
`25 / 17 / 10 / 4`、屏高模式。

### 注解

缺省样例是共享只读静态件: 与任何成员状态无关, 也不得写入。成员条目的引用在成员被释放后即失效 ⇒ 不要跨
`FreeFlexLayouts` 持有。

参考尺寸是渲染所需的尺寸基准(成员不建窗口, 尺寸只用于绘制与几何换算), 与窗口矩形没有对应关系。

---

## GetFlexCount 函数 (FlexHost)

取内嵌成员总数。

### 语法

```cpp
static DWORD GetFlexCount();
```

### 参数

此函数无参数。

### 返回值

类型: **DWORD**

装配结果中的成员枚数; 应用未就绪时返回 `0`。

### 注解

该值是"成功装配的成员数", 不是名单长度: 缺员与展开失败都会减少它。未装配、名单为空、全部成员被跳过时均为
`0`。

用途是诊断与"展示器是否可用"的判据(`0` = 无可显示成员); 它在装配成功提交之后不再变化, 直到下次装配或
释放。

---

## SetFlexActive 函数 (FlexHost)

按布局名置某内嵌成员的活动状态。

### 语法

```cpp
static BOOL SetFlexActive(
  [in] LPCSTR name,
  [in] BOOL   bActive
);
```

### 参数

`[in] name`

类型: **LPCSTR**

成员布局名, 即装配时写入条目的名称(取自中间态, 空串时回落名单名)。`NULL` 返回 `FALSE`; 匹配为整串比较,
不做前缀或模糊匹配。

`[in] bActive`

类型: **BOOL**

`TRUE` = 活动(参与渲染); `FALSE` = 非活动(不渲染)。

### 返回值

类型: **BOOL**

`TRUE` = 命中并已设置; `FALSE` = 未装配 / `name` 为 `NULL` / 无同名成员。

### 注解

按名线性比对全部成员, 命中首个即设值并返回; 只改活动位, 不动布局对象、几何与参考尺寸。

按需显示口径: 装配后全部成员非活动 ⇒ 展示前置真, 用毕复位。活动位不持久化: 下次装配会把它重置为
`FALSE`, 因此"上次显示过"不构成默认显示的理由。

同名成员不会出现(名单由本模块决定且不含重复名), 因此不存在"命中多枚"的情形。

---

## FlexEntry 结构 (FlexHost)

一个内嵌(flex)窗口条目。

### 语法

```cpp
struct FlexEntry
{
  LayoutEntry entry;
  BOOL        bActive;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `entry` | 内嵌成员条目: 线性只读展开态 + 自身参考尺寸 + 中间态节点绑定。 |
| `bActive` | 是否活动; 渲染查询按此过滤; 装配后恒为 `FALSE`。 |

### 注解

内嵌窗口不拥有独立 `HWND`: 位置 = 该布局窗体区声明的像素位置(相对宿主客户区), 尺寸 = 该窗体区声明尺寸
⇒ 成员不参与窗口创建, 只参与绘制, 且绘制与宿主同 context 同帧。

`entry.layout` 是该成员的一份线性(只读)展开态, 来源为外壳源文档; 会话级固定, 不随被编辑文档变化。
`entry.pNode` 指向中间态节点(几何系数的存放处), 成员只读它, 不做增删。

条目数组由会话分配器持有, 本模块不单独释放数组内存; 逐成员的布局对象则在 `FreeFlexLayouts` 中析构。

---

## FlexHost 类 (FlexHost)

内嵌成员装配、活动开关、渲染查询与释放的唯一入口。

### 语法

```cpp
class FlexHost
{
public:
  FlexHost() = delete;
  ~FlexHost() = delete;

  static VOID SetShellBase(Layout* pShell);
  static LayoutEntry* FindFlexEntry(Layout* layout);
  static BOOL BuildFlexMembersFromSolver(LayoutSolver* solver);
  static VOID FreeFlexLayouts();
  static Layout** GetActiveFlexLayouts(Layout& base, DWORD* pdwCount);
  static CONST AutoSize& GetFlexRef(Layout* layout);
  static DWORD GetFlexCount();
  static BOOL SetFlexActive(LPCSTR name, BOOL bActive);
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `FlexHost()` | 构造私有且删除: 类不可实例化。 |
| `~FlexHost()` | 析构私有且删除: 类不可实例化。 |
| `SetShellBase` | 注入基座判据(外壳布局指针)。 |
| `FindFlexEntry` | 按布局对象反查成员条目。 |
| `BuildFlexMembersFromSolver` | 装配全部成员并提交状态。 |
| `FreeFlexLayouts` | 析构全部成员布局对象并清状态。 |
| `GetActiveFlexLayouts` | 收集当前活动成员(渲染接线入口)。 |
| `GetFlexRef` | 取成员参考尺寸(未命中给缺省样例)。 |
| `GetFlexCount` | 取成员枚数。 |
| `SetFlexActive` | 按名切换成员活动位。 |

### 注解

全静态类, 无实例、无虚函数: 状态全部是静态成员 —— 条目数组 / 成员数量 / 活动指针缓存三枚, 外加注入的外壳
基座指针一枚。构造与析构私有并删除, 因此语义上等价于进程级单例。

不变式: 三枚成员状态字段的唯一非空来源是 `BuildFlexMembersFromSolver` 的成功提交, 而该函数入口已判应用
就绪, 且应用侧在成员存储失效的同点先调用 `FreeFlexLayouts()` ⇒ "应用未就绪 ⇒ 三字段必为空"。因此各查询
接口只需用"数组为空"即可判定"未装配", 不必再加一层就绪判据; 释放与装配接口仍各自判一次就绪, 以免在未就绪
档触碰状态。

调用顺序: `SetShellBase` → `BuildFlexMembersFromSolver` → 查询族(`GetActiveFlexLayouts` /
`GetFlexRef` / `GetFlexCount` / `FindFlexEntry`)与开关(`SetFlexActive`)→ `FreeFlexLayouts` +
`SetShellBase(NULL)` 成对收尾。装配与释放都自持前置释放, 因此"重复装配"与"重复释放"都安全。

所有权: 本模块不拥有任何存储 —— 条目数组、活动指针缓存、字符串与布局对象的存储都由会话分配器提供; 本模块
只持有指针并负责调用析构。释放只断引用、不归还池块。

线程: 全部接口无内部同步, 静态状态共享; 装配、释放、查询必须在同一线程串行(通常即界面线程)。渲染期内只
允许调用查询族与 `SetFlexActive`。

---

## GENUI_HINT_LAYOUT_NAME 常量 (FlexHost)

内嵌成员名单里的初版唯一成员名。

### 语法

```cpp
constexpr LPCSTR GENUI_HINT_LAYOUT_NAME = u8"提示布局";
```

### 值

`u8"提示布局"`(UTF-8 字面量; 按字节比较, 与解析器写入的布局名同一编码口径)。

### 注解

该名有三处语义, 改动即三处同时变: 名单里唯一的成员名; 成员缺席时条目 `name` / `label` 的回落值; 调用方经
`SetFlexActive` 按名命中时的键。

名单里只有它 ⇒ 当前内嵌成员恒为 1 枚; 外壳源缺少该布局时为 0 枚(装配按"缺员跳过"处理, 不算失败), 此时
`GetFlexCount` 返回 0。名字是保留名: 外壳源里的布局名必须与它逐字节相同才被认作该成员。

---

## HINT_BODY_BUF 常量 (FlexHost)

提示正文控件的缓冲容量。

### 语法

```cpp
constexpr DWORD HINT_BODY_BUF = 512;
```

### 值

`512`。

### 注解

通用装配给文本类控件的容量是 `CtrlBuffer.h` 的 `CTRL_TEXT_BUF`(256); 提示正文可能较长(文件名 / 路径等)
⇒ 装配时对 `##TEXT_HINT` 这一个控件单独扩到本值, 旧缓冲回池。

扩容判据是"现有容量小于本值"才扩 ⇒ 已有更大容量不缩、不重建; 新缓冲分配失败时保留原缓冲(不破坏已有内容)。
本值只影响该正文控件, 不改其它文本控件与结构件的容量口径。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `app/FlexHost.h`(`src\app\FlexHost.h`) |
| 实现 | `src\app\FlexHost.cpp` |
| 命名空间 | 全局(类 `FlexHost`、结构 `FlexEntry`、两个 `constexpr` 常量) |
| 依赖 | 工程内: `app\Application`(就绪判据与条目几何族)、`doc\Document`(条目形状 `LayoutEntry`)、`framework\base\mem\AllocatorHub`(会话分配器与串池化)、`framework\layout\Layout`(`Layout` 与析构)、`framework\layout\LayoutModel`(中间态取节点)、`framework\layout\LayoutBuilder`(中间态 → 展开态 / 整枚布局缓冲回收)、`framework\layout\LayoutSolver`(解析器与中间态)、`framework\base\CtrlBuffer`(控件缓冲: 容量口径 / 取址 / 回收); 渲染出口为 `framework\layout\LayoutRender` 的 `RenderFlex`。系统: CRT(`printf` / `strcmp` / `strncpy_s`) |
| 平台 | Windows; 内嵌成员无独立 `HWND`, 依赖宿主窗口同帧绘制, 无窗口的进程不必调用本模块 |
| 线程安全 | 否(状态全为静态成员, 无内部同步; 装配 / 释放 / 查询须同线程串行) |
