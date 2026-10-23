// ================================
// 文件: CtrlBuffer.cpp
// 描述: 控件缓冲的装配与回收实现 —— 依控件类型分配/初始化缓冲、按容量写回文本、
//       回收单枚控件的自身缓冲(接口契约见同目录 CtrlBuffer.md)
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>					// DOUBLE / DWORD 等基础类型(winnt.h)
#include <stdlib.h>						// atof / atoi(声明文本到数值的初值换算)
#include <string.h>						// strcpy_s / strncpy_s(文本写入)
#include "framework/base/CtrlBuffer.h"
#include "framework/base/mem/AllocatorHub.h"		// 控件缓冲的分配/回收原语

// ================================
// 命名空间: CtrlBuffer(控件缓冲的构造与释放)
// 说明: 本命名空间内的函数逐枚无状态被调用; 缓冲一律取自会话小对象池(AllocatorHub),
//       由 FreeCtrlBuffer 成对回收。
// ================================
namespace CtrlBuffer
{

	// ================================
	// 区域: 常量定义区
	// ================================
	// 常量: LIST_CAPACITY
	// 描述: 列表类控件的项目容量(项目块个数)
	constexpr DWORD LIST_CAPACITY = 64;

	// ================================
	// 区域: 控件缓冲构造实现
	// ================================

