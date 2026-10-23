# LayoutModel 模块 (src\framework\layout\LayoutModel.h)

## 概述

布局数据模型(中间态):JSON 与 bin 两种文件格式**被解析为 UI 布局对象之前的统一内存状态**。载体是
"块 + 链" —— 每个布局与每个元素各自成一块(块内自持规格与其链节点, 节点在块首 ⇒ 块地址即节点地址),
逻辑顺序由链表达; 增删布局/元素是链指针操作加单块分配/回收, 就地完成, 不移动其它块。

本模块属 **framework 层**: 定长字符串、几何系数、参考尺寸、元素规格、布局信息、两种链节点与文件中间
态都定义在此, 并提供块链操作与字段工具; **不做任何堆分配**(内存一律取自静态注入的分配器), 不做几何
解算, 不做校验/压缩/加密算法(算法在 `FileCodec`), 也不提供"中间态 → 文件"的序列化(写方向不在本类),
也没有反向的"布局对象 → 中间态": 展开态里的几何是已解算的像素, 逆不回未解算的系数声明。

控件层次不进本模块: 层次是展开态的派生量, 由 `Layout` 在装配与增删时按 `parentID` 父链递推;
`ElementSpec` / `LayoutInfo` 不承载该字段, JSON 与 bin 都不写不读它 ⇒ 中间态与文件格式契约不受
影响(`parent` 本身是声明字段, 由本模块承载)。

bin 文件格式的字节布局(文件头 / 段头 / 头部区 / 控件记录、版本号位域、规模上限与段大小公式)声明在本
文件, 因为它就是上列结构体的内存镜像; 读写两侧对这些记录区整块 `memcpy`, 三段 `static_assert` 把结构
体尺寸与关键偏移钉死。

本模块包含以下编程接口:

### 类型

| 类型 | 说明 |
| --- | --- |
| `SizeFactor` | 单轴向几何系数: 8 项参考量系数 + 固定像素偏移。 |
| `SizeFactorSet` | 元素四轴的系数集合(`x` / `y` / `width` / `height`)。 |
| `RefSize` | 布局参考尺寸与经验参数(文件 `autosize` 段的直映)。 |
| `ElementSpec` | 一个控件在文件里的全部声明数据。 |
| `LayoutInfo` | 一个布局的头部信息(bin 头部区的镜像)。 |
| `ElementNode` | 元素链节点(与元素规格同块)。 |
| `LayoutNode` | 布局链节点(与布局信息同块)。 |
| `FileMidState` | 整份文档的中间态: 布局链哨兵 + 两个计数。 |

### 函数

| 函数 | 说明 |
| --- | --- |
| `BinSegBytes` | 计算一个布局段(段头 + 头部区 + 控件记录区)的字节数。 |

### 类

| 类 | 说明 |
| --- | --- |
| `LayoutModel` | 中间态的块链操作与字段工具(纯静态, 不可实例化)。 |

### 常量

**定长字符串上限**

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `STR_NAME` | `64` | 布局名(`LayoutInfo::name`)容量, 含结尾符。 |
| `STR_TYPE` | `64` | 元素类型串(`ElementSpec::type`)容量。 |
| `STR_LABEL` | `128` | 元素标签与布局标签(`ElementSpec::label` / `LayoutInfo::label`)容量。 |
| `STR_TEXT` | `128` | 元素文本(`ElementSpec::text`)容量。 |
| `STR_EVENT` | `64` | 元素事件名(`ElementSpec::event`)容量。 |
| `STR_MODE` | `64` | 自动尺寸模式串(`RefSize::mode`)容量。 |

**几何系数**

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `GENFACTOR_NUM` | `8` | 单轴参考量系数个数, 与 `SizeFactor::coeff.RefCoeffs` 的长度一致。 |

