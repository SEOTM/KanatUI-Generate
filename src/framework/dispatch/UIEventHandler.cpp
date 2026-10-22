// ================================
// 文件: UIEventHandler.cpp
// 描述: UI 事件绑定类 —— 布局 JSON 中控件的 event 名字符串经此处映射为处理函数
// 说明: ① 绑定表由宿主注入(见 UIEventHandler.h 的说明; 本模块不依赖任何具体模块 ——
//       ② 未登记的事件名 ⇒ 返回 pfnEvent == NULL(控件无响应);
//       ③ 时机: 需要帧间执行的处理器由各处理器自身经 FrameEvent::Post 挂载到帧间队列
//          (帧内触发只挂载, 帧末排空时才执行; 见 FrameEvent.h) —— 本模块不参与时机判定。
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>
#include <string.h>
#include "framework/dispatch/UIEventHandler.h"

// ================================
// 区域: 文件内静态状态
// ================================
// 注入的绑定表(只存指针; 表内存由调用方持有且进程级常驻 ⇒ 本模块不复制、不释放)
static CONST UIEventHandler::EventBinding* sBindings = NULL;
static size_t sBindingCount = 0;

// ================================
// 区域: 接口实现区
// ================================
// 函数: SetBindings
// 描述: 注入"事件名 → 处理函数"绑定表(见 .h)
VOID UIEventHandler::SetBindings(CONST EventBinding* pTable, size_t nCount)
{
	sBindings = pTable;
	sBindingCount = (pTable != NULL) ? nCount : 0;
}

// 函数: BindEventHandler
// 描述: 按事件名取绑定的事件处理函数(线性查表; 命中即止)
EventInfo UIEventHandler::BindEventHandler(LPCSTR eventname)
{
	EventInfo ret{};
	if (eventname == NULL)
	{
		return ret;
	}
	for (size_t i = 0; i < sBindingCount; i++)
	{
		CONST EventBinding& b = sBindings[i];
		if (b.szEvent == NULL || b.pfn == NULL)
		{
			continue;					// 表内空项: 跳过(容错)
		}
		if (b.chDigitLo == '\0')
		{
			// 完整名: 精确匹配
			if (strcmp(eventname, b.szEvent) == 0)
			{
				ret.pfnEvent = b.pfn;
				break;
			}
		}
		else
		{
			// 前缀家族: 前缀 + 恰一位数字(落在 [lo, hi]) + '\0'
			// 注: strncmp 在 eventname 短于前缀时即判不等 ⇒ 后续下标访问被短路, 无越界
			CONST size_t n = strlen(b.szEvent);
			if (strncmp(eventname, b.szEvent, n) == 0 &&
				eventname[n] >= b.chDigitLo && eventname[n] <= b.chDigitHi &&
				eventname[n + 1] == '\0')
			{
				ret.pfnEvent = b.pfn;
				break;
			}
		}
	}
	// 未登记的事件名: pfnEvent 保持 NULL(控件无响应)
	return ret;
}
