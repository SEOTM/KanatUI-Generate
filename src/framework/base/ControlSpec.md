# ControlSpec 模块 (src\framework\base\ControlSpec.h)

## 概述

控件类型的**身份真源**与**逐类型纯约定**:控件类型(枚举名 ↔ 字符串名,同一份表生成)、控件信息结构
`CtrlInfo` 与"层次 → 投影层号"换算、控件事件名约定(含"委托父容器"特殊事件)、操作码(OPER)三段系统、
窗口位置参数槽、列表缓冲头。

本模块属 **framework 层 · 控件库的约定面**,只含编译期常量、内联函数与两枚纯数据结构(不含分配、绘制、
状态)。依赖方向一律向下:向上被 `Controls`(绘制与装配)、`Widgets`(部件)、
应用侧与渲染出口共同引用;自身只依赖 `framework/base/mem/MemoryManager.h`(`BufferInfo`)与 `framework/base/FuncPtr.h`
(`pfnCtrlEvent`)。

**不做什么**:缓冲容量的取值、缓冲布局与生命周期 → `CtrlBuffer`;文本区取址 → `CtrlBuffer`;渲染分派表与绘制 → `Controls`;
应用侧类型信息与业务意图码 → 应用层自己的实现。控件类型的属性查询(`CtrlTypeIsList` / `CtrlTypeIsCompound` /
`CtrlTypeIsRawGraphic` / `CtrlTypeHandlesClickItself` / `CtrlTypeIsTextKind`)属本模块(纯类型语义,不接触缓冲)。

**语义边界(不可混用)**:`CTRL_TYPE_INVALID` = 未使用槽位(布局器保守分配时未用到)⇒ 不建缓冲、渲染不可见;
`CTRL_TYPE_UNKNOWN` = 文档类型串未命中表 ⇒ 不建缓冲、渲染层画矩形占位、保存按**原串**写回(原串由文档层
保留,本模块不存串)。

**操作码常量按命名空间成组**:组名写成通配形式(如 `OPER_EDIT_*`),组内逐值在该节的成员表列全。
编译期不变量用的校验常量(`OPER_SEG_PROBE`)与 `static_assert` 组不属接口面,不单列;`CtrlInfo` 的尺寸与字段偏移由同文件内的
`static_assert` 钉住,改字段会编译失败。

常量节以"语法 / 值 / 注解 / 要求"四节书写(常量无可传参数,也无返回值),函数与结构 / 枚举节沿用五节骨架。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `CtrlLayerOf` | 由控件层次值得出投影层号(全库唯一换算口径)。 |
| `CtrlLayerStep` | 取某控件类型在层次递推中的步进量。 |
| `GetTypeName` | 由类型枚举取规范类型串。 |
| `GetTypeEnum` | 由类型串取类型枚举(精确匹配)。 |
| `CtrlTypeIsList` | 判断该类型是否列表类控件。 |
| `CtrlTypeIsCompound` | 判断该类型是否复合控件(多枚子项合成)。 |
| `CtrlTypeIsRawGraphic` | 判断该类型是否纯图元控件(不自带可交互项)。 |
| `CtrlTypeHandlesClickItself` | 判断该类型是否由控件库自己派发点击。 |
| `CtrlTypeIsTextKind` | 判断该类型是否文本缓冲控件。 |
| `GetSegment` | 取操作码某一段的段内值(已右移)。 |
| `IsWithin` | 判断操作码某一段是否"有码"。 |
| `MakeCode` | 把三段段内值拼成整字操作码。 |
| `MakeCodeCommon` | 构造只含公共段的操作码整字。 |
| `MakeCodeOwn` | 构造只含专属段的操作码整字。 |
| `GetCommonCode` | 取公共码(剔除搭载标志与预留位)。 |
| `HasCommon` | 测试公共段的位标志。 |
| `GetPayloadKind` | 取搭载标志(`OPER_PAYLOAD_*`)。 |
| `MakeCommon` | 拼公共段段内值(公共码 + 搭载标志)。 |
| `PackWndPos` | 把窗口期望位置打包进事件的通用参数槽。 |
| `UnpackWndPos` | 从事件的通用参数槽解出窗口期望位置。 |
| `CtrlEventIsEntrustParent` | 判定事件名是否为"委托父容器"特殊事件。 |

### 类型

| 类型 | 说明 |
| --- | --- |
| `CtrlType` | 控件类型枚举(取值由类型表行序决定)。 |
| `CTRL_LAYER_STEP` | 层次递推的逐类型步进(`-1` / `0` / `+1`)。 |
| `OPER_SEG` | 操作码的段序号(公共 / 专属 / 搭载)。 |
| `vec2` | 二维坐标(与 `ImVec2` 二进制等价)及其运算符。 |
| `CtrlInfo` | 控件信息:缓冲、层次、事件处理器、父容器与像素几何。 |
| `ListBuffer` | 列表类控件的缓冲头(选中项 / 数量 / 容量 / 行长)。 |

### 常量

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `CTRL_TYPE_TABLE` / `CtrlTypeNames` | 编译期展开(24 行类型表) | 类型身份的单一真源:同一份表同时生成枚举、"枚举 → 字符串名"表与行数断言。 |
| `CTRL_EVENT_ENTRUST_PARENT` | `"CTRL_EVENT_ENTRUST_PARENT"` | 事件字段的特殊值:把本控件的事件绑定至父容器的事件。 |
| `PROJ_LAYER_*` | `8` / `1` / `16` | 投影层号的基准档与上下限(背景层 / 下限 / 上限)。 |
| `OPER_SEG_MASK_*` / `OPER_NONE` | `0xFFFF` / `0xFFFF0000` / `0xFFFFFFFF00000000` / `0x0` | 三段的整字掩码与整字空值。 |
| `OPER_COMMON_PAYLOAD_*` / `OPER_COMMON_CODE_MASK` | `14` / `0xC000` / `0x3FFF` | 公共段内"搭载标志"的移位与掩码,以及公共码位域。 |
| `OPER_PAYLOAD_*` | `0` / `1` / `2` / `3` | 搭载标志取值(未搭载 / 操作码 / 数据 / 混合)。 |
| `OPER_COMMON_*` | `0x0000` … `0x0080` | 公共码位标志(点击 / 激活 / 拖放三组)。 |
| `OPER_EDIT_*` | `0x0001` / `0x0002` / `0x0003` | 编辑族专属码(提交 / 变更 / 取消)。 |
| `OPER_CUSTOM_*` | `0x0001` / `0x0002` | 自定义组合部件的专属码(子空间按控件)。 |
| `OPER_LIST_*` | `0x0001` … `0x0007` | 列表族专属码(行事件 / 新增 / 滚动)。 |
| `OPER_STRUCT_TITLE_CLOSE` | `0x0001` | 结构件族:关闭钮被点击。 |
| `OPER_PROG_UPDATED` 等类型单列码 | `0x0001` / `0x0002` | 上述分类覆盖不到时按类型单列的专属码。 |
| `LIST_*` | `0xFFFFFFFF` / `0` / `1` | 列表选中哨兵与特殊行位置标志。 |

---

## CtrlLayerOf 函数 (ControlSpec)

由控件层次值得出投影层号。

### 语法

```cpp
inline INT32 CtrlLayerOf(INT32 nLayer);
```

### 参数

`[in] nLayer`

类型: **INT32**

控件层次(即 `CtrlInfo::nLayer`),有符号:0 = 窗口底那一档,负 = 更沉,正 = 更亮。结构件恒 `0`。
任意整数都合法 —— 越出层号域时由本函数钳制。

### 返回值

类型: **INT32**

投影层号(1-based),取值 = `PROJ_LAYER_BACKGROUND + nLayer`,并钳到
[`PROJ_LAYER_MIN`, `PROJ_LAYER_MAX`]。返回值恒在层号域内,可直接作为投影轴下标。

### 注解

"层次 → 层号"的**唯一换算口径**:宿主取色与控件库取色必须都经本函数,不得各写一份(两处口径不一致会让
同一控件的底色与描边分属不同层)。

前置条件:`CtrlInfo::nLayer` 已由布局在装配 / 增删时按"父控件层次 + 自身类型步进"递推完毕;
本函数不做几何回溯、不查链结构、不查父链。

