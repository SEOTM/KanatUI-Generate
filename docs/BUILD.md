# 构建与运行

## 1. 发布包内容

发布树包含：

```text
KanatUI-Generate/
├─ src/                            框架、文档层、编辑器层与应用层源码
├─ docs/                           工程文档
│  ├─ examples/KanatUIGenerate.json          自举布局文档（设计器自身的界面）
│  └─ LICENSE.md                   MIT 许可
└─ README.md
```

不包含 CMake、Visual Studio 工程、第三方依赖、预编译产物或构建脚本。用户按本文自行建立工程并准备依赖。

## 2. 环境要求

| 项 | 要求 |
| :--- | :--- |
| 系统 | Windows 10 / 11 x64 |
| Visual Studio | 2022（17.10 或更高）或 2026；安装"使用 C++ 的桌面开发"工作负载 |
| 平台 | x64 |
| 语言标准 | C++20 |
| 第三方依赖 | Dear ImGui 1.92.8（Win32 + DX11 后端）、RapidJSON 1.1.0 |

不需要 CMake、NuGet 或 Python。依赖获取方式见第 3 节。

### 语言标准

工程按 C++20 配置与构建。源码未使用 C++20 专属语法，但 C++17 未经本仓库完整验证，不作为支持目标。MSVC 需带 `/utf-8`（源码为 UTF-8）与 `/Zc:char8_t-`（`u8"..."` 仍作 `char*` 使用）。

## 3. 准备依赖

### 方式一：直接使用 ImGui 源码（推荐）

1. 获取 Dear ImGui **1.92.8** 源码；
2. 获取 RapidJSON **1.1.0** 源码（header-only，只需要 `include/rapidjson`）；
3. 把 ImGui 的以下源文件加入工程：
   - `imgui.cpp`
   - `imgui_draw.cpp`
   - `imgui_tables.cpp`
   - `imgui_widgets.cpp`
   - `backends/imgui_impl_win32.cpp`
   - `backends/imgui_impl_dx11.cpp`

ImGui 与 RapidJSON 均以 MIT License 分发。本工程只依赖 ImGui 的 `imgui.h`、`imgui_internal.h` 与两个 Win32 / DX11 后端头文件。

### 方式二：vcpkg（可选）

若已安装 vcpkg，可安装静态库版本：

```powershell
vcpkg install imgui[core,win32-binding,dx11-binding]:x64-windows-static
vcpkg install rapidjson:x64-windows-static
```

然后在工程中包含目录指向 `installed\x64-windows-static\include`，并链接 `imgui.lib`。此时不再把 ImGui 的 `.cpp` 加入工程。

## 4. 用 Visual Studio 构建

### 4.1 建立工程

1. 新建"空项目"（C++，Windows，控制台应用），平台选择 `x64`；
2. 把 `src/` 下全部 `.cpp` 加入项目（不加入 `src/` 下的 `.md` 文件）；
3. 若使用 ImGui 源码，把第 3 节的 6 个 ImGui `.cpp` 也加入项目；
4. 若使用 vcpkg，改为链接 `imgui.lib`，不再加入 ImGui `.cpp`。

### 4.2 工程属性

| 位置 | 设置 |
| :--- | :--- |
| C/C++ → 常规 → 附加包含目录 | `src`；ImGui 根目录；ImGui `backends`；RapidJSON 的 `include` 目录 |
| C/C++ → 语言 → C++ 语言标准 | ISO C++20（`/std:c++20`） |
| C/C++ → 代码生成 → 运行库 | Release：多线程 `/MT`；Debug：多线程调试 `/MTd` |
| C/C++ → 预处理器 → 预处理器定义 | `_AMD64_;UNICODE;_UNICODE` |
| C/C++ → 命令行 → 其他选项 | `/utf-8 /Zc:char8_t-` |
| 链接器 → 系统 → 子系统 | 控制台 |
| 链接器 → 输入 → 附加依赖项 | `d3d11.lib;dxgi.lib;dwmapi.lib;imm32.lib;comdlg32.lib` |

`user32.lib`、`gdi32.lib` 等 Windows 默认库由 Visual Studio 工程自动带入；若手工配置链接器，需一并加入。

### 4.3 生成与运行

选择 `Release|x64` 生成，得到 `KanatUIGenerate.exe`。把 [docs/examples/KanatUIGenerate.json](examples/KanatUIGenerate.json) 复制到 exe 同目录并命名为 `KanatUIGenerate.json`，然后运行：

```powershell
cd out\Release
.\KanatUIGenerate.exe
```

### 4.4 内嵌文档资源（可选）

内置资源是加载链的第 3 / 4 级。不内嵌也能运行（exe 同目录的外部文件是第 1 / 2 级），内嵌只是让 exe 自带一份可用的初始文档。

在工程里加一个 `.rc` 文件（放在工程根目录，下面的资源路径按工程根相对）：

```rc
#pragma code_page(65001)
#include "src\app\resource.h"

IDR_KANATUI_GENERATE_JSON JSON "docs\examples\KanatUIGenerate.json"
```

`IDR_KANATUI_GENERATE_JSON` = 101、`IDR_KANATUI_GENERATE_BIN` = 102 定义在 `src/app/resource.h`；资源类型是自定义串 `JSON` / `BIN`，运行期按这两个名字查找。若同时提供 `KanatUIGenerate.bin`，再补一行：

```rc
IDR_KANATUI_GENERATE_BIN BIN "out\KanatUIGenerate.bin"
```

