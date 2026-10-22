# FileCodec 模块 (src\common\FileCodec.h)

## 概述

`FileCodec` 是对"一段字节"做纯校验与摘要的静态工具类: 当前提供 CRC32(IEEE 802.3)。它不感知文档
结构 —— 不引用布局模型, 也不知道校验码在文件里的位置与覆盖范围, 那些属文件格式契约, 由调用方按
契约决定对哪些字节、以何顺序调用本模块。

层归属是公共基础设施层, 无工程内依赖。类为纯静态工具类: 构造与析构被删除, 不建立实例。
版本号低位标记的压缩与加密尚未实现, 实现时落在本模块内。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `Crc32Begin` | 取 CRC32 的初始状态(流式计算入口)。 |
| `Crc32Update` | 用一段字节推进 CRC32 状态。 |
| `Crc32Finish` | 对状态取反, 得到最终校验值。 |
| `Crc32` | 一次算完一段字节的 CRC32。 |

---

## Crc32Begin 函数 (FileCodec)

取 CRC32 的初始状态, 作为流式计算的入口。

### 语法

```cpp
static DWORD Crc32Begin();
```

### 参数

此函数无参数。

### 返回值

类型: **DWORD**

初始状态, 固定为 `0xFFFFFFFF`。

### 注解

本函数与 `Crc32Update` / `Crc32Finish` 构成流式三件套, 供"要覆盖不连续字节区间"的调用方使用,
例如校验码字段自身按零参与计算: 先推进入字段的四个零字节, 再推进其余正文。

---

## Crc32Update 函数 (FileCodec)

用一段字节推进 CRC32 状态。

### 语法

```cpp
static DWORD Crc32Update(
  [in] DWORD   dwCrc,
  [in] LPCVOID data,
  [in] size_t  cbData
);
```

### 参数

`[in] dwCrc`

类型: **DWORD**

上一次的状态, 取自 `Crc32Begin` 或上一次 `Crc32Update` 的返回值。

`[in] data`

类型: **LPCVOID**

字节首址。允许为 `NULL`。

`[in] cbData`

类型: **size_t**

字节数, 可为 `0`。

### 返回值

类型: **DWORD**

推进后的状态, 尚未取反 —— 必须再经 `Crc32Finish` 才能与写入文件的校验值比较。
`data` 为 `NULL` 时原样返回入参状态。

### 注解

算法是反射式: 每字节异或进状态后逐位右移, 按多项式 `0xEDB88320` 决定是否异或。可连续多次调用,
把不连续的多个区间按顺序拼接计算; 调用顺序即字节顺序, 顺序不同结果不同。

---

## Crc32Finish 函数 (FileCodec)

对状态取反, 得到最终校验值。

### 语法

```cpp
static DWORD Crc32Finish(
  [in] DWORD dwCrc
);
```

### 参数

`[in] dwCrc`

类型: **DWORD**

流式推进后的状态。

### 返回值

类型: **DWORD**

最终校验值(状态按位取反)。

### 注解

取反是 CRC32 标准定义的一部分, 不是额外加工。单独调用 `Crc32Update` 得到的值不应直接写盘或比较。

---

## Crc32 函数 (FileCodec)

一次算完一段字节的 CRC32。

### 语法

```cpp
static DWORD Crc32(
  [in] LPCVOID data,
  [in] size_t  cbData
);
```

### 参数

`[in] data`

类型: **LPCVOID**

字节首址。允许为 `NULL`。

`[in] cbData`

类型: **size_t**

字节数, 可为 `0`。

### 返回值

类型: **DWORD**

校验值; `data` 为 `NULL` 时返回 `0`。

### 注解

等价于 `Crc32Finish(Crc32Update(Crc32Begin(), data, cbData))`。注意空指针与零长度语义不同:
空指针返回 `0`, 零长度返回"空区间的 CRC32"(初值取反), 两者不相等。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `common/FileCodec.h`(`src\common\FileCodec.h`) |
| 实现 | `src\common\FileCodec.cpp` |
| 命名空间 | 全局(类 `FileCodec`, 纯静态) |
| 依赖 | 工程内: 无; 系统: 无额外链接库 |
| 平台 | Windows; 算法与平台无关 |
| 线程安全 | 是(纯函数, 无静态可变状态) |