钳制是**压平**而非报错:嵌套过深 / 过浅的控件被压到边界层,仍正常绘制;控件库在取色路径上另有一次性
诊断输出,不影响本函数的返回值语义。

纯函数、无状态、无分配。

---

## CtrlLayerStep 函数 (ControlSpec)

取某控件类型在层次递推中的步进量。

### 语法

```cpp
inline INT32 CtrlLayerStep(CtrlType type);
```

### 参数

`[in] type`

类型: **CtrlType**

本控件自身的类型。任意枚举值都可传入(未列出的类型走继承分组)。

### 返回值

类型: **INT32**

步进量,取 `CTRL_LAYER_STEP_*` 之一:`-1` = 域 / 承载面(每嵌套一层下沉一档);`0` = 继承父控件层次;
`+1` = 件 / 操作物(相对父控件上浮一档)。

### 注解

**分组口径**(判据 = 该类型是"供内容存在的区域"还是"用户直接操作的物件"):

| 分组 | 步进 | 覆盖类型 |
| --- | --- | --- |
| 域 / 承载面 | `-1` | `RECT`、`INPUT_*`、`LIST_*`、`COMBO` / `COMBO_ACTION`、`RENDER`、`SLIDER` |
| 件 / 操作物 | `+1` | `BUTTON`、`CHECKBOX` |
| 其余 | `0` | 结构件(`WND` / `TITLE` / `CNTR`)、`TEXT`、`TIMEBAR`、`PROG`、`UNKNOWN`、`INVALID` |

语义是"每层递进方向"而非"相对固定基准的一次性调整" —— 嵌套几层就累进几档,由递推自然给出,不需要另定
累加规则。

**新增类型默认归"继承"**(`0`):要改变层次观感,必须在本函数的 `switch` 里显式加 `case`,不要改其它调用点。

---

## GetTypeName 函数 (ControlSpec)

由类型枚举取规范类型串。

### 语法

```cpp
inline LPCSTR GetTypeName(CtrlType type);
```

### 参数

`[in] type`

类型: **CtrlType**

控件类型枚举。越界值(`< 0` 或 `>= CTRL_TYPE_COUNT`)也接受,由函数给出兜底串。

### 返回值

类型: **LPCSTR**

该枚举对应的规范类型串(静态存储,**恒非 `NULL`**);越界 / 非法枚举返回 `CTRL_TYPE_INVALID` 的串。
返回的串不归调用方所有,不得释放。

### 注解

串与枚举同源:两者由 `CTRL_TYPE_TABLE` 同一份数据生成,行数由 `static_assert` 与 `CTRL_TYPE_COUNT` 对齐
⇒ 不可能出现"枚举有、串没有"的落空。

用途:保存 / 导出时把枚举写回文档类型串、诊断输出、应用侧展示。反查用 `GetTypeEnum`。

---

## GetTypeEnum 函数 (ControlSpec)

由类型串取类型枚举。

### 语法

```cpp
inline CtrlType GetTypeEnum(LPCSTR type);
```

### 参数

`[in] type`

类型: **LPCSTR**

文档声明的类型串(C 串)。传 `NULL` 时返回 `CTRL_TYPE_UNKNOWN`;空串同样走未命中路径。

### 返回值

类型: **CtrlType**

命中的枚举值;**未命中或入参为空**时返回 `CTRL_TYPE_UNKNOWN`(不返回 `INVALID` —— 那是"未使用槽位"的语义)。

### 注解

**精确匹配、区分大小写**(逐项字符串比较),不做前缀 / 忽略大小写 / 别名匹配 ⇒ 类型串必须与
`CtrlTypeNames` 完全一致。

未命中不等于失败:控件照常装配(`CtrlInfo::type` = `CTRL_TYPE_UNKNOWN`),渲染层画矩形占位,编辑层可感知
并把**原串**写回文档 —— 原串由文档层保留,本模块不保存。因此"改类型串"这类误写不会丢失信息。

装载期唯一入口;查表为线性扫描,类型数量固定(24),无需另建索引。

---

## CtrlTypeIsList 函数 (ControlSpec)

判断该类型是否列表类控件。

### 语法

```cpp
inline BOOL CtrlTypeIsList(CtrlType type);
```

### 参数

`[in] type`

类型: **CtrlType**

待判定的控件类型。任意枚举值(含未知 / 未使用槽位)都可传入。

### 返回值

类型: **BOOL**

`TRUE` = 列表类(`CTRL_TYPE_LIST_INPUT` / `CTRL_TYPE_LIST_COMBO`);其余为 `FALSE`。

### 注解

列表类控件的内容由控件库按类型分派绘制(`Controls::ListInput` / `Controls::ListCombo`),其余类型不按
列表缓冲解释。`CTRL_TYPE_LIST`(无缓冲语义的旧列表类型)**不属**本判据 —— 它没有绘制实现。

---

## CtrlTypeIsCompound 函数 (ControlSpec)

判断该类型是否复合控件。

### 语法

```cpp
inline BOOL CtrlTypeIsCompound(CtrlType type);
```

### 参数

`[in] type`

类型: **CtrlType**

待判定的控件类型。

### 返回值

类型: **BOOL**

`TRUE` = 复合控件(输入动作 / 下拉动作 / 列表类);`FALSE` = 单个可交互项(末项即整控件)。

### 注解

复合控件 = 单个控件由多枚不相交的可交互子项合成(输入动作 = 左段输入 + 右段按钮,下拉动作 = 左段下拉 +
右段按钮,列表 = 行内逐枚子项)。

判定依据:这类控件绘制完成后,ImGui 的"末项"只指向最后提交的那一枚子项(通常是右段按钮)⇒ 调用方若直接
用"末项是否被点击"判定整个控件,点在其余子项区域时会判为未点击 ⇒ 调用方须另按控件整体矩形补一次命中判定。

补判定仍限本控件矩形,与 ImGui 的叠加归属不冲突(重叠区由记录方按索引最大者裁决)。

---

## CtrlTypeIsRawGraphic 函数 (ControlSpec)

判断该类型是否纯图元控件。

### 语法

```cpp
inline BOOL CtrlTypeIsRawGraphic(CtrlType type);
```

### 参数

`[in] type`

类型: **CtrlType**

待判定的控件类型。

### 返回值

类型: **BOOL**

`TRUE` = 纯图元(`TEXT` / `RECT` / `UNKNOWN`),需调用方在控件矩形上补一枚无绘制可交互项;
`FALSE` = 自带可交互项,绘制后直接判末项即可。

### 注解

"自带可交互项"的类型(按钮 / 输入 / 下拉 / 滑块 / 进度 / 标题 / 列表)在绘制时就已提交可交互项,调用方
绘制后直接判定末项即可;纯图元类型不自带,须由调用方补一枚无绘制的可交互项(如 `InvisibleButton`)。

`TEXT` 的文本图元项与未知类型的占位项其 ImGui 标识均为 `0` ⇒ 不参与命中判定,故这两类仍归"纯图元"。

---

## CtrlTypeHandlesClickItself 函数 (ControlSpec)

判断该类型是否由控件库自己派发点击。

### 语法

```cpp
inline BOOL CtrlTypeHandlesClickItself(CtrlType type);
```

### 参数

`[in] type`

类型: **CtrlType**

待判定的控件类型。

### 返回值

类型: **BOOL**

`TRUE` = 控件库自身会派发点击,渲染出口须跳过点击判定;`FALSE` = 需渲染出口统一判定并派发。

### 注解

控件库只有三处派发"点击":①按钮(完整单击口径);②绑定了事件的文本 / 矩形(补一枚覆盖控件矩形的无绘制
可交互项,完整单击口径);③标题栏(结构件,不参与预览布局的内容控件判定)。其余类型只在各自的
"提交 / 变更 / 电平"等语义时派发操作码,点击本身不派发。

`TRUE` 的类型若被渲染出口再判一次,同一次点击会被登记两次。

---

## CtrlTypeIsTextKind 函数 (ControlSpec)

判断该类型是否文本缓冲控件。

### 语法

```cpp
inline BOOL CtrlTypeIsTextKind(CtrlType type);
```

### 参数

`[in] type`

类型: **CtrlType**

待判定的控件类型。

### 返回值

类型: **BOOL**

