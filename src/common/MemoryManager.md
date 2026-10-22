# MemoryManager 模块 (src\common\MemoryManager.h)

## 概述

`MemoryManager` 是工程的内存分配底座: 三个按用途分工的分配器, 加一套统一的分配结果表达。本模块只负责从系统整块取内存、按各自的模型切分与回收, 不感知任何上层对象的语义, 也不承担通用堆的职责。三者的释放模型互不相同: `SecureBuffer` 整体析构并安全清零, `Arena` 经 `Reset` 或 `Mark` / `Rewind` 整体回收, `MemPool` 支持任意顺序单独 `Free`。

共同约定有三条。其一, 分配结果统一为 `BufferInfo`, 其中 `size` 是请求大小而不是对齐后的实际占用, 因此判空一律看 `buffer`。其二, 分配失败统一返回 `{ nullptr, 0 }`, 涵盖“请求为 `0`”“超出容量或尺寸类上限”“底层 `VirtualAlloc` 失败”三种情形, 且失败不抛异常。其三, 三个分配器都不含任何隐式线程安全机制, 且禁止拷贝构造与赋值: 内部无锁, 跨线程使用同一实例必须由调用方同步。

底层一律 `VirtualAlloc` / `VirtualFree`, 模块内部不使用 `new` / `delete`, `MemPool` 的尺寸类数组是内嵌静态数组。依赖方向单向: 本模块不引用工程内任何其它模块, 上层反过来把分配器注入自己的对象里。

本模块包含以下编程接口:

### 类

| 类 | 说明 |
| --- | --- |
| `SecureBuffer` | 敏感数据专用缓冲区: 页级保护与安全清零, 内存整体析构释放。 |
| `Arena` | 线性分配器: 分配即前移游标, 经 `Reset` 或 `Mark` / `Rewind` 整体回收。 |
| `MemPool` | 分级固定块池: 按尺寸类切块, 支持任意顺序单独 `Free`。 |

### 类型

| 类型 | 说明 |
| --- | --- |
| `BufferInfo` | 分配结果: 内存首址加请求大小, 三个分配器统一返回。 |

### 常量

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `MAX_SMALL_SIZE` | `4096` | `MemPool` 可服务的单次请求上限, 上层据此决定进池还是走线性分配。 |

### 函数

| 函数 | 说明 |
| --- | --- |
| `AlignUp` | 把大小向上对齐到指定对齐值。 |

---

## SecureBuffer 类 (MemoryManager)

敏感数据专用缓冲区: 一整块按系统页对齐的连续内存, 分配是线性的, 取用期间可切换页级保护, 释放前强制安全清零。

### 语法

