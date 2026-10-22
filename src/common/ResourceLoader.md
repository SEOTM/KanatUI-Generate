# ResourceLoader 模块 (src\common\ResourceLoader.h)

## 概述

`ResourceLoader` 提供资源与路径加载原语: 缓存 exe 所在目录、拼接 exe 目录内的文件路径、判断文件
是否存在、读取随 exe 内置的资源节。命名空间内全部是自由函数, 状态是文件内静态变量, 语义上是
进程级单例 —— 同一进程只有一份缓存的 exe 目录。

层归属是公共基础设施层, 依赖 `AllocatorHub` 的会话分配器(路径串与资源副本都分配在会话内存里),
不依赖框架层。它只处理"路径与字节", 不解析资源内容(JSON / bin 的解析属各自模块)。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `Init` | 缓存 exe 所在目录(启动期调用一次, 幂等)。 |
| `GetExeDir` | 取缓存的 exe 目录。 |
| `GetFilePath` | 把文件名拼到 exe 目录后(每次新分配)。 |
| `FileExists` | 判断普通文件是否存在。 |
| `LoadResource` | 读取随 exe 内置的资源节字节并拷进会话内存。 |

---

## Init 函数 (ResourceLoader)

缓存 exe 所在目录。

### 语法

```cpp
VOID Init();
```

### 参数

此函数无参数。

### 返回值

类型: **VOID**

此函数不返回值。

### 注解

应用启动期调用一次, 幂等: 已缓存时直接返回。目录取自 `GetModuleFileNameA` 的目录部分, 结果串以反斜杠
结尾, 存放在会话分配器里。取路径失败时目录保持未缓存状态, 此后 `GetFilePath` 返回 `NULL`。

目录只在首次调用时读取, 之后即使 exe 被移动也不会更新。

---

## GetExeDir 函数 (ResourceLoader)

取缓存的 exe 目录。

### 语法

```cpp
LPCSTR GetExeDir();
```

### 参数

此函数无参数。

### 返回值

类型: **LPCSTR**

缓存的目录串(以反斜杠结尾); 未初始化或初始化失败时返回空串。

### 注解

返回的串由会话分配器持有, 调用方不得释放。返回值恒非空指针, 因此判"目录可用"不能靠判空, 应看
`GetFilePath` 是否返回非空。

---

## GetFilePath 函数 (ResourceLoader)

把文件名拼到 exe 目录之后。

### 语法

```cpp
LPCSTR GetFilePath(
  [in] LPCSTR filename
);
```

### 参数

`[in] filename`

类型: **LPCSTR**

文件名或相对路径, 原样拼接到目录后(不补分隔符)。允许为 `NULL`。

### 返回值

类型: **LPCSTR**

拼接结果串(每次调用新分配, 由会话分配器持有); 目录未就绪或文件名为 `NULL` 时返回 `NULL`。

### 注解

拼接在固定长度的栈缓冲内完成(`MAX_PATH * 2` 字节), 且拼接不检查剩余空间: 当目录与文件名之和超出该
缓冲时会被截断或越界, 属已知长度上限。调用方应保证单次拼接不超过 `MAX_PATH * 2 - 1` 字节。
结果串只随会话释放失效, 调用方可长期持有。

---

## FileExists 函数 (ResourceLoader)

判断普通文件是否存在。

### 语法

```cpp
BOOL FileExists(
  [in] LPCSTR path
);
```

### 参数

`[in] path`

类型: **LPCSTR**

完整路径。允许为 `NULL`。

### 返回值

类型: **BOOL**

`TRUE` = 路径指向一个存在的文件系统对象; `FALSE` = 不存在或 `path` 为 `NULL`。

### 注解

实现只问"属性查询是否成功", 不区分文件、目录与其它类型; 需要区分时由调用方自行再查。本函数是加载链
"外部文件是否存在"这一级判据。

---

## LoadResource 函数 (ResourceLoader)

读取随 exe 内置的资源节字节并拷进会话内存。

### 语法

```cpp
BOOL LoadResource(
  [in]  DWORD   dwResId,
  [in]  LPCSTR  szResType,
  [out] LPVOID* ppData,
  [out] DWORD*  pcbData
);
```

### 参数

`[in] dwResId`

类型: **DWORD**

资源 ID, 由调用方定义; 本模块只把它转交系统资源查询。

`[in] szResType`

类型: **LPCSTR**

资源类型串, 如 `"JSON"` / `"BIN"`。允许为 `NULL`(此时直接失败)。

`[out] ppData`

类型: **LPVOID\***

数据首址输出: 成功时为会话分配器内的副本首址, 失败时置 `NULL`。指针本身不得为 `NULL`。

`[out] pcbData`

类型: **DWORD\***

字节数输出: 成功时为资源原始字节数, 失败时置 `0`。指针本身不得为 `NULL`。

### 返回值

类型: **BOOL**

`TRUE` = 命中并已拷贝; `FALSE` = 会话分配器未就绪、入参为空、资源未找到、资源为空或内存分配失败。

### 注解

资源节的原始字节不保证以零结尾, 而文本解析需要零结尾, 因此副本在数据后追加一个零字节; 该字节不计入
`pcbData`, 按显式长度解析的调用方(bin)不受影响。两个输出参数在进入函数时即被置空/置零, 失败时保持该值。

资源以只读方式读取(锁定资源内存后拷贝), 不修改 exe 内的镜像。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `common/ResourceLoader.h`(`src\common\ResourceLoader.h`) |
| 实现 | `src\common\ResourceLoader.cpp` |
| 命名空间 | `ResourceLoader` |
| 依赖 | 工程内: `AllocatorHub`(会话分配器); 系统: `kernel32`(`GetModuleFileNameA` / `GetFileAttributesA` / `FindResourceA` / `LoadResource` / `SizeofResource`), 无额外链接库 |
| 平台 | Windows; 路径按 ANSI 代码页解释, 单次拼接上限 `MAX_PATH * 2` 字节 |
| 线程安全 | 否(缓存目录是文件内静态变量, 无同步; `Init` 与其它调用不应并发) |
