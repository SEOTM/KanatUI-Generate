# Render 模块 (src\framework\render\Render.h)

## 概述

渲染能力接口:一个文件四部分 —— 渲染设施能力表(`RenderApi`, 16 项)+ 控件绘制接口表(`CtrlApi`, 3 项)+
宿主接口表(`HostApi`, 16 项)+ 样式与状态定义(`StyleDesc` / `WndShowState` / `pfnFrame` / `pfnSegment`),
外加窗口状态位常量。

本模块只定义**接口形式**——能力类型(`using` 定义的函数指针类型)、状态位常量、能力表结构体,
不含任何具体 UI 库的类型、头文件与实现。能力由实现件填表提供, 由宿主在初始化期注入,
消费件只经本表调用 ⇒ 更换 UI 实现时改为注入另一张表, 消费件零改动。

接口形式是"能力类型 + 成员全为该类型的结构体", 不用抽象基类(本工程在无外部库要求时不引入虚函数与
RTTI)。表中的几何量一律为框架自有语汇(像素坐标 `vec2`), 由实现件换算为目标库的类型;
失败口径与框架其余部分一致:静默返回码(`FALSE`), 不弹窗、不打印。

本模块包含以下编程接口:

### 常量

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `RENDER_WND_BASE` | `0x00000001` | 基座窗口标准状态:无标题栏 / 禁缩放 / 禁移动 / 禁折叠 / 焦点不置顶 / 无滚动条 / 滚轮无效。 |
| `RENDER_WND_NOINPUT` | `0x00000002` | 窗口级无输入(严格模态:本窗口内控件与悬停一律不响应)。 |

### 类型

| 类型 | 说明 |
| --- | --- |
| `pfnAcquireWnd` | 取得本次渲染目标窗口的能力。 |
| `pfnOpenWnd` | 开启一个窗口(基座窗口与内嵌窗口同走本能力)的能力。 |
| `pfnCloseWnd` | 结束当前窗口的能力。 |
| `pfnBringWndToFront` | 把最近开启的窗口置于绘制顺序最前的能力。 |
| `pfnOpenPanel` | 在当前窗口内开启内容容器的能力。 |
| `pfnClosePanel` | 结束当前内容容器的能力。 |
| `pfnSetCursor` | 定位绘制光标的能力。 |
| `pfnGetContentOrigin` | 取当前容器内容原点的能力。 |
| `pfnPushId` | 压入整数 ID 命名空间的能力。 |
| `pfnPushPtrId` | 压入指针 ID 命名空间的能力。 |
| `pfnPopId` | 弹出最近压入的 ID 命名空间的能力。 |
| `pfnTakeItemId` | 取当前窗口末项 ID 的能力。 |
| `pfnAddHitArea` | 补一枚无绘制命中项的能力。 |
| `pfnIsHitClicked` | 判定最近提交项本帧是否被左键点击的能力。 |
| `pfnIsRectClicked` | 判定指定矩形内本帧是否左键按下的能力。 |
| `pfnGetDrawVtxCount` | 取当前窗口绘制顶点数的能力。 |
| `pfnUpdateCtrlContext` | 更新控件绘制上下文的能力(参考尺寸 / 投影配色 / 窗口位置记录槽 / 内容容器原点)。 |
| `pfnDrawCtrl` | 逐控件绘制的能力(按控件类型在实现内部分派)。 |
| `pfnHasActiveInput` | 查询"是否存在编辑中的输入控件"的能力。 |
| `pfnFrame` | 每帧处理流程的函数指针类型(宿主主循环每帧调用一次)。 |
| `pfnSegment` | 段前回调的函数指针类型(后端开帧之后、UI 库帧开始之前调用)。 |

### 枚举

| 枚举 | 说明 |
| --- | --- |
| `WndShowState` | 宿主按标准窗口序号报告的显示状态(四态)。 |

### 结构

| 结构 | 说明 |
| --- | --- |
| `RenderApi` | 渲染能力表:一次布局渲染所需的全部能力, 成员顺序即典型使用顺序。 |
| `CtrlApi` | 控件绘制接口表:布局渲染出口逐帧所需的控件库能力(3 项)。 |
| `HostApi` | 宿主接口表:调度层所需的宿主能力(窗口生命周期 / 帧循环 / 样式 / 可见性 / 窗口矩形 / 自证统计)。 |
| `StyleDesc` | 宿主样式描述:圆角口径(4 项)与配色槽(17 项), 由调度层计算、由宿主映射到具体 UI 库。 |

