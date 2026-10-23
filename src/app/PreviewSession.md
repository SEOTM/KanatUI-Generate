# PreviewSession 模块 (src\app\PreviewSession.h)

## 概述

`PreviewSession` 承接会话面只读 / 装配件的两面:

1. 预览基座与帧间同步: 全部文档条目共享的同一个链式布局对象(基座), 其内容按中间态当前 active 条目装配;
   编辑动作改完中间态后经统一入口请求同步 —— 渲染帧内挂帧间队列、帧末排空执行, 排空执行中当场同步。
2. 外壳快照: 启动载入的布局文件里保留名 `GENUI_SHELL_LAYOUT_NAME` 那一份布局的独立只读副本, 供编辑窗口
   渲染; 含装载、查询、建窗几何校正与失效复位。

两面同属"会话面只读 / 装配件": 生命周期都随应用会话(都在应用侧"分配成员存储"一步建立、释放点同点回收),
都与被编辑文档的切换或新建无关。

数据面全部经注入的句柄读取: 承载者(条目数组 / 中间态 / 激活索引)、版本号地址(结构变更信号)、分配器句柄
(仅作就绪判据)。本模块不持文档面、不持版本号本体、不持旁挂表本体、不建四级探测、不做内存池建立与释放;
也不拥有任何布局对象的存储 —— 基座与快照对象都由会话分配器与小对象池持有, 本模块只负责调用析构。

父链面: 基座装配把中间态的父容器 ID 与解算出的声明坐标写入控件, 并在装配出口统一重推控件层次与有效坐标
(有效坐标 = 声明坐标 + 父链累加); 声明为"委托父容器"的控件在预览侧一律改绑预览点击处理器, 不解析真实委托。

内部件不立条: 私有静态件 `IsReady`(就绪判据) / `FreeShellSnapshot`(快照释放) /
`BuildShellSnapshotFromSolver`(快照构建)的语义并入相关公开函数的注解; 保留名
`GENUI_SHELL_LAYOUT_NAME` 定义在 `app\Application.h`, 本模块不重复定义。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `SetSession` | 注入文档承载者 / 版本号地址 / 会话分配器句柄。 |
| `GetPreviewBase` | 取预览唯一基座(只读, 不建立)。 |
| `ResetPreviewBase` | 复位基座指针(只丢指针, 不释放对象)。 |
| `EnsurePreviewBase` | 取基座, 不存在则建立空链式布局并重指全部条目。 |
| `FreePreviewBase` | 释放基座: 回收控件缓冲、析构对象、断开条目引用。 |
| `EnsurePreviewBaseHook` | 命令层回调形态的基座取用入口。 |
| `BumpDocRevHook` | 命令层回调形态的文档版本号递增。 |
| `SyncPreviewLayout` | 把中间态当前内容按编辑情况更新到基座。 |
| `RequestPreviewSync` | 统一同步入口: 帧内挂队列, 排空中当场同步。 |
| `OnPreviewSyncRequest` | 帧末排空的同步处理器。 |
| `RecalcEntryGeometry` | 依条目 autosize 重建窗口像素与参考尺寸。 |
| `LoadShellSnapshot` | 从文件装载外壳快照(非致命)。 |
| `LoadShellSnapshotFromMemory` | 从内存缓冲装载外壳快照。 |
| `GetShellValid` | 外壳快照是否有效(只看是否已解析出该布局)。 |
| `GetShellLayout` | 取外壳布局(编辑窗口渲染源), 无效给空对象。 |
| `GetShellRef` | 取外壳解算后的参考像素。 |
| `GetShellRefSize` | 取外壳中间态参考尺寸与经验参数。 |
| `GetShellEntry` | 取外壳快照条目指针(与预览基座同族的反查入口)。 |
| `GetShellWndRect` | 取外壳窗口矩形(宿主据此建编辑窗口)。 |
| `SetShellClientSize` | 建窗后以实际客户区幂等校正外壳几何。 |
| `ResetShellSnapshot` | 复位外壳快照全部状态(含内嵌成员)。 |

### 类型

| 类型 | 说明 |
| --- | --- |
| `PreviewSession` | 全静态类: 基座族与外壳快照族的唯一入口。 |

---

## SetSession 函数 (PreviewSession)

注入文档承载者、文档版本号地址与会话分配器句柄。

### 语法

```cpp
static VOID SetSession(
  [in] Document* pDocument,
  [in] LONG*     pnRev,
  [in] Arena*    pArenaHub
);
```

