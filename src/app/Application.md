# Application 模块 (src\app\Application.h)

## 概述

`Application` 是应用会话的持有者与编排者: 持有会话分配器上分配的成员块(`AppData`)、内嵌的文档承载者
(`Document`)、文档结构版本号; 对外提供文档模型访问、几何与参考量编辑、元素规格查询与中间态展开四个门面。
层归属为 app 层(应用主类), 框架复用时不含本模块。

三条硬契约:

1. **索引口径唯一**: 文档索引 == 布局控件索引。链内只含文档控件(编辑器的选中态表达件是链外视图叠加件),
   结构件(`WND` / `TITLE` / `CNTR`)在索引空间中正常占位也是文档控件 ⇒ 全部涉及索引的接口按同一口径解释,
   不存在换算层。
2. **预览基座唯一**: 全部文档条目共享同一个布局对象(预览基座), 其内容随激活条目装配 ⇒ 按任意合法索引取到的
   是同一对象; 跨结构变更持有的控件指针必然失效, 调用方按索引访问。
3. **文档面数据归承载者**: 条目数组 / 中间态 / 条目数 / 激活索引 / 关联路径 / 脏标记由 `doc\Document` 承载,
   本类只内嵌该承载者并按会话语义读写; 对外只提供取址访问器 `GetDocument`。

内存一律经 `AllocatorHub` 的两个分配器: 布局对象取自 `Arena`, 控件缓冲与中间态块优先取自 `MemPool`(超限回落
`Arena`), 条目数组、旁挂表与池内字符串同样取自这两个分配器; 各自的归还入口是中间态整份释放、控件缓冲回收
入口与布局析构入口 —— 本模块不越权释放他人内存, 自身也不做堆分配。分配器的注入顺序是装配契约:
`AllocatorHub::Init` → `Layout::SetAllocators` / `LayoutModel::SetAllocators` / `ProjCR::SetAllocators`
(同源同批) → 才可 `CreateLayout` / `StateInit`, 顺序不可颠倒。本模块不含线程安全机制。失败语义统一为返回
`FALSE` / `NULL` / 空引用 / 默认值, 不抛异常; 应用未就绪时查询接口一律返回空对象、默认值、`0` 或 `NULL`,
不崩、不制造空态替身。

本模块不做的事: 不做渲染与控件库(绘制、控件缓冲的构造与回收归框架侧), 不做编辑器界面与选中态表达(交互归
编辑侧, 选中态表达件在布局链外), 不持文档数据面(条目数组 / 中间态 / 关联路径 / 脏标记归承载者), 不持旁挂表
(原类型串与无缓冲声明文本归数据保真模块)。文件内的 `static` 工具件与内部编排函数不立条。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `Init` | 建立分配器与会话状态, 走启动加载链载入文档并取外壳快照。 |
| `Release` | 按逆序释放会话资源并解绑分配器。 |
| `IsReady` | 判断应用是否就绪(成员块已分配)。 |
| `GetDocument` | 取文档承载者地址(指针型, 未就绪为 `NULL`)。 |
| `GetDocCount` | 取布局条目数。 |
| `GetDocState` | 取当前文档中间态(只读)。 |
| `GetDocLayout` | 按条目索引取布局对象引用(全部条目共享同一基座)。 |
| `GetActiveIndex` | 取当前激活布局索引。 |
| `SetActiveIndex` | 切换激活布局(越界钳到末项)。 |
| `GetActiveLayout` | 取激活条目的布局对象引用。 |
| `GetActiveRef` | 取激活条目的解算后参考像素。 |
| `GetActiveRefSize` | 取激活条目的参考尺寸与经验参数。 |
| `GetDocRef` | 取指定条目的解算后参考像素。 |
| `GetDocRefSize` | 取指定条目的参考尺寸与经验参数。 |
| `GetDocWndRect` | 取指定条目的预览窗口矩形。 |
| `SetDocWndPos` | 写回指定条目的窗口桌面位置(仅内存生效)。 |
| `ActiveAutoSetRatio` | 写激活条目的参考量单项。 |
| `SetDocAutoRatio` | 写指定条目的参考量单项并重算该条目几何。 |
| `ApplyDocAutoRatio` | 写参考量单项并就地更新像素几何。 |
| `ApplyDocAutoMode` | 写自动尺寸模式串并就地更新像素几何。 |
| `ApplyDocAutoUnit` | 写参考量单位(不改几何)。 |
| `GetCtrlSpec` | 取指定文档索引的中间态元素规格。 |
| `ApplyDocCtrlGeometry` | 按元素规格就地重解一枚控件的像素几何。 |
| `SetCtrlType` | 改控件类型并按新类型重建该控件的缓冲。 |
| `MoveCtrl` | 在同一布局内拖拽重排控件(三处同步搬运)。 |
| `BuildEntryRef` | 依参考尺寸重建条目的窗口像素与内部参考。 |
| `ApplyEntryGeometry` | 依条目参考对全部控件求解像素几何。 |
| `BuildLayoutFromState` | 把中间态的指定布局展开为布局对象。 |