---

## RENDER_WND_BASE 常量 (Render)

基座窗口的标准状态位。

### 语法

```cpp
constexpr DWORD RENDER_WND_BASE = 0x00000001;
```

### 值

`0x00000001`。作 `OpenWnd` 的 `dwStates` 入参使用。

### 注解

本状态位是"普通内容窗口"的基础行为:无标题栏、禁止用户缩放与移动、禁止折叠、窗口获得焦点时不置顶、
不显示滚动条、滚轮无效。窗口的移动与缩放因此只能由框架自己发起(标题栏拖动走业务事件)。

状态位是框架级语义, 由实现方翻译成目标 UI 库的窗口标志; 因此同一位在不同实现下的具体标志组合
可以不同, 但对外可见的行为应一致。与 `RENDER_WND_NOINPUT` 可位或叠加。

---

## RENDER_WND_NOINPUT 常量 (Render)

窗口级无输入状态位。

### 语法

```cpp
constexpr DWORD RENDER_WND_NOINPUT = 0x00000002;
```

### 值

`0x00000002`。作 `OpenWnd` 的 `dwStates` 入参使用。

### 注解

语义是严格模态:该窗口内的控件与悬停一律不响应, 窗口只承载绘制。用于"底图窗口""预览窗口"
一类只读展示面。

与 `RENDER_WND_BASE` 位或后一起传入即可; 单独使用没有意义(无输入窗口仍需要基础行为)。

---

## pfnAcquireWnd 类型 (Render)

取得本次渲染目标窗口的能力。

### 语法

```cpp
using pfnAcquireWnd = BOOL(*)(const vec2& pos, const vec2& size, LPCSTR szTitle);
```

### 参数

`[in] pos`

类型: **const vec2&**

期望的窗口左上角位置(桌面像素坐标)。这是**期望值**而非命令:与目标窗口实际几何不符时由实现方
自行调整(移动窗口), 两者已一致时不调窗。

`[in] size`

类型: **const vec2&**

期望的窗口客户区尺寸(像素)。同样按期望值结算; 尺寸非正视为参数非法(见返回值)。

`[in] szTitle`

类型: **LPCSTR**

期望的窗口标题(UTF-8)。实现方在该窗口开帧之前比较并更新标题; 为 `NULL` 表示不改标题。标题只影响
任务栏与窗口枚举(基座窗口无系统标题栏)。

### 返回值

类型: **BOOL**

`TRUE` = 目标窗口已就绪且已成为当前绘制目标; `FALSE` = 不得继续本次渲染。

### 注解

**调用时机**:基座布局的渲染路径在 `OpenWnd` 之前调用一次; 内嵌(柔性)布局不调用本能力
(内嵌窗口没有独立句柄, 沿用当前窗口)。

**位置来源**:位置来自布局的窗口位置记录(基座布局即窗口控件的 `position`:声明期给出初值,
运行期由窗口级交互改写)。实现方不解释它的来源, 只按值结算。

**标题来源**:标题来自基座布局窗口控件的 `label`; 实现方只在标题变化时更新窗口标题, 标题不变时不调窗。

**几何同步**:实现方在调整窗口时负责把客户区尺寸与交换链、窗口圆角等一并同步, 使后续绘制坐标与
实际像素对齐。

**失败口径**:返回 `FALSE` 表示实现方无法给出目标窗口(未就绪、窗口被遮挡)或参数非法(尺寸非正);
此时调用方不得继续渲染, 也不得再调 `OpenWnd`。失败是静默的, 不弹窗不打印。

---

## pfnOpenWnd 类型 (Render)

开启一个窗口的能力。

### 语法

```cpp
using pfnOpenWnd = BOOL(*)(LPCSTR szName, const vec2& pos, const vec2& size, DWORD dwStates);
```

### 参数

`[in] szName`

类型: **LPCSTR**

窗口名, 用于目标库的窗口标识。同一帧内必须唯一; 内嵌窗口由调用方以"标签 + 布局地址"的方式唯一化。

`[in] pos`

类型: **const vec2&**

窗口左上角位置(像素), 相对宿主客户区。

`[in] size`

类型: **const vec2&**

窗口尺寸(像素)。

`[in] dwStates`

类型: **DWORD**

窗口状态位, 取 `RENDER_WND_BASE` / `RENDER_WND_NOINPUT` 的位或组合; 传 `0` 表示无附加状态。

### 返回值

类型: **BOOL**

