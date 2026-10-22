#pragma once
#include <minwindef.h>
#include <stddef.h>

// 前置声明(本文件只经指针引用这三个类型)
class Arena;
class MemPool;
class Layout;

// ================================
// 文件: LayoutModel.h
// 描述: 布局数据模型(布局模型): json/bin 文件与 UI 布局对象(Layout)之间的统一内存状态(接口契约见同目录 LayoutModel.md)
// ================================

// ================================
// 区域: 定长字符串上限
// ================================
constexpr int STR_NAME  = 64;		// 布局名(含结尾符)
constexpr int STR_TYPE  = 64;		// 元素类型串
constexpr int STR_LABEL = 128;		// 元素/布局标签
constexpr int STR_TEXT  = 128;		// 元素文本
constexpr int STR_EVENT = 64;		// 元素事件名
constexpr int STR_MODE  = 64;		// 自动尺寸模式串("AUTO_SIZE_BY_*")

// ================================
// 区域: 几何系数与参考尺寸
// ================================

// 结构: SizeFactor
struct SizeFactor
{
	union
	{
		struct
		{
			FLOAT screenWidth;		// 系数 0: 倍屏幕宽度
			FLOAT screenHeight;		// 系数 1: 倍屏幕高度
			FLOAT windowWidth;		// 系数 2: 倍窗口宽度
			FLOAT windowHeight;		// 系数 3: 倍窗口高度
			FLOAT controlHeight;	// 系数 4: 倍控件高度
			FLOAT fontSize;			// 系数 5: 倍字体大小
			FLOAT space;			// 系数 6: 倍控件间隙
			FLOAT padding;			// 系数 7: 倍内部边距
		};
		FLOAT RefCoeffs[8];			// 下标视图: 按参考量序号(0..7)排列的系数数组(与上方具名成员同存储)
	} coeff;						// 元素各参考量的系数(相对于参考尺寸的比例)
	int offset;						// 固定偏移量(像素; 用于微调位置/尺寸)
};

// 常量: GENFACTOR_NUM
constexpr int GENFACTOR_NUM = 8;

// 结构: SizeFactorSet
struct SizeFactorSet
{
	SizeFactor x;		// X 位置系数
	SizeFactor y;		// Y 位置系数
	SizeFactor width;	// 宽度系数
	SizeFactor height;	// 高度系数
};

// 结构: RefSize
struct RefSize
{
	char mode[STR_MODE];	// 自动尺寸模式
	int  unit;				// 单位(0 = 千分比; 1 = 像素)
	int  width;				// 参考宽度
	int  height;			// 参考高度
	int  controlHeight;		// 参考控件高度
	int  fontSize;			// 参考字体大小
	int  space;				// 参考控件间隙
	int  padding;			// 参考内部边距
};

// ================================
// 区域: 元素与布局(中间态字段)
// ================================

// 结构: ElementSpec
struct ElementSpec
{
	char        label[STR_LABEL];	// 元素标签
	char        text [STR_TEXT ];	// 元素文本内容
	char        type [STR_TYPE ];	// 元素类型(串形态; 未命中类型表 = 未知类型)
	char        event[STR_EVENT];	// 元素事件名

	SizeFactor  x;					// X 轴位置系数
	SizeFactor  y;					// Y 轴位置系数
	SizeFactor  width;				// 宽度系数
	SizeFactor  height;				// 高度系数

	int         id;					// 元素 ID
	int         parentID;			// 父容器 ID(文档 parent 字段; -1 = 无父容器)
};

// 结构: LayoutInfo
struct LayoutInfo
{
	char        label[STR_LABEL];	// 布局标签
	char        name [STR_NAME ];	// 布局名称
	RefSize     refSize;				// 参考尺寸 / 经验参数
	SizeFactor  width;				// 布局宽度系数
	SizeFactor  height;				// 布局高度系数
	BOOL        IsMaxiable;			// 是否可最大化
	BOOL        IsResizeable;		// 是否可调整大小
	LONG        wndX;				// 窗口桌面位置 X
	LONG        wndY;				// 窗口桌面位置 Y
	DWORD       dwElements;			// 元素数量(本布局内)
};

