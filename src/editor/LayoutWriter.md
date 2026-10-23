# LayoutWriter —— 接口文档

> 模块：`src\editor\LayoutWriter.h` / `src\editor\LayoutWriter.cpp`（全静态工具类，私有化构造/析构）
> 层归属：**editor 部件**（非框架件）—— 写方向只服务"可自举 UI 设计程序"这一用法，
> 未来**框架复用时不含本模块**（框架只含布局 / 控件 / 渲染 / 事件与读方向解析）。
> 定位：**写方向**序列化 —— 把中间态（`FileMidState`）生成**规范 JSON 文本**或 **bin 字节流**，并写盘（备份 + 临时文件 + 原子替换）。
> 对读方向模块 = `LayoutSolver`（文件 → 中间态，含 bin 解析）；两者共用同一中间态定义与**同一套 bin 格式契约**
> （契约定义在 `src\framework\layout\LayoutModel.h` 的"bin 文件格式契约"区）⇒ **彼此无依赖**；校验码算法取 `src\framework\base\util\FileCodec.h`（CRC32，流式三件套）。

## 1. 公开接口

| 接口 | 签名 | 说明 |
|---|---|---|
| `BuildJsonText` | `static BOOL BuildJsonText(CONST FileMidState& state, LPSTR* ppszText, size_t* pcbText)` | 生成规范 JSON 文本。内部两趟（先计数再分配写入）；`ppszText` 出参由**调用方** `delete[]` 释放；失败置 `NULL`/0 |
| `SaveToFile` | `static BOOL SaveToFile(CONST FileMidState& state, LPCSTR szPath)` | 生成规范 JSON 文本后写盘（内部转调 `SaveRawToFile`） |
| `SaveRawToFile` | `static BOOL SaveRawToFile(CONST BYTE* pData, size_t cbData, LPCSTR szPath)` | **通用字节写盘原语**：把一段内存字节按 §3 的"备份 → 同目录临时文件 → 原子替换"写入；**JSON 与 bin 两条写盘链共用** |
| `BuildBinBytes` | `static BOOL BuildBinBytes(CONST FileMidState& state, LPBYTE* ppBytes, size_t* pcbBytes)` | 把中间态生成 **bin 字节流**（末尾回填 CRC32 校验码：覆盖整个文件，校验码字段按 0 参与）；`ppBytes` 由**调用方** `delete[]`；失败置 `NULL`/0 |
| `SaveBinToFile` | `static BOOL SaveBinToFile(CONST FileMidState& state, LPCSTR szPath)` | 生成 bin 字节流并写盘（内部转调 `SaveRawToFile`） |

**bin 格式契约（单一真源 = `src\framework\layout\LayoutModel.h` 的"bin 文件格式契约"区，见其接口文档 §1.9）**：
版本号 `0x00000200`（高 24 位代际 = 2，低 8 位格式标记 = 0 明文；压缩 / 加密尚未实现）/ 文件头 12 B（校验码 + 版本号 + 段数量）/
段头 4 B（本段大小，**含本字段自身**）/ 头部区 = `sizeof(LayoutInfo)`（**定长，无"头部大小"字段**）/ 控件记录 = `sizeof(ElementSpec)` /
段大小公式 `BinSegBytes(N)` / 上限 `LAYOUT_MAX=1024`、`CTRL_MAX=8192`（JSON 与 bin 共用）。
**记录区 = 中间态结构体的内存镜像**（头部区 = 一枚 `LayoutInfo`，其末 4 B 即"控件数量"；每控件 = 一枚 `ElementSpec`，
其中 `parentID` 字段偏移 `532`、记录总长 `536` B），读写两侧**整块 `memcpy`**，不做逐字段解析/编码 ⇒ 字节序随编译目标，文件不另作声明。
**校验码算法归 `FileCodec`**（本模块只按契约调用 `FileCodec::Crc32`）。格式与校验规则的公开说明见 `docs/LAYOUT-FORMAT.md` 第 9 节。

**数据真源 = 中间态**：布局链（`LayoutModel::LayoutFirst/LayoutNext`）与元素链（`ElementFirst/ElementNext`）即文件顺序；
`LayoutInfo`/`ElementSpec` 与文件字段一一对应 ⇒ 不需要任何旁挂表（`type`/`text`/`event` 的原串保存在元素规格里）。

## 2. 规范 JSON 文本格式 v1

