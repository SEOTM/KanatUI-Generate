# AllocatorHub 模块 (src\framework\base\mem\AllocatorHub.h)

## 概述

会话分配器的持有处:应用统一线性分配器(`Arena`)与小对象池(`MemPool`)的建立、持有与分配原语。
进程级单例语义——分配器以命名空间内静态状态持有, 全进程只有一份; 调用方不持有分配器对象, 只调本模块原语。

本模块属 **framework 层 · base · mem**(2026-10-23 自 common 归位), 依赖同目录 `MemoryManager` 的两个分配器类型;
不感知文档结构, 不做引用计数, 不提供任何线程同步。
分配策略 = "小对象优先走池, 超限或池分配失败回退线性分配器"。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `Init` | 建立会话分配器(应用启动期调用一次; 已建立时先释放再重建)。 |
| `Release` | 释放线性分配器(应用退出期调用一次; 幂等)。 |
| `GetArena` | 取应用统一线性分配器; 未建立返回 `NULL`。 |
| `GetSmallPool` | 取应用小对象池; 恒非 `NULL`。 |
| `AllocBuffer` | 从线性分配器分配一段内存。 |
| `AllocSmall` | 小对象分配: 池优先, 超限或失败回退线性分配器。 |
| `FreeSmall` | 回收一次 `AllocSmall` 的产物; 仅当指针确属小对象池时才真正回收。 |
| `CopyString` | 把字符串复制到线性分配器并返回其指针。 |

本模块不定义公开类型与公开常量。

---

## Init 函数 (AllocatorHub)

建立会话分配器。

### 语法

```cpp
BOOL Init(size_t cbArena);
```

### 参数

`[in] cbArena`

类型: **size_t**

线性分配器容量(字节)。按系统页大小向上对齐后提交; 传 `0` 时建立的是容量为 `0` 的分配器
(实现不做缺省容量替换), 此后所有分配返回空。

### 返回值

类型: **BOOL**

`TRUE` = 分配器对象已建立; `FALSE` = 建立失败。

返回值只反映分配器对象是否建立, 不保证底层内存提交成功——`Arena` 构造内部提交失败时容量归零,
`Init` 仍返回 `TRUE`, 后续分配原语一律返回空。调用方首次分配失败即应视为不可用。

### 注解

调用时机为应用启动期, 一次即可。已存在分配器时先 `Release` 再重建, 因此重复调用是安全的,
但此前分配的内存随旧分配器一起失效。

小对象池是值对象, 随模块静态存储期存在, 不随 `Init` 建立或重建。

---

## Release 函数 (AllocatorHub)

释放线性分配器。

### 语法

```cpp
VOID Release();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

幂等: 未建立时调用无副作用。释放后 `GetArena` 返回 `NULL`, 会话语义上的线性分配内存全部失效
(其中对象不得再访问)。

小对象池**不复位**: 池内 slab、空闲链表与已分配块在 `Release` 后仍保持原状, 随进程结束回收。
因此 `Release` 只用于应用退出期, 不要指望它回收小对象。

---

## GetArena 函数 (AllocatorHub)

取应用统一线性分配器。

### 语法

```cpp
Arena* GetArena();
```

### 参数

此函数无参数。

### 返回值

类型: **Arena\***

线性分配器指针; 未 `Init` 或已 `Release` 时返回 `NULL`。返回的指针由模块持有, 调用方不得释放。

### 注解

调用方通常只在启动期取一次并自行保存, 以便直接调用 `Arena` 的分配与复位原语。
`Release` 之后该指针失效。

---

## GetSmallPool 函数 (AllocatorHub)

取应用小对象池。

### 语法

```cpp
MemPool* GetSmallPool();
```

### 参数

此函数无参数。

### 返回值

类型: **MemPool\***

小对象池指针; **恒非 `NULL`**(池为静态值对象, 随进程存续)。返回的指针由模块持有, 调用方不得释放。

### 注解

池服务于不定生命周期的小对象, 单次请求上限为 `MemPool::MAX_SMALL_SIZE`(4096 字节)。
超出上限的请求由 `MemPool::Allocate` 返回空, 不自动回退——需要回退时用 `AllocSmall`。

---

## AllocBuffer 函数 (AllocatorHub)

从线性分配器分配一段内存。

### 语法

```cpp
BufferInfo AllocBuffer(size_t size);
```

### 参数

`[in] size`

类型: **size_t**

请求字节数。`0` 或线性分配器未就绪时返回空信息。

### 返回值

类型: **BufferInfo**

`buffer` = 内存首址(失败为 `nullptr`), `size` = **请求大小**(不是对齐后的实际占用)。
判空一律看 `buffer`。

### 注解

分配不初始化内存。该内存随 `Reset` / `Release` 整体失效, 不能单独释放。
容量不足时返回空, 不扩容、不抛错。

---

## AllocSmall 函数 (AllocatorHub)

小对象分配: 先取小对象池, 超限或池分配失败时回退线性分配器。

### 语法

```cpp
BufferInfo AllocSmall(size_t size);
```

### 参数

`[in] size`

类型: **size_t**

请求字节数。`size` 大于 `0` 且不超过 `MemPool::MAX_SMALL_SIZE`(4096)时先走池;
超限、池分配失败或 `size` 为 `0` 时回退线性分配器(结果为请求为空)。

### 返回值

类型: **BufferInfo**

`buffer` = 内存首址(失败为 `nullptr`), `size` = 请求大小。判空看 `buffer`。

### 注解

两条路径的回收方式不同: 池内块可经 `FreeSmall` 单独回收; 回退到线性分配器的块随整体释放,
`FreeSmall` 对其静默忽略。调用方无需知道本次走的是哪条路径, 统一交 `FreeSmall` 即可。

---

## FreeSmall 函数 (AllocatorHub)

回收一次 `AllocSmall` 的产物。

### 语法

```cpp
VOID FreeSmall(BufferInfo info);
```

### 参数

`[in] info`

类型: **BufferInfo**

待回收的分配信息, 通常为 `AllocSmall` 的返回值。`buffer` 为空时直接返回;
`buffer` 不属于小对象池时静默忽略。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

是否真正回收以归属校验(`MemPool::Contains`)为准: 回退到线性分配器的指针、字符串字面量、
外部指针都安全忽略。因此可对任意 `BufferInfo` 调用, 不要求先判断来源。

同一块**不得重复回收**——归属校验挡不住二次释放, 会把同一块挂进空闲链表两次。
`info.size` 不参与回收判定, 可传 `{ buffer, 0 }`。

---

## CopyString 函数 (AllocatorHub)

把字符串复制到线性分配器。

### 语法

```cpp
LPCSTR CopyString(LPCSTR text);
```

### 参数

`[in] text`

类型: **LPCSTR**

以 `\0` 结尾的源字符串。为 `NULL` 时返回 `NULL`。

### 返回值

类型: **LPCSTR**

指向线性分配器内副本的指针(含结尾 `\0`); `text` 为 `NULL` 或分配器未就绪 / 分配失败时返回 `NULL`。

### 注解

每次调用都新分配, 不做字符串驻留或去重; 副本随线性分配器整体失效, 不可单独释放。
常用于把短生命周期字符串(如启动期路径)固定到会话分配器里。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/base/mem/AllocatorHub.h`(`src\framework\base\mem\AllocatorHub.h`) |
| 实现 | `src\framework\base\mem\AllocatorHub.cpp` |
| 命名空间 | `AllocatorHub` |
| 依赖 | `MemoryManager` · `kernel32.dll` · `ucrt` |
| 平台 | Windows 全版本 |
| 线程安全 | 否; 须由调用方串行 |