// 结构: ElementNode
struct ElementNode
{
	ElementSpec* data;		// 元素规格(同块内)
	ElementNode* next;		// 下一元素节点
};

// 结构: LayoutNode
struct LayoutNode
{
	LayoutInfo*  info;		// 布局信息(同块内)
	ElementNode* elements;	// 元素链头
	LayoutNode*  next;		// 下一布局节点
};

// 结构: FileMidState
struct FileMidState
{
	LayoutNode   layouts;		// 布局链头(哨兵)
	DWORD        dwLayouts;		// 布局数量
	DWORD        dwElements;	// 元素总数(全部布局求和)
};

// ================================
// 区域: bin 文件格式契约(v2; 记录区 = 上列结构体的内存镜像)
// ================================
constexpr DWORD BIN_VERSION         = 0x00000200;	// 当前版本号(代际 2 + 明文标记; 代际 2 = 控件记录增 parentID)
constexpr DWORD BIN_VER_GEN_MASK    = 0xFFFFFF00;	// 版本号高 24 位: 代际版本号
constexpr DWORD BIN_VER_FLAG_MASK   = 0x000000FF;	// 版本号低 8 位: 格式标记
constexpr DWORD BIN_VER_FLAG_COMPRESS = 0x00000001;	// 格式标记: 压缩(尚未实现)
constexpr DWORD BIN_VER_FLAG_ENCRYPT  = 0x00000002;	// 格式标记: 加密(尚未实现)
constexpr int   BIN_FILE_HEAD       = 12;			// 文件头字节数: 校验码 4 + 版本号 4 + 段数量 4
constexpr int   BIN_CRC_SIZE        = 4;			// 校验码字段字节数(计算时该字段按 0 参与)
constexpr int   BIN_SEG_HEAD        = 4;			// 段头字节数: 本段大小(含本字段自身)
constexpr int   BIN_LAYOUT_SIZE     = (int)sizeof(LayoutInfo);		// 头部区字节数 = 一枚 LayoutInfo(定长)
constexpr int   BIN_LAYOUT_NELEM    = (int)offsetof(LayoutInfo, dwElements);	// 头部区内"控件数量"字段的偏移(372)
constexpr int   BIN_CTRL_SIZE       = (int)sizeof(ElementSpec);		// 单控件记录字节数(536)

static_assert(sizeof(LayoutInfo) == 376, "bin 头部区须与 LayoutInfo 布局一致(结构体变更请同步 BIN_VERSION)");
static_assert(offsetof(LayoutInfo, dwElements) == 372, "bin 头部区内的控件数量偏移须与 LayoutInfo 布局一致");
static_assert(sizeof(ElementSpec) == 536, "bin 控件记录须与 ElementSpec 布局一致(结构体变更请同步 BIN_VERSION)");
static_assert(offsetof(ElementSpec, parentID) == 532, "bin 控件记录内的父容器ID偏移须与 ElementSpec 布局一致");

// 函数: BinSegBytes
// 描述: 计算一个布局段的字节数(段头 + 头部区 + 控件记录区)
constexpr size_t BinSegBytes(DWORD dwCtrls)
{
	return (size_t)BIN_SEG_HEAD + (size_t)BIN_LAYOUT_SIZE + (size_t)dwCtrls * BIN_CTRL_SIZE;
}

// ================================
// 区域: 文档规模上限(JSON 与 bin 共用同一组上限)
// ================================
constexpr DWORD LAYOUT_MAX = 1024;	// 单文件布局(段)数上限
constexpr DWORD CTRL_MAX   = 8192;	// 单布局控件数上限

