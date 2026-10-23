#pragma once
#include <minwindef.h>
#include <stdint.h>
#include <stddef.h>
#include <basetsd.h>
#include <string.h>
#include "framework/base/mem/MemoryManager.h"
#include "framework/base/FuncPtr.h"

// ================================
// 文件: ControlSpec.h
// 描述: 控件类型约定与控件数据 —— 控件类型的身份(枚举名 ↔ 字符串名)、控件信息结构体(CtrlInfo)
//       与逐类型约定(接口契约见同目录 ControlSpec.md)
// ================================

// ================================
// 区域: 控件类型身份
// ================================

// 宏: CTRL_TYPE_TABLE
#define CTRL_TYPE_TABLE(ENTRY)                 \
	ENTRY(CTRL_TYPE_INVALID,       "CTRL_TYPE_INVALID")       \
	ENTRY(CTRL_TYPE_UNKNOWN,       "CTRL_TYPE_UNKNOWN")       \
	ENTRY(CTRL_TYPE_WND,           "CTRL_TYPE_WND")           \
	ENTRY(CTRL_TYPE_CNTR,          "CTRL_TYPE_CNTR")          \
	ENTRY(CTRL_TYPE_TITLE,         "CTRL_TYPE_TITLE")         \
	ENTRY(CTRL_TYPE_RECT,          "CTRL_TYPE_RECT")          \
	ENTRY(CTRL_TYPE_TEXT,          "CTRL_TYPE_TEXT")          \
	ENTRY(CTRL_TYPE_INPUT_SINGLE,  "CTRL_TYPE_INPUT_SINGLE")  \
	ENTRY(CTRL_TYPE_INPUT_MULTI,   "CTRL_TYPE_INPUT_MULTI")   \
	ENTRY(CTRL_TYPE_INPUT_NUMERIC, "CTRL_TYPE_INPUT_NUMERIC") \
	ENTRY(CTRL_TYPE_INPUT_REAL,    "CTRL_TYPE_INPUT_REAL")    \
	ENTRY(CTRL_TYPE_INPUT_ACTION,  "CTRL_TYPE_INPUT_ACTION")  \
	ENTRY(CTRL_TYPE_BUTTON,        "CTRL_TYPE_BUTTON")        \
	ENTRY(CTRL_TYPE_LIST,          "CTRL_TYPE_LIST")          \
	ENTRY(CTRL_TYPE_LIST_INPUT,    "CTRL_TYPE_LIST_INPUT")    \
	ENTRY(CTRL_TYPE_LIST_COMBO,    "CTRL_TYPE_LIST_COMBO")    \
	ENTRY(CTRL_TYPE_TIMEBAR,       "CTRL_TYPE_TIMEBAR")       \
	ENTRY(CTRL_TYPE_RENDER,        "CTRL_TYPE_RENDER")        \
	ENTRY(CTRL_TYPE_SLIDER,        "CTRL_TYPE_SLIDER")        \
	ENTRY(CTRL_TYPE_COMBO,         "CTRL_TYPE_COMBO")         \
	ENTRY(CTRL_TYPE_COMBO_ACTION,  "CTRL_TYPE_COMBO_ACTION")  \
	ENTRY(CTRL_TYPE_PROG,          "CTRL_TYPE_PROG")           \
	ENTRY(CTRL_TYPE_CHECKBOX,      "CTRL_TYPE_CHECKBOX")       \
	ENTRY(CTRL_TYPE_LIST_TIMESPAN, "CTRL_TYPE_LIST_TIMESPAN")

// 类型: CtrlType
enum CtrlType
{
#define CTRL_TYPE_ENUM(name, str) name,
	CTRL_TYPE_TABLE(CTRL_TYPE_ENUM)
#undef CTRL_TYPE_ENUM
	CTRL_TYPE_COUNT			// 类型总数(枚举哨兵, 不是合法类型)
};

// 结构: vec2
struct vec2
{
	float x;
	float y;
	vec2() : x(0.0f), y(0.0f) {}
	vec2(float _x, float _y) : x(_x), y(_y) {}
	vec2 operator+() const { return *this; }
	vec2 operator-() const { return vec2(-x, -y); }
	vec2 operator+(const vec2& other) const { return vec2(x + other.x, y + other.y); }
	vec2 operator-(const vec2& other) const { return vec2(x - other.x, y - other.y); }
	vec2 operator*(const vec2& other) const { return vec2(x * other.x, y * other.y); }
	vec2 operator/(const vec2& other) const { return vec2(x / other.x, y / other.y); }
	BOOL operator!=(const vec2& other) const { return (x != other.x || y != other.y); }
	BOOL operator==(const vec2& other) const { return (x == other.x && y == other.y); }
};

