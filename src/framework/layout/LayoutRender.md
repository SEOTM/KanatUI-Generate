# LayoutRender 模块 (src\framework\layout\LayoutRender.h)

## 概述

布局级渲染出口(对外调用面):基座窗口与内容容器的编排、逐控件向控件库的分派、逐控件的命中判定。
本模块不依赖任何具体 UI 库, 也不认识窗口句柄 —— 一切 UI 设施动作(开窗 / 开容器 / 压 ID / 定位 /
命中判定 / 置顶)只经注入的渲染能力表(`Render.h` 的 `RenderApi`)发起; "渲染到哪个窗口"由能力表的
`AcquireWnd` 决定。

能力表由宿主在 UI 设施初始化期注入(渲染设施表与控件绘制表在同一处注入: `SetRenderApi` / `SetCtrlApi`);
**未注入时各渲染入口静默失败**(返回 `FALSE`), 不弹窗、不打印。
本模块属 **framework 层**, 只做布局语义与编排: 控件自身的几何与外观、类型分派与缓冲管理归控件库
(`Controls` / `CtrlBuffer.h`)。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `SetRenderApi` | 注入渲染能力表(幂等; 传 `NULL` 清除注入)。 |
| `GetRenderApi` | 取当前注入的渲染能力表; 未注入时为 `NULL`。 |
| `SetCtrlApi` | 注入控件绘制接口表(幂等; 传 `NULL` 清除注入)。 |
| `GetCtrlApi` | 取当前注入的控件绘制接口表; 未注入时为 `NULL`。 |
| `RenderBase` | 渲染占一个标准窗口的基座布局; 可按严格模态关闭本窗口输入。 |
| `RenderPreviewBase` | 基座渲染之上为内容控件逐枚做点击判定, 并可插入视图叠加件。 |
| `RenderFlex` | 渲染无独立窗口句柄的内嵌(flex)布局, 沿用当前窗口。 |
| `IsIMEDisabled` | 查询本帧是否存在编辑中的输入控件(供宿主决定 IME 开关)。 |

本模块不定义公开类型与公开常量。

---

## SetRenderApi 函数 (LayoutRender)

注入渲染能力表。

### 语法

```cpp
VOID SetRenderApi(
    [in] CONST RenderApi* pApi
);
```

### 参数

`[in] pApi`

类型: **CONST RenderApi\***

能力表只读指针。表内存由调用方持有且须进程级常驻(本模块只存指针, 不复制); 传 `NULL` = 清除注入,
此后各渲染入口一律静默失败。注入时机须早于首次渲染调用。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

幂等: 重复注入以最后一次为准, 不做参数校验(函数指针表由宿主保证齐全)。

---

## GetRenderApi 函数 (LayoutRender)

取当前注入的渲染能力表。

### 语法

```cpp
CONST RenderApi* GetRenderApi();
```

### 参数

此函数无参数。

### 返回值

类型: **CONST RenderApi\***

当前注入的能力表指针; 未注入(或已清除)时为 `NULL`。返回指针归调用方所有, 本模块不释放。

### 注解

只读查询, 不改变注入状态; 供宿主自证与诊断读取。

---

## SetCtrlApi 函数 (LayoutRender)

注入控件绘制接口表。

### 语法

```cpp
VOID SetCtrlApi(
    [in] CONST CtrlApi* pApi
);
```

### 参数

`[in] pApi`

类型: **CONST CtrlApi\***

控件绘制接口表(3 项: `UpdateContext` / `DrawCtrl` / `HasActiveInput`)的只读指针。表内存由控件库持有
(取值入口 `Controls::GetCtrlApi()`)且须进程级常驻, 本模块只存指针、不复制; 传 `NULL` = 清除注入,
此后各渲染入口一律静默失败。注入时机须早于首次渲染调用(宿主在 UI 设施初始化期与 `SetRenderApi` 同处注入)。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

幂等: 重复注入以最后一次为准, 不做参数校验。注入后本模块的控件上下文、逐控件绘制与"是否存在编辑中输入控件"
三项查询全部改为表调用(不再直连控件库), 因此换控件实现只改注入方。