### 参数

`[in] pDocument`

类型: **Document\***

文档承载者句柄(条目数组 / 中间态 / 激活索引的读取源)。`NULL` = 复位, 此后预览族接口一律视为未就绪。

`[in] pnRev`

类型: **LONG\***

文档结构变更版本号的地址。`NULL` = 不递增: `BumpDocRevHook` 随之显式降级为不驱动, 不报错。

`[in] pArenaHub`

类型: **Arena\***

会话分配器句柄, 仅作外壳快照区的"是否就绪"判据, 不用于分配。`NULL` = 外壳快照族未就绪。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

三个句柄可分别传 `NULL`(逐个复位), 也可整体传 `NULL` 一次性复位。

注入时机: 应用侧"分配成员存储"之后 —— 承载者按值内嵌在成员块内, 早于该步取不到其地址。复位时机: 初始化
失败清理与释放的同点(成员存储置空同点), 因此"承载者失效"与"本模块未就绪"始终同时成立, 不存在中间态。

注入不做任何事: 不建立对象、不分配内存、不触碰既有基座与快照; 重复注入等价于覆盖三个借用句柄。

就绪口径分两面, 不要混用: 预览族以 `pDocument == NULL` 判定"未注入"(取基座返回 `NULL`、建立与同步失败);
外壳快照族以 `pArenaHub != NULL` 且会话分配器本身非空判定就绪。由于第三参只作判据, 就绪不等于"可直接分配",
分配一律走 `framework\base\mem\AllocatorHub` 的公开入口。

三句柄都是借用: 本模块不释放、不拷贝、不接管所有权; 调用方须保证它们在复位之前比本模块的所有调用活得久。

---

## GetPreviewBase 函数 (PreviewSession)

取预览唯一基座。

### 语法

```cpp
static Layout* GetPreviewBase();
```

### 参数

此函数无参数。

### 返回值

类型: **Layout\***

基座指针; 未建立或已释放返回 `NULL`。

### 注解

只读返回当前指针: 不建立、不校验、不重指条目。指针对应一个会话级单例对象, 身份在会话内稳定 —— 可长期用于
比较("该布局是否为预览基座")。

指针只在会话内有效: 经过 `FreePreviewBase` 或 `ResetPreviewBase` 之后不得再使用(对象可能已析构)。

---

## ResetPreviewBase 函数 (PreviewSession)

复位基座指针。

### 语法

```cpp
static VOID ResetPreviewBase();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

只丢弃指针, 不释放对象: 基座对象由会话线性分配器持有, 随池整体释放, 本函数不做析构、不回收控件缓冲。

由应用侧成员初始化一步原位调用 ⇒ 之后 `GetPreviewBase` 返回 `NULL`, 下次 `EnsurePreviewBase` 会重新建立
一枚新的空链式布局。

复位不改动任何条目的 `layout` 字段: 条目仍指向旧对象, 直到下一次建立或同步重指。因此复位后到重新建立之间的
窗口里, 取用路径可能拿到已失效的布局指针 ⇒ 复位与重建应连续完成。

---

## EnsurePreviewBase 函数 (PreviewSession)

取预览唯一基座, 不存在则建立空链式布局, 并把全部文档条目的布局引用指向它。

### 语法

```cpp
static Layout* EnsurePreviewBase();
```

### 参数

此函数无参数。

### 返回值

类型: **Layout\***

基座指针; `NULL` = 未注入承载者, 或建立失败(链式布局分配失败, 此时不提交半成品, 下次调用重试)。

### 注解

幂等: 已有基座时跳过建立, 只重指条目引用; 无基座时新建一枚"空链式布局" —— 结构件与内容控件由
`SyncPreviewLayout` 按中间态追加, 对象身份自始至终不变(不重建对象)。

建立的形态必须是链式: 后续同步按增删控件实现, 非链式基座会被同步入口拒绝。

全部文档条目共享同一对象 ⇒ 取用路径"任意条目都返回有效对象"; 重指只改条目的布局指针, 不动条目其余字段。
承载者为空时直接返回 `NULL`(不建立)。建立需要会话分配器可用(链节点取自小对象池)。

---

## FreePreviewBase 函数 (PreviewSession)

释放预览唯一基座。

### 语法

```cpp
static VOID FreePreviewBase();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

