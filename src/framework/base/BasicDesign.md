# BasicDesign 模块 (src\framework\base\BasicDesign.h)

## 概述

设计基准模块:界面像素基准与色值的三处来源——屏幕参考尺寸(`ScreenRef`)、按经验参数推算的自动尺寸
(`AutoSize`)、投影配色(`ProjCR`), 外加一个把打包色值展开为浮点分量的宏。

本模块属 **framework 层的底座**:只产出数值与色值, 不做布局解析、不持窗口句柄、不绘制、不查控件。
依赖方向单向向下:只依赖 `common/MemoryManager.h` 的分配器类型(且仅 `ProjCR` 的轴位置表使用),
被控件层、部件层、几何解算与渲染出口消费。

文件内的显示器枚举回调、比例与尺寸互算辅助、`ProjCR` 的分配器静态状态均为内部件, 不对外。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `operator*` | 逐字段相乘两份尺寸信息(系数与参考量各自相乘)。 |

### 类型

| 类型 | 说明 |
| --- | --- |
| `ScreenRef` | 屏幕像素基准的静态查询件:屏宽、屏高、控件高、字号、间距与显示器工作区。 |
| `AutoSize` | 自动尺寸换算:参考基准乘千分比(或取固定像素)得到八项参考量的像素值。 |
| `ProjCR` | 投影配色:一条从底色端到光色端的单调色轴加槽位派生, 按绘图层号取色。 |

### 常量

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `MAKE_CR_IMVEC4(dw)` | 四分量初值表 `{ r, g, b, a }` | 把打包色值 `0xAABBGGRR` 逐字节展开为四个 `[0,1]` 浮点分量。 |

---

## operator* 函数 (全局作用域)

逐字段相乘两份尺寸信息。

### 语法

```cpp
template<typename oper>
inline AutoSize::SizeInfo<oper> operator*(
  const AutoSize::SizeInfo<oper>& a,
  const AutoSize::SizeInfo<oper>& b
);
```

### 参数

`[in] a`

类型: **const AutoSize::SizeInfo\<oper\>&**

左操作数, 八项参考量逐项参与相乘。不做有效域检查, 任一分量取 0 即该项结果为 0。

`[in] b`

类型: **const AutoSize::SizeInfo\<oper\>&**

右操作数, 与 `a` 必须为同一模板实参类型(本函数不提供跨类型重载)。

### 返回值

类型: **AutoSize::SizeInfo\<oper\>**

逐字段乘积。无失败路径、无饱和或钳位, 结果按 `oper` 自身的算术口径(整型实参会截断)。

### 注解

用途是"参考量像素 × 同一参考量上的比例系数"这类逐量缩放; 两个操作数同型是硬性要求,
混用 `SizeInfoDWORD` 与 `SizeInfoFLOAT` 时调用方须先自行转换。

乘法在头文件内定义并内联, 对任意算术 `oper` 可用; 与 `AutoSize` 的成员取值模板不同,
本函数不限于 `DWORD` / `FLOAT`。

---

## ScreenRef 类 (全局作用域)

屏幕像素基准的静态查询件:屏幕宽高、控件高、字号、间距与显示器工作区。

### 语法

