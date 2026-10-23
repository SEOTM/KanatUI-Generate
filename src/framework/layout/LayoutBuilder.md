# LayoutBuilder 模块 (src\framework\layout\LayoutBuilder.h)

## 概述

布局对象构建原语:把中间态的布局节点(`LayoutNode` + 元素链)展开成一枚 `Layout` 对象, 以及把
一枚既有 `Layout` 深拷贝为不共享可写存储的独立对象。

本模块属 **framework 层**, 只做"中间态 → 展开态"的组装与对象复制; 不做外层编排(哪些布局要展开、
展开成哪种储存模式、失败如何降级都归调用方), 不做几何解算(像素几何归 `GeometrySolver`, 本模块只搬
像素值与缓冲), 也不持有任何状态: 分配器由 `Layout` / `LayoutModel` / `AllocatorHub` 各自持有。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `BuildLayoutFromNode` | 依中间态布局节点构建一枚布局对象(控件缓冲 / 标签 / 事件处理器)。 |
| `CopyLayout` | 深拷贝一枚布局对象(储存模式随源, 可写存储不共享)。 |
| `BuildCtrlInfoFromSpec` | 依中间态元素规格装配一枚控件信息(控件字段 / 事件绑定 / 类型缓冲)。 |
| `FreeLayoutCtrlBuffers` | 回收一枚布局内全部控件的自身缓冲(布局对象析构之前调用)。 |
| `ResolveEntrustedEvents` | 解析"委托父容器"的事件绑定(沿父链上溯: 委托父继续上溯; 无事件且无处理函数的父即中断)。 |

本模块不定义公开类型与公开常量。

---

## BuildLayoutFromNode 函数 (LayoutBuilder)

依中间态布局节点构建一枚布局对象。

### 语法

```cpp
Layout* BuildLayoutFromNode(
    [in] CONST LayoutNode* pNode,
    [in] BOOL IsLink
);
```

### 参数

`[in] pNode`

类型: **CONST LayoutNode\***

中间态的布局节点(布局信息 + 元素链)。为 `NULL`、`pNode->info` 为 `NULL` 或
`pNode->info->dwElements` 为 `0` 时返回 `NULL`, 且不建立任何对象。

`[in] IsLink`

类型: **BOOL**

储存模式: `TRUE` = 链式(可按下标增删控件), `FALSE` = 线性(只读展开态)。取值随对象固化,
之后不可转换。

### 返回值

类型: **Layout\***

成功 = 新建的布局对象(控件数 = `pNode->info->dwElements`); 失败 = `NULL`, 此时不残留任何对象
与缓冲(已建的控件缓冲、已装配的控件、布局对象本身按序回收)。

### 注解

装配顺序固定: `Layout::CreateLayout(元素数, IsLink)` → 逐元素组装 `CtrlInfo` → 逐枚
`Layout::SetCtrlInfo(下标, 信息)` → 出口统一 `Layout::DeriveCtrlLayers()` → `Layout::DeriveCtrlPositions()`。

逐元素组装的取值口径: `id` 取 `ElementSpec::id`; `parentID` 取 `ElementSpec::parentID`(文档 `parent`
字段的直映, `-1` = 无父容器); `type` 取 `GetTypeEnum(type 串)`, 未命中类型表
⇒ `CTRL_TYPE_UNKNOWN`(渲染前按矩形占位); `label` 直接指向中间态块内的定长字段(中间态块常驻
⇒ 该指针可长期持有); 事件名非空且**不是**"委托父容器"特殊值(`CtrlEventIsEntrustParent`)时经
`UIEventHandler::BindEventHandler` 取处理函数写入 `pEventHandler`, 名字未登记 ⇒ 留 `NULL`(控件无响应);
仅当 `EventInfo::bufferSize` 大于 `0` 时才额外分配一块事件缓冲并清零(现绑定表该项恒为 `0`, 故实际不分配)。

事件声明为 `CTRL_EVENT_ENTRUST_PARENT` 的控件在本步**不查绑定表**(该名字不是绑定表里的业务事件名),
其处理器留待出口的 `ResolveEntrustedEvents` 按父容器解析。

随后 `Controls::PrepareCtrlBuffer(&info, text)` 建立类型缓冲(文本即写进缓冲): 返回 `FALSE`
(内存池分配失败)即整体失败, 按 控件缓冲 `FreeCtrlBuffer(&info)` → 已装配控件的
`FreeLayoutCtrlBuffers(layout)` → `Layout::DestroyLayout(layout)` 回收。

元素链上 `data` 为 `NULL` 的元素**跳过该枚**并继续后续元素, 不改变返回码; 此时对应槽位保持构造
初值(`CTRL_TYPE_INVALID` 之类), 控件总数仍等于 `dwElements`。

几何系数不驻控件: `CtrlInfo` 只接收像素几何, 四轴系数留在 `ElementSpec` 里由 `GeometrySolver`
解算, 本函数不写 `position` / `posDeclared` / `size`。