// 结构: CtrlInfo
struct CtrlInfo
{
	BufferInfo bufferInfo;		// 控件数据缓冲(首址 + 请求字节数; buffer == nullptr = 无缓冲)
	INT32 nLayer;				// 控件层次(相对量, 有符号: 0 = 窗口底那一档, 负 = 更沉 / 正 = 更亮;
								// 由 Layout 按 parentID 链递推 = "父控件层次 + 自身类型步进",
								// 见 CtrlLayerStep; 结构件(WND/TITLE/CNTR)恒 0; 绘制时的投影层号 =
								// PROJ_LAYER_BACKGROUND + nLayer(钳到 [PROJ_LAYER_MIN, PROJ_LAYER_MAX]),
								// 见本文件"区域: 控件层次与层号换算")
	pfnCtrlEvent pEventHandler;	// 控件的事件处理函数指针（由布局 JSON 的 event 经 UIEventHandler 绑定表解析；
								// 语义随控件类型: 按钮=点击触发 / 输入=提交触发 / TITLE=标题标志触发 /
								// 列表类(LIST_*)=渲染该列表时按帧调用以填充列表内容）；
								// event 为 CtrlEventIsEntrustParent 判定的特殊值时改为父容器(按 parentID
								// 上溯)的处理函数, 见本文件"区域: 控件事件名约定"
	vec2 size;					// 控件大小(像素; 由解析期的几何系数与 AutoSize 解算而来)
	vec2 position;				// 控件位置(像素; 有效坐标 = 窗口本地坐标) ——
								// 内容控件 = 声明坐标 + 父链累计偏移(由 Layout::DeriveCtrlPositions 落地);
								// 结构件不自带累加: WND = 桌面位置 / TITLE · CNTR = 窗口本地坐标
	vec2 posDeclared;			// 声明坐标(像素; 父控件坐标系内的坐标 —— 几何解算写入, 父链累加的输入) ——
								// 声明坐标一经解算即不再改写 ⇒ 有效坐标可反复重推(见 Layout::DeriveCtrlPositions)
	LPCSTR label;				// 控件标签
	CtrlType type;				// 控件类型(装载期由文档类型串经 GetTypeEnum 解析; 未命中串 = CTRL_TYPE_UNKNOWN)
	int id;						// 控件ID
	int parentID;				// 父容器ID(声明值 = 文档 parent 字段; -1 = 无父容器; 层次推导 · 坐标累加 ·
								// 事件委托三者的共同父链)

	// 函数: 构造函数
	// 描述: 按无效控件初值清零构造(未使用槽位 / 无缓冲 / 无事件处理器 / 无父容器)
	CtrlInfo()
		: id(0), type(CTRL_TYPE_INVALID), label(nullptr),
		position(0.0f, 0.0f), size(0.0f, 0.0f), posDeclared(0.0f, 0.0f),
		bufferInfo{ nullptr, 0 },
		nLayer(0),
		pEventHandler(nullptr),
		parentID(-1)
	{}

	// 函数: IsKnownType
	// 描述: 判断渲染层是否认识本控件类型(未知类型与未使用槽位都不认识)
	BOOL IsKnownType() const;
};

// 结构布局不变量(CtrlInfo)
// 说明: CtrlInfo 是跨模块按值传递的结构(含"控件层次"与"父容器"字段), 其尺寸与关键字段偏移用编译期断言固定,
//       防止某处按旧字段集合推算索引/大小(那会读到邻元素, 表现为"类型与索引错位")。
//       新增/调整字段时本组断言会失败 ⇒ 必须同轮核对其它按 sizeof/offsetof 工作的代码。
static_assert(sizeof(CtrlInfo) == 80, "CtrlInfo 尺寸变了: 请核对按 sizeof 分配/索引的代码");
static_assert(offsetof(CtrlInfo, nLayer) == 16, "CtrlInfo::nLayer 偏移变了: 请核对字段顺序");
static_assert(offsetof(CtrlInfo, posDeclared) == 48, "CtrlInfo::posDeclared 偏移变了: 请核对字段顺序");
static_assert(offsetof(CtrlInfo, type) == 64, "CtrlInfo::type 偏移变了: 请核对字段顺序");
static_assert(offsetof(CtrlInfo, id) == 68, "CtrlInfo::id 偏移变了: 请核对字段顺序");
static_assert(offsetof(CtrlInfo, parentID) == 72, "CtrlInfo::parentID 偏移变了: 请核对字段顺序");

