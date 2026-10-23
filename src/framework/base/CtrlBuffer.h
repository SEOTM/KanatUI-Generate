#pragma once
#include <minwindef.h>
#include <stdint.h>
#include <stddef.h>
#include "framework/base/ControlSpec.h"

// ================================
// 文件: CtrlBuffer.h
// 描述: 控件缓冲的约定与装配回收 —— 逐类型缓冲的数据布局、容量常量、控件缺省文案、
//       文本通道取址, 以及缓冲的分配/初始化/写回/回收(接口契约见同目录 CtrlBuffer.md)
// 说明: ① 数据布局与容量是控件库与装配期的共同约定, 只依赖 framework/base(含 mem)与 C 运行时;
//       ② 类型语义(含"是否文本类")归 framework/base/ControlSpec.h;
//       ③ 容量一律随缓冲头走(CtrlInfo::bufferInfo.size), 常量只用于装配期分配;
//       ④ 本模块不含绘制、不含 UI 库类型。
// ================================

// ================================
// 区域: 缓冲容量常量
// ================================

// 常量: CTRL_TEXT_BUF
// 描述: 单行文本类控件(TEXT / BUTTON / INPUT_SINGLE / TITLE)的缓冲字节数
constexpr DWORD CTRL_TEXT_BUF = 256;

// 常量: CTRL_TEXT_MULTI_BUF
// 描述: 多行文本控件(INPUT_MULTI)的缓冲字节数
// 说明: 多行控件用于长文本(提示词 / 负面提示词 / 参数文件内容), 单行 256 字节不够, 故单列 16 倍容量。
constexpr DWORD CTRL_TEXT_MULTI_BUF = 4096;

// 常量: CTRL_LABEL_BUF
// 描述: 控件内联显示文字的字节数(复选框右侧文字等)
// 说明: 该容量服务于"结构内字段", 与文本通道容量(CTRL_TEXT_BUF)语义分离;
//       两者当前数值相同, 但分属不同通道, 改一处不应连带改另一处。
constexpr DWORD CTRL_LABEL_BUF = 256;

// ================================
// 区域: 控件缺省文案
// 说明: 控件尚未获得应用赋值时的占位文案; 应用可按控件通道覆盖
//       (输入动作控件的按钮文本区 / CtrlComboData 的 lpBtn)。
// ================================

// 常量: CTRL_DEFAULT_ACTION_BTN
// 描述: 下拉动作控件右段按钮的缺省文案
inline constexpr LPCSTR CTRL_DEFAULT_ACTION_BTN = u8"动作";

// 常量: CTRL_DEFAULT_BROWSE_BTN
// 描述: 输入动作控件按钮文本区的缺省文案
inline constexpr LPCSTR CTRL_DEFAULT_BROWSE_BTN = u8"浏览";

// 常量: CTRL_DEFAULT_COMBO_ITEM
// 描述: 下拉类控件未获选项文本时的缺省选项
inline constexpr LPCSTR CTRL_DEFAULT_COMBO_ITEM = u8"选项 1";

// ================================
// 区域: 输入动作控件(INPUT_ACTION)缓冲
// 说明: 缓冲形态 = 定长头 + 变长载荷:
//       · 定长头 = CtrlInputActionData;
//       · 载荷 data 内: data[0, CTRL_INPUT_ACTION_SPLIT) = 按钮文本,
//         data[CTRL_INPUT_ACTION_SPLIT, ...) = 输入文本。
//       载荷起点与两个文本区的偏移都是编译期常量(见下), 运行时不再读结构字段推算。
// ================================

// 常量: CTRL_INPUT_ACTION_ACTION
// 描述: 载荷内按钮文本区的字节数(含结尾 NUL)
constexpr DWORD CTRL_INPUT_ACTION_ACTION = 64;

// 常量: CTRL_INPUT_ACTION_SPLIT
// 描述: 载荷内按钮文本区与输入文本区的分割位置(按钮文本区容量向上取整到 8 的倍数)
constexpr DWORD CTRL_INPUT_ACTION_SPLIT = ((CTRL_INPUT_ACTION_ACTION + 7u) & ~7u);

