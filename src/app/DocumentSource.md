# DocumentSource 模块 (src\app\DocumentSource.h)

## 概述

`DocumentSource` 是应用启动加载链的落点: 按固定级序探测可用的文档源, 命中即用, 并以 `DocSource` 回传来源。
四级顺序 = exe 目录外部文件 `.\KanatUIGenerate.bin` → exe 目录外部文件 `.\KanatUIGenerate.json` → 随 exe 内置的 bin 资源 →
随 exe 内置的 JSON 资源; 前两级返回池内路径, 后两级经 Win32 资源 API 取件、拷进会话内存池并补 `'\0'`。
级序是口径、形态由内容决定: `bBin` 只是来源标注, 四条读入口一律由载入侧按内容嗅探分派解析。

本模块只探测并给出来源: 不做载入(解析与构建归载入侧), 不做"载入被拒 ⇒ 该级并入已试掩码 ⇒ 继续探测"的
循环(归调用方), 也不做通用加载原语(exe 目录缓存 / 路径拼接归
`framework\dispatch\ResourceLoader`; 资源节读取与补 `'\0'` 由本文件完成); 本模块不持状态, 无静态字段、无初始化与释放步骤, 内存取自会话分配器。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `ResolveDocumentSource` | 按四级顺序探测可用文档源, 命中即用并填写结果。 |
| `GetEmbeddedDoc` | 取随 exe 内置的 JSON 文档资源(第 4 级来源)。 |
| `GetEmbeddedDocBin` | 取随 exe 内置的 bin 文档资源(第 3 级来源)。 |

### 类型

| 类型 | 说明 |
| --- | --- |
| `DocSource` | 一次探测的结果形状: 命中级 / 形态 / 是否内置 / 描述 / 路径或数据。 |

---

## ResolveDocumentSource 函数 (DocumentSource)

按四级顺序探测可用文档源, 命中即用, 并把命中信息写入结果结构。

### 语法

```cpp
BOOL ResolveDocumentSource(
  [in]  DocSource* pOut,
  [in]  DWORD      dwLevelMask = 0xFu
);
```

### 参数

`[in] pOut`

类型: **DocSource\***

结果写回处。传 `NULL` 时直接返回 `FALSE` 且不写任何字段。非空时入口先整体零初始化, 再按命中级填写对应
字段: 第 1/2 级写 `lpPath`(`pData` / `cbData` 保持 0), 第 3/4 级写 `pData` / `cbData`(`lpPath` 保持
`NULL`); 四级均未命中时保持零初始化形态, 即 `nLevel == 0`。

`[in] dwLevelMask`

类型: **DWORD**

允许尝试的级别掩码: bit0..bit3 = 第 1..4 级, 默认 `0xF`(全部开放)。未开放的级次不做任何尝试, 也不产生
该级诊断。掩码可在多次调用之间变化, 本函数不对其做归一化或记忆。

### 返回值

类型: **BOOL**

`TRUE` = 某级命中, `pOut` 已按该级填写; `FALSE` = `pOut` 为 `NULL`, 或掩码允许的级次全部不可用。

### 注解

级序即口径: 第 1 级 `.\KanatUIGenerate.bin`(exe 目录外部文件, bin 形态, 掩码位 `0x1`)→ 第 2 级
`.\KanatUIGenerate.json`(exe 目录外部文件, JSON 形态, `0x2`)→ 第 3 级 `resource.res\KanatUIGenerate.bin`(内置资源, bin,
`0x4`)→ 第 4 级 `resource.res\KanatUIGenerate.json`(内置资源, JSON, `0x8`)。命中即止, 不再探测后续级别。

第 1/2 级的命中判据是文件存在(属性查询不是 `INVALID_FILE_ATTRIBUTES`), 本函数不打开、不解析文件; 畸形文件
在载入时被拒 ⇒ 由调用方关闭该级并回落后续级别, 不越级。第 3/4 级的命中判据是资源存在; 资源存在但取件或
池分配失败时按该级"未命中"处理, 继续下一级。

形态标注不参与分派: 四条读入口一律经载入侧按内容嗅探(跳过 BOM 与空白后首字节为 `'{'` ⇒ JSON, 否则 bin),
因此把 bin 文件放到第 2 级也应能载入, 反之亦然。

本函数只返回一个命中结果, 逐级回落的循环归调用方: "载入被拒 ⇒ 把该级并入已试掩码 ⇒ 以剩余掩码再次调用"
的循环中, 掩码语义与返回码原样保留 ⇒ 本函数必须可被重复调用, 每次重新探测、不缓存上次命中。

第 3/4 级返回的 `pData` 是会话内存池中的副本, 末尾已补 `'\0'`, `cbData` 不含该字节; 所有权归池, 调用方
不得释放, 应直接交给内存载入入口(`app\DocumentIO` 的内存载入与 `app\PreviewSession` 的外壳快照内存入口都
要求 NUL 结尾)。默认不向 exe 目录投放 `.\KanatUIGenerate.bin` ⇒ 现行启动通常在第 1 级未命中、在第 2 级命中。

每次实际尝试都会输出一行该级诊断(命中 / 未命中及原因), 级次回退的诊断责任在本函数, 不在调用方。级次全不
可用时不构造任何内置文档替身(不内置代码回退), 让调用方的启动失败路径接管。

---

## GetEmbeddedDoc 函数 (DocumentSource)

取随 exe 内置的 JSON 文档资源, 拷贝进会话内存池并补 `'\0'`。

### 语法

```cpp
BOOL GetEmbeddedDoc(
  [out] LPVOID* ppData,
  [out] DWORD*  pcbData
);
```

### 参数

`[out] ppData`