只在应用释放时调用。顺序固定: 回收基座全部控件的自身缓冲 → 析构布局对象(链节点回小对象池) → 基座指针置空
→ 断开全部条目的布局引用(逐条置 `NULL`)。

基座对象本身的内存归会话线性分配器, 此处不归还, 只析构。未注入承载者或当前无基座时无操作(幂等)。

释放后条目不再指向任何布局 ⇒ 取用路径在重新建立前应视为"无布局"。

---

## EnsurePreviewBaseHook 函数 (PreviewSession)

命令层会话回调形态的基座取用入口。

### 语法

```cpp
static Layout* EnsurePreviewBaseHook();
```

### 参数

此函数无参数。

### 返回值

类型: **Layout\***

基座指针; `NULL` = 未注入承载者(不进入建立路径)或建立失败。

### 注解

语义与 `EnsurePreviewBase` 相同, 差别只在"未注入承载者"这一档: 本函数在入口显式早返 `NULL`, 不进入建立
路径。

存在两个入口的原因是可调用形态: 命令层以函数指针获得同义能力(回调形状见 `framework\base\FuncPtr.h` 的
`pfnEnsurePreviewBase`), 从而不直接依赖本模块的类名与静态状态。

---

## BumpDocRevHook 函数 (PreviewSession)

命令层会话回调形态的文档结构变更版本号递增。

### 语法

```cpp
static VOID BumpDocRevHook();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

递增的是注入的地址所指的版本号(自增 1), 用作"界面文本缓冲需同步"的信号。未注入地址 ⇒ 不递增(显式降级:
不报错、不找替代)。

只递增: 不判断回绕、不比较、不以任何方式读写文档本体, 也不触发同步(同步由调用方另行请求)。

回调形状见 `framework\base\FuncPtr.h` 的 `pfnBumpDocRev`; 版本号本体归应用侧的会话数据, 本模块只借地址写它。

---

## SyncPreviewLayout 函数 (PreviewSession)

把中间态当前内容按编辑情况更新到基座。

### 语法

```cpp
static BOOL SyncPreviewLayout();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 完整同步; `FALSE` = 无文档 / 无中间态节点 / 存在失败项(已装配部分保留)。

### 注解

前置判据依次为: 承载者与条目数组非空且条目数非 0(否则直接 `FALSE`, 无诊断); active 索引越界时回落 0; 该
条目须有中间态节点(缺则诊断并 `FALSE`); 基座须可用且为链式布局(不满足则诊断并 `FALSE`, 必要时在此建立
基座)。

随后按固定六段执行:

1. 结构件区 = 基座当前已装配的结构件槽位数, 且不超过中间态元素数(`min(基座结构件数, 元素数)`)。首次为空链
   基座 ⇒ 槽位数为 0, 结构件随之走第 4 段的追加路径。
2. 卸载内容区: 自尾向头逐枚处理, 先回收该控件的自身缓冲再按下标移除; 移除失败即诊断、计数并停止本段; 旁挂
   表条目(原类型串 / 声明文本)随索引前移一并移除。
3. 结构件就地刷新: 写 `id` / 父容器 ID / 类型 / 标签 / 四轴像素几何 / 文本缓冲, 并写旁挂的原类型串与声明文本。
   事件绑定保留不动 —— 结构件的既有绑定(如标题栏拖动)必须存活。
4. 内容控件全部追加: 按中间态元素链逐枚装配(含缓冲分配与事件绑定); 装配失败即回池、诊断、计数并跳过; 随后
   对"原本有绑定且非结构件"的内容控件改绑预览点击处理器, 写像素几何与声明坐标, 尾插到链尾(失败则回池、诊断、
   计数并停止追加); 最后写旁挂两表。声明为"委托父容器"的控件在此不解析真实委托, 一律改绑预览点击处理器
   (避免点它触发业务事件); 无点击记录器时保持无响应。
5. 层次与有效坐标整理: 在全部内容控件装配完成后整体重推两遍 —— 控件层次(按 `parentID` 父链)与有效坐标
   (声明坐标加父链累加)。逐枚尾插使各控件的层次与坐标只按"插入当刻的前序"求值, 统一重推让结果与装配顺序
   无关(两者幂等, 重复调用不漂移)。该步即使本批出现诊断也必须执行: 声明坐标已写、父链尚未累加, 不重推会让
   这些控件停在未累加的坐标上。