**bin 文件格式契约(v2)**

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `BIN_VERSION` | `0x00000200` | 版本号: 高 24 位代际版本号 + 低 8 位格式标记;代际 2 = 控件记录增 `parentID`。 |
| `BIN_VER_GEN_MASK` | `0xFFFFFF00` | 代际版本号的位掩码(取用时右移 8 位比对)。 |
| `BIN_VER_FLAG_MASK` | `0x000000FF` | 格式标记的位掩码。 |
| `BIN_VER_FLAG_COMPRESS` | `0x00000001` | 标记位: 压缩形态(尚未实现, 出现即拒绝)。 |
| `BIN_VER_FLAG_ENCRYPT` | `0x00000002` | 标记位: 加密形态(尚未实现, 出现即拒绝)。 |
| `BIN_FILE_HEAD` | `12` | 文件头字节数: 校验码 4 + 版本号 4 + 段数量 4。 |
| `BIN_CRC_SIZE` | `4` | 校验码字段字节数; 计算时该字段按 0 参与。 |
| `BIN_SEG_HEAD` | `4` | 段头字节数: 本段大小(**含本字段自身**)。 |
| `BIN_LAYOUT_SIZE` | `376`(`sizeof(LayoutInfo)`) | 头部区字节数 = 一枚 `LayoutInfo`(定长, 无独立的头部大小字段)。 |
| `BIN_LAYOUT_NELEM` | `372`(`offsetof(LayoutInfo, dwElements)`) | 头部区内"控件数量"字段的偏移。 |
| `BIN_CTRL_SIZE` | `536`(`sizeof(ElementSpec)`) | 单控件记录字节数。 |

**文档规模上限(JSON 与 bin 共用)**

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `LAYOUT_MAX` | `1024` | 单文件布局(段)数上限。 |
| `CTRL_MAX` | `8192` | 单布局控件数上限。 |

---

## SizeFactor 结构 (LayoutModel)

单轴向几何系数:8 项参考量系数 + 一个固定像素偏移。

### 语法

```cpp
struct SizeFactor
{
    union
    {
        struct
        {
            FLOAT screenWidth;
            FLOAT screenHeight;
            FLOAT windowWidth;
            FLOAT windowHeight;
            FLOAT controlHeight;
            FLOAT fontSize;
            FLOAT space;
            FLOAT padding;
        };
        FLOAT RefCoeffs[8];
    } coeff;
    int offset;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `coeff.screenWidth` / `coeff.screenHeight` | 系数 0 / 1: 倍屏幕宽度 / 倍屏幕高度。 |
| `coeff.windowWidth` / `coeff.windowHeight` | 系数 2 / 3: 倍窗口宽度 / 倍窗口高度。 |
| `coeff.controlHeight` | 系数 4: 倍控件高度。 |
| `coeff.fontSize` | 系数 5: 倍字体大小。 |
| `coeff.space` | 系数 6: 倍控件间隙。 |
| `coeff.padding` | 系数 7: 倍内部边距。 |
| `coeff.RefCoeffs[8]` | 下标视图: 按参考量序号 0..7 排列的系数数组, 与具名成员同一存储。 |
| `offset` | 固定偏移量(像素), 用于微调位置或尺寸。 |

### 注解

两种视图由 `union` 提供, 共享存储、无填充(8 枚 `FLOAT` 加一枚 `int`); 具名成员的声明顺序即参考量
序号顺序, 与 `AutoSize::SizeInfo` 的成员声明顺序逐一对应。系数是"相对参考尺寸的比例", 解算不在本
模块。

---

## SizeFactorSet 结构 (LayoutModel)

元素四个轴向的系数集合。

### 语法

```cpp
struct SizeFactorSet
{
    SizeFactor x;
    SizeFactor y;
    SizeFactor width;
    SizeFactor height;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `x` | X 位置系数。 |
| `y` | Y 位置系数。 |
| `width` | 宽度系数。 |
| `height` | 高度系数。 |

### 注解

四轴各自独立解算: 位置轴与尺寸轴用同一套参考量口径, 但可各自给出不同的系数与偏移。

---

## RefSize 结构 (LayoutModel)

布局参考尺寸与经验参数, 是文件 `autosize` 段的直映; 每个布局独立一份。

### 语法

```cpp
struct RefSize
{
    char mode[STR_MODE];
    int  unit;
    int  width;
    int  height;
    int  controlHeight;
    int  fontSize;
    int  space;
    int  padding;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `mode` | 自动尺寸模式串, 定长; 缺省值 `"AUTO_SIZE_BY_SCREEN_HEIGHT"`。 |
| `unit` | 单位: `0` = 千分比, `1` = 像素。 |
| `width` / `height` | 参考窗口宽度 / 高度。 |
| `controlHeight` | 参考控件高度。 |
| `fontSize` | 参考字体大小。 |
| `space` | 参考控件间隙。 |
| `padding` | 参考内部边距。 |

### 注解

`unit` 为 `0` 时上列六项为**千分比**(相对 `mode` 决定的换算基准), 为 `1` 时按像素解释。模式串到
自动尺寸模式的映射不属于数据模型(属解析与解算侧知识), 本文件只按串存 `mode`, 因此不依赖解算模块。
四个模式串的取值与 `AutoSize` 的自动尺寸模式一一对应。

---

## ElementSpec 结构 (LayoutModel)

一个控件在文件里的全部声明数据。

### 语法

```cpp
struct ElementSpec
{
    char       label[STR_LABEL];
    char       text [STR_TEXT ];
    char       type [STR_TYPE ];
    char       event[STR_EVENT];