类型: **LPVOID\***

池内副本地址的写回处。成功时非 `NULL`, 且缓冲区末尾已补一个 `'\0'`(JSON 解析要求 NUL 结尾)。

`[out] pcbData`

类型: **DWORD\***

字节数的写回处, 不含补的 `'\0'`。成功时有效; 失败时两个出参的内容不作约定, 调用方不得使用。

### 返回值

类型: **BOOL**

`TRUE` = 命中, 两个出参均有效; `FALSE` = 未内嵌(无资源节 / 无该资源 ID)或取件与池分配失败。

### 注解

资源 ID 与类型见 `src\app\resource.h` 的 `IDR_KANATUI_GENERATE_JSON`(类型 `"JSON"`)。取件与池分配由
本文件的 `LoadEmbeddedResource` 完成, 本函数只固定资源 ID、类型与"补 `'\0'`"这三项口径。

唯一消费点在本模块内的 `ResolveDocumentSource` 第 4 级 ⇒ 对外视为内部取件件, 不单独使用; 需要文档数据时
走 `ResolveDocumentSource` 取结果, 由它决定用哪一级。

返回的缓冲随会话内存池整体释放, 本函数不提供归还入口, 也不缓存句柄: 每次调用都重新拷贝一份。

---

## GetEmbeddedDocBin 函数 (DocumentSource)

取随 exe 内置的 bin 文档资源, 同样拷进会话内存池并补 `'\0'`。

### 语法

```cpp
BOOL GetEmbeddedDocBin(
  [out] LPVOID* ppData,
  [out] DWORD*  pcbData
);
```

### 参数

`[out] ppData`

类型: **LPVOID\***

池内副本地址的写回处。成功时非 `NULL`, 末尾已补 `'\0'`(与 JSON 入口统一持有口径, bin 解析本身不要求)。

`[out] pcbData`

类型: **DWORD\***

字节数的写回处, 不含补的 `'\0'`。

### 返回值

类型: **BOOL**

`TRUE` = 命中, 两个出参均有效; `FALSE` = 未内嵌或取件与池分配失败。

### 注解

资源 ID 与类型见 `src\app\resource.h` 的 `IDR_KANATUI_GENERATE_BIN`(类型 `"BIN"`)。除资源 ID、类型与形态标注外,
语义与 `GetEmbeddedDoc` 完全一致: 会话池副本、末尾补 `'\0'`、所有权归池、唯一消费点在本模块内
(`ResolveDocumentSource` 第 3 级)。

补 `'\0'` 对 bin 不是解析需求, 而是让两条内置路径返回的缓冲具备相同的可持有形态; 判定实际形态仍归载入侧的
内容嗅探, 不由本函数决定。

---

## DocSource 结构 (DocumentSource)

一次文档源探测的结果形状。

### 语法

```cpp
struct DocSource
{
  INT    nLevel;
  BOOL   bBin;
  BOOL   bEmbedded;
  LPCSTR lpDesc;
  LPCSTR lpPath;
  LPVOID pData;
  DWORD  cbData;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `nLevel` | 命中级 `1`..`4`; `0` = 四级均未命中(零初始化的默认值)。 |
| `bBin` | 该级目标是否为 bin 形态(仅第 1/3 级为 `TRUE`); 来源标注, 不参与解析分派。 |
| `bEmbedded` | 是否来自随 exe 内置的资源(第 3/4 级为 `TRUE`); `FALSE` = exe 目录下的外部文件。 |
| `lpDesc` | 来源描述文案(池内字符串), 用于诊断与回显。 |
| `lpPath` | 外部文件路径(第 1/2 级命中时有效; 池内字符串)。 |
| `pData` | 内置资源数据(第 3/4 级命中时有效; 池内副本, 末尾已补 `'\0'`)。 |
| `cbData` | `pData` 的字节数, 不含补的 `'\0'`; 仅第 3/4 级有效。 |

### 注解

值语义结构: 无构造与析构约定, 可整体拷贝。它不持有也不释放指针字段指向的存储 —— `lpPath` / `lpDesc` /
`pData` 均指向会话内存池, 池整体释放前一直有效, 之后一并失效。

字段有效域由 `nLevel` 决定, 其它字段在非本级的形态下无意义: 1/2 级只填 `lpPath`(与 `lpDesc`、`bBin`、
`bEmbedded`), 3/4 级只填 `pData` / `cbData`(与 `lpDesc`、`bBin`、`bEmbedded`)。`nLevel == 0` 表示四级
全未命中, 除零值外不提供其它信息。

该结构是一次探测的快照, 不随后续状态变化; 需要最新结果时再次调用 `ResolveDocumentSource`。填写者唯一:
除 `ResolveDocumentSource` 外没有其它接口写它。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `app/DocumentSource.h`(`src\app\DocumentSource.h`) |
| 实现 | `src\app\DocumentSource.cpp` |
| 命名空间 | 命名空间 `DocumentSource`(三个函数) + 全局结构 `DocSource` |
| 依赖 | 工程内: `framework\dispatch\ResourceLoader`(`GetFilePath`)、`framework\base\mem\AllocatorHub`(资源节副本)、`app\resource.h`(`IDR_KANATUI_GENERATE_JSON` / `IDR_KANATUI_GENERATE_BIN`); 系统: `kernel32`(文件属性查询与资源 API)、CRT(`printf`) |
| 平台 | Windows; 第 3/4 级依赖随 exe 携带的资源节, 缺失时该两级按"未命中"处理 |
| 线程安全 | 否(无内部同步; 结果写入调用方提供的结构, 取件内存来自共享会话池 ⇒ 跨线程调用需外部串行) |