```cpp
class SecureBuffer
{
public:
    SecureBuffer(size_t requestedSize);
    ~SecureBuffer();

    BufferInfo Allocate(size_t size);
    BOOL       Contains(LPCVOID p) const;
    size_t     Capacity() const;
    size_t     Used() const;

    VOID SecureZero(size_t offset, size_t size);
    VOID SecureZeroAll();

    BOOL Protect(DWORD protectFlag);
    BOOL SetReadOnly();
    BOOL SetReadWrite();
    BOOL SetNoAccess();
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `SecureBuffer(size_t)` | 构造: 容量按系统页向上对齐后整块提交, 提交失败时容量归零。 |
| `~SecureBuffer()` | 析构: 先 `SecureZeroAll` 安全清零, 再整块归还系统。 |
| `Allocate(size_t)` | 线性分配一块, 按 `sizeof(void*)` 对齐, 返回请求大小。 |
| `Contains(LPCVOID)` | 地址归属查询: 只比地址区间, 不解引用。 |
| `Capacity()` | 取页对齐后的总容量。 |
| `Used()` | 取已用字节数, 含每块的对齐空隙。 |
| `SecureZero(size_t, size_t)` | 对指定偏移区间安全清零。 |
| `SecureZeroAll()` | 对整块容量安全清零。 |
| `Protect(DWORD)` | 切换整块的页级保护标志。 |
| `SetReadOnly()` | 等价于 `Protect(PAGE_READONLY)`。 |
| `SetReadWrite()` | 等价于 `Protect(PAGE_READWRITE)`。 |
| `SetNoAccess()` | 等价于 `Protect(PAGE_NOACCESS)`。 |

### 注解

构造不报告失败: 底层 `VirtualAlloc` 失败时容量置 `0`, 此后 `Allocate` 一律返回 `{ nullptr, 0 }`、`Contains` 一律返回 `FALSE`、`Protect` 一律返回 `FALSE`, 调用方只能经 `Capacity() == 0` 观察到构造失败。

`Allocate(size_t)`: 请求为 `0` 或缓冲区未就绪时失败；对齐粒度为 `sizeof(void*)`(x86 为 4 字节、x64 为 8 字节), 不保证 16 字节对齐。越界判据是 `used + alignedSize > capacity`(`alignedSize` 为请求按对齐粒度上取整后的大小), 越界即整块拒绝, 不切分、不部分满足。记账按对齐后的占用累加(`used += AlignUp(size, alignBytes)`), `Used()` 因此含每块尾部空隙、不小于请求大小之和；返回的 `BufferInfo::size` 仍是请求大小而非对齐后大小。

`SecureZero` / `SecureZeroAll`: 内部走 `RtlSecureZeroMemory`, 编译器不会把清零优化掉。越界判据为 `offset + size > capacity`, 越界调用静默返回(不清零、不报告、不改任何状态)；判据用的是容量而不是已用量, 因此可以清零尚未分配的尾部区域。`SecureZeroAll()` 覆盖整块容量(含未用尾部), 用于释放前擦除, 也可在生存期内主动擦除。

`Protect(DWORD)` 是整块页级操作: 一次系统调用覆盖全部容量, 开销明显高于普通内存访问, 不要高频切换；成功后才记录当前标志, 失败(未就绪或系统调用失败)返回 `FALSE` 且不改记录。三个便利方法只是固定标志的转发。切到只读或不可访问期间, 此前 `Allocate` 返回的指针均不可写, 需要写时先切回读写；`Protect` 不改变 `used`, 也不影响已分配区间。

`Contains(LPCVOID)` 的判据是 `buffer <= p < buffer + capacity`: 指向未分配尾部区域或块间空隙的指针同样算“属于本缓冲区”, 归属查询不等于“该指针是一次有效分配”。空指针恒为 `FALSE`。

碎片形态: 全部内存取自一次整块提交, 因此没有外部碎片；浪费只体现为内部空隙(页对齐余量加每块的指针对齐余量), 最小浪费量级是一整页。

生命周期契约: 数据生存期不得超过缓冲区自身；析构即清零并整块归还系统, 此后该缓冲区发出的任何 `BufferInfo` 全部失效。缓冲区禁止拷贝与赋值, 按引用或指针传递。

适用面: 密钥、口令、令牌、会话标识等需要“使用期可读、闲置期只读或不可访问、释放前必须擦除”的数据；以及生命周期明确、一次分配一批、最后整体释放的敏感负载。不适用面: 普通业务数据、日志、临时字符串(页对齐加析构清零的代价不值得), 需要单独释放某个对象的负载, 需要 16 字节以上对齐的负载, 以及需要高频小块分配或高频切换保护的负载。

---

## Arena 类 (MemoryManager)

线性分配器: 一整块按系统页对齐的连续内存, 分配即前移游标, 回收靠整体重置或按标记回退。

### 语法

```cpp
class Arena
{
public:
    Arena(size_t requestedSize);
    ~Arena();

    BufferInfo Allocate(size_t size);
    BOOL       Contains(LPCVOID p) const;
    size_t     Capacity() const;
    size_t     Used() const;