`TRUE` = 窗口已开启(随后必须配对调用 `CloseWnd`); `FALSE` = 开启失败(此时**不得**再调 `CloseWnd`)。

### 注解

基座窗口与无独立句柄的内嵌窗口同走本能力:开启后该窗口即成为当前绘制目标, 直到 `CloseWnd`。

成对约束是硬性的:`TRUE` 才配对关闭, `FALSE` 不配对——否则会破坏实现的窗口栈。
窗口名重复时行为由实现方决定(通常导致同名窗口状态串味), 调用方须自行保证唯一。

---

## pfnCloseWnd 类型 (Render)

结束当前窗口的能力。

### 语法

```cpp
using pfnCloseWnd = VOID(*)();
```

### 参数

此能力无参数。

### 返回值

类型: **VOID**

此能力不返回值。

### 注解

与 `OpenWnd` 成对:只在 `OpenWnd` 返回 `TRUE` 之后调用一次。关闭后当前绘制目标回到上一层
(通常是宿主窗口), 期间提交的绘制与命中项已归入该窗口。

---

## pfnBringWndToFront 类型 (Render)

把最近开启的窗口置于绘制顺序最前的能力。

### 语法

```cpp
using pfnBringWndToFront = VOID(*)();
```

### 参数

此能力无参数。

### 返回值

类型: **VOID**

此能力不返回值。

### 注解

用于内嵌窗口需要绘制在其宿主之上时:在 `CloseWnd` 之后调用, 被提升的是"最近开启的那一个窗口"。
宿主的基座窗口不调用本能力; 多枚内嵌窗口按开启顺序逐枚调用, 即自下而上叠放。

---

## pfnOpenPanel 类型 (Render)

在当前窗口内开启一个内容容器的能力。

### 语法

```cpp
using pfnOpenPanel = BOOL(*)(LPCSTR szName, const vec2& pos, const vec2& size, DWORD crBack);
```

### 参数

`[in] szName`

类型: **LPCSTR**

容器名。在同一窗口内须唯一(调用方负责唯一化)。

`[in] pos`

类型: **const vec2&**

容器左上角位置(像素), 窗口本地坐标。

`[in] size`

类型: **const vec2&**

容器尺寸(像素)。

`[in] crBack`

类型: **DWORD**

容器背景色, 打包口径 `0xAABBGGRR`。传 `0x00000000` 表示全透明(由内层内容自绘底色)。

### 返回值

类型: **BOOL**

`TRUE` = 容器已开启(随后必须配对调用 `ClosePanel`); `FALSE` = 开启失败(不配对关闭)。

### 注解

容器是"承载内容控件的子区域":开启后它成为当前绘制目标, 容器内控件的声明坐标以容器本地坐标为准,
屏幕零点经 `GetContentOrigin` 取得。

背景色为全透明时不绘制底色, 但容器仍然开启(裁剪与坐标原点语义照常)。

---

## pfnClosePanel 类型 (Render)

结束当前内容容器的能力。

### 语法

```cpp
using pfnClosePanel = VOID(*)();
```

### 参数

此能力无参数。

### 返回值

类型: **VOID**

此能力不返回值。

### 注解

与 `OpenPanel` 成对:只在 `OpenPanel` 返回 `TRUE` 之后调用一次。关闭后当前绘制目标回到所属窗口。

---

## pfnSetCursor 类型 (Render)

定位绘制光标的能力。

### 语法

```cpp
using pfnSetCursor = VOID(*)(const vec2& pos);
```

### 参数

`[in] pos`

类型: **const vec2&**

目标位置(像素), 当前窗口或容器的本地坐标。

### 返回值

类型: **VOID**

此能力不返回值。

### 注解

用于把下一个绘制项放到指定位置(逐控件绘制前先定位光标)。越界坐标不报错:是否裁剪由当前窗口或
容器的裁剪区决定。

---

## pfnGetContentOrigin 类型 (Render)

取当前容器内容原点的能力。

### 语法

```cpp
using pfnGetContentOrigin = vec2(*)();
```

### 参数

此能力无参数。

### 返回值

类型: **vec2**

当前容器的内容原点(屏幕坐标), 即"容器内控件声明坐标的零点"。

### 注解

仅供需要把容器本地坐标换算成屏幕坐标的场合(如矩形命中判定要求屏幕坐标时的换算)。
无容器开启时返回当前窗口的内容原点。

---

## pfnPushId 类型 (Render)

压入整数 ID 命名空间的能力。