`TRUE` = 文本类(`TEXT` / `BUTTON` / `INPUT_SINGLE` / `INPUT_MULTI` / `INPUT_ACTION` / `TITLE`);
`FALSE` = 其它类型。

### 注解

"文本类"= 缓冲内容为 C 字符串、可通过文本通道编辑的类型。本谓词只回答类型语义;文本区的取址与容量归
`CtrlBuffer.h` 的 `CtrlTextRangeOf`。

两处例外须注意:`INPUT_ACTION` 的输入文本位于载荷内(不在缓冲偏移 0 处,偏移 0 是定长头的 `split`);
`TITLE` 的缓冲即标题文本(标题栏状态走回调操作码,不占缓冲)。

**复选框不属文本类** —— 其显示文字是结构内字段,不参与文本通道;数值类与列表类同理。

---

## GetSegment 函数 (OperCode)

取操作码某一段的段内值。

### 语法

```cpp
constexpr uint64_t GetSegment(uint64_t code, OPER_SEG seg);
```

### 参数

`[in] code`

类型: **uint64_t**

整字操作码。任何 64 位值都合法。

`[in] seg`

类型: **OPER_SEG**

段序号。`OPER_SEG_COUNT` 及更大的值都属非法段,由函数返回 `0`(不报错、不抛异常)。

### 返回值

类型: **uint64_t**

该段的**段内值**(已右移到最低位):公共段 `[15:0]`、专属段 `[31:16]`、搭载段 `[63:32]`;
非法段返回 `0`。

### 注解

"前 / 次 / 后"一律按**从最低位起**理解:公共段在前(低位)、专属段次之、搭载段在后(高位)。这是本系统
最易错的一点 —— 文档与代码注释都按此口径。

段内值只能与**段内常量**比较;与整字比较一律用 `OPER_NONE`。跨段等值比较没有意义(同一数值在不同段
表示不同事实)。

---

## IsWithin 函数 (OperCode)

判断操作码某一段是否"有码"。

### 语法

```cpp
constexpr BOOL IsWithin(uint64_t code, OPER_SEG seg);
```

### 参数

`[in] code`

类型: **uint64_t**

整字操作码。

`[in] seg`

类型: **OPER_SEG**

段序号。非法段(`OPER_SEG_COUNT` 及更大)返回 `FALSE`。

### 返回值

类型: **BOOL**

`TRUE` = 该段段内值非 `0`(`GetSegment` 结果不为 0);`FALSE` = 该段为空码或段号非法。

### 注解

只判"该段有没有码",不判该段内容是否合法 —— 语义校验在消费者侧。

"有搭载"不能只看搭载段的数值:`OPER_PAYLOAD_DATA` 且数据恰为 `0` 时搭载段也是 `0` ⇒ 判有无搭载一律看
搭载标志(`GetPayloadKind`)。

---

## MakeCode 函数 (OperCode)

把三段段内值拼成整字操作码。

### 语法

```cpp
constexpr uint64_t MakeCode(uint16_t common, uint16_t own, uint32_t payload);
```

### 参数

`[in] common`

类型: **uint16_t**

公共段段内值(公共码位标志与搭载标志的组合,通常由 `MakeCommon` 先拼好);`0` = 公共段为空。

`[in] own`

类型: **uint16_t**

专属段段内值(按控件族解释的互斥码);`0` = 该控件无专属码。

`[in] payload`

类型: **uint32_t**

搭载段段内值(从属部件码或约定数据);`0` = 未搭载内容(是否搭载另看公共段的搭载标志)。

### 返回值

类型: **uint64_t**

整字操作码:公共段在低 16 位、专属段在次 16 位、搭载段在高 32 位。三段全为 `0` 时等于 `OPER_NONE`。

### 注解

**三段 = "谁在说话"**:公共段 = 任何一方合意即可用的通用结论(各位含义全库一致);专属段 = **当前回调的
发出者**(当前控件自己)的结论;搭载段 = **被携带的下属**(子部件 / 行件 / 管道内件)的码或数据。

**点对点约定,不是消息总线**:控件 →(`event` 名)→ 绑定表 → 处理函数 是一对一约定 ⇒ 操作码的读者唯一
且已知,不需要运行期"类型 → 命名空间"查表,也不需要 `(类型, 码)` 双键分派表。

各段宽度由形参类型给定(`uint16_t` / `uint32_t`),函数内不再掩码;因此不会出现段间进位污染。

整字为 `OPER_NONE` 表示**非控件来源**(无参帧间挂载 / 直接调用);控件来源至少一段非 0 —— 无码不回调。

---

## MakeCodeCommon 函数 (OperCode)

构造只含公共段的操作码整字。

### 语法

```cpp
constexpr uint64_t MakeCodeCommon(uint16_t common);
```

### 参数

`[in] common`

类型: **uint16_t**

公共段段内值;专属段与搭载段置空。

### 返回值

类型: **uint64_t**

等价于 `MakeCode(common, 0, 0)` 的整字。

### 注解

用于"控件只报通用结论"的场合(如按钮点击、文本 / 矩形被点击)。等价性由编译期断言钉住,调用点无需改动。

---

## MakeCodeOwn 函数 (OperCode)

构造只含专属段的操作码整字。

### 语法

```cpp
constexpr uint64_t MakeCodeOwn(uint16_t own);
```

### 参数

`[in] own`

类型: **uint16_t**

专属段段内值;公共段与搭载段置空。

### 返回值

类型: **uint64_t**

等价于 `MakeCode(0, own, 0)` 的整字。

### 注解

用于"控件只报自己的专属结论、不带通用结论、不搭载"的场合(如进度更新、勾选态改变)。公共段为空意味着
消费者无法从公共段读到点击事实 —— 需要点击事实的类型不要走本构造。

---

## GetCommonCode 函数 (OperCode)

取公共码(剔除搭载标志与预留位)。

### 语法

```cpp
constexpr uint16_t GetCommonCode(uint64_t code);
```

### 参数

`[in] code`

类型: **uint64_t**

整字操作码。

### 返回值

类型: **uint16_t**

公共码位图:公共段段内值与 `OPER_COMMON_CODE_MASK`(`0x3FFF`)按位与的结果 —— 搭载标志(`[15:14]`)
与预留位(`[13:8]` 的高位部分)都不参与。

### 注解

取公共码**必须**经本函数:**禁止整段等值比较**,因为搭载标志占公共段高位 —— 同一枚公共码带不同搭载标志
时段内值不等。

---

## HasCommon 函数 (OperCode)

测试公共段的位标志。

### 语法

```cpp
constexpr BOOL HasCommon(uint64_t code, uint16_t bit);
```

### 参数

`[in] code`

类型: **uint64_t**

整字操作码。

`[in] bit`

类型: **uint16_t**

待测的公共码位标志:单个 `OPER_COMMON_*`,或若干位的按位或(组合测试)。

### 返回值

类型: **BOOL**

`TRUE` = 公共码与该位标志有交集;`FALSE` = 无交集(含公共码为空的情形)。

### 注解

内部先经 `GetCommonCode` 剔除搭载标志,故传入含高位标志的段内值也不会误判。

按位测试对"组合位"成立:同一帧可同时置位单击与双击,消费者按需取用;需要"只判单击"时应显式与本位比较,
不要用"非零即点击"。

---

## GetPayloadKind 函数 (OperCode)

取搭载标志。

### 语法

```cpp
constexpr uint16_t GetPayloadKind(uint64_t code);
```

### 参数

`[in] code`

类型: **uint64_t**

整字操作码。

### 返回值

类型: **uint16_t**

搭载标志,取 `OPER_PAYLOAD_NONE`(0)/ `OPER_PAYLOAD_OPCODE`(1)/ `OPER_PAYLOAD_DATA`(2)/
`OPER_PAYLOAD_MIXED`(3);段内高位两位右移 `OPER_COMMON_PAYLOAD_SHIFT` 得到。

### 注解

**判"有无搭载"一律看本标志**,不要看搭载段是否为 `0`:携带数据时数据本身可能恰为 `0`,
此时搭载段全零但搭载仍然成立。

---

## MakeCommon 函数 (OperCode)

拼公共段段内值(公共码 + 搭载标志)。

### 语法

```cpp
constexpr uint16_t MakeCommon(uint16_t code, uint16_t kind);
```

### 参数

`[in] code`

类型: **uint16_t**

