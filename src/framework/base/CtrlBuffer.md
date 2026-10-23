# CtrlBuffer 模块 (src\framework\base\CtrlBuffer.h)

## 概述

控件缓冲的**数据约定与装配回收**:逐类型缓冲的数据布局、容量常量、控件缺省文案、文本通道取址,
以及依控件类型分配并初始化缓冲、把文本按容量写回、回收单枚控件缓冲。缓冲一律取自会话小对象池
(`AllocatorHub`),分配与回收成对;数据布局是控件库与装配期的共同约定。

本模块属 **framework 层 · base**,只依赖 `framework/base/*`(含 `mem/*` 的分配原语与 `BufferInfo`)以及
C 运行时,**不依赖任何上层**(render / layout / dispatch),也不含任何 UI 库类型与绘制实现 ——
它只回答"这枚控件的数据长什么样、多大、怎么取址、怎么释放",不回答"怎么画"。

**不做什么**:不绘制、不排版、不持纹理与设备;不做类型语义判定(是否文本类等归 `ControlSpec.h`,
本模块只转调);不替应用决定业务初值(末值由应用按业务语义写入)。

**与相邻模块的边界**:控件类型身份、`CtrlInfo`、类型属性谓词与操作码归 `framework/base/ControlSpec.h`;
布局级批量回收归 `LayoutBuilder::FreeLayoutCtrlBuffers`;绘制与命中归控件库。

缓冲容量一律**随缓冲头走**(`CtrlInfo::bufferInfo.size`),故控件库、事件层与文本通道无需另知具体常量;
常量只有装配期分配一处引用。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| :--- | :--- |
| `CtrlInputActionBtnText` | 取输入动作控件载荷内按钮文本区首址。 |
| `CtrlInputActionText` | 取输入动作控件载荷内输入文本区首址。 |
| `CtrlTextRangeOf` | 取文本类控件的文本首址与可写容量(一次给出)。 |
| `CtrlTextBuffer` | 取文本类控件的文本首址(兼容入口)。 |
| `CtrlTextCapacity` | 取文本类控件的可写容量(兼容入口)。 |
| `PrepareCtrlBuffer` | 依控件类型为该控件分配并初始化缓冲。 |
| `WriteCtrlTextToBuffer` | 把中间态文本按缓冲容量写回控件。 |
| `FreeCtrlBuffer` | 回收一枚控件的自身缓冲及其附属块。 |

### 类型

| 类型 | 说明 |
| :--- | :--- |
| `CtrlInputActionData` | 输入动作控件缓冲的定长头(变长载荷紧随其后)。 |
| `CtrlComboData` | 下拉类控件的缓冲结构(选项表 / 数量 / 选中索引 / 按钮文案)。 |
| `CtrlProgData` | 进度控件缓冲结构。 |
| `CtrlRenderData` | 渲染面控件缓冲结构(纹理视图 + 像素尺寸,类型擦除)。 |
| `CtrlCheckData` | 复选框控件缓冲结构(勾选态 + 内联显示文字)。 |
| `CtrlSliderData` | 滑动条控件缓冲结构(手柄当前值 + 值域)。 |
| `CtrlTextRange` | 文本通道的取址结果(首址 + 容量)。 |

### 常量

| 常量 | 值 | 说明 |
| :--- | :--- | :--- |
| `CTRL_TEXT_BUF` | `256` | 单行文本类控件的缓冲字节数(含结尾 `\0`)。 |
| `CTRL_TEXT_MULTI_BUF` | `4096` | 多行文本控件的缓冲字节数。 |
| `CTRL_LABEL_BUF` | `256` | 控件内联显示文字的字节数(与文本通道容量语义分离)。 |
| `CTRL_INPUT_ACTION_ACTION` | `64` | 输入动作控件载荷内按钮文本区的字节数(含结尾 `\0`)。 |
| `CTRL_INPUT_ACTION_SPLIT` | `64` | 载荷内按钮文本区与输入文本区的分割位置(8 字节对齐)。 |
| `CTRL_INPUT_ACTION_BUF` | 编译期算出(`sizeof(CtrlInputActionData) + 64 + 256`) | 输入动作控件的缓冲总字节数。 |
| `CTRL_DEFAULT_ACTION_BTN` | `u8"动作"` | 下拉动作控件右段按钮的缺省文案。 |
| `CTRL_DEFAULT_BROWSE_BTN` | `u8"浏览"` | 输入动作控件按钮文本区的缺省文案。 |
| `CTRL_DEFAULT_COMBO_ITEM` | `u8"选项 1"` | 下拉类控件未获选项文本时的缺省选项。 |

