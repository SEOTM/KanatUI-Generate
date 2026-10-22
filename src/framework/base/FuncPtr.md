# FuncPtr 模块 (src\framework\base\FuncPtr.h)

## 概述

函数指针与配套数据结构类型的声明处:控件事件回调、宿主注入的应用层契约(视图叠加件绘制)、
编解码回调, 以及它们随参数传递的数据结构。

本模块只有类型声明, 没有函数实现:它为"模块之间不互相包含头文件"这条纪律提供共同的函数签名——
上层把能力实现出来, 下层只持函数指针按需调用, 因此依赖方向不由头文件包含关系决定, 而由注入时机决定。
所有回调的缺省态度一致:**未注入即降级**(该能力不驱动、不做, 而不是崩溃或替代实现)。

回调类型统一用 `pfn` 前缀命名, 与 `Render.h` 的能力类型同一口径。

本模块包含以下编程接口:

### 类型

| 类型 | 说明 |
| --- | --- |
| `pfnCtrlEvent` | 控件事件回调:四元组(控件对象 / 操作码 / 参数 / 附加数据)。 |
| `pfnOverlayBefore` | 视图叠加件绘制回调:在某索引控件绘制之前插入一次绘制。 |
| `pfnEncoder` | 数据编码回调:RAW 数据按操作哈希与索引增量编码。 |
| `pfnDecoder` | 数据解码回调:RAW 数据按操作哈希与索引解码。 |

### 结构

| 结构 | 说明 |
| --- | --- |
| `EventInfo` | 控件事件绑定项:事件处理函数指针 + 该处理器要求的缓冲字节数。 |
| `RawDataInfo` | 二进制 RAW 数据:首址 + 字节数 + 有效性标志。 |
| `OperDataInfo` | 编码 / 解码数据:首址 + 字节数。 |

本模块不定义公开常量。

---

## pfnCtrlEvent 类型 (FuncPtr)

控件事件回调。

### 语法

```cpp
using pfnCtrlEvent = VOID(*)(VOID* obj, uint64_t oper, uint64_t param, void* data);
```

### 参数

`[in] obj`

类型: **VOID\***

触发或正在被绘制的控件对象, 通常即该控件 `CtrlInfo` 的地址。回调内可读可写该控件(如写回勾选态)。

`[in] oper`

类型: **uint64_t**

操作码, 64 位三段:低 16 位为公共段、次 16 位为专属段、高 32 位为搭载段; 解析口径见
`ControlSpec.h` 的"区域:操作码(OPER)系统"。

`[in] param`

类型: **uint64_t**

业务参数(行号、槽位号、新值等), 语义随控件类型与操作码而定; 无参数时传 `0`。

`[in] data`

类型: **void\***

附加数据指针; 默认 `nullptr`。有缓冲的控件传其 `bufferInfo.buffer`(缓冲大小见 `EventInfo::bufferSize`)。

### 返回值

类型: **VOID**

此回调不返回值。

### 注解

**语义随控件类型**:按钮 = 点击触发; 输入类 = 提交触发; 标题件 = 标题标志触发;
列表类 = 渲染该列表时按帧调用, 用于填充列表内容。因此同一签名的回调在不同控件上会被以不同节奏调用,
实现方须按控件类型解释 `oper`。

**`param` 的承载约定**:窗口位置一类的二维量按打包值传递, 形态见 `ControlSpec.h` 的 `PackWndPos` /
`UnpackWndPos`; 除此之外 `param` 的解释权在回调实现方。

**调用时机**:在渲染帧内、对应控件绘制或交互结算处调用; 不得在回调里发起新的渲染帧。

---

## pfnOverlayBefore 类型 (FuncPtr)

视图叠加件绘制回调(渲染插入点)。

### 语法

```cpp
using pfnOverlayBefore = VOID(*)(Layout& layout, size_t index);
```

### 参数

`[in] layout`

类型: **Layout&**

正在渲染的布局对象(只读)。回调可从中取该索引控件的几何与状态, 但不得改动布局结构。

`[in] index`

类型: **size_t**

即将绘制的控件索引。调用时该控件**尚未绘制**。

### 返回值

类型: **VOID**

此回调不返回值。

### 注解

用途是让"视图件"(如选中态表达件)画进正在渲染的布局, 而不必把它寄生进布局的控件链。
实现方按索引自行决定是否绘制:索引不匹配即零动作, 渲染出口不感知其语义。

插入点位置决定了层级:回调发生在内容子窗口内、该控件提交之前 ⇒ 叠加件自然处于"被绘制控件之下"。

---

## pfnEncoder 类型 (FuncPtr)

数据编码回调。

### 语法

```cpp
using pfnEncoder = RawDataInfo(*)(
  RawDataInfo* pRawDataInfo,
  const BYTE* pOperHash,
  OperDataInfo* pOperDataInfo,
  DWORD index
);
```

### 参数

`[in] pRawDataInfo`

类型: **RawDataInfo\***

指向二进制 RAW 数据信息(首址 + 字节数 + 有效性标志)。实现方据此读取原始数据。

`[in] pOperHash`