公共码位图(单个 `OPER_COMMON_*` 或若干位的按位或);高位(搭载标志位)会被掩掉。

`[in] kind`

类型: **uint16_t**

搭载标志(`OPER_PAYLOAD_*`);只有低 2 位有效,越界值按低 2 位截取。

### 返回值

类型: **uint16_t**

公共段段内值 = `(code & OPER_COMMON_CODE_MASK) | ((kind & 0x3) << OPER_COMMON_PAYLOAD_SHIFT)`,
可直接作为 `MakeCode` 的 `common` 实参。

### 注解

公共段的内部布局:`[15:14]` = 搭载标志(2 位枚举)· `[13:8]` = 预留 · `[7:0]` = 当前使用的公共码位域
(`OPER_COMMON_CODE_MASK` 按低 14 位掩码,故新增公共码最多可用到第 13 位)。

公共码是**组合位**:段内值 = `1u << n`;公共段移位为 0 ⇒ 段内值即整字掩码,可与整字直接按位与。

---

## PackWndPos 函数 (ControlSpec)

把窗口期望位置打包进事件的通用参数槽。

### 语法

```cpp
inline uint64_t PackWndPos(CONST vec2& pos);
```

### 参数

`[in] pos`

类型: **CONST vec2&**

窗口期望位置:窗口左上角的桌面像素坐标。小数部分被截断为整数(`INT32`),负坐标按有符号解释合法。
`pos` 只在本调用内被读取,不保留引用。

### 返回值

类型: **uint64_t**

可直接作为控件事件回调 `param` 的打包值:低 32 位 = `x`,高 32 位 = `y`(各按有符号 32 位解释)。

### 注解

**只上报、不落地**:标题栏一类窗口级控件用本函数把期望位置放进 `param`,自己不调任何窗口 API、也不改写
位置记录。是否记录由"该控件是否绑定了事件"决定 —— 绑定的处理函数把坐标记进窗口件(`WND`)的 `position`
(即窗口位置记录),渲染出口在下次取得该渲染目标时以该记录为参数结算成实质移动。未绑定 ⇒ 窗口不可拖动。

未处于拖动中(无坐标)时该槽为 `0`,与"位置恰为原点"区分不了 ⇒ 消费者须结合操作码的按住位判断。
与 `UnpackWndPos` 互逆。

---

## UnpackWndPos 函数 (ControlSpec)

从事件的通用参数槽解出窗口期望位置。

### 语法

```cpp
inline vec2 UnpackWndPos(uint64_t param);
```

### 参数

`[in] param`

类型: **uint64_t**

事件回调的通用参数槽,须由 `PackWndPos` 打包(本函数不做来源校验;任意 64 位值都按其字段解释)。

### 返回值

类型: **vec2**

窗口期望位置(桌面像素坐标),`x` 取低 32 位、`y` 取高 32 位,均按有符号 32 位解释后转为浮点。

### 注解

与 `PackWndPos` 完全互逆,**但只对像素整数坐标成立**:打包时小数部分已被截断,回读得到的是整数值
(控件位置的像素口径本就是整数,不构成精度损失)。

非 `PackWndPos` 打包的 `param`(如列表行的源行号、行件搭载数据)不要喂给本函数。

---

## CtrlType 枚举 (ControlSpec)

控件类型枚举。

### 语法

```cpp
enum CtrlType
{
    // 由 CTRL_TYPE_TABLE 逐行展开(此处按表序列出取值)
    CTRL_TYPE_INVALID = 0,
    ...
    CTRL_TYPE_LIST_TIMESPAN = 23,
    CTRL_TYPE_COUNT
};
```

### 成员

| 枚举值 | 值 | 说明 |
| --- | --- | --- |
| `CTRL_TYPE_INVALID` | `0` | 未使用槽位:不建缓冲,渲染层不可见(只占位不画)。 |
| `CTRL_TYPE_UNKNOWN` | `1` | 未知类型串:不建缓冲,渲染层画矩形占位,保存按原串写回。 |
| `CTRL_TYPE_WND` | `2` | 窗口件(基座布局的窗口记录件)。 |
| `CTRL_TYPE_CNTR` | `3` | 容器件(承载内容区域的域)。 |
| `CTRL_TYPE_TITLE` | `4` | 标题栏:缓冲即标题文本,状态经操作码回传。 |
| `CTRL_TYPE_RECT` | `5` | 纯矩形(无文本、无子内容)。 |
| `CTRL_TYPE_TEXT` | `6` | 静态文本。 |
| `CTRL_TYPE_INPUT_SINGLE` | `7` | 单行输入,缓冲 `CTRL_TEXT_BUF`(256 字节)。 |
| `CTRL_TYPE_INPUT_MULTI` | `8` | 多行输入,缓冲 `CTRL_TEXT_MULTI_BUF`(4096 字节)。 |
| `CTRL_TYPE_INPUT_NUMERIC` | `9` | 整数输入,缓冲为 `INT`。 |
| `CTRL_TYPE_INPUT_REAL` | `10` | 实数输入,缓冲为 `DOUBLE`。 |
| `CTRL_TYPE_INPUT_ACTION` | `11` | 输入动作:左段单行输入 + 右段按钮(复合控件)。 |
| `CTRL_TYPE_BUTTON` | `12` | 按钮。 |
| `CTRL_TYPE_LIST` | `13` | 列表(无缓冲语义、无绘制实现 ⇒ 有控件、无像素)。 |
| `CTRL_TYPE_LIST_INPUT` | `14` | 布局列表:行 = 可编辑输入动作行。 |
| `CTRL_TYPE_LIST_COMBO` | `15` | 控件列表:项目行只读,特殊行 = 下拉动作。 |
| `CTRL_TYPE_TIMEBAR` | `16` | 时间条:无缓冲语义,缓冲缺失时按 `0` 值绘制。 |
| `CTRL_TYPE_RENDER` | `17` | 渲染面:纹理由宿主注入,无图时画占位面。 |
| `CTRL_TYPE_SLIDER` | `18` | 单手柄滑动条(一个值 + 一个值域)。 |
| `CTRL_TYPE_COMBO` | `19` | 下拉框。 |
| `CTRL_TYPE_COMBO_ACTION` | `20` | 下拉动作:左段下拉 + 右段按钮(复合控件)。 |
| `CTRL_TYPE_PROG` | `21` | 进度状态显示。 |
| `CTRL_TYPE_CHECKBOX` | `22` | 复选框(原生复选框 + 内联显示文字)。 |
| `CTRL_TYPE_LIST_TIMESPAN` | `23` | 列表型时间跨度:已登记但既无缓冲语义也无绘制实现 ⇒ 有控件、无像素。 |
| `CTRL_TYPE_COUNT` | `24` | 类型总数,**枚举哨兵,不是合法类型**。 |

### 注解

取值由 `CTRL_TYPE_TABLE` 的**行序**决定;新增类型一律**追加在表尾**(`LIST_TIMESPAN` 之前也行,只要追加)
⇒ 既有枚举值不变,已保存文档的类型串仍解析到同一枚举。

`CTRL_TYPE_INVALID` 与 `CTRL_TYPE_UNKNOWN` 是两种语义完全不同的非法值,不得混用(见概述的语义边界)。

判断"渲染层是否认识该类型"用 `CtrlInfo::IsKnownType`(两者都返回 `FALSE`);判"能画多少"用 `Controls::DrawCtrl`
的返回值(未知类型有占位像素、未使用槽位无像素)。

---

## CTRL_LAYER_STEP 枚举 (ControlSpec)

层次递推的逐类型步进。

### 语法

```cpp
enum CTRL_LAYER_STEP
{
    CTRL_LAYER_STEP_DOWN = -1,
    CTRL_LAYER_STEP_FLAT = 0,
    CTRL_LAYER_STEP_UP   = 1
};
```

### 成员

| 枚举值 | 值 | 说明 |
| --- | --- | --- |
| `CTRL_LAYER_STEP_DOWN` | `-1` | 域 / 承载面:每嵌套一层下沉一档。 |
| `CTRL_LAYER_STEP_FLAT` | `0` | 其余类型:继承父控件的层次。 |
| `CTRL_LAYER_STEP_UP` | `1` | 件 / 操作物:相对父控件上浮一档。 |

### 注解

只被 `CtrlLayerStep` 返回、由布局的层次递推消费;不做位组合(步进是互斥的三选一)。

