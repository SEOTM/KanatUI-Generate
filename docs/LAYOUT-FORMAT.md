# 布局文档格式

## 1. 顶层结构

一份布局文档是一个 JSON 对象，只有一个必需成员 `layouts`：

```json
{
  "layouts": [
    { "name": "DemoMain", "label": "Demo", "width": {}, "height": {}, "ctrls": [] }
  ]
}
```

`layouts` 是数组，每项是一份布局；布局内 `ctrls` 是控件数组。运行期同时支持同构的二进制 `.bin`（第 9 节），两种形态装载为同一份中间态。

单文件上限：布局数 1..1024，单布局控件数 0..8192。

## 2. 布局字段

| 字段 | 类型 | 必需 | 说明 |
| :--- | :--- | :--- | :--- |
| `name` | 字符串（≤ 63 字节） | 否 | 布局名。保留名 `GenUIEditor` 表示编辑窗口外壳，启动时按它取外壳快照 |
| `label` | 字符串（≤ 127 字节） | 否 | 布局显示标签，为空时界面回落 `name` |
| `IsMaxiable` | 布尔 | 否 | 窗口是否可最大化，缺省 `false` |
| `IsResizeable` | 布尔 | 否 | 窗口是否可调整大小，缺省 `false` |
| `wndX` / `wndY` | 整数 | 否 | 窗口桌面位置（像素），缺省 0；预览窗口标题拖动会写回这两个值 |
| `width` | 尺寸因子 | 是 | 窗口宽度系数 |
| `height` | 尺寸因子 | 是 | 窗口高度系数 |
| `autosize` | 对象 | 否 | 参考尺寸 / 经验参数，缺省为屏高模式 + 默认千分比 |
| `ctrls` | 数组 | 是 | 控件数组 |

布局级 `width` / `height` 解算的是窗口客户区尺寸：

```text
窗口宽 = Σ(width.coeff[i] × 参考量[i]) + width.offset
窗口高 = Σ(height.coeff[i] × 参考量[i]) + height.offset
```

## 3. autosize

`autosize` 决定六个"经验参考量"如何由模式与基准推导：

```json
"autosize": {
  "mode": "AUTO_SIZE_BY_SCREEN_HEIGHT",
  "unit": 0,
  "width": 622,
  "height": 432,
  "controlHeight": 25,
  "fontSize": 13,
  "space": 10,
  "padding": 4
}
```

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `mode` | 字符串或整数 | 换算基准：`AUTO_SIZE_BY_SCREEN_WIDTH` / `AUTO_SIZE_BY_SCREEN_HEIGHT` / `AUTO_SIZE_BY_WINDOW_WIDTH` / `AUTO_SIZE_BY_WINDOW_HEIGHT`，也可写 0..3。字符串未命中按屏高；整数越界按 0（屏宽） |
| `unit` | 整数 | 0 = 千分比（缺省），1 = 像素。只影响输入值的解释，中间态恒存千分比 |
| `width` / `height` | 0..10000 | 窗口宽 / 高（相对换算基准） |
| `controlHeight` | 0..10000 | 控件高度基准（相对换算基准） |
| `fontSize` | 0..10000 | 字号基准（相对换算基准） |
| `space` | 0..10000 | 设计间隔（相对换算基准） |
| `padding` | 0..10000 | 设计内边距（相对换算基准） |

推导：

```text
参考量像素 = 千分比 × 基准像素 / 1000
```

`screenWidth` / `screenHeight` 直接取主显示器；`windowWidth` / `windowHeight` 在窗口建好后取实际客户区。

## 4. 尺寸因子

四轴（`x` / `y` / `width` / `height`）的取值都是一个尺寸因子对象：

```json
{ "coeff": [0, 0, 1, 0, 0, 0, 0, 0], "offset": 0 }
```

`coeff` 必须给满 8 项（若不足，缺项按 0 处理并打印诊断），顺序与参考量一一对应：

| 下标 | 参考量 | 含义 |
| :--- | :--- | :--- |
| 0 | `screenWidth` | 屏幕宽 |
| 1 | `screenHeight` | 屏幕高 |
| 2 | `windowWidth` | 窗口客户区宽 |
| 3 | `windowHeight` | 窗口客户区高 |
| 4 | `controlHeight` | 控件高度基准 |
| 5 | `fontSize` | 字号基准 |
| 6 | `space` | 设计间隔 |
| 7 | `padding` | 设计内边距 |

