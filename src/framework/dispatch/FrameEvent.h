#pragma once
#include <minwindef.h>
#include <stdint.h>		// uint64_t(回调四元组的 oper/param)
#include "framework/base/FuncPtr.h"	// pfnCtrlEvent(事件处理函数指针类型)

// ================================
// 类型: FrameEvent
// 描述: 渲染帧间事件队列(纯静态); 承载"需要等渲染帧结束之后再执行"的事件
// 说明: 事件回调分两类:
//       ① 帧内触发(默认): 控件渲染过程中立即调用处理函数;
//       ② 帧间触发(本模块): 处理函数在函数最前面用 IsInFrame() 判定 ——
//          为真(渲染帧内触发)⇒ Post() 把自身挂到本队列并返回; 为假(帧末排空)⇒ 继续执行函数内业务。
//       队列容量固定, 满时挂载失败并打印一条诊断。
// ================================
class FrameEvent
{
private:
	FrameEvent() = delete;
	~FrameEvent() = delete;
	FrameEvent(const FrameEvent&) = delete;
	FrameEvent& operator=(const FrameEvent&) = delete;

public:
	// 结构: Entry
	// 描述: 一枚帧间事件(字段按大小降序排布, 便于核对对齐)
	// 备注: 成员与 pfnCtrlEvent 的四元组一一对应 ⇒ 排空时可按原调用形状回传
	struct Entry
	{
		pfnCtrlEvent pfnEvent;	// 事件处理函数 | 8 Byte(x64)
		VOID* pObj;				// 触发的控件对象(挂载时原值保存; 帧间执行时可能已失效)
		void* pData;			// 附加数据指针(同上)
		uint64_t qwOper;		// 操作码
		uint64_t qwParam;		// 参数
	};

	// 池容量 64(块大小 8+8+8+8+8 = 40 Byte; x64 下 64 × 40 = 2560 Byte)
	static constexpr DWORD FRAME_EVENT_CAPACITY = 64;

	// 函数: Post
	// 描述: 把一枚事件挂载到帧间队列(等待本渲染帧结束之后执行)
	static BOOL Post(pfnCtrlEvent pfnEvent, VOID* pObj, uint64_t qwOper, uint64_t qwParam, void* pData);

	// 函数: Drain
	// 描述: 执行当前全部挂载项(排空队列) —— 调用方 = 宿主主循环(两窗口的渲染帧均结束之后)
	static VOID Drain();

	// 函数: IsInFrame
	// 描述: 查询当前是否处于渲染帧内 —— 帧间事件的时机判定入口(处理函数最前面调用)
	static BOOL IsInFrame();

	// 函数: IsDraining
	// 描述: 查询帧末排空流程是否活跃(与 IsInFrame 互为反面: 排空活跃 ⇔ IsInFrame 为假)
	static BOOL IsDraining();
};
