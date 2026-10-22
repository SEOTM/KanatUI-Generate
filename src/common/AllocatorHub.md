# AllocatorHub 模块 (src\common\AllocatorHub.h)

## 概述

`AllocatorHub` 是会话分配器的持有处: 统一建立一个线性分配器(`Arena`)与一个小对象池
(`MemPool`), 并向调用方提供分配、回收与字符串复制的原语。命名空间内全部是自由函数, 状态是文件内
静态变量, 语义上是进程级单例 —— 同一进程只有一对分配器。

层归属是公共基础设施层, 不依赖框架层; 依赖方向为单向: 本模块依赖 `MemoryManager` 的两个分配器
类型, 反向无依赖。分配器本身的页保护、对齐、分档等行为不属本模块, 由 `MemoryManager` 承担;
本模块只负责"建立、持有、把请求路由到正确的分配器"。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `Init` | 建立会话分配器(启动期调用一次, 已建立则先释放再重建)。 |
| `Release` | 释放会话分配器(退出期调用一次, 幂等)。 |
| `GetArena` | 取应用统一线性分配器, 未建立时为空。 |
| `GetSmallPool` | 取应用小对象池(恒非空)。 |
| `AllocBuffer` | 从线性分配器分配一段内存。 |
| `AllocSmall` | 小对象分配: 池优先, 超限或失败回落线性分配器。 |
| `FreeSmall` | 回收一次 `AllocSmall` 的产物(仅当指针确属小对象池)。 |
| `CopyString` | 把字符串复制到线性分配器并返回其指针。 |

---

## Init 函数 (AllocatorHub)

建立会话分配器。

### 语法

```cpp
BOOL Init(
  [in] size_t cbArena
);
```

### 参数

`[in] cbArena`

类型: **size_t**

线性分配器容量(字节)。传 `0` 表示使用实现内的默认容量。

### 返回值

类型: **BOOL**

`TRUE` = 分配器已就绪; `FALSE` = 线性分配器分配失败, 此后各分配原语一律返回空。

### 注解

应用启动期调用一次。若此前已经建立过, 函数先执行 `Release` 再重建 —— 重建会丢弃上一轮的全部线性
分配结果, 因此不要在会话中途调用。小对象池随本模块静态构造时就绪, 不参与本次建立。

---

## Release 函数 (AllocatorHub)

释放会话分配器。

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

应用退出期调用一次, 幂等(未建立时为空操作)。线性分配器被整体释放, 会话内所有经它分配的对象随之
失效; 小对象池复位为初值, 但已向系统申请的 slab 不归还。`Release` 之后 `GetArena` 返回空,
`AllocSmall` 只能落到池路径。

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

线性分配器指针; 未建立(或已释放)时返回 `NULL`。

### 注解

返回的指针由本模块持有, 调用方不得释放。线性分配器承载文档与布局等会话级对象, 其整体释放时机由
`Release` 决定。

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

小对象池指针, 恒非空(池对象是模块内的静态值对象)。

### 注解

小对象池承载不定生命周期的小对象: 中间态块、链节点、旁挂表、控件缓冲。这些对象可被单独回收并复用,
因此池指针在 `Release` 前后都可用。

---

## AllocBuffer 函数 (AllocatorHub)

从线性分配器分配一段内存。

### 语法

```cpp
BufferInfo AllocBuffer(
  [in] size_t size
);
```

### 参数

`[in] size`

类型: **size_t**

请求字节数。

### 返回值

类型: **BufferInfo**

成功时返回缓冲首址与请求字节数; 分配器未就绪或分配失败时返回 `{nullptr, 0}`。

### 注解

线性分配器的内存只随 `Release` 整体归还, 无法单独释放 —— 需要单独回收的小对象应改用 `AllocSmall`。

---

## AllocSmall 函数 (AllocatorHub)

小对象分配: 池优先, 超限或失败回落线性分配器。

### 语法

```cpp
BufferInfo AllocSmall(
  [in] size_t size
);
```

### 参数

`[in] size`

类型: **size_t**

请求字节数。处于 `1`..`MemPool::MAX_SMALL_SIZE` 时走小对象池。

### 返回值

类型: **BufferInfo**

成功时返回缓冲首址与请求字节数; 池与线性分配器都失败时返回 `{nullptr, 0}`。

### 注解

请求落在池上限内时先从池分配(该内存可被 `FreeSmall` 单独回收); 超限或池分配失败时回落到线性
分配器(该内存随整体释放)。返回值不标明来源 —— 回收一律交给 `FreeSmall`, 由它按归属校验决定是否
真正回收。

---

## FreeSmall 函数 (AllocatorHub)

回收一次 `AllocSmall` 的产物。

### 语法

```cpp
VOID FreeSmall(
  [in] BufferInfo info
);
```

### 参数

`[in] info`

类型: **BufferInfo**

待回收的缓冲, 通常来自 `AllocSmall`。允许为空(`buffer` 为空时为空操作)。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

仅当指针确属小对象池时才真正回收(经池的归属校验); 回落到线性分配器的指针、字符串字面量以及任何
外部指针都被安全忽略。因此对同一缓冲重复调用不会破坏分配器状态, 但重复回收后该块可能被再次分配
出去, 调用方不应保留悬垂视图。

---

## CopyString 函数 (AllocatorHub)

把字符串复制到线性分配器并返回其指针。

### 语法

```cpp
LPCSTR CopyString(
  [in] LPCSTR text
);
```

### 参数

`[in] text`

类型: **LPCSTR**

待复制的以零结尾的字符串。允许为 `NULL`。

### 返回值

类型: **LPCSTR**

复制件指针(内存取自线性分配器); `text` 为 `NULL` 或分配器未就绪时返回 `NULL`, 分配失败亦返回 `NULL`。

### 注解

每次调用都新分配一份, 不做字符串去重。复制件只随 `Release` 整体失效, 调用方可长期持有; 返回的串
不得被调用方释放。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `common/AllocatorHub.h`(`src\common\AllocatorHub.h`) |
| 实现 | `src\common\AllocatorHub.cpp` |
| 命名空间 | `AllocatorHub` |
| 依赖 | 工程内: `MemoryManager`(`Arena` / `MemPool` / `BufferInfo`); 系统: 无额外链接库 |
| 平台 | Windows; 进程级单例语义, 同一进程只有一对会话分配器 |
| 线程安全 | 否(状态是文件内静态变量, 无同步; 多线程访问需外部串行化) |
