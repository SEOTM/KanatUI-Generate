# Widgets 模块 (src\framework\render\imgui\control\Widgets.h)

## 概述

控件库的部件层:把"一枚控件怎么画"从控件层剥出来, 成为与控件类型无关的可复用绘制单元。部件 = 一枚绘制
单元, 输入是"矩形 + 绘制方式 + 该件自身数据", 输出是它自己的交互结论; 部件不知道控件类型、不持控件缓冲、
不接触控件结构, 也不按控件类型分派。控件层因此不再优先直调 UI 库原生控件, 改为优先调用同名部件,
只有在部件内部才使用原生控件或自绘。

按参数形态分三型, 各设一个嵌套命名空间:基础部件(`Widgets::Base`, 固定四参数 + 该件数据)、命令部件
(`Widgets::Command`, 给一枚描述符一次画完)、管道部件(`Widgets::Pipe`, 由调用方逐行送入)。列表的两种部件
并存而非新旧替代:命令式把整份数据组进描述符, 管道式按行送入(容器状态归子窗口, 业务状态归调用方);
"命令式 / 管道式"只用于部件, 不用于控件。

本模块属 **framework 层 · 控件库内部**, 依赖 `ControlSpec.h`(操作码)、`BasicDesign.h`(`AutoSize` /
`ProjCR`)、`CtrlBuffer.h`(行数据块)、`Controls.h`(`ListRowDropOper` 落点换算)、`TextWidth.h`(按钮宽推算),
不引 `app/`、`editor/`、`doc/`、演示层头。部件不持窗口与设备(不取 `hWnd`, 不持纹理生命周期),
也不做控件级操作码分段(只报结论, 落到哪一段由调用方决定)。

一切失败与非法入参都是静默的:不弹窗、不打印、不写日志, 只返回空结论。取色一律经第四固定参数传入成品
配色方案, 部件不按控件层次取色(唯一例外 = 列表拖动辅助的两枚绝对档位色)。非根窗口级圆角一律取部件层
上下文当前帧的圆角值。

本模块包含以下编程接口:

### 类型

| 类型 | 说明 |
| --- | --- |
| `WidgetState` | 部件状态位: 常态、悬停、激活、禁用。 |
| `StateColors` | 一个色组的四态颜色, 并带取态方法。 |
| `Palette` | 部件配色板: 六个色组(底 / 前景 / 表面 / 表面前景 / 边框 / 内边框)。 |
| `desc` | 绘制方式的判别值: 配色方案或纹理。 |
| `PaintWay` | 绘制方式: 判别位 + 载荷联合, 作为部件的第四固定参数。 |
| `pfnListWidget` | 列表行件的函数指针类型。 |
| `ListSpec` | 列表命令的描述符(命令部件的入参)。 |
| `ListPipe` | 列表管道的描述(管道部件的入参, 由调用方逐次携带)。 |

### 函数

| 函数 | 说明 |
| --- | --- |
| `MakePaletteWay` | 构造配色方案绘制方式(值返回, 供调用点直接作实参)。 |
| `StateColorU32` | 把状态色换算为绘制列表可用色(再按当前样式透明度乘算)。 |
| `ItemDisabled` | 读最后一个项的禁用标志。 |
| `CenterOffset` | 计算容器与内容的居中偏移量(下限 0)。 |
| `ExitSign` | 关闭按钮部件: 两段交叉线符号 + 悬停底。 |
| `InputText` | 帧类单行文本件: 底与边框由原生帧绘制, 帧高对齐声明高。 |
| `InputTextMulti` | 帧类多行文本件: 项尺寸取声明矩形。 |
| `InputScalar` | 帧类数值件: 底与边框由原生帧绘制。 |
| `Combo` | 帧类下拉件: 原生帧底 + 补画边框, 展开项表与两段件同源。 |
| `TimeSpan` | 时段显示件: 自绘圆角底 + 起止时码文本, 无交互项。 |
| `InputAction` | 两段件: 左段单行输入 + 右段按钮。 |
| `ComboAction` | 两段件: 左段下拉框 + 右段按钮。 |
| `SingleSlider` | 单手柄滑动条: 命中区覆盖整个控件矩形。 |
| `ProgressStatic` | 进度状态显示件: 自绘圆角底 + 进度文本。 |
| `Button` | 按钮件: 三态面 + 面内文字, 自带可交互项。 |
| `Text` | 文本图元件: 矩形内垂直居中绘制一行文本并裁剪。 |
| `TextMulti` | 块级多行文本件: 四边内缩一枚内边距后按宽折行。 |
| `CheckBox` | 复选框件: 原生方框 / 勾 / 文字, 勾选态经引用写回。 |
| `RenderSurface` | 渲染面件: 有纹理等比居中绘制, 无纹理画占位面。 |
| `InputAction`(列表行重载) | 列表行件: 解包行数据后转单件实现(行首拖拽把手占位)。 |
| `InputActionRO` | 列表行件只读形态: 文本容量按 0 传入。 |
| `ComboAction`(列表行重载) | 列表行件: 解包选项表后转单件实现。 |
| `List` | 命令部件: 按列表描述符一次画完并逐行收集结论。 |
| `ListRow` | 管道部件: 逐行送入并绘制一行。 |
| `UpdateContext` | 更新部件层上下文(每帧一次), 并复位编辑中计数。 |
| `HasActiveInput` | 查询本帧是否存在编辑中的输入项。 |

本模块不定义公开常量:拖拽载荷类型串、滑块几何比例、拖动辅助的两枚绝对档位色, 以及矩形面绘制、下拉
项表提交、行首把手、整行放置目标、行结论折算这几个内部件, 均为文件内静态实现细节, 不对外暴露。

---

## WidgetState 枚举 (Controls::Widgets::Base)

部件状态位(位标志), 用于按状态取色与判定禁用。

### 语法

```cpp
enum WidgetState : DWORD
{
    WIDGET_STATE_BASE   = 0,
    WIDGET_STATE_HOVERED = 1,
    WIDGET_STATE_ACTIVE  = 2,
    WIDGET_STATE_DISABLE = 4,
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `WIDGET_STATE_BASE` | 基础状态(默认值, 位图为零)。 |
| `WIDGET_STATE_HOVERED` | 悬停。 |
| `WIDGET_STATE_ACTIVE` | 激活(按下)。 |
| `WIDGET_STATE_DISABLE` | 禁用。 |

### 注解

同名的三枚位运算重载(位或、位与、复合位或)就定义在本枚举之后:位或用于合并两枚状态位, 位与用于判定某位
是否置位, 复合位或用于就地置位。三者都是头文件内联的小件, 语义限于位图运算, 不改任何绘制状态。

取态优先级由 `StateColors::StateColor` 决定:禁用高于激活, 激活高于悬停, 悬停高于常态 ⇒ 当多个位置位时
只有优先级最高的那一态生效。基座位为零, 因此"位与"判定某位是否置位时对基座恒为假。

---

## StateColors 结构 (Controls::Widgets::Base)

一个色组的四态颜色, 并按状态位取其中一态。

### 语法

```cpp
struct StateColors
{
    DWORD crBase;
    DWORD crHovered;
    DWORD crActive;
    DWORD crDisable;