`KanatUIGenerate.bin` 可用设计器的"保存"把 JSON 另存为 `.bin` 得到（保存方向见 [LAYOUT-FORMAT.md](LAYOUT-FORMAT.md) 第 9 节）。

## 5. 用 cl 命令行构建

在"Developer PowerShell for VS 2022/2026"中执行。先设置依赖路径和输出目录：

```powershell
$imgui = "C:\deps\imgui-1.92.8"
$rapidjson = "C:\deps\rapidjson-1.1.0\include"
New-Item -ItemType Directory -Force out | Out-Null
```

枚举本项目全部源文件，并加入 ImGui 源文件：

```powershell
$src = Get-ChildItem src -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }
$imguiSrc = @(
    "$imgui\imgui.cpp",
    "$imgui\imgui_draw.cpp",
    "$imgui\imgui_tables.cpp",
    "$imgui\imgui_widgets.cpp",
    "$imgui\backends\imgui_impl_win32.cpp",
    "$imgui\backends\imgui_impl_dx11.cpp"
)
```

编译并链接：

```powershell
cl /nologo /std:c++20 /EHsc /MT /O2 /utf-8 /Zc:char8_t- `
   /D_AMD64_ /DUNICODE /D_UNICODE `
   /I src /I $imgui /I "$imgui\backends" /I $rapidjson `
   $src $imguiSrc `
   /Fe:out\KanatUIGenerate.exe `
   /link d3d11.lib dxgi.lib dwmapi.lib imm32.lib comdlg32.lib user32.lib gdi32.lib
```

复制布局文档并运行：

```powershell
Copy-Item docs\examples\KanatUIGenerate.json out\KanatUIGenerate.json
.\out\KanatUIGenerate.exe
```

Debug 版本把 `/MT /O2` 改为 `/MTd /Od /Zi`，并把输出改为 `out\KanatUIGenerateDebug.exe`。

如果使用 vcpkg 或已有预编译 ImGui 库，去掉 `$imguiSrc`，在 `/link` 后加入 `imgui.lib` 的完整路径，并把包含目录指向对应的 `include`。

## 6. 运行参数

工作目录必须是可执行文件所在目录，布局文档按 exe 同目录解释。加载链依次为：

1. `.\KanatUIGenerate.bin`（exe 目录外部文件）
2. `.\KanatUIGenerate.json`（exe 目录外部文件）
3. 内嵌资源 `resource.res\KanatUIGenerate.bin`
4. 内嵌资源 `resource.res\KanatUIGenerate.json`

命中即用，不再探测后续级别；形态由内容嗅探判定，与扩展名无关。

| 环境变量 | 作用 |
| :--- | :--- |
| `KANATUI_GENERATE_AUTOCLOSE_MS` | 启动后 N 毫秒自动退出（无人值守用）；不设则常驻 |
| `KANATUI_GENERATE_PROJCR_LIGHT` | 置 1 强制按亮色模式取色（诊断用；不改变系统设置） |

示例：

```powershell
$env:KANATUI_GENERATE_AUTOCLOSE_MS = 1500
.\KanatUIGenerate.exe
```

退出方式：`ESC` 或 `Alt+F4`；有未保存改动时先弹出确认。

## 7. 布局文档

`docs/examples/KanatUIGenerate.json` 是设计器自身的完整布局，含三份布局：`预览布局`（预览画布）、`KanatUIGenerateEditor`（编辑窗口外壳，保留名）与 `提示布局`（内嵌提示窗口）。

启动时程序按保留名 `KanatUIGenerateEditor` 查找外壳布局；缺该布局会直接退出（退出码 1），因此自定义文档必须保留一份同名布局。字段、控件类型、尺寸因子与二进制格式见 [LAYOUT-FORMAT.md](LAYOUT-FORMAT.md)。

## 8. 排障

| 现象 | 原因与处置 |
| :--- | :--- |
| 编译报找不到 `imgui.h` | 附加包含目录未包含 ImGui 根目录与 `backends` |
| 编译报找不到 `rapidjson/document.h` | 附加包含目录未包含 RapidJSON 的 `include` 目录 |
| 链接报 `LNK2019`（ImGui 符号） | 未加入 ImGui `.cpp`，也未链接 `imgui.lib` |
| 链接报 `LNK2038` / 运行库不匹配 | 工程与 ImGui 库的 `/MT`、`/MTd` 不一致；统一运行库 |
| 链接报 `LNK2019`（`CreateDXGIFactory`、`GetOpenFileNameW` 等） | 未链接 `d3d11.lib`、`dxgi.lib`、`dwmapi.lib`、`imm32.lib`、`comdlg32.lib` |
| 编译报 `C7555` 或 C++20 语法错误 | 语言标准未设为 C++20 |
| 源码中文乱码或编译告警 | 缺少 `/utf-8` 或 `/Zc:char8_t-` |
| 启动弹窗"应用初始化失败" | 工作目录不对，或 exe 同目录没有 `KanatUIGenerate.json` / `KanatUIGenerate.bin`，且未内嵌资源 |
| 启动打印"外壳布局快照无效" | 文档缺少 `name == KanatUIGenerateEditor` 的布局，或该布局缺结构件、解析失败 |
| 修改 JSON 后界面没变 | 同目录存在旧的 `KanatUIGenerate.bin`，加载链优先 bin；删除或重新生成 |
| 窗口不出现 | 需要可交互的 Windows 桌面会话 |
