# RawAttributes 模块 (src\doc\RawAttributes.h)

## 概述

`RawAttributes` 是数据保真旁挂表族的读写与扩容: 文档原类型串表与无缓冲控件声明文本表。两张表挂在
文档条目上(条目结构定义在 `doc\Document.h`), 都按文档索引键, 表本体与容量字段不属本模块。

层归属是文档层, 依赖方向为 `doc → framework`(含 `framework\base\mem` 分配原语), 该方向合法。表只承载"原串与
声明文本的原样往返": 控件对象不持有原类型串, 有自身缓冲的控件其文本以缓冲为准, 旁挂表不参与。
条目反查(布局指针 → 条目)属会话面编排, 本体在应用侧, 本模块只按注入的函数指针调用。

本模块包含以下编程接口:

### 类型

| 类型 | 说明 |
| --- | --- |
| `pfnFindLayoutEntry` | 条目反查函数指针: 由布局指针取条目地址, 未命中返回空。 |

### 函数

| 函数 | 说明 |
| --- | --- |
| `SetEntryLookup` | 注入条目反查函数指针(空表示复位)。 |
| `SetRawType` | 记录或覆盖指定文档索引的原类型串。 |
| `GetRawType` | 取指定文档索引的原类型串。 |
| `InsertRawTypeAt` | 在指定文档索引处插入一个原类型串条目。 |
| `RemoveRawTypeAt` | 移除指定文档索引处的原类型串条目。 |
| `SetRawText` | 记录或覆盖无缓冲控件的声明文本。 |
| `GetRawText` | 取声明文本(控件已有自身缓冲时返回空)。 |
| `InsertRawTextAt` | 在指定文档索引处插入一个文本条目。 |
| `RemoveRawTextAt` | 移除指定文档索引处的文本条目。 |

---

## pfnFindLayoutEntry 类型 (RawAttributes)

条目反查函数指针。

### 语法

```cpp
typedef LayoutEntry* (*pfnFindLayoutEntry)(Layout* layout);
```

### 成员

| 成员 | 说明 |
| --- | --- |
| 参数 `layout` | 待反查的布局指针, 允许为空。 |
| 返回值 | 命中的条目地址; 未命中返回空。 |

### 注解

实现由应用侧提供: 预览基座归当前激活条目, 其余按条目数组线性反查, 并覆盖快照条目与内嵌成员条目。
本模块不认识这些来源, 只按指针调用; 因此未注入时一切读写都按"未就绪"失败。

---

## SetEntryLookup 函数 (RawAttributes)

注入条目反查函数指针。

### 语法

```cpp
VOID SetEntryLookup(
  [in] pfnFindLayoutEntry pfnLookup
);
```

### 参数

`[in] pfnLookup`

类型: **pfnFindLayoutEntry**

反查函数指针。传空表示复位, 此后所有读写按未就绪失败。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

必须在"分配成员存储"之后、启动加载链之前注入, 并与文档与预览会话的注入同点同序; 释放与初始化失败
清理时复位为空。就绪判据与成员块同刻成立、同刻失效。

---

## SetRawType 函数 (RawAttributes)

记录或覆盖指定文档索引的原类型串。

### 语法

```cpp
BOOL SetRawType(
  [in] Layout* layout,
  [in] size_t  index,
  [in] LPCSTR  rawType
);
```

### 参数

`[in] layout`

类型: **Layout\***

控件所属布局。允许为空(此时返回失败)。

`[in] index`

类型: **size_t**

文档索引, 合法域 `0`..`GetTotalNum() - 1`。

`[in] rawType`

类型: **LPCSTR**

原类型串。传空表示把该条目置空。

### 返回值

类型: **BOOL**

成功返回成功; 未注入、布局为空、条目反查未命中、索引越界或分配失败均返回失败, 表保持原状。

### 注解

表按需惰性建立: 首次写入时建表, 容量取当时的文档控件数; 此后容量只增不减, 文档控件数超过容量时
迁移扩容(旧表确属小对象池才回收)。写入内容与既有条目相同时复用既有串, 不重复分配。

---

## GetRawType 函数 (RawAttributes)

取指定文档索引的原类型串。

### 语法

```cpp
LPCSTR GetRawType(
  [in] Layout* layout,
  [in] size_t  index
);
```

### 参数

`[in] layout`

类型: **Layout\***

控件所属布局。允许为空。

`[in] index`

类型: **size_t**

文档索引。

### 返回值

类型: **LPCSTR**

原类型串; 表内为空、表未建立、索引越界或未就绪时返回空。

### 注解

返回的串由会话分配器持有, 调用方不得释放。空返回同时表示"未记录"与"已置空", 两者语义相同。

---

## InsertRawTypeAt 函数 (RawAttributes)

在指定文档索引处插入一个原类型串条目。

### 语法