### 语法

```cpp
using pfnPushId = VOID(*)(INT32 nId);
```

### 参数

`[in] nId`

类型: **INT32**

命名空间键, 通常取控件 `id`。同帧内相同键的控件会共享命名空间。

### 返回值

类型: **VOID**

此能力不返回值。

### 注解

逐控件绘制前压入控件 id, 使不同容器下同名控件互不冲突; 压栈后必须由 `PopId` 配对弹出。
命名空间只影响目标库的项标识, 不影响绘制坐标。

---

## pfnPushPtrId 类型 (Render)

压入指针 ID 命名空间的能力。

### 语法

```cpp
using pfnPushPtrId = VOID(*)(const VOID* pKey);
```

### 参数

`[in] pKey`

类型: **const VOID\***

命名空间键, 通常取布局对象地址。调用方保证该地址在窗口存续期内唯一且稳定(不得指向会搬家的临时对象)。

### 返回值

类型: **VOID**

此能力不返回值。

### 注解

用于内嵌窗口按布局对象地址隔离:多枚内嵌窗口的同名控件因此互不冲突。压栈后必须由 `PopId` 配对弹出。

键的稳定性是硬性要求:同一地址在窗口存续期内复用会与旧项串味, 地址被释放后复用于另一对象
则会误命中。

---

## pfnPopId 类型 (Render)

弹出最近压入的 ID 命名空间的能力。

### 语法

```cpp
using pfnPopId = VOID(*)();
```

### 参数

此能力无参数。

### 返回值

类型: **VOID**

此能力不返回值。

### 注解

与 `PushId` / `PushPtrId` 成对, 按后进先出弹出。多弹或少弹都会让后续控件的项标识错位
(表现为交互落到别的控件上), 因此压栈与弹栈必须严格配对。

---

## pfnTakeItemId 类型 (Render)

取当前窗口末项 ID 的能力。

### 语法

```cpp
using pfnTakeItemId = uint64_t(*)();
```

### 参数

此能力无参数。

### 返回值

类型: **uint64_t**

当前窗口最近一次提交的可交互项 ID; 尚无末项时为 `0`。

### 注解

用法是"逐控件绘制前后各取一次":两次取值不同(或由 `0` 变为非 `0`)即说明该控件自行提交了项,
调用方据此决定是否还要补无绘制命中项。

返回的 ID 只用于"有没有变"的比较, 不承诺跨帧稳定, 也不得持久化。

---

## pfnAddHitArea 类型 (Render)

补一枚无绘制命中项的能力。

### 语法

```cpp
using pfnAddHitArea = VOID(*)(LPCSTR szLabel, const vec2& pos, const vec2& size);
```

### 参数

`[in] szLabel`

类型: **LPCSTR**

命中项名, 可为 `NULL`(实现方取兜底名)。名字只用于调试与目标库的项标识, 不参与命中判定。

`[in] pos`

类型: **const vec2&**

矩形左上角(像素), 当前窗口或容器的本地坐标。

`[in] size`

类型: **const vec2&**

矩形尺寸(像素)。尺寸非正时**不得**提交:部分 UI 库对零尺寸项会断言, 调用方须先行过滤。

### 返回值

类型: **VOID**

此能力不返回值。

### 注解

用途是给纯图元控件(自绘图形、无交互项)与未自行提交项的控件补一个可命中的矩形。

三条约束:

1. 补项不产生任何绘制, 只提交一个零像素的命中区。
2. 实现方须令补项可被其上(后绘制)的可交互项覆盖, 否则它会抢走上层控件的命中。
3. 调用后须以 `IsHitClicked` 判定, 或再补下一枚项——补项本身不返回命中结论。

调用顺序:在控件自身绘制之后补, 保证补项处于该控件的层级之上、覆盖其矩形。

---

## pfnIsHitClicked 类型 (Render)

判定最近提交项本帧是否被左键点击的能力。

### 语法

```cpp
using pfnIsHitClicked = BOOL(*)();
```

### 参数

此能力无参数。

### 返回值

类型: **BOOL**

`TRUE` = 最近提交的项命中并被左键点击; `FALSE` = 未点击。

### 注解

必须紧接在绘制该控件或 `AddHitArea` 之后调用:它判定的对象是"最近提交的项", 中间若插入了别的项,
结论会落到那一个项上。

---

## pfnIsRectClicked 类型 (Render)

判定指定矩形内本帧是否发生左键按下的能力。

### 语法