    SizeFactor x;
    SizeFactor y;
    SizeFactor width;
    SizeFactor height;

    int        id;
    int        parentID;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `label` | 元素标签(控件 ID 与标题文本用), 定长。 |
| `text` | 元素文本内容, 定长。 |
| `type` | 元素类型, **串形态**; 未命中类型表即未知类型。 |
| `event` | 元素事件名; 绑定表未覆盖该名时控件无响应;写 `CTRL_EVENT_ENTRUST_PARENT` 表示把事件委托给父容器。 |
| `x` / `y` / `width` / `height` | 四轴几何系数(未解算)。 |
| `id` | 元素 ID。 |
| `parentID` | 父容器 ID(文档 `parent` 字段的直映); `-1` = 无父容器。 |

### 注解

四个字符串字段是**定长字符数组**(内嵌自持于所属块), 均含结尾符; 写入统一走 `StrCopy`, 超长按截断
处理。类型以串形态保存, 枚举映射在边界处完成(`GetTypeEnum` / `GetTypeName`) ⇒ 本模块不含类型表。

四轴系数是**未解算的声明值**, 像素几何只存在于展开态; 本结构不承载控件层次。

`parentID` 是**声明值**: 本模块只存它, 不做存在性校验也不推导; 指向不存在的 ID 时, 装配期的事件委托
解析静默降级(该控件无响应)。零值元素块的该字段由 `LayoutModel` 置 `-1`(不取零值, `0` 是合法 ID)。

---

## LayoutInfo 结构 (LayoutModel)

一个布局的头部信息, 同时是 bin 文件头部区的镜像。

### 语法

```cpp
struct LayoutInfo
{
    char        label[STR_LABEL];
    char        name [STR_NAME ];
    RefSize     refSize;
    SizeFactor  width;
    SizeFactor  height;
    BOOL        IsMaxiable;
    BOOL        IsResizeable;
    LONG        wndX;
    LONG        wndY;
    DWORD       dwElements;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `label` | 布局标签, 定长。 |
| `name` | 布局名称, 定长。 |
| `refSize` | 参考尺寸与经验参数。 |
| `width` / `height` | 布局宽 / 高系数(未解算)。 |
| `IsMaxiable` | 是否可最大化。 |
| `IsResizeable` | 是否可调整大小。 |
| `wndX` / `wndY` | 窗口桌面位置。 |
| `dwElements` | 本布局的元素数量, 与元素链长度一致。 |

### 注解

`dwElements` 是"本布局元素数"的唯一记数点: 元素增删时由 `ElementInsert` / `ElementRemove` 同步维护,
`StateInit` 依它建链, bin 读侧以**实际加载枚数**覆写它。结构体总长 `376` 字节, "控件数量"字段的偏移
`372`, 两者由 `static_assert` 钉死 ⇒ 改动字段布局即编译期报错, 须同步 `BIN_VERSION`。bin 的头部区就是
本结构的内存镜像, 整块 `memcpy` 出入, 不做逐字段解析。

---

## ElementNode 结构 (LayoutModel)

元素链节点, 与元素规格同块。

### 语法

```cpp
struct ElementNode
{
    ElementSpec* data;
    ElementNode* next;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `data` | 元素规格指针, 指向**同一块内**的 `ElementSpec`。 |
| `next` | 下一元素节点; `NULL` = 链尾。 |

### 注解

节点位于块首、规格紧随其后 ⇒ 节点地址即块地址, 回收时无需偏移换算;"节点指针"与"规格指针"同生
共死。元素链**无哨兵**: 空链即布局的 `elements` 为 `NULL`。

---

## LayoutNode 结构 (LayoutModel)

布局链节点, 与布局信息同块。

### 语法

```cpp
struct LayoutNode
{
    LayoutInfo*  info;
    ElementNode* elements;
    LayoutNode*  next;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `info` | 布局信息指针, 指向**同一块内**的 `LayoutInfo`。 |
| `elements` | 本布局的元素链头; `NULL` = 无元素。 |
| `next` | 下一布局节点; `NULL` = 链尾。 |

### 注解

节点位于块首、布局信息紧随其后 ⇒ 节点地址即块地址。布局链带一枚**哨兵头节点**(见
`FileMidState::layouts`), 哨兵自身的 `info` / `elements` 恒为 `NULL`, 只有 `next` 参与链。

---

## FileMidState 结构 (LayoutModel)

整份文档的中间态:布局链哨兵 + 两个计数。

### 语法

```cpp
struct FileMidState
{
    LayoutNode layouts;
    DWORD      dwLayouts;
    DWORD      dwElements;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `layouts` | 布局链**哨兵头节点**(值语义): `info` / `elements` 恒 `NULL`, `next` 指向首个布局节点; 空文档时 `next` 为 `NULL`。 |
| `dwLayouts` | 布局数量(当前链上)。 |
| `dwElements` | 元素总数(全部布局求和)。 |

### 注解

块的位置由链表达(不另设顺序池)。遍历布局一律从 `layouts.next`(或 `LayoutFirst`)开始, **不要**使用
哨兵自身的 `info` / `elements`。

按值传递只复制链头与计数, **不产生深拷贝**: 多个副本共享同一批块, 任一副本增删都会影响其余副本;
对任一副本调用 `StateClear` 后其余副本全部失效。空态(零值)即"无布局、无元素", 可直接作为初始值。

---

## BinSegBytes 函数 (LayoutModel)

计算一个布局段的字节数。

### 语法

```cpp
constexpr size_t BinSegBytes(
    [in] DWORD dwCtrls
);
```

### 参数

`[in] dwCtrls`

类型: **DWORD**

该布局的控件记录数。合法域 `0..CTRL_MAX`; 本函数不做上限校验(超上限由读侧按规模上限拒绝), 数值
再大也只做算术换算。

### 返回值

类型: **size_t**

段字节数 = `BIN_SEG_HEAD + BIN_LAYOUT_SIZE + dwCtrls × BIN_CTRL_SIZE`。`constexpr` 函数, 编译期即可
求值; 无失败路径。

### 注解

bin 的文件结构: 文件头(`BIN_FILE_HEAD` = 12 字节)加段区; 一个"段"就是一个布局, 由段头(4 字节, 值 =
本段大小且含该字段自身)、头部区(一枚 `LayoutInfo`)与控件记录区(若干枚 `ElementSpec`)组成; 文件总长 =
`BIN_FILE_HEAD` + 各段 `BinSegBytes(N)` 之和。

记录区是上列结构体的**内存镜像**, 读写两侧整块 `memcpy`, 因此字节序与结构体布局随编译目标, 文件不
另作字节序声明。段大小的下界校验、余量须整除 `BIN_CTRL_SIZE`、段区是否正好铺满文件等判定在 bin 读侧,
本函数只给公式。

---

## LayoutModel 类 (LayoutModel)

中间态的块链操作与字段工具。

### 语法

```cpp
class LayoutModel
{
public:
    static VOID SetAllocators(Arena* arena, MemPool* pool);

    static BOOL StrCopy(char* dst, size_t cap, LPCSTR src);

    static BOOL StateInit(FileMidState& state, DWORD dwLayouts, CONST DWORD* ElementCounts);
    static VOID StateClear(FileMidState& state);

    static LayoutNode*  LayoutAdd(FileMidState& state);
    static BOOL         LayoutRemove(FileMidState& state, DWORD layoutIndex);
    static DWORD        LayoutCount(CONST FileMidState& state);
    static LayoutNode*  LayoutAt(CONST FileMidState& state, DWORD layoutIndex);
    static LayoutNode*  LayoutFirst(CONST FileMidState& state);
    static LayoutNode*  LayoutNext(CONST LayoutNode* pNode);

    static ElementNode* ElementFirst(CONST LayoutNode* pNode);
    static ElementNode* ElementNext(CONST ElementNode* pElem);
    static ElementSpec* ElementAt(CONST FileMidState& state, DWORD layoutIndex, DWORD elementIndex);
    static BOOL         ElementInsert(FileMidState& state, DWORD layoutIndex, DWORD elementIndex);
    static BOOL         ElementRemove(FileMidState& state, DWORD layoutIndex, DWORD elementIndex);

    static INT  RefSizeValueAt(CONST RefSize& refSize, INT index);
    static VOID SetRefSizeValueAt(RefSize& refSize, INT index, INT value);
    static RefSize DefaultRefSize();

private:
    LayoutModel() = delete;
    ~LayoutModel() = delete;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `SetAllocators(Arena*, MemPool*)` | 注入块分配器(静态, 全进程一份); 传 `NULL` 解绑。 |
| `StrCopy(char*, size_t, LPCSTR)` | 把字符串安全拷入定长数组; 超长按容量截断并返回 `FALSE`。 |
| `StateInit(FileMidState&, DWORD, CONST DWORD*)` | 初始化一份中间态: 先清零, 再按布局数与各布局元素数逐块建链。 |
| `StateClear(FileMidState&)` | 释放中间态持有的全部块并复位为空态。 |
| `LayoutAdd(FileMidState&)` | 在布局链尾追加一个空布局。 |
| `LayoutRemove(FileMidState&, DWORD)` | 移除指定布局, 并回收其元素块与布局块。 |
| `LayoutCount(CONST FileMidState&)` | 遍历布局链取布局数量。 |
| `LayoutAt(CONST FileMidState&, DWORD)` | 按布局索引取布局节点; 越界返回 `NULL`。 |
| `LayoutFirst(CONST FileMidState&)` | 遍历入口: 取首个布局节点。 |
| `LayoutNext(CONST LayoutNode*)` | 遍历推进: 取下一布局节点。 |
| `ElementFirst(CONST LayoutNode*)` | 遍历入口: 取指定布局的元素链首。 |
| `ElementNext(CONST ElementNode*)` | 遍历推进: 取下一元素节点。 |
| `ElementAt(CONST FileMidState&, DWORD, DWORD)` | 按布局索引与元素索引取元素规格; 越界返回 `NULL`。 |
| `ElementInsert(FileMidState&, DWORD, DWORD)` | 在指定位置插入一枚空元素(其后元素索引后移)。 |
| `ElementRemove(FileMidState&, DWORD, DWORD)` | 移除指定元素并回收其元素块(其后元素索引前移)。 |
| `RefSizeValueAt(CONST RefSize&, INT)` | 按参考量序号读经验参数值; 序号越界返回 `0`。 |
| `SetRefSizeValueAt(RefSize&, INT, INT)` | 按参考量序号写经验参数值; 序号越界不写入。 |
| `DefaultRefSize()` | 取经验参数默认样例(屏高模式 + 千分比)。 |

### 注解

**构造与析构**。构造与析构私有且 `= delete` ⇒ 纯静态工具类, 不可实例化, 无状态; 全部成员按
`LayoutModel::` 静态调用。

**分配器注入**。`SetAllocators(arena, pool)` 必须在首次结构性操作之前一次性注入: `pool` 是小对象池
(元素块与布局块的常规来源), `arena` 是回落分配器(块尺寸超出 `MemPool::MAX_SMALL_SIZE` 或池分配失败
时使用); 传 `NULL` 即解绑(用于初始化失败回卷与退出清理)。未注入时 `StateInit` / `LayoutAdd` /
`ElementInsert` 等分配类操作一律失败(返回 `FALSE` / `NULL`), 中间态保持为空或原状; `StateClear`、
`StrCopy` 与参考量工具**不需要**分配器。

**定长字符串**。`StrCopy(dst, cap, src)`: `dst` 为 `NULL` 或 `cap` 为 `0` ⇒ 返回 `FALSE` 且不写入;
`src` 为 `NULL` ⇒ 写入空串并返回 `TRUE`; 长度不小于 `cap` ⇒ 拷满 `cap - 1` 字节、补结尾符并返回
`FALSE`; 正常拷入返回 `TRUE`。返回 `FALSE` 表示"入参非法或发生截断", **不表示没写进去** —— 截断时
内容已写入, 返回值只用于诊断。

**初始化与释放**。`StateInit(state, dwLayouts, ElementCounts)` 先整体清零(失败时保持空态), 再按布局
数逐块追加布局、按各布局的元素数逐块追加元素; `ElementCounts` 为 `NULL` 时每个布局按 `0` 个元素建。
任一分配失败即回卷已建块并返回 `FALSE`。成功时元素与布局内容全为**零值**。`StateClear(state)` 遍历
两条链逐块归还分配器(元素块与布局块)并复位为空态; 由回落分配器承载的块不归还, 随分配器整体释放。

**布局链操作**。`LayoutAdd(state)` 自哨兵头逐枚推进到链尾再尾插, 成功返回新节点(布局信息为零值),
失败返回 `NULL` 且中间态不变。`LayoutRemove(state, layoutIndex)` 的下标从 `0` 起, 越界返回 `FALSE` 且
原状态不变; 成功时先解链再回收该布局的元素块与布局块, 并把布局计数减一、元素总数减去该布局的元素数
(下溢保护)。`LayoutCount` 与 `LayoutAt` 都是线性遍历(O(n)), 不做游标缓存; `LayoutAt` 越界返回 `NULL`。

**遍历**。`LayoutFirst` / `LayoutNext` / `ElementFirst` / `ElementNext` 都是 O(1) 推进; 链尾或入参为
`NULL` 时返回 `NULL`。批量遍历请沿链走, 不要在循环里反复按下标取。

**元素操作**。`ElementAt(state, layoutIndex, elementIndex)` 自元素链头逐枚推进(O(n)), 布局或元素下标
越界返回 `NULL`。`ElementInsert(state, layoutIndex, elementIndex)` 的插入位置合法域为
`0..元素数`(等于元素数即追加到末尾), 先分配成功再改链 ⇒ 布局下标越界、插入位置越界或分配失败都返回
`FALSE` 且原状态不变; 新元素为零值 `ElementSpec`(字符串为空串、系数与 `id` 为 `0`), 需调用方填充。
`ElementRemove` 的元素下标合法域为 `0..元素数-1`, 成功时解链并回收该元素块, 同时更新布局的
`dwElements` 与中间态的元素总数。

**参考量工具**。`RefSizeValueAt` / `SetRefSizeValueAt` 的序号合法域为 `0..5`, 依次对应
宽 / 高 / 控件高 / 字号 / 间隙 / 内距; 读越界返回 `0`, 写越界不写入。`DefaultRefSize()` 返回屏高模式
加千分比的样例: 宽 `622`、高 `432`、控件高 `25`、字号 `17`、间隙 `10`、内距 `4`, `unit` 为 `0`。

**就地性与指针稳定**。增删布局/元素只改链与被增删的块, 不重建也不移动其它块 ⇒ 未被移除的
`LayoutNode*` / `LayoutInfo*` / `ElementSpec*` 在增删后仍然有效; 仅**被移除**的块归还分配器, 其指针
随即失效, 不得再使用。元素链节点的 `data` 指向同块规格, 故节点指针与规格指针同生共死。

**块来源与回收配对**。块由 `StateInit` / `LayoutAdd` / `ElementInsert` 分配, 由 `LayoutRemove` /
`ElementRemove` / `StateClear` 回收; 结构性操作要么成功、要么保持原状态。

**线程**。无任何内部同步, 分配器与工具函数都是静态的 ⇒ 跨线程使用须由调用方串行。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/layout/LayoutModel.h`(`src\framework\layout\LayoutModel.h`) |
| 实现 | `src\framework\layout\LayoutModel.cpp`;结构定义在头文件内 |
| 命名空间 | 全局作用域 |
| 依赖 | `minwindef.h` · `Layout` · `LayoutBuilder` · `framework/base/mem/MemoryManager.h` · `ucrt` |
| 平台 | Windows 全版本 |
| 线程安全 | 不适用(值类型, 按值传递) |