// ================================
// 区域: 控件事件名约定
// 说明: 文档的 event 字段(CtrlInfo::pEventHandler 的来源)除"绑定表里的业务事件名"外, 另有一个
//       框架保留的特殊值 —— 委托父容器(CTRL_EVENT_ENTRUST_PARENT)。该值不是绑定表中的名字,
//       装配期不按事件名查表, 而是把本控件的事件绑定到父容器(按 parentID 上溯)的事件。
// ================================
// 常量: CTRL_EVENT_ENTRUST_PARENT
// 描述: 事件字段的特殊值 —— 把本控件的事件绑定至父容器的事件
constexpr LPCSTR CTRL_EVENT_ENTRUST_PARENT = "CTRL_EVENT_ENTRUST_PARENT";

// 函数: CtrlEventIsEntrustParent
// 描述: 判定事件名是否为"委托父容器"特殊事件
inline BOOL CtrlEventIsEntrustParent(LPCSTR eventname)
{
	if (eventname == NULL)
	{
		return FALSE;
	}
	return (strcmp(eventname, CTRL_EVENT_ENTRUST_PARENT) == 0) ? TRUE : FALSE;
}

// ================================
// 区域: 控件层次与层号换算
// 说明: 投影配色是"一条色轴 + 层号取样"的口径 ⇒ 控件必须知道自己在第几层。
//       控件库逐枚无状态被调用, 看不到层级 ⇒ 层次由 Layout 在装配/增删时按父容器 ID 链自动递推,
//       并随控件信息(CtrlInfo::nLayer)送进绘制件(控件库取值处)。
//       本区 = "层次 → 投影层号"的唯一换算口径(宿主与控件库同源, 不得各写一份)。
// --------------------------------
// 层次递推口径(parentchain 轮落地):
//   ①层次 = 相对量: 0 = "窗口底"那一档(结构件所在); 允许为负(负 = 比窗口底更沉), 亦允许为正;
//   ②逐枚按 parentID 链递推 —— 取父控件(按 ID 寻址)的层次为 last, 再按本控件自身类型的步进取值:
//     nLayer = last + CtrlLayerStep(自身类型); 父缺失(parentID 为 -1 / 目标 ID 不存在 / 成环)时 last = 0;
//     即"域(承载内容的区域)逐级下沉 · 件(操作交互的物件)上浮 · 其余继承容器" ——
//     嵌套多少层就递进多少档("逐级变化"由父链深度自然给出, 无需另定累加规则);
//   ③推导与链序、几何均无关: 父可声明在子之后, 也可与子无几何包含关系(只认声明, 不做包含推导);
//     同一条父链同时是坐标累加的链 —— 内容控件的有效坐标 = 声明坐标 + 父链上各枚控件的声明坐标之和
//     (见 Layout::DeriveCtrlPositions; 结构件的坐标另有归属, 不作父基也不参与累加);
//   ④结构件(WND/TITLE/CNTR)恒 0(无视 parent 声明); 预览选中标记不参与推导(保持原值), 且不作父基
//     (子控件声明它为父 ⇒ 视同无父);
//   ⑤未列入"域 / 件"两组的类型一律归 else(继承) —— 新增类型默认不改变层次, 归组须显式加 case。
// --------------------------------
// 背景层(窗口底档)的层号 —— 本常量是"整屏明暗"的唯一旋钮:
//   ⇒ 由 5 抬到 8: 低端腾出 7 档给"域逐级下沉", 高端留 8 档给"件上浮";
//   抬高后典型"域中域"控件(输入框 / 下拉)回落至层 6 —— 与定稿前的内容控件档位一致。
constexpr INT32 PROJ_LAYER_BACKGROUND = 8;	// 背景层 / 窗口底档(层次 0)对应的投影层号
constexpr INT32 PROJ_LAYER_MIN  = 1;		// 投影层号下限(层次过负 = 嵌套过深 ⇒ 压平到本层)
constexpr INT32 PROJ_LAYER_MAX  = 16;		// 投影层号上限(层次过大 ⇒ 压平到本层)

