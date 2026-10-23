# 派生新应用

GenUI 同时是"框架"和"用框架做的设计器"。派生有两种口径：只要框架，或连编辑器一起拿走再改。

## 1. 只要框架

复制 `src/framework/` 到新工程，保留四层目录结构：

```text
framework/
├─ base/        参考量、控件类型、控件缓冲、文本宽度、分配器、CRC32
├─ render/      能力表(RenderApi / CtrlApi / HostApi)与 ImGui 实现
├─ layout/      布局对象、中间态、解析、几何、渲染出口
└─ dispatch/    帧间事件、事件绑定、调度与场景切片、资源路径
```

然后：

1. 建立自己的宿主（窗口、D3D11、ImGui 帧循环），实现并注入 `HostApi`；
2. 复用 `framework/render/imgui/` 提供的 `RenderApi` 与 `CtrlApi`，或按 `Render.h` 另写一套；
3. 在自己的应用层写事件绑定表，把文档里的 `event` 名接到处理函数；
4. 准备一份布局文档，其中必须含一份结构件齐全的布局（`WND` / `TITLE` / `CNTR`）。

框架层不含文档编辑、保存、预览这些语义，这些都由应用层自行决定。

## 2. 连编辑器一起派生

复制整个 `src/`，然后按顺序改：

| 项 | 位置 | 说明 |
| :--- | :--- | :--- |
| 应用名 | 工程属性、窗口标题、`docs/examples/*.json` 的 `label` | 显示用 |
| 文档文件名 | `app/Application.cpp` 的加载链、`app/DocumentSource.cpp` | 默认 `GenUI.bin` / `GenUI.json`，可换成自己的名字 |
| 外壳布局保留名 | `app/Application.h` 的 `GENUI_SHELL_LAYOUT_NAME` | 缺省 `GenUIEditor`；文档里必须有同名布局 |
| 事件绑定表 | `editor/GenEditor.cpp` 的 `GetEventBindings` | 文档 `event` 名到处理函数的映射 |
| 内置资源 ID | `app/resource.h` | 101 / 102，类型串 `JSON` / `BIN` |
| 编辑器界面 | 文档里的 `GenUIEditor` 布局 | 增删控件只改文档，不改代码 |

## 3. 建议改动

- 删掉不需要的控件类型：从 `ControlSpec.h` 的类型表删一项，同步删控件库的绘制分派与缓冲容量。
- 换配色：`app/AppHost.cpp` 里的投影配色参数（底色端 / 光色端 / 层数 / 采样函数）。
- 换图标与快捷键：`editor/GenEditor` 的事件处理函数与 `app/AppHost` 的窗口消息。
- 加自己的命令：在 `editor/EditorCommands` 加命令，在绑定表登记事件。

## 4. 派生后自证

至少验证：

1. Release 与 Debug 两种配置均 0 警告 0 错误；
2. 把文档放到 exe 同目录，无人值守启动（`GENUI_AUTOCLOSE_MS`）退出码 0；
3. 加载链打印的命中级别符合预期；
4. 外壳布局缺少时能明确失败，不静默空跑；
5. 若是编辑器形态：增删控件、改几何、保存后重新打开，界面不漂移。

## 5. 不要改的部分

- 布局文档的字段名与 bin 记录布局（改了就与其他文档不兼容）。
- `CtrlInfo` 的尺寸与关键字段偏移（有编译期断言守着，改字段必须同轮核对按 `sizeof` / `offsetof` 工作的代码）。
- 控件类型串的拼写（文档按字符串精确匹配）。

## 6. 常见错误

| 现象 | 原因 |
| :--- | :--- |
| 启动即退出，退出码 1 | 文档里没有 `GENUI_SHELL_LAYOUT_NAME` 对应的布局 |
| 界面一片空白 | 内容控件没有 `parent` 指向容器，或容器高度系数算成 0 |
| 控件点不动 | `event` 名没有出现在绑定表里 |
| 改 JSON 无效 | exe 同目录存在旧 `GenUI.bin`，加载链优先 bin |
| 派生后文档读不出 | 仍按旧文件名探测加载链，但磁盘上放的是新名字 |