---

## CtrlInputActionBtnText 函数 (全局)

取输入动作控件载荷内按钮文本区首址。

### 语法

```cpp
inline CHAR* CtrlInputActionBtnText(CtrlInputActionData* pData);
```

### 参数

`[in] pData`

类型: **CtrlInputActionData\***

输入动作控件缓冲指针。传 `NULL` 时返回 `NULL`。

### 返回值

类型: **CHAR\***

按钮文本区首址(可写),容量 `CTRL_INPUT_ACTION_ACTION`(64 字节,含结尾 `\0`);`pData` 为空时返回 `NULL`。
指向控件缓冲内部,不转移所有权。

### 注解

按钮文本区固定落在载荷起点(定长头之后第一个字节),偏移是编译期常量,不读结构字段。

按钮文本是**可改写**的:装配期写入缺省文案 `CTRL_DEFAULT_BROWSE_BTN`,之后可由应用按业务语义改写。
写入须留结尾 `\0`;超出容量时 `strcpy_s` 一族会整串拒绝。

---

## CtrlInputActionText 函数 (全局)

取输入动作控件载荷内输入文本区首址。

### 语法

```cpp
inline CHAR* CtrlInputActionText(CtrlInputActionData* pData);
```

### 参数

`[in] pData`

类型: **CtrlInputActionData\***

输入动作控件缓冲指针。`pData` 为空、或其定长头的 `split` 不等于 `CTRL_INPUT_ACTION_SPLIT`(尚未装配)时,
函数返回 `NULL`。

### 返回值

类型: **CHAR\***

输入文本区首址(可写);可写容量 = 缓冲总长 − 定长头 − `CTRL_INPUT_ACTION_SPLIT`。以上前提不成立时返回 `NULL`。

### 注解

偏移由编译期常量 `CTRL_INPUT_ACTION_SPLIT` 给出(单一真源);结构字段 `split` 只作"行块已初始化"的标志,
不参与偏移计算。装配期把 `split` 写成同一常量,故正常缓冲恒通过校验。

本类型没有独立的"输入文本容量"常量:容量随缓冲头计算(缓冲总长 − 定长头 − 分割位置) ⇒ 配合
`CtrlTextRangeOf` 即可,调用方无需另知常量。

---

## CtrlTextRangeOf 函数 (全局)

取文本类控件的文本首址与可写容量。

### 语法

```cpp
inline CtrlTextRange CtrlTextRangeOf(CtrlInfo* info);
```

### 参数

`[in] info`

类型: **CtrlInfo\***

控件信息指针。`info` 为空、其缓冲为空、缓冲容量为 `0`、或该类型不属文本类时返回 `{ NULL, 0 }`;
输入动作控件另要求缓冲容量不小于 `CTRL_INPUT_ACTION_BUF`。

### 返回值

类型: **CtrlTextRange**

首址与容量成对给出:`lpText` 为文本首址(可写,不适用时为 `NULL`),`cbCapacity` 为可写字节容量
(含结尾 `\0`,不适用时为 `0`)。返回的是**控件缓冲内部**的地址,不转移所有权、不复制。

### 注解

本函数是文本通道的**唯一取址口径**;`CtrlTextBuffer` / `CtrlTextCapacity` 是它的两个兼容薄包装。
首址与容量一次给出,调用方不可能只取首址而漏取容量。

容量为 `0` 表示"不可写":调用方应按只读展示处理,而不是按空缓冲处理(首址可能仍然有效)。

`INPUT_ACTION` 走载荷内的输入文本区(不经缓冲偏移 0);其余文本类控件的缓冲首字段即 C 串。

---

## CtrlTextBuffer 函数 (全局)

取文本类控件的文本首址(兼容入口)。

### 语法

```cpp
inline CHAR* CtrlTextBuffer(CtrlInfo* info);
```

### 参数

`[in] info`

类型: **CtrlInfo\***

控件信息指针。判定条件与 `CtrlTextRangeOf` 相同。

### 返回值

类型: **CHAR\***

文本首址(可写);不可用时返回 `NULL`。等价于 `CtrlTextRangeOf(info).lpText`。

### 注解

保留本函数只为既有调用点;新代码应使用 `CtrlTextRangeOf`,避免首址与容量分别取用时口径分叉。

---

## CtrlTextCapacity 函数 (全局)