6. 窗口位置记录(实现内编号 4-ter: 第 4 段装配块的收尾一步): 基座 `WND` 件的 `position` / `posDeclared` 置为该条目的窗口
   桌面位置(`wndX/wndY`)。结构件刷新与坐标重推都按布局内的声明坐标写该字段(声明值恒为 0), 而预览窗口的桌面
   位置来自条目字段(标题栏拖动经 `SetDocWndPos` 写回) ⇒ 以条目字段为唯一真源覆盖, 并把 `posDeclared` 同步为
   同值(重推坐标只读声明坐标, 不会把记录改回旧值)。渲染出口每帧以该记录为参数经 `AcquireWnd` 结算预览窗口
   几何 ⇒ 切换 active 时窗口随之移到新条目的位置。

收尾: 条目布局引用指向基座(使取用路径恒返回有效对象), 返回值 = "无失败项"。

点击处理器改绑的两条边界: ① 结构件(窗口 / 标题 / 容器三类)一律不覆盖 —— 首次装配时结构件也走追加路径, 故
按类型排除而不是按索引排除, 预览标题栏的拖动等既有绑定必须保留; ② 原本没有事件绑定的控件保持不绑定 —— 控件
库只对已绑定的文本 / 矩形类控件补一枚覆盖控件矩形的可交互项, 给它们挂处理器会让那枚先提交的覆盖项抢走其上方
可交互控件的命中, 这类控件的点击由渲染出口判定。

像素口径: 四轴像素 = 元素系数经 `GeometrySolver::FactorAbsPx` 按条目参考尺寸换算; 该解算值同时作为该控件的
声明坐标(父控件坐标系内的坐标), 有效坐标由第 5 段的父链重推给出。标签每轮重指当前中间态池(池重建后旧指针
失效), 因此标签不跨轮持有。

每次调用都按当前中间态重装内容控件, 调用时机由外部决定; 部分失败逐条诊断后继续 —— 已装配部分保留(尽力而
为), 返回 `FALSE` 只表示"未完整完成", 不表示"什么都没做"。

选中态表达件不在基座内(它是编辑器自持的视图叠加件) ⇒ 本函数不涉及它, 也无须"先摘除、后重建"。

可调用形态: 命令层与编辑侧经函数指针调用本函数(回调形状见 `framework\base\FuncPtr.h` 的 `pfnSyncPreviewLayout`),
因此本入口必须保持"无参数、无返回值以外依赖"的形态。

---

## RequestPreviewSync 函数 (PreviewSession)

请求把中间态同步到基座, 编辑动作改完中间态后的统一入口。

### 语法

