# Document 模块 (src\doc\Document.h)

## 概述

`Document` 是"当前文档"的数据面承载者: 一份布局文档的条目表(条目数组指针 + 计数 + 激活索引)、
中间态、关联磁盘路径与未保存改动标记, 以及建立在条目与中间态之上的数据访问原语。

层归属是文档层, 不属于可复用框架: 只有"同时是编辑器"的工程才需要它。依赖方向为 `doc → framework`
单向, 本模块只经框架层的现有公开接口取类型与数据, 不包含应用层或编辑器层的头。

边界: 不含会话面状态(文档版本号、预览基座、外壳快照、内嵌成员一律归会话侧); 不含窗口、视图与渲染;
不含文件格式与保存策略; 自身不做任何堆分配(只持指针与计数, 存储由持有者提供); 不编排条目归属
(按布局指针反查只覆盖文档条目数组这一段)。

本模块包含以下编程接口:

### 类型

| 类型 | 说明 |
| --- | --- |
| `LayoutEntry` | 文档中的一个布局条目(布局对象、参考尺寸、旁挂表、中间态节点、窗口几何、窗口标志)。 |
| `Document` | 当前文档的承载者本体(恰六个字段)与数据访问原语。 |

### LayoutEntry 结构 (Document)

文档中的一个布局条目。

### 语法

```cpp
struct LayoutEntry
{
    Layout*    layout;
    AutoSize   ref;
    RefSize    refSize;
    LPCSTR     name;
    LPCSTR     label;
    LPCSTR*    aRawType;
    LPCSTR*    aRawText;
    DWORD      qwRawType;
    DWORD      qwRawText;
    LayoutNode* pNode;
    LONG       wndX;
    LONG       wndY;
    DWORD      wndW;
    DWORD      wndH;
    BOOL       IsMaxiable;
    BOOL       IsResizeable;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `layout` | 条目布局对象; 内容随当前激活布局装配。 |
| `ref` | 布局内部参考, 其窗口宽高取该布局窗口的客户区。 |
| `refSize` | 本布局的参考尺寸与经验参数(中间态字段)。 |
| `name` / `label` | 布局名与布局标签, 均为会话内存里的字符串。 |
| `aRawType` / `aRawText` | 文档原类型串旁挂表、无缓冲控件的声明文本旁挂表, 均按文档索引, 池内自持。 |
| `qwRawType` / `qwRawText` | 上述两张表的容量(等于建表时的文档控件数)。 |
| `pNode` | 中间态布局节点(元素规格与几何系数)。 |
| `wndX` / `wndY` | 布局窗口的桌面位置(像素)。 |
| `wndW` / `wndH` | 布局窗口的客户区宽高(由 autosize 千分比乘参考基准除 1000 得来)。 |
| `IsMaxiable` / `IsResizeable` | 布局窗口是否可最大化、是否可调整大小, 随文件保存。 |

### 注解

控件几何是派生值: 依 `ref`(窗口宽高取该窗口客户区)对中间态元素的几何系数求解。系数真源恒为中间态
(`pNode` 所指的元素规格), 像素几何只是解析产物。

### Document 类 (Document)

当前文档的承载者本体。

### 语法

```cpp
class Document
{
public:
    LayoutEntry* FindByLayout(Layout* layout);
    static ElementSpec* SpecAt(LayoutEntry& entry, size_t index);
    VOID MarkDirty();
    VOID ClearDirty();

    DWORD GetCount();
    DWORD GetActive();
    VOID SetActive(DWORD index);
    LayoutEntry* GetEntry(DWORD index);
    FileMidState& GetState();
    LPCSTR GetPath();
    VOID SetPath(LPCSTR path);
    BOOL IsDirty();
    BOOL LayoutLabelAt(DWORD index, LPSTR buf, size_t cap);
    VOID RebindNodes();
    BOOL InsertSpecAt(DWORD docIndex, Layout* layout, size_t index);
    BOOL RemoveSpecAt(DWORD docIndex, size_t index);

