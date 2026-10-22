# UIEventHandler 模块 (src\framework\dispatch\UIEventHandler.h)

## 概述

UI 事件处理的绑定表:纯静态类, 承载"布局中控件的事件名 → 处理函数"的映射。控件名解析出 event 名之后经本
模块换成回调指针, 让控件在触发时能调回宿主侧的处理器。

绑定表由宿主注入而不由本模块内建:本模块属 framework 层, 而表的内容是应用侧的处理函数, 直接引用会形成
framework 到应用层的反向依赖; 宿主在启动期把表交给本模块, 本模块只认 `pfnCtrlEvent` 这个契约层类型,
不认识任何具体模块。

本模块属 **framework 层**, 依赖 `framework/base/FuncPtr.h`(回调类型与事件信息结构), 不参与事件时机判定 ——
需要帧间执行的处理器由处理器自身挂到帧间队列(见 `framework/dispatch/FrameEvent.h`)。未登记的事件名解析为空绑定,
控件因此无响应; 这是静默降级, 不崩、不报错。

本模块包含以下编程接口:

### 类型

| 类型 | 说明 |
| --- | --- |
| `EventBinding` | 一条"事件名 → 处理函数"绑定: 完整名或前缀家族。 |

### 函数

| 函数 | 说明 |
| --- | --- |
| `SetBindings` | 注入绑定表(只存指针, 不复制); 传空即清空。 |
| `BindEventHandler` | 按事件名取绑定的事件处理函数; 未覆盖的名字返回空绑定。 |

本模块不定义公开常量。注入进来的表指针与条目数、以及线性查表的循环均为文件内静态实现细节, 不立条。

---

## EventBinding 结构 (UIEventHandler)

一条"事件名 → 处理函数"绑定。一个条目按 `chDigitLo` 的取值分两种匹配形态: 完整名或前缀家族。

### 语法

```cpp
struct EventBinding
{
    LPCSTR       szEvent;
    pfnCtrlEvent pfn;
    CHAR         chDigitLo;
    CHAR         chDigitHi;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `szEvent` | 事件名(完整名)或前缀家族的前缀。 |
| `pfn` | 处理函数, 非 `NULL`(空项在查表时被跳过)。 |
| `chDigitLo` | 前缀家族的数字下界; `'\0'` = 本项按完整名精确匹配。 |
| `chDigitHi` | 前缀家族的数字上界; 仅在 `chDigitLo` 非 `'\0'` 时有意义。 |

### 注解

两种匹配形态的判据在 `chDigitLo` 上:

- 完整名(`chDigitLo == '\0'`): 与 `szEvent` 逐字符精确比较, 全等才算命中。
- 前缀家族(`chDigitLo != '\0'`): `szEvent` 作为前缀, 其后须恰有一位数字且落在
  [`chDigitLo`, `chDigitHi`] 闭区间内, 且再之后即字符串结束(例: 前缀 `GENUI_EXP_R` 配 `'0'` 到 `'5'`
  覆盖 `GENUI_EXP_R0` 到 `GENUI_EXP_R5`)。前缀之后还有多余字符(例如两位数字)不算命中。

表按数组序线性匹配、命中即止 ⇒ 若表中出现相互重叠的条目, 先出现者生效; 现行表内各条互不重叠,
因此顺序不影响结果。`szEvent` 为 `NULL` 或 `pfn` 为 `NULL` 的条目在查表时被跳过(容错)。

表的所有权归调用方: 本模块只保存首址与条目数, 不复制、不释放表内存 ⇒ 表须在进程级常驻,
且其生命周期须覆盖全部控件装配与事件触发。

---

## SetBindings 函数 (UIEventHandler)

注入"事件名 → 处理函数"绑定表。

### 语法

```cpp
static VOID SetBindings(
  [in] CONST EventBinding* pTable,
  [in] size_t             nCount
);
```

### 参数

`[in] pTable`

类型: **CONST EventBinding\***

绑定表首址。可为 `NULL`, 与 `nCount` 为 `0` 同效: 清空注入。表内存由调用方持有, 须进程级常驻 ——
本模块只存指针, 不复制条目的任何字段。

`[in] nCount`

类型: **size_t**

条目数。只有 `pTable` 非 `NULL` 时才被采用; `pTable` 为 `NULL` 时该值被忽略并记为 `0`。
调用方须保证前 `nCount` 项都在表内。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

调用时机: 须早于首次装配控件 —— 控件装配时即解析 event 名, 此后再注入不会追认已装配的控件。
重复注入以最后一次为准(旧表指针整体被替换, 不做合并)。

清空注入后, 全部 event 名一律解析为空绑定(控件无响应); 未注入时同样是这一口径 —— 静默降级,
不崩、不打印。本函数不校验表内容, 条目合法性(处理函数非空、前缀家族区间有效)由调用方保证。

---

## BindEventHandler 函数 (UIEventHandler)

按事件名取绑定的事件处理函数。

### 语法

```cpp
static EventInfo BindEventHandler(
  [in] LPCSTR eventname
);
```

### 参数

`[in] eventname`

类型: **LPCSTR**

待解析的事件名(以 `\0` 结尾)。为 `NULL` 时直接返回空绑定。字符串只需在调用期内有效; 本模块不保存它,
只做逐字符比较。

### 返回值

类型: **EventInfo**

`pfnEvent` = 命中的处理函数; 未命中、未注入或 `eventname` 为 `NULL` 时为 `NULL`(空绑定, 控件无响应)。
`bufferSize` 恒为 `0`: 本函数只填处理函数一项, 不表达"该处理器要求的缓冲字节数", 需要该信息时由控件层
另行取用。

### 注解

匹配按表序线性进行、命中即止; 每个条目先跳过空项, 再按其形态走完整名比较或前缀家族比较。前缀家族的比较
在前缀长度处即判不等时短路 ⇒ 事件名短于前缀时不会越界读。

本函数只读, 不改注入状态、不产生诊断输出。未命中是正常路径(控件无响应), 不视为错误。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/dispatch/UIEventHandler.h`(`src\framework\dispatch\UIEventHandler.h`) |
| 实现 | `src\framework\dispatch\UIEventHandler.cpp`;`EventBinding` 结构定义在头文件内 |
| 命名空间 | `UIEventHandler`(命名空间内含 `EventBinding` 结构) |
| 依赖 | `framework/base/FuncPtr.h` · `minwindef.h` · `ucrt` |
| 平台 | Windows 全版本 |
| 线程安全 | 否; 绑定表为进程级静态, 注入与解析须由调用方串行 |