解算式：

```text
像素值 = Σ(coeff[i] × 参考量[i]) + offset     (i = 0..7)
```

`offset` 是整数像素微调。系数可正可负，也可以大于 1；例如 `height.coeff = [0,0,0,1,-1,0,0,0]` 表示"窗口高减去一个控件高"。

## 5. 控件字段

```json
{
  "id": 0,
  "parent": -1,
  "type": "CTRL_TYPE_WND",
  "label": "##MAINWND",
  "text": "",
  "event": "",
  "x": { "coeff": [0,0,0,0,0,0,0,0], "offset": 0 },
  "y": { "coeff": [0,0,0,0,0,0,0,0], "offset": 0 },
  "width": { "coeff": [0,0,1,0,0,0,0,0], "offset": 0 },
  "height": { "coeff": [0,0,0,1,0,0,0,0], "offset": 0 }
}
```

| 字段 | 类型 | 必需 | 说明 |
| :--- | :--- | :--- | :--- |
| `id` | 整数 | 否 | 控件 ID，缺省取数组下标。父链与事件委托按 ID 寻址 |
| `parent` | 整数 | 否 | 父容器 ID，缺省 -1（无父）。`parent` 同时驱动层序推导与坐标累加 |
| `type` | 字符串 | 是 | 控件类型串，必须是第 6 节的规范名；未命中串装载为 `UNKNOWN`，按矩形占位 |
| `label` | 字符串（≤ 127 字节） | 否 | 控件标签，界面用 `##` 前缀做命中键 |
| `text` | 字符串（≤ 127 字节） | 否 | 控件文本；文本类控件的初始缓冲内容 |
| `event` | 字符串（≤ 63 字节） | 否 | 事件名；空串表示不绑定 |
| `x` / `y` | 尺寸因子 | 是 | 控件左上角（父坐标系） |
| `width` / `height` | 尺寸因子 | 是 | 控件宽高 |

定长字段超长时按容量截断并打印诊断。

## 6. 控件类型

| 分组 | 规范名 | 说明 |
| :--- | :--- | :--- |
| 结构件 | `CTRL_TYPE_WND` | 窗口体。必须存在于布局前三个槽位；取窗口位置 |
| 结构件 | `CTRL_TYPE_TITLE` | 标题栏：标题文本 + 右上关闭钮；按住可拖动窗口 |
| 结构件 | `CTRL_TYPE_CNTR` | 内容容器。内容控件应声明 `parent` 指向它（或指向容器内的其他控件） |
| 文字与图形 | `CTRL_TYPE_TEXT` | 静态文本 |
| 文字与图形 | `CTRL_TYPE_RECT` | 矩形域 |
| 文字与图形 | `CTRL_TYPE_RENDER` | 渲染面：有纹理时等比居中绘制，无纹理时占位 |
| 输入 | `CTRL_TYPE_INPUT_SINGLE` | 单行文本输入 |
| 输入 | `CTRL_TYPE_INPUT_MULTI` | 多行文本输入 |
| 输入 | `CTRL_TYPE_INPUT_NUMERIC` | 整数输入 |
| 输入 | `CTRL_TYPE_INPUT_REAL` | 实数输入 |
| 输入 | `CTRL_TYPE_INPUT_ACTION` | 输入动作：单行输入 + 右侧按钮（复合控件） |
| 选择 | `CTRL_TYPE_BUTTON` | 按钮 |
| 选择 | `CTRL_TYPE_CHECKBOX` | 复选框 |
| 选择 | `CTRL_TYPE_SLIDER` | 单手柄滑动条 |
| 选择 | `CTRL_TYPE_COMBO` | 下拉框 |
| 选择 | `CTRL_TYPE_COMBO_ACTION` | 下拉动作：下拉框 + 右侧按钮（复合控件） |
| 选择 | `CTRL_TYPE_PROG` | 进度显示 |
| 选择 | `CTRL_TYPE_TIMEBAR` | 时间条（起止时码） |
| 列表 | `CTRL_TYPE_LIST` | 列表域（类型已登记，暂无绘制实现） |
| 列表 | `CTRL_TYPE_LIST_INPUT` | 布局列表：行内是输入动作块，支持行编辑与拖拽重排 |
| 列表 | `CTRL_TYPE_LIST_COMBO` | 控件列表：行内是只读输入动作块，特殊行是下拉动作块 |
| 列表 | `CTRL_TYPE_LIST_TIMESPAN` | 时段列表（类型已登记，暂无绘制实现） |

