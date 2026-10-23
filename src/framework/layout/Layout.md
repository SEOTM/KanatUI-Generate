# Layout 模块 (src\framework\layout\Layout.h)

## 概述

一个界面的控件容器:按索引持有值语义的控件信息(`CtrlInfo`), 并给结构件(`WND` / `TITLE` / `CNTR`)
提供定位入口。布局对象**不保证跨帧可用性** —— 消费者一律按索引访问控件, 不持久化持有控件指针。

控件储存分**线性数组**与**链式节点**两种互斥模式, 由 `CreateLayout` 在创建时确定, 运行期不转换;
线性数组只读(不可增删), 链式链支持按下标插入与删除。内存一律经 `MemoryManager` 的两个分配器
(静态注入): 布局对象自身 / 控件数组 / 链视图取自 `Arena`, 链节点取自 `MemPool`; 本模块自身不做
堆分配, 控件信息用定位 new 在已分配内存上构造。

本模块属 **framework 层**, 只依赖 `MemoryManager` 与 `ControlSpec.h`; 不感知控件类型语义、缓冲结构与
渲染分派, 不做几何解算(像素几何在解析期算好后送进来), 不提供任何线程同步。

本模块包含以下编程接口:

### 类

| 类 | 说明 |
| --- | --- |
| `Layout` | 一个界面的控件容器: 按索引持有的值语义控件序列, 线性或链式储存。 |

本模块不定义公开类型与公开常量: 控件信息 `CtrlInfo` 与坐标 `vec2` 定义在 `ControlSpec.h`, 本模块只
`#include` 使用。

---

## Layout 类 (Layout)

一个界面的控件容器。

### 语法