| 项 | 规则 |
|---|---|
| 编码 / 换行 | UTF-8 **无 BOM**；**LF**；文件末尾带一个换行 |
| 缩进 | 2 空格 / 级（不使用 Tab） |
| 顶层 | `{ "layouts": [ … ] }` |
| 布局键序 | `name` `label` `IsMaxiable` `IsResizeable` `wndX` `wndY` `width` `height` `autosize` `ctrls` |
| 因子对象 | **单行**：`"key": { "coeff": [c0, c1, …, c7], "offset": N }` |
| `autosize` | **多行**，键序 `mode` `unit` `width` `height` `controlHeight` `fontSize` `space` `padding` |
| 控件对象 | **多行**，键序 `id` `parent` `type` `label` `text` `event` `x` `y` `width` `height`；`parent` 恒写出（无父容器写 `-1`） |
| 数字 | 整值（含 `FLOAT` 整数值，绝对值 < 1e8）写整数形式；其余取**最短往返十进制**（`std::to_chars` 的 FLOAT 重载）；`-0.0f` 归一为 `0` |
| 字符串 | 转义 `"`、`\`、`\n`/`\r`/`\t` 与 `< 0x20` 控制符；UTF-8 多字节（中文）按字节原样 |

**不变式**：对由本写出器产出的文件（或与工程源 `GenUI.json` 同款排版的文件），`载入 → BuildJsonText` 输出与源文件**逐字节相同**。

## 3. 写盘行为（`SaveRawToFile`；`SaveToFile` 生成规范 JSON 文本后转调它）

1. **备份**：目标已存在且 `<目标>.bak` 不存在 ⇒ `CopyFileA(目标, 目标.bak, FALSE)`（**不覆盖**既有备份；备份失败只打印诊断，不阻断写入）。
2. **正文**：写同目录 `<目标>.tmp`（写满校验）→ `FlushFileBuffers`。
3. **替换**：`MoveFileExA(.tmp, 目标, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` ⇒ 原子替换。
4. **失败**：删除残留 `.tmp`，打印 `[APP] 保存失败: …(GetLastError=…)`，返回 FALSE（**不弹窗**）。
5. **成功**：打印 `[APP] 保存: <路径> (<N> 字节)`。

## 4. 调用方式

```cpp
// 应用侧(整份文档写盘; 工程源文档路径被拒)
Application::SaveDocument(szPath);

// 只取文本(不写盘): 调用方负责 delete[]
LPSTR lpText = NULL;
size_t cbText = 0;
if (LayoutWriter::BuildJsonText(Application::GetDocState(), &lpText, &cbText))
{
	/* 使用 lpText / cbText */
	delete[] lpText;
}
```

**工程源保护（按加载路径）**：`Application::SaveDocument` 在 **当前文档来自 `.\GenUI.json` 加载路径**（`Application::GetDocPath()` == exe 目录 `GenUI.json`）
且保存目标就是该文件时**拒绝写盘**并打印 `[APP] 保存被拒: …`（判据 = `Application::IsProtectedSavePath`，`IsEngineSourcePath` 只判路径本身）；
GUI 入口（`GenUIEditor::OnSave`）在此之上把该情形转为"另存为"对话框。文档来源为其它文件 / 内置资源 / 新建时**不保护**（目标为该文件也允许写盘）。

## 5. 边界与失败语义

| 情形 | 行为 |
|---|---|
| `state` 无布局 | 输出 `{\n  "layouts": [\n  ]\n}\n` |
| 中间态字符串被读方向截断（`name` 64 / `label` 128 / `type` 64 / `text` 128 / `event` 64 / `mode` 64） | 按中间态现值写出 ⇒ **含超长串的源文件"载入 → 保存"不逐字节**（截断在载入期发生并打印 `[JSON]` 诊断） |
| 文件里存在中间态未承载的字段 | **不保留**（保存后丢失）⇒ 新增文件字段必须同时进 `LayoutInfo`/`ElementSpec` |
| 目标路径过长（`<路径>.tmp` 超出 `MAX_PATH`） | 失败并诊断，原文件不变 |
| 目标被占用 / 只读 / 替换失败 | 失败并诊断（`GetLastError`），原文件不变，`.tmp` 已清理 |
| 内存不足（文本缓冲分配失败） | 返回 FALSE |
| bin: 段数为 0 或 > `LAYOUT_MAX` / 某段控件数 > `CTRL_MAX` | **生成中止**并诊断（`[BIN] …: 生成中止`）；不产出字节流 |
| bin: 中间态元素链与 `dwElements` 不一致 | 失败（返回 FALSE，已分配缓冲释放）—— 写侧按 `dwElements` 反算段大小，两侧必须自洽（读侧已把 `dwElements` 归一到实际加载枚数） |

## 6. 相关

- **bin 写方向即本模块**：`BuildBinBytes`（中间态 → bin 字节流）+ `SaveBinToFile`（生成后转调 `SaveRawToFile`）。
  **读方向**（`LayoutSolver::LoadLayoutsBin` / `IsBinDocument` / 形态分派 `LoadLayoutsAuto`）**不依赖本模块** ——
  两侧共用 `LayoutModel` 的格式契约与 `FileCodec` 的校验算法。
  **接入面**：`Application::SaveDocument` 按目标扩展名分派（`.bin` ⇒ `SaveBinToFile`，其余 ⇒ `SaveToFile`）；
  加载链第 3 级内嵌 bin 资源（`IDR_GENUI_BIN`）为可选：构建期可由工程源生成同源 bin（字段偏移与本模块格式一致）；
  未内嵌时由第 1 / 2 级外部文件提供文档。
  格式与校验规则的公开说明见 `docs/LAYOUT-FORMAT.md` 第 9 节。
  **bin 编解码不另立文件**：读方向落 `LayoutSolver`（与 JSON 读同文件），写方向落本模块；格式契约落 `LayoutModel`，校验算法落 `FileCodec`。