```cpp
static BOOL RequestPreviewSync();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 已挂载到帧间队列, 或已当场同步(含同步本身未完整完成的尽力而为结果); `FALSE` = 挂载失败(帧间队列
满, 请求丢失)。

### 注解

两种时机: 渲染帧内 ⇒ 把同步请求挂到帧间事件队列, 帧末排空时执行; 同一帧内多次请求由队列按"处理函数 + 全部
参数"合并为一枚。帧末排空执行中 ⇒ 当场同步(此时两窗口都不在渲染帧内)。

帧内路径直接返回挂载结果, 它不代表已同步: 帧内调用后, 同一帧结束前基座内容仍可能是旧的 ⇒ 需要"同步完成后
再量取几何"的调用方必须走非帧内路径, 或在帧末之后再读。

队列满时请求丢失且不重试, 诊断由队列负责输出。

---

## OnPreviewSyncRequest 函数 (PreviewSession)

帧末排空时的同步请求处理器。

### 语法

```cpp
static VOID OnPreviewSyncRequest(
  [in] VOID*    obj,
  [in] uint64_t oper,
  [in] uint64_t param,
  [in] void*    data
);
```

### 参数

`[in] obj`

类型: **VOID\***

帧间事件的第一元组位。本请求不使用(同步作用于全局唯一基座), 传任意值。

`[in] oper`

类型: **uint64_t**

第二元组位。本请求不使用。

`[in] param`

类型: **uint64_t**

第三元组位。本请求不使用。

`[in] data`

类型: **void\***

第四元组位。本请求不使用。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

实现只转调 `SyncPreviewLayout()`: 处理器只改数据、不绘制, 因此可在排空阶段安全执行。

签名固定为帧间事件处理函数形状(见 `framework\dispatch\FrameEvent.h` 的 `Post`), 由 `RequestPreviewSync` 挂载; 不供
外部直接调用(直接调用只会立即同步一次, 绕过排队语义)。

四参数一律显式标记为未使用, 以免被误认为存在按参数区分请求的通道。

---

## RecalcEntryGeometry 函数 (PreviewSession)

依条目 autosize 重建窗口像素与内部参考尺寸, 并在该条目为 active 时同步基座。

### 语法

```cpp
static VOID RecalcEntryGeometry(
  [in] DWORD index
);
```

### 参数

`[in] index`

类型: **DWORD**

文档条目索引。越界或该条目取不到时静默返回(不重算、不诊断)。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

先依条目自有的尺寸参数重建窗口像素与内部参考尺寸(与顶层窗口同法), 随后只在该条目为 active 时按新参考尺寸
同步基座 —— 基座只承载当前 active 条目的控件。同步经 `SyncPreviewLayout`, 因此控件的声明坐标、控件层次与
有效坐标都会按新参考尺寸与父链重新给出。

非 active 条目在此只重建自身参考尺寸与窗口像素; 其控件几何留到它被切为 active 时由 `SyncPreviewLayout`
求解。因此本函数不保证"任何条目调用后几何都已解算"。

用于 autosize / 参考尺寸变更之后; 未注入承载者时静默返回。可调用形态见 `framework\base\FuncPtr.h` 的
`pfnRecalcEntryGeometry`(编辑器侧按索引回调)。

---

## LoadShellSnapshot 函数 (PreviewSession)

从布局文件装载外壳快照。

### 语法

```cpp
static BOOL LoadShellSnapshot(
  [in] LPCSTR path
);
```

### 参数

`[in] path`

类型: **LPCSTR**

布局文件路径。`NULL` 返回 `FALSE`(静默)。

### 返回值

类型: **BOOL**

`TRUE` = 已构建快照; `FALSE` = 未就绪 / 路径为空 / 解析器创建失败 / 解析失败 / 构建失败(如文件里没有该
保留名布局)。

### 注解

非致命: 失败只表示"快照无效", 不抛错、不中止进程 —— 由调用方诊断后决定是否中止启动。文件缺失与"文件里没有
该布局"同归失败。

过程: 取解析器(`DocumentIO::NewSolver`)→ 按内容嗅探分派 JSON / bin 的整文件解析 → 转构建入口。解析器是临时
件: 其规格字符串已进会话内存池, 因此解析器对象本身不必长期持有, 解析产物(中间态与字符串)可长期使用。

替换语义: 构建入口在开始处先释放既有快照, 因此成功装载即替换; 而"解析成功但找不到该布局"会让既有快照已被
释放、新快照未提交 ⇒ 结束时快照为无效状态(不留旧快照)。调用方不要假设"失败 = 旧快照还在"。

就绪判据: 注入的分配器句柄非空且会话分配器本身非空。未就绪时不触碰任何状态(既有快照保持不变)。

---

## LoadShellSnapshotFromMemory 函数 (PreviewSession)

从内存缓冲装载外壳快照。

### 语法

```cpp
static BOOL LoadShellSnapshotFromMemory(
  [in] LPVOID pData,
  [in] DWORD  cbData
);
```

### 参数

`[in] pData`

类型: **LPVOID**

内存中的布局文档(启动加载链命中内置资源时即该级返回的池内副本)。`NULL` 返回 `FALSE`。

`[in] cbData`

类型: **DWORD**

缓冲字节数。`0` 返回 `FALSE`。

### 返回值

类型: **BOOL**

`TRUE` = 已构建快照; `FALSE` = 未就绪 / 缓冲为空 / 长度为零 / 解析或构建失败。

### 注解

与文件入口同语义、同替换口径、同非致命约定, 差别只在数据来源: 本入口按长度解析, 不要求缓冲以 `'\0'`
结尾之外的额外形态(加载链返回的副本已补 `'\0'`)。

缓冲由调用方提供: 本模块只读它, 不接管所有权、不释放、不修改; 解析产物另存于会话池。

---

## GetShellValid 函数 (PreviewSession)

判断外壳快照是否有效。

### 语法

```cpp
static BOOL GetShellValid();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 就绪, 且有效位为真, 且快照条目非空, 且其布局对象非空; 其余情况一律 `FALSE`。

### 注解

判据只看"已成功解析出该保留名布局", 与几何是否已按实际客户区校正无关: 未调用 `SetShellClientSize` 的快照
也算有效。