// 函数: CtrlLayerOf
// 描述: 由控件自身的层次值得出投影层号(全库唯一换算口径)
inline INT32 CtrlLayerOf(INT32 nLayer)
{
	INT32 nRet = PROJ_LAYER_BACKGROUND + nLayer;
	if (nRet > PROJ_LAYER_MAX)
	{
		nRet = PROJ_LAYER_MAX;
	}
	else if (nRet < PROJ_LAYER_MIN)
	{
		nRet = PROJ_LAYER_MIN;
	}
	return nRet;
}

// 枚举: CTRL_LAYER_STEP
// 描述: 层次递推的逐类型步进(相对"父控件层次"的调档方向)
// 说明: 分三组(判据 = 该类型是"供内容存在的区域"还是"用户直接操作的物件"):
//       · 下沉组(域 / 承载面) ⇒ −1: RECT · INPUT_* · LIST_* · COMBO* · RENDER · SLIDER;
//       · 上浮组(件 / 操作物) ⇒ +1: BUTTON · CHECKBOX
//       · 其余 ⇒ 0(继承容器层次): 结构件 · TEXT · TIMEBAR · PROG · UNKNOWN · INVALID。
//       语义 = "每层递进方向"(而非"相对固定基准的一次性调整"): 嵌套几层就累进几档。
enum CTRL_LAYER_STEP
{
	CTRL_LAYER_STEP_DOWN = -1,		// 域/承载面: 每嵌套一层下沉一档
	CTRL_LAYER_STEP_FLAT = 0,		// 其余: 继承父控件的层次
	CTRL_LAYER_STEP_UP   = 1		// 件/操作物: 相对父控件上浮一档
};

// 函数: CtrlLayerStep
// 描述: 取某控件类型在层次递推中的步进(相对"父控件层次"的调档量)
inline INT32 CtrlLayerStep(CtrlType type)
{
	switch (type)
	{
	// 件 / 操作物: 上浮一档
	case CTRL_TYPE_BUTTON:
	case CTRL_TYPE_CHECKBOX:	// 复选框 = 用户直接操作的物件
		return (INT32)CTRL_LAYER_STEP_UP;
	// 域 / 承载面: 下沉一档(矩形域 / 输入域 / 列表域 / 下拉域 / 渲染域 / 滑动条)
	case CTRL_TYPE_RECT:
	case CTRL_TYPE_INPUT_SINGLE:
	case CTRL_TYPE_INPUT_MULTI:
	case CTRL_TYPE_INPUT_NUMERIC:
	case CTRL_TYPE_INPUT_REAL:
	case CTRL_TYPE_INPUT_ACTION:
	case CTRL_TYPE_LIST:
	case CTRL_TYPE_LIST_INPUT:
	case CTRL_TYPE_LIST_COMBO:
	case CTRL_TYPE_LIST_TIMESPAN:
	case CTRL_TYPE_COMBO:
	case CTRL_TYPE_COMBO_ACTION:
	case CTRL_TYPE_RENDER:
	case CTRL_TYPE_SLIDER:
		return (INT32)CTRL_LAYER_STEP_DOWN;
	// 其余(结构件 / TEXT / TIMEBAR / PROG / UNKNOWN / INVALID): 继承容器层次
	default:
		return (INT32)CTRL_LAYER_STEP_FLAT;
	}
}

// 表: CtrlTypeNames
inline constexpr LPCSTR CtrlTypeNames[] = {
#define CTRL_TYPE_NAME(name, str) str,
	CTRL_TYPE_TABLE(CTRL_TYPE_NAME)
#undef CTRL_TYPE_NAME
};
static_assert(sizeof(CtrlTypeNames) / sizeof(CtrlTypeNames[0]) == (size_t)CTRL_TYPE_COUNT,
	"控件类型表与枚举不同步");

// 函数: GetTypeName
// 描述: 由类型枚举取规范类型串(越界或非法枚举回退未使用槽位的串)
inline LPCSTR GetTypeName(CtrlType type)
{
	if (type < 0 || type >= CTRL_TYPE_COUNT)
	{
		return CtrlTypeNames[CTRL_TYPE_INVALID];
	}
	return CtrlTypeNames[type];
}

// 函数: GetTypeEnum
// 描述: 由类型串取类型枚举(精确匹配; 空串或未命中一律归未知类型)
inline CtrlType GetTypeEnum(LPCSTR type)
{
	if (type == NULL)
	{
		return CTRL_TYPE_UNKNOWN;
	}
	for (size_t i = 0; i < (size_t)CTRL_TYPE_COUNT; i++)
	{
		if (strcmp(type, CtrlTypeNames[i]) == 0)
		{
			return (CtrlType)i;
		}
	}
	return CTRL_TYPE_UNKNOWN;
}