`CTRL_TYPE_INVALID` 与 `CTRL_TYPE_UNKNOWN` 是哨兵，不是可声明类型；文档写 `INVALID` 会被当作未知类型占位。

结构件固定占前三个槽位，顺序建议 `WND`、`TITLE`、`CNTR`。控件层次按类型自动递进：承载内容的"域"类每嵌套一层下沉一档，按钮与复选框上浮一档，其余继承父层；结构件恒为背景层。

## 7. 父链、层次与坐标

`parent` 同时是三条链：绘制层次、坐标累加、事件委托。

- **坐标**：内容控件的有效坐标 = 声明坐标 + 父链上各枚控件的声明坐标之和。声明坐标由几何解算写入且不再改写，有效坐标可以反复重推。
- **层次**：`nLayer = 父层次 + 本类型步进`，父缺失或成环时父层次按 0。步进见 `ControlSpec.h` 的 `CtrlLayerStep`。
- **事件委托**：`event` 写特殊名 `CTRL_EVENT_ENTRUST_PARENT`，本控件不绑定处理函数，事件按 `parent` 上溯交给父容器。

只认声明：父控件可以声明在子控件之后，也可以与子控件没有几何包含关系。

## 8. 事件

`event` 是一个名字，装配期由事件绑定表解析为处理函数。绑定表由应用提供；文档里未登记的名字不会报错，该控件表现为无响应。

框架还有一个保留的特殊事件名 `CTRL_EVENT_ENTRUST_PARENT`（见第 7 节）。它不是绑定表条目。

## 9. 二进制格式

bin 是定长记录格式：文件头 + 若干布局段；每个布局段是段头 + 一枚布局头 + 若干控件记录。记录区是 C++ 结构体的内存镜像（小端、x64、MSVC 布局）。

### 9.1 文件头（12 字节）

| 偏移 | 大小 | 内容 |
| :--- | :--- | :--- |
| 0 | 4 | CRC32 校验码 |
| 4 | 4 | 版本号 |
| 8 | 4 | 段数量（布局数，1..1024） |

校验码覆盖整个文件，计算时校验码字段自身按 0 参与。版本号高 24 位是代际版本（当前 `0x00000200`，即代际 2），低 8 位是格式标记（压缩 / 加密尚未实现，必须为 0）。

### 9.2 布局段

| 偏移 | 大小 | 内容 |
| :--- | :--- | :--- |
| 0 | 4 | 本段字节数（含本字段自身） |
| 4 | 376 | 布局头（`LayoutInfo` 镜像） |
| 380 | 536 × N | 控件记录（`ElementSpec` 镜像），顺序即控件顺序 |

段大小校验：≥ 4 + 376；`(段大小 − 380)` 必须能被 536 整除；各段首尾相接后必须正好铺满文件。

### 9.3 布局头（376 字节）

| 偏移 | 大小 | 字段 |
| :--- | :--- | :--- |
| 0 | 128 | `label`（定长串） |
| 128 | 64 | `name`（定长串） |
| 192 | 64 | `refSize.mode`（模式串） |
| 256 | 4 | `refSize.unit` |
| 260 | 4 | `refSize.width` |
| 264 | 4 | `refSize.height` |
| 268 | 4 | `refSize.controlHeight` |
| 272 | 4 | `refSize.fontSize` |
| 276 | 4 | `refSize.space` |
| 280 | 4 | `refSize.padding` |
| 284 | 36 | `width` 尺寸因子（8 × float + 1 × int） |
| 320 | 36 | `height` 尺寸因子 |
| 356 | 4 | `IsMaxiable`（BOOL） |
| 360 | 4 | `IsResizeable`（BOOL） |
| 364 | 4 | `wndX`（LONG） |
| 368 | 4 | `wndY`（LONG） |
| 372 | 4 | `dwElements`（控件数量） |