    LayoutEntry* entries;
    FileMidState docState;
    DWORD        dwEntries;
    DWORD        dwActive;
    LPCSTR       lpDocPath;
    BOOL         bDocDirty;
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `entries` | 文档布局条目数组; 内存由会话分配器持有, 本结构只持指针。 |
| `docState` | 文档中间态(布局链 + 元素池), 是编辑器改动的唯一真源。 |
| `dwEntries` | 布局条目数量。 |
| `dwActive` | 当前激活(编辑与预览)布局的索引。 |
| `lpDocPath` | 当前文档关联的磁盘路径(池内); 空表示无关联文件。 |
| `bDocDirty` | 是否有未保存改动; 载入、新建、保存成功时清零。 |
| `FindByLayout(layout)` | 按布局指针在文档条目数组内线性反查条目。 |
| `SpecAt(entry, index)` | 静态; 取该条目中间态内指定索引的元素规格。 |
| `MarkDirty()` / `ClearDirty()` | 置脏 / 清脏原语, 语义等同直接给 `bDocDirty` 赋值。 |
| `GetCount()` | 条目数量。 |
| `GetActive()` / `SetActive(index)` | 读 / 写激活索引。 |
| `GetEntry(index)` | 取指定索引的条目地址。 |
| `GetState()` | 取文档中间态引用(非副本)。 |
| `GetPath()` / `SetPath(path)` | 读 / 写关联磁盘路径。 |
| `IsDirty()` | 是否有未保存改动。 |
| `LayoutLabelAt(index, buf, cap)` | 取指定条目的显示名拷入调用方缓冲。 |
| `RebindNodes()` | 把全部条目的中间态节点按索引重新挂接到当前中间态布局链。 |
| `InsertSpecAt(docIndex, layout, index)` | 在指定布局的文档索引处插入一枚元素规格。 |
| `RemoveSpecAt(docIndex, index)` | 在指定布局的文档索引处移除一枚元素规格。 |

### 注解

`FindByLayout` 只在条目数组内线性反查: 布局指针为空或条目数组未挂接时返回空; 遍历未命中亦返回空;
不抛异常; 复杂度为条目数的线性, 不做缓存(条目数组随文档结构变更整体重置, 缓存必然失效)。其覆盖范围
只是条目数组这一段, 会话面的其它条目来源由调用方自行编排。

`SpecAt` 只沿条目节点的元素链走指定步数, 不判几何系数是否有效(那是解算方的事); 条目节点为空或链长
不足时返回空。它是静态成员, 只持条目的调用方不必持有 `Document` 实例。

脏标记两函数是纯数据操作: 不写盘、不弹窗、不触发同步。何时置脏、何时清脏、何时提醒都属调用方策略。

八个存取原语(条目数、激活索引读写、取条目、中间态引用、路径读写、脏标记读)都是纯字段存取, 不含越界
钳制、版本号递增、预览同步、路径池化等策略。`GetEntry` 的边界就是"条目数组未挂接或索引越界返回空",
它等价于直写判据的封装。

`LayoutLabelAt` 的显示名优先取标签, 标签为空时回落布局名。缓冲为空或容量为零时返回失败且不写入; 条目
越界、条目无中间态节点或节点无布局信息时返回失败并把缓冲写为空串; 命中时按截断式拷贝写入并返回成功,
因此调用方无需预先清空缓冲。

`RebindNodes` 逐条把条目的中间态节点重新指向当前中间态里对应索引的布局, 未命中则置空; 它只改条目的
挂接关系, 不动中间态本体。条目数组未挂接时直接返回。

`InsertSpecAt` 与 `RemoveSpecAt` 就地修改中间态元素链(其后元素索引整体后移或前移), 随即按索引对齐条目
节点; 中间态操作失败返回失败, 成功返回成功。插入时新元素的几何以当前像素表达(系数留零、全部落偏移,
控件 ID 取同索引控件), 取不到同索引控件时几何字段保持原样但仍返回成功。两者都只动中间态元素链:
不回收内存、不判控件类型, 与控件增删的配对、随后的预览同步与脏标记都由调用方负责。

两个增删函数只接收调用方已解析好的布局条目索引, 本模块不做"布局指针 → 条目索引"的反查: 预览基座被
全部条目共享, 用指针反查会命中首条目, 不能据此定位中间态元素链, 该反查必须留在持会话语义的一侧。

索引口径: 本模块全部索引参数一律解释为文档索引, 与中间态元素链、旁挂表、布局控件索引同一口径。
所有权口径: 本模块不拥有任何内存(条目数组、旁挂表、池内字符串、中间态块与链节点各归其持有者与分配器),
因此没有释放接口。指针有效期: 文档结构变更(新建、载入、增删布局)会整体重置条目数组与中间态, 调用方
不得跨结构变更缓存条目指针或元素规格指针, 一律按索引重新获取。

可注入性: 承载者按值内嵌在持有者的成员块中(非指针、非独立分配), 因此地址恒定, 允许对外交出裸指针;
若退化为指针式持有(额外一次分配), 地址语义与生命周期即不再成立。取址必须晚于持有者初始化, 放弃持有
必须早于持有者释放, 否则得到空指针或悬垂指针。本模块不记录"谁持有我", 也不提供回调,"未注入如何降级"
归消费方策略。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `doc/Document.h`(`src\doc\Document.h`) |
| 实现 | `src\doc\Document.cpp` |
| 命名空间 | 全局(结构 `LayoutEntry` 与类 `Document`) |
| 依赖 | 工程内: `Layout`(`Layout` 类型)、`BasicDesign`(`AutoSize`)、`LayoutModel`(`FileMidState` / `LayoutNode` / `ElementSpec` / `RefSize`); 系统: 无额外链接库 |
| 平台 | Windows; 尺寸与指针假设按 64 位构建 |
| 线程安全 | 否(承载者无同步; 文档结构变更与访问需在同一线程) |