// ================================
// 区域: 控件类型属性查询
// 说明: 只回答"该类型是否具备某纯类型属性"(不涉及缓冲、不涉及绘制实现) ——
//       控件库的缓冲装配与绘制分派、布局渲染出口的命中判定共用同一口径;
//       文本区的取址与容量归 CtrlBuffer.h, 本区只判"是否文本类"。
// ================================

// 函数: CtrlTypeIsList
// 描述: 是否"列表类控件"(布局列表 LIST_INPUT / 控件列表 LIST_COMBO;
//       其内容由控件库按类型分派(见 Controls::ListInput / Controls::ListCombo)绘制)
inline BOOL CtrlTypeIsList(CtrlType type)
{
	return (type == CTRL_TYPE_LIST_INPUT || type == CTRL_TYPE_LIST_COMBO);
}

// 函数: CtrlTypeIsCompound
// 描述: 是否"复合控件"(单个控件由多枚不相交的可交互子项合成; 如输入动作 = 左段输入 + 右段按钮,
//       下拉动作 = 左段下拉 + 右段按钮, 列表 = 行内逐枚子项)
inline BOOL CtrlTypeIsCompound(CtrlType type)
{
	return ((type == CTRL_TYPE_INPUT_ACTION) ||
		(type == CTRL_TYPE_COMBO_ACTION) ||
		(CtrlTypeIsList(type) == TRUE)) ? TRUE : FALSE;
}

// 函数: CtrlTypeIsRawGraphic
// 描述: 是否"纯图元控件"(控件库只产出像素、不自带可交互 ImGui 项的控件类型)
inline BOOL CtrlTypeIsRawGraphic(CtrlType type)
{
	return (type == CTRL_TYPE_TEXT || type == CTRL_TYPE_RECT || type == CTRL_TYPE_UNKNOWN) ? TRUE : FALSE;
}

// 函数: CtrlTypeHandlesClickItself
// 描述: 该控件类型是否由控件库自身在点击时派发 OPER_COMMON_CLICKED(⇒ 渲染出口不得再判一次,
//       否则同一击会被登记两次)
inline BOOL CtrlTypeHandlesClickItself(CtrlType type)
{
	return (type == CTRL_TYPE_TEXT || type == CTRL_TYPE_RECT || type == CTRL_TYPE_BUTTON) ? TRUE : FALSE;
}

// 函数: CtrlTypeIsTextKind
// 描述: 是否"文本缓冲控件"(缓冲内容为 C 字符串, 可通过文本通道编辑)
// 说明: 文本类 = TEXT / BUTTON / INPUT_SINGLE / INPUT_MULTI / INPUT_ACTION / TITLE;
//       复选框不属文本类(其显示文字是结构内字段, 不参与文本通道)。
inline BOOL CtrlTypeIsTextKind(CtrlType type)
{
	return (type == CTRL_TYPE_TEXT || type == CTRL_TYPE_BUTTON ||
		type == CTRL_TYPE_INPUT_SINGLE || type == CTRL_TYPE_INPUT_MULTI ||
		type == CTRL_TYPE_INPUT_ACTION || type == CTRL_TYPE_TITLE);
}

// ================================
// 区域: 操作码(OPER)系统
// ================================

// 命名空间: OperCode
namespace OperCode
{
	// 枚举: OPER_SEG
	enum OPER_SEG
	{
		OPER_SEG_COMMON  = 0,	// 低 16 位 [15:0]
		OPER_SEG_OWN     = 1,	// 次 16 位 [31:16]
		OPER_SEG_PAYLOAD = 2,	// 高 32 位 [63:32]
		OPER_SEG_COUNT			// 段总数(哨兵, 不是合法段)
	};

	// 函数: GetSegment
	// 描述: 取操作码某一段的段内值(已右移到最低位; 非法段返回 0)
	constexpr uint64_t GetSegment(uint64_t code, OPER_SEG seg)
	{
		switch (seg)
		{
		case OPER_SEG_COMMON:	return (code & 0x000000000000FFFF);
		case OPER_SEG_OWN:		return ((code >> 16) & 0x000000000000FFFF);
		case OPER_SEG_PAYLOAD:	return (code >> 32);
		default:				return 0;
		}
	}

	// 函数: IsWithin
	// 描述: 判断操作码某一段是否"有码"(段内值非 0; 非法段判无码)
	constexpr BOOL IsWithin(uint64_t code, OPER_SEG seg)
	{
		return (GetSegment(code, seg) != 0) ? TRUE : FALSE;
	}