### 常量

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `GENUI_SHELL_LAYOUT_NAME` | `"GenUIEditor"` | 编辑窗口外壳布局在布局文档中的保留名; 外壳快照按该名查找。 |

---

## Init 函数 (Application)

建立会话分配器与会话状态, 走启动加载链载入文档, 并取外壳快照。

### 语法

```cpp
static BOOL Init();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 应用已就绪(成员块已分配且文档已载入); `FALSE` = 启动失败, 调用方诊断后中止启动。

### 注解

**建立分配器**: 先 `AllocatorHub::Init` 建立会话分配器(线性分配器容量 128 MB)。装配顺序是硬契约:
`AllocatorHub::Init` → `Layout::SetAllocators` / `LayoutModel::SetAllocators` / `ProjCR::SetAllocators`(同源
同批) → 才可 `CreateLayout` / `StateInit`, 顺序不可颠倒; 三者取同一对分配器(线性分配器 + 小对象池)。
分配器是静态成员, 本步必须早于后续任何 `CreateLayout` / `StateInit`, 中途解绑后不得再创建布局。

**分配成员存储**: 从 `Arena` 分配 `AppData`(内嵌文档承载者), 并把字段初始化到位: 条目数组置 `NULL`、
预览基座复位、中间态零值、条目数 `0`、激活索引 `0`、两个窗口句柄复位、文档版本号置 `1`、清脏标记、
内嵌成员字段复位, 并初始化资源加载原语。文档承载者内嵌使地址自分配起终生稳定, 不引入额外分配。

**接线**: 在成员存储就位之后注入文档 I/O 侧的承载者与三个会话面回调(预览基座建立 / 文档版本号递增 /
条目几何重算), 注入预览会话侧的三项会话句柄(承载者 / 版本号地址 / 分配器), 并注入旁挂表族的条目反查函数。
上述注入必须晚于成员存储分配, 否则该侧取到 `NULL`。

**启动加载链(四级顺序探测)**: 默认四级全放开, 逐级“探测 → 载入”, 命中即用:

| 级 | 来源 | 形态 |
| --- | --- | --- |
| 1 | exe 目录 `.\GenUI.bin`(外部文件) | bin |
| 2 | exe 目录 `.\GenUI.json`(外部文件) | JSON |
| 3 | 内置资源 `resource.res\GenUI.bin`(`IDR_GENUI_BIN = 102`) | bin |
| 4 | 内置资源 `resource.res\GenUI.json`(`IDR_GENUI_JSON = 101`) | JSON |

级序是口径, 形态由内容决定: 第 1/2 级走路径入口、第 3/4 级走内存入口, 两个入口内部按内容嗅探分派
(跳过 BOM 与空白后首字节为 `'{'` ⇒ JSON, 否则 bin), 与扩展名无关。命中级载入被拒(校验码 / 版本 /
段大小 / 超限 / 过短 / 分配失败 / 结构件排布违规)时打印诊断并把该级并入已试掩码, 以剩余级别重新探测;
探测本体(级序 / 文件名 / 资源 ID / 诊断文案 / 来源结果形状)在 `app\DocumentSource`, 本函数只做逐级回落编排。
全链无可用来源或全被拒 ⇒ 诊断 + 返回 `FALSE`(不内置代码回退)。构建期可向 exe 目录投放 `.\GenUI.bin`
(由工程源生成同源 bin), 此时启动命中第 1 级; 未投放时命中第 2 级 JSON。工程源 JSON 保留, 供只读输入与对账。

**外壳快照**: 与文档同一命中级别, 把保留名 `GENUI_SHELL_LAYOUT_NAME` 的布局解析为独立副本, 驱动编辑窗口内容
(会话内固定, 不随打开其它文档变化)。文件缺失 / 无该名 / 解析失败 ⇒ 快照无效; 本步非致命, 不阻断 `Init`,
无效性由宿主在启动校验处判定并中止。

**收尾**: 返回 `TRUE`。

**失败清理**: 分配器建立失败 / 成员存储分配失败 / 全链失败均走同一清理: 三处分配器解绑 → 分配器整体释放 →
成员块指针置 `NULL` → 文档 I/O 侧承载者、预览会话侧三项句柄、旁挂表条目反查同点复位, 返回 `FALSE`。
路径入口读毕即关文件, 载入被拒不遗留句柄。

---

## Release 函数 (Application)

释放会话资源并解绑分配器。

### 语法

```cpp
static VOID Release();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