// 结构: CtrlInputActionData
// 描述: 输入动作控件缓冲的定长头(变长载荷 data 紧随其后)
struct CtrlInputActionData
{
	DWORD split;	// 行块初始化标志: 装配期写入 CTRL_INPUT_ACTION_SPLIT; 文本取址不依赖本字段
	// 尾随区: BYTE data[CTRL_INPUT_ACTION_SPLIT + CTRL_TEXT_BUF](按钮文本 + 输入文本)
};

// 常量: CTRL_INPUT_ACTION_BUF
// 描述: 输入动作控件的缓冲总字节数(= 定长头 + 按钮文本区 + 输入文本区)
// 说明: 载荷内联在定长头之后, 故总量可在编译期定死; 列表行的行块长度也取本值。
constexpr DWORD CTRL_INPUT_ACTION_BUF =
	(DWORD)(sizeof(CtrlInputActionData) + CTRL_INPUT_ACTION_SPLIT + CTRL_TEXT_BUF);

// 函数: CtrlInputActionBtnText
// 描述: 取载荷内按钮文本区首址(容量 CTRL_INPUT_ACTION_ACTION)
inline CHAR* CtrlInputActionBtnText(CtrlInputActionData* pData)
{
	if (pData == NULL)
	{
		return NULL;
	}
	return (CHAR*)((BYTE*)pData + sizeof(CtrlInputActionData));
}

// 函数: CtrlInputActionText
// 描述: 取载荷内输入文本区首址(容量 = 缓冲总长 - 定长头 - CTRL_INPUT_ACTION_SPLIT)
// 说明: 偏移取编译期常量; 结构字段 split 只作未初始化校验(不等于常量即视为尚未装配)。
inline CHAR* CtrlInputActionText(CtrlInputActionData* pData)
{
	if (pData == NULL)
	{
		return NULL;
	}
	if (pData->split != CTRL_INPUT_ACTION_SPLIT)
	{
		return NULL;
	}
	return (CHAR*)((BYTE*)pData + sizeof(CtrlInputActionData) + CTRL_INPUT_ACTION_SPLIT);
}

// ================================
// 区域: 控件缓冲数据结构
// ================================

// 结构: CtrlComboData
// 描述: 下拉类控件缓冲(COMBO 与 COMBO_ACTION 共用; 列表特殊行块也按本结构解释)
struct CtrlComboData
{
	INT count;			// 选项数量(<= 0 = 无可选项, 控件不产出像素)
	INT index;			// 当前选中索引(下拉项变更时写回)
	LPCSTR* items;		// 选项字符串指针数组: 数组本身由装配期分配, 回收路径释放数组;
						// 数组内字符串不归本结构释放(由分配方按其生命周期管理)
	LPCSTR lpBtn;		// 右段按钮文案(NULL = 用缺省文案 CTRL_DEFAULT_ACTION_BTN)
};

// 结构: CtrlProgData
// 描述: 进度控件(PROG)缓冲
// 说明: 进度值由应用在渲染前写入缓冲; 可见性属于数据而非布局属性, 隐藏进度条应写 IsVisible。
struct CtrlProgData
{
	int64_t current;		// 当前进度
	int64_t expect;			// 预期进度(分母; 0 时进度比无定义, 由绘制实现兜底)
	BOOL IsVisible;			// 是否可见
};

// 结构: CtrlRenderData
// 描述: 渲染面控件(RENDER)缓冲
// 说明: 控件只声明"图像显示矩形"; 像素来源由宿主注入(宿主创建纹理并把视图指针写入本结构),
//       控件库不持设备、不管纹理生命周期, 故视图以 void* 类型擦除, 本头文件不依赖 d3d11.h。
struct CtrlRenderData
{
	void* pTexture;		// 纹理视图(D3D11 的 ID3D11ShaderResourceView*; NULL = 无图, 画占位面)
	INT32 texWidth;		// 纹理像素宽(等比缩放基准; <= 0 视为无图)
	INT32 texHeight;	// 纹理像素高(同上)
};

// 结构: CtrlCheckData
// 描述: 复选框控件(CHECKBOX)缓冲
// 说明: 缓冲 = 勾选态 + 内联显示文字, 一体定长; 勾选态落在首字段,
//       便于应用把缓冲首址直接对接开关型数据。
struct CtrlCheckData
{
	BOOL IsChecked;				// 勾选态(TRUE = 已勾选)
	CHAR szLabel[CTRL_LABEL_BUF];	// 右侧显示文字(C 串)
};