```cpp
using pfnIsRectClicked = BOOL(*)(const vec2& mn, const vec2& mx);
```

### 参数

`[in] mn`

类型: **const vec2&**

矩形左上角(像素), **屏幕坐标**(不是窗口本地坐标)。

`[in] mx`

类型: **const vec2&**

矩形右下角(像素), 屏幕坐标。`mx` 应不小于 `mn`, 否则矩形为空、恒不命中。

### 返回值

类型: **BOOL**

`TRUE` = 鼠标落在矩形内且本帧左键按下; `FALSE` = 否则。

### 注解

这是给复合控件用的矩形兜底判定:当控件由多段组成、没有单一可命中项时, 用整块矩形直接问
"本帧是否在这里按下"。判定含当前窗口裁剪——落在窗口之外的部分不参与。

坐标口径是屏幕坐标; 容器本地坐标需求 `mn` / `mx` 时先经 `GetContentOrigin` 换算。

---

## pfnGetDrawVtxCount 类型 (Render)

取当前窗口绘制顶点数的能力。

### 语法

```cpp
using pfnGetDrawVtxCount = INT64(*)();
```

### 参数

此能力无参数。

### 返回值

类型: **INT64**

当前窗口累计的绘制顶点数。

### 注解

可在 `AddHitArea` 前后各取一次, 两次相等即说明补项没有产生绘制。
不作性能统计用, 也不保证跨帧单调递增(窗口关闭后归零)。

---

## RenderApi 结构 (Render)

渲染能力表:一次布局渲染所需的全部 UI 设施能力。

### 语法

```cpp
struct RenderApi
{
    pfnAcquireWnd      AcquireWnd;
    pfnOpenWnd         OpenWnd;
    pfnCloseWnd        CloseWnd;
    pfnBringWndToFront BringWndToFront;
    pfnOpenPanel       OpenPanel;
    pfnClosePanel      ClosePanel;
    pfnSetCursor       SetCursor;
    pfnGetContentOrigin GetContentOrigin;
    pfnPushId          PushId;
    pfnPushPtrId       PushPtrId;
    pfnPopId           PopId;
    pfnTakeItemId      TakeItemId;
    pfnAddHitArea      AddHitArea;
    pfnIsHitClicked    IsHitClicked;
    pfnIsRectClicked   IsRectClicked;
    pfnGetDrawVtxCount GetDrawVtxCount;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `AcquireWnd` | 取得本次渲染的目标窗口并使之成为当前绘制目标; 期望标题经参数给出。 |
| `OpenWnd` | 开启一个窗口。 |
| `CloseWnd` | 结束当前窗口。 |
| `BringWndToFront` | 把最近开启的窗口置于绘制顺序最前。 |
| `OpenPanel` | 在当前窗口内开启内容容器。 |
| `ClosePanel` | 结束当前内容容器。 |
| `SetCursor` | 定位绘制光标。 |
| `GetContentOrigin` | 取当前容器的内容原点(屏幕坐标)。 |
| `PushId` | 压入整数 ID 命名空间。 |
| `PushPtrId` | 压入指针 ID 命名空间。 |
| `PopId` | 弹出最近压入的 ID 命名空间。 |
| `TakeItemId` | 取当前窗口末项 ID。 |
| `AddHitArea` | 补一枚无绘制命中项。 |
| `IsHitClicked` | 判定最近提交项本帧是否被左键点击。 |
| `IsRectClicked` | 判定指定矩形内本帧是否左键按下。 |
| `GetDrawVtxCount` | 取当前窗口绘制顶点数。 |

### 注解

**所有权与生命周期**:整表由实现方一次性填好(进程级静态), 消费方只持**只读指针**, 不复制、不改造、
不释放。因此表的存活期覆盖所有渲染调用; 换实现等于把指针换成另一张表, 消费件零改动。

**成员顺序即典型使用顺序**:取得窗口 → 开窗 → 开容器 → 定位光标 → 压 ID 命名空间 → 逐控件绘制与
命中判定 → 弹命名空间 → 关容器 → 关窗口。顺序被打破时命中判定与裁剪会落到错误的层级上。

**几何语汇**:入参一律是框架自有语汇(像素坐标 `vec2`), 由实现件换算成目标 UI 库的类型;
消费件不接触任何目标库类型, 也不做单位换算。

**失败口径**:返回 `BOOL` 的能力以 `FALSE` 表示失败, 静默不报错; 无返回值的能力不提供失败回传,
调用方须按前序返回值(如 `OpenWnd` / `OpenPanel` 的结论)决定是否配对关闭。

**调用时机**:一律在宿主已开启一帧之后(实现方的"当前上下文"之内)调用; 帧外调用属调用方违约,
实现方不保证行为。

**缺项**:成员是函数指针, 表内某项未被填(空指针)时调用即崩溃——填充完整是注入方的责任, 本模块
不提供默认实现与空指针兜底。


---

## pfnUpdateCtrlContext 类型 (Render)

更新控件绘制上下文能力的函数指针类型。

### 语法

```cpp
using pfnUpdateCtrlContext = BOOL(*)(CONST AutoSize& refsize, CONST ProjCR* pProj,
	CONST vec2* pWndPos, CONST vec2& orgPanel);
