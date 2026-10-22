# GeometrySolver 模块 (src\framework\layout\GeometrySolver.h)

## 概述

几何解算与参考量换算:布局级像素几何的唯一解算式所在——自动尺寸模式名与枚举的互认、换算基准的取法、
经验参数(千分比)与像素值的互折、控件几何因子到像素矩形的解算, 以及窗口件位置记录的落定。

本模块属 **framework 层**:只做纯换算与就地写回, 不持布局对象、不分配内存、不画不改任何状态机;
输入一律是中间态规格(`ElementSpec` / `RefSize` / `SizeFactor`)与参考尺寸, 输出是像素量或写回控件信息。
依赖方向单向下行:向下依赖 `BasicDesign`(屏幕基准与尺寸容器)、`ControlSpec`(控件信息与几何因子词表)、
`LayoutModel`(中间态规格与参考量顺序), 不被这三者反向引用。

文件内的随机落点辅助与"随机位置"哨兵常量是内部件, 不对外; 其对外可见后果已并入 `ResolveWndPos` 的注解。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `AutoModeNameOf` | 取自动尺寸模式枚举对应的规范名串。 |
| `AutoModeIndex` | 取自动尺寸模式名串对应的枚举值。 |
| `EntryBasePx` | 取某个自动尺寸模式的换算基准像素。 |
| `EntryRefSizeInfo` | 依参考尺寸与经验参数推导八项参考量的像素值。 |
| `RefValueToRatio` | 把按单位输入的参考量折算为千分比。 |
| `FactorAbsPx` | 由几何因子与参考尺寸算出一个像素量。 |
| `ApplySpecGeometry` | 依中间态规格就地解算一枚控件的像素几何。 |
| `ResolveWndPos` | 落定窗口件的位置记录(随机 / 采用声明 / 越屏降级)。 |

本模块不定义公开类型与公开常量。

---

## AutoModeNameOf 函数 (GeometrySolver)

取自动尺寸模式枚举对应的规范名串。

### 语法

```cpp
LPCSTR AutoModeNameOf(INT mode);
```

### 参数

`[in] mode`

类型: **INT**

自动尺寸模式枚举值, 合法域 `0..3`(屏宽 / 屏高 / 窗宽 / 窗高), 语义与 `AutoSize::AutoSizeMode` 一致。
负数或大于 3 的取值不报错, 按屏高模式处理。

### 返回值

类型: **LPCSTR**

指向模块内静态名表的常量字符串, 四种模式之一: `AUTO_SIZE_BY_SCREEN_WIDTH` /
`AUTO_SIZE_BY_SCREEN_HEIGHT` / `AUTO_SIZE_BY_WINDOW_WIDTH` / `AUTO_SIZE_BY_WINDOW_HEIGHT`。
指针由模块持有、随进程存续, 调用方不得释放或改写。

### 注解

本函数与 `AutoModeIndex` 构成双向对照:中间态只按串存模式名, 枚举只在解算期使用, 两者经这一对函数互认。
名字串是文档字段的存储形态(写进布局文档的 `autosize` 字段), 因此改名会改变文档格式。

越界入参的回退方向是"屏高模式", 与 `AutoModeIndex` 的未命中回退一致——两侧都不把无法识别的模式
当作错误, 而是收敛到同一档。

---

## AutoModeIndex 函数 (GeometrySolver)

取自动尺寸模式名串对应的枚举值。

### 语法

```cpp
INT AutoModeIndex(LPCSTR mode);
```

### 参数

`[in] mode`

类型: **LPCSTR**

以 `\0` 结尾的模式名串。合法取值 = `AutoModeNameOf` 的四个规范名(逐字符全等比较, 区分大小写)。
传 `NULL`、空串或任何未命中的串都不报错, 按屏高模式处理。

### 返回值

类型: **INT**

`AutoSize::AutoSizeMode` 的枚举值(`0..3`)。未命中与 `NULL` 一律返回屏高模式的枚举值。

### 注解

比较是精确匹配:大小写不同、前后带空白的串都算未命中。因此从文档读到的模式串应当就是
`AutoModeNameOf` 写出的形态; 跨版本改名会静默退化为屏高模式, 而不是解析失败。

---

## EntryBasePx 函数 (GeometrySolver)

取某个自动尺寸模式的换算基准像素。

### 语法

```cpp
DWORD EntryBasePx(INT mode);
```

### 参数

`[in] mode`

类型: **INT**

自动尺寸模式枚举值。合法域 `0..3`; 越界不报错, 按屏高处理。

### 返回值

类型: **DWORD**

换算基准像素:屏宽模式与窗宽模式取屏幕宽度, 屏高模式与窗高模式取屏幕高度。
因此窗口模式在当前语境下与对应的屏幕模式同值(见注解)。