// 结构: CtrlSliderData
// 描述: 滑块控件(SLIDER)缓冲
// 说明: 单手柄口径, 语义 = 一个当前值 + 一个值域; 值域由应用按业务语义写入。
struct CtrlSliderData
{
	int64_t vMin;			// 手柄当前值
	int64_t lMin;			// 值域下限
	int64_t lMax;			// 值域上限
};

// ================================
// 区域: 文本通道
// 说明: 文本类控件的可编辑文本以"首址 + 容量"成对给出; 唯一入口是 CtrlTextRangeOf,
//       避免只取首址而漏取容量(容量 0 表示不可写, 调用方应按只读展示处理)。
// ================================

// 结构: CtrlTextRange
// 描述: 文本通道的取址结果
struct CtrlTextRange
{
	CHAR* lpText;			// 文本首址(可写; 不适用时为 NULL)
	size_t cbCapacity;		// 可写字节容量(含结尾 NUL; 0 = 只读或不适用)
};

// 函数: CtrlTextRangeOf
// 描述: 取文本类控件的文本首址与可写容量(首址 + 容量一次给出)
inline CtrlTextRange CtrlTextRangeOf(CtrlInfo* info)
{
	CtrlTextRange range{ NULL, 0 };
	if (info == NULL || info->bufferInfo.buffer == NULL || info->bufferInfo.size == 0)
	{
		return range;
	}
	if (CtrlTypeIsTextKind(info->type) == FALSE)
	{
		return range;
	}
	CHAR* lpText = NULL;
	if (info->type == CTRL_TYPE_INPUT_ACTION)
	{
		// 组合控件: 文本 = 载荷内的输入文本区(偏移由编译期常量给出)
		if (info->bufferInfo.size < CTRL_INPUT_ACTION_BUF)
		{
			return range;
		}
		lpText = CtrlInputActionText((CtrlInputActionData*)info->bufferInfo.buffer);
	}
	else
	{
		// 其余文本类: 缓冲即文本
		lpText = (CHAR*)info->bufferInfo.buffer;
	}
	if (lpText == NULL)
	{
		return range;
	}
	const size_t nOffset = (size_t)(lpText - (CHAR*)info->bufferInfo.buffer);
	if (nOffset >= (size_t)info->bufferInfo.size)
	{
		return range;
	}
	range.lpText = lpText;
	range.cbCapacity = (size_t)info->bufferInfo.size - nOffset;
	return range;
}

// 函数: CtrlTextBuffer
// 描述: 取文本类控件的文本首址(与 CtrlTextRangeOf 同源; 保留为兼容入口)
inline CHAR* CtrlTextBuffer(CtrlInfo* info)
{
	return CtrlTextRangeOf(info).lpText;
}

// 函数: CtrlTextCapacity
// 描述: 取文本类控件的可写文本容量(与 CtrlTextRangeOf 同源; 保留为兼容入口)
inline size_t CtrlTextCapacity(CtrlInfo* info)
{
	return CtrlTextRangeOf(info).cbCapacity;
}

// ================================
// 命名空间: CtrlBuffer
// 描述: 控件缓冲的构造与释放(无模块状态; 逐枚控件为操作对象)
// ================================
namespace CtrlBuffer
{
	// 函数: PrepareCtrlBuffer
	// 描述: 依据控件类型为控件分配/初始化缓冲区(文本/数值/复合数据)
	BOOL PrepareCtrlBuffer(CtrlInfo* info, LPCSTR text);

	// 函数: WriteCtrlTextToBuffer
	// 描述: 把中间态文本按控件缓冲容量写回该控件(结构件"外观信息"刷新用; 无文本缓冲者安全忽略)
	VOID WriteCtrlTextToBuffer(CtrlInfo* pCtrl, LPCSTR text);

	// 函数: FreeCtrlBuffer
	// 描述: 回收一枚控件的自身缓冲(含下拉类控件的选项指针数组), 并把 bufferInfo 置空
	VOID FreeCtrlBuffer(CtrlInfo* info);
}