	// 函数: MakeCode
	// 描述: 把三段段内值拼成整字操作码(公共在低 16 位 · 专属在次 16 位 · 搭载在高 32 位)
	constexpr uint64_t MakeCode(uint16_t common, uint16_t own, uint32_t payload)
	{
		return (uint64_t)common | ((uint64_t)own << 16) | ((uint64_t)payload << 32);
	}

	// 函数: MakeCodeCommon
	// 描述: 构造只含公共段的整字操作码(专属段与搭载段置空)
	constexpr uint64_t MakeCodeCommon(uint16_t common)
	{
		return MakeCode(common, 0, 0);
	}

	// 函数: MakeCodeOwn
	// 描述: 构造只含专属段的整字操作码(公共段与搭载段置空)
	constexpr uint64_t MakeCodeOwn(uint16_t own)
	{
		return MakeCode(0, own, 0);
	}
}

// 整字掩码(与 oper 直接位与)
constexpr uint64_t OPER_SEG_MASK_COMMON  = 0x000000000000FFFF;
constexpr uint64_t OPER_SEG_MASK_OWN     = 0x00000000FFFF0000;
constexpr uint64_t OPER_SEG_MASK_PAYLOAD = 0xFFFFFFFF00000000;

// 整字空值(⇒ 非控件来源: 无参帧间挂载 / 直接调用)
constexpr uint64_t OPER_NONE = 0x0;


// ================================
// 区域: 公共段(前 16 位)
// ================================

// 搭载标志(公共段 [15:14])
constexpr uint16_t OPER_COMMON_PAYLOAD_SHIFT = 14;		// 搭载标志在公共段内的移位
constexpr uint16_t OPER_COMMON_PAYLOAD_MASK  = 0xC000;	// 搭载标志掩码(段内值, 高位两位)
constexpr uint16_t OPER_COMMON_CODE_MASK     = 0x3FFF;	// 公共码位域(低 14 位; 标志位永不参与)

// 搭载标志取值
constexpr uint16_t OPER_PAYLOAD_NONE   = 0;		// 未搭载(搭载段恒 0)
constexpr uint16_t OPER_PAYLOAD_OPCODE = 1;		// 搭载操作码(从属部件码; 低 16 = 码, 高 16 = 附加)
constexpr uint16_t OPER_PAYLOAD_DATA   = 2;		// 搭载数据(32 位按约定解释)
constexpr uint16_t OPER_PAYLOAD_MIXED  = 3;		// 码 + 数据混合(低 16 = 码, 高 16 = 数据)

// 公共码(位标志; 组 1 = 指针点击, 组 2 = 激活, 组 3 = 拖放)
constexpr uint16_t OPER_COMMON_NONE        = 0x0000;	// 无公共码(未使用公共段)
constexpr uint16_t OPER_COMMON_CLICKED     = 0x0001;	// 完整单击(左键按下并在本控件命中区内释放)
constexpr uint16_t OPER_COMMON_DOUBLECLICK = 0x0002;	// 双击
constexpr uint16_t OPER_COMMON_RIGHTCLICK  = 0x0004;	// 右键(上下文菜单入口)
constexpr uint16_t OPER_COMMON_ACTIVE      = 0x0008;	// 激活: 本帧获得激活态
constexpr uint16_t OPER_COMMON_DEACTIVATED = 0x0010;	// 失去激活: 本帧释放激活态
constexpr uint16_t OPER_COMMON_HOLD        = 0x0020;	// 按住中(电平)
constexpr uint16_t OPER_COMMON_DRAG        = 0x0040;	// 拖动本控件(沿: 开始拖动)
constexpr uint16_t OPER_COMMON_DROP        = 0x0080;	// 已接收投放(目标侧)

// 命名空间: OperCode(续) —— 公共段辅助
namespace OperCode
{
	// 函数: GetCommonCode
	// 描述: 取公共码(剔除搭载标志与预留位, 只留公共码位域)
	constexpr uint16_t GetCommonCode(uint64_t code)
	{
		return (uint16_t)(GetSegment(code, OPER_SEG_COMMON) & OPER_COMMON_CODE_MASK);
	}

	// 函数: HasCommon
	// 描述: 测试公共段的位标志(单个位或若干位组合; 已剔除搭载标志)
	constexpr BOOL HasCommon(uint64_t code, uint16_t bit)
	{
		return ((GetCommonCode(code) & bit) != 0) ? TRUE : FALSE;
	}

