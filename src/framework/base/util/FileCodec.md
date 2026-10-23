# FileCodec 模块 (src\framework\base\util\FileCodec.h)

## 概述

字节处理件:对"一段字节"做纯校验 / 摘要, 现提供 CRC32。本模块**不感知文档结构**, 只处理字节;
校验码写在文件格式的哪个字段、覆盖哪些字节, 属文件格式契约, 由调用方决定, 本模块只提供算法。

CRC32 口径 = IEEE 802.3:反射, 多项式 `0xEDB88320`, 初值 `0xFFFFFFFF`, 结果取反。入口 = 流式三件套,
用于覆盖不连续区间(如校验码字段自身按 0 参与计算: 先推进 4 个零字节, 再推进其余正文)。

本模块属 **framework 层 · base · util**(2026-10-23 自 common 归位), 无可执行实例、无状态、不分配内存。

本模块包含以下编程接口:

### 类

| 类 | 说明 |
| --- | --- |
| `FileCodec` | CRC32 校验工具类(纯静态, 不可实例化)。 |

---

## FileCodec 类 (FileCodec)

CRC32 校验工具类。

### 语法

```cpp
class FileCodec
{
public:
    static DWORD Crc32Begin();
    static DWORD Crc32Update(DWORD dwCrc, LPCVOID data, size_t cbData);
    static DWORD Crc32Finish(DWORD dwCrc);
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `Crc32Begin()` | 取流式计算的初始状态 `0xFFFFFFFF`。 |
| `Crc32Update(dwCrc, data, cbData)` | 用一段字节推进状态; 可连续多次调用以覆盖不连续区间。 |
| `Crc32Finish(dwCrc)` | 对推进后的状态取反, 得到最终校验值。 |

### 注解

`Crc32Update`

参数 `dwCrc` 为上一次的状态(`Crc32Begin` 或上一次 `Crc32Update` 的返回值); `data` 为字节首址,
为 `NULL` 时**原样返回** `dwCrc`(不推进); `cbData` 为字节数。返回推进后的状态, **尚未取反**,
须经 `Crc32Finish` 才能与写入值比较。

`Crc32Finish`

参数 `dwCrc` 为流式推进后的状态; 返回按位取反后的校验值。

算法与状态均无副作用, 可对同一段字节重复计算; 结果只取决于字节内容与调用顺序。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/base/util/FileCodec.h`(`src\framework\base\util\FileCodec.h`) |
| 实现 | `src\framework\base\util\FileCodec.cpp` |
| 命名空间 | 全局(类) |
| 依赖 | 无 |
| 平台 | Windows 全版本 |
| 线程安全 | 是(纯函数, 无状态) |