// ================================
// 类名: LayoutModel
// ================================
class LayoutModel
{
private:
	// 私有化构造/析构(纯静态工具类)
	LayoutModel() = delete;
	~LayoutModel() = delete;
public:
	// -------- 分配器注入 --------
	// 函数: SetAllocators
	// 描述: 注入块分配器(静态, 全进程一份; 传 NULL = 解绑)
	static VOID SetAllocators(Arena* arena, MemPool* pool);

	// -------- 定长字符串 --------
	// 函数: StrCopy
	// 描述: 把字符串安全拷入定长数组(超长按容量截断)
	static BOOL StrCopy(char* dst, size_t cap, LPCSTR src);

	// -------- 池与链 --------
	// 函数: StateInit
	// 描述: 初始化一份中间态(按布局数与各布局元素数逐块建立两条链)
	static BOOL StateInit(FileMidState& state, DWORD dwLayouts, CONST DWORD* ElementCounts);

	// 函数: StateClear
	// 描述: 释放中间态持有的全部块并把 state 复位为空态
	static VOID StateClear(FileMidState& state);

	// 函数: LayoutAdd
	// 描述: 在布局链尾追加一个空布局(分配一个布局块并尾插)
	static LayoutNode* LayoutAdd(FileMidState& state);

	// 函数: LayoutRemove
	// 描述: 移除指定布局(解链后回收其元素块与布局块)
	static BOOL LayoutRemove(FileMidState& state, DWORD layoutIndex);

	// 函数: LayoutCount
	// 描述: 遍历布局链取布局数量
	static DWORD LayoutCount(CONST FileMidState& state);

	// 函数: LayoutAt
	// 描述: 按布局索引取布局节点(越界返回 NULL)
	static LayoutNode* LayoutAt(CONST FileMidState& state, DWORD layoutIndex);

	// 函数: LayoutFirst
	// 描述: 遍历入口: 取首个布局节点(空文档返回 NULL)
	static LayoutNode* LayoutFirst(CONST FileMidState& state);

	// 函数: LayoutNext
	// 描述: 遍历推进: 取下一布局节点(链尾或入参 NULL 返回 NULL)
	static LayoutNode* LayoutNext(CONST LayoutNode* pNode);

	// 函数: ElementFirst
	// 描述: 遍历入口: 取指定布局的元素链首(无元素或入参 NULL 返回 NULL)
	static ElementNode* ElementFirst(CONST LayoutNode* pNode);

	// 函数: ElementNext
	// 描述: 遍历推进: 取下一元素节点(链尾或入参 NULL 返回 NULL)
	static ElementNode* ElementNext(CONST ElementNode* pElem);

	// 函数: ElementAt
	// 描述: 按布局索引与元素索引取元素规格(越界返回 NULL)
	static ElementSpec* ElementAt(CONST FileMidState& state, DWORD layoutIndex, DWORD elementIndex);

	// 函数: ElementInsert
	// 描述: 在指定位置插入一枚空元素(分配元素块并就地链接)
	static BOOL ElementInsert(FileMidState& state, DWORD layoutIndex, DWORD elementIndex);

	// 函数: ElementRemove
	// 描述: 移除指定元素(解链后回收其元素块)
	static BOOL ElementRemove(FileMidState& state, DWORD layoutIndex, DWORD elementIndex);

	// -------- RefSize 字段工具(按参考量序号访问) --------
	// 函数: RefSizeValueAt
	// 描述: 按参考量序号读经验参数值(序号越界返回 0)
	static INT RefSizeValueAt(CONST RefSize& refSize, INT index);
	// 函数: SetRefSizeValueAt
	// 描述: 按参考量序号写经验参数值(序号越界不写入)
	static VOID SetRefSizeValueAt(RefSize& refSize, INT index, INT value);
	// 函数: DefaultRefSize
	// 描述: 取经验参数默认样例(屏高模式 + 千分比)
	static RefSize DefaultRefSize();
};