类型: **const BYTE\***

指向 32 字节操作哈希的指针。哈希用于把编码结果与产生它的操作绑定。

`[in] pOperDataInfo`

类型: **OperDataInfo\***

指向编码数据(首址 + 字节数)。失败时由实现方按 `{ nullptr, 0 }` 语义处理。

`[in] index`

类型: **DWORD**

数据索引, 用于在增量编码中定位该条数据。

### 返回值

类型: **RawDataInfo**

编码后的新 RAW 数据信息。**失败时**返回的 `size` 为 `0`、`data` 为 `nullptr`、`isUseful` 为 `FALSE`;
调用方一律以 `data` 是否为空判定失败。

### 注解

编码不修改入参指向的原 RAW 数据, 而是产出新的 RAW 数据信息; 原数据是否仍然有效由返回结构的
`isUseful` 表达(见 `RawDataInfo` 的字段语义)。

索引与哈希的配对由调用方保证:同一 `index` 配不同哈希会得到互不相认的编码结果。

---

## pfnDecoder 类型 (FuncPtr)

数据解码回调。

### 语法

```cpp
using pfnDecoder = OperDataInfo(*)(
  RawDataInfo* pRawDataInfo,
  const BYTE* pOperHash,
  DWORD index
);
```

### 参数

`[in] pRawDataInfo`

类型: **RawDataInfo\***

指向二进制 RAW 数据信息。解码只读它, 不改变其有效性标志。

`[in] pOperHash`

类型: **const BYTE\***

指向 32 字节操作哈希的指针, 须与编码时使用的一致。

`[in] index`

类型: **DWORD**

数据索引, 用于在 RAW 数据中定位该条数据。

### 返回值

类型: **OperDataInfo**

解码出的数据(首址 + 字节数)。**失败时**返回的 `size` 为 `0`、`data` 为 `nullptr`;
调用方一律以 `data` 是否为空判定失败。

### 注解

解码不影响 RAW 数据的有效性(与编码相反):同一份 RAW 数据可以被反复解码。

哈希与索引配对错误时不报错, 只会得到解码失败(`data` 为空)或按实现方口径的退化结果。

---

## EventInfo 结构 (FuncPtr)

控件事件绑定项。

### 语法

```cpp
struct EventInfo
{
    pfnCtrlEvent pfnEvent;
    DWORD        bufferSize;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `pfnEvent` | 指向控件响应函数的指针; `NULL` = 该控件无事件响应。 |
| `bufferSize` | 该处理器要求的缓冲字节数; `0` = 无需缓冲, 非 `0` 时按此为该控件分配缓冲。 |

### 注解

绑定表按事件名把布局文档的 `event` 字段解析成本结构, 结果写进控件的 `pEventHandler` 与缓冲信息。
`bufferSize` 决定控件缓冲容量:分配所得的首址即回调 `data` 入参的来源, 因此处理器需要临时数据时
应当在此声明容量, 而不是在回调里自行分配。

---

## RawDataInfo 结构 (FuncPtr)

二进制 RAW 数据及其有效性标志。

### 语法

```cpp
struct RawDataInfo
{
    const BYTE* data;
    size_t      size;
    BOOL        isUseful;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `data` | 指向二进制 RAW 数据的指针; 空 = 无数据(大小允许为 0)。 |
| `size` | 数据字节数; `0` 表示空数据。 |
| `isUseful` | 有效性标志:解码不影响它; 编码产出新 RAW 后由实现方按"原数据是否仍有效"取值。 |

### 注解

**空数据是合法状态**:`size` 允许为 `0`, 与"失败"不是同一件事——判失败看 `data` 是否为空。

**编码的影响**:编码会产出新的 RAW 数据, 而新数据的大小可能与原数据不同, 因此编码后原数据的有效性与
完整性都可能改变; `isUseful` 就是把这一结论回传给调用方的手段, 当它为 `FALSE` 时原 RAW 数据不可再使用。

**所有权**:`data` 指向的内存在调用方(或编解码实现方)一侧管理, 本结构不释放它。

---

## OperDataInfo 结构 (FuncPtr)

编码 / 解码数据。

### 语法

```cpp
struct OperDataInfo
{
    BYTE*  data;
    size_t size;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `data` | 指向编码 / 解码数据的指针; 空 = 无数据且表示编解码失败。 |
| `size` | 数据字节数; 失败时为 `0`。 |

### 注解

本结构同时充当编码的输入与解码的输出, 因此 `data` 不声明为 `const`:编码实现方可能需要就地写入。
解码路径上不得修改入参内容。

判失败一律看 `data` 是否为空(与 `RawDataInfo` 同一口径)。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/base/FuncPtr.h`(`src\framework\base\FuncPtr.h`) |
| 实现 | 无(类型声明; 回调由绑定方提供) |
| 命名空间 | 全局作用域 |
| 依赖 | `minwindef.h` · `stdint.h`(无工程内头文件依赖: 布局类以不完整类型声明, 由使用方提供完整类型) |
| 平台 | Windows 全版本 |
| 线程安全 | 不适用(值类型, 按值传递) |
