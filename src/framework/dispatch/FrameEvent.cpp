#include <stdio.h>
#include "framework/dispatch/FrameEvent.h"

// ================================
// 类型: FrameEvent
// 描述: 渲染帧间事件队列(纯静态) —— 实现
// ================================

// ================================
// 区域: 模块级状态
// ================================

// 队列存储: 手写环形队列(容量 = FRAME_EVENT_CAPACITY; 存储即槽位池)
static FrameEvent::Entry s_aQueue[FrameEvent::FRAME_EVENT_CAPACITY]{};
static DWORD s_dwHead = 0;			// 出队游标(下一个待执行槽位)
static DWORD s_dwTail = 0;			// 入队游标(下一个空槽位)
static BOOL s_bDraining = FALSE;	// 排空流程活跃标记(决定 IsInFrame 的返回值; TRUE ⇒ 帧末排空执行中)

// 函数: Post
// 描述: 把一枚事件挂载到帧间队列(等待本渲染帧结束之后执行)
BOOL FrameEvent::Post(pfnCtrlEvent pfnEvent, VOID* pObj, uint64_t qwOper, uint64_t qwParam, void* pData)
{
	BOOL bRet = FALSE;
	if (pfnEvent == NULL)
	{
		return bRet;
	}
	// 1. 合并判定: 逐枚比较队列内已有挂载项(pObj/qwOper/qwParam/pData 全等才算同一项)
	for (DWORD i = s_dwHead; i != s_dwTail; i = (i + 1) % FRAME_EVENT_CAPACITY)
	{
		CONST Entry& ev = s_aQueue[i];
		if (ev.pfnEvent == pfnEvent && ev.pObj == pObj && ev.qwOper == qwOper &&
			ev.qwParam == qwParam && ev.pData == pData)
		{
			return TRUE;
		}
	}
	// 2. 入队(环形队列留一格区分空/满)
	DWORD dwNext = (s_dwTail + 1) % FRAME_EVENT_CAPACITY;
	if (dwNext == s_dwHead)
	{
		// 环形队列留一格区分空/满 ⇒ 容量 -1 枚
		printf("[FRAME] 帧间事件队列已满(%u 枚): 丢弃一枚挂载项\n", (unsigned)(FRAME_EVENT_CAPACITY - 1));
		return bRet;
	}
	Entry& slot = s_aQueue[s_dwTail];
	slot.pfnEvent = pfnEvent;
	slot.pObj = pObj;
	slot.qwOper = qwOper;
	slot.qwParam = qwParam;
	slot.pData = pData;
	s_dwTail = dwNext;
	bRet = TRUE;
	return bRet;
}

// 函数: Drain
// 描述: 执行当前全部挂载项(排空队列) —— 调用方 = 宿主主循环(两窗口的渲染帧均结束之后)
VOID FrameEvent::Drain()
{
	if (s_bDraining)
	{
		// 防重入: 排空流程已在执行
		printf("[FRAME] 帧间排空已在执行: 忽略重复调用\n");
		return;
	}
	s_bDraining = TRUE;
	while (s_dwHead != s_dwTail)
	{
		Entry ev = s_aQueue[s_dwHead];
		s_dwHead = (s_dwHead + 1) % FRAME_EVENT_CAPACITY;
		if (ev.pfnEvent != NULL)
		{
			ev.pfnEvent(ev.pObj, ev.qwOper, ev.qwParam, ev.pData);
		}
	}
	// 排空收尾: 游标复位 + 槽位清零(下一轮从空队列开始)
	s_dwHead = 0;
	s_dwTail = 0;
	for (DWORD i = 0; i < FRAME_EVENT_CAPACITY; i++)
	{
		s_aQueue[i] = Entry{};
	}
	s_bDraining = FALSE;
}

// 函数: IsInFrame
// 描述: 查询当前是否处于渲染帧内 —— 帧间事件的时机判定入口
BOOL FrameEvent::IsInFrame()
{
	return (s_bDraining == FALSE);
}

// 函数: IsDraining
// 描述: 查询帧末排空流程是否活跃(与 IsInFrame 互为反面: 排空活跃 ⇔ IsInFrame 为假)
BOOL FrameEvent::IsDraining()
{
	return s_bDraining;
}