取文本类控件的可写文本容量(兼容入口)。

### 语法

```cpp
inline size_t CtrlTextCapacity(CtrlInfo* info);
```

### 参数

`[in] info`

类型: **CtrlInfo\***

控件信息指针。判定条件与 `CtrlTextRangeOf` 相同。

### 返回值

类型: **size_t**

可写字节容量(含结尾 `\0`);不可用时返回 `0`。等价于 `CtrlTextRangeOf(info).cbCapacity`。

### 注解

保留本函数只为既有调用点;新代码应使用 `CtrlTextRangeOf`。

---

## CtrlInputActionData 结构 (全局)

输入动作控件缓冲的定长头。

### 语法

```cpp
struct CtrlInputActionData
{
    DWORD split;
    // 尾随区: BYTE data[CTRL_INPUT_ACTION_SPLIT + CTRL_TEXT_BUF]
};
```

### 成员

| 成员 | 说明 |
| :--- | :--- |
| `split` | 行块初始化标志:装配期写入 `CTRL_INPUT_ACTION_SPLIT`。文本取址不依赖本字段(偏移由常量给出),本字段只用于"是否已装配"的校验。 |
| (尾随区) | 不声明为字段的变长载荷:按钮文本 + 输入文本,内联在定长头之后(非指针指向别处)。 |

### 注解

缓冲形态 = **定长头 + 变长载荷**,总字节数 = `CTRL_INPUT_ACTION_BUF`;两个文本区经
`CtrlInputActionBtnText` / `CtrlInputActionText` 取。

本结构由输入动作控件与列表行共用:列表行块也是输入动作块(长度 = 列表头部的 `cbItem`)。

---

## CtrlComboData 结构 (全局)

下拉类控件的缓冲结构。

### 语法

```cpp
struct CtrlComboData
{
    INT count;
    INT index;
    LPCSTR* items;
    LPCSTR lpBtn;
};
```

### 成员

| 成员 | 说明 |
| :--- | :--- |
| `count` | 选项数量。`0` 或负数 ⇒ 无可选项(下拉类控件不产出像素)。 |
| `index` | 当前选中索引;下拉项变更时被写回缓冲。 |
| `items` | 选项字符串指针数组。数组本身由装配期分配、由控件缓冲回收路径释放;数组内字符串不归本结构释放(由分配方按其生命周期管理)。 |
| `lpBtn` | 右段按钮文案;`NULL` = 用缺省文案 `CTRL_DEFAULT_ACTION_BTN`。 |

### 注解

`CTRL_TYPE_COMBO`(下拉框)与 `CTRL_TYPE_COMBO_ACTION`(下拉动作)共用本结构;列表行的特殊行块也按本结构
解释。渲染端按键读选项表,选中索引被下拉项写回。

**所有权**:`items` 数组本身由装配期分配、由控件缓冲回收路径释放;数组内字符串的来源由分配方决定,
本模块不假设"数组与字符串同源",也不释放字符串。

---

## CtrlProgData 结构 (全局)

进度控件缓冲结构。

### 语法

```cpp
struct CtrlProgData
{
    int64_t current;
    int64_t expect;
    BOOL IsVisible;
};
```

### 成员

| 成员 | 说明 |
| :--- | :--- |
| `current` | 当前进度。装配期初值 `0`。 |
| `expect` | 预期进度(分母)。装配期初值 `100`;`0` 时进度比无定义,由绘制实现兜底。 |
| `IsVisible` | 是否可见。装配期初值 `TRUE`;不可见时控件既不绘制也不回传操作码。 |

### 注解

进度值由应用在渲染前直接写缓冲(本模块不提供写入函数)。

可见性属于**数据**而非布局属性:隐藏进度条应写 `IsVisible`,不要靠改控件尺寸或位置。

---

## CtrlRenderData 结构 (全局)

渲染面控件缓冲结构。

### 语法

```cpp
struct CtrlRenderData
{
    void* pTexture;
    INT32 texWidth;
    INT32 texHeight;
};
```

### 成员

| 成员 | 说明 |
| :--- | :--- |
| `pTexture` | 纹理视图(D3D11 的着色器资源视图),类型擦除为 `void*`;`NULL` = 无图 ⇒ 画占位面。 |
| `texWidth` | 纹理像素宽(等比缩放基准);`<= 0` 视为无图。 |
| `texHeight` | 纹理像素高(同上)。 |

### 注解

