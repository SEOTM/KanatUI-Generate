# LayoutSolver 模块 (src\framework\layout\LayoutSolver.h)

## 概述

布局求解器的读方向:把 JSON 或 bin 文档解析为中间态(`LayoutModel` 的 `FileMidState`), 并在解析期把
逐字段声明写进元素规格与布局信息。几何解算不在此处发生 —— 本模块只产出未解算的系数声明。

本模块属 **framework 层**, 工程内只依赖 `LayoutModel`(中间态类型与块链操作)与 `FileCodec`(CRC32); 只负责
**读方向**, 不依赖写方向模块, 也不负责释放中间态持有的块(块的回收归 `LayoutModel::StateClear`)。
bin 的字节布局、版本号位域与规模上限声明在 `LayoutModel.h` 的格式契约区, 本模块只按该契约校验与取用。

本模块包含以下编程接口:

### 类

| 类 | 说明 |
| --- | --- |
| `LayoutSolver` | 文档读入器: 持有一份中间态, 把文件或内存缓冲解析进来。 |

本模块不定义公开类型与公开常量。

---

## LayoutSolver 类 (LayoutSolver)

文档读入器。

### 语法

```cpp
class LayoutSolver
{
public:
    LayoutSolver(Arena* arena);
    ~LayoutSolver();

    BOOL LoadLayouts(LPCSTR filename);
    BOOL LoadLayouts(LPVOID data, DWORD size);
    DWORD GetLayoutsCount();
    CONST LayoutInfo* GetLayout(DWORD index);
    CONST FileMidState& GetState();

    BOOL LoadLayoutsBin(LPCSTR filename);
    BOOL LoadLayoutsBin(LPCVOID data, DWORD size);
    static BOOL IsBinDocument(LPCVOID data, DWORD size);

    BOOL LoadLayoutsAuto(LPCSTR filename);
    BOOL LoadLayoutsAuto(LPCVOID data, DWORD size);
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `LayoutSolver(Arena*)` | 记录承载中间态的线性分配器, 计数清零, 中间态置空。 |
| `~LayoutSolver()` | 只把分配器指针与中间态置空, 不释放任何块。 |
| `LoadLayouts(LPCSTR)` | 内存映射打开 JSON 文件并转交内存入口解析。 |
| `LoadLayouts(LPVOID, DWORD)` | 从内存缓冲解析 JSON, 产物 = 中间态。 |
| `GetLayoutsCount()` | 取当前中间态的布局数量。 |
| `GetLayout(DWORD)` | 按索引取中间态内的布局信息; 越界返回 `NULL`。 |
| `GetState()` | 取整份中间态(布局链 + 元素链)的引用, 不复制。 |
| `LoadLayoutsBin(LPCSTR)` | 读入整个 bin 文件并转交内存入口解析。 |
| `LoadLayoutsBin(LPCVOID, DWORD)` | 从内存缓冲解析 bin, 产物 = 中间态。 |
| `IsBinDocument(LPCVOID, DWORD)` | 嗅探缓冲的文档形态: `TRUE` = bin, `FALSE` = JSON。 |
| `LoadLayoutsAuto(LPCSTR)` | 嗅探文件形态后分派到 JSON 或 bin 入口。 |
| `LoadLayoutsAuto(LPCVOID, DWORD)` | 嗅探内存缓冲形态后分派到 JSON 或 bin 入口。 |

### 注解

**构造与析构**。`LayoutSolver(Arena*)` 只记录分配器指针, 计数清零, 中间态置为空态; `arena` 可为
`NULL`, 此时所有分配类入口一律失败(与"未注入分配器"同口径)。析构**不释放**分配器内存, 也不回收
中间态的块: 只把分配器指针与中间态置空(块随 `Arena` 整体释放)。

**中间态的所有权**。实例持有一份 `FileMidState`; 解析成功后块(布局块 + 元素块)归 `Arena` 承载,
由调用方经 `LayoutModel::StateClear` 回收。重复解析时本模块会先 `StateClear` 旧中间态再建新的;
`GetState()` 返回的是内部中间态的**引用**(不复制), 该引用的有效期到下一次解析或实例析构。

**JSON 读入**。两趟结构: 第一趟校验结构并数出各布局的元素数, 第二趟按该数量建立中间态并逐字段填充。

- 入口守卫: `data` 为 `NULL` 或分配器未注入 ⇒ 直接返回 `FALSE`(不解析、不诊断)。
- 缓冲须以 `'\0'` 结尾(解析器按 C 串读入); 长度参数不参与 JSON 解析。
- 必需结构: 顶层 `layouts` 数组(缺失或非数组 ⇒ 拒绝), 段数须在 `1..LAYOUT_MAX`; 每个布局须是对象
  且含 `ctrls` 数组, 控件数不得超过 `CTRL_MAX`; 元素总数过大导致尺寸溢出 ⇒ 拒绝。
- 布局级字段: `width` / `height` 因子**必需**(缺失或不合法即拒绝整份加载); `name` / `label` 为定长
  字段(超长截断并诊断); `IsMaxiable` / `IsResizeable` 缺省 `FALSE`; `wndX` / `wndY` 缺省 `0`;
  `autosize` 缺失时整组参考量取缺省样例。
- 控件级字段: `type` **必需**为字符串; `id` 缺失时按数组下标兜底并诊断; `parent` 为可选的父容器 ID
  (缺省 `-1` = 无父容器, 负数一律归 `-1`, 本模块只存不校验); `label` / `text` / `event`
  缺省为空串(定长字段, 超长截断并诊断); `x` / `y` / `width` / `height` 因子**必需**。
- 尺寸因子: 须为对象, 含 8 项数字的 `coeff` 数组与整数 `offset`; 缺项或非数字按 `0` 处理并诊断,
  整体仍判为不合法 ⇒ 拒绝。
- 参考量(`autosize`): `mode` 可为模式串或整数索引(串未命中 / 索引越界回落 `0`), `unit` 只接受
  `0` / `1`, 六项千分比取值钳到 `0..10000`。
- 失败一律返回 `FALSE`, 逐条打印诊断并在返回前冲刷标准输出; 中间态可能停留在"已建链、内容为
  零值"的半成品状态(调用方不得把它当作有效文档)。

**bin 读入**。与 JSON 同构(先校验并数数, 再建中间态并填充), 差别是记录区整块 `memcpy`、校验更严:

- 文件入口: 路径为空 / 打开失败 / 文件大小为 0 / 读不全 / 缓冲分配失败 ⇒ `FALSE`, 打印 `[BIN]`
  诊断; 读全后转交内存入口。
- 内存入口: `data` 为 `NULL` 或分配器未注入 ⇒ 静默 `FALSE`; 长度须不小于文件头
  `BIN_FILE_HEAD`(校验码 4 + 版本号 4 + 段数量 4)。
- 校验码: 对整个文件算 CRC32, **校验码字段自身按 0 参与**(先推进 4 个零字节, 再推进其余正文; 不改
  调用方缓冲); 与文件内记录值不符 ⇒ 拒绝。算法在 `FileCodec`, 本模块只调用。
- 版本号: 高 24 位代际版本号须与 `BIN_VERSION` 的代际一致; 低 8 位格式标记须为 `0`(压缩 / 加密形态
  尚未实现) ⇒ 任一不满足即拒绝并指出形态。
- 段数须在 `1..LAYOUT_MAX`; 逐段校验: 段头须可读, 本段大小须容得下段头加头部区、不得越出文件,
  余量须恰为 `BIN_CTRL_SIZE` 的整数倍(不整除 ⇒ 拒绝); 头部区"控件数量"超过 `CTRL_MAX` ⇒ 拒绝;
  头部声明与实际容纳枚数不一致时**按较小值取用并诊断**(不拒绝); 段区总长须正好铺满文件。
- 填充: 头部区整块 `memcpy` 一枚 `LayoutInfo`, 控件区逐枚整块 `memcpy` 一枚 `ElementSpec`(顺序 =
  元素链顺序); 中间态的 `dwElements` 以**实际加载枚数**为准; 每个定长字符串字段的末字节置 `0`
  兜底(容忍外部构造出的缺结尾符字段); `parentID` 为负数的记录一律归 `-1`。
- 失败一律返回 `FALSE`, 打印 `[BIN]` 诊断并冲刷标准输出。

**形态嗅探与分派**。`IsBinDocument` 跳过可选的 UTF-8 BOM(`EF BB BF`)与前导空白(空格 / 制表 /
回车 / 换行)后看首字节: `{` ⇒ 按 JSON(`FALSE`), 其余 ⇒ 按 bin(`TRUE`); 入参为空、长度 `0` 或内容
全为空白 ⇒ 按 JSON。bin 无魔数, 该判据是"内容排除法"。

`LoadLayoutsAuto` 只额外读文件头 16 字节用于嗅探, 解析仍走两条既有入口 ⇒ 解析行为与诊断口径不变;
文件打不开或读不到时按 JSON 形态交给 `LoadLayouts`, 由它维持原有的失败语义。内存版本中 bin 用显式
长度, JSON 仍要求 `'\0'` 结尾。

**查询与失败口径**。`GetLayoutsCount()` 返回当前中间态的布局数量(解析失败后可能仍停留在上一次成功
的值); `GetLayout(DWORD)` 的下标从 `0` 起, 越界返回 `NULL`, 返回指针归中间态所有(不得释放, 增删
布局后可能失效); `GetState()` 不复制, 多个副本共享同一批块。全部入口以返回码表示结果, 不抛异常。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/layout/LayoutSolver.h`(`src\framework\layout\LayoutSolver.h`) |
| 实现 | `src\framework\layout\LayoutSolver.cpp` |
| 命名空间 | 全局(类) |
| 依赖 | `framework/layout/LayoutModel.h` · `framework/base/util/FileCodec.h` · `framework/base/mem/MemoryManager.h` · `document.h` · `kernel32.dll` · `ucrt` |
| 平台 | Windows 全版本 |
| 线程安全 | 否(实例持有中间态与计数, 无内部同步); 同一实例须由单线程使用 |