```cpp
class ScreenRef
{
private:
    ScreenRef();
    ~ScreenRef();
public:
    static DWORD GetScreenWidth();
    static DWORD GetScreenHeight();
    static BOOL  GetPrimaryWorkArea(RECT* pWork);
    static BOOL  IsRectOnAnyMonitor(const RECT& rc);
    static DWORD GetContrlHeight();
    static DWORD GetFontSize();
    static DWORD GetSpace();
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `ScreenRef()` / `~ScreenRef()` | 私有且为空, 使本类不可实例化、不可继承使用(纯静态工具件)。 |
| `GetScreenWidth()` | 主显示器的屏幕宽度(像素)。 |
| `GetScreenHeight()` | 主显示器的屏幕高度(像素)。 |
| `GetPrimaryWorkArea(RECT*)` | 取主显示器的工作区矩形(桌面坐标, 已排除任务栏等保留区)。 |
| `IsRectOnAnyMonitor(const RECT&)` | 判定矩形是否与任一显示器的工作区相交。 |
| `GetContrlHeight()` | 控件高度基准 = 屏高 / 40。 |
| `GetFontSize()` | 字号基准 = 屏高 × 13 / 800。 |
| `GetSpace()` | 设计间隔基准 = 屏高 / 100。 |

### 注解

**取值口径**:三项派生基准(控件高 / 字号 / 间隔)都由屏高按整数除法算出, 因此它们随主显示器分辨率
整体缩放。返回值是像素量, 进程已启用 DPI 感知(见 `ImGuiHost::EnableDpiAwareness`) ⇒ 取到的是物理像素,
不是系统虚拟化后的逻辑像素。

**无缓存**:每次调用现场查询系统, 显示器热插拔或分辨率变更后下一次调用即反映新值, 不需要也不提供刷新入口。

**工作区查询**:`GetPrimaryWorkArea` 走系统工作区参数, 只认主显示器; 出参指针可以为 `NULL`
(此时只回答查询是否成功)。`IsRectOnAnyMonitor` 枚举全部显示器, 逐个取工作区做相交判定:
相交且交集非空即为可见, 右 / 下边界按开区间处理; 传入空矩形(右不大于左, 或下不大于上)恒为不可见。
两者用途分工:前者给"随机落点的范围", 后者答"声明位置是否越屏"。

**线程安全**:全部为无状态查询, 可跨线程并发调用。

---

## AutoSize 类 (全局作用域)

自动尺寸换算:以"参考基准 × 千分比"或"固定像素"两条路得出八项参考量的像素值, 并保留互算所需的另一侧。

### 语法

```cpp
class AutoSize
{
public:
    enum AutoSizeMode
    {
        AUTO_SIZE_BY_SCREEN_WIDTH,
        AUTO_SIZE_BY_SCREEN_HEIGHT,
        AUTO_SIZE_BY_WINDOW_WIDTH,
        AUTO_SIZE_BY_WINDOW_HEIGHT
    };

    template<typename numerical>
    struct SizeInfo
    {
        numerical screenWidth;
        numerical screenHeight;
        numerical windowWidth;
        numerical windowHeight;
        numerical controlHeight;
        numerical fontSize;
        numerical space;
        numerical padding;
    };
    using SizeInfoDWORD = SizeInfo<DWORD>;
    using SizeInfoFLOAT = SizeInfo<FLOAT>;

    struct RatioInfo
    {
        DWORD dwRatioWidth;
        DWORD dwRatioHeight;
        DWORD dwRatioControlHeight;
        DWORD dwRatioFontSize;
        DWORD dwRatioSpace;
        DWORD dwRatioPadding;
    };

    AutoSize();

    static AutoSize CreateByRatio(
        DWORD dwRatioWidth, DWORD dwRatioHeight,
        DWORD dwRatioControlHeight, DWORD dwRatioFontSize,
        DWORD dwRatioSpace, DWORD dwRatioPadding,
        AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);
    static AutoSize CreateByRatio(
        const RatioInfo& ratioInfo,
        AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);

    static AutoSize CreateByFixedSize(
        DWORD dwWindowWidth, DWORD dwWindowHeight,
        DWORD dwControlHeight, DWORD dwFontSize,
        DWORD dwSpace, DWORD dwPadding,
        AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);
    static AutoSize CreateByFixedSize(
        const SizeInfoDWORD& sizeInfo,
        AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);

    BOOL SetWindowRect(DWORD dwWindowWidth, DWORD dwWindowHeight);
    BOOL SetRatio(const RatioInfo& ratioInfo);
    BOOL SetRatio(
        DWORD dwRatioWidth, DWORD dwRatioHeight,
        DWORD dwRatioControlHeight, DWORD dwRatioFontSize,
        DWORD dwRatioSpace, DWORD dwRatioPadding);
    BOOL SetSize(const SizeInfoDWORD& sizeInfo);
    BOOL SetSize(const SizeInfoFLOAT& sizeInfo);