它是外壳查询族的统一前置判据: 其余查询在无效时各返缺省物(空布局对象 / 缺省样例 / `FALSE`)。因此调用方不必
自己拼判据, 但必须承受"有效 ≠ 几何已校正"这一档。

未就绪时同样返回 `FALSE`(四项合取, 就绪是其中一项)。

---

## GetShellLayout 函数 (PreviewSession)

取外壳布局, 即编辑窗口的渲染源。

### 语法

```cpp
static Layout& GetShellLayout();
```

### 参数

此函数无参数。

### 返回值

类型: **Layout&**

快照布局对象的引用; 快照无效时返回 `Layout::GetNullObject()` 的引用(空对象)。

### 注解

返回引用而非指针 ⇒ 不提供空判据: 用前须经 `GetShellValid` 判定, 或接受"拿到的是空对象"。

快照布局是线性只读展开态: 可读、可渲染, 但不应增删控件 —— 增删会与快照条目的几何脱节。

引用在快照被释放(`ResetShellSnapshot` 或下一次装载)后失效 ⇒ 不得跨这两点持有。

---

## GetShellRef 函数 (PreviewSession)

取外壳解算后的参考像素。

### 语法

```cpp
static CONST AutoSize& GetShellRef();
```

### 参数

此函数无参数。

### 返回值

类型: **CONST AutoSize&**

命中返回快照条目内解算后的参考尺寸; 无效时返回函数内静态缺省样例 —— 比例 `622×432`、经验量 `25 / 17 / 10 /
4`、屏高模式。

### 注解

该值会被 `SetShellClientSize` 改写为按实际客户区推导的版本 ⇒ 同一快照在不同时刻可能不同, 不要在客户区校正
之前缓存它。

缺省样例是共享只读静态件, 不得写入; 它与"某个快照的具体尺寸"无关。

---

## GetShellRefSize 函数 (PreviewSession)

取外壳中间态的参考尺寸与经验参数。

### 语法

```cpp
static CONST RefSize& GetShellRefSize();
```

### 参数

此函数无参数。

### 返回值

类型: **CONST RefSize&**

命中返回快照条目内的参考尺寸与经验参数(中间态原样, 不经像素解算); 无效时返回
`LayoutModel::DefaultRefSize()` 的静态副本。

### 注解

与 `GetShellRef` 的分工: 前者是解算结果(像素), 本函数是解算输入(模式与经验量)。二者不可互推 ——
本函数的返回值不随客户区校正变化, 前者会变。

需要按同一块模式与经验量二次换算(例如另算一份几何)时用本函数, 需要现成像素时用 `GetShellRef`。

---

## GetShellEntry 函数 (PreviewSession)

取外壳快照条目指针。

### 语法

```cpp
static LayoutEntry* GetShellEntry();
```

### 参数

此函数无参数。

### 返回值

类型: **LayoutEntry\***

快照条目指针; 未载入(或已释放)返回 `NULL`。

### 注解

返回条目本体而非副本: 写它等于改快照状态, 调用方按只读投影使用, 不要写字段。

与 `GetPreviewBase` 同族: 供条目族反查判定"该布局是否为外壳快照"(比较条目的布局字段), 也用于取条目级字段
(名称 / 标签 / 参考尺寸 / 中间态节点 / 窗体区位置)。窗口矩形另有 `GetShellWndRect`, 不必自己拼。

条目内存由会话分配器持有 ⇒ 释放后不再归还池, 但指针即失效 ⇒ 不得跨 `ResetShellSnapshot` 与下一次装载持有。

---

## GetShellWndRect 函数 (PreviewSession)

取外壳快照布局的窗口矩形。

### 语法

```cpp
static BOOL GetShellWndRect(
  [out] LONG*  x,
  [out] LONG*  y,
  [out] DWORD* w,
  [out] DWORD* h
);
```

### 参数

`[out] x`

类型: **LONG\***

窗口位置的写回处(窗体区声明的位置)。可为 `NULL`(不写)。

`[out] y`

类型: **LONG\***

窗口位置的写回处。可为 `NULL`。

`[out] w`

类型: **DWORD\***

窗口宽度写回处, 尺寸 = 该布局 autosize 推导。可为 `NULL`。

`[out] h`

类型: **DWORD\***

窗口高度写回处。可为 `NULL`。

### 返回值

类型: **BOOL**

`TRUE` = 四个出参已按需写回; `FALSE` = 快照无效, 此时四个出参一律不动(不留脏值、不清零)。

### 注解