	// 函数: GetPayloadKind
	// 描述: 取搭载标志(OPER_PAYLOAD_*; 判"有无搭载"只看本标志, 不看搭载段是否为 0)
	constexpr uint16_t GetPayloadKind(uint64_t code)
	{
		return (uint16_t)((GetSegment(code, OPER_SEG_COMMON) & OPER_COMMON_PAYLOAD_MASK) >> OPER_COMMON_PAYLOAD_SHIFT);
	}

	// 函数: MakeCommon
	// 描述: 拼公共段段内值(公共码位域 + 搭载标志; 高位标志位先掩掉再合成)
	constexpr uint16_t MakeCommon(uint16_t code, uint16_t kind)
	{
		return (uint16_t)((code & OPER_COMMON_CODE_MASK) | ((kind & 0x3) << OPER_COMMON_PAYLOAD_SHIFT));
	}
}

// ================================
// 区域: 专属段(次 16 位)
// ================================

// 命名空间: EDIT(编辑族: INPUT_* / COMBO / SLIDER)
constexpr uint16_t OPER_EDIT_SUBMIT  = 0x0001;	// 编辑提交(回车/失焦且有改动)
constexpr uint16_t OPER_EDIT_CHANGED = 0x0002;	// 值/选项已改变
constexpr uint16_t OPER_EDIT_CANCEL  = 0x0003;	// 编辑取消(Esc / 恢复原值)

// 命名空间: CUSTOM(我方自定义的组合部件/控件; 子空间按控件)
constexpr uint16_t OPER_CUSTOM_IA_INPUT_SUBMIT  = 0x0001;	// 输入段提交
constexpr uint16_t OPER_CUSTOM_IA_BTN_CLICKED   = 0x0002;	// 按钮段点击
constexpr uint16_t OPER_CUSTOM_CA_COMBO_CHANGED = 0x0001;	// 下拉段变更
constexpr uint16_t OPER_CUSTOM_CA_BTN_CLICKED   = 0x0002;	// 按钮段点击

// 命名空间: LIST(列表族)
constexpr uint16_t OPER_LIST_ROW_ACTIVATED         = 0x0001;	// 行被激活(点选)
constexpr uint16_t OPER_LIST_ROW_SUBMIT            = 0x0002;	// 行文本提交(改名)
constexpr uint16_t OPER_LIST_ROW_BTN_CLICKED       = 0x0003;	// 行内按钮点击
constexpr uint16_t OPER_LIST_ROW_SELECTION_CHANGED = 0x0004;	// 行内下拉变更
constexpr uint16_t OPER_LIST_ROW_MOVED             = 0x0005;	// 行被拖拽重排
constexpr uint16_t OPER_LIST_ADD                   = 0x0006;	// 特殊行新增
constexpr uint16_t OPER_LIST_SCROLLED              = 0x0007;	// 列表滚动

// 命名空间: STRUCT(结构件族)
constexpr uint16_t OPER_STRUCT_TITLE_CLOSE = 0x0001;	// 关闭按钮被点击(请求关闭窗口)

// 命名空间: 类型单列(上述分类无法覆盖时)
constexpr uint16_t OPER_PROG_UPDATED       = 0x0001;	// 进度已更新(电平型)
constexpr uint16_t OPER_COMBO_OPENED       = 0x0001;	// 下拉展开(暂未产出)
constexpr uint16_t OPER_COMBO_CLOSED       = 0x0002;	// 下拉收起(暂未产出)
constexpr uint16_t OPER_SLIDER_THUMB_LEFT  = 0x0001;	// 左柄(旧双柄口径遗留)
constexpr uint16_t OPER_SLIDER_THUMB_RIGHT = 0x0002;	// 右柄(同上)