### 注解

窗口两档在无外部宿主的语境下没有"外部宿主"可问窗口尺寸, 故按屏幕宽 / 高兜底; 真实窗口尺寸的影响
留给 `EntryRefSizeInfo` 的窗口宽高入参。两种屏幕尺寸都由 `ScreenRef` 现场查询, 不做缓存。

基准恒为正(屏幕尺寸不会为 0), 调用方不需要再判零。

---

## EntryRefSizeInfo 函数 (GeometrySolver)

依参考尺寸与经验参数推导八项参考量的像素值。

### 语法

```cpp
AutoSize::SizeInfo<DWORD> EntryRefSizeInfo(
  const RefSize& refSize,
  DWORD base,
  DWORD wndW,
  DWORD wndH
);
```

### 参数

`[in] refSize`

类型: **const RefSize&**

该布局的参考尺寸 / 经验参数(中间态形态)。读四项: 控件高、字号、间隔、内边距, 单位一律千分比;
`mode` 与 `unit` 字段不参与本函数。千分比超出 `0..10000` 不拦(结果按同一算式放大)。

`[in] base`

类型: **DWORD**

换算基准像素, 通常取 `EntryBasePx` 的返回值。传 `0` 时内部按 `1` 处理, 因此不会出现除零。

`[in] wndW`

类型: **DWORD**

该布局窗口客户区宽度(像素)。传 `0` 时窗口宽取屏幕宽度(不视为错误)。

`[in] wndH`

类型: **DWORD**

该布局窗口客户区高度(像素)。传 `0` 时窗口高取屏幕高度。

### 返回值

类型: **AutoSize::SizeInfo\<DWORD\>**

八项参考量的像素值:屏宽 / 屏高恒取系统值; 窗口宽 / 高取 `wndW` / `wndH`(为 0 时回退屏幕尺寸);
控件高 / 字号 / 间隔 / 内边距 = 基准 × 千分比 / 1000, 并**至少为 1 像素**。无失败路径。

### 注解

四项经验值的最小值钳位是刻意的:千分比取很小的值时仍然给出 1 像素, 避免出现零高控件与零号字。
这一点与 `AutoSize` 的比例换算不同(后者不做下限钳位), 因为这里的产物要直接用于绘制。

窗口宽高为零即"未知窗口" ⇒ 回退屏幕尺寸, 使解算在布局尚未窗口化的阶段也能给出可用几何。

---

## RefValueToRatio 函数 (GeometrySolver)

把按单位输入的参考量折算为千分比。

### 语法

```cpp
DWORD RefValueToRatio(DWORD value, DWORD base, INT unit);
```

### 参数

`[in] value`

类型: **DWORD**

按该条目当前单位输入的参考量。千分比口径下的合法域为 `0..10000`; 本函数不做下限检查,
超上限的结果由返回值钳位。

`[in] base`

类型: **DWORD**

换算基准像素(见 `EntryBasePx`)。仅在像素口径下参与运算; 传 `0` 时内部按 `1` 处理。

`[in] unit`

类型: **INT**

参考量单位。`0` = 千分比(原值即千分比, 不做换算); 非 `0` 任一取值 = 像素(按基准反算)。

### 返回值

类型: **DWORD**

千分比, 恒落在 `0..10000`。像素口径按四舍五入折算, 结果超过 10000 时钳到 10000。

### 注解

存储侧恒为千分比:单位只决定入参如何解释, 与存储形态无关。因此"同一物理尺寸在两种单位下输入"
应得到同一个千分比(受四舍五入影响可能差 1)。

本函数只做单向折算(输入值 → 千分比); 反向(千分比 → 像素)由 `EntryRefSizeInfo` 与 `AutoSize`
负责, 两侧口径一致。

---

## FactorAbsPx 函数 (GeometrySolver)

由几何因子与参考尺寸算出一个像素量。

### 语法

```cpp
FLOAT FactorAbsPx(const SizeFactor& f, const AutoSize::SizeInfo<FLOAT>& ref);
```

### 参数

`[in] f`

类型: **const SizeFactor&**

控件几何因子:八个系数(按参考量序号排列)加一个固定偏移。系数与偏移都可为负, 合法域不设限;
未初始化的因子按零处理(结果只剩偏移)。

`[in] ref`

类型: **const AutoSize::SizeInfo\<FLOAT\>&**

八项参考量的像素值, 顺序与系数顺序一一对应(屏宽 / 屏高 / 窗宽 / 窗高 / 控件高 / 字号 / 间隔 / 内边距)。
该顺序由 `LayoutModel` 的 `SizeFactor` 与 `GENFACTOR_NUM` 固定。