取值是"相对量":与"父控件层次"相加后才是本控件的层次,再经 `CtrlLayerOf` 换算成投影层号。

---

## OPER_SEG 枚举 (OperCode)

操作码的段序号。

### 语法

```cpp
enum OPER_SEG
{
    OPER_SEG_COMMON  = 0,
    OPER_SEG_OWN     = 1,
    OPER_SEG_PAYLOAD = 2,
    OPER_SEG_COUNT
};
```

### 成员

| 枚举值 | 值 | 说明 |
| --- | --- | --- |
| `OPER_SEG_COMMON` | `0` | 公共段:`[15:0]`,对任何控件都成立的通用结论 + 搭载标志。 |
| `OPER_SEG_OWN` | `1` | 专属段:`[31:16]`,对特定控件(族)成立、按命名空间解释的互斥结论。 |
| `OPER_SEG_PAYLOAD` | `2` | 搭载段:`[63:32]`,从属部件的操作码或约定数据。 |
| `OPER_SEG_COUNT` | `3` | 段总数,**枚举哨兵,不是合法段**(传入即判"无码")。 |

### 注解

段序号的数值本身不是移位量:`GetSegment` 内部按段各自移位(0 / 16 / 32),不要把 `seg` 直接当移位用。

---

## vec2 结构 (ControlSpec)

二维坐标/尺寸值类型。

### 语法

```cpp
struct vec2
{
    float x;
    float y;

    vec2();
    vec2(float _x, float _y);

    vec2 operator+() const;
    vec2 operator-() const;
    vec2 operator+(const vec2& other) const;
    vec2 operator-(const vec2& other) const;
    vec2 operator*(const vec2& other) const;
    vec2 operator/(const vec2& other) const;
    BOOL operator!=(const vec2& other) const;
    BOOL operator==(const vec2& other) const;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `x` | 横坐标 / 宽度分量(像素)。 |
| `y` | 纵坐标 / 高度分量(像素)。 |
| `vec2()` | 默认构造:两分量清零(`0.0f`)。 |
| `vec2(float, float)` | 按分量构造。 |
| `operator+()` | 一元取正:返回自身的副本。 |
| `operator-()` | 取负:两分量取反。 |
| `operator+(const vec2&)` | 分量逐项相加。 |
| `operator-(const vec2&)` | 分量逐项相减。 |
| `operator*(const vec2&)` | 分量逐项相乘(**不是**点积或标量积)。 |
| `operator/(const vec2&)` | 分量逐项相除(除数为 0 时按浮点语义得到 `inf` / `nan`,不抛错)。 |
| `operator==(const vec2&)` | 两分量精确相等(浮点逐位比较,不含误差容限)。 |
| `operator!=(const vec2&)` | 任一分量不等。 |

### 注解

**布局契约**:两个 `float` 连续存放,二进制与 `ImVec2` 等价 —— 控件层据此在 `vec2` 与 `ImVec2` 之间做
不改变量值的直接转换。给本结构加字段、改字段顺序或改类型都会让该转换失效(读到的将是错位数据),
故本结构只承载坐标,不承载附加状态。

运算符全部按分量逐项,`==` / `!=` 是**精确比较**:由同一来源算出的值是稳定的,但经过累加 / 缩放的几何值
不应依赖相等判定(布局侧应以几何解算结果为准,不做模糊比较)。

本结构是值类型:按值传递、可复制、无所有权。

---

## CtrlInfo 结构 (ControlSpec)

控件信息:一枚控件的全部运行期数据。

### 语法

```cpp
struct CtrlInfo
{
    BufferInfo bufferInfo;
    INT32 nLayer;
    pfnCtrlEvent pEventHandler;
    vec2 size;
    vec2 position;
    vec2 posDeclared;
    LPCSTR label;
    CtrlType type;
    int id;
    int parentID;