	// 函数: PrepareCtrlBuffer
	// 描述: 依据控件类型为控件分配/初始化缓冲区(文本/数值/复合数据; 见 CtrlBuffer.h)
	BOOL PrepareCtrlBuffer(CtrlInfo* info, LPCSTR text)
	{
		if (info == NULL)
		{
			return FALSE;
		}
		// 0. 渲染层不认识 / 无缓冲语义的类型: 保留控件对象, 不建缓冲。三类:
		//    ① 未知类型串(枚举 = CTRL_TYPE_UNKNOWN): 编辑层可感知、渲染层按矩形占位;
		//    ② 未使用槽位(枚举 = CTRL_TYPE_INVALID): 线性布局器预留, 渲染层不可见;
		//    ③ 无缓冲语义的类型(LIST/TIMEBAR/LIST_TIMESPAN): 只保留控件对象、不建缓冲。
		if (info->IsKnownType() == FALSE ||
			info->type == CTRL_TYPE_LIST || info->type == CTRL_TYPE_TIMEBAR ||
			info->type == CTRL_TYPE_LIST_TIMESPAN)
		{
			return TRUE;
		}
		// 0-bis. 输入动作控件(INPUT_ACTION): 组合控件缓冲 = 定长头(split) + 按钮文本区 + 输入文本区。
		//       初值: split = CTRL_INPUT_ACTION_SPLIT(行块初始化标志);
		//             按钮文案 = 缺省文案(可经缓冲改写); 输入文本 = JSON 声明文本(text)。
		if (info->type == CTRL_TYPE_INPUT_ACTION)
		{
			info->bufferInfo = AllocatorHub::AllocSmall(CTRL_INPUT_ACTION_BUF);
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			{
				CtrlInputActionData* pAction = (CtrlInputActionData*)info->bufferInfo.buffer;
				pAction->split = CTRL_INPUT_ACTION_SPLIT;
				// 按钮文本区(data + 0)
				{
					CHAR* lpBtn = CtrlInputActionBtnText(pAction);
					if (lpBtn != NULL)
					{
						strcpy_s(lpBtn, (size_t)CTRL_INPUT_ACTION_ACTION, CTRL_DEFAULT_BROWSE_BTN);
					}
				}
				// 输入文本区(data + split; 容量 = 缓冲总长 - 定长头 - split)
				{
					CHAR* lpInput = CtrlInputActionText(pAction);
					CONST size_t cbInput = (size_t)(info->bufferInfo.size - sizeof(CtrlInputActionData) - CTRL_INPUT_ACTION_SPLIT);
					if (lpInput != NULL && cbInput > 0 && text != NULL && text[0] != '\0')
					{
						strcpy_s(lpInput, cbInput, text);
					}
				}
			}
			return TRUE;
		}
		// 0-ter. 复选框控件: 缓冲 = 勾选态 + 右侧显示文字
		//        (一体定长; 见 CtrlBuffer.h 的 CtrlCheckData)。
		//        初值: 勾选态恒 FALSE(未勾选; 开关的默认值由应用在装配期写入);
		//             显示文字 = JSON 声明文本(text)。
		//        本类型不归入文本类(CtrlTypeIsTextKind) —— 其文字是结构内字段, 不参与
		//          "文本往返 / 属性编辑"通道 ⇒ 必须在本处(文本类分支之前)单独分配。
		if (info->type == CTRL_TYPE_CHECKBOX)
		{
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlCheckData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			{
				CtrlCheckData* pCheck = (CtrlCheckData*)info->bufferInfo.buffer;
				if (text != NULL && text[0] != '\0')
				{
					strcpy_s(pCheck->szLabel, (size_t)CTRL_LABEL_BUF, text);
				}
			}
			return TRUE;
		}
		// 0-quater. 多行文本控件: 缓冲比单行大(长文本用途
		//          见 CtrlBuffer.h 的 CTRL_TEXT_MULTI_BUF) ⇒ 必须在通用文本类分支之前按类型单独定容量,
		//          否则会落到 CTRL_TEXT_BUF(256) 而在长提示词上静默截断。
		//          其余口径与单行一致: 初值 = JSON 声明文本(text); 可编辑(容量有效)。
		if (info->type == CTRL_TYPE_INPUT_MULTI)
		{
			info->bufferInfo = AllocatorHub::AllocSmall(CTRL_TEXT_MULTI_BUF);
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			{
				CHAR* lpText = CtrlTextBuffer(info);
				if (lpText != NULL && text != NULL)
				{
					strcpy_s(lpText, info->bufferInfo.size, text);
				}
			}
			return TRUE;
		}
		// ---- 属性驱动的分组缓冲 ----
		// 1. 文本类控件(可编辑文本缓冲)
		if (CtrlTypeIsTextKind(info->type))
		{
			info->bufferInfo = AllocatorHub::AllocSmall(CTRL_TEXT_BUF);
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			// 文本写入: 以分配到的实际容量为界
			{
				char* lpText = CtrlTextBuffer(info);
				if (lpText && text)
				{
					strcpy_s(lpText, info->bufferInfo.size, text);
				}
			}
			return TRUE;
		}
		// 2. 列表类控件(LIST_INPUT/LIST_COMBO): 缓冲 = ListBuffer 头 + [特殊行块] + [项目块 × 容量];
		//    容量与行长由本函数写入缓冲头 —— 渲染端只按头部字段工作。
		if (CtrlTypeIsList(info->type))
		{
			// 两列表类型的每行数据长度同为 INPUT_ACTION 块: LIST_INPUT 全部行为可编辑输入行;
			// LIST_COMBO 项目行为只读输入行(文本 = 该控件类型显示名), 其"新增行"(特殊行)按
			// CtrlComboData 解释同长度块的前部(选项表 / 数量 / 选中索引 / 按钮文案)
			const DWORD cbItem = CTRL_INPUT_ACTION_BUF;
			const DWORD dwCapacity = LIST_CAPACITY;
			const DWORD cbBuffer = (DWORD)(sizeof(ListBuffer) + (size_t)cbItem * (dwCapacity + 1));
			info->bufferInfo = AllocatorHub::AllocSmall(cbBuffer);
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			ListBuffer* pList = (ListBuffer*)info->bufferInfo.buffer;
			pList->dwSelected = LIST_SEL_NONE;	// 未选中
			pList->dwCount = 0;					// 空列表(行数据由该列表绑定的事件处理器填)
			pList->dwCapacity = dwCapacity;		// 项目容量(供处理器/渲染端校验)
			pList->cbItem = cbItem;				// 每行数据长度(自描述)
			return TRUE;
		}
		switch (info->type)
		{
		case CTRL_TYPE_WND:
		case CTRL_TYPE_CNTR:
		case CTRL_TYPE_RECT:		// RECT 控件: 无文本/数值/复合缓冲(外观 = 纯矩形绘制)
		{
			// 结构件与 RECT 类控件无需缓冲区
			return TRUE;
		}
		case CTRL_TYPE_INPUT_REAL:
		{
			// 实数输入框: double 缓冲
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(DOUBLE));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			*(DOUBLE*)info->bufferInfo.buffer = (text && text[0]) ? atof(text) : 0.0;
			return TRUE;
		}
		case CTRL_TYPE_INPUT_NUMERIC:
		{
			// 整数数字输入框: INT 缓冲
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(INT));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			*(INT*)info->bufferInfo.buffer = (text && text[0]) ? atoi(text) : 0;
			return TRUE;
		}
		case CTRL_TYPE_COMBO:
		case CTRL_TYPE_COMBO_ACTION:
		{
			// 下拉框 / 下拉动作: 结构 + 选项字符串指针数组(两类型缓冲同构)
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlComboData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			CtrlComboData* combo = (CtrlComboData*)info->bufferInfo.buffer;
			combo->count = 1;
			combo->index = 0;
			combo->items = NULL;
			// 选项指针数组与默认选项字符串: 提供 1 个可选默认项, 保证渲染可显示一项;
			// 数组/字符串由内存池整生命周期持有、退出统一释放 —— 无逐项释放。
			BufferInfo arrbuf = AllocatorHub::AllocSmall(sizeof(LPCSTR));
			if (arrbuf.buffer == NULL) { return FALSE; }
			combo->items = (LPCSTR*)arrbuf.buffer;
			combo->items[0] = AllocatorHub::CopyString((text && text[0]) ? text : CTRL_DEFAULT_COMBO_ITEM);
			return TRUE;
		}
		case CTRL_TYPE_SLIDER:
		{
			// 滑块(单手柄口径, 见 CtrlBuffer.h 的 CtrlSliderData): 结构缓冲
			// 初值 = JSON 声明文本(text → atoi)作手柄当前值, 值域缺省 0 ~ 100
			// (值域由应用按业务语义写入)
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlSliderData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			CtrlSliderData* slider = (CtrlSliderData*)info->bufferInfo.buffer;
			slider->vMin = (text && text[0]) ? atoi(text) : 0;
			slider->lMin = 0;
			slider->lMax = 100;
			return TRUE;
		}
		case CTRL_TYPE_PROG:
		{
			// 进度条: 进度结构缓冲(默认可见, 期望 100)
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlProgData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			CtrlProgData* prog = (CtrlProgData*)info->bufferInfo.buffer;
			prog->expect = 100;
			prog->IsVisible = TRUE;
			return TRUE;
		}
		case CTRL_TYPE_RENDER:
		{
			// 渲染面: 缓冲 = 纹理视图 + 纹理尺寸。
			// 初值全空(零值 ⇒ 无图) ⇒ 渲染层画"占位面"; 纹理与尺寸由宿主注入
			// (见 CtrlBuffer.h 的 CtrlRenderData)。
			info->bufferInfo = AllocatorHub::AllocSmall(sizeof(CtrlRenderData));
			if (info->bufferInfo.buffer == NULL) { return FALSE; }
			RtlZeroMemory(info->bufferInfo.buffer, info->bufferInfo.size);
			return TRUE;
		}
		default:
			// 兜底: 类型枚举未在上方分配缓冲 ⇒ 返回失败, 调用方据此否决该文档
			// (未知类型串 / 未使用槽位走函数开头的 IsKnownType()==FALSE 早退分支)。
			return FALSE;
		}
	}

	// ================================
	// 区域: 控件文本写回实现
	// ================================

	// 函数: WriteCtrlTextToBuffer
	// 描述: 把中间态文本按控件缓冲容量写回该控件(见 CtrlBuffer.h)
	VOID WriteCtrlTextToBuffer(CtrlInfo* pCtrl, LPCSTR text)
	{
		if (pCtrl == NULL)
		{
			return;
		}
		// 文本首址/容量 = CtrlBuffer.h 的合一取址(非文本类返回 NULL/0 ⇒ 本函数对其无操作)
		CtrlTextRange range = CtrlTextRangeOf(pCtrl);
		if (range.lpText == NULL || range.cbCapacity == 0)
		{
			return;
		}
		strncpy_s(range.lpText, range.cbCapacity, (text != NULL) ? text : "", _TRUNCATE);
	}

	// ================================
	// 区域: 控件缓冲回收实现
	// ================================

	// 函数: FreeCtrlBuffer
	// 描述: 回收一枚控件的自身缓冲(含下拉类控件的选项指针数组), 并把 bufferInfo 置空(见 CtrlBuffer.h)
	VOID FreeCtrlBuffer(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		// 1. 下拉类控件: 选项字符串指针数组(装配期分配的附属块) —— 只释放数组本身;
		//    数组内字符串由分配方按其生命周期管理, 不归本函数释放。
		if ((info->type == CTRL_TYPE_COMBO || info->type == CTRL_TYPE_COMBO_ACTION) &&
			info->bufferInfo.buffer != NULL && info->bufferInfo.size >= sizeof(CtrlComboData))
		{
			CtrlComboData* pCombo = (CtrlComboData*)info->bufferInfo.buffer;
			if (pCombo->items != NULL)
			{
				AllocatorHub::FreeSmall(BufferInfo{ (VOID*)pCombo->items, sizeof(LPCSTR) });
			}
			pCombo->items = NULL;
		}
		// 2. 控件自身缓冲
		AllocatorHub::FreeSmall(info->bufferInfo);
		info->bufferInfo = BufferInfo{ NULL, 0 };
	}

}	// namespace CtrlBuffer