控件只声明"图像显示矩形",像素来源由宿主注入:宿主创建纹理并把视图指针写入本结构。控件库不持 D3D11
设备、不管理纹理生命周期 —— 创建与释放在宿主;视图指针的类型擦除正是为了让本头文件不依赖 `d3d11.h`。

装配期把本结构清零 ⇒ 默认无图;宿主注入后才显示图像。写入须在渲染帧外或按单线程节奏完成。

---

## CtrlCheckData 结构 (全局)

复选框控件缓冲结构。

### 语法

```cpp
struct CtrlCheckData
{
    BOOL IsChecked;
    CHAR szLabel[CTRL_LABEL_BUF];
};
```

### 成员

| 成员 | 说明 |
| :--- | :--- |
| `IsChecked` | 勾选态(`TRUE` = 已勾选)。装配期初值 `FALSE`;点击后被控件写回。 |
| `szLabel` | 复选框右侧显示文字(C 串,容量 `CTRL_LABEL_BUF`)。装配期取文档声明的文本。 |

### 注解

缓冲 = "勾选态 + 显示文字"一体定长,文字内联在结构内 ⇒ 本类型**无变长载荷**,也不属文本类控件(其文字
不参与文本通道)。

勾选态落在**首字段**是有意的:应用可以把缓冲首址直接对接开关型数据,无需二次映射。换字段顺序会破坏该对接。

显示文字容量用 `CTRL_LABEL_BUF` 而非 `CTRL_TEXT_BUF`:两者当前数值相同,但前者服务于"结构内字段",
后者服务于"文本通道",语义分离后改一处不会连带改另一处。

---

## CtrlSliderData 结构 (全局)

滑动条控件缓冲结构。

### 语法

```cpp
struct CtrlSliderData
{
    int64_t vMin;
    int64_t lMin;
    int64_t lMax;
};
```

### 成员

| 成员 | 说明 |
| :--- | :--- |
| `vMin` | 手柄当前值。拖动 / 点击跳转即写回;装配期初值取文档声明文本。 |
| `lMin` | 值域下限,装配期初值 `0`。 |
| `lMax` | 值域上限,装配期初值 `100`。 |

### 注解

语义 = "一个值 + 一个值域":参与语义的只有 `vMin`(手柄当前值)与 `lMin` / `lMax`(值域)。

值域初值只是占位,真实值域由应用按业务语义写入;`lMin > lMax` 不在本模块校验。

---

## CtrlTextRange 结构 (全局)

文本通道的取址结果。

### 语法

```cpp
struct CtrlTextRange
{
    CHAR* lpText;
    size_t cbCapacity;
};
```

### 成员

| 成员 | 说明 |
| :--- | :--- |
| `lpText` | 文本首址(可写;不适用时为 `NULL`)。 |
| `cbCapacity` | 可写字节容量(含结尾 `\0`;`0` = 只读或不适用)。 |

### 注解

由 `CtrlTextRangeOf` 一次给出,避免"只取首址、漏取容量"的配对错误。容量为 `0` 时首址可能仍有效,
调用方应按只读处理。

---

## CTRL_TEXT_BUF 常量 (全局)

单行文本类控件的缓冲字节数。

### 语法

```cpp
constexpr DWORD CTRL_TEXT_BUF = 256;
```

### 值

`256`(含结尾 `\0`)。

### 注解

适用于 `TEXT` / `BUTTON` / `INPUT_SINGLE` / `TITLE` 四类单行文本控件的装配期分配。

`INPUT_MULTI` 不用本容量 —— 多行控件按 `CTRL_TEXT_MULTI_BUF` 分配,见 `PrepareCtrlBuffer` 的类型对照。

运行期不查该常量:容量一律随缓冲头(`CtrlInfo::bufferInfo.size`)走,调用方不应据本常量推算实际可写长度。

---

## CTRL_TEXT_MULTI_BUF 常量 (全局)

多行文本控件的缓冲字节数。

### 语法

```cpp
constexpr DWORD CTRL_TEXT_MULTI_BUF = 4096;
```

### 值

`4096`(含结尾 `\0`)。

### 注解

多行控件的用途是长文本(提示词 / 负面提示词 / 参数文件内容),单行的 256 字节明显不够 ⇒ 单列 16 倍容量。

容量随缓冲头走(`CtrlTextRangeOf` 读缓冲尺寸) ⇒ 控件库与文本通道都无需另知本常量;只有装配期分配一处引用它。

本类型必须在通用文本类分支**之前**单独定容量,否则会落到 256 字节而在长文本上静默截断。

