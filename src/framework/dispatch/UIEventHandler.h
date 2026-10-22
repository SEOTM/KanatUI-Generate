#pragma once
#include "framework/base/FuncPtr.h"
// ================================
// 类名: UIEventHandler
// 描述: UI 事件处理类(纯静态)
// 说明: 承载布局 JSON 中控件的 event 名 → 处理函数(回调)的绑定表;
//       未登记的事件名 ⇒ BindEventHandler 返回 pfnEvent == NULL(控件无响应)。
// 绑定表由宿主注入: 本模块属 framework, 而绑定表的内容是应用侧的处理函数
//   ⇒ 直接引用会形成 framework → 应用层的反向依赖(模块级环);
//   宿主在启动期把该表注入本模块 ——
//   本模块只认 pfnCtrlEvent(契约层), 不认识任何具体模块 ⇒ 该环断开。
// 注入时机: 必须早于首次装配控件(Dispatch::Init 载入文档时即解析 event 名);
//   未注入(或表为空)时所有 event 名一律解析为空绑定(控件无响应; 静默降级, 不崩)。
// ================================
class UIEventHandler
{
private:
	UIEventHandler() = delete;
	~UIEventHandler() = delete;
	UIEventHandler(const UIEventHandler&) = delete;
	UIEventHandler& operator=(const UIEventHandler&) = delete;

public:
	// 结构: EventBinding
	// 描述: 一条"事件名 → 处理函数"绑定
	// 说明: 两类匹配 ——
	//   · 完整名: chDigitLo == '\0' ⇒ 按 strcmp 精确匹配 szEvent;
	//   · 前缀家族: chDigitLo != '\0' ⇒ szEvent 为前缀, 其后恰一位数字且落在
	//     [chDigitLo, chDigitHi]、再以 '\0' 结尾(例: "GENUI_EXP_R" + '0'..'5')。
	//   表按数组序线性匹配、命中即止(现表内各条互不重叠 ⇒ 顺序不影响结果)。
	struct EventBinding
	{
		LPCSTR szEvent;			// 事件名(完整名) 或 前缀家族的前缀
		pfnCtrlEvent pfn;		// 处理函数(非 NULL)
		CHAR chDigitLo;			// 前缀家族的数字下界; '\0' = 本项按完整名精确匹配
		CHAR chDigitHi;			// 前缀家族的数字上界(chDigitLo != '\0' 时有效)
	};

	// 函数: SetBindings
	// 描述: 注入"事件名 → 处理函数"绑定表(宿主启动期调用一次; 须早于首次装配控件)
	static VOID SetBindings(CONST EventBinding* pTable, size_t nCount);

	// 函数: BindEventHandler
	// 描述: 按事件名取绑定的事件处理函数(绑定表未覆盖的名字返回空绑定)
	static EventInfo BindEventHandler(LPCSTR eventname);
};