宿主据此创建编辑窗口 ⇒ 它是建窗前唯一的尺寸来源; 建窗后由 `SetShellClientSize` 按实际客户区校正。

四个出参逐个判空 ⇒ 调用方可只取需要的字段(典型是只取宽高)。返回值与"写了几个字段"无关: 有效即 `TRUE`。

---

## SetShellClientSize 函数 (PreviewSession)

建窗后以实际客户区幂等校正外壳参考尺寸并重算外壳几何。

### 语法

```cpp
static VOID SetShellClientSize(
  [in] LONG width,
  [in] LONG height
);
```

### 参数

`[in] width`

类型: **LONG**

实际客户区宽度(像素); 小于 16 按 16 处理。

`[in] height`

类型: **LONG**

实际客户区高度(像素); 小于 16 按 16 处理。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

建窗后调用一次: 以实际客户区重建外壳参考尺寸, 并重算外壳布局全部控件的几何。

无边框弹出窗的口径下, 实际客户区通常等于建窗尺寸(等于 autosize 推导值)⇒ 本步为幂等校正; 系统夹取或 DPI
缩放使二者不等时以实际客户区为准。

换算基准(模式索引与像素基准)取自快照的参考尺寸参数, 基准下限 1。快照条目或其布局为空时静默返回(不校正)。

客户区尺寸不参与 `GetShellRefSize`(它是解算输入), 只影响 `GetShellRef` 与几何。

---

## ResetShellSnapshot 函数 (PreviewSession)

复位外壳快照的全部状态。

### 语法

```cpp
static VOID ResetShellSnapshot();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

顺序固定: 回收快照布局的控件缓冲(重载时不遗留池块)→ 断开条目的布局引用 → 条目指针置空 → 复位内嵌成员基座
(注入 `NULL`)→ 释放全部内嵌成员(它们与外壳快照同生命周期)→ 清有效位。

由应用释放时原位调用, 且在同一释放序列里先于承载者复位(复位承载者之后就取不到快照条目了); 内部与私有释放
件同义(单一实现口径), 不另设平行路径。未就绪时早返(不触碰状态), 幂等。

复位后: `GetShellValid` 为 `FALSE`, `GetShellEntry` 为 `NULL`, `GetShellLayout` 给空对象; 内嵌成员同时被
释放 ⇒ 活动集合为空。

---

## PreviewSession 类 (PreviewSession)

预览基座族与外壳快照族的唯一入口。

### 语法

```cpp
class PreviewSession
{
public:
  PreviewSession() = delete;
  ~PreviewSession() = delete;

  static VOID SetSession(Document* pDocument, LONG* pnRev, Arena* pArenaHub);
  static Layout* GetPreviewBase();
  static VOID ResetPreviewBase();
  static Layout* EnsurePreviewBase();
  static VOID FreePreviewBase();
  static Layout* EnsurePreviewBaseHook();
  static VOID BumpDocRevHook();
  static BOOL SyncPreviewLayout();
  static BOOL RequestPreviewSync();
  static VOID OnPreviewSyncRequest(VOID* obj, uint64_t oper, uint64_t param, void* data);
  static VOID RecalcEntryGeometry(DWORD index);