出口的层次整理对两种储存模式都必须做: 本路径逐枚 `SetCtrlInfo`(不经 `AddCtrlInfo`), 层次不会
自动推导; `DeriveCtrlLayers` 对同一状态幂等, 线性分支同样可写("只读"指不可增删控件)。层次口径
按 `parentID` 链逐级求值(父层 + 自身类型步进), 不再做几何包含推导。坐标整理(`DeriveCtrlPositions`)
与层次同一次出口调用: 它只读声明坐标, 而装配期尚未解算像素几何(声明坐标全 `0`) ⇒ 本次结果也是 `0`,
真正的累加由调用方在几何解算之后再调一次。

层次整理之后调用 `ResolveEntrustedEvents(layout, pNode)` 解析事件委托: 委托未解析(父容器缺失 /
父无事件且无处理函数 / 祖先未登记事件 / 成环)时该控件保持无响应, **不影响**本函数的成功判定。

文档原类型串(串形态)与无缓冲声明文本不入控件, 也不由本函数复制: 它们按"布局 + 索引"存在调用方
的旁挂表里。

成功返回的对象归调用方所有, 用毕须 `Layout::DestroyLayout`; 对象内存取自分配器, 单枚对象不能
单独归还。

---

## CopyLayout 函数 (LayoutBuilder)

深拷贝一枚布局对象。

### 语法

```cpp
Layout* CopyLayout(
    [in] Layout& src
);
```

### 参数

`[in] src`

类型: **Layout&**

源布局对象。源被判为无效(`IsInvalid`)或控件数为 `0` 时返回 `NULL`, 且不新建任何对象。

### 返回值

类型: **Layout\***

成功 = 独立的新布局对象; 失败 = `NULL`, 此时已分配的缓冲与已装配的控件按序回收, 源对象不受影响。

### 注解

储存模式随源: 目标按 `Layout::CreateLayout(src.GetTotalNum(), src.IsLinkMode())` 创建。模式必须
随源, 否则写回会落到容量不符的线性数组上而越界。

逐控件复制的内容与语义:

- 类型缓冲: 按源容量重新分配并逐字节复制(`AllocatorHub::AllocSmall` + `memcpy`) ⇒ 文本、行数据
  等内容保留; 源 `buffer` 为空或容量为 `0` 时目标缓冲置空。
- 标签: `AllocatorHub::CopyString` 复制为池内新串, 保持 `NULL` 语义 ⇒ 不与源共享。
- 几何: `position` / `posDeclared` / `size` 按值复制(像素值), 尺寸系数不随副本建立 ⇒ 副本保持当前像素, 之后源侧
  参考尺寸变化不影响副本; 副本的父链(按 `parentID`)与源相同 ⇒ 两侧的坐标累加结果一致。
- 事件处理器: 函数指针按值复制, 副本与源指向同一处理函数。
- 父容器 ID: 按值复制(`parentID`), 副本与源指向同一父。

副本不共享任何可写存储, 但**不带**旁挂表(文档原类型串、无缓冲声明文本按布局指针反查归属, 不随
本函数复制)。

失败回收顺序: 缓冲分配失败 ⇒ `FreeLayoutCtrlBuffers(dst)` → `DestroyLayout(dst)`; 某枚
`SetCtrlInfo` 失败 ⇒ 该枚未入目标布局的副本缓冲 `FreeCtrlBuffer(&d)` → `FreeLayoutCtrlBuffers(dst)`
→ `DestroyLayout(dst)`。内存池本身不回收。

成功返回的对象归调用方所有, 用毕须 `Layout::DestroyLayout`。


---

## ResolveEntrustedEvents 函数 (LayoutBuilder)

解析"委托父容器"的事件绑定: 沿父容器 ID 上溯, 委托父继续上溯, 无事件且无处理函数的父即中断。

### 语法

```cpp
BOOL ResolveEntrustedEvents(
    [in, out] Layout& layout,
    [in]      CONST LayoutNode* pNode
);
```

### 参数

`[in, out] layout`

类型: **Layout&**

已装配完的布局对象(控件数与元素链一一对应)。命中委托的控件会被写入
`pEventHandler`(解析不成功则写 `NULL`)。

`[in] pNode`

类型: **CONST LayoutNode\***

与 `layout` 同源的中间态布局节点(提供 `event` 与 `parentID` 的声明值)。为 `NULL`、
`pNode->info` 为 `NULL`、`dwElements` 为 `0` 或 `layout` 的控件数少于元素数时返回 `FALSE`,
且不改动任何控件。

### 返回值

类型: **BOOL**

`TRUE` = 布局内没有委托者, 或全部委托者都已解析到处理器;`FALSE` = 至少一枚委托未解析
(父容器缺失 / 父无事件且无处理函数 / 祖先未登记事件 / 委托成环), 或入参不合法。**装配不因本返回值失败**。