    CtrlInfo();
    BOOL IsKnownType() const;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `bufferInfo` | 控件数据缓冲(首址 + 请求字节数);`buffer` 为空 = 无缓冲。归控件库分配与回收。 |
| `nLayer` | 控件层次(有符号相对量:0 = 窗口底那档,负 = 更沉,正 = 更亮);由布局按 `parentID` 链递推(父层 + 自身类型步进),结构件恒 `0`。 |
| `pEventHandler` | 控件的事件处理函数指针;语义随类型(按钮 = 点击 / 输入 = 提交 / 标题栏 = 标题状态 / 列表类 = 渲染该列表时按帧调用以填充内容);事件声明为 `CTRL_EVENT_ENTRUST_PARENT` 时改为父容器(按 `parentID` 上溯)的处理函数。 |
| `size` | 控件大小(像素),由解析期几何解算得出。 |
| `position` | 控件位置(像素,有效坐标 = 窗口本地坐标):内容控件 = 声明坐标 + 父链累计偏移(由 `Layout::DeriveCtrlPositions` 落地);结构件不自带累加 —— `WND` 为桌面位置,`TITLE` / `CNTR` 为窗口本地坐标。 |
| `posDeclared` | 声明坐标(像素,父控件坐标系内的坐标):几何解算写入,父链累加的输入;一经解算不再改写,故有效坐标可反复重推。 |
| `label` | 控件标签(C 串);指向中间态池内的定长字段,控件不复制、不释放。 |
| `type` | 控件类型:装载期由文档类型串经 `GetTypeEnum` 解析,未命中即 `CTRL_TYPE_UNKNOWN`。 |
| `id` | 控件标识(同布局内唯一),用于 ImGui 项标识与命中记录。 |
| `parentID` | 父容器 ID:文档 `parent` 字段的直映;`-1` = 无父容器(`0` 是合法 ID,故缺省不能取 `0`)。 |
| `CtrlInfo()` | 默认构造:`id` = 0、`type` = `CTRL_TYPE_INVALID`、`label` = `NULL`、几何清零、无缓冲、`nLayer` = 0、无事件处理器、`parentID` = -1。 |
| `IsKnownType()` | 渲染层是否认识该类型:`UNKNOWN` / `INVALID` ⇒ `FALSE`,其余 ⇒ `TRUE`。 |

### 注解

**只持像素几何**:几何系数属中间态元素规格,解析期已解算成像素 ⇒ 本结构不存系数、不存原始类型串。

**`parentID` 是层次推导、坐标累加与事件委托的共同父链**:层次按它逐级递推(父层 + 自身类型步进;无父 / 父缺失 /
成环时以窗口底档 `0` 为基),坐标按它逐级累加(`position` = `posDeclared` + 父链声明坐标之和;规则见
`Layout::DeriveCtrlPositions`),事件委托按它上溯(见 `CTRL_EVENT_ENTRUST_PARENT`);消费方按父寻址用
`Layout::GetCtrlInfoByID`。父链只认声明,不看几何包含与链序。

**声明坐标与有效坐标的分工**:`posDeclared` 是解算产物中的"父坐标系坐标",只在几何解算时写入;`position` 是
消费面读的"窗口本地坐标",由 `Layout::DeriveCtrlPositions` 按父链重算。控件库绘制时把 `position` 再换算成
内容容器本地坐标(容器已提供裁剪与偏移,见 `Controls::DrawCtrl`),因此同一枚控件在"数据面 / 绘制面"的坐标
基准不同,换算只做一次。

**布局不变量**:本结构跨模块按值传递,其尺寸与关键字段偏移由同文件内的编译期断言钉住
(尺寸 80 字节;`nLayer` 偏移 16、`posDeclared` 偏移 48、`type` 偏移 64、`id` 偏移 68、`parentID` 偏移 72)。新增或调整字段会命中这些断言 ——
这是刻意的:凡按 `sizeof` / 字段偏移推算索引的代码都必须同轮核对,否则会读到邻元素(表现为类型与索引错位)。

**所有权**:`bufferInfo` 由控件库分配与回收(装配期与 `FreeCtrlBuffer`);`label` 与 `pEventHandler` 由调用方
提供,控件不拥有;`pEventHandler` 为 `NULL` 表示该控件未绑定事件 —— 此时控件只产出像素,不回调。

**`IsKnownType` 的两个非法值不可混用**:`UNKNOWN` 在渲染层画矩形占位(消费者可感知),`INVALID` 只占位
不画(线性布局器预留的槽位)。

---

## CtrlEventIsEntrustParent 函数 (ControlSpec)

判定事件名是否为"委托父容器"特殊事件。

### 语法

```cpp
inline BOOL CtrlEventIsEntrustParent(
  [in] LPCSTR eventname
);
```

### 参数

`[in] eventname`

类型: **LPCSTR**

待判定的事件名(文档 `event` 字段的取值)。为 `NULL` 时判为"非委托"。

### 返回值

类型: **BOOL**

`TRUE` = 该名恰为 `CTRL_EVENT_ENTRUST_PARENT`;`FALSE` = 其余(含空名与普通业务事件名)。

### 注解

判据是**完整名字相等**(逐字节比较),不做前缀匹配;该名字不是绑定表里的业务事件名,装配期不按它
查绑定表,而由 `LayoutBuilder::ResolveEntrustedEvents` 解析成父容器的处理函数。

---

## CTRL_EVENT_ENTRUST_PARENT 常量 (ControlSpec)

事件字段的特殊值:把本控件的事件绑定至父容器的事件。

### 语法

```cpp
constexpr LPCSTR CTRL_EVENT_ENTRUST_PARENT = "CTRL_EVENT_ENTRUST_PARENT";
```

### 值

`"CTRL_EVENT_ENTRUST_PARENT"`

### 注解

成对契约:布局文档控件的 `event` 字段写该值时,该控件不查绑定表,而是按自身的 `parentID` 上溯到
最近一个**持有处理器的祖先**,并把该祖先的处理函数当作自己的;父容器缺失、祖先未登记事件或委托成环
(上溯步数超过控件总数)时该控件保持无响应,不报错。

**只继承处理函数,不继承缓冲**:控件自己的类型缓冲仍由控件类型决定(`Controls::PrepareCtrlBuffer`)。

---

## ListBuffer 结构 (ControlSpec)

列表类控件的缓冲头。

### 语法

```cpp
struct ListBuffer
{
    DWORD dwSelected;
    DWORD dwCount;
    DWORD dwCapacity;
    DWORD cbItem;
    // 尾随区:行数据区
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `dwSelected` | 选中项索引;`LIST_SEL_NONE`(`0xFFFFFFFF`)= 未选中。 |
| `dwCount` | 当前项目数量;超过 `dwCapacity` 时消费方按容量截断。 |
| `dwCapacity` | 项目容量(可容纳的项目块个数);装配期写入,供事件处理器与渲染端校验。 |
| `cbItem` | 每行数据长度(**自描述**);行数据区按该字节数步进,渲染端不假定行长常量。 |
| (尾随区) | 行数据区:`[特殊行数据块 cbItem] + [项目数据块 cbItem × dwCapacity]`,紧随本结构之后。 |

### 注解

**行数据区布局**:特殊行块恒占第一块(是否有特殊行由"特殊行部件指针是否为 `NULL`"表达,不靠头部标志),
其后是项目块数组。行块内容由各类型自己的约定解释:布局列表与控件列表的项目行都是输入动作块
(见 `CtrlBuffer.h` 的 `CtrlInputActionData`),控件列表的特殊行块按下拉结构解释。

**职责划分**:容量与每行长度的**取值**由行为端(装配期)写入缓冲头;渲染端只按头部字段工作,不重新计算
容量,也不假定行块语义 —— 需要行内细节时按类型协商。

缓冲总字节数 = 本结构字节数 + `cbItem × (dwCapacity + 1)`;头部与行数据区是同一块连续内存,不由指针相连。

---

## CTRL_TYPE_TABLE 常量 (ControlSpec)

控件类型身份的单一真源。

### 语法

```cpp
#define CTRL_TYPE_TABLE(ENTRY)                 \
    ENTRY(CTRL_TYPE_INVALID,       "CTRL_TYPE_INVALID")       \
    ...                                        \
    ENTRY(CTRL_TYPE_LIST_TIMESPAN, "CTRL_TYPE_LIST_TIMESPAN")

inline constexpr LPCSTR CtrlTypeNames[] = { /* 由同一张表展开 */ };
```

### 值

编译期展开:表共 **24 行** = 22 个可用类型 + `UNKNOWN` + `INVALID`,每行形态
`ENTRY(枚举名, 字符串名)`。

### 注解

**同源契约**:同一份表数据同时生成三样东西 —— ①`CtrlType` 的枚举值;②"枚举 → 字符串名"表
`CtrlTypeNames`;③两者行数的一致性断言。因此不可能出现"枚举有、串没有"的落空,也不可能出现两侧行数不一致。

`CtrlTypeNames` 是 `inline constexpr LPCSTR[]`,下标即枚举值,由 `GetTypeName` / `GetTypeEnum` 使用;
元素指向静态字面量,不归调用方所有。

**扩表规则**:新类型一律追加在表尾(或表尾哨兵之前),既有行的枚举值与其类型串保持不变 —— 枚举值是文档中
类型串的隐式序号,重排会让已保存文档解析到别的类型。

不属本文件职责:缓冲布局与容量见 `CtrlBuffer.h`,绘制实现见 `Controls.h`。

---

## PROJ_LAYER_* 常量 (ControlSpec)

投影层号的基准档与上下限。

### 语法

```cpp
constexpr INT32 PROJ_LAYER_BACKGROUND = 8;
constexpr INT32 PROJ_LAYER_MIN  = 1;
constexpr INT32 PROJ_LAYER_MAX  = 16;
```

### 值

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `PROJ_LAYER_BACKGROUND` | `8` | 背景层 / 窗口底档(层次 0)对应的投影层号 —— "整屏明暗"的唯一旋钮。 |
| `PROJ_LAYER_MIN` | `1` | 层号下限;层次过负(嵌套过深)时压平到本层。 |
| `PROJ_LAYER_MAX` | `16` | 层号上限;层次过大时压平到本层。 |

### 注解

基准档取 `8` 是留给两个方向的余地:低端 7 档给"域逐级下沉",高端 8 档给"件上浮"。抬高基准档会把整屏
压亮一档,并把典型"域中域"控件(输入框 / 下拉)推到更高层 —— 调这一个常量即可整体明暗迁移。

上下限是**压平**边界(不裁剪控件、不报错),由 `CtrlLayerOf` 在所有取色路径上统一施加。

---

## OPER_SEG_MASK_* 与 OPER_NONE 常量 (ControlSpec)

操作码三段的整字掩码与整字空值。

### 语法

```cpp
constexpr uint64_t OPER_SEG_MASK_COMMON  = 0x000000000000FFFF;
constexpr uint64_t OPER_SEG_MASK_OWN     = 0x00000000FFFF0000;
constexpr uint64_t OPER_SEG_MASK_PAYLOAD = 0xFFFFFFFF00000000;
constexpr uint64_t OPER_NONE = 0x0;
```

### 值

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `OPER_SEG_MASK_COMMON` | `0x000000000000FFFF` | 公共段(`[15:0]`)的整字掩码。 |
| `OPER_SEG_MASK_OWN` | `0x00000000FFFF0000` | 专属段(`[31:16]`)的整字掩码。 |
| `OPER_SEG_MASK_PAYLOAD` | `0xFFFFFFFF00000000` | 搭载段(`[63:32]`)的整字掩码。 |
| `OPER_NONE` | `0x0` | 整字空值 ⇒ 非控件来源(无参帧间挂载 / 直接调用)。 |

### 注解

**段布局**(与掩码一致,"前 / 次 / 后"按从最低位起):

| 段 | 位域 | 段内移位 | 整字掩码 | 语义 |
| --- | --- | --- | --- | --- |
| 公共段 | `[15:0]` | 0 | `OPER_SEG_MASK_COMMON` | 对所有控件生效的通用结论 + 搭载标志 |
| 专属段 | `[31:16]` | 16 | `OPER_SEG_MASK_OWN` | 对特定控件(族)生效的专属结论 |
| 搭载段 | `[63:32]` | 32 | `OPER_SEG_MASK_PAYLOAD` | 从属部件操作码 / 约定数据 |

公共段移位为 0 ⇒ 其段内值即整字掩码,可与整字直接按位与;专属段与搭载段必须先右移再比较(用
`GetSegment`,不要自己移位)。

**来源判据**:整字 `code == OPER_NONE` ⇒ 非控件来源(无参帧间挂载或直接调用);控件来源至少一段非 0
(无码不回调)。与整字比较一律用本常量,不用段的段内值。

---

## OPER_COMMON_PAYLOAD_* 与 OPER_COMMON_CODE_MASK 常量 (ControlSpec)

公共段内部的位域划分。

### 语法

```cpp
constexpr uint16_t OPER_COMMON_PAYLOAD_SHIFT = 14;
constexpr uint16_t OPER_COMMON_PAYLOAD_MASK  = 0xC000;
constexpr uint16_t OPER_COMMON_CODE_MASK     = 0x3FFF;
```

### 值

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `OPER_COMMON_PAYLOAD_SHIFT` | `14` | 搭载标志在公共段内的移位(`[15:14]`)。 |
| `OPER_COMMON_PAYLOAD_MASK` | `0xC000` | 搭载标志掩码(段内值,高位两位)。 |
| `OPER_COMMON_CODE_MASK` | `0x3FFF` | 公共码位域(低 14 位);搭载标志位永不参与。 |

### 注解

公共段的内部布局:`[15:14]` = 搭载标志 · `[13:8]` = 预留 · `[7:0]` = 当前使用的公共码(`0x3FFF` 掩码
涵盖低 14 位,故新增公共码最多可用到第 13 位)。

取公共码必须经 `GetCommonCode`(内部按 `OPER_COMMON_CODE_MASK` 掩码),直接按位与段内值会把搭载标志误当
成公共码位。

---

## OPER_PAYLOAD_* 常量 (ControlSpec)

搭载标志取值。

### 语法

```cpp
constexpr uint16_t OPER_PAYLOAD_NONE   = 0;
constexpr uint16_t OPER_PAYLOAD_OPCODE = 1;
constexpr uint16_t OPER_PAYLOAD_DATA   = 2;
constexpr uint16_t OPER_PAYLOAD_MIXED  = 3;
```

### 值

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `OPER_PAYLOAD_NONE` | `0` | 未搭载(搭载段恒 0)。 |
| `OPER_PAYLOAD_OPCODE` | `1` | 携带操作码:低 16 = 从属部件的专属码,高 16 = 附加(保留)。 |
| `OPER_PAYLOAD_DATA` | `2` | 携带数据:32 位按约定解释(宽度由约定定)。 |
| `OPER_PAYLOAD_MIXED` | `3` | 码 + 数据混合:低 16 = 码,高 16 = 数据。 |

### 注解

**搭载段定位**:与控件约定好的多功能搭载区 —— 既可从属**部件操作码**,也可搭载**约定数据**;类别由公共段
`[15:14]` 显式给出。

| 模式 | 布局 | 用途 |
| --- | --- | --- |
| 未搭载 | 恒 0 | —— |
| 携带操作码 | 低 16 = 从属部件专属码;高 16 = 附加(保留) | 管道 / 命令部件把行件结论带回控件层 |
| 携带数据 | 32 位整块按约定解释 | `param` 已被占用 / 数据需随码跨层 |
| 码 + 数据混合 | 低 16 = 码,高 16 = 数据 | 保留 |

**布局登记**(约定实体,供人读与文档生成,不是运行期查表):

| 发出者 | 专属段段内值 | 模式 | 布局 |
| --- | --- | --- | --- |
| 列表控件(行件结论被携带) | 空(专属段留 0) | 携带操作码 | 低 16 = 行内件码 |
| 列表控件(行被拖拽重排) | `OPER_LIST_ROW_MOVED` | 携带数据 | 32 位 = 目标插入位置(源行号走 `param`) |

**与 `param` 的分工**:`param` = 终端业务数据通道(行号、槽位号、新值等,默认优先);搭载段 = 让操作码
自包含的扩展槽(仅当 ①`param` 已被占用 ②操作码需跨层 / 跨帧携带 ③部件产出"码 + 局部数据"绑定的复合结果)。

`PAYLOAD == 0` **不代表"无搭载"**(数据可能恰为 0)⇒ 判有无搭载一律看本组标志,经 `GetPayloadKind` 取。

---

## OPER_COMMON_* 常量 (ControlSpec)

公共码位标志。

### 语法

```cpp
constexpr uint16_t OPER_COMMON_NONE        = 0x0000;
constexpr uint16_t OPER_COMMON_CLICKED     = 0x0001;
constexpr uint16_t OPER_COMMON_DOUBLECLICK = 0x0002;
constexpr uint16_t OPER_COMMON_RIGHTCLICK  = 0x0004;
constexpr uint16_t OPER_COMMON_ACTIVE      = 0x0008;
constexpr uint16_t OPER_COMMON_DEACTIVATED = 0x0010;
constexpr uint16_t OPER_COMMON_HOLD        = 0x0020;
constexpr uint16_t OPER_COMMON_DRAG        = 0x0040;
constexpr uint16_t OPER_COMMON_DROP        = 0x0080;
```

### 值

| 常量 | 值 | 说明 | 类型 |
| --- | --- | --- | --- |
| `OPER_COMMON_NONE` | `0x0000` | 无公共码(未使用公共段)。 | —— |
| `OPER_COMMON_CLICKED` | `0x0001` | 完整单击(左键按下并在本控件命中区内释放)。 | 沿 |
| `OPER_COMMON_DOUBLECLICK` | `0x0002` | 双击。 | 沿 |
| `OPER_COMMON_RIGHTCLICK` | `0x0004` | 右键(上下文菜单入口)。 | 沿 |
| `OPER_COMMON_ACTIVE` | `0x0008` | 激活:本帧获得激活态。 | 沿 |
| `OPER_COMMON_DEACTIVATED` | `0x0010` | 失去激活:本帧释放激活态。 | 沿 |
| `OPER_COMMON_HOLD` | `0x0020` | 按住中。 | **电平** |
| `OPER_COMMON_DRAG` | `0x0040` | 拖动本控件(沿 = 开始拖动)。 | 沿 |
| `OPER_COMMON_DROP` | `0x0080` | 已接收投放(目标侧)。 | 沿 |

### 注解

**形式 = 组合位**:段内值 = `1u << n`,公共段移位为 0 ⇒ 段内值即整字掩码。三组语义:指针点击
(`CLICKED` / `DOUBLECLICK` / `RIGHTCLICK`)、激活生命周期(`ACTIVE` / `DEACTIVATED` / `HOLD`)、拖放
(`DRAG` / `DROP`)。

`ACTIVE` 是"激活","按住"另算(`HOLD`);三者(+`DEACTIVATED`)构成"按下 → 持续 → 松开"的完整生命周期。
`CLICKED` 与 `DOUBLECLICK` **不互斥**(同帧可同时置位,消费者按需取舍)。

**入选判据**:只允许依赖"控件作为一个可交互矩形"这一通用事实(与控件数据类型 / 编辑语义无关)。因此
编辑语义(`SUBMIT` / `CHANGED` / `CANCEL` / `SCROLLED`)归专属段;悬停与"鼠标按下"属原始输入事实,不进
操作码;创建 / 销毁 / 移动 / 尺寸 / 可见性 / 启用属框架或宿主语义,也不进操作码。

**清零口径**:沿位 = 触发帧置位,回调后清或帧末统一清;电平位(`HOLD`)每帧重算,不跨帧遗留。

**命中区**:所有公共位一律限定**本控件命中区**,禁止直接取全局鼠标状态。

---

## OPER_EDIT_* 常量 (ControlSpec)

编辑族专属码。

### 语法

```cpp
constexpr uint16_t OPER_EDIT_SUBMIT  = 0x0001;
constexpr uint16_t OPER_EDIT_CHANGED = 0x0002;
constexpr uint16_t OPER_EDIT_CANCEL  = 0x0003;
```

### 值

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `OPER_EDIT_SUBMIT` | `0x0001` | 编辑提交(回车 / 失焦且有改动)。 |
| `OPER_EDIT_CHANGED` | `0x0002` | 值或选项已改变。 |
| `OPER_EDIT_CANCEL` | `0x0003` | 编辑取消(Esc / 恢复原值)。 |

### 注解

**适用控件**:有值可改的编辑族 —— 单行 / 多行 / 整数 / 实数输入、下拉框、滑动条,以及复选框的"勾选态改变"
(与下拉 / 滑块共用 `CHANGED`,不额外派发公共码)。

**专属段是命名空间式、允许重码**:同一段内值在不同命名空间(不同控件族)内有不同含义,`0x0000` 在每个
命名空间都表示"该控件无专属码"。解释专属码前**必须先知道控件类型**,诊断打印操作码时必须成对给出类型。

**专属段不用位图**:它是互斥结论,用递增码(1、2、3…),不要按位组合。

---

## OPER_CUSTOM_* 常量 (ControlSpec)

自定义组合部件的专属码。

### 语法

```cpp
constexpr uint16_t OPER_CUSTOM_IA_INPUT_SUBMIT  = 0x0001;
constexpr uint16_t OPER_CUSTOM_IA_BTN_CLICKED   = 0x0002;
constexpr uint16_t OPER_CUSTOM_CA_COMBO_CHANGED = 0x0001;
constexpr uint16_t OPER_CUSTOM_CA_BTN_CLICKED   = 0x0002;
```

### 值

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `OPER_CUSTOM_IA_INPUT_SUBMIT` | `0x0001` | 输入动作控件:输入段提交。 |
| `OPER_CUSTOM_IA_BTN_CLICKED` | `0x0002` | 输入动作控件:按钮段点击。 |
| `OPER_CUSTOM_CA_COMBO_CHANGED` | `0x0001` | 下拉动作控件:下拉段变更。 |
| `OPER_CUSTOM_CA_BTN_CLICKED` | `0x0002` | 下拉动作控件:按钮段点击。 |

### 注解

**子空间按控件**:`IA`(输入动作)与 `CA`(下拉动作)是两个独立子空间 ⇒ 同一数值 `0x0001` 在两个子空间
里含义不同,这是合法的重码,与 `OPER_EDIT_*` 的 `0x0001` 也不冲突。

这两类控件的部件结论"独立使用"(控件自身即该件)⇒ 结论落**专属段**;若同一个件被当行件携带进列表,
其结论则落搭载段(见 `OPER_PAYLOAD_*` 的布局登记)。"落哪一段"由调用方按使用情景决定,部件只报自己的结论。

---

## OPER_LIST_* 常量 (ControlSpec)

列表族专属码。

### 语法

```cpp
constexpr uint16_t OPER_LIST_ROW_ACTIVATED         = 0x0001;
constexpr uint16_t OPER_LIST_ROW_SUBMIT            = 0x0002;
constexpr uint16_t OPER_LIST_ROW_BTN_CLICKED       = 0x0003;
constexpr uint16_t OPER_LIST_ROW_SELECTION_CHANGED = 0x0004;
constexpr uint16_t OPER_LIST_ROW_MOVED             = 0x0005;
constexpr uint16_t OPER_LIST_ADD                   = 0x0006;
constexpr uint16_t OPER_LIST_SCROLLED              = 0x0007;
```

### 值

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `OPER_LIST_ROW_ACTIVATED` | `0x0001` | 行被激活(点选)。 |
| `OPER_LIST_ROW_SUBMIT` | `0x0002` | 行文本提交(改名)。 |
| `OPER_LIST_ROW_BTN_CLICKED` | `0x0003` | 行内按钮点击。 |
| `OPER_LIST_ROW_SELECTION_CHANGED` | `0x0004` | 行内下拉变更。 |
| `OPER_LIST_ROW_MOVED` | `0x0005` | 行被拖拽重排。 |
| `OPER_LIST_ADD` | `0x0006` | 特殊行(新增行)被使用。 |
| `OPER_LIST_SCROLLED` | `0x0007` | 列表滚动。 |

### 注解

**落段规则**:普通行内事件**不占专属段** —— 行是"被携带的下属",其结论落搭载段(搭载标志 = 操作码),
`param` = 行号;只有"行被重排"这一由列表自己得出的结论才落专属段,并把目标插入位置放进搭载段
(搭载标志 = 数据)、源行号放进 `param`。

**重排折算**:绘制侧先用 `Controls.h` 的 `ListRowDropOper` 得到行件返回约定,再折算成上述三段;
落点只由"放置目标行 + 鼠标落在上半 / 下半"决定,与源行号无关。

**应用业务意图码不进本系统**:选中 / 新增 / 删除 / 改名 / 改类型 / 移动等**应用意图**留在应用侧
(其私有常量),控件库只报"原始交互结论"。

---

## OPER_STRUCT_TITLE_CLOSE 常量 (ControlSpec)

结构件族专属码:关闭钮被点击。

### 语法

```cpp
constexpr uint16_t OPER_STRUCT_TITLE_CLOSE = 0x0001;
```

### 值

`0x0001`(结构件族的第一个专属码)。

### 注解

由标题栏控件在关闭钮被点击的那一帧产出,语义 = **请求关闭窗口**;实际关闭由绑定的事件处理器决定
(标题栏自身不调窗口 API)。

与其它族的 `0x0001` 重码是合法的(命名空间不同);消费前须知道控件类型。

---

## OPER_PROG_UPDATED 等类型单列码 常量 (ControlSpec)

上述分类覆盖不到时按类型单列的专属码。

### 语法

```cpp
constexpr uint16_t OPER_PROG_UPDATED       = 0x0001;
constexpr uint16_t OPER_COMBO_OPENED       = 0x0001;
constexpr uint16_t OPER_COMBO_CLOSED       = 0x0002;
constexpr uint16_t OPER_SLIDER_THUMB_LEFT  = 0x0001;
constexpr uint16_t OPER_SLIDER_THUMB_RIGHT = 0x0002;
```

### 值

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `OPER_PROG_UPDATED` | `0x0001` | 进度已更新(**电平型**:可见期间每帧报一次)。 |
| `OPER_COMBO_OPENED` | `0x0001` | 下拉展开(已登记,当前无产出点)。 |
| `OPER_COMBO_CLOSED` | `0x0002` | 下拉收起(已登记,当前无产出点)。 |
| `OPER_SLIDER_THUMB_LEFT` | `0x0001` | 左柄(旧双柄口径遗留,保留以防重码)。 |
| `OPER_SLIDER_THUMB_RIGHT` | `0x0002` | 右柄(同上)。 |

### 注解

**滑动条已改为单手柄口径**:一个值 + 一个值域,值变更即 `OPER_EDIT_CHANGED`,不再用"柄身份"推断 ⇒
两个柄码仅作编号占位,不再产出。

**下拉的展开 / 收起**属类型特有语义,不并入 `OPER_EDIT_*`;当前无产出点,消费者不应依赖它们出现。

**进度更新是电平型**:与"沿"型码不同,它在可见期间每帧都成立,不得按"只触发一次"的假设消费。

---

## LIST_* 常量 (ControlSpec)

列表选中哨兵与特殊行位置标志。

### 语法

```cpp
constexpr DWORD LIST_SEL_NONE = 0xFFFFFFFF;
constexpr DWORD LIST_SPECIAL_FIRST = 0;
constexpr DWORD LIST_SPECIAL_LAST = 1;
```

### 值

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `LIST_SEL_NONE` | `0xFFFFFFFF` | 无选中项(填充 `ListBuffer::dwSelected`)。 |
| `LIST_SPECIAL_FIRST` | `0` | 特殊行在前(特殊行标志的低 16 位取值 = 位置)。 |
| `LIST_SPECIAL_LAST` | `1` | 特殊行在后(同上)。 |

### 注解

**特殊行标志是"一个 `DWORD`"**:低 16 位 = 位置(取 `LIST_SPECIAL_FIRST` / `LIST_SPECIAL_LAST`),
高 16 位 = 特殊行高度(像素)。本组常量只占低 16 位的取值域,不要与高度混用。

**特殊行是否存在**由"特殊行部件指针是否为 `NULL`"表达,不靠头部标志或本组常量;位置标志只决定它排在
项目行之前还是之后。

`LIST_SEL_NONE` 是"未选中"的哨兵值,与"选中第 0 项"不同 ⇒ 消费者须先与本值比较再取索引。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/base/ControlSpec.h`(`src\framework\base\ControlSpec.h`) |
| 实现 | `src\framework\base\ControlSpec.cpp`;常量与类型在头文件内 |
| 命名空间 | 全局作用域 |
| 依赖 | `minwindef.h` · `ucrt` · `stdint.h` |
| 平台 | Windows 全版本 |
| 线程安全 | 是(纯函数) |
