# EditorCommands —— 接口文档

> 模块：`src\editor\EditorCommands.{h,cpp}`（全静态类；构造/析构私有化 ⇒ 进程级单例语义）
> 层归属：**editor 层**（编辑器命令层）—— 与同层的 `GenUIEditor`（外壳交互）、`SelectionOverlay`（选中态叠加件）、
> `LayoutWriter`（写方向序列化）相邻但职责不同：本模块只承载**文档编辑命令**。
> 定位：**编辑命令的落点** —— 把"改中间态元素链 / 条目数据"的命令自应用主类搬到编辑层，使命令不依赖 `app` 层。

---

## 1. 职责与边界

### 1.1 根本职责（当前口径）

1. **承载者接入**: 持宿主注入的 `doc\Document*`（`SetDocument`；注入链路 = 宿主 → `GenUIEditor::SetDocument` → 本模块转发）。
2. **元素规格族命令**: 取值（`SpecAt`）· 插入（`InsertSpecAt`）· 移除（`RemoveSpecAt`）——
   判据本体归 `doc\Document`（`SpecAt` / `InsertSpecAt` / `RemoveSpecAt`），本模块负责**布局→文档索引的反查**与降级口径。
3. **布局族命令**: 改名（`SetDocLayoutName`）· 追加（`AddDocLayout`）·
 删除（`RemoveDocLayout`）· 重排（`MoveDocLayout`）—— **自应用主类（含其全部拒绝分支与返回码；
   `AddDocLayout` 的"元素链操作失败**不回滚**"与 `MoveDocLayout` 的 **9 处裸 `return`** 属**有意保留**，不得顺手修）。
   会话面副作用（预览唯一基座 / 预览同步 / 文档版本号 / 条目节点重挂）不自行实现 ⇒ 见 §3.3。

### 1.2 边界（本模块**不做**的事）

- **不持会话面状态**: 预览唯一基座 / 外壳快照 / 内嵌成员 / 文档版本号归应用侧；
  需要时经**注入回调**（`SessionHooks`）驱动，不把会话状态搬进本模块。
- **不做渲染 / 交互**: 归 `framework\render\Render.h` + `editor\GenUIEditor`。
- **不做外壳布局**: 归 `GenUIEditor`。
- **不做控件缓冲的构造 / 回收**: 归 `framework\base\CtrlBuffer.h` 与 `framework\layout\LayoutBuilder.h`。
- **依赖方向**: `editor → doc / framework`；不引 `app/` 头。

## 2. 公开接口

| 接口 | 签名 | 说明 |
|---|---|---|
| `SetDocument` | `VOID SetDocument(Document* pDocument)` | 注入文档承载者（`NULL` = 复位）。未注入 ⇒ 全部命令**显式降级**（`FALSE` / `NULL`），不制造空态替身 |
| `SetSessionHooks` | `VOID SetSessionHooks(pfnEnsurePreviewBase, pfnSyncPreviewLayout, pfnBumpDocRev)` | 注入**会话面回调三件套**（与承载者同点；三者可分别传 `NULL` ⇒ 该项不驱动）。布局族命令改完中间态后经它们**原位**请求"取/建预览基座 · 预览同步 · 版本号递增" |
| `SetDocLayoutName` | `BOOL SetDocLayoutName(DWORD docIndex, LPCSTR name)` | 写指定布局条目的显示名（label 与 name **同写**：中间态 `pNode->info` 与条目副本两处）；空串 / 空指针 / 索引越界 / 条目节点缺失 ⇒ `FALSE` |
| `AddDocLayout` | `BOOL AddDocLayout(LPCSTR name)` | 追加一个布局（`WND`/`TITLE`/`CNTR` 三结构件模板 + 条目数组扩容，新布局随即 active）；`name` 空 ⇒ 默认名 `新布局`。与一致：元素链操作失败**不回滚** |
| `RemoveDocLayout` | `BOOL RemoveDocLayout(DWORD index)` | 删除指定布局（文档**保底 1 个**；删除激活项时激活切回 0）；数组整体搬迁 + 条目节点经 `Document::RebindNodes` 重挂 |
| `MoveDocLayout` | `BOOL MoveDocLayout(DWORD dwFrom, DWORD dwDest)` | **拖拽重排布局**：逐布局深拷 → 清空中间态布局链 → 按新序重建 + 条目数组同序重排（激活索引跟随）；`dwDest` = **删除源之后**的插入索引 |
| `SpecAt` | `ElementSpec* SpecAt(Layout* layout, size_t index)` | 取指定布局内某**文档索引**的条目中间态元素规格（非条目 / 未挂接 / 索引越界 ⇒ `NULL`） |
| `InsertSpecAt` | `BOOL InsertSpecAt(Layout* layout, size_t index)` | 在指定布局内插入一枚元素规格（与控件插入同步；新元素以当前像素表达几何） |
| `RemoveSpecAt` | `BOOL RemoveSpecAt(Layout* layout, size_t index)` | 移除指定布局内一枚元素规格（与控件删除同步） |