### 返回值

类型: **FLOAT**

`Σ(系数[i] × 参考量[i]) + 偏移`。结果不取整、不钳位, 可为负或超出屏幕范围——取整与裁剪由调用方按需处理。

### 注解

本函数是"系数 + 参考尺寸 → 像素"的唯一算式, 整条重解与单枚重解共用它, 因此同一份中间态
在任意调用点都得到同一像素值。

中间按双精度累加再落回单精度, 系数与参考量都较大时也不会在累加过程中丢精度。

---

## ApplySpecGeometry 函数 (GeometrySolver)

依中间态规格就地解算一枚控件的像素几何。

### 语法

```cpp
VOID ApplySpecGeometry(
  CtrlInfo& info,
  const ElementSpec* pSpec,
  const AutoSize::SizeInfo<FLOAT>& ref
);
```

### 参数

`[in, out] info`

类型: **CtrlInfo&**

待解算的控件信息。只写 `posDeclared`(声明坐标, 父控件坐标系内的坐标)、`position`(有效坐标, 解算当下
先取与声明值相同的值)与 `size` 三个字段, 其余字段(类型、层次、缓冲、事件指针)保持原值。

`[in] pSpec`

类型: **const ElementSpec\***

该控件对应的中间态元素规格。传 `NULL` 表示"无规格可依" ⇒ 函数直接返回, `info` 的像素几何**保持现状**
(不报错、不清零)。

`[in] ref`

类型: **const AutoSize::SizeInfo\<FLOAT\>&**

该条目的参考尺寸(像素), 逐项喂给 `FactorAbsPx`。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

调用前提是 `info` 已经与 `pSpec` 配对(同一条链序位置); 本函数不校验二者是否属于同一控件,
配对错误只会得到另一枚控件的几何。

`pSpec` 为空即"保持现状"是刻意口径:预览选中态一类的临时控件没有中间态规格, 但仍需保住既有几何。
因此调用方不需要在调用前判空。

**声明坐标与有效坐标分工**:本函数产出的是**父控件坐标系内的声明坐标**(`posDeclared`); 内容控件的
有效坐标(窗口本地)随后由 `Layout::DeriveCtrlPositions` 按父链累加得出。解算后立即读 `position` 的
调用方拿到的是声明值(与 `posDeclared` 相等), 而不是最终有效坐标。

---

## ResolveWndPos 函数 (GeometrySolver)

落定窗口件的位置记录(随机 / 采用声明 / 越屏降级三分支)。

### 语法

```cpp
VOID ResolveWndPos(CtrlInfo* pWnd);
```

### 参数

`[in, out] pWnd`

类型: **CtrlInfo\***

窗口件。读其 `position`(声明位置)与 `size`(客户区尺寸), 成功后把 `position` 就地改写为解算结果;
`size` 与其余字段不动; 收尾把 `posDeclared` 同步为最终 `position`(两者一致, 便于后续重推坐标)。
传 `NULL` 直接返回。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

**三分支**:声明位置为 `(-1, -1)` ⇒ 取随机位置; 声明位置非哨兵且窗口矩形与任一显示器工作区相交 ⇒
直接采用声明位置; 声明位置非哨兵但窗口完全落在所有屏幕之外(显示器配置变化等) ⇒ 降级为随机位置。

**随机位置的落点口径**:在主显示器工作区内均匀取点, 保证窗口完整落在工作区内(上界为
"工作区右 / 下减窗口宽 / 高"); 窗口尺寸不小于工作区时有效区间为空, 结果夹到工作区左上角。
取不到主显示器工作区时落点退化为 `(0, 0)`。

**随机性与调用次数**:随机源在进程内首次使用时按系统计时播种一次, 因此每次启动的落点不同、
同一次启动内逐枚窗口不同; 同一进程内不提供"复现上次落点"的手段。

**可见性判据**:部分可见即算可见——窗口只要与某个显示器工作区有非空交集就保留声明位置,
多屏交界处不强行归位。越屏判断用桌面坐标, 与声明位置同一坐标系。

**调用时机**:解析期几何解算的最后一步, 必须在该窗口件的 `size` 已解算完成之后调用(落点依赖尺寸)。
本函数是位置记录的唯一落点, 其它模块不得另写一套位置判定。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/layout/GeometrySolver.h`(`src\framework\layout\GeometrySolver.h`) |
| 实现 | `src\framework\layout\GeometrySolver.cpp` |
| 命名空间 | `GeometrySolver` |
| 依赖 | `ucrt` · `BasicDesign` · `LayoutModel` · `ControlSpec` · `AutoSize` |
| 平台 | Windows 全版本 |
| 线程安全 | 是(纯函数) |