```

### 参数

`[in] refsize`

类型: **CONST AutoSize&**

参考尺寸(控件几何解算与绘制取值的基准)。

`[in] pProj`

类型: **CONST ProjCR\***

投影配色。传 `NULL` 表示配色未就绪, 实现方返回 `FALSE`。

`[in] pWndPos`

类型: **CONST vec2\***

窗口位置记录槽。传 `NULL` 表示本帧无窗口位置记录(窗口级交互按无记录处理)。

`[in] orgPanel`

类型: **CONST vec2&**

内容容器原点(容器本地坐标的零点)。

### 返回值

类型: **BOOL**

`TRUE` = 上下文已设定; `FALSE` = 必要输入缺失(如投影配色为空), 本帧不绘制控件。

### 注解

每次渲染前调用一次(每帧单一写点)。实现方为控件库(见 `framework/render/imgui/control/Controls.h`),
消费方为布局渲染出口(见 `framework/layout/LayoutRender.h`)。

---

## pfnDrawCtrl 类型 (Render)

逐控件绘制能力的函数指针类型。

### 语法

```cpp
using pfnDrawCtrl = BOOL(*)(CtrlInfo* ctrl);
```

### 参数

`[in, out] ctrl`

类型: **CtrlInfo\***

待绘制的控件信息。实现方按其类型分派绘制实现, 并可能写回控件缓冲与操作码; 传 `NULL` 时返回 `FALSE`。

### 返回值

类型: **BOOL**

`TRUE` = 已绘制(未知类型按矩形占位绘制, 也返回 `TRUE`); `FALSE` = 无绘制实现或入参为空。

### 注解

控件类型的分派(含 `TITLE`, 不再单列标题栏能力)与控件自身事件的派发都在实现方内部完成; 消费方只负责按控件顺序
调用本能力。控件坐标的容器本地换算由实现方自行处理。

---

## pfnHasActiveInput 类型 (Render)

查询"是否存在编辑中的输入控件"能力的函数指针类型。

### 语法

```cpp
using pfnHasActiveInput = BOOL(*)();
```

### 参数

无参数。

### 返回值

类型: **BOOL**

`TRUE` = 当前存在处于编辑中(项 active)的输入控件; `FALSE` = 不存在。

### 注解

供宿主按帧决定 IME 覆盖(存在编辑中的输入控件时启用输入法)。无参、无副作用, 可在帧内重复调用。

---

## WndShowState 枚举 (Render)

宿主按"标准窗口序号"报告的显示状态。

### 语法

```cpp
enum WndShowState
{
	WND_SHOW_NEVER = 0,
	WND_SHOW_VISIBLE,
	WND_SHOW_HIDDEN_BY_ACTIVE,
	WND_SHOW_HIDDEN_BY_HOST,
};
```

### 成员

| 成员 | 值 | 说明 |
| --- | --- | --- |
| `WND_SHOW_NEVER` | `0` | 建窗后从未显示。 |
| `WND_SHOW_VISIBLE` | `1` | 当前可见。 |
| `WND_SHOW_HIDDEN_BY_ACTIVE` | `2` | 当前不可见, 隐藏由"激活窗口数不足"策略发起(实现方可自行恢复)。 |
| `WND_SHOW_HIDDEN_BY_HOST` | `3` | 当前不可见, 隐藏由宿主发起(实现方不擅自恢复)。 |

### 注解

状态由具体宿主维护(写点唯一 = 其窗口过程的显示 / 隐藏分支), 调度层只经 `HostApi::GetWndShowState` 读取。
序号越界按 `WND_SHOW_NEVER` 处理。

---

## pfnFrame 类型 (Render)

每帧处理流程的函数指针类型。

### 语法

```cpp
using pfnFrame = VOID(*)(VOID* pUser);
```

### 参数

`[in] pUser`

类型: **VOID\***

调用方自带的用户指针, 原样回传(本工程传 `NULL`)。

### 返回值

此类型不返回值。

### 注解

宿主主循环每帧调用一次; 帧内的绘制、自证与退出判据都由实现方(调度层)负责, 宿主不介入其内容。

---

## pfnSegment 类型 (Render)

段前回调的函数指针类型。

### 语法

```cpp
using pfnSegment = VOID(*)(DWORD index, VOID* pUser);
```

### 参数

`[in] index`

类型: **DWORD**

当前段(标准窗口)序号。

`[in] pUser`

类型: **VOID\***

调用方自带的用户指针, 原样回传。

### 返回值

此类型不返回值。

### 注解

调用时机 = 目标窗口后端开帧之后、UI 库帧开始之前(供合成输入事件一类的段前动作入队); 本工程经
`HostApi::Start` 注入 `Dispatch::OnSegment` 作范例挂载点。

---

## StyleDesc 结构 (Render)

宿主样式描述:圆角口径与配色槽。

### 语法

```cpp
struct StyleDesc
{
	FLOAT fWindowRounding;
	FLOAT fFrameRounding;
	FLOAT fChildRounding;
	FLOAT fPopupRounding;
	DWORD crWindowBg, crChildBg, crFrameBg, crFrameBgHovered, crFrameBgActive;
	DWORD crButton, crButtonHovered, crButtonActive, crBorder, crText, crTextDisabled;
	DWORD crSliderGrab, crSliderGrabActive, crCheckMark, crHeader, crHeaderHovered, crHeaderActive;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `fWindowRounding` | 窗口圆角(根窗口 = 设计间隔)。 |
| `fFrameRounding` | 帧类件圆角(设计内边距)。 |
| `fChildRounding` | 内容子窗圆角(同上)。 |
| `fPopupRounding` | 弹出层圆角(同上)。 |
| `crWindowBg` | 窗口底。 |
| `crChildBg` | 内容子窗底。 |
| `crFrameBg` | 帧类件底(输入框 / 按钮 / 表头)。 |
| `crFrameBgHovered` | 帧类件底(悬停)。 |
| `crFrameBgActive` | 帧类件底(激活)。 |
| `crButton` | 按钮底。 |
| `crButtonHovered` | 按钮底(悬停)。 |
| `crButtonActive` | 按钮底(激活)。 |
| `crBorder` | 边框。 |
| `crText` | 文本。 |
| `crTextDisabled` | 文本(禁用 / 次级)。 |
| `crSliderGrab` | 滑块把手。 |
| `crSliderGrabActive` | 滑块把手(激活)。 |
| `crCheckMark` | 勾选标记。 |
| `crHeader` | 表头底。 |
| `crHeaderHovered` | 表头底(悬停)。 |
| `crHeaderActive` | 表头底(激活)。 |

### 注解

色值口径 = GDI 打包值(`0x00BBGGRR`, 低字节 = R), 与 `BasicDesign.h` 的取色正门同源, 由调度层填好;
宿主 `ApplyStyle` 负责把它映射到具体 UI 库的样式槽(含 `ImGuiCol_*`)。圆角取值由调度层计算(根窗口 = 设计间隔,
其余 = 设计内边距)。本结构只含框架自有语汇, 不含任何 UI 库类型。

---

## CtrlApi 结构 (Render)

控件绘制接口表:布局渲染出口逐帧所需的控件库能力。

### 语法

```cpp
struct CtrlApi
{
	pfnUpdateCtrlContext UpdateContext;
	pfnDrawCtrl          DrawCtrl;
	pfnHasActiveInput    HasActiveInput;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `UpdateContext` | 每次渲染前设一次控件上下文。 |
| `DrawCtrl` | 按控件类型内部分派绘制一枚控件(含 `TITLE`)。 |
| `HasActiveInput` | 是否存在编辑中的输入控件(供 IME 开关判定)。 |

### 注解

整表由控件库一次性填好(进程级静态, 取值入口 = `Controls::GetCtrlApi()`), 消费方持只读指针; 由宿主在 UI 设施
初始化期注入布局渲染出口(`LayoutRender::SetCtrlApi`), 未注入时渲染入口静默失败(与 `RenderApi` 同口径)。

---

## HostApi 结构 (Render)

宿主接口表:调度层所需的宿主能力。

### 语法

```cpp
struct HostApi
{
	VOID    (*EnableDpiAwareness)();
	BOOL    (*Start)(DWORD dwWndCount, LPCSTR szAppName, pfnSegment pfn, VOID* pUser);
	VOID    (*Run)(pfnFrame pfnFrame, VOID* pUser, CONST BOOL* pbExit);
	VOID    (*Release)();
	VOID    (*DestroyWnds)();
	VOID    (*SetDarkMode)(BOOL bDark);
	BOOL    (*ApplyStyle)(CONST StyleDesc& desc);
	VOID    (*SetIMEDisabled)(BOOL bDisabled);
	DWORD   (*GetStandardWndCount)();
	WndShowState (*GetWndShowState)(DWORD index);
	BOOL    (*GetWndRect)(DWORD index, RECT* prcWnd, RECT* prcClient);
	VOID    (*GetRenderStats)(DWORD* pdwSegments, DWORD* pdwCmdLists, INT64* pllVtx, INT64* pllIdx);
	VOID    (*GetResizeStats)(DWORD* pdwCount, DWORD* pdwLastW, DWORD* pdwLastH);
	VOID    (*SetAppIcon)(VOID* hIcon);
	VOID*   (*GetNativeDevice)();
	VOID*   (*GetNativeDeviceContext)();
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `EnableDpiAwareness()` | 打开进程级 DPI 感知(启动首件事; 幂等)。 |
| `Start(dwWndCount, szAppName, pfn, pUser)` | 启动宿主(建窗 → 建立 UI 设施), 并以段前回调挂载调度层的挂载点。 |
| `Run(pfnFrame, pUser, pbExit)` | 运行主消息循环(每帧调一次调度层流程; 退出判据只读其地址)。 |
| `Release()` | 释放全部窗口的 UI 面与图形设施(幂等, 不动窗口)。 |
| `DestroyWnds()` | 销毁全部标准窗口并注销窗口类(幂等)。 |
| `SetDarkMode(bDark)` | 登记亮暗基础样式(已就绪窗口立即刷新)。 |
| `ApplyStyle(desc)` | 登记宿主样式(圆角与配色); `FALSE` 只表示当前没有就绪窗口。 |
| `SetIMEDisabled(bDisabled)` | 设置本帧的 IME 覆盖(段结算时应用, 结算后失效)。 |
| `GetStandardWndCount()` | 取已声明的标准窗口数量。 |
| `GetWndShowState(index)` | 取指定窗口的显示状态(序号越界按从未显示处理)。 |
| `GetWndRect(index, prcWnd, prcClient)` | 取窗口矩形与客户区矩形; 窗口未建时返回 `FALSE`。 |
| `GetRenderStats(...)` | 取绘制量自证计数(段数 / 绘制命令数 / 顶点数 / 索引数)。 |
| `GetResizeStats(...)` | 取窗口尺寸调整计数与最近一次调整后的客户区尺寸。 |
| `SetAppIcon(hIcon)` | 登记应用窗口图标(`HICON`; 建窗前调用)。 |
| `GetNativeDevice()` | 取首个标准窗口的原生图形设备(未就绪时返回 `NULL`; 类型擦除)。 |
| `GetNativeDeviceContext()` | 取首个标准窗口的原生图形设备上下文(未就绪时返回 `NULL`; 类型擦除)。 |

### 注解

整表由具体宿主一次性填好(进程级静态), 调度层持只读指针; 推荐经宿主的 `Register()` 取得(幂等)。`SetDarkMode` 与
`ApplyStyle` 的语义是**登记**: 宿主在建立每个窗口的 UI 面时应用, 已就绪窗口立即刷新。`GetWndRect` 取代调度层直接
接触窗口句柄(几何自证只经本项取矩形)。未列出的高级用法仍可直调具体宿主的静态接口 —— 本表只是其函数指针化投影。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/render/Render.h`(`src\framework\render\Render.h`) |
| 实现 | 无(纯头文件: 能力类型 / 能力表 / 枚举 / 常量均在此定义) |
| 命名空间 | 全局作用域 |
| 依赖 | `minwindef.h` · `windef.h`(`RECT`) · `stdint.h` · `framework/base/ControlSpec.h`(`vec2` / `CtrlInfo`) · `framework/base/BasicDesign.h`(`AutoSize` / `ProjCR`) |
| 平台 | Windows 全版本 |
| 线程安全 | 否(操作当前绘制上下文; 只在渲染线程内调用) |