    template<typename numerical> SizeInfo<numerical> GetSizeInfo() const;
    template<typename numerical> numerical GetScreenWidth() const;
    template<typename numerical> numerical GetScreenHeight() const;
    template<typename numerical> numerical GetWindowWidth() const;
    template<typename numerical> numerical GetWindowHeight() const;
    template<typename numerical> numerical GetControlHeight() const;
    template<typename numerical> numerical GetFontSize() const;
    template<typename numerical> numerical GetSpace() const;
    template<typename numerical> numerical GetPadding() const;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `AutoSizeMode` | 尺寸计算模式:决定"参考基准数值"取自屏宽、屏高、窗宽还是窗高。 |
| `AUTO_SIZE_BY_SCREEN_WIDTH` | 基准 = 屏幕宽度。 |
| `AUTO_SIZE_BY_SCREEN_HEIGHT` | 基准 = 屏幕高度。 |
| `AUTO_SIZE_BY_WINDOW_WIDTH` | 基准 = 窗口宽度。 |
| `AUTO_SIZE_BY_WINDOW_HEIGHT` | 基准 = 窗口高度。 |
| `SizeInfo<numerical>` | 八项参考量的通用容器(模板参数为 `DWORD` 或 `FLOAT`), 字段顺序与经验参数的参考量顺序一致。 |
| `SizeInfoDWORD` / `SizeInfoFLOAT` | 上述模板的两种别名(整数口径 / 浮点口径)。 |
| `RatioInfo` | 六项千分比(窗宽、窗高、控件高、字号、间距、内边距), 每项合法域 `0..10000`。 |
| `AutoSize()` | 默认构造:八项尺寸、六项比例、模式与基准全部零值(模式取屏宽档)。 |
| `CreateByRatio(六个千分比, sizeMode)` | 按比例创建; 基准由 `sizeMode` 决定, 再换算成八项像素值。 |
| `CreateByRatio(const RatioInfo&, sizeMode)` | 同上, 比例以结构体给出。 |
| `CreateByFixedSize(六个像素值, sizeMode)` | 按固定像素创建; 屏宽 / 屏高自动取系统值, 基准由 `sizeMode` 决定。 |
| `CreateByFixedSize(const SizeInfoDWORD&, sizeMode)` | 同上, 八项参考量整体以结构体给出。 |
| `SetWindowRect(w, h)` | 仅在窗口模式下有效:更新窗口宽高、重定基准并按比例重算其余项。 |
| `SetRatio(const RatioInfo&)` | 换比例参数, 按当前基准重算八项像素值。 |
| `SetRatio(六个千分比)` | 同上, 比例以形参给出。 |
| `SetSize(const SizeInfoDWORD&)` | 换固定尺寸(整数口径), 按当前基准反算六项比例。 |
| `SetSize(const SizeInfoFLOAT&)` | 同上, 入参为浮点口径(先转整数口径再反算)。 |
| `GetSizeInfo<numerical>()` | 取整份尺寸信息(按调用方指定的数值类型转换后按值返回)。 |
| `GetScreenWidth<numerical>()` | 取屏幕宽度。 |
| `GetScreenHeight<numerical>()` | 取屏幕高度。 |
| `GetWindowWidth<numerical>()` | 取窗口宽度。 |
| `GetWindowHeight<numerical>()` | 取窗口高度。 |
| `GetControlHeight<numerical>()` | 取控件高度。 |
| `GetFontSize<numerical>()` | 取字号。 |
| `GetSpace<numerical>()` | 取设计间隔。 |
| `GetPadding<numerical>()` | 取设计内边距。 |

### 注解

**模式与基准**:两类工厂对基准的取法不同。`CreateByRatio` 在 `SCREEN_*` 档取屏宽 / 屏高,
在 `WINDOW_*` 档取"屏宽 / 2"或"屏高 / 2"作缺省窗口尺寸(此后须由 `SetWindowRect` 换成真实客户区);
`CreateByFixedSize` 在 `SCREEN_*` 档取屏宽 / 屏高, 在 `WINDOW_*` 档直接取入参的窗口宽 / 高。
模式越界(非四档之一)一律按屏宽档处理。

**比例与固定尺寸互算**:创建时一侧推另一侧, 改一侧同样重算另一侧。口径为"像素 = 基准 × 千分比 / 1000"
与"千分比 = 像素 × 1000 / 基准", 均为整数运算向下取整因而**不保证可逆**——往返换算存在 1 像素级取整差,
不要把"设进去的比例"与"读回来的比例"当同一数值比较。

**比例不做下限钳位**:千分比取 0 时该项像素即为 0(与 `GeometrySolver::EntryRefSizeInfo` 的"至少 1 像素"
不同), 比例超过 10000 也不拦, 由调用方保证合法域。

**`SetWindowRect` 的有效性**:仅 `AUTO_SIZE_BY_WINDOW_WIDTH` / `AUTO_SIZE_BY_WINDOW_HEIGHT` 两档生效,
其余模式下返回 `FALSE` 且对象状态完全不变。生效时窗口宽高、参考基准与六项比例一并按新窗口重算,
因此窗口尺寸变化后必须调用它, 否则窗口类参考量与控件量会脱节。

**`SetSize` 的取值口径**:入参的 `screenWidth` / `screenHeight` 会被原样写入(不再查询系统),
需要系统屏尺寸时由调用方先填好; 入参其余六项覆盖原值, 随后按当前基准反算六项比例。
两个重载都恒返回 `TRUE`, 不做校验。

**模板取值只支持 `DWORD` / `FLOAT`**:取值模板只在实现文件里对这两种类型显式实例化, 其它实参类型
在链接期得不到定义。逐项取值走同一条转换路径, 与 `GetSizeInfo` 的结果一致。

**值语义**:本类无指针成员、无动态资源, 可按值拷贝与按值返回(两类工厂都是按值返回的)。

**线程安全**:查询接口(取值模板)只读自身状态, 与其它线程的写接口并发时不安全;
`SetWindowRect` / `SetRatio` / `SetSize` 会整体覆写内部状态。

---

## ProjCR 类 (全局作用域)

投影配色:一条从底色端到光色端的单调色轴加槽位派生, 按绘图层号取色。

### 语法

```cpp
class ProjCR
{
public:
    enum SAMPLE_DIRECTION
    {
        PROJ_DARK_TO_LIGHT = 0,
        PROJ_LIGHT_TO_DARK = 1
    };

    enum CR_SLOT
    {
        PROJ_SLOT_SURFACE  = 0,
        PROJ_SLOT_HOVER    = 1,
        PROJ_SLOT_ACTIVE   = 2,
        PROJ_SLOT_CONTRAST = 3
    };

    using SampleFunc = FLOAT (*)(DWORD dwParam, DWORD dwLayers);

    static constexpr FLOAT DEFAULT_CONTRAST_STRENGTH = 1.0f;
    static constexpr FLOAT DEFAULT_BORDER_STRENGTH   = 0.25f;
    static constexpr FLOAT SLOT_OFFSET_RATIO         = 0.75f;

    static VOID SetAllocators(Arena* arena, MemPool* pool);

    static BOOL Create(ProjCR*& pOut, DWORD crBase, DWORD crLight, SAMPLE_DIRECTION eDirection,
        DWORD dwLayers, SampleFunc pfnSample = NULL);
    static VOID Release(ProjCR*& pOut);

    DWORD Sample(DWORD dwLayer, CR_SLOT eSlot = PROJ_SLOT_SURFACE) const;
    DWORD SampleBorder(DWORD dwLayerA, DWORD dwLayerB) const;

    VOID SetBase(DWORD crBase);
    VOID SetLight(DWORD crLight);
    VOID SetDirection(SAMPLE_DIRECTION eDirection);
    BOOL SetLayers(DWORD dwLayers);
    BOOL SetSampleFunc(SampleFunc pfnSample);
    VOID SetContrastStrength(FLOAT fStrength);
    VOID SetBorderStrength(FLOAT fStrength);

    DWORD GetBase() const;
    DWORD GetLight() const;
    SAMPLE_DIRECTION GetDirection() const;
    DWORD GetLayers() const;
    FLOAT GetContrastStrength() const;
    FLOAT GetBorderStrength() const;
    SampleFunc GetSampleFunc() const;
    FLOAT GetAxis(DWORD dwLayer) const;
    BOOL IsInvalid() const;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `SAMPLE_DIRECTION` | 采样方向:层号反射为采样参数的方式, 亮暗模式即由此切换。 |
| `PROJ_DARK_TO_LIGHT` | 暗到亮:层号即采样参数(层 1 取参数 1)。 |
| `PROJ_LIGHT_TO_DARK` | 亮到暗:层号反射为采样参数(层 1 取参数 层数)。 |
| `CR_SLOT` | 槽位:取色的目标种类。 |
| `PROJ_SLOT_SURFACE` | 基础槽:该层的表面色。 |
| `PROJ_SLOT_HOVER` | 附属槽:表面色与本层 +1 层表面色之差乘 `SLOT_OFFSET_RATIO`。 |
| `PROJ_SLOT_ACTIVE` | 附属槽:表面色与本层 -1 层表面色之差乘 `SLOT_OFFSET_RATIO`。 |
| `PROJ_SLOT_CONTRAST` | 对比槽:表面之上的对比色(按远端法向对侧推进)。 |
| `SampleFunc` | 单调采样函数类型:逐层调用, 返回该层在轴上的位置比例 `[0,1]`; 传 `NULL` 表示线性均匀采样。 |
| `DEFAULT_CONTRAST_STRENGTH` | 对比槽强度默认值 `1.0`(满对比, 承载文字与强调色)。 |
| `DEFAULT_BORDER_STRENGTH` | 边框强度默认值 `0.25`(柔和)。 |
| `SLOT_OFFSET_RATIO` | 悬停 / 激活的偏移比例 `0.75`(乘"本层与相邻一层的表面色差")。 |
| `SetAllocators(Arena*, MemPool*)` | 一次性静态注入轴位置表的两个分配器; 传 `NULL` 表示解绑。 |
| `Create(pOut, crBase, crLight, eDirection, dwLayers, pfnSample)` | 工厂构造:先校验层数与采样函数, 再分配轴位置表并构造对象。 |
| `Release(ProjCR*&)` | 与 `Create` 成对:归还轴位置表并释放对象本体, 引用置 `NULL`。 |
| `Sample(dwLayer, eSlot)` | 取某层的槽位色(取色正门)。 |
| `SampleBorder(dwLayerA, dwLayerB)` | 取两个绘图层之间的边框色(两层表面色取中后复用对比槽的远端法)。 |
| `SetBase(DWORD)` | 设底色端(轴的低 / 暗端), 恒成功。 |
| `SetLight(DWORD)` | 设光色端(轴的高 / 亮端), 恒成功。 |
| `SetDirection(SAMPLE_DIRECTION)` | 设采样方向, 恒成功; 亮暗模式切换的唯一入口。 |
| `SetLayers(DWORD)` | 设层数:先按新层数重校验采样函数, 再换表并重算; 失败则保持旧状态。 |
| `SetSampleFunc(SampleFunc)` | 设单调采样函数; 校验失败则保持旧函数与旧表。 |
| `SetContrastStrength(FLOAT)` | 设对比槽强度(钳到 `0..1`), 不改表。 |
| `SetBorderStrength(FLOAT)` | 设边框强度(钳到 `0..1`), 不改表。 |
| `GetBase()` | 取底色端。 |
| `GetLight()` | 取光色端。 |
| `GetDirection()` | 取采样方向。 |
| `GetLayers()` | 取层数。 |
| `GetContrastStrength()` | 取对比槽强度。 |
| `GetBorderStrength()` | 取边框强度。 |
| `GetSampleFunc()` | 取采样函数(`NULL` = 线性均匀)。 |
| `GetAxis(DWORD)` | 取某层的轴位置(`0` = 底色端, `1` = 光色端)。 |
| `IsInvalid()` | 取无效标志:未构造成功者恒 `TRUE`。 |

### 注解

**色值口径**:全部色值是 UI 库的打包值 `0xAABBGGRR`(低字节为 R), 与同文件的 `MAKE_CR_IMVEC4` 一致;
插值逐字节进行(含 α 字节), 因此结果与通道命名口径无关, 可直接喂给 `MAKE_CR_IMVEC4`。
构造与取色都不检查颜色本身(任意 32 位值都接受)。

**一条投影 = 一套配色**:底色端与光色端定义轴的两端, 与亮暗模式无关——切模式不对调两端颜色。
模式切换只改采样方向, 且方向在"喂给采样函数之前"把层号反射为采样参数(暗到亮 = 层号即参数 /
亮到暗 = 参数为 层数 + 1 - 层号), 两种方向下采样函数拿到的是同一枚函数在不同参数上的取值,
故色轴的单调性不因方向而反转。

**层号与合法域**:层号 1-based, 合法域为 `1..层数`, 端外越界属调用方违约——本类不做边界处理
(不夹取、不报错), 也不提供归一化浮点入口与槽位组合入口。层数无默认值, 且必须不小于 2
(层间差值至少需要两层)。

**采样函数的校验口径**:`Create` / `SetLayers` / `SetSampleFunc` 都逐层调用待校验函数,
判据两条——每次返回值落在 `[0,1]` 内(非数亦被拒), 且整串序列非严格单调(不允许既升又降);
`NULL` 视为线性均匀采样, 恒通过。

**所有权**:对象本体由 `Create` 内部建立, 轴位置表由注入的分配器给出(层表大小不超小对象池上限时取池,
否则回落线性分配器)。调用方只持有可用指针, 归还只能走 `Release`——不得自行释放, 也不要把同一指针
交两次 `Release`(函数内部置 `NULL`, 因此空指针调用无副作用)。

**分配器注入**:`SetAllocators` 是全进程一份的静态注入, 必须在首次 `Create` 之前完成;
未注入时层表分配失败 ⇒ `Create` 不构造。传 `NULL` 解绑用于初始化回卷与退出清理,
注入口径与 `Layout::SetAllocators` / `LayoutModel::SetAllocators` 同源。

**取色的失败口径**:未构造成功(工厂被拒或层表分配失败)时 `Sample` / `SampleBorder` 返回 `0`,
`GetAxis` 返回 `0`; `IsInvalid` 恒 `TRUE`。此时其余取值接口返回构造期写入的初值, 不报错。

**槽位派生**:悬停取"本层与 +1 层表面色之差"、激活取"本层与 -1 层表面色之差", 差值按
`SLOT_OFFSET_RATIO` 混合; 端点层缺少该侧相邻层时相邻层取回本层 ⇒ 差值为 0, 结果即表面色。
对比槽与边框共用"远端法":把轴位置朝离它更远的那一端推进强度比例, 强度为 `1` 时结果即远端端点,
轴位置为 `0.5` 时两端等距、固定取底色端。边框不占独立颜色, 且参数对称
(`SampleBorder(a, b)` 与 `SampleBorder(b, a)` 同值)。

**轴位置表与底色 / 光色无关**:表内容只由方向、层数与采样函数决定。`SetBase` / `SetLight`
会触发重算但表内容不变(重算属内部路径), 因此改两端颜色不需要重新构造对象。

**强度参数的时序**:`SetContrastStrength` / `SetBorderStrength` 只改强度、不改表——派生是取色时现算的,
因此可在任意时刻调整, 无需重算。

**派生强度默认值**:对比槽 `1.0`、边框 `0.25`、偏移比例 `0.75`; 前两者可改, 偏移比例是编译期常量。

**线程安全**:无内部同步。同一对象的取色可跨线程并发(只读), 但任何 `Set*` 与取色并发均不安全;
`SetAllocators` 改的是进程级静态状态, 不得与 `Create` / `Release` 并发。

---

## MAKE_CR_IMVEC4 宏 (全局作用域)

把打包色值展开为四分量浮点初值表。

### 语法

```cpp
#define MAKE_CR_IMVEC4(dw) { ((dw) & 0xFF) / 255.0f, (((dw) >> 8) & 0xFF) / 255.0f, (((dw) >> 16) & 0xFF) / 255.0f, ((dw) >> 24) / 255.0f }
```

### 参数

`[in] dw`

类型: **DWORD**

待展开的打包色值, 口径 `0xAABBGGRR`(低字节为 R, 与 `ProjCR` 的取色口径一致)。
四个字节一律除以 `255` 归一, 不检查取值范围, 也不做伽马或色彩空间换算。

### 返回值

类型: **四分量浮点初值表**(花括号列表, 不是表达式值)

依次为 R、G、B、A 四个 `[0,1]` 浮点分量。α 字节为 `0` 时展开为全透明。

### 注解

展开结果是**初值表**而非单值表达式, 只能出现在允许花括号初值的位置(变量初始化、聚合赋值)。
用于把 `ProjCR` 的打包色值直接写成 UI 库样式表的颜色项, 例如把某层某槽的颜色赋给样式数组元素。

宏对参数求值两次以上(每个分量各取一次), 因此入参应为无副作用的纯值表达式;
传函数调用一类的表达式会重复求值。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/base/BasicDesign.h`(`src\framework\base\BasicDesign.h`) |
| 实现 | `src\framework\base\BasicDesign.cpp`;模板与内联件在头文件内 |
| 命名空间 | 全局作用域 |
| 依赖 | `AutoSize` · `user32.dll` · `ScreenRef` · `common/MemoryManager.h` · `ucrt` · `minwindef.h` |
| 平台 | Windows 全版本 |
| 线程安全 | 是(查询与纯换算无共享状态); `ProjCR` 实例的建立 / 改向 / 重建须由调用方串行 |