// ================================
// 区域: 操作码不变量(编译期校验)
// ================================
constexpr uint64_t OPER_SEG_PROBE = OperCode::MakeCode(0x1234, 0x5678, 0x9ABCDEF0);
static_assert(OperCode::GetSegment(OPER_SEG_PROBE, OperCode::OPER_SEG_COMMON) == 0x1234, "操作码公共段位置/宽度错");
static_assert(OperCode::GetSegment(OPER_SEG_PROBE, OperCode::OPER_SEG_OWN) == 0x5678, "操作码专属段位置/宽度错");
static_assert(OperCode::GetSegment(OPER_SEG_PROBE, OperCode::OPER_SEG_PAYLOAD) == 0x9ABCDEF0, "操作码搭载段位置/宽度错");
static_assert(OperCode::MakeCode(0, 0, 0) == OPER_NONE, "操作码三段全空必须等于 OPER_NONE");
static_assert(OperCode::IsWithin(OPER_SEG_PROBE, OperCode::OPER_SEG_OWN) != FALSE, "操作码 IsWithin 判据错");
static_assert(OperCode::IsWithin(OPER_SEG_PROBE, OperCode::OPER_SEG_COUNT) == FALSE, "操作码非法段应判无码");
static_assert(OperCode::GetCommonCode(OperCode::MakeCommon(OPER_COMMON_CLICKED, OPER_PAYLOAD_NONE)) == OPER_COMMON_CLICKED, "公共码取码错");
static_assert(OperCode::HasCommon(OperCode::MakeCommon(OPER_COMMON_CLICKED, OPER_PAYLOAD_NONE), OPER_COMMON_CLICKED) != FALSE, "公共位测试错");
static_assert(OperCode::GetPayloadKind(OperCode::MakeCommon(OPER_COMMON_CLICKED, OPER_PAYLOAD_DATA)) == OPER_PAYLOAD_DATA, "搭载标志取码错");
static_assert(OperCode::MakeCodeCommon(0x1234) == OperCode::MakeCode(0x1234, 0, 0), "MakeCodeCommon 与 MakeCode 不等价");
static_assert(OperCode::MakeCodeOwn(0x5678) == OperCode::MakeCode(0, 0x5678, 0), "MakeCodeOwn 与 MakeCode 不等价");
static_assert(OperCode::MakeCodeOwn(OPER_EDIT_SUBMIT) ==
	OperCode::MakeCode(OperCode::MakeCommon(OPER_COMMON_NONE, OPER_PAYLOAD_NONE), OPER_EDIT_SUBMIT, 0),
	"编辑族提交码构造须与 MakeCode(MakeCommon(NONE,NONE), OPER_EDIT_SUBMIT, 0) 一致");

// ================================
// 区域: 特定类型控件约定
// 说明: 逐类型的缓冲布局与容量归 CtrlBuffer.h; 本区只写与类型身份、事件、参数槽相关的约定。
// ================================

// ================
// CTRL_TYPE_TITLE 约定
// 说明: 标题栏的数据 = 控件缓冲(标题文本字符串); 状态经回调函数传入的
//       操作码(pfnCtrlEvent 的 oper)回传(状态不写入缓冲)。
// ================

// 操作码约定

// 参数槽约定

// 函数: PackWndPos
// 描述: 把窗口期望位置打包进事件的通用参数槽(低 32 位 = x, 高 32 位 = y)
inline uint64_t PackWndPos(CONST vec2& pos)
{
	CONST INT32 nX = (INT32)pos.x;
	CONST INT32 nY = (INT32)pos.y;
	return (uint64_t)(uint32_t)nX | ((uint64_t)(uint32_t)nY << 32);
}

// 函数: UnpackWndPos
// 描述: 从事件的通用参数槽解出窗口期望位置(与 PackWndPos 互逆)
inline vec2 UnpackWndPos(uint64_t param)
{
	CONST INT32 nX = (INT32)(uint32_t)(param & 0xFFFFFFFFull);
	CONST INT32 nY = (INT32)(uint32_t)(param >> 32);
	return vec2((FLOAT)nX, (FLOAT)nY);
}

// ================================
// 区域: 列表控件约定(LIST_*)
// ================================

// 结构: ListBuffer
struct ListBuffer
{
	DWORD dwSelected;	// 选中项(0xFFFFFFFF = 未选)
	DWORD dwCount;		// 项目数量
	DWORD dwCapacity;	// 项目容量
	DWORD cbItem;		// 每行数据长度(自描述; 行为端装载期写入, 渲染端按此步进)
	// 行数据区(紧随本结构之后):
	//     [特殊行数据块 cbItem] + [项目数据块 cbItem × dwCapacity]
	// 行数据块内容由各类型自己的约定解释(LIST_INPUT = INPUT_ACTION 块; LIST_COMBO = 文本)
};

// 常量: 列表选中哨兵与特殊行标志
constexpr DWORD LIST_SEL_NONE = 0xFFFFFFFF;		// 无选中项
constexpr DWORD LIST_SPECIAL_FIRST = 0;			// 特殊行在前(低 16 位取值)
constexpr DWORD LIST_SPECIAL_LAST = 1;			// 特殊行在后(低 16 位取值)