---

## GetCtrlApi 函数 (LayoutRender)

取当前注入的控件绘制接口表。

### 语法

```cpp
CONST CtrlApi* GetCtrlApi();
```

### 参数

此函数无参数。

### 返回值

类型: **CONST CtrlApi\***

当前注入的控件绘制接口表指针; 未注入(或已清除)时为 `NULL`。返回指针归提供方所有, 本模块不释放。

### 注解

只读查询; 供宿主自证与诊断读取(例如核对注入是否到位)。

---

## RenderBase 函数 (LayoutRender)

渲染占一个标准窗口的基座布局。

### 语法

```cpp
BOOL RenderBase(
    [in] Layout& layout,
    [in] CONST AutoSize& refsize,
    [in] CONST ProjCR* pProj,
    [in] BOOL bNoInputs = FALSE
);
```

### 参数

`[in] layout`

类型: **Layout&**

待渲染的布局对象。被判为无效(`IsInvalid`)或缺 `WND` 结构件时渲染失败, 不进入开窗流程。

`[in] refsize`

类型: **CONST AutoSize&**

该布局的参考尺寸, 作为控件库上下文的参考量来源。

`[in] pProj`

类型: **CONST ProjCR\***

投影配色(宿主持有), 作为控件库上下文一并送入; 层号映射归调用方。

`[in] bNoInputs`

类型: **BOOL**

`TRUE` = 本窗口不接收任何输入(严格模态); 默认 `FALSE`。不受内嵌(flex)成员影响。

### 返回值

类型: **BOOL**

`TRUE` = 本次渲染已完成; `FALSE` = 失败(未注入渲染能力 / 布局无效 / 缺 `WND` 结构件 / 取不到目标
窗口)。失败时不继续本次渲染的后续步骤。

### 注解

渲染目标窗口由能力表的 `AcquireWnd` 取得, 期望几何与标题取基座布局 `WND` 控件的记录值 —— 位置 = 窗口
位置记录(声明期初值, 运行期由窗口级交互改写), 尺寸 = 客户区尺寸, 标题 = 该控件的标签; 记录值与实际不符时
由能力表实现方主动调窗, 本模块不认识任何窗口句柄。

开窗后的编排顺序固定: 开窗口(`WND` 的标签作窗口名, 摆放位置恒为布局原点, 窗口状态 =
`RENDER_WND_BASE`) → 画标题栏(`TITLE` 结构件, 存在才画; 关闭钮恒绘制, 关闭与拖动是否生效由该控件
绑定的事件处理器决定) → 开内容容器(`CNTR` 结构件; 容器原点 = `(0, 容器件的 position.y)`, 底色透明) →
从索引 `GetShellNum()` 起逐枚渲染内容控件(压 ID、绘制、弹 ID)。结构件不参与内容控件循环。

**内容容器原点是一等输入**: 两枚结构件在 `AcquireWnd` 之前就先取到 —— `WND` 的几何决定渲染目标,
`CNTR` 的原点经 `Controls::UpdateContext` 注入控件库, 控件库据此把内容控件的有效坐标(窗口本地)换算成
容器本地坐标后再绘制。容器因此同时承担"裁剪区"与"坐标系原点"两个职责; 绘制侧只消费一次该偏移, 不会
叠加。命中判定(见 `RenderPreviewBase`)用同一对原点把控件矩形换算成屏幕坐标。

`bNoInputs` 只追加"窗口级无输入"状态位(`RENDER_WND_NOINPUT`), 其余窗口状态不变 ⇒ 本窗口内所有控件
与拖动/悬停一律不响应(鼠标穿透到更下层); **不变灰**, 不改任何颜色。本入口不自行判断"当前是否
处于提示展示期间", 时机一律由调用方以显式参数给出。

列表类控件的内容由控件库的绘制件按其自身缓冲绘制, 本入口只做逐控件分派。

---

## RenderPreviewBase 函数 (LayoutRender)

基础渲染之上, 为注入预览布局的内容控件逐枚做点击判定。