### 注解

**判据**: 中间态元素规格的 `event` 经 `CtrlEventIsEntrustParent` 判定为 `CTRL_EVENT_ENTRUST_PARENT`
即"委托者"; 其余控件保持装配期已有的绑定, 不被本函数改动。

**上溯口径**(逐级两步判定): 从委托者的 `parentID` 开始找同布局内 `id` 命中的元素;

1. 该元素的事件声明为 `CTRL_EVENT_ENTRUST_PARENT` ⇒ **向上委托**, 改取它自己的 `parentID` 继续上溯
   (最多走一遍控件总数, 成环时自然止步);
2. 否则该元素是普通控件: 若它**既无独立处理函数**(`pEventHandler` 为 `NULL`)**又是无事件控件**
   (事件字段为空) ⇒ **委托中断**, 委托者保持无响应, 不再上溯;
3. 其余情形 ⇒ 把该元素的 `pEventHandler` 写给委托者(它有自己的事件名但绑定表未登记 ⇒ 继承到
   `NULL`, 同样保持无响应且不再上溯)。

父容器 `id` 不存在 ⇒ 直接中断(静默降级)。

**只继承处理函数**: 委托者的 `bufferInfo` 不变(其类型缓冲在装配时已由 `CtrlBuffer::PrepareCtrlBuffer`
建立), 也不复制父容器的缓冲容量。

**链序约束**: 父容器须与委托者同在本布局内(按 `id` 查找, 与链序无关); `parentID` 指向不存在的 `id`
即静默降级。本函数不改变层次、几何与控件顺序, 可重复调用(幂等)。

**调用时机**: 装配期由 `BuildLayoutFromNode` 在出口自动调用一次; 经 `Layout::AddCtrlInfo` 链式新增
控件(含委托者)时**不会**自动重解析 —— 需要委托生效的调用方须在增删之后自行再调本函数, 否则新增的
委托控件保持无响应。


---

## BuildCtrlInfoFromSpec 函数 (LayoutBuilder)

依中间态元素规格装配一枚控件信息。

### 语法

```cpp
BOOL BuildCtrlInfoFromSpec(CtrlInfo& info, CONST ElementSpec* es, BOOL bBindEvent);
```

### 参数

`[in, out] info`

类型: **CtrlInfo&**

待装配的控件信息(id / 类型 / 声明坐标与尺寸已由调用方填好), 本函数补齐标签、事件处理器与类型缓冲。

`[in] es`

类型: **CONST ElementSpec\***

中间态元素规格(尺寸与坐标系数 / 标签 / 事件名 / 父容器 ID)。传 `NULL` 时返回 `FALSE`。

`[in] bBindEvent`

类型: **BOOL**

`TRUE` = 按 `es->event` 名在绑定表内解析处理函数; `FALSE` = 不解析(该控件无响应)。委托串不当作事件名查表。

### 返回值

类型: **BOOL**

`TRUE` = 装配成功; `FALSE` = 规格为空或控件缓冲装配失败(此时不保证控件字段完整)。

### 注解

控件缓冲的分配与初值由 `framework/base/CtrlBuffer.h` 的 `PrepareCtrlBuffer` 完成(本函数只转调, 不含缓冲构造知识)。
`parentID` 的缺省口径为 `-1`(无父), 该字段同时是层序推导与坐标累加的父链。返回 `FALSE` 时不回滚已写入的字段。

---

## FreeLayoutCtrlBuffers 函数 (LayoutBuilder)

回收一枚布局内全部控件的自身缓冲。

### 语法

```cpp
VOID FreeLayoutCtrlBuffers(Layout& layout);
```

### 参数

`[in, out] layout`

类型: **Layout&**

目标布局对象。本函数只遍历其控件表并逐枚回收缓冲, 不释放布局对象本身。

### 返回值

此函数不返回值。

### 注解

须在布局对象析构之前调用; 逐控件转调 `CtrlBuffer::FreeCtrlBuffer`。之所以归布局层而非 base: 整枚布局的遍历需要
`Layout` 完整类型, 而 `framework/base/CtrlBuffer.h` 只依赖 base(含 mem;以单枚控件为操作对象)。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/layout/LayoutBuilder.h`(`src\framework\layout\LayoutBuilder.h`) |
| 实现 | `src\framework\layout\LayoutBuilder.cpp` |
| 命名空间 | `LayoutBuilder` |
| 依赖 | `framework/layout/Layout.h` · `framework/layout/LayoutModel.h` · `framework/base/CtrlBuffer.h` · `framework/base/BasicDesign.h` · `framework/dispatch/UIEventHandler.h` · `framework/base/mem/AllocatorHub.h` · `windows.h` · `ucrt` |
| 平台 | Windows 全版本 |
| 线程安全 | 否(经 `Layout` 的静态分配器分配); 须由调用方串行 |