---

## CTRL_LABEL_BUF 常量 (全局)

控件内联显示文字的字节数。

### 语法

```cpp
constexpr DWORD CTRL_LABEL_BUF = 256;
```

### 值

`256`(含结尾 `\0`)。

### 注解

服务于"结构内字段"形态的显示文字(当前唯一使用者 = 复选框的 `szLabel`),与文本通道容量 `CTRL_TEXT_BUF`
语义分离。两者数值当前相同,但分属不同通道:改文本缓冲容量不应连带改复选框结构尺寸。

---

## CTRL_INPUT_ACTION_ACTION 常量 (全局)

输入动作控件缓冲内按钮文本区的字节数。

### 语法

```cpp
constexpr DWORD CTRL_INPUT_ACTION_ACTION = 64;
```

### 值

`64`(含结尾 `\0`)。

### 注解

与 `CTRL_INPUT_ACTION_SPLIT` 成对:分割位置由本容量向上取整到 8 的倍数得到,故给按钮文本留的窗口
就是这里的 64 字节。

---

## CTRL_INPUT_ACTION_SPLIT 常量 (全局)

载荷内按钮文本区与输入文本区的分割位置。

### 语法

```cpp
constexpr DWORD CTRL_INPUT_ACTION_SPLIT = ((CTRL_INPUT_ACTION_ACTION + 7u) & ~7u);
```

### 值

`64`(按钮文本区容量向上对齐到 8 的倍数;当前 64 本已对齐)。

### 注解

本常量是分割位置的**唯一真源**:`CtrlInputActionText` 直接按它计算偏移,不读结构字段 `split`。
装配期把同一值写入 `split` 作为"行块已初始化"的标志;两处必须一致,不要在别处另算分割位置。

保留 8 字节对齐是为了让载荷起点落在对齐边界上,便于按定长段扩展。

---

## CTRL_INPUT_ACTION_BUF 常量 (全局)

输入动作控件的缓冲总字节数。

### 语法

```cpp
constexpr DWORD CTRL_INPUT_ACTION_BUF =
    (DWORD)(sizeof(CtrlInputActionData) + CTRL_INPUT_ACTION_SPLIT + CTRL_TEXT_BUF);
```

### 值

编译期算出:定长头字节数 + 按钮文本区 64 + 输入文本区 256。

### 注解

变长载荷内联在定长头之后 ⇒ 按钮文本与输入文本都是缓冲内的固定偏移(非指针指向别处),总容量因此可以
在编译期定死。

三处依赖本值:①装配期按它分配;②`CtrlTextRangeOf` 与控件绘制实现按它校验缓冲尺寸(不足即按无缓冲处理);
③列表行块长度也取它(`cbItem`),故列表行数据区与输入动作缓冲同构。

---

## CTRL_DEFAULT_ACTION_BTN 常量 (全局)

下拉动作控件右段按钮的缺省文案。

### 语法

```cpp
inline constexpr LPCSTR CTRL_DEFAULT_ACTION_BTN = u8"动作";
```

### 值

`u8"动作"`。

### 注解

调用方未给出按钮文案时使用(缓冲内的 `lpBtn` 为 `NULL`,或控件按缺省文案绘制)。控件库各处一律引用本常量,
不得另写字面量。

该串是静态字面量,**不归任何缓冲所有**,不得释放。应用可经 `CtrlComboData::lpBtn` 覆盖。

---

## CTRL_DEFAULT_BROWSE_BTN 常量 (全局)

输入动作控件按钮文本区的缺省文案。

### 语法

```cpp
inline constexpr LPCSTR CTRL_DEFAULT_BROWSE_BTN = u8"浏览";
```

### 值

`u8"浏览"`。

### 注解

装配期写入按钮文本区(`PrepareCtrlBuffer`),之后可由应用按业务语义改写。该值是占位文案,不代表具体业务。

该串是静态字面量,**不归任何缓冲所有**,不得释放。

---

## CTRL_DEFAULT_COMBO_ITEM 常量 (全局)

下拉类控件未获选项文本时的缺省选项。

### 语法

```cpp
inline constexpr LPCSTR CTRL_DEFAULT_COMBO_ITEM = u8"选项 1";
```

### 值

`u8"选项 1"`。

### 注解

装配期为下拉类控件提供 1 个可选默认项,保证渲染可显示一项;应用可在装配后改写选项表。

该串是静态字面量,**不归任何缓冲所有**,不得释放。

---