    DWORD StateColor(WidgetState state) const;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `crBase` | 常态色。 |
| `crHovered` | 悬停色。 |
| `crActive` | 激活(按下)色。 |
| `crDisable` | 禁用色。 |
| `StateColor(WidgetState)` | 按状态位取一态颜色; 优先级 = 禁用 → 激活 → 悬停 → 常态。 |

### 注解

颜色是 32 位打包值, 分量次序为红、绿、蓝、透明度(透明度在最高字节); 灰色不透明即 `0xFFAFAFAF` 一类写法。
本结构只存值, 不持有任何绘制资源; 换算成绘制列表可用色须再经 `StateColorU32`(其中含当前样式透明度的
乘算), 直接用原始值绘制会漏掉透明度。

---

## Palette 结构 (Controls::Widgets::Base)

部件配色板:六个色组, 两面各自取各自的前景色。

### 语法

```cpp
struct Palette
{
    StateColors crBackground;
    StateColors crForeground;
    StateColors crSurface;
    StateColors crSurfaceForeground;
    StateColors crBorder;
    StateColors crBorderInner;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `crBackground` | 底色(整条背景)。 |
| `crForeground` | 底色面上的前景色(文本 / 图形)。 |
| `crSurface` | 表面色(段面: 两段件的右段 / 按钮面)。 |
| `crSurfaceForeground` | 段面上的前景色(文本 / 图形)。 |
| `crBorder` | 边框色(整条外框)。 |
| `crBorderInner` | 内边框色。 |

### 注解

两面层次不同时不得混用同一枚前景色:`crForeground` 只用于底色面, `crSurfaceForeground` 只用于段面
(按钮、两段件的右段面)。每个色组自带常态、悬停、激活、禁用四态, 取态规则见 `StateColors`。

配色板由调用方按控件层次采样后整份传入, 部件只消费不构造 ⇒ 换配色不改部件。

---

## desc 枚举 (Controls::Widgets::Base)

绘制方式的判别值。

### 语法

```cpp
enum desc
{
    PALETTE = 0,
    IMAGE   = 1,
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `PALETTE` | 按成品配色方案绘制; 现行唯一落地的形态。 |
| `IMAGE` | 按纹理绘制; 只预留能力, 无调用方。 |

### 注解

判别值为 `IMAGE` 时各部件一律不产出像素, 直接返回空结论(零值或 `FALSE`) —— 这是预留位而非可用能力,
调用方不得依赖它。判别值默认取 `PALETTE`(绘制方式零初始化即为该值)。

---

## PaintWay 结构 (Controls::Widgets::Base)

绘制方式:判别位 + 载荷联合, 是所有基础部件的第四固定参数。

### 语法

```cpp
struct PaintWay
{
    desc eDesc;
    union
    {
        Palette palette;
        void*   pImage;
    };
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `eDesc` | 绘制方式判别位(见 `desc`)。 |
| `palette` | 判别位为 `PALETTE` 时的成品配色方案。 |
| `pImage` | 判别位为 `IMAGE` 时的纹理载荷(预留, 无消费实现)。 |

### 注解

缺省构造 = 零初始化 ⇒ 判别位为零(即 `PALETTE`), 载荷为零; 调用点一律经 `MakePaletteWay` 构造,
不手写联合体赋值。

取色口径:部件**不按控件层次取色**, 配色方案由调用方按控件层次采样后整份传入, 部件内部只用
`StateColors` 与 `StateColorU32` 取态。唯一例外 = 列表拖动辅助(行首把手与插入线)按两枚**绝对档位**取色,
与控件层次无关; 该处取色在投影配色未注入时返回 `0`(色值全零), 表现为不着色, 不影响其余绘制。

纹理方式(判别位为 `IMAGE`)只预留:载荷指针随控件缓冲传入, 没有消费实现, 各件在该方式下不产出像素。

---

## pfnListWidget 类型 (Controls::Widgets::Base)

列表行件的函数指针类型:把一行数据画成一行。

### 语法

```cpp
using pfnListWidget = int64_t(*)(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem);
```

### 参数

`[in] id`

类型: **int**

行项 ID, 行件用作项 ID(压入 ID 栈), 调用方保证同一列表内唯一。

`[in] pos` / `[in] size`

类型: **vec2**

行项矩形(部件通用参数)。行件自行把光标定位到 `pos`。

`[in] way`

类型: **CONST PaintWay&**

绘制方式。行件配色方案由调用方构造一次后逐行送入。

`[in] lpItem`

类型: **VOID\***

该行数据首址。结构与长度由行件自己约定(列表侧只按"首址 + 单项目长度"步进); **可写** ——
行件可以改行数据(例如写回文本缓冲或选中索引)。

### 返回值

类型: **int64_t**

本行的返回约定(`0` = 无操作), 位布局与各件产出见 `InputText` 的注解。行件只报结论, 由调用方收集后统一
折算(被携带 ⇒ 结论落搭载段)。

### 注解

行件是"被携带"的使用形态:它不决定结论最终落到操作码的哪一段, 也不自行回调事件 —— 收集与回调归列表。
`lpItem` 可为空指针, 行件须自行判定并按"本行不画"处理。

---

## ListSpec 结构 (Controls::Widgets::Command)

列表命令的描述符:调用方按自己类型的缓冲约定解析数据后整份构造, 交给 `List` 一次画完。

### 语法

```cpp
struct ListSpec
{
    int            id;
    vec2           pos;
    vec2           size;
    FLOAT          fRowH;
    DWORD          dwSelected;
    DWORD          dwCount;
    Palette        palette;
    DWORD          crShell;
    pfnListWidget  pfnItem;
    VOID*          lpItemBase;
    size_t         cbItem;
    pfnListWidget  pfnSpecial;
    VOID*          lpSpecial;
    size_t         cbSpecial;
    DWORD          dwSpecialFlag;
    pfnCtrlEvent   pfnEvent;
    VOID*          pEventObj;
    void*          pEventData;
    LPCSTR         label;
    FLOAT          fRounding;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `id` | 列表 ID(绘制时压入 ID 栈)。 |
| `pos` / `size` | 列表矩形(左上角为窗口本地坐标)。 |
| `fRowH` | 行高; 控件取参考尺寸的控件高。 |
| `dwSelected` | 选中项(未选 = 专用哨兵值); 命令部件不消费。 |
| `dwCount` | 项目数量。 |
| `palette` | 行件配色方案(调用方按列表所属控件层次构造)。 |
| `crShell` | 列表壳体底色(调用方按列表所属控件层次采样)。 |
| `pfnItem` | 行件; 为空则不绘制任何行, 且本件直接返回 `0`。 |
| `lpItemBase` | 项目数据首址; 为空时按项目数 `0` 处理(仍可画特殊行)。 |
| `cbItem` | 单项目数据长度, 用作逐项步进。 |
| `pfnSpecial` | 特殊行行件; 为空则无特殊行。 |
| `lpSpecial` | 特殊行数据首址。 |
| `cbSpecial` | 特殊行数据长度; 命令部件不消费, 由行件按自身约定解释行块。 |
| `dwSpecialFlag` | 特殊行标志: 低 16 位 = 位置(在前 / 在后), 高 16 位 = 高度(像素)。 |
| `pfnEvent` | 事件回调; 为空则无事件出口。 |
| `pEventObj` | 回调的控件对象参数(通常 = 控件信息指针)。 |
| `pEventData` | 回调的附加数据参数(通常 = 控件缓冲)。 |
| `label` | 列表子窗标签(稳定 ID 的一部分); 为空取内部默认标签。 |
| `fRounding` | 列表子窗圆角(取当前帧圆角 = 设计内边距)。 |

### 注解

描述符是纯输入:`List` 只读它, 不保存、不改写其中任何字段(行数据经由 `lpItemBase` + `cbItem` 交给行件,
行件可以改行数据本身)。行件被逐行以相同 `palette` 调用 ⇒ 一份列表内各行配色一致。

特殊行只有一行:位置由标志低 16 位决定(在前取行号 `0`, 在后取项目数), 高度取标志高 16 位; 它不占项目
数据区, 数据来自 `lpSpecial`。

回调参数 `param` 由 `List` 按来源行号给出(重排结论时为源行号), 不由调用方提供。

---

## ListPipe 结构 (Controls::Widgets::Pipe)

列表管道的描述:调用方每次送入一行时携带, 部件不持有 ⇒ 不构成部件状态。

### 语法

```cpp
struct ListPipe
{
    int      id;
    vec2     pos;
    vec2     size;
    FLOAT    fRowH;
    DWORD    dwCount;
    DWORD    crShell;
    LPCSTR   label;
    FLOAT    fRounding;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `id` | 列表 ID(送入方控件 id)。 |
| `pos` / `size` | 列表矩形(左上角为父窗口本地坐标)。 |
| `fRowH` | 行高; 控件取参考尺寸的控件高。 |
| `dwCount` | 项目数量; 仅用于判定是否需要为滚动条让位。 |
| `crShell` | 列表壳体底色(调用方按列表所属控件层次采样)。 |
| `label` | 列表子窗标签(稳定 ID 的一部分, 同帧多次进入同一子窗口靠它); 为空取内部默认标签。 |
| `fRounding` | 列表子窗圆角(取当前帧圆角)。 |

### 注解

容器状态(滚动、裁剪、累积高度)由列表子窗口按稳定标签提供 —— 同帧多次进入同一子窗口等于在已有空间上
继续绘制, 因此管道式不需要保存行游标;业务状态(行游标、几何递增、结论累积)由调用方自己维护。

行件配色不由本结构携带:它经 `ListRow` 的绘制方式参数逐次传入。

---

## MakePaletteWay 函数 (Controls::Widgets::Base)

构造配色方案绘制方式。

### 语法

```cpp
static inline PaintWay MakePaletteWay(
  [in] CONST Palette& palette
);
```

### 参数

`[in] palette`

类型: **CONST Palette&**

成品配色方案(由调用方按控件层次采样后传入)。按值拷入返回值, 调用后即可失效。

### 返回值

类型: **PaintWay**

判别位 = `PALETTE`、载荷 = 该配色方案的绘制方式; 值返回 ⇒ 调用点可直接写作实参。

### 注解

这是构造 `PALETTE` 绘制方式的唯一入口:调用点不手写联合体赋值, 以免判别位与载荷不一致。
判别位为 `IMAGE` 的方式没有对应的构造入口(该形态未落地)。

---

## StateColorU32 函数 (Controls::Widgets::Base)

把状态色换算为绘制列表可用色。

### 语法

```cpp
static inline ImU32 StateColorU32(
  [in] CONST StateColors& crs,
  [in] WidgetState        state
);
```

### 参数

`[in] crs`

类型: **CONST StateColors&**

一个色组(常态 / 悬停 / 激活 / 禁用四态)。

`[in] state`

类型: **WidgetState**

状态位, 决定取哪一态(优先级见 `WidgetState` 的注解)。

### 返回值

类型: **ImU32**

绘制列表可用的颜色值, 已按当前样式的透明度乘算。

### 注解

状态色是原始打包值, 直接交给绘制列表会漏掉样式透明度; 需要落笔的颜色一律经本函数换算。
本函数只读全局样式, 不改任何状态。

---

## ItemDisabled 函数 (Controls::Widgets::Base)

读最后一个项的禁用标志。

### 语法

```cpp
static inline BOOL ItemDisabled();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 最后提交的项本帧被禁用; `FALSE` = 未禁用。

### 注解

判定依据是项标志里的禁用位, 而不是本模块状态位:需要把"库侧禁用"折算成部件状态
(`WIDGET_STATE_DISABLE`)时用之。须紧接在该项提交之后调用, 否则读到的是其它项的标志。

---

## CenterOffset 函数 (Controls::Widgets::Base)

计算容器与内容的居中偏移量。

### 语法

```cpp
static inline FLOAT CenterOffset(
  [in] FLOAT parent,
  [in] FLOAT child
);
```

### 参数

`[in] parent`

类型: **FLOAT**

容器尺寸(矩形高、声明高等)。

`[in] child`

类型: **FLOAT**

内容尺寸(设计字号、绘制尺寸等)。

### 返回值

类型: **FLOAT**

`(parent - child) / 2`, 且不小于 `0`。内容不小于容器时返回 `0`(不出现负偏移)。

### 注解

这是"容器与内容居中偏移量"的唯一口径, 全层两处消费:文本与图元的垂直居中量、原生帧的垂直内边距。
下限 0 意味着内容比容器大时偏移为 0, 内容自容器原点起排并被裁剪(不会上越界)。

---

## ExitSign 函数 (Controls::Widgets::Base)

关闭按钮部件:两段交叉线符号 + 悬停底。

### 语法

```cpp
BOOL ExitSign(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] LPCSTR          label,
  [in] FLOAT           fPadding,
  [in] FLOAT           thickness
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

按钮矩形(窗口本地坐标)。矩形负责命中区, 符号在矩形内居中。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 悬停底取底色的悬停态, 符号取底色的前景态。

`[in] label`

类型: **LPCSTR**

按钮标签(命中项名)。为 `NULL` 时取内部默认标签。

`[in] fPadding`

类型: **FLOAT**

内边距:符号边长 = 矩形短边让出 `4 × fPadding`, 再在矩形内居中。

`[in] thickness`

类型: **FLOAT**

交叉线线宽(像素)。

### 返回值

类型: **BOOL**

`TRUE` = 本帧被点击; `FALSE` = 未点击, 或绘制方式不是配色方案(不产出像素)。

### 注解

本件自带一枚覆盖整个矩形的命中项(不可见按钮), 悬停时补一层悬停底色 —— 因此调用点无需再补命中项。
符号与底的取色都来自配色方案, 不读库侧样式槽。

---

## InputText 函数 (Controls::Widgets::Base)

帧类单行文本件:底与边框由原生帧绘制, 帧高对齐声明高。

### 语法

```cpp
int64_t InputText(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] CHAR*           lpText,
  [in] size_t          nTextCap,
  [in] LPCSTR          lpHint
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。`size.y` 决定帧高(垂直内边距按它反算), `size.x` 决定项宽。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 底色、悬停色、激活色、边框色与文本色都取该配色方案。

`[in] lpText`

类型: **CHAR\***

文本缓冲。为 `NULL` 时终止绘制(返回 `0`)。可编辑模式下由输入项就地改写。

`[in] nTextCap`

类型: **size_t**

缓冲容量。大于 `0` = 编辑模式; 等于 `0` = 只读模式(按当前字符串长度提交, 不会写回缓冲, 也不可能产生
提交结论)。编辑模式下容量须覆盖缓冲实际大小。

`[in] lpHint`

类型: **LPCSTR**

空缓冲时的占位提示。只读模式不显示; 为 `NULL` 时亦不显示。

### 返回值

类型: **int64_t**

部件返回约定(`0` = 无操作)。本件可能置位:编辑结束且有改动 ⇒ 专属段 `OPER_EDIT_SUBMIT`;
激活 ⇒ 公共位图 `OPER_COMMON_ACTIVE`。终止绘制(缓冲为空、矩形非法、绘制方式不是配色方案)时返回 `0`。

位布局 = 四位字段:`bit[15:0]` 专属段码(段内值, 互斥结论, `0` = 无操作);`bit[31:16]` 公共位图(公共标志
的位或, `0` = 无);`bit[47:32]` 附属数据 1;`bit[63:48]` 附属数据 2。附属数据两段只有列表重排结论使用
(目标插入位置与源行号), 其余各件恒为 `0`。

各件的产出清单:`InputScalar` 值变化 ⇒ `OPER_EDIT_SUBMIT`;`Combo` 选项变化 ⇒ `OPER_EDIT_CHANGED`;
两段件输入提交 ⇒ `OPER_CUSTOM_IA_INPUT_SUBMIT`、右段按钮点击 ⇒ `OPER_CUSTOM_IA_BTN_CLICKED`;
两段件下拉变化 ⇒ `OPER_CUSTOM_CA_COMBO_CHANGED`、右段按钮点击 ⇒ `OPER_CUSTOM_CA_BTN_CLICKED`;
`Button` 被点击 ⇒ 公共位图 `OPER_COMMON_CLICKED`;`SingleSlider` 值改变与 `CheckBox` 勾选态改变 ⇒
`OPER_EDIT_CHANGED`;激活一律进公共位图 `OPER_COMMON_ACTIVE`(不用专属段的激活码);列表拖拽投放 ⇒
专属段 `OPER_LIST_ROW_MOVED` + 附属数据 1(目标插入位置) + 附属数据 2(源行号)。

### 注解

目标形态是"单件即整控件":调用点把本件当整枚控件画, 本件自己推入原生帧所需的样式槽(底色三态、边框、
文本色)与垂直内边距, 项提交后不再自绘外框。

- 帧类件的共用口径:帧高 = 设计字号 + 2 × 垂直内边距, 垂直内边距取 `max(1, CenterOffset(声明高, 设计字号))`
  ⇒ 帧高恰为声明高, 文字随之垂直居中;水平内边距取样式当前值, 项自声明矩形左上起排。
- 边框画法分两类:本件由原生帧绘制(画线与否取决于宿主的边框宽度设置);`Combo` 与两段件则补画 1 像素
  边框(按项状态取色, 不受该设置约束)。
- 编辑中(项处于激活状态)会计入部件层的"编辑中计数", 控件层因此可判定 IME 开关(见 `HasActiveInput`)。
- 只读与编辑的分界只看容量:容量为 `0` 时提交长度按当前字符串算, 不写回。

---

## InputTextMulti 函数 (Controls::Widgets::Base)

帧类多行文本件:项尺寸取声明矩形, 不做单行件那样的垂直居中。

### 语法

```cpp
int64_t InputTextMulti(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] CHAR*           lpText,
  [in] size_t          nTextCap,
  [in] LPCSTR          lpHint
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。`size` 直接作为多行项的尺寸(多行项自带文本内距与滚动)。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 底色三态、边框色与文本色都取该配色方案。

`[in] lpText`

类型: **CHAR\***

文本缓冲。为 `NULL` 时终止绘制(返回 `0`)。

`[in] nTextCap`

类型: **size_t**

缓冲容量。大于 `0` = 编辑模式; 等于 `0` = 只读模式(按当前字符串长度提交, 不写回)。

`[in] lpHint`

类型: **LPCSTR**

占位提示。本件不消费:多行项没有带提示的变体, 该参数只登记不生效;传 `NULL` 与传文本等价。

### 返回值

类型: **int64_t**

部件返回约定(`0` = 无操作)。置位口径与单行件一致:失焦且有改动 ⇒ 专属段 `OPER_EDIT_SUBMIT`;
激活 ⇒ 公共位图 `OPER_COMMON_ACTIVE`。终止绘制时返回 `0`。

### 注解

项内回车是换行、不是提交 ⇒ 提交语义与单行件相同(失焦且有改动才算)。因为项尺寸就是声明矩形, 本件不推入
垂直内边距, 也不做垂直居中。

编辑中的计数累计与单行件一致。

---

## InputScalar 函数 (Controls::Widgets::Base)

帧类数值件:底与边框由原生帧绘制, 文字经原生内边距居中。

### 语法

```cpp
int64_t InputScalar(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] VOID*           pData,
  [in] ImGuiDataType   eType,
  [in] LPCSTR          lpFormat,
  [in] LPCSTR          lpHint
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。`size.y` 决定帧高, `size.x` 决定项宽。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 底色三态、边框色与文本色都取该配色方案。

`[in] pData`

类型: **VOID\***

数值数据首址, 类型由 `eType` 解释。为 `NULL` 时终止绘制(返回 `0`)。编辑由输入项就地写回。

`[in] eType`

类型: **ImGuiDataType**

数值类型(决定读写宽度与解析方式)。须与 `pData` 实际类型一致。

`[in] lpFormat`

类型: **LPCSTR**

显示与解析格式。为 `NULL` 时取库侧默认格式。

`[in] lpHint`

类型: **LPCSTR**

占位提示。本件不消费:数值项没有带提示的变体, 该参数只登记不生效。

### 返回值

类型: **int64_t**

部件返回约定(`0` = 无操作)。值变化 ⇒ 专属段 `OPER_EDIT_SUBMIT`;激活 ⇒ 公共位图
`OPER_COMMON_ACTIVE`。终止绘制时返回 `0`。

### 注解

输入项按十进制字符过滤, 因此只适合数值型缓冲;垂直内边距与单行文本件同口径(帧高恰为声明高)。
编辑中的计数累计与前两件一致。

---

## Combo 函数 (Controls::Widgets::Base)

帧类下拉件:原生帧底 + 补画边框, 展开项表与两段件同源。

### 语法

```cpp
int64_t Combo(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] LPCSTR*         lpItems,
  [in] INT             nCount,
  [in] INT&            nIndex
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。`size.y` 决定帧高与展开列表的行高, `size.x` 决定项宽。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 底色三态、边框色与文本色都取该配色方案。

`[in] lpItems`

类型: **LPCSTR\***

选项文本表(以 `nCount` 为准, 不要求以空指针结尾)。为 `NULL` 时本件不产出像素(返回 `0`)。

`[in] nCount`

类型: **INT**

选项数量。小于等于 `0` 时本件不产出像素(返回 `0`)。

`[in, out] nIndex`

类型: **INT&**

选中索引。传入时越界(小于 `0` 或不小于 `nCount`)按 `0` 夹取并写回;展开列表点选某项时改写为该索引。
指针/引用本身不可为空(引用语义)。

### 返回值

类型: **int64_t**

部件返回约定(`0` = 无操作)。选项变化 ⇒ 专属段 `OPER_EDIT_CHANGED`;激活 ⇒ 公共位图
`OPER_COMMON_ACTIVE`。终止绘制时返回 `0`。

### 注解

边框由本件补画(1 像素、按项状态取色、圆角取当前帧圆角), 不依赖原生帧的边框开关 ⇒ 观感与两段件完全同源。
展开列表的行观感(行高 = 声明控件高、行距 0、文字行内垂直居中)由两件共用的内部件统一提交, 因此两件的
展开列表没有差异。

选中索引的夹取发生在绘制之前:传入越界值时本帧按第 0 项显示并把缓冲改成 `0`(属于对外可观察的写回)。

---

## TimeSpan 函数 (Controls::Widgets::Base)

时段显示件:自绘圆角底 + 起点到终点的时码文本, 无交互项。

### 语法

```cpp
int64_t TimeSpan(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] int64_t         nStartMs,
  [in] int64_t         nEndMs,
  [in] int64_t         nLimitMs
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。矩形非法时本件不产出像素。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 圆角底取底色, 文本取底色面的前景色。

`[in] nStartMs`

类型: **int64_t**

起点(毫秒)。负值归零;大于上限时饱和到上限。

`[in] nEndMs`

类型: **int64_t**

终点(毫秒)。负值归零;大于上限时饱和到上限。

`[in] nLimitMs`

类型: **int64_t**

上限(毫秒)。大于 `0` 时起止值都饱和到该值;小于等于 `0` 时不夹取。

### 返回值

类型: **int64_t**

恒 `0`(本件无交互结论)。绘制方式不是配色方案或矩形非法时同样返回 `0`。

### 注解

时码文本按"时 / 分 / 秒 / 毫秒"四段固定宽度格式给出, 两段以分隔符相连。因为本件不产出可交互项, 仍提交
一枚占位项占住矩形 —— 否则"推进光标而不提交项"会触发库侧的窗口边界扩展告警, 占位项正是为了消除该告警。
文本纵向居中(按设计字号), 横向自矩形左缘内缩一枚内边距。

---

## InputAction 函数 (Controls::Widgets::Base)

两段件:左段单行输入 + 右段按钮, 两段底与边框在项提交后补画。

### 语法

```cpp
int64_t InputAction(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] FLOAT           fBtnW,
  [in] CHAR*           lpText,
  [in] size_t          nTextCap,
  [in] LPCSTR          lpHint,
  [in] LPCSTR          lpBtn
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。右段按钮贴矩形右缘。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 左段底取底色面、右段按钮面取表面色, 文本分别取各自面的前景色。

`[in] fBtnW`

类型: **FLOAT**

右段按钮宽(像素)。贴部件右缘;实际宽度不小于控件高(默认至少见方)。

`[in] lpText`

类型: **CHAR\***

左段文本缓冲。为 `NULL` 时终止绘制(返回 `0`)。

`[in] nTextCap`

类型: **size_t**

左段缓冲容量。与 `lpText` 共同决定输入项模式:两者有效 = 可编辑;仅 `lpText` 有效(容量 `0`)= 只读。

`[in] lpHint`

类型: **LPCSTR**

左段空缓冲占位提示。只读模式不显示;为 `NULL` 时亦不显示。

`[in] lpBtn`

类型: **LPCSTR**

右段按钮文案。为 `NULL` 时取缺省文案 `CTRL_DEFAULT_ACTION_BTN`。

### 返回值

类型: **int64_t**

部件返回约定(`0` = 无操作)。输入提交 ⇒ 专属段 `OPER_CUSTOM_IA_INPUT_SUBMIT`;右段按钮点击 ⇒
专属段 `OPER_CUSTOM_IA_BTN_CLICKED`。终止绘制时返回 `0`。

### 注解

两段的底与外框都由本件补画(通道拆分:先提交左段输入项与右段按钮项, 再在另一层补画两段底与外框)
⇒ 观感不受原生帧边框开关影响。左段文本容量为 `0` 时走只读分支:不可编辑, 但仍可被点击激活,
右段按钮行为与可编辑形态一致。

同一帧内输入提交与按钮点击不会同时发生(输入提交要求失焦, 点击按钮本身就使其失焦, 二者分属相邻两帧)。

---

## ComboAction 函数 (Controls::Widgets::Base)

两段件:左段下拉框 + 右段按钮, 补画口径与 `InputAction` 一致。

### 语法

```cpp
int64_t ComboAction(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] FLOAT           fBtnW,
  [in] LPCSTR*         lpItems,
  [in] INT             nCount,
  [in] INT&            nIndex,
  [in] LPCSTR          lpBtn
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。右段按钮贴矩形右缘。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 左段底取底色面、右段按钮面取表面色。

`[in] fBtnW`

类型: **FLOAT**

右段按钮宽(像素)。贴部件右缘;实际宽度不小于控件高(默认至少见方)。

`[in] lpItems`

类型: **LPCSTR\***

选项文本表。为 `NULL` 时本件不产出像素(返回 `0`)。

`[in] nCount`

类型: **INT**

选项数量。小于等于 `0` 时本件不产出像素(返回 `0`)。

`[in, out] nIndex`

类型: **INT&**

选中索引。越界按 `0` 夹取并写回;展开列表点选时改写。

`[in] lpBtn`

类型: **LPCSTR**

右段按钮文案。为 `NULL` 时取缺省文案 `CTRL_DEFAULT_ACTION_BTN`。

### 返回值

类型: **int64_t**

部件返回约定(`0` = 无操作)。下拉变更 ⇒ 专属段 `OPER_CUSTOM_CA_COMBO_CHANGED`;右段按钮点击 ⇒
专属段 `OPER_CUSTOM_CA_BTN_CLICKED`。终止绘制时返回 `0`。

### 注解

展开列表与 `Combo` 共用同一内部件提交 ⇒ 两件的展开列表观感一致(行高 = 声明控件高、行距 0、文字行内
垂直居中)。补画的底与外框、按项状态选色、圆角口径都与 `InputAction` 相同。

---

## SingleSlider 函数 (Controls::Widgets::Base)

单手柄滑动条:命中区覆盖整个控件矩形, 轨道与手柄自绘。

### 语法

```cpp
int64_t SingleSlider(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in, out] int64_t&   value,
  [in] int64_t         lMin,
  [in] int64_t         lMax
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。命中区与轨道都以此为准。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 轨道取底色、已选段取表面面的前景色、手柄取边框色(悬停或按下时取边框的悬停态)。

`[in, out] int64_t& value`

类型: **int64_t&**

当前值。拖动期间按鼠标横坐标写回, 四舍五入到值域上的整数刻度;值未变时不写回。

`[in] lMin`

类型: **int64_t**

值域下限(含)。

`[in] lMax`

类型: **int64_t**

值域上限(含)。小于等于 `lMin` 时值域非法 ⇒ 本件不产出像素(返回 `0`)。

### 返回值

类型: **int64_t**

`bit[15:0] = OPER_EDIT_CHANGED`(值已改变), 否则 `0`。值域非法或绘制方式不是配色方案时返回 `0`。

### 注解

命中区是覆盖整个控件矩形的无绘制项, 一切判定都经该项(不读全局鼠标态) ⇒ 细槽小柄也容易点中。
按下即跳到该位置, 继续按住即拖动;手柄直径与轨道厚度按控件高的固定比例给, 两者都不外溢控件矩形,
因此不会压到相邻行。手柄与轨道纵向居中。

值域与刻度都由调用方给出(例如 `0` 到 `100` 表示百分比), 本件不做单位换算。

---

## ProgressStatic 函数 (Controls::Widgets::Base)

进度状态显示件:自绘圆角底 + 进度文本。

### 语法

```cpp
BOOL ProgressStatic(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] int64_t         current,
  [in] int64_t         expect,
  [in] FLOAT           fPadding
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 圆角底取底色, 文本取底色面的前景色。

`[in] current`

类型: **int64_t**

当前进度值。

`[in] expect`

类型: **int64_t**

期望值(分母)。为 `0` 时按 0% 处理(不做除零)。

`[in] fPadding`

类型: **FLOAT**

文本内边距(像素)。横向自矩形左缘内缩该值;调用方按当前帧参考尺寸给出。

### 返回值

类型: **BOOL**

恒 `FALSE`(本件无交互结论; 绘制方式不是配色方案时同样返回 `FALSE`)。

### 注解

百分比按"当前值 / 期望值"算, 不做上下限夹取(超出 100% 会按实际比例显示)。文本纵向居中(按设计字号),
横向内缩一枚内边距;圆角底的圆角取当前帧圆角。

本件不产出可交互项, 但仍在末尾提交一枚占位项占住矩形, 用于消除"推进光标而不提交项"的库侧告警。

---

## Button 函数 (Controls::Widgets::Base)

按钮件:三态面 + 面内文字, 自带可交互项。

### 语法

```cpp
int64_t Button(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] LPCSTR          lpText
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。矩形非法时本件不产出像素。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 面三态取表面色的常态 / 悬停 / 激活, 面内文字取表面面的前景色。

`[in] lpText`

类型: **LPCSTR**

按钮文案。为 `NULL` 时视为空串(仍可点击)。

### 返回值

类型: **int64_t**

`bit[31:16] = OPER_COMMON_CLICKED`(完整单击), 否则 `0`。单击判定沿用库侧的按钮语义(按下并在同一项内
松开)。

### 注解

本件自带可交互项(原生按钮), 调用点无需再补命中项;三态面与文字都取配色方案, 不读库侧样式槽,
因此换配色只改配色方案。本件不占用专属段, 点击结论一律进公共位图。

---

## Text 函数 (Controls::Widgets::Base)

文本图元件:部件矩形内垂直居中绘制一行文本, 并裁剪到矩形。

### 语法

```cpp
int64_t Text(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] LPCSTR          lpText
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。`size` 同时是定位基准与裁剪边界;矩形非法时本件不产出像素。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 文本色取底色面的前景色。

`[in] lpText`

类型: **LPCSTR**

文本内容。为 `NULL` 时视为空串。

### 返回值

类型: **int64_t**

恒 `0`(本件不含可交互项)。

### 注解

纵向对齐只有一条口径:矩形顶 + `CenterOffset(矩形高, 设计字号)`, 与内边距无关。横向不越界:长文本被裁到
矩形内, 不外溢到相邻控件;裁剪只作用于本次绘制, 不改变项提交与光标推进 ⇒ 命中项与占位由控件层按需补。

---

## TextMulti 函数 (Controls::Widgets::Base)

块级多行文本件:文本区四边各内缩一枚设计内边距后按宽折行。

### 语法

```cpp
int64_t TextMulti(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] LPCSTR          lpText
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。`size` 同时是文本区的基准与裁剪边界;矩形非法时本件不产出像素。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 文本色取底色面的前景色。

`[in] lpText`

类型: **LPCSTR**

文本内容。为 `NULL` 时视为空串。

### 返回值

类型: **int64_t**

恒 `0`(本件不含可交互项, 文本项自身即占位项)。

### 注解

内距四边同取一枚设计内边距;内距超过矩形短边一半时按短边一半夹取, 以保证文本区恒有可用宽高
(夹取后文本区仍小于 1 像素时本件不产出像素)。

可排行数 = 文本区高 ÷ 设计字号, 超出部分被裁剪矩形截断:不滚动、不省略号、不再折出矩形。折行由库侧文本层
完成, 本件只给出折行宽度与裁剪范围。声明高由调用方按"整数倍控件高"给出, 部件只消费 `size.y`。

---

## CheckBox 函数 (Controls::Widgets::Base)

复选框件:方框、勾与文字由原生控件绘制, 勾选态经引用写回。

### 语法

```cpp
int64_t CheckBox(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in, out] BOOL&      IsChecked,
  [in] LPCSTR          lpLabel
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。`size.y` 决定方框边长(方框边 = 帧高 = 声明高);矩形非法时本件不产出像素。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 方框面取底色三态、描边取边框色、勾与文字取底色面的前景色。

`[in, out] BOOL& IsChecked`

类型: **BOOL&**

勾选态。点击即翻转并写回;未点击时保持传入值。

`[in] lpLabel`

类型: **LPCSTR**

标签文本。为 `NULL` 或空串时用内部默认标签(此时标签不显示文本, 只为项提供名字)。

### 返回值

类型: **int64_t**

`bit[15:0] = OPER_EDIT_CHANGED`(勾选态已改变), 否则 `0`。绘制方式不是配色方案或矩形非法时返回 `0`。

### 注解

本件自带可交互项(原生复选框) ⇒ 命中项与状态写回都由它完成, 调用点无需再补。勾选态只在点击发生时写回,
写回值已被归一化为 `TRUE` / `FALSE`。

---

## RenderSurface 函数 (Controls::Widgets::Base)

渲染面件:有纹理时等比居中绘制, 无纹理时画占位面。

### 语法

```cpp
BOOL RenderSurface(
  [in] int             id,
  [in] ImVec2          pos,
  [in] ImVec2          size,
  [in] CONST PaintWay& way,
  [in] CONST VOID*     pTexture,
  [in] INT32           texWidth,
  [in] INT32           texHeight
);
```

### 参数

`[in] id`

类型: **int**

部件 ID(压入 ID 栈), 调用方保证唯一。

`[in] pos` / `[in] size`

类型: **ImVec2**

部件矩形(窗口本地坐标)。纹理绘制区与裁剪基准。

`[in] way`

类型: **CONST PaintWay&**

绘制方式; 占位面的底取底色、描边取边框色、文案取底色面的前景色。

`[in] pTexture`

类型: **CONST VOID\***

纹理引用(由调用方持有, 本件不管理其生命周期)。为 `NULL` 时画占位面。

`[in] texWidth` / `[in] texHeight`

类型: **INT32**

纹理像素尺寸。任一小于等于 `0` 时画占位面。

### 返回值

类型: **BOOL**

`TRUE` = 已按纹理绘制;`FALSE` = 无图(已画占位面, 调用方自行补区域占位项), 或绘制方式不是配色方案。

### 注解

有图时按"铺满矩形且保持长宽比"等比缩放并居中, 只改绘制区不改控件矩形。无图时的占位面与矩形控件的取色
口径一致, 并居中画一枚占位文案;占位面的圆角取当前帧圆角。

本件不提交可交互项(有无图都不提交) ⇒ 需要点击时由控件层补命中项。

---

## InputAction 函数 (Controls::Widgets::Base · 列表行重载)

列表行件:解包行数据后转单件实现, 行首拖拽把手占位后行内容右移。

### 语法

```cpp
int64_t InputAction(
  [in] int             id,
  [in] vec2            pos,
  [in] vec2            size,
  [in] CONST PaintWay& way,
  [in] VOID*           lpItem
);
```

### 参数

`[in] id`

类型: **int**

行项 ID(既作行件项 ID, 也作拖拽辅助的 ID 与源行号)。调用方须传入本行行号。

`[in] pos` / `[in] size`

类型: **vec2**

行项矩形(部件通用参数)。行首拖拽手柄占一条, 行内容按手柄宽度右移。

`[in] way`

类型: **CONST PaintWay&**

绘制方式。行件配色方案由调用方构造一次后逐行送入。

`[in] lpItem`

类型: **VOID\***

该行数据首址(行数据块)。为 `NULL`, 或行数据未初始化(文本区无效)时, 本行不画并返回 `0`。

### 返回值

类型: **int64_t**

行件返回约定(`0` = 无操作):行内结论(输入提交 / 右段按钮点击)照单件口径;若本行发生拖拽投放, 则返回
重排结论(专属段 = 行移动、附属数据 1 = 目标插入位置、附属数据 2 = 源行号), 且**投放交付优先于行内结论**
(同一帧二者不会同时发生)。

### 注解

按钮宽按按钮文案折算(依文本宽度模块; 部件再按不小于控件高取宽), 文本容量取行数据内文本区的容量常量 ⇒ 本行恒为可编辑形态。
行首手柄与整行放置目标都由行件内部完成:手柄只登记拖拽并自绘图标, 放置目标按鼠标落在行的上半或下半决定
插入位置并在该侧自绘插入线。

落点换算走控件层的公开件 `ListRowDropOper`(目标插入位置只随本行与上/下半而定), 本件不自行换算。

---

## InputActionRO 函数 (Controls::Widgets::Base · 列表行重载)

列表行件只读形态:文本容量按 `0` 传入, 单件走只读分支。

### 语法

```cpp
int64_t InputActionRO(
  [in] int             id,
  [in] vec2            pos,
  [in] vec2            size,
  [in] CONST PaintWay& way,
  [in] VOID*           lpItem
);
```

### 参数

`[in] id`

类型: **int**

行项 ID(既作行件项 ID, 也作拖拽辅助的 ID 与源行号)。调用方须传入本行行号。

`[in] pos` / `[in] size`

类型: **vec2**

行项矩形。行内容按手柄宽度右移。

`[in] way`

类型: **CONST PaintWay&**

绘制方式,由调用方逐行送入。

`[in] lpItem`

类型: **VOID\***

该行数据首址(行数据块)。为 `NULL` 或行数据未初始化时本行不画并返回 `0`。

### 返回值

类型: **int64_t**

行件返回约定(`0` = 无操作)。不可编辑, 但仍可被点击激活 ⇒ 公共位图可含激活标记;右段按钮行为与可编辑
形态一致;投放交付优先于行内结论。

### 注解

与可编辑形态的唯一差异是文本容量按 `0` 传入 ⇒ 单件走只读分支(不写回文本, 也不可能产生提交结论)。
按钮缺省文案与可编辑形态不同(只读形态给占位短横)。

---

## ComboAction 函数 (Controls::Widgets::Base · 列表行重载)

列表行件:解包选项表后转单件实现, 触发结论打包返回。

### 语法

```cpp
int64_t ComboAction(
  [in] int             id,
  [in] vec2            pos,
  [in] vec2            size,
  [in] CONST PaintWay& way,
  [in] VOID*           lpItem
);
```

### 参数

`[in] id`

类型: **int**

行项 ID(既作行件项 ID, 也作拖拽辅助的 ID 与源行号)。调用方须传入本行行号。

`[in] pos` / `[in] size`

类型: **vec2**

行项矩形。行内容按手柄宽度右移。

`[in] way`

类型: **CONST PaintWay&**

绘制方式,由调用方逐行送入。

`[in] lpItem`

类型: **VOID\***

该行数据首址(行数据块:选项表、数量、选中索引)。为 `NULL` 或行数据未初始化(无选项表)时本行不画并返回 `0`。

### 返回值

类型: **int64_t**

行件返回约定(`0` = 无操作):下拉变化与右段按钮点击照单件口径;投放交付优先于行内结论。

### 注解

右侧按钮文案取自行数据(为空时用内部默认文案), 按钮宽按文案折算(部件再按不小于控件高取宽)。展开列表与单件的下拉件同源,
选中索引经行数据写回。

---

## List 函数 (Controls::Widgets::Command)

命令部件:按列表描述符一次画完, 逐行收集结论并在末步回调。

### 语法

```cpp
int64_t List(
  [in] CONST ListSpec& spec
);
```

### 参数

`[in] spec`

类型: **CONST ListSpec&**

列表描述符(字段见 `ListSpec`)。行件为空、或列表矩形非法时本件直接返回 `0`(不产出像素、不回调);
项目数据首址为空时按项目数 `0` 处理(仍可画特殊行)。

### 返回值

类型: **int64_t**

本帧收集到的操作码(`0` = 无操作, 且此时不回调)。收集口径 = **取最后一个非 0 值的行结论**;
行号(或重排的源行号)经回调的 `param` 送出。

### 注解

六步固定次序:① 压入列表样式(子窗圆角 + 壳体底色);② 创建列表子窗;③ 算行几何并逐项绘制
(行宽 = 子窗可用宽减两侧内距;行数超出列表高时再让出一枚内距给滚动条, 下限 1 像素;特殊行按标志决定画在
首行之前或末行之后);④ 关列表子窗;⑤ 弹出样式;⑥ 执行回调(`oper` 非 `0` 且回调非空时调用一次)。

行件结论被携带 ⇒ 落搭载段:重排结论进专属段并带回附属数据(目标插入位置与源行号), 普通行内事件进搭载段、
`param` = 本行行号。本件不解释行数据结构 —— 行数据只以"首址 + 单项目长度"表达, 特殊行的存在与位置由
描述符字段表达。

命令式与管道式并存:本件把整份数据一次画完, 容器状态与业务状态都留在本件内;需要调用方逐行控制时改用
`ListRow`。

---

## ListRow 函数 (Controls::Widgets::Pipe)

管道部件:逐行送入并绘制一行。

### 语法

```cpp
int64_t ListRow(
  [in] CONST ListPipe&    pipe,
  [in] int                nRowId,
  [in] vec2               posRow,
  [in] FLOAT              fRowH,
  [in] VOID*              lpItem,
  [in] pfnListWidget      pfnItem,
  [in] CONST PaintWay&    way
);
```

### 参数

`[in] pipe`

类型: **CONST ListPipe&**

列表管道描述(字段见 `ListPipe`)。列表尺寸非法时本件直接返回 `0`。

`[in] nRowId`

类型: **int**

本行行号。行件用它作项 ID;特殊行取 `0` 或项目数。

`[in] posRow`

类型: **vec2**

本行矩形左上角(父窗口本地坐标)。行游标与几何递增由调用方算好。

`[in] fRowH`

类型: **FLOAT**

本行行高(像素)。

`[in] lpItem`

类型: **VOID\***

该行数据首址,原样交给行件。

`[in] pfnItem`

类型: **pfnListWidget**

行件。为 `NULL` 时本次不绘制行, 但仍进入并退出列表子窗口(维护列表本身, 供"无缓冲的空列表"用)。

`[in] way`

类型: **CONST PaintWay&**

绘制方式。行件配色由调用方构造后逐行送入, 不由管道描述携带。

### 返回值

类型: **int64_t**

行件的返回约定(`0` = 无操作或无行件);列表尺寸非法时返回 `0`。

### 注解

五步固定次序:① 压入列表样式(子窗圆角 + 壳体底色);② 进入同一列表子窗(稳定 ID = 标签);③ 调用行件绘制
本行(行件自行定位到 `posRow`;行宽 = 子窗可用宽减两侧内距, 行数超出列表高时再让出一枚内距);
④ 关列表子窗;⑤ 弹出样式。

容器状态归子窗口(同帧重复进入同一子窗口等于在已有空间上继续绘制), 业务状态(行游标、几何递增、结论累积)
归调用方 ⇒ 本件自身无状态。返回的行件结论由调用方累积后统一回调(被携带 ⇒ 落搭载段)。

---

## UpdateContext 函数 (Controls::Widgets)

更新部件层上下文(每帧渲染布局之前调用一次), 并复位编辑中计数。

### 语法

```cpp
BOOL UpdateContext(
  [in] CONST AutoSize& refsize,
  [in] CONST ProjCR*   pProj
);
```

### 参数

`[in] refsize`

类型: **CONST AutoSize&**

当前参考尺寸。本函数取其中的像素值作为本帧的字号、内边距、间隔与控件高。

`[in] pProj`

类型: **CONST ProjCR\***

投影配色。为 `NULL` 表示未注入:列表拖动辅助的取色返回 `0`(该处不着色), 其余绘制不受影响。
指针由调用方持有, 须覆盖本帧的全部绘制。

### 返回值

类型: **BOOL**

恒 `TRUE`(无失败路径)。

### 注解

这是部件层上下文唯一的写点, 每帧一次;同时完成三件事:复位编辑中计数、写入参考尺寸、计算当前帧圆角
(圆角 = 设计内边距)。

圆角是非根窗口级圆角的唯一来源:矩形面与占位面、时段件的底、两段件与下拉件的补画面与外框、进度件的底、
列表拖动插入线、列表子窗的圆角都取该值。根窗口的圆角不在本层(仍取设计间隔)。

调用时机 = 每帧渲染布局之前;控件层的同名上下文函数内部转调本函数(每帧单一写点), 渲染出口的调用点因此
不变。本函数必须在任何部件绘制之前调用, 否则本帧上下文与圆角沿用上一帧的值。

---

## HasActiveInput 函数 (Controls::Widgets)

查询本帧是否存在编辑中的输入项。

### 语法

```cpp
BOOL HasActiveInput();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 本帧存在编辑中的输入项(计数非 `0`);`FALSE` = 无。

### 注解

计数由三个文本输入件在项处于激活状态时累加, 每帧由 `UpdateContext` 复位 ⇒ 本值反映"本帧到目前为止"的
状态, 控件层据此判定是否关闭输入法。查询本身不改变计数。

读的是部件层上下文, 因此须在 `UpdateContext` 之后调用;整帧未更新上下文时读到的是上一帧的残留值。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/render/imgui/control/Widgets.h`(`src\framework\render\imgui\control\Widgets.h`) |
| 实现 | `src\framework\render\imgui\control\Widgets.cpp`;少量运算符与 `CenterOffset` 在头文件内 |
| 命名空间 | `Controls::Widgets`(三型分居 `Base` / `Command` / `Pipe` 子命名空间) |
| 依赖 | `minwindef.h` · `framework/render/imgui/control/Widgets.h` · `framework/base/ControlSpec.h` · `stdint.h` · `framework/base/FuncPtr.h` · `imgui.h` · `ucrt` · `framework/base/CtrlBuffer.h` · `framework/base/TextWidth.h` · `framework/render/imgui/control/Controls.h` · `framework/base/BasicDesign.h` |
| 平台 | Windows 全版本 |
| 线程安全 | 否(在 ImGui 已开帧的当前窗口内绘制) |