### 语法

```cpp
BOOL RenderPreviewBase(
    [in] Layout& layout,
    [in] CONST AutoSize& refsize,
    [in] CONST ProjCR* pProj,
    [in] pfnCtrlEvent pfnClick,
    [in] pfnOverlayBefore pfnOverlay = NULL
);
```

### 参数

`[in] layout`

类型: **Layout&**

待渲染的布局对象(预览布局)。判为无效或缺 `WND` 结构件时渲染失败。

`[in] refsize`

类型: **CONST AutoSize&**

该布局的参考尺寸。

`[in] pProj`

类型: **CONST ProjCR\***

投影配色(宿主持有; 层号映射归调用方)。

`[in] pfnClick`

类型: **pfnCtrlEvent**

逐控件点击记录器。为 `NULL` 时不做任何点击判定, 与 `RenderBase` 等价。

`[in] pfnOverlay`

类型: **pfnOverlayBefore**

视图叠加件绘制回调, 默认 `NULL`(无叠加件)。形态见 `FuncPtr.h` 的 `pfnOverlayBefore`。

### 返回值

类型: **BOOL**

`TRUE` = 本次渲染已完成; `FALSE` = 失败(口径同 `RenderBase`)。

### 注解

渲染主体与 `RenderBase` 完全一致(开窗 / 标题栏 / 内容容器 / 逐控件分派), 差别只有两处: 逐控件点击
判定, 以及可选的视图叠加件。记录器与叠加件都为空时与 `RenderBase` 等价, 不提交任何额外绘制项。

点击判定的类型分派(判定范围 = 内容控件, 结构件不参与):

- **控件库自身会派发点击的类型**(`BUTTON`, 以及绑定了事件的 `TEXT` / `RECT`, 判据见 `ControlSpec.h` 的
  `CtrlTypeHandlesClickItself`): 其事件处理器在装配点已被改绑为记录器, 点击由控件库派发 ⇒ 本入口
  跳过, 否则同一击会被登记两次。控件库的点击口径是完整单击(释放帧), 本入口对其余类型用的是
  按下帧判定, 故"跳过"是必要的去重手段。
- **纯图元类型**(`CtrlTypeIsRawGraphic`, 实际只剩 `UNKNOWN`): 先经 `AddHitArea` 在控件矩形上补一枚
  无绘制命中项, 再调 `IsHitClicked` 判定。命中矩形的坐标 = 控件有效坐标减去内容容器原点(容器本地),
  由能力表实现按容器内光标定位。
- **其余自带可交互项的类型**(输入 / 下拉 / 滑块 / 进度 / 列表): 绘制后直接调 `IsHitClicked` 判定其已
  提交的项; 若本帧未提交任何项(末项表 ID 未变)则补一枚兜底, 避免判定读到上一枚控件的项。
- **复合控件**(输入动作 / 下拉动作 / 列表, 判据见 `ControlSpec.h` 的 `CtrlTypeIsCompound`): UI 库的末项
  只指向其最后提交的那一枚子项(通常是右段按钮) ⇒ 点在其余子项区域(如左段输入区)时按上面第三条判
  不出来, 故再按控件整体矩形经 `IsRectClicked` 补一次命中判定。矩形 = 内容容器内容原点(屏幕坐标) +
  控件容器本地坐标; 该判定 = 矩形内 + 当前窗口裁剪 + 本帧左键按下, 与按下帧口径一致, 且不提交任何
  绘制项(顶点变化恒 `0`)。同一次点击仍只派发一次。

派发入参 = (控件指针, `OPER_COMMON_CLICKED` 操作码, 控件在布局对象中的索引(经 `param` 透传),
控件缓冲)。判定一律限定本控件的命中区(矩形裁剪 + 悬停语义), 不使用全局鼠标状态; 重叠区的归属由记录器
按"同帧取索引最大者"裁决。

跳过条件: 控件为空 / 记录器为空 / 控件矩形尺寸非正(零尺寸命中项会触发 UI 库断言)/ 未使用槽位
(`CTRL_TYPE_INVALID`, 无像素)。