> 私有件：`DocIndexOfLayout(Layout*)` —— 布局指针 → 文档索引反查（未命中 ⇒ -1）。

## 3. 关键契约

### 3.1 索引反查（**唯一口径**）

- **预览唯一基座被全部条目共享** ⇒ 反查对基座一律归属**当前 active 条目**；其余布局按条目数组线性反查。
- **与承载者的 `FindByLayout` 刻意不同**：后者是"仅条目数组"的线性反查，对基座会命中 **0 号条目**
  —— 而编辑命令改的是 **active 条目的中间态元素链** ⇒ 必须用本模块的反查。

### 3.2 注入时序

- 承载者由宿主在 `Application::Init()` **成功之后**单点注入，经 `GenUIEditor::SetDocument` 转发到本模块。
  该时序与既有三处注入（事件绑定表 / 点击处理器 / 窗口句柄）**相反** —— 理由见 `src\app\AppHost.md` §3.1。
- 复位 = `SetDocument(NULL)`，须**早于** `Application::Release()`。

### 3.3 会话面驱动（**只请求、不实现**）

- **为什么注入**: 本模块刻意**不持**会话面状态（预览唯一基座 `pPreviewBase` / 外壳快照 / 内嵌成员 / 文档版本号
  `nDocRev` —— 见 `src\doc\Document.md` 的模块边界）；而布局族命令改完中间态后**必须**驱动它们。
  ⇒ 一律经 `SetSessionHooks` 注入的回调**原位**请求（`pfnEnsurePreviewBase` / `pfnSyncPreviewLayout` / `pfnBumpDocRev`，
 类型见 `src\framework\base\FuncPtr.h`；三者由 **`app\PreviewSession`** 实现）；
  **未注入 ⇒ 显式降级**（跳过该项驱动，不伪造替身）。
- **位置口径**: 回调调用点与在命令内的**相对位置逐字保持一致**（例如版本号递增仍在"两处同写之后、`return` 之前"）。
- **条目节点重挂**: 不属会话面 —— 直接经承载者的等价原语 `Document::RebindNodes`。
- **反向接线（应用侧 → 命令层）**: `Application::NewDocument` 的"新建默认布局"一步需要 `AddDocLayout`，而应用侧
  不得引编辑器头 ⇒ 由宿主在**同一注入点**把命令地址交给应用侧（`Application::SetDocLayoutAdder`；
  类型 = `framework\base\FuncPtr.h` 的 `pfnAddDocLayout`）。本模块**不参与**该接线，仅提供静态命令入口。

## 4. 失败语义

| 情形 | 行为 |
|---|---|
| 承载者未注入 | 命令返回 `FALSE` / `NULL`（显式降级，静默） |
| 布局非文档条目 | 反查失败 ⇒ `FALSE` / `NULL` |
| 中间态元素链操作失败 | 返回 `FALSE`（由承载者判定） |
| 会话回调未注入 | **跳过**该项会话面驱动（预览基座 / 预览同步 / 版本号）—— 数据面改动照常落定，返回码不受影响 |

## 5. 相关

- 文档承载者: `src\doc\Document.h` + `Document.md`
- 编辑器: `src\editor\GenEditor.h`（外壳交互；承载者注入入口 `SetDocument`）
- 会话面回调类型: `src\framework\base\FuncPtr.h`（`pfnEnsurePreviewBase` / `pfnSyncPreviewLayout` / `pfnBumpDocRev` / `pfnAddDocLayout`）
- 接线（宿主）: `src\app\AppHost.cpp`（承载者 + 会话回调 + 命令面回调同点注入 / 复位）
