# ImGuiRender 模块 (src\framework\render\imgui\ability\ImGuiRender.h)

## 概述

Dear ImGui 渲染能力提供者:纯静态命名空间, 只对外暴露"取能力表"一个入口。它按 `framework/render/Render.h`
的接口形式填一张能力表, 把框架语汇的窗口与容器开合、ID 命名空间、光标定位与内容原点、逐控件命中判定、
窗口层级置顶翻译成 ImGui 调用。

本模块是渲染侧唯一出现 ImGui 类型与 ImGui 头文件的边界 ⇒ 消费件只持 `RenderApi` 表, 不必也不得依赖
`imgui.h`; 换用其它 UI 实现 = 另写一份同形态的能力提供者并在注入点替换, 消费件零改动。

本模块属 **framework 层**, 依赖 `framework/render/Render.h`(接口形式)与 `framework/render/imgui/ImGuiHost`(反调面:
渲染目标窗口由宿主按渲染顺序决定), 不认识布局、不持控件数据、不含应用语义。

使用前提: 全部能力的调用都发生在 `ImGuiHost` 已开帧的当前上下文内 —— 实现直接作用于"当前窗口"。
本模块无所有权: 不持窗口句柄与 ImGui 上下文, 只在开窗时记下窗口指针供帧末置顶。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `GetApi` | 取本实现填好的渲染能力表(进程级静态, 恒非空)。 |

本模块不定义公开类型与公开常量:能力表的成员类型(`pfn*`)与窗口状态位(`RENDER_WND_*`)都定义在
`framework/render/Render.h`。文件内的补项计数、补项顶点增量与"最近开启的窗口"指针均为文件内静态, 各能力的
实现函数均为文件内 `static`, 不立条。

---

## GetApi 函数 (ImGuiRender)

取本实现填好的渲染能力表。

### 语法

```cpp
CONST RenderApi* GetApi();
```

### 参数

此函数无参数。

### 返回值

类型: **CONST RenderApi\***

能力表只读指针。表为进程级静态实例, 恒非 `NULL`, 生命周期与进程相同; 调用方不得写它, 也不得释放它。

### 注解

唯一注入点: `ImGuiHost::Acquire` 在每窗口 ImGui 面就绪后调用本函数, 把返回的表交给布局渲染出口
(`LayoutRender::SetRenderApi`)。表与窗口存续无关 ⇒ 多窗口重复注入同一张表无副作用。注入之前,
布局渲染出口的各渲染入口一律静默失败(返回 `FALSE`), 不弹窗、不打印。

表中的能力全部直接作用于"当前 ImGui 上下文内的当前窗口", 因此调用方须保证: 调用发生在宿主已开启的
一帧之内, 且 Id 命名空间的压栈与出栈、窗口与容器的开启与关闭都按配对关系使用。实现不自持窗口与句柄,
也不检查是否已开帧(未开帧时由 ImGui 自身的行为决定结果)。

各能力的实现侧口径:

| 能力 | 实现口径 |
| --- | --- |
| `AcquireWnd` | 尺寸非正即返回 `FALSE`; 其余判据(是否就绪、是否被遮挡、是否调窗、是否改标题)全部转交宿主 `ImGuiHost::AcquireNextWnd`(标题参数原样下传)。 |
| `OpenWnd` | 状态位按位翻译: `RENDER_WND_BASE` 译为无标题栏、禁缩放、禁移动、禁折叠、焦点不置顶、无滚动条、滚轮无效一组标志; `RENDER_WND_NOINPUT` 追加窗口级无输入。恒返回 `TRUE`(开窗无失败语义)。 |
| `CloseWnd` | 关闭当前窗口, 与 `OpenWnd` 成对。 |
| `BringWndToFront` | 把本模块记录的"最近开启的窗口"置于绘制顺序最前。ImGui 的绘制顺序会随焦点与激活调整, 仅靠创建顺序不足以保证层级, 故须每帧显式前置一次(在 `CloseWnd` 之后调用)。 |
| `OpenPanel` | 以当前光标位置开子窗口(内容容器), 背景色经样式槽临时压入、开启后立即弹出 ⇒ 只影响本容器底色; `0x00000000` 为全透明。恒返回 `TRUE`。 |
| `ClosePanel` | 关闭当前内容容器, 与 `OpenPanel` 成对。 |
| `SetCursor` | 把当前窗口或容器的光标移到给定本地坐标。 |
| `GetContentOrigin` | 取当前光标的位置作为内容原点(屏幕坐标); 须在进入容器之后立即取一次, 此时光标初始位置即内容原点。 |
| `PushId` / `PushPtrId` / `PopId` | 直接映射 ImGui 的 ID 压栈与出栈; 指针形态按键地址隔离(内嵌窗口按布局对象地址隔离)。 |
| `TakeItemId` | 取"当前窗口最近一次提交的可交互项"的 ID(尚无末项时为 `0`); 逐控件绘制前后各取一次即可判断该控件是否自行提交了项。 |
| `AddHitArea` | 以不可见按钮在指定矩形上补一枚无绘制命中项, 并允许其上(后绘制)的可交互项覆盖它 —— 否则它会抢走上层控件的命中; 补项不产生任何绘制。标签为 `NULL` 时取内部兜底名。 |
| `IsHitClicked` | 判定"最近提交的项"本帧是否被左键点击; 须紧接绘制或补项之后调用。 |
| `IsRectClicked` | 判定指定屏幕矩形内本帧是否发生左键按下(含当前窗口裁剪) —— 复合控件的矩形兜底判定用。 |
| `GetDrawVtxCount` | 取当前窗口累计的绘制顶点数, 可供观察补项是否产生绘制。 |

失败口径与框架其余部分一致: 一律静默返回码(返回 `FALSE` 或空值), 不弹窗、不打印、不写日志。
本模块不解释几何与语义: 表中的几何量都是框架自有语汇(像素坐标 `vec2`), 由本模块换算成 ImGui 类型。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/render/imgui/ability/ImGuiRender.h`(`src\framework\render\imgui\ability\ImGuiRender.h`) |
| 实现 | `src\framework\render\imgui\ability\ImGuiRender.cpp` |
| 命名空间 | `ImGuiRender` |
| 依赖 | `imgui` · `framework/render/Render.h` · `framework/render/imgui/ImGuiHost` |
| 平台 | Windows 全版本 |
| 线程安全 | 否; 全部能力须在宿主主循环线程的已开帧上下文内调用, 进程级状态亦无同步 |