  static BOOL LoadShellSnapshot(LPCSTR path);
  static BOOL LoadShellSnapshotFromMemory(LPVOID pData, DWORD cbData);
  static BOOL GetShellValid();
  static Layout& GetShellLayout();
  static CONST AutoSize& GetShellRef();
  static CONST RefSize& GetShellRefSize();
  static LayoutEntry* GetShellEntry();
  static BOOL GetShellWndRect(LONG* x, LONG* y, DWORD* w, DWORD* h);
  static VOID SetShellClientSize(LONG width, LONG height);
  static VOID ResetShellSnapshot();
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `PreviewSession()` | 构造私有且删除: 类不可实例化。 |
| `~PreviewSession()` | 析构私有且删除: 类不可实例化。 |
| `SetSession` | 注入三个借用句柄(承载者 / 版本号地址 / 分配器)。 |
| `GetPreviewBase` | 读当前基座指针。 |
| `ResetPreviewBase` | 只丢基座指针。 |
| `EnsurePreviewBase` | 建立或取用基座并重指全部条目。 |
| `FreePreviewBase` | 析构基座并断开条目引用。 |
| `EnsurePreviewBaseHook` | 回调形态的基座取用。 |
| `BumpDocRevHook` | 回调形态的版本号递增。 |
| `SyncPreviewLayout` | 按中间态增量更新基座内容。 |
| `RequestPreviewSync` | 帧内挂队列 / 排空中当场同步。 |
| `OnPreviewSyncRequest` | 排空处理器, 只转同步。 |
| `RecalcEntryGeometry` | 重建条目几何, active 时同步基座。 |
| `LoadShellSnapshot` | 文件入口装载快照。 |
| `LoadShellSnapshotFromMemory` | 内存入口装载快照。 |
| `GetShellValid` | 快照四合一有效判据。 |
| `GetShellLayout` | 外壳渲染源引用。 |
| `GetShellRef` | 外壳参考像素。 |
| `GetShellRefSize` | 外壳参考尺寸与经验参数。 |
| `GetShellEntry` | 外壳条目指针。 |
| `GetShellWndRect` | 外壳窗口矩形。 |
| `SetShellClientSize` | 按实际客户区校正几何。 |
| `ResetShellSnapshot` | 复位快照与内嵌成员。 |

### 注解

全静态类, 无实例、无虚函数。状态是六枚静态成员: 预览族四枚(基座 / 承载者句柄 / 版本号地址 / 分配器句柄)与
外壳族两枚(快照条目 / 有效位), 静态初始化即"未载入"档(空指针与 `FALSE`)。

生命周期: 两族都随应用会话建立与复位, 都与被编辑文档的切换或新建无关 —— 外壳快照来源恒为启动载入的那一份
(保留名 `GENUI_SHELL_LAYOUT_NAME` 定义在 `app\Application.h`), 预览基座内容则随 active 条目变化。两族共用
同一族装配件(条目几何族在 `app\Application` 的公开接口, 旁挂表在 `doc\RawAttributes`)与同一份注入句柄。

调用顺序: `SetSession` 之后分两条独立链 —— 预览链(建立 → 同步 / 重算 → 释放)与外壳链(装载 → 查询 →
建窗校正 → 复位)。外壳链的装载入口在构建成功后原位注入内嵌成员基座并装配内嵌成员, 因此外壳的装载与复位必须
与内嵌成员的装配与释放成对(复位入口已内含这一步)。

所有权: 本模块不拥有承载者、版本号、分配器、解析器与任何布局对象的存储; 基座与快照对象由会话分配器与小对象
池持有, 本模块只持有指针并负责调用析构。字符串与条目内存一律随池释放, 本模块不单独归还。

就绪: 预览族看承载者句柄, 外壳族看分配器句柄加会话分配器本身 —— 两个就绪面不同, 不要用同一个判据覆盖两族
(见 `SetSession` 注解)。

线程: 全部接口无内部同步, 静态状态共享。同步请求虽可在渲染帧内挂载, 但建立、释放、装载与查询必须在同一线程
串行(通常即界面线程)。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `app/PreviewSession.h`(`src\app\PreviewSession.h`) |
| 实现 | `src\app\PreviewSession.cpp` |
| 命名空间 | 全局(类 `PreviewSession`) |
| 依赖 | 工程内: `app\Application`(条目几何族与保留名)、`app\DocumentIO`(解析器工厂)、`app\FlexHost`(内嵌成员装配 / 释放 / 基座注入)、`app\HostContext`(预览点击处理器)、`doc\Document`(承载者与条目)、`doc\RawAttributes`(旁挂表)、`framework\base\mem\AllocatorHub`(条目分配 / 串池化 / 缓冲回收)、`framework\layout\Layout`、`framework\layout\LayoutModel`(中间态与缺省参考尺寸)、`framework\layout\LayoutBuilder`(中间态 → 展开态 / 控件装配 / 整枚布局缓冲回收)、`framework\layout\LayoutSolver`、`framework\base\CtrlBuffer`(控件缓冲: 数据结构 / 取址 / 构造与回收)、`framework\base\ControlSpec`(类型身份表)、`framework\layout\GeometrySolver`(像素几何解算)、`framework\dispatch\FrameEvent`(帧间队列)、`framework\base\BasicDesign`; 系统: `kernel32`(串比较)、CRT(`printf`) |
| 平台 | Windows; 编辑窗口按无边框弹出窗口口径建窗(客户区与建窗尺寸一致), 不适用时以实际客户区校正 |
| 线程安全 | 否(六枚状态全为静态成员, 无内部同步; 建立 / 装配 / 释放 / 查询须同线程串行) |
