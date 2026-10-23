# GenUI（声明式界面框架与自举设计器）

GenUI 是一个 Windows 原生的声明式界面框架，以及一个用该框架自身实现的界面设计器。界面由一份布局文档（JSON 或二进制 `.bin`）描述：控件类型、标签、文本、事件与四轴几何系数都在文档里，代码里不写界面。设计器负责增删控件、调整几何、打开与保存文档，预览窗口实时呈现被编辑的布局；设计器自身的界面同样是一份 GenUI 布局，因此它是自举的。

> GitHub 发布树只保留 `src/` 与 `docs/`（另含本 README 与 `docs/LICENSE.md`）；不附带 CMake、Visual Studio 工程、第三方依赖或预编译产物。用户按 [docs/BUILD.md](docs/BUILD.md) 自行建立工程并准备依赖。

## 项目沿革

本框架的机制源自其早期应用 [VideoCut](https://github.com/SEOTM/VideoCut)。GenUI 在该框架上反复尝试模块间的耦合方式：断开反向依赖、抽出 `RenderApi` / `CtrlApi` 能力表、按 `base` / `render` / `layout` / `dispatch` 分层归位、剥离应用语义并移除非声明式回退路径。框架机制在 GenUI 阶段基本定型，此后以细节完善（几何、圆角、配色、文本、拖拽、提示）与编辑器能力为主。该框架的最小演示子集随后抽离为 Kanat 项目，GenUI 则是这个框架的自举编辑器应用。

## AI 使用

本项目的实现、重构、文档与验证由 AI 辅助完成，包括模块搬迁与接口化、像素与日志的逐字节零漂移验证、控件与部件以及宿主与文档链路的优化与适配。需求、模型、关键拍板、手工修改与验收由人工完成。所有产物经双配置零警告、逐字节基线与负向对照检验，并经人工审查与验收，内容由维护者负责。

## 特性

- **声明式布局**：界面结构、控件、几何参考量与事件名由布局文档声明；运行期不提供用代码拼界面的回退路径。
- **几何量全部相对化**：每个控件的 `x` / `y` / `width` / `height` 由 8 个参考量系数加像素偏移解算；窗口尺寸变化时按新参考尺寸整体重算，代码里不硬编码像素。
- **双格式文档**：JSON 便于阅读与版本管理；`.bin` 是定长记录 + CRC32 校验的紧凑格式，解析不依赖 JSON 库。两种形态用同一套模型，读方向按内容嗅探，写方向按目标扩展名分派。
- **数据经控件缓冲交互**：`ctrl.buffer` 承载控件状态，渲染是缓冲的函数；事件只经回调函数执行。
- **父链三用**：`parent` 字段同时驱动绘制层次、坐标累加与事件委托；`CTRL_EVENT_ENTRUST_PARENT` 把子控件的事件交给父容器。
- **渲染后端解耦**：布局渲染只依赖 `RenderApi` 与 `CtrlApi` 两张能力表，当前实现是 Dear ImGui + D3D11；替换 UI 技术时提供另一套实现并注入即可，布局与几何代码不改。
- **22 种控件类型**：其中 18 种有绘制实现，`WND` / `TITLE` / `CNTR` 是结构件，由渲染出口开合窗口与容器。
- **可自举的编辑器**：编辑窗口由保留名布局 `GenUIEditor` 驱动，无需为设计器另写一套界面代码。
- **自包含运行时**：静态链接（`/MT`），只依赖系统自带的 D3D11 / DXGI / DWM。

## 快速开始

### 环境要求

| 项 | 要求 |
| :--- | :--- |
| 系统 | Windows 10 / 11 x64 |
| 编译器 | Visual Studio 2022（17.10+）或 Visual Studio 2026，安装"使用 C++ 的桌面开发"工作负载 |
| 语言标准 | C++20（工程按此配置；C++17 未经完整验证） |
| 依赖 | Dear ImGui 1.92.8（Win32 + DX11 后端）、RapidJSON 1.1.0，由用户自备 |

### 构建

发布包不含构建工程，需自行建立 Visual Studio 项目或直接用 `cl` 编译。最小步骤：

1. 新建 x64 控制台空项目，把 `src/` 下全部 `.cpp` 加入；
2. 加入 ImGui 源文件：`imgui.cpp`、`imgui_draw.cpp`、`imgui_tables.cpp`、`imgui_widgets.cpp`、`backends/imgui_impl_win32.cpp`、`backends/imgui_impl_dx11.cpp`；
3. 配置：C++20、`/MT`（Debug 用 `/MTd`）、Unicode、`/utf-8 /Zc:char8_t-`；
4. 包含目录：`src`、ImGui 根目录、ImGui `backends`、RapidJSON 的 `include` 目录；
5. 链接：`d3d11.lib`、`dxgi.lib`、`dwmapi.lib`、`imm32.lib`、`comdlg32.lib`；
6. 生成后把 [docs/examples/GenUI.json](docs/examples/GenUI.json) 复制到 exe 同目录，命名为 `GenUI.json`。

完整的工程属性、命令行 `cl` 示例、可选的资源内嵌与排障见 [docs/BUILD.md](docs/BUILD.md)。

### 运行

工作目录必须是可执行文件所在目录，布局文档按 exe 同目录解析：

```powershell
cd <exe 所在目录>
.\GenUI.exe
```

启动后会出现两个窗口：预览窗口（被编辑的布局）与编辑窗口（设计器外壳）。`ESC` 或 `Alt+F4` 退出；有未保存改动时先弹出确认。`GENUI_AUTOCLOSE_MS` 可让程序在指定毫秒后自动退出，便于无人值守验证：

```powershell
$env:GENUI_AUTOCLOSE_MS = 1500
.\GenUI.exe
```

完整的构建选项与排障见 [docs/BUILD.md](docs/BUILD.md)。

## 界面如何描述

一份布局文档是一个 `layouts` 数组。每个布局包含布局级元数据、参考尺寸与控件数组：

```json
{
  "layouts": [
    {
      "name": "DemoMain",
      "label": "Demo",
      "width":  { "coeff": [0, 0, 1, 0, 0, 0, 0, 0], "offset": 0 },
      "height": { "coeff": [0, 0, 0, 1, 0, 0, 0, 0], "offset": 0 },
      "autosize": {
        "mode": "AUTO_SIZE_BY_SCREEN_HEIGHT",
        "unit": 0,
        "width": 570,
        "height": 500,
        "controlHeight": 25,
        "fontSize": 13,
        "space": 10,
        "padding": 4
      },
      "ctrls": [
        {
          "id": 0,
          "parent": -1,
          "type": "CTRL_TYPE_WND",
          "label": "##MAINWND",
          "text": "",
          "event": "",
          "x": { "coeff": [0,0,0,0,0,0,0,0], "offset": 180 },
          "y": { "coeff": [0,0,0,0,0,0,0,0], "offset": 120 },
          "width":  { "coeff": [0,0,1,0,0,0,0,0], "offset": 0 },
          "height": { "coeff": [0,0,0,1,0,0,0,0], "offset": 0 }
        }
      ]
    }
  ]
}
```

控件位置与尺寸的通用公式是：

```text
像素值 = Σ(coeff[i] × 参考量[i]) + offset     (i = 0..7)
```

八个参考量依次为屏幕宽、屏幕高、窗口客户区宽、窗口客户区高、控件高、字号、间隔、内边距；`autosize` 的六个经验参数按 `mode` 选定的基准推导后六项。字段含义、控件类型、父链规则与二进制格式见 [docs/LAYOUT-FORMAT.md](docs/LAYOUT-FORMAT.md)。

## 设计器

编辑窗口由外壳布局 `GenUIEditor` 声明的五块面板组成：

| 面板 | 内容 |
| :--- | :--- |
| 布局文件 | 当前文档路径；创建、打开、保存 |
| 布局管理 | 文档内全部布局的列表，选中即切换预览；支持增删与拖拽重排 |
| 经验参数 | 当前布局的 autosize：模式、单位、窗口宽高、控件高、字号、间距、内边距 |
| 控件列表 | 当前布局的控件列表：选中、改名、拖拽重排 |
| 控件编辑 | 选中控件的类型、标签、文本、事件、尺寸轴、8 个系数、像素偏移、前序 ID |

预览窗口内可直接点选控件；选中态以叠加件绘制，不修改布局对象。保存方向支持 JSON 与 `.bin` 两种目标：写入 JSON 时保证"载入后再写出"逐字节一致；`.bin` 由同一份中间态按定长记录写出。

## 架构

```text
main.cpp
  └─ AppHost（宿主:两窗口 / 消息循环 / 帧流程）
       ├─ Application          应用单例:文档、条目、激活布局、启动加载链
       ├─ PreviewSession       预览唯一基座 + 外壳快照 + 帧间同步
       ├─ FlexHost             内嵌(flex)窗口成员
       ├─ DocumentIO / DocumentSource   按路径载入 / 四级启动加载链
       ├─ GenUIEditor          外壳交互:选中、路径、提示、事件处理
       ├─ EditorCommands       文档编辑命令:增删改移
       └─ LayoutWriter         写方向:中间态 → JSON / bin

LayoutSolver（读方向:JSON / bin → 中间态）
  └─ LayoutBuilder → Layout（布局对象）+ CtrlBuffer（控件缓冲）
        └─ LayoutRender ──RenderApi / CtrlApi──> ImGui ──> D3D11
```

框架分四层：

| 层 | 目录 | 职责 |
| :--- | :--- | :--- |
| 基础 | `src/framework/base/` | 设计参考量、控件类型契约、控件缓冲、文本宽度、分配器、CRC32 |
| 渲染 | `src/framework/render/` | 渲染能力接口；`imgui/` 下的宿主、D3D11 设施、控件库与部件层 |
| 布局 | `src/framework/layout/` | 布局对象、中间态、构建、读方向解析、几何解算、渲染出口 |
| 调度 | `src/framework/dispatch/` | 帧间事件、事件绑定、调度与场景切片、资源路径 |

应用层分为 `src/doc/`（文档承载者、一致性校验、保真旁挂表）、`src/editor/`（外壳、命令、写方向、选中叠加）与 `src/app/`（宿主、应用、文档 I/O、预览会话、内嵌成员）。各层依赖方向、数据流与扩展点见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 目录结构

```text
GenUI/
├─ src/
│  ├─ main.cpp                  进程入口
│  ├─ app/                      应用层:宿主、应用单例、文档 I/O、加载链、预览会话、内嵌成员
│  ├─ doc/                      文档领域层:文档承载者、结构一致性、保真旁挂表
│  ├─ editor/                   编辑器层:外壳逻辑、编辑命令、写方向、选中叠加件
│  └─ framework/                框架内核
│     ├─ base/                  参考量、控件契约、缓冲、文本宽度、分配器、校验
│     ├─ render/                能力表与 ImGui / D3D11 实现
│     ├─ layout/                布局对象、中间态、解析、几何、渲染出口
│     └─ dispatch/              帧间事件、事件绑定、调度切片、资源路径
├─ docs/                        工程文档
│  └─ examples/GenUI.json       自举布局文档
└─ README.md
```

`src/` 下每个模块都在同目录配一份接口文档；契约细节以这些接口文档为准，代码只保留函数的 `函数 / 描述` 摘要。

## 文档

| 文档 | 内容 |
| :--- | :--- |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 分层、数据流、文档模型、几何、能力表、事件、编辑器会话、内存、扩展点 |
| [docs/BUILD.md](docs/BUILD.md) | 构建、依赖准备、资源内嵌、运行参数与排障 |
| [docs/LAYOUT-FORMAT.md](docs/LAYOUT-FORMAT.md) | 布局字段、autosize、尺寸因子、控件类型、父链、事件、bin 格式与校验 |
| [docs/DERIVE.md](docs/DERIVE.md) | 从本仓库派生新应用 |
| [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) | 编码风格、注释与文档约定、自检、提交 |
| [docs/INTERFACE-DOC-STYLE.md](docs/INTERFACE-DOC-STYLE.md) | 接口文档体例 |
| `src/**/<模块>.md` | 逐模块接口契约 |

## 已知限制

- 仅支持 Windows x64 与 MSVC；渲染后端目前只有 ImGui + D3D11。
- 一次只编辑一份文档，没有多标签页；没有撤销 / 重做。
- `LIST` / `LIST_TIMESPAN` 已在类型表中登记，但暂无绘制实现（按未知类型占位）。
- 未启用 ImGui viewports：预览窗口是真实的被编辑文档窗口，与 viewports 语义冲突。
- 交互观感（拖动、悬停、输入法）需在本机人工复核；无人值守下只覆盖启动与渲染链路。
- 发布树不含构建定义与第三方依赖，构建需按 [docs/BUILD.md](docs/BUILD.md) 自行准备。

## 许可

MIT License，见 [docs/LICENSE.md](docs/LICENSE.md)。Dear ImGui 与 RapidJSON 均为 MIT License，由用户自行获取。
