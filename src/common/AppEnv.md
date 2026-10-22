# AppEnv 模块 (src\common\AppEnv.h)

## 概述

`AppEnv` 是与控制台、系统设置、窗口外观相关的无状态通用原语: 设置控制台代码页、读取系统亮暗模式、
设置窗口圆角。命名空间内全部是自由函数, 不持任何状态, 每次调用都直接向系统查询或设置。

层归属是公共基础设施层, 不依赖工程内任何其它模块, 因此可被上层任意方向合法依赖。它只做"进程与系统
环境的一次性操作", 不涉及布局、控件或渲染, 也不做窗口创建与消息处理; 调用时机由宿主决定, 本模块不记忆
是否已调用过。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `InitConsoleUtf8` | 把控制台输入输出代码页都设为 UTF-8。 |
| `IsSystemDarkMode` | 读取系统深色模式设置。 |
| `ApplyWindowCornerRounding` | 设置窗口圆角, 优先 DWM 属性, 不支持时按区域裁剪。 |

---

## InitConsoleUtf8 函数 (AppEnv)

把控制台输入输出代码页都设为 UTF-8。

### 语法

```cpp
VOID InitConsoleUtf8();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

进程入口的首件事: 源码按 UTF-8 编译, 中文诊断经标准输出直出时必须与代码页一致, 否则控制台显示为乱码。
本函数只改控制台代码页, 不改文件或流的编码; 没有控制台时调用无副作用。

---

## IsSystemDarkMode 函数 (AppEnv)

读取系统深色模式设置。

### 语法

```cpp
bool IsSystemDarkMode();
```

### 参数

此函数无参数。

### 返回值

类型: **bool**

`true` = 系统当前为深色(应用)模式; `false` = 浅色, 或该设置读不到。

### 注解

读取当前用户注册表 `SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize` 的
`AppsUseLightTheme` 值, 值为 `0` 表示深色。键打不开、值不存在或类型不是 `DWORD` 时一律按浅色处理,
不报诊断。每次调用都重新读取, 不缓存 —— 系统设置变化后立刻生效, 代价是每次一次注册表查询。

---

## ApplyWindowCornerRounding 函数 (AppEnv)

设置窗口圆角。

### 语法

```cpp
VOID ApplyWindowCornerRounding(
  [in]  HWND  hWnd,
  [in]  DWORD w,
  [in]  DWORD h,
  [in]  DWORD space,
  [out] BOOL* pbByRgn
);
```

### 参数

`[in] hWnd`

类型: **HWND**

目标窗口句柄。为 `NULL` 时函数直接返回。

`[in] w`

类型: **DWORD**

窗口外框宽度(像素)。小于 `1` 时函数直接返回。窗口为无边框弹出式时外框即客户区。

`[in] h`

类型: **DWORD**

窗口外框高度(像素)。小于 `1` 时函数直接返回。

`[in] space`

类型: **DWORD**

圆角半径(像素), 回退到区域裁剪时作为圆角矩形的宽高。

`[out] pbByRgn`

类型: **BOOL\***

实现路径输出: `TRUE` = 本次由区域裁剪实现(此后尺寸变化需按新尺寸重设), `FALSE` = 由 DWM 属性实现。
允许为 `NULL`(不接收该状态); 两条失败路径(取不到区域对象、设置区域失败)不改写它。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

实现分两级: 优先设置 DWM 的窗口圆角属性为圆形(系统不支持时该调用失败), 失败后才回退为用圆角矩形区域
裁剪窗口。区域裁剪路径在设置成功后区域对象交系统接管, 调用方不得再释放; 设置失败时由本函数释放。

成功时向标准输出打印一行实现路径诊断并立即刷新。两条失败路径都不回写 `pbByRgn`, 因此调用方不能靠该输出
判断"是否真的设置了圆角" —— 它只在成功时被写入。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `common/AppEnv.h`(`src\common\AppEnv.h`) |
| 实现 | `src\common\AppEnv.cpp` |
| 命名空间 | `AppEnv` |
| 依赖 | 工程内: 无; 系统: `dwmapi`(`DwmSetWindowAttribute`)、`gdi32`(`CreateRoundRectRgn` / `SetWindowRgn`)、`advapi32`(`RegOpenKeyExW` / `RegQueryValueExW`), 链接库 `dwmapi` / `gdi32` / `advapi32` |
| 平台 | Windows; DWM 圆角属性需要较新的系统版本, 不支持时自动回退为区域裁剪 |
| 线程安全 | 是(无状态; 各调用只操作系统句柄或注册表, 不同窗口之间互不影响) |