```cpp
class Layout
{
public:
    static Layout* CreateLayout(size_t num, BOOL IsLink = FALSE);
    static Layout& GetNullObject();
    static VOID DestroyLayout(Layout* layout);
    static VOID SetAllocators(Arena* arena, MemPool* pool);

    BOOL     IsLinkMode() const;
    BOOL     SetCtrlInfo(size_t index, const CtrlInfo& info);
    CtrlInfo* GetCtrlInfo(size_t index);
    CtrlInfo& GetCtrlInfoRef(size_t index);
    CtrlInfo* GetCtrlInfoByID(int id);
    CtrlInfo* GetLayoutBase(CtrlType type);
    size_t   GetShellNum();
    size_t   GetTotalNum() const;
    BOOL     AddCtrlInfo(size_t index, const CtrlInfo& info);
    BOOL     RemoveCtrlInfo(size_t index);
    BOOL     IsInvalid();
    BOOL     DeriveCtrlLayers();
    BOOL     DeriveCtrlPositions();
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `CreateLayout(size_t, BOOL)` | 唯一的创建入口: 按 `num` 预置控件, 储存模式在此确定; 失败返回 `NULL`。 |
| `GetNullObject()` | 取空布局引用(调用方"无效引用"的返回物); 静态实例, 长期有效。 |
| `DestroyLayout(Layout*)` | 调用析构(只析构, 不归还分配器); 入参 `NULL` 安全。 |
| `SetAllocators(Arena*, MemPool*)` | 注入两个分配器(静态, 全进程一份); 传 `NULL` 解绑。 |
| `IsLinkMode()` | 是否链式储存; `FALSE` = 线性(不可增删控件)。 |
| `SetCtrlInfo(size_t, const CtrlInfo&)` | 按索引按值覆盖控件信息; 越界返回 `FALSE`。 |
| `GetCtrlInfo(size_t)` | 按索引取可写控件指针; 越界返回 `NULL`。 |
| `GetCtrlInfoRef(size_t)` | 按索引取控件引用; 越界返回共享的静态空信息。 |
| `GetCtrlInfoByID(int)` | 按控件 ID 取可写控件指针(链序首个命中); 未找到返回 `NULL`。 |
| `GetLayoutBase(CtrlType)` | 按类型取结构件, 只在前三个槽位内查找; 未命中返回 `NULL`。 |
| `GetShellNum()` | 结构件计数(`WND` / `TITLE` / `CNTR` 中存在者的个数)。 |
| `GetTotalNum()` | 控件总数(两模式共用的计数)。 |
| `AddCtrlInfo(size_t, const CtrlInfo&)` | 在指定位置插入一枚控件(链式专属); 线性模式恒返回 `FALSE`。 |
| `RemoveCtrlInfo(size_t)` | 移除指定位置的控件(链式专属), 其后控件索引前移。 |
| `IsInvalid()` | 是否为无效布局(空对象 / 缺结构件 / 窗口件零尺寸)。 |
| `DeriveCtrlLayers()` | 整体重推全部控件的层次(`CtrlInfo::nLayer`; 按 `parentID` 链求值)。 |
| `DeriveCtrlPositions()` | 整体重推全部内容控件的有效坐标(`CtrlInfo::position`; 按 `parentID` 链累加声明坐标)。 |

### 注解

**构造与销毁**。构造与析构是私有成员, 创建只经 `CreateLayout`、释放只经 `DestroyLayout`, 调用方不
自行 `new` / `delete` 布局对象。`DestroyLayout(NULL)` 是无操作; 析构只逐控件(链式则逐节点)析构并把
链节点交还小对象池, **不归还** `Arena` 承载的布局对象、控件数组与链视图。销毁后对象内存仍属
`Arena`, 但逻辑上已析构, 不得再访问。

**创建失败判据**。`CreateLayout(num, IsLink)` 在下列任一情形返回 `NULL`: 分配器未注入; 布局对象或
链视图分配失败; 预置数量不足。判定口径 = 内部 `GetTotalNum() != num`(失败对象随即被析构, 它自身
已整链回卷节点并退化为空 + 线性)。`num` 为 `0` 是合法调用(空布局, `GetTotalNum() == 0`)。
`SetAllocators` 必须在创建任何布局之前一次性注入; 未注入 `Arena` 时 `CreateLayout` 直接返回 `NULL`,
构造过程不做任何分配。

**储存模式**。线性 = `Arena` 里一片连续 `CtrlInfo` 数组, 下标 O(1), 只读; 链式 = `Arena` 里的链视图
(首/尾/游标)加 `MemPool` 里的链节点, 支持按下标插入/删除, 控件信息内嵌在节点里 ⇒ 一枚控件一次分配,
删除即回池、再次新增即复用(回收前做归属校验)。模式在创建时确定, **不转换、不交叉、不回落**;
模式不支持的操作返回失败, 不静默降级。

**控件访问**。`GetCtrlInfo` / `GetCtrlInfoRef` / `SetCtrlInfo` 的下标合法域为 `0..GetTotalNum()-1`,
越界一律失败(`NULL` / 静态空信息 / `FALSE`), 不改动任何控件。`GetCtrlInfoRef` 越界返回的是一枚
**共享的可写静态对象**: 写它不改变任何布局, 也不应被当作某控件的存储; 需要"可写且存在"的语义请先
用 `GetCtrlInfo` 判空。`SetCtrlInfo` 是整结构赋值 ⇒ 指针字段(标签、缓冲)照抄, 不深拷贝也不释放;
需要深拷贝请用 `LayoutBuilder::CopyLayout`。控件访问不保证跨帧可用性: 消费方按索引取值, 不持久化
持有 `CtrlInfo*` / `CtrlInfo&`(同帧内使用不受限), 基于控件的旁挂结构一律以"布局 + 索引"为键, 并在
增删时同步。

**按 ID 寻址**。`GetCtrlInfoByID(id)` 按链序扫描**全部**控件并返回首个 `CtrlInfo::id` 命中者(两种储存
模式通用); 未命中或 `id` 不存在时返回 `NULL`。它是"父容器 ID"字段的消费入口(例: 层次推导按父取
层, 事件处理器由 `parentID` 找父控件), 不建立索引表 ⇒ 复杂度 O(n); 同布局内 `id` 重复时只认链序首个。

**结构件定位**。`GetLayoutBase(type)` 扫描范围 = 索引 `0..min(控件总数, 3)-1`, 返回该范围内**首个**
类型命中者; 越出该范围的一律不认。`GetShellNum()` 是**存在计数**(用同一个定位入口逐类型查询),
不是"前 N 个都是结构件"的假设。

**增删(链式专属)**。`AddCtrlInfo(index, info)` 的合法插入位置为 `0..GetTotalNum()`(等于总数即追加
到末尾), 控件信息按值复制进池内新节点; `RemoveCtrlInfo(index)` 的下标合法域为
`0..GetTotalNum()-1`, 节点析构后交还池。两者在线性模式下**恒返回 `FALSE`**, 索引越界或分配失败亦
返回 `FALSE`。插入/删除成功后其后控件索引随之平移; 复杂度 = 按下标定位(内部游标就近)加 O(1) 挂接,
顺序或逆序遍历配顺序删除摊销 O(1)。

**插入后的层次重推**。层次按父容器 ID 链求值、与链序无关 ⇒ `AddCtrlInfo` 在挂链后**整体重推**
(调同一口径的 `DeriveCtrlLayers`, 幂等): 新节点可能成为链上任意位置控件的父, 只重推其后继不够。
`RemoveCtrlInfo` 不重推层次; 移除父容器后, 原先以它为父的控件在下一次重推时才回落到底档。
坐标同理: 删除后不重推有效坐标(已算出的 `position` 保留到下一次 `DeriveCtrlPositions`)。
插入参数按**声明坐标**解释: `AddCtrlInfo` 在入链前把传入的 `position` 同步为 `posDeclared`, 有效坐标
随后由 `DeriveCtrlPositions` 按父链重算(与层次同一次重推)。

**增删与事件委托**。链式增删不解析事件委托(`pEventHandler` 按传入的 `CtrlInfo` 原样保留); 新增的
委托控件(`CTRL_EVENT_ENTRUST_PARENT`)须由调用方在增删后重跑
`LayoutBuilder::ResolveEntrustedEvents`, 否则保持无响应。移除父容器时, 已解析到该父的委托者仍持原
处理函数指针(不在删除时回卷); 需要按当前父链重算的同样重跑上述函数。

**层次递推**。`DeriveCtrlLayers()` 整体重推全部控件的 `CtrlInfo::nLayer`, 供绕过插入路径的批量装配
统一(解析器逐枚 `SetCtrlInfo`、整体重装、搬移之后); 对同一状态幂等; 两种储存模式都支持(链式沿链
指针、线性按数组下标)。空布局或无储存视图时返回 `FALSE`。

层次口径(按父链): 取**父控件**(按 `parentID` 经 `GetCtrlInfoByID` 寻址)的层次为 last, 加上本控件
自身类型的步进 `CtrlLayerStep` —— 域(`RECT` / `INPUT_*` / `LIST_*` / `COMBO*` / `RENDER` / `SLIDER`)
下沉一档、件(`BUTTON` / `CHECKBOX`)上浮一档、其余(结构件 / `TEXT` / `TIMEBAR` / `PROG` /
`UNKNOWN` / `INVALID`)继承。层次是有符号相对量, 允许为负。父缺失(`parentID` 为 `-1`、目标 `id`
不存在、以自身为父、成环)时 last 取 `0`(窗口底档) —— 成环由"趟数上限 + 兜底"
收口, 不会死循环。结构件(`WND` / `TITLE` / `CNTR`)恒 `0`(无视 `parent` 声明)。

推导**只认声明, 不看几何与链序**: 父可声明在子之后, 也可与子无包含关系。实现为"初值 → 按父链
迭代收敛(趟数上限 = 控件总数, 每趟按 ID 线性查找父) → 成环兜底"三步, 上界 O(n³), 控件规模下有界。
层次到投影层号的换算(含钳制)统一走 `ControlSpec.h` 的 `CtrlLayerOf`, 本模块不另写一份。

**坐标累加**。`DeriveCtrlPositions()` 整体重推全部**内容控件**的有效坐标:
`CtrlInfo::position = CtrlInfo::posDeclared + 父链上各枚控件的 posDeclared 之和`。口径:

| 判据 | 处置 |
| --- | --- |
| 累加来源 | 只读 `posDeclared`(声明坐标, 几何解算写入) ⇒ 与链序无关、可反复重推、与层次重推互不干扰 |
| 父链终点 | 父缺失(`parentID` 为 `-1` / 目标 `id` 不存在)即止; `WND` 的坐标是桌面位置, 累加在 `WND` 处止步; `TITLE` / `CNTR` 照常计入(它们的坐标是窗口本地坐标) |
| 成环 / 自指 | 父链回到自身或上溯步数越界 ⇒ 视同无父容器(偏移按 `0` 计), 不取链上半截的偏和 |
| 结构件 | 不参与累加也不被本函数改写: `WND` / `TITLE` / `CNTR` 的有效坐标恒等于声明坐标(窗口位置记录在运行期会被窗口级交互改写) |
| 调用时机 | 几何解算之后(装配出口的调用结果必为 `0`, 因为那时声明坐标尚未落地); `AddCtrlInfo` 内自动调用 |

消费面的换算义务: 内容控件在**内容容器**内绘制, 容器已提供裁剪与原点偏移 ⇒ 绘制侧必须把有效坐标换算成
容器本地坐标(有效坐标 − 内容容器原点), 见 `Controls::DrawCtrl` 与 `LayoutRender` 的命中判定; 数据面与
命中面共用同一坐标(窗口本地)。

**空对象与有效性**。`GetNullObject()` 返回函数内静态实例的引用, 该引用长期有效; `NULLOBJ` 在
**首次被调用时**登记。`IsInvalid()` 的三条判据(任一成立即"无效"): 本对象就是 `GetNullObject()` 的
实例; 缺 `WND` 或 `CNTR`; `WND` 的 `size == {0,0}`。因登记发生在首次取用时, 若要在"从未取过空
对象"的进程里判定某个空布局为无效, 需先取一次。

**分配器与所有权**。布局对象自身、线性控件数组、链视图由 `Arena` 承载 ⇒ 不能也不需要单独释放,
`DestroyLayout` 只调析构; 链节点由 `MemPool` 承载 ⇒ 删除/析构时回池复用。`Arena` 的整体释放由持有者
负责。`SetAllocators(NULL, NULL)` 仅用于初始化失败回卷与退出清理。

**失败语义**。统一为返回 `FALSE` / `NULL` / 空引用, 不抛异常; 控件信息缓冲与标签串的所有权不在本
模块(按值覆盖, 指针照抄, 不分配也不释放)。

**相邻模块边界**。控件类型身份(枚举 ↔ 类型串)与层次步进来自 `ControlSpec.h` 的 `GetTypeEnum` /
`CtrlLayerStep` / `CtrlLayerOf`; 本模块只按 `CtrlType` 存值, 不承载缓冲结构与渲染分派
(缓冲布局与容量在 `CtrlBuffer.h`, 渲染分派在渲染侧)。分配器只调用 `MemoryManager` 的现有公开接口, 不扩其接口。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/layout/Layout.h`(`src\framework\layout\Layout.h`) |
| 实现 | `src\framework\layout\Layout.cpp` |
| 命名空间 | 全局(类) |
| 依赖 | `framework/base/mem/MemoryManager.h` · `framework/base/ControlSpec.h` · `ucrt` |
| 平台 | Windows 全版本 |
| 线程安全 | 否(分配器与空对象为静态成员, 无内部同步); 跨线程使用须由调用方串行 |