释放**逆启动顺序**执行: 中间态整份释放(`LayoutModel::StateClear`) → 内嵌成员布局对象析构 → 外壳快照复位
(含内嵌成员与快照条目) → 内嵌成员再次清理 → 预览唯一基座释放 → 三处分配器解绑 → 分配器整体释放 →
成员块指针置 `NULL` → 文档 I/O 侧承载者、预览会话侧三项句柄、旁挂表条目反查同点复位。

中间态与基座必须先回收: 两者的块与链节点要归还小对象池, 只有归还之后才能整体释放线性分配器, 顺序颠倒会
使池内节点失去宿主。`Release` 只把成员块指针置 `NULL`, **不搬迁地址**; 调用后取址类接口一律返回 `NULL`,
此前交出的承载者地址随即悬垂。分配器未建立时本函数为空操作。进程退出前调用一次。

本模块不释放控件缓冲与条目布局对象: 前者归控件缓冲的回收入口, 后者归基座 / 快照的释放入口。

---

## IsReady 函数 (Application)

判断应用是否就绪。

### 语法

```cpp
static BOOL IsReady();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 成员块已分配; `FALSE` = 未就绪(成员块指针为 `NULL`)。

### 注解

判据就是成员块指针非空, 即“分配成员存储”之后、`Release` 或失败清理之前这一区间。本函数是只读判据入口,
不制造空态替身, 也不改变任何状态。

---

## GetDocument 函数 (Application)

取文档承载者地址。

### 语法

```cpp
static Document* GetDocument();
```

### 参数

此函数无参数。

### 返回值

类型: **Document\***

承载者地址; 应用未就绪(成员块为 `NULL`)时返回 `NULL`。

### 注解

**指针型契约**: 未就绪返回 `NULL`, 不伪造空态替身 —— 承载者保持“纯数据、无策略”的定位, 空态降级由调用方
显式处置。

**地址稳定性**: 承载者按值内嵌于成员块 ⇒ 分配后地址终生不变; `NewDocument` / `LoadDocument` 只重置其字段
(条目数组 / 中间态), 不换址, 但跨结构变更持有的条目与元素指针必然失效, 调用方不得缓存。承载者不得改为指针式
持有(那会新增一次分配并改变地址语义)。

**时序硬约束**: 取用必须晚于 `Init`(成员块在 `Init` 内才分配, 之前取到 `NULL`); 放弃持有必须早于 `Release`
(之后该地址随分配器失效 ⇒ 悬垂)。本访问器与宿主上下文的窗口句柄是同一类“随 `Init` 就绪、随 `Release` 失效”
的量, 与函数指针型注入(可早于 `Init`、不随 `Init` 复位)口径不同。

---

## GetDocCount 函数 (Application)

取布局条目数。

### 语法

```cpp
static DWORD GetDocCount();
```

### 参数

此函数无参数。

### 返回值

类型: **DWORD**

条目数; 应用未就绪时为 `0`。

### 注解

转发承载者的条目计数原语, 不推导、不缓存。计数随整份文档替换与布局增删同步。

---

## GetDocState 函数 (Application)

取当前文档中间态(只读)。

### 语法

```cpp
static CONST FileMidState& GetDocState();
```

### 参数

此函数无参数。

### 返回值

类型: **CONST FileMidState&**

中间态引用; 应用未就绪时返回本文件内的静态空态对象的引用。

### 注解

中间态是几何系数与控件声明的真源, 也是保存写出物。本接口供只读消费(序列化写出等), 调用方不得就地改动。
空态回落用本文件内静态空态对象, 不搬入承载者; 该空态对象是共享实例, 写它不影响任何文档。

---

## GetDocLayout 函数 (Application)

按条目索引取布局对象引用。

### 语法

```cpp
static Layout& GetDocLayout(
  [in] DWORD index
);
```

### 参数

`[in] index`

类型: **DWORD**

条目索引, 合法域 `0`..`GetDocCount() - 1`。

### 返回值

类型: **Layout&**

命中的布局对象引用; 未就绪或索引越界时返回空布局对象(`Layout::GetNullObject()`)的引用。

### 注解

全部条目共享同一个布局对象(预览唯一基座), 其内容随激活条目装配 ⇒ 任意合法索引返回的是**同一对象**。
空布局不是 `NULL` 而是空对象引用, 调用方用 `Layout::IsInvalid` 判别有效性。

基座身份自始至终不变, 但其控件内容在结构变更与同步路径上会被整体重装 ⇒ 调用方不得跨同步持有控件指针或
引用, 按索引访问。条目布局对象本身不归本函数释放。

---

## GetActiveIndex 函数 (Application)

取当前激活布局索引。

### 语法

```cpp
static DWORD GetActiveIndex();
```

### 参数

此函数无参数。

### 返回值

类型: **DWORD**

激活条目索引; 应用未就绪时为 `0`。

### 注解

激活索引是预览与编辑的共同目标, 由 `SetActiveIndex` 改写。本函数只读, 不做越界钳制。

---

## SetActiveIndex 函数 (Application)

切换激活布局。

### 语法

```cpp
static BOOL SetActiveIndex(
  [in] DWORD index
);
```

### 参数

`[in] index`

类型: **DWORD**

目标条目索引; 越界时钳到末项(`GetDocCount() - 1`), 不报错。

### 返回值

类型: **BOOL**

`TRUE` = 已处置(含“索引未变”的情形); `FALSE` = 应用未就绪。

### 注解

**未就绪判据**: 条目数组未挂接或条目数为 `0`, 判断形态为承载者的首个条目取不到 ⇒ 返回 `FALSE`。

**已就绪**: 越界先钳到末项; 目标与当前激活索引相同时**不改动**、不递增版本号、不触发预览同步, 仍返回 `TRUE`;
不同时写入激活索引、递增文档结构版本号, 并请求预览基座按新激活条目的内容同步(基座对象身份不变)。

版本号递增是 UI 缓冲同步信号, 由本模块负责, 承载者只做字段存取。

---

## GetActiveLayout 函数 (Application)

取激活条目的布局对象引用。

### 语法

```cpp
static Layout& GetActiveLayout();
```

### 参数

此函数无参数。

### 返回值

类型: **Layout&**

激活条目的布局对象引用; 未就绪或越界时为空布局对象引用。

### 注解

等价于 `GetDocLayout(GetActiveIndex())`, 是同一基座 + 当前激活索引的组合结果, 不引入额外状态。有效性判别见
`GetDocLayout`。

---

## GetActiveRef 函数 (Application)

取激活条目的解算后参考像素。

### 语法

```cpp
static CONST AutoSize& GetActiveRef();
```

### 参数

此函数无参数。

### 返回值

类型: **CONST AutoSize&**

激活条目的参考像素; 未就绪或越界时为默认参考的引用(见 `GetDocRef`)。

### 注解

等价于 `GetDocRef(GetActiveIndex())`。参考像素是控件几何求解与渲染的基准: 其窗口宽高等于该布局窗口客户区
像素, 控件按“窗口内”布局, 不相对屏幕画布。

---

## GetActiveRefSize 函数 (Application)

取激活条目的参考尺寸与经验参数。

### 语法

```cpp
static CONST RefSize& GetActiveRefSize();
```

### 参数

此函数无参数。

### 返回值

类型: **CONST RefSize&**

激活条目的参考尺寸与经验参数; 未就绪或越界时为默认样例的引用(见 `GetDocRefSize`)。

### 注解

等价于 `GetDocRefSize(GetActiveIndex())`。该结构是中间态字段的只读快照, 供样式与换算取用; 写入走
`ActiveAutoSetRatio` / `SetDocAutoRatio` 一类入口。

---

## GetDocRef 函数 (Application)

取指定条目的解算后参考像素。

### 语法

```cpp
static CONST AutoSize& GetDocRef(
  [in] DWORD index
);
```

### 参数

`[in] index`

类型: **DWORD**

条目索引, 合法域 `0`..`GetDocCount() - 1`。

### 返回值

类型: **CONST AutoSize&**

命中的参考像素引用; 未就绪或索引越界时返回本函数内静态默认参考的引用。

### 注解

参考像素由条目参考尺寸经换算基准求解而来, 是几何求解与渲染的基准; 本函数只读, 不重建。

默认参考是函数内静态对象(622 / 432 千分比窗口宽高与 25 / 17 / 10 / 4 的控件高 / 字号 / 间隔 / 内距,
屏高模式), 首次调用时构造; 引用长期有效, 但写它不影响任何条目。

---

## GetDocRefSize 函数 (Application)

取指定条目的参考尺寸与经验参数。

### 语法

```cpp
static CONST RefSize& GetDocRefSize(
  [in] DWORD index
);
```

### 参数

`[in] index`

类型: **DWORD**

条目索引, 合法域 `0`..`GetDocCount() - 1`。

### 返回值

类型: **CONST RefSize&**

命中的参考尺寸与经验参数引用; 未就绪或索引越界时返回静态默认样例的引用。

### 注解

默认样例取自中间态模块的默认参考尺寸(屏高模式 + 千分比), 首次调用时构造。该值是条目字段的只读视图, 写入
一律走经验参数编辑入口 —— 那些入口会把条目副本与中间态布局信息两处同写, 只改副本会丢改动。

---

## GetDocWndRect 函数 (Application)

取指定条目的预览窗口矩形。

### 语法

```cpp
static BOOL GetDocWndRect(
  [in]  DWORD  index,
  [out] LONG*  x,
  [out] LONG*  y,
  [out] DWORD* w,
  [out] DWORD* h
);
```

### 参数

`[in] index`

类型: **DWORD**

条目索引, 合法域 `0`..`GetDocCount() - 1`。

`[out] x` / `[out] y`

类型: **LONG\***

窗口桌面位置(像素), 来自该布局 `WND` 控件的几何偏移。

`[out] w` / `[out] h`

类型: **DWORD\***

窗口客户区宽高, 由自动尺寸的宽高千分比乘以换算基准得到。

### 返回值

类型: **BOOL**

`TRUE` = 已写出; `FALSE` = 应用未就绪或索引越界。

### 注解

四个出参**逐个可空**: 传 `NULL` 的项不写出, 其余照常; 失败时一律不写出参。宿主据此建窗与每帧摆窗; 客户区
尺寸的下限夹取由调用方处置, 本函数原样给出条目数值。

---

## SetDocWndPos 函数 (Application)

写回指定条目的窗口桌面位置。

### 语法

```cpp
static BOOL SetDocWndPos(
  [in] DWORD index,
  [in] LONG  x,
  [in] LONG  y
);
```

### 参数

`[in] index`

类型: **DWORD**

条目索引, 合法域 `0`..`GetDocCount() - 1`。

`[in] x` / `[in] y`

类型: **LONG**

新的桌面位置(像素)。

### 返回值

类型: **BOOL**

`TRUE` = 已处置(含位置未变的情形); `FALSE` = 应用未就绪或索引越界。

### 注解

**仅内存生效, 不落盘**: 位置写回条目字段, 并同步到中间态布局信息(中间态才是保存写出物, 只写条目副本会丢
改动)。位置与当前值相同时不改动、不递增版本号, 仍返回 `TRUE`; 中间态节点缺失时只改条目副本。

条目字段与中间态两处同源同写是本函数的契约, 调用方无需自行补齐另一处。

---

## ActiveAutoSetRatio 函数 (Application)

写激活条目的参考量单项。

### 语法

```cpp
static BOOL ActiveAutoSetRatio(
  [in] INT   index,
  [in] DWORD ratio
);
```

### 参数

`[in] index`

类型: **INT**

参考量序号, 合法域 `0`..`5`(宽 / 高 / 控件高 / 字号 / 间隙 / 内距)。

`[in] ratio`

类型: **DWORD**

参考量输入值, 按该条目单位解释(千分比或像素), 取值域 `0`..`10000`。

### 返回值

类型: **BOOL**

`TRUE` = 已写入并重算几何; `FALSE` = 应用未就绪、序号越界或条目索引失效。

### 注解

等价于 `SetDocAutoRatio(GetActiveIndex(), index, ratio)`, 六项任一变更均即时影响激活条目几何。单位解释、
两处同写、版本号递增与几何重算的契约见 `SetDocAutoRatio`。

---

## SetDocAutoRatio 函数 (Application)

写指定条目的参考量单项并重算该条目几何。

### 语法

```cpp
static BOOL SetDocAutoRatio(
  [in] DWORD docIndex,
  [in] INT   index,
  [in] DWORD ratio
);
```

### 参数

`[in] docIndex`

类型: **DWORD**

条目索引, 合法域 `0`..`GetDocCount() - 1`; 非激活条目亦可。

`[in] index`

类型: **INT**

参考量序号, 合法域 `0`..`5`。

`[in] ratio`

类型: **DWORD**

参考量输入值, 按该条目单位解释: 单位为千分比时按原值, 单位为像素时按换算基准反算为千分比(结果超 `10000`
按 `10000` 处理); 中间态恒存千分比。

### 返回值

类型: **BOOL**

`TRUE` = 已写入并重算; `FALSE` = 应用未就绪、序号越界或条目索引失效(拒绝写入, 原状态不变)。

### 注解

写入形态: 参数量在条目副本与中间态布局信息**两处同写**(中间态才是保存写出物), 条目副本跟随以保持内存显示
一致; 中间态节点缺失时只改副本。随后递增文档结构版本号, 并转调条目几何重算入口 —— 重建该条目的窗口几何与
参考, 激活条目的基座内容按中间态重新装配。

按索引取不到条目(文档已替换 / 布局已删)即拒绝, 属正常失败路径, 不诊断。

---

## ApplyDocAutoRatio 函数 (Application)

写参考量单项并就地更新像素几何。

### 语法

```cpp
static BOOL ApplyDocAutoRatio(
  [in] DWORD docIndex,
  [in] INT   index,
  [in] DWORD ratio
);
```

### 参数

`[in] docIndex`

类型: **DWORD**

条目索引, 合法域 `0`..`GetDocCount() - 1`。

`[in] index`

类型: **INT**

参考量序号, 合法域 `0`..`5`。

`[in] ratio`

类型: **DWORD**

参考量输入值, 按该条目单位解释(千分比原值, 或像素按基准反算为千分比; 中间态恒存千分比)。

### 返回值

类型: **BOOL**

`TRUE` = 已写入并就地更新; `FALSE` = 应用未就绪、序号越界或条目索引失效。

### 注解

与 `SetDocAutoRatio` 的差别在于**不做整体重装**: 只重建该条目的参考尺寸与窗口像素, 再按中间态元素系数逐枚
改写基座控件的像素几何 —— 控件数量、类型缓冲、事件绑定均不动; 非激活条目只重建自身参考(其几何在切为激活时
装配)。

写入与版本号契约同 `SetDocAutoRatio`(条目副本与中间态两处同写, 递增版本号)。

---

## ApplyDocAutoMode 函数 (Application)

写自动尺寸模式串并就地更新像素几何。

### 语法

```cpp
static BOOL ApplyDocAutoMode(
  [in] DWORD  docIndex,
  [in] LPCSTR mode
);
```

### 参数

`[in] docIndex`

类型: **DWORD**

条目索引, 合法域 `0`..`GetDocCount() - 1`。

`[in] mode`

类型: **LPCSTR**

自动尺寸模式串; 未知串按屏高模式处理(与换算入口的模式名解析同口径)。

### 返回值

类型: **BOOL**

`TRUE` = 已写入并就地更新; `FALSE` = 应用未就绪、`mode` 为空指针或空串、条目索引失效。

### 注解

换算基准随模式变化 ⇒ 写入后重建该条目参考, 并对激活条目就地改写基座控件的像素几何(与 `ApplyDocAutoRatio`
同法: 不整体重装, 控件数量、类型缓冲、事件绑定均不动)。

模式串同样两处同写(条目副本 + 中间态布局信息), 并递增文档结构版本号。

---

## ApplyDocAutoUnit 函数 (Application)

写参考量单位。

### 语法

```cpp
static BOOL ApplyDocAutoUnit(
  [in] DWORD docIndex,
  [in] INT   unit
);
```

### 参数

`[in] docIndex`

类型: **DWORD**

条目索引, 合法域 `0`..`GetDocCount() - 1`。

`[in] unit`

类型: **INT**

参考量单位: `0` = 千分比, `1` = 像素。

### 返回值

类型: **BOOL**

`TRUE` = 已写入; `FALSE` = 应用未就绪、单位越界或条目索引失效。

### 注解

单位只决定入参如何解释(见 `ApplyDocAutoRatio`), 中间态存储恒为千分比 ⇒ **换单位不改几何**: 控件像素几何与
预览窗口尺寸均不变。

写入两处同写(条目副本 + 中间态布局信息)并递增文档结构版本号; 不做几何重算。

---

## GetCtrlSpec 函数 (Application)

取指定文档索引的中间态元素规格。

### 语法

```cpp
static ElementSpec* GetCtrlSpec(
  [in] Layout* layout,
  [in] size_t  index
);
```

### 参数

`[in] layout`

类型: **Layout\***

目标布局对象; 非文档条目(如外壳快照)不属文档面。

`[in] index`

类型: **size_t**

文档索引, 与中间态元素链一一对应。

### 返回值

类型: **ElementSpec\***

命中的元素规格指针; 布局无效 / 非文档条目 / 索引越界时返回 `NULL`。

### 注解

四轴系数(系数 + 像素偏移)与控件声明的真源在中间态元素规格; 控件对象只保留像素几何(position / size),
不持系数。元素规格随索引与控件同步(增删控件时中间态池重建)。

返回的指针指向中间态, 就地写它会绕过两处同写与版本号递增的既有入口, 调用方应仅作只读取用, 并按“布局 +
索引”建立键、不跨结构变更缓存。

---

## ApplyDocCtrlGeometry 函数 (Application)

按元素规格就地重解一枚控件的像素几何。

### 语法

```cpp
static BOOL ApplyDocCtrlGeometry(
  [in] Layout* layout,
  [in] size_t  index
);
```

### 参数

`[in] layout`

类型: **Layout\***

目标布局对象(编辑器提交时即当前激活条目承载的预览基座)。

`[in] index`

类型: **size_t**

文档索引, 与中间态元素索引一一对应。

### 返回值

类型: **BOOL**

`TRUE` = 已就地重解; `FALSE` = 入参无效、非文档条目、索引越界或元素规格缺失。

### 注解

像素几何 = 中间态元素规格(系数与偏移)的解析产物; 本函数按条目参考尺寸重算该控件的 position / size, **不**做
整体重装: 控件数量、类型缓冲、事件绑定均不动。

失败时保持现状像素、不写任何字段。编辑器在系数与偏移提交之后调用本入口即生效, 无需重新装配。

---

## SetCtrlType 函数 (Application)

改控件类型并按新类型重建该控件的缓冲。

### 语法

```cpp
static BOOL SetCtrlType(
  [in] Layout*  layout,
  [in] size_t   index,
  [in] CtrlType type
);
```

### 参数

`[in] layout`

类型: **Layout\***

目标布局对象; `NULL` 或空布局(无效引用)一律拒绝。

`[in] index`

类型: **size_t**

文档索引, 与中间态元素索引一一对应。

`[in] type`

类型: **CtrlType**

新类型; 不可构建类型(`CTRL_TYPE_INVALID` / `CTRL_TYPE_UNKNOWN`)拒绝。

### 返回值

类型: **BOOL**

`TRUE` = 已改并重建缓冲; `FALSE` = 入参无效、元素规格或控件缺失、缓冲重建失败。

### 注解

三处同步: ① 中间态元素类型串(枚举经类型表转串); ② 布局对象的控件类型; ③ 控件缓冲按新类型重建, 文本取自
中间态声明文本。旧缓冲在重建之前回收(先回收后重建是回收纪律), 顺序颠倒会让旧缓冲失去归还点。

**不改几何**: 控件像素几何由中间态元素规格决定, 与新类型无关。成功后该索引的旁挂“原类型串”被移除
(该索引现为已知类型)。

缓冲重建失败时控件类型已改写、旧缓冲已回收, 返回 `FALSE`; 调用方不得假定失败后该控件仍可用, 需重新装配该
条目或撤销改动。

---

## MoveCtrl 函数 (Application)

在同一布局内拖拽重排控件。

### 语法

```cpp
static BOOL MoveCtrl(
  [in] Layout* layout,
  [in] size_t  from,
  [in] size_t  dest
);
```

### 参数

`[in] layout`

类型: **Layout\***

目标布局对象; `NULL`、空布局或应用未就绪一律拒绝。

`[in] from`

类型: **size_t**

源文档索引, 合法域 `0`..`GetTotalNum() - 1`。

`[in] dest`

类型: **size_t**

目标插入索引, **删除源之后的口径**, 合法域 `0`..`GetTotalNum() - 1`。

### 返回值

类型: **BOOL**

`TRUE` = 已重排; `FALSE` = 入参无效、索引越界、源与目标同处、中间态或控件链操作失败。

### 注解

控件顺序的真源是中间态元素链, 因此重排按“取出源的三处副本 → 三处同步删除 → 三处同步插入”实施, 三处 =
控件值、中间态元素规格、旁挂两表(原类型串与声明文本)。控件缓冲与标签指针随控件值整体搬移(池内自持, 不逐块
释放); 控件身份不受影响, 标识不变。

成功后整体重推控件层次(搬移改变了链序与前序包含关系, 增量重推不成立)。预览选中态表达件在链外, 本函数不触碰
其状态。

**失败不回卷**: 任一步失败即退出, 已完成的删除或插入保持生效 ⇒ 调用方不得假定失败后三处仍一致, 需重新装配
该条目。

---

## BuildEntryRef 函数 (Application)

依参考尺寸重建条目的窗口像素与内部参考。

### 语法

```cpp
static VOID BuildEntryRef(
  [in, out] LayoutEntry& entry
);
```

### 参数

`[in, out] entry`

类型: **LayoutEntry&**

待重建的条目; 读其参考尺寸(模式 / 窗口宽高千分比 / 单位), 写其窗口客户区宽高与内部参考。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

换算: 由参考尺寸的模式取换算基准(基准小于 `1` 时按 `1` 处理); 窗口客户区像素 = 宽高千分比 × 基准 / 1000,
宽高各自下限 `16` 像素; 内部参考的窗口宽高即该客户区像素 —— 控件按“窗口内”布局, 不相对屏幕画布。

`WINDOW` 一类模式在编辑器语境下按屏兜底。本函数是装配族共用件(外壳快照 / 文档条目 / 内嵌成员同法), 只写
条目的几何字段, 不改中间态, 也不递增版本号。

---

## ApplyEntryGeometry 函数 (Application)

依条目参考对全部控件求解像素几何。

### 语法

```cpp
static VOID ApplyEntryGeometry(
  [in, out] LayoutEntry& entry
);
```

### 参数

`[in, out] entry`

类型: **LayoutEntry&**

目标条目; 读其布局对象与内部参考, 逐控件写像素几何。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

逐控件按中间态元素规格(系数与像素偏移)与条目参考求解 position / size; 元素规格缺失(尚未挂接)时保留现状
像素, 索引照常推进并跳过该枚, 不中断其余控件。

中间态元素与文档索引一一对应且均带系数(不存在“无系数”态): 新增控件的元素以像素偏移加零系数表达, 因此重解
结果与新增当刻的几何一致。布局对象为 `NULL` 时直接返回。

本函数服务外壳快照(线性只读展开态)与客户区校正; 文档条目的控件几何在装配路径上逐枚解算。

---

## BuildLayoutFromState 函数 (Application)

把中间态的指定布局展开为布局对象。

### 语法

```cpp
static Layout* BuildLayoutFromState(
  [in] CONST FileMidState& state,
  [in] int                index     = 0,
  [in] BOOL               IsLinkmode = FALSE
);
```

### 参数

`[in] state`

类型: **CONST FileMidState&**

布局模型中间态(元素规格与几何系数的来源)。

`[in] index`

类型: **int**

布局索引, 默认 `0`; 负数拒绝。

`[in] IsLinkmode`

类型: **BOOL**

展开态的储存模式: `FALSE`(默认)= 线性, `TRUE` = 链式。

### 返回值

类型: **Layout\***

展开出的布局对象指针; 索引为负、中间态取不到该布局节点或构建失败时返回 `NULL`。

### 注解

展开本体转调布局构建模块(中间态节点 → 布局对象 + 池内深拷贝), 本函数只保留负数拒绝与“取节点”一步。自由函数
`FormatToUI` 的声明已随结构治理移除(无调用者), 本函数是同一展开动作的应用侧落点。

展开出的布局对象取自注入的分配器, **分配器必须先注入**, 否则必然失败; 该对象由调用方经 `Layout::DestroyLayout`
析构(只析构不归还分配器, 内存随分配器整体释放)。展开不修改中间态。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `app/Application.h`(`src\app\Application.h`) |
| 实现 | `src\app\Application.cpp` |
| 命名空间 | 全局(类 `Application`; 文件作用域保留名常量 `GENUI_SHELL_LAYOUT_NAME`) |
| 依赖 | 工程内: `AllocatorHub`、`Layout`、`LayoutModel`、`LayoutBuilder`、`Document`、`DocumentSource`、`DocumentIO`、`PreviewSession`、`FlexHost`、`RawAttributes`、`GeometrySolver`、`Controls`、`ResourceLoader`、`HostContext`、`FrameEvent`; 系统: 无额外链接库(仅 Win32 头与 CRT) |
| 平台 | Windows; 尺寸与指针假设按 64 位构建 |
| 线程安全 | 否(分配器为静态成员, 全进程一份; 会话状态单线程访问, 跨线程需外部同步) |