## PrepareCtrlBuffer 函数 (CtrlBuffer)

依控件类型为该控件分配缓冲,并按该类型的初值口径初始化(文本类的初值 = 声明文本;数值类 = 声明文本折算;
复合类 = 结构头 + 默认项)。

### 语法

```cpp
BOOL PrepareCtrlBuffer(
  [in]      CtrlInfo* info,
  [in] const char*    text
);
```

### 参数

`[in] info`

类型: **CtrlInfo\***

待装配控件。本函数读取 `type`,并**直接覆盖** `bufferInfo`(不先回收旧缓冲,调用前须为空)。

`[in] text`

类型: **const char\***

文档声明的初值文本(可为 `NULL`;`NULL` 与空串同义,表示采用类型默认值)。

### 返回值

类型: **BOOL**

`TRUE` = 装配完成(含"该类型无需缓冲"与"未知类型不建缓冲"两类正常结果);
`FALSE` = 内存池分配失败,或类型枚举落在 switch 兜底分支(调用方据此否决该文档)。

### 注解

分派顺序(自上而下,先特例后分组):不建缓冲的类型(未知 / 未使用槽位 / `LIST` / `TIMEBAR` /
`LIST_TIMESPAN`)→ `INPUT_ACTION`(定长头 + 按钮文本区 + 输入文本区)→ `CHECKBOX`(勾选态 + 显示文字)→
`INPUT_MULTI`(4096 字节,长文本)→ 文本类(`CTRL_TEXT_BUF`)→ 列表类(`ListBuffer` 头 + 特殊行块 +
项目块 × 容量)→ 其余定长结构(`INPUT_REAL` / `INPUT_NUMERIC` / `COMBO*` / `SLIDER` / `PROG` / `RENDER`)→ 兜底失败。

顺序不可随意调整:`INPUT_MULTI` 必须在通用文本类分支之前(否则长文本被 256 字节截断);
`CHECKBOX` 必须在文本类之前(其文字是结构内字段,不参与文本往返通道)。

列表类控件的容量与行长写入缓冲头(`dwCapacity` / `cbItem`),渲染端只按头部字段工作。

---

## WriteCtrlTextToBuffer 函数 (CtrlBuffer)

把中间态文本按该控件的缓冲容量写回其文本区(结构件"外观信息"刷新用)。

### 语法

```cpp
VOID WriteCtrlTextToBuffer(
  [in] CtrlInfo*   pCtrl,
  [in] const char* text
);
```

### 参数

`[in] pCtrl`

类型: **CtrlInfo\***

目标控件;`NULL` 直接返回。无文本缓冲的控件(数值类 / 结构件 / 未知类型)同样直接返回(安全忽略)。

`[in] text`

类型: **const char\***

写入的文本;`NULL` 按空串处理。超出容量部分被截断(截断由 `strncpy_s` 的 `_TRUNCATE` 语义给出)。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

文本首址与容量一律走 `CtrlTextRangeOf`,本函数不自行推算缓冲布局(输入动作控件的变长载荷也由该取址口径换算)。

---

## FreeCtrlBuffer 函数 (CtrlBuffer)

回收一枚控件的自身缓冲(含下拉类控件的选项指针数组),并把 `bufferInfo` 置空。

### 语法

```cpp
VOID FreeCtrlBuffer(
  [in] CtrlInfo* info
);
```

### 参数

`[in] info`

类型: **CtrlInfo\***

目标控件;`NULL` 直接返回(幂等)。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

只回收 `bufferInfo` 指向的缓冲与下拉类的附属块,不动控件对象本身。回收后 `bufferInfo` 置为
`{ NULL, 0 }`,再次调用为空操作。

---

## 要求

| 要求 | 值 |
| :--- | :--- |
| 标头 | `framework/base/CtrlBuffer.h`(`src\framework\base\CtrlBuffer.h`) |
| 实现 | `src\framework\base\CtrlBuffer.cpp`;数据约定与取址为头文件内联 |
| 命名空间 | `CtrlBuffer`(装配与回收函数) · 全局(数据结构、常量与取址函数) |
| 依赖 | `framework/base/ControlSpec.h` · `framework/base/mem/AllocatorHub.h` · `windows.h` · `stdlib.h` · `string.h` · `minwindef.h` · `stdint.h` · `stddef.h` |
| 平台 | Windows 全版本 |
| 线程安全 | 否(装配与回收操作会话分配器与小对象池);数据布局读取与文本取址为纯函数 |