    VOID   Reset();
    size_t Mark() const;
    VOID   Rewind(size_t mark);
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `Arena(size_t)` | 构造: 容量按系统页向上对齐后整块提交, 提交失败时容量归零。 |
| `~Arena()` | 析构: 整块归还系统, 不清零。 |
| `Allocate(size_t)` | 线性分配一块, 按 16 字节对齐, 返回请求大小。 |
| `Contains(LPCVOID)` | 地址归属查询: 只比地址区间, 不解引用。 |
| `Capacity()` | 取页对齐后的总容量。 |
| `Used()` | 取已用字节数, 含每块的对齐空隙。 |
| `Reset()` | 整体归零: 全部已分配内存失效。 |
| `Mark()` | 取当前分配位置, 作为后续 `Rewind` 的标记。 |
| `Rewind(size_t)` | 回退到标记位置: 标记之后分配的内存失效。 |

### 注解

构造不报告失败: 底层 `VirtualAlloc` 失败时容量置 `0`, 此后 `Allocate` 一律返回 `{ nullptr, 0 }`, 调用方只能经 `Capacity() == 0` 观察到构造失败。

`Allocate(size_t)`: 对齐粒度固定为 16 字节, 与平台指针大小无关, 因此适合放需要 16 字节对齐的数据。请求为 `0` 或未就绪时失败；越界判据是 `used + alignedSize > capacity`(`alignedSize` 为请求按 16 字节上取整后的大小), 越界即整块拒绝, 不切分、不部分满足。返回的 `BufferInfo::size` 是请求大小。

记账必须按对齐后的占用: `used += AlignUp(size, 16)`。若只累加请求大小, 请求不是 16 的整数倍时, 下一块的起点会落进上一块尾部, 相邻分配发生重叠；`Used()` 与越界判据都必须用对齐后大小, 否则 `Capacity()` 会被提前或滞后用尽。

回收有两条互不等价的路径。`Reset()` 把 `used` 归零, 此前分配的全部内存一次性失效, 之前取到的所有标记同时作废。`Mark()` 返回当前 `used` 作为标记, `Rewind(mark)` 把 `used` 退回该标记, 标记之后分配的内存失效。`Rewind` 只接受不大于当前用量的标记: 传入已过期(大于当前用量)的标记时静默忽略, 既不报错也不前进。

标记必须按后进先出顺序使用: 回退到较早标记后, 之前取到的较晚标记即失效, 不能再用来回退；也不应在回退后继续推进、再回退到更早的位置。回退后再分配会立即复用刚释放的区间, 旧内容不做清理, 调用方不得依赖“回退后内容归零”。

本类不提供内存保护, 且任何路径都不清零(`Reset` 与析构均不清零), 敏感数据不得放这里, 应改用 `SecureBuffer`。

`Contains(LPCVOID)` 的判据是 `buffer <= p < buffer + capacity`: `Reset` 之后旧指针仍会被判为“属于本 Arena”, 因此归属查询只说明地址落在本块内存内, 不说明该次分配仍然有效。空指针恒为 `FALSE`。

容量按系统页向上对齐, 因此实际可用容量略大于构造时请求的字节数；析构整块归还系统。禁止拷贝与赋值。

碎片形态: 内存始终是一整块连续区间, 因此没有外部碎片；代价是内部碎片 —— 每块尾部的对齐空隙, 加上 `Reset` 或 `Rewind` 之后仍未被重新分配出去的尾部容量。

适用面: 同生共死的一批对象(解析中间结果、帧内临时缓冲、一次性构建的树或图)、请求级内存(一个请求内分配、请求结束整体 `Reset`)、需要反复重建整代的负载(配 `Mark` / `Rewind`), 以及生命周期很长但可整体释放的内存。不适用面: 生命周期互相独立、需要单独释放的负载；需要频繁申请与释放交替的负载(会持续推进 `used` 直到耗尽, 中间空洞无法回收)；以及需要保护或清零的负载。

---

## MemPool 类 (MemoryManager)

分级固定块内存池: 按尺寸类把整块 slab 切成固定大小的块, 块之间完全等价, 支持任意顺序单独 `Free`, 块回收到所属尺寸类的空闲链表复用。

### 语法

```cpp
class MemPool
{
public:
    MemPool();
    ~MemPool();

    BufferInfo Allocate(size_t size);
    VOID       Free(BufferInfo info);
    VOID       Free(VOID* ptr);
    BOOL       Contains(LPCVOID p) const;
    size_t     SlabCount() const;

    static constexpr size_t MAX_SMALL_SIZE = 4096;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `MemPool()` | 构造: 初始化 28 个尺寸类的块大小与空闲链表, 不申请 slab。 |
| `~MemPool()` | 析构: 释放全部 slab, 不逐块清零。 |
| `Allocate(size_t)` | 从尺寸类取一块, 返回请求大小；`0`、超限、补 slab 失败均为失败。 |
| `Free(BufferInfo)` | 释放一块(`BufferInfo` 版本, 只读 `buffer` 字段)。 |
| `Free(VOID*)` | 释放一块(裸指针版本, 含归属校验与静默忽略)。 |
| `Contains(LPCVOID)` | 本池归属查询: 指针是否落在某个 slab 的数据区范围内。 |
| `SlabCount()` | 取已分配的 slab 数(遍历链表, `O(n)`)。 |
| `MAX_SMALL_SIZE` | 公开常量: 可服务的单次请求上限(4096 字节)。 |

### 注解

分档: `Allocate` 先判 `size == 0` 与 `size > MAX_SMALL_SIZE`(两者都直接失败), 再由 `FindSizeClass` 取能容纳“块头 + 请求”的最小尺寸类, 判据是 `blockSize >= sizeof(BlockHeader) + size`；块头固定 16 字节, 因此每一档服务的请求上限是块总大小减 16, 找不到可用档同样返回 `{ nullptr, 0 }`。

尺寸类表共 28 档, 元素是块总大小(含 16 字节块头), 均为 16 的倍数、按升序排列；第三列由 `sizeof(BlockHeader) + size` 的判据派生(块总大小减 16):

| 档序 | 块总大小(字节) | 可服务请求上限(字节) |
| --- | --- | --- |
| 1 | 32 | 16 |
| 2 | 48 | 32 |
| 3 | 64 | 48 |
| 4 | 80 | 64 |
| 5 | 96 | 80 |
| 6 | 112 | 96 |
| 7 | 128 | 112 |
| 8 | 160 | 144 |
| 9 | 192 | 176 |
| 10 | 224 | 208 |
| 11 | 256 | 240 |
| 12 | 320 | 304 |
| 13 | 384 | 368 |
| 14 | 448 | 432 |
| 15 | 512 | 496 |
| 16 | 640 | 624 |
| 17 | 768 | 752 |
| 18 | 896 | 880 |
| 19 | 1024 | 1008 |
| 20 | 1280 | 1264 |
| 21 | 1536 | 1520 |
| 22 | 1792 | 1776 |
| 23 | 2048 | 2032 |
| 24 | 2560 | 2544 |
| 25 | 3072 | 3056 |
| 26 | 3584 | 3568 |
| 27 | 4096 | 4080 |
| 28 | 4112 | 4096 |

末档 `4112 = sizeof(BlockHeader) + MAX_SMALL_SIZE`: 若末档仍是 `4096`, 请求 `4081`..`4096` 将无档可服务而被判为分配失败。改动 `MAX_SMALL_SIZE` 必须同时改末档, 两者成对。

slab 与切分: 每个尺寸类各有一条空闲链表, 链表为空时补一块 64KB 的 slab(`VirtualAlloc` 整块提交)。slab 头内嵌在 slab 内存开头、不额外分配, 占 16 字节；数据区从偏移 `16` 起, 可用 65520 字节, 块数 = `65520 / blockSize` 向下取整(余数丢弃)。每块开头是 16 字节块头(记录所属尺寸类的指针加填充), 用户指针 = 块首 + 16。因为数据区起点与块大小都是 16 的倍数, 用户指针始终保持 16 字节对齐；单块的固定开销是 16 字节, 请求 `1` 字节的块实际占 32 字节。

分配与释放都是 `O(1)` 链表操作(取头 / 挂头)。`Free(BufferInfo)` 等价于 `Free(info.buffer)`, 只用 `buffer` 字段、忽略 `size`, 两个重载语义完全相同。

归属校验与静默忽略: `Free(VOID*)` 先忽略空指针, 再经 `IsOwnedPointer` 判指针是否落在本池某个 slab 的数据区 `[slab + 16, slab + 65536)` 内, 不在范围内则静默忽略；命中后读用户指针前 16 字节的块头, 若其尺寸类指针不在本池的 28 档数组内(数据已被踩坏)亦静默忽略、拒绝挂链。两道校验都不报告、不抛异常、不改动任何状态。`Contains` 只做第一道判据(区间归属), 比 `Free` 的完整校验粗: “回收前先 `Contains`”是安全但非必需的额外守卫；它与 `SecureBuffer` / `Arena` 的归属查询一样只比地址区间、不解引用, 且空指针恒为 `FALSE`。

重复释放: 归属校验挡不住二次释放 —— 同一块被 `Free` 两次会以同一地址两次挂进空闲链表, 形成自环, 之后的分配可能把同一块发给两个调用方。调用方必须保证每块只释放一次。跨分配器释放(把 `Arena` 或 `SecureBuffer` 的指针交给本池 `Free`)一律被静默忽略, 但语义上即为错误, 不要依赖忽略来兜错。

池优先回退: 由于非本池指针一律被静默忽略, “请求不超过 `MAX_SMALL_SIZE` 先进池、超限或补 slab 失败时回退线性分配器、回收时统一交给池”的用法在回收端是安全的: 回退出去的那部分指针会被池忽略, 不会误挂链。

用量口径: 本池没有 `Capacity` / `Used`, 总量随 slab 追加而增长；`SlabCount()` 遍历 slab 链表计数, `O(n)`, 只反映已提交的 slab 数、不反映池内空闲量, 仅用于诊断。单 slab 承载的块数 = `(65536 - 16) / blockSize` 向下取整, 块越大则单 slab 块数越少。

碎片形态: 同一尺寸类内的块大小完全一致, 因此类内没有外部碎片；不同尺寸类各占自己的 slab、互不共用, 因此类间由 slab 隔离；代价是档位向上取整带来的内部浪费(每块固定多出 16 字节块头, 且请求量级落在两档之间时按较大档占用)。

本类不提供内存保护、任何路径都不清零(析构不逐块清零), 敏感数据应改用 `SecureBuffer`；内部无锁, 跨线程申请释放同一池需外部同步。析构逐个 `VirtualFree` 归还全部 slab(slab 头内嵌, 无需 `delete`), 此后该池发出的任何 `BufferInfo` 失效。禁止拷贝与赋值。

适用面: 生命周期互相独立、需要单独释放、单对象不超过 `MAX_SMALL_SIZE` 的小对象(节点、连接、事件、会话上下文一类)。不适用面: 单对象超过 `MAX_SMALL_SIZE`(直接返回失败)；变长缓冲区与超大对象；一次性只分配不回收的批量负载(池要按 slab 补充, 且每块多 16 字节块头, 摊销成本高于线性分配器)；需要保护、清零或移动已分配块的负载。

---

## BufferInfo 结构 (MemoryManager)

分配结果: 一次分配的内存首址与请求大小, 三个分配器的分配接口统一用它表达, `MemPool::Free` 也以它作为入参形态。

### 语法

```cpp
struct BufferInfo
{
    VOID*  buffer;
    size_t size;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `buffer` | 内存首址；分配失败为 `nullptr`。 |
| `size` | 请求大小；分配失败为 `0`。 |

### 注解

`size` 是请求大小, 不是对齐后的实际占用: `SecureBuffer` 按 `sizeof(void*)`、`Arena` 按 16 字节、`MemPool` 按尺寸类向上取整, 实际占用的容量可能大于 `size`, 因此不要用 `size` 反推可写边界或归还量。判空一律看 `buffer`, 不要用 `size == 0` 判失败。

失败口径统一为 `{ nullptr, 0 }`, 三个分配器一致；请求为 `0` 一律失败, 因此成功分配的 `size` 恒大于 `0`。

结构是纯值类型, 自身不持有资源: 复制它不转移所有权, 也不导致重复释放；释放只作用于 `buffer` 指向的那一块, 且必须交给发放它的那个分配器(`MemPool` 的块交给 `MemPool::Free`, `Arena` 与 `SecureBuffer` 的内存只能随容器整体回收)。`MemPool::Free(BufferInfo)` 只读取 `buffer`, `size` 不参与释放。

---

## MAX_SMALL_SIZE 常量 (MemPool)

`MemPool` 能服务的单次请求上限。

### 语法

```cpp
static constexpr size_t MAX_SMALL_SIZE = 4096;
```

### 值

`4096`(字节)。编译期常量, 是类的静态成员而不是实例成员, 因此可以不经对象直接读到(`MemPool::MAX_SMALL_SIZE`)。

### 注解

`MemPool::Allocate` 的上限判据: 请求大于本值即返回 `{ nullptr, 0 }`, 不进入分档。上层据此决定“进池还是走线性分配”, 回退策略的阈值就是它。本值与本类尺寸类表的末档成对: 末档块总大小 `4112 = sizeof(BlockHeader) + MAX_SMALL_SIZE`, 保证请求 `4081`..`4096` 仍有档可服务；只改本值而不改末档, 会让上限附近的请求无档可服务。上限之外的负载不属于本池的职责, 应交给 `Arena` 或系统分配。

---

## AlignUp 函数 (MemoryManager)

把大小向上对齐到指定对齐值。

### 语法

```cpp
inline size_t AlignUp(
  [in] size_t size,
  [in] size_t align
);
```

### 参数

`[in] size`

类型: **size_t**

原始大小(字节), 合法域为任意非负值；本函数不对它做溢出检查。

`[in] align`

类型: **size_t**

对齐值(字节), 必须是 2 的幂且不为 `0`。传入非 2 的幂时位掩码退化、传入 `0` 时 `align - 1` 下溢, 两种情况下结果都不可用；本函数不校验入参。

### 返回值

类型: **size_t**

不小于 `size` 的最小 `align` 整数倍；`size` 已是 `align` 整数倍时原值返回。

### 注解

模块内在三类位置使用本函数: `SecureBuffer` 与 `Arena` 构造时按系统页对齐容量, 两类的 `Allocate` 按各自对齐粒度(`sizeof(void*)` / 16 字节)对齐请求大小, `MemPool` 的数据区起点与块起点按 16 字节对齐。

算式是 `(size + align - 1) & ~(align - 1)`: 不做溢出检查, `size` 接近 `SIZE_MAX` 时相加会回绕并得到偏小的结果, 调用方须保证入参在合法域内。本函数没有任何状态, 可安全并发调用。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `common/MemoryManager.h`(`src\common\MemoryManager.h`) |
| 实现 | `src\common\MemoryManager.cpp` |
| 命名空间 | 全局(函数 `AlignUp`、结构 `BufferInfo`、类 `SecureBuffer` / `Arena` / `MemPool`) |
| 依赖 | 工程内: 无(本模块不引用任何工程内模块)；系统: `kernel32`(`VirtualAlloc` / `VirtualFree` / `VirtualProtect` / `GetSystemInfo` / `RtlSecureZeroMemory`) |
| 平台 | Windows；页大小与分配粒度运行期取自 `GetSystemInfo` 的 `dwPageSize`；对齐粒度 `SecureBuffer` 为指针大小、`Arena` 与 `MemPool` 为 16 字节 |
| 线程安全 | 否(三个分配器均无内部同步；同一实例的并发 `Allocate` / `Free` 需外部加锁, `AlignUp` 等纯函数除外) |