```cpp
BOOL InsertRawTypeAt(
  [in] Layout* layout,
  [in] size_t  index,
  [in] LPCSTR  rawType
);
```

### 参数

`[in] layout`

类型: **Layout\***

控件所属布局。允许为空。

`[in] index`

类型: **size_t**

插入位置, 合法域 `0`..文档控件数。

`[in] rawType`

类型: **LPCSTR**

新条目的原类型串, 允许为空。

### 返回值

类型: **BOOL**

成功返回成功; 未就绪、反查未命中、越界或分配失败返回失败。

### 注解

插入后其后条目整体后移, 与控件插入必须同步调用, 否则旁挂表与文档索引错位。

---

## RemoveRawTypeAt 函数 (RawAttributes)

移除指定文档索引处的原类型串条目。

### 语法

```cpp
BOOL RemoveRawTypeAt(
  [in] Layout* layout,
  [in] size_t  index
);
```

### 参数

`[in] layout`

类型: **Layout\***

控件所属布局。允许为空。

`[in] index`

类型: **size_t**

待移除条目的文档索引。

### 返回值

类型: **BOOL**

成功返回成功; 未就绪、反查未命中或越界返回失败。

### 注解

移除后其后条目整体前移, 末位补空, 表容量不变, 因此表与文档索引继续一一对应。必须与控件删除同步调用。

---

## SetRawText 函数 (RawAttributes)

记录或覆盖无缓冲控件的声明文本。

### 语法

```cpp
BOOL SetRawText(
  [in] Layout* layout,
  [in] size_t  index,
  [in] LPCSTR  text
);
```

### 参数

`[in] layout`

类型: **Layout\***

控件所属布局。允许为空。

`[in] index`

类型: **size_t**

文档索引。

`[in] text`

类型: **LPCSTR**

声明文本。传空表示把该条目置空。

### 返回值

类型: **BOOL**

成功返回成功; 未就绪、反查未命中、越界或分配失败返回失败。

### 注解

机制与 `SetRawType` 相同(惰性建表、容量只增不减、内容未变复用既有串)。本表只用于没有自身缓冲的控件。

---

## GetRawText 函数 (RawAttributes)

取声明文本。

### 语法

```cpp
LPCSTR GetRawText(
  [in] Layout* layout,
  [in] size_t  index
);
```

### 参数

`[in] layout`

类型: **Layout\***

控件所属布局。允许为空。

`[in] index`

类型: **size_t**

文档索引。

### 返回值

类型: **LPCSTR**

声明文本; 表内为空、表未建立、索引越界、未就绪返回空。

### 注解

控件一旦有自身缓冲, 本函数返回空 —— 文本以控件缓冲为准, 旁挂表条目不参与。调用方需要"有效文本"
时应先判缓冲再回落到本函数。

---

## InsertRawTextAt 函数 (RawAttributes)

在指定文档索引处插入一个文本条目。

### 语法

```cpp
BOOL InsertRawTextAt(
  [in] Layout* layout,
  [in] size_t  index,
  [in] LPCSTR  text
);
```

### 参数

`[in] layout`

类型: **Layout\***

控件所属布局。允许为空。

`[in] index`

类型: **size_t**

插入位置, 合法域 `0`..文档控件数。

`[in] text`

类型: **LPCSTR**

新条目的声明文本, 允许为空。

### 返回值

类型: **BOOL**

成功返回成功; 未就绪、反查未命中、越界或分配失败返回失败。

### 注解

与 `InsertRawTypeAt` 同口径: 其后条目整体后移, 必须与控件插入同步调用。

---

## RemoveRawTextAt 函数 (RawAttributes)

移除指定文档索引处的文本条目。

### 语法

```cpp
BOOL RemoveRawTextAt(
  [in] Layout* layout,
  [in] size_t  index
);
```

### 参数

`[in] layout`

类型: **Layout\***

控件所属布局。允许为空。

`[in] index`

类型: **size_t**

待移除条目的文档索引。

### 返回值

类型: **BOOL**

成功返回成功; 未就绪、反查未命中或越界返回失败。

### 注解

与 `RemoveRawTypeAt` 同口径: 其后条目整体前移、末位补空、容量不变, 必须与控件删除同步调用。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `doc/RawAttributes.h`(`src\doc\RawAttributes.h`) |
| 实现 | `src\doc\RawAttributes.cpp` |
| 命名空间 | 全局(自由函数 + 函数指针类型) |
| 依赖 | 工程内: `Layout`(`GetTotalNum` / `GetCtrlInfo`)、`Document`(`LayoutEntry` 字段)、`AllocatorHub`(`AllocSmall` / `FreeSmall` / `CopyString` / `GetArena`); 系统: 无额外链接库 |
| 平台 | Windows; 尺寸与指针假设按 64 位构建 |
| 线程安全 | 否(旁挂表与会话分配器均无同步; 读写需与文档结构变更串行) |