### 9.4 控件记录（536 字节）

| 偏移 | 大小 | 字段 |
| :--- | :--- | :--- |
| 0 | 128 | `label`（定长串） |
| 128 | 128 | `text`（定长串） |
| 256 | 64 | `type`（定长串） |
| 320 | 64 | `event`（定长串） |
| 384 | 36 | `x` 尺寸因子 |
| 420 | 36 | `y` 尺寸因子 |
| 456 | 36 | `width` 尺寸因子 |
| 492 | 36 | `height` 尺寸因子 |
| 528 | 4 | `id`（int） |
| 532 | 4 | `parentID`（int） |

尺寸因子内部 = 8 个 float 系数（32 字节）+ 1 个 int 偏移（4 字节）。定长串必须带结尾符或由装载端兜底置 0。

### 9.5 校验规则

装载时按顺序检查，任一不过即拒绝（打印诊断并返回失败）：

1. 文件不短于 12 字节；
2. CRC32 与文件头记录一致；
3. 版本代际与当前格式一致；格式标记为 0；
4. 段数量 1..1024；
5. 每段段头不越界、段大小容得下段头与布局头、控件区整除、段不越界；
6. 布局头声明的控件数不超过 8192；
7. 段区总长正好等于文件长度。

宽容项：布局头的控件数与段大小反算值不一致时取较小值并打印诊断；`parentID < -1` 归一为 -1；定长串末字节强制置 0。

## 10. 限制与校验一览

| 项 | 上限 / 规则 |
| :--- | :--- |
| 布局数 | 1..1024 |
| 单布局控件数 | 0..8192 |
| 布局名 | 63 字节 |
| 控件 label / text | 127 字节 |
| 控件 type / event | 63 字节 |
| autosize 六个参考量 | 0..10000 |
| `coeff` | 8 项，缺项按 0（打印诊断） |
| `offset` | 整数，缺失按 0（打印诊断） |
| JSON 必需字段 | `layouts`、每布局 `width` / `height` / `ctrls`、每控件 `type` / `x` / `y` / `width` / `height` |

## 11. 最小示例

下面是文档结构的最小示范（一份布局、三枚结构件）。它本身不能单独启动设计器——启动要求文档内含保留名 `GenUIEditor` 的外壳布局；完整示例见 [examples/GenUI.json](examples/GenUI.json)。

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
          "id": 0, "parent": -1, "type": "CTRL_TYPE_WND", "label": "##MAINWND",
          "text": "", "event": "",
          "x": { "coeff": [0,0,0,0,0,0,0,0], "offset": 180 },
          "y": { "coeff": [0,0,0,0,0,0,0,0], "offset": 120 },
          "width":  { "coeff": [0,0,1,0,0,0,0,0], "offset": 0 },
          "height": { "coeff": [0,0,0,1,0,0,0,0], "offset": 0 }
        },
        {
          "id": 1, "parent": -1, "type": "CTRL_TYPE_TITLE", "label": "##TITLEBAR",
          "text": "Demo", "event": "",
          "x": { "coeff": [0,0,0,0,0,0,0,0], "offset": 0 },
          "y": { "coeff": [0,0,0,0,0,0,0,0], "offset": 0 },
          "width":  { "coeff": [0,0,1,0,0,0,0,0], "offset": 0 },
          "height": { "coeff": [0,0,0,0,1,0,0,0], "offset": 0 }
        },
        {
          "id": 2, "parent": -1, "type": "CTRL_TYPE_CNTR", "label": "##CONTAINER",
          "text": "", "event": "",
          "x": { "coeff": [0,0,0,0,0,0,0,0], "offset": 0 },
          "y": { "coeff": [0,0,0,0,1,0,0,0], "offset": 0 },
          "width":  { "coeff": [0,0,1,0,0,0,0,0], "offset": 0 },
          "height": { "coeff": [0,0,0,1,-1,0,0,0], "offset": 0 }
        }
      ]
    }
  ]
}
```