视图叠加件: 在索引 `i` 的控件绘制之前给叠加件一次绘制机会, 位置在该控件 ID 压栈之前、同一内容容器
内 ⇒ 叠加件位于其下层绘制位置。叠加件**不是控件链成员** ⇒ 本入口不对它做任何点击判定, 控件链索引
口径保持唯一。

---

## RenderFlex 函数 (LayoutRender)

渲染无独立窗口句柄的内嵌(flex)布局。

### 语法

```cpp
BOOL RenderFlex(
    [in] Layout& layout,
    [in] CONST AutoSize& refsize,
    [in] CONST ProjCR* pProj
);
```

### 参数

`[in] layout`

类型: **Layout&**

内嵌成员的布局对象(线性只读展开态); 判为无效、缺 `WND` 或 `WND` 标签为空时渲染失败。

`[in] refsize`

类型: **CONST AutoSize&**

该成员自身的参考尺寸, 由调用方取自中间态的参考尺寸。

`[in] pProj`

类型: **CONST ProjCR\***

投影配色(宿主持有)。

### 返回值

类型: **BOOL**

`TRUE` = 本次渲染已完成; `FALSE` = 失败(未注入渲染能力 / 布局无效 / 缺 `WND` 或标签为空)。

### 注解

本入口**不取得目标窗口、不新建 UI 上下文**: 内嵌窗口无独立句柄, 沿用当前窗口(即由基座布局路径经
`AcquireWnd` 取得的那个窗口)。因此必须由宿主在基座窗口的渲染帧内、`RenderBase` 之后逐枚调用(同帧、
同一 UI 上下文), 且调用前须已进入宿主窗口的帧。

窗口几何 = 该布局 `WND` 结构件的声明像素: 位置相对宿主客户区, 尺寸即该布局窗口像素。窗口名以
"`WND` 标签 + 布局对象地址"唯一化 —— 各布局的结构件标签同名, 而 UI 库的窗口 ID 由名字算出, 不唯一化
会令多个内嵌窗口命中同一个窗口(内容互相覆盖)。控件 ID 空间另经 `PushPtrId(布局对象地址)` 隔离, 同名
控件在不同内嵌窗口内不冲突。

控件库上下文的位置记录槽传 `NULL`: 内嵌窗口没有可移动的窗口几何, 标题栏一类窗口级控件据此不建立
拖动。

内容编排与 `RenderBase` 同法(标题栏 → 内容容器 → 从 `GetShellNum()` 起逐控件分派), 差别是结尾显式
把本窗口置顶一次: UI 库的窗口绘制顺序会随焦点/激活调整, 而内嵌窗口与其宿主几何完全重叠(同为宿主
客户区坐标), 仅靠创建顺序不足以保证层级 ⇒ 每帧前置一次, 多枚成员即按渲染顺序自下而上叠放。

---

## IsIMEDisabled 函数 (LayoutRender)

查询本帧是否需要禁用输入法。

### 语法

```cpp
BOOL IsIMEDisabled();
```

### 参数

此函数无参数。

### 返回值

类型: **BOOL**

`TRUE` = 本帧存在处于编辑中的输入控件, 宿主应禁用 IME; `FALSE` = 无编辑中的输入控件。

### 注解

结论来自控件库的当前帧状态(`Controls::HasActiveInput`), 本模块不另存状态; 须在控件绘制之后、
同一帧内查询。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/layout/LayoutRender.h`(`src\framework\layout\LayoutRender.h`) |
| 实现 | `src\framework\layout\LayoutRender.cpp` |
| 命名空间 | `LayoutRender` |
| 依赖 | `framework/render/Render.h`(RenderApi 与 CtrlApi) · `framework/layout/Layout.h` · `framework/base/BasicDesign.h` · `framework/base/ControlSpec.h` · `framework/base/FuncPtr.h` · `ucrt` |
| 平台 | Windows 全版本 |
| 线程安全 | 否(经模块级能力表指针并写 UI 库状态); 须在渲染线程调用 |
