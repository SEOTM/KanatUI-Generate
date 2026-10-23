# AppEnv 模块 (src\framework\base\AppEnv.h)

## 概述

应用环境件:与控制台代码页、系统亮暗模式、窗口圆角相关的无状态通用原语。属于 **framework 层 · base**
(2026-10-23 自 common 归位), 只经 Win32 API 与 C 运行库, 不依赖工程内任何其它模块,
可被 framework 任意方向引用(平台查询件放在底座, 上层一律向下引用)。

本模块不做窗口创建与消息处理(归 `ImGuiHost` 与宿主), 不持有任何状态; 调用方按下列接口原文使用即可。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `InitConsoleUtf8` | 将控制台输入与输出代码页设为 UTF-8。 |
| `IsSystemDarkMode` | 读取系统"应用"亮暗模式; 深色返回 `true`, 读不到按浅色处理。 |
| `ApplyWindowCornerRounding` | 为窗口设置圆角: 优先 DWM 属性, 不可用时回退为圆角矩形区域裁剪。 |

本模块不定义公开类型与公开常量。

---

## InitConsoleUtf8 函数 (AppEnv)

把控制台输入与输出代码页都设置为 UTF-8(`CP_UTF8`)。

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

应在进程入口最先调用。本工程源码按 `/utf-8` 编译, 中文诊断经标准输出直出时须与代码页一致,
否则控制台按当前 ANSI 代码页解码会出现乱码。

进程无控制台(未附加控制台的 GUI 进程)时调用不报错, 也不产生可观察效果。

---

## IsSystemDarkMode 函数 (AppEnv)

读取系统当前的应用亮暗模式。

### 语法

```cpp
bool IsSystemDarkMode();
```

### 参数

此函数无参数。

### 返回值

类型: **bool**

`true` = 系统处于深色模式; `false` = 浅色模式或无法判定。

判定依据为注册表 `HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize` 的
`AppsUseLightTheme`(`REG_DWORD`, 取值 `0` ⇒ 深色)。键不存在、值类型不是 `REG_DWORD`、查询失败
一律返回 `false`(按浅色处理), 不抛错、不写注册表。

### 注解

本函数只读, 不缓存结果; 调用方如需按帧读取应在自己的配置刷新区调用, 不必每帧调用。

返回值只反映"应用"配色(任务栏 / 应用界面), 与"系统"配色(`SystemUsesLightTheme`)无关。

---

## ApplyWindowCornerRounding 函数 (AppEnv)

为指定窗口设置圆角, 并报出本次采用的是哪种实现。

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

目标窗口句柄。为 `NULL` 时函数直接返回, 不做任何事。

`[in] w`

类型: **DWORD**

窗口外框宽度(像素)。小于 `1` 时函数直接返回。

`[in] h`

类型: **DWORD**

窗口外框高度(像素)。小于 `1` 时函数直接返回。

`[in] space`

类型: **DWORD**

圆角半径(像素), 取设计间隔。仅在回退到区域裁剪时使用; 传给 DWM 的实现不读该值。

`[out] pbByRgn`

类型: **BOOL\***

接收本次实现方式, 可为 `NULL`(调用方不接收状态)。

- `TRUE`: 本次由圆角矩形区域裁剪实现 —— DWM 属性不可用, 窗口尺寸变化后须按新尺寸重设。
- `FALSE`: 由 DWM 属性实现 —— 系统随窗口尺寸保持圆角, 调用方无需重设。

函数在 `hWnd` 为空或尺寸非法而提前返回时**不改写**该值。

### 返回值

类型: **VOID**

此函数不返回值; 实现结果经 `pbByRgn` 报出。

### 注解

实现顺序固定为"先 DWM、后区域裁剪":

1. 设置 DWM 窗口属性 `DWMWA_WINDOW_CORNER_PREFERENCE` 为 `DWMWCP_ROUND`。`HRESULT` 成功 ⇒
   本次即完成, `pbByRgn` 写 `FALSE`。
2. DWM 属性不可用(旧系统、合成被禁用等)⇒ 用 `CreateRoundRectRgn(0, 0, w, h, space, space)` 建
   圆角矩形区域并以 `SetWindowRgn(hWnd, hRgn, TRUE)` 应用, `pbByRgn` 写 `TRUE`。

区域所有权纪律: `SetWindowRgn(..., TRUE)` 成功 ⇒ 区域对象交系统接管, 本函数不再释放; 设置失败 ⇒
所有权未转移, 由本函数 `DeleteObject` 释放, 且不改写 `pbByRgn`。

两种实现各打一行诊断到标准输出: `[MAIN] 圆角实现: DWM` / `[MAIN] 圆角实现: RGN:<space>`。

窗口为 `WS_POPUP` 且不含系统边框时, 外框即客户区, 直接传客户区尺寸即可。窗口尺寸变化后是否需要
重设, 以 `pbByRgn` 为准(见上)。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/base/AppEnv.h`(`src\framework\base\AppEnv.h`) |
| 实现 | `src\framework\base\AppEnv.cpp` |
| 命名空间 | `AppEnv` |
| 依赖 | `kernel32.dll` · `advapi32.dll` · `dwmapi.dll` · `user32.dll` · `gdi32.dll` |
| 平台 | Windows 全版本 |
| 线程安全 | 是(无模块级状态); 对同一窗口的调用须由调用方串行, 避免与其它窗口区域操作交错 |
