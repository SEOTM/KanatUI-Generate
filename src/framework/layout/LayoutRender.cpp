// ================================
// 文件: LayoutRender.cpp
// 描述: 布局级渲染出口实现 —— 基座窗口的开启/关闭、标题栏与内容容器的编排, 逐控件向控件库
//   (Controls)的分派, 以及预览布局的逐控件命中判定; 控件自身的几何与外观由控件库内部处理,
//   一切 UI 设施动作(开窗/开容器/压 ID/定位/命中判定/置顶)只经注入的渲染能力表发起。
// 说明: 本件不包含任何 UI 库头文件 —— 能力表未注入时各渲染入口静默失败(返回 FALSE), 因此
//   渲染前置条件(宿主已开帧、已注入能力)由宿主负责, 本件只做布局语义与编排。
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>
#include <stdio.h>			// snprintf(内嵌窗口名唯一化)

#include "framework/layout/LayoutRender.h"
#include "framework/base/ControlSpec.h"		// CtrlTypeIsRawGraphic / CtrlTypeIsCompound / CtrlTypeHandlesClickItself(逐控件点击判定的类型属性)

// ================================
// 区域: 模块级状态定义区
// ================================
static CONST RenderApi* pRenderApi = NULL;	// 注入的渲染能力表(NULL = 未注入 ⇒ 渲染入口静默失败)
static CONST CtrlApi* pCtrlApi = NULL;		// 注入的控件绘制接口表(NULL = 未注入 ⇒ 渲染入口静默失败)

// ================================
// 区域: 文件内函数声明区
// ================================
static VOID PushClickProbeItem(CtrlInfo* ctrl, CONST vec2& orgPanel);
static VOID DispatchCtrlClick(CtrlInfo* ctrl, size_t index, uint64_t qwItemIdBefore, CONST vec2& orgContent,
	CONST vec2& orgPanel, pfnCtrlEvent pfnClick);
static BOOL RenderLayoutCore(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, DWORD dwStates,
	pfnCtrlEvent pfnClick, pfnOverlayBefore pfnOverlay);

// ================================
// 区域: 布局渲染实现区
// ================================

// 函数: PushClickProbeItem
// 描述: 在控件矩形上经能力表补一枚无绘制命中项(矩形换算到内容容器本地坐标)
static VOID PushClickProbeItem(CtrlInfo* ctrl, CONST vec2& orgPanel)
{
	pRenderApi->AddHitArea(ctrl->label, ctrl->position - orgPanel, ctrl->size);
}

// 函数: DispatchCtrlClick
// 描述: 逐控件点击判定: 本帧命中即把点击事件派发给记录器
static VOID DispatchCtrlClick(CtrlInfo* ctrl, size_t index, uint64_t qwItemIdBefore, CONST vec2& orgContent,
	CONST vec2& orgPanel, pfnCtrlEvent pfnClick)
{
	if (ctrl == NULL || pfnClick == NULL ||
		ctrl->size.x <= 0.0f || ctrl->size.y <= 0.0f ||
		ctrl->type == CTRL_TYPE_INVALID)
	{
		return;
	}
	// 1. 控件库自身会派发点击的类型(其 pEventHandler 已是记录器) ⇒ 本处跳过(避免同一击登记两次)
	if (CtrlTypeHandlesClickItself(ctrl->type) == TRUE && ctrl->pEventHandler != NULL)
	{
		return;
	}
	// 2. 纯图元类型(只产出像素、无自带可交互项) ⇒ 补一枚覆盖控件矩形的无绘制命中项后再判定
	if (CtrlTypeIsRawGraphic(ctrl->type) == TRUE)
	{
		PushClickProbeItem(ctrl, orgPanel);
	}
	else
	{
		// 3. 自带可交互项的类型: 本帧未提交项时补一枚兜底(否则末项判定会读到上一枚控件的项)
		CONST BOOL bHasItem = (pRenderApi->TakeItemId() != qwItemIdBefore) ? TRUE : FALSE;
		if (bHasItem == FALSE)
		{
			PushClickProbeItem(ctrl, orgPanel);
		}
	}
	// 4. 点击判定(命中即派发; 控件索引走 param)
	if (pRenderApi->IsHitClicked() == TRUE)
	{
		pfnClick(ctrl, OperCode::MakeCodeCommon(OPER_COMMON_CLICKED), (uint64_t)index, ctrl->bufferInfo.buffer);
	}
	else if (CtrlTypeIsCompound(ctrl->type) == TRUE)
	{
		// 5. 复合控件的矩形兜底: UI 库的末项只指向其最后提交的那一枚子项(通常是右段按钮) ⇒ 点在其余
		//    子项区域(如输入动作的左段输入区)时上面判不出来 ⇒ 此处按控件整体矩形再判一次(不提交任何项,
		//    顶点变化恒 0)。屏幕矩形 = 内容容器的内容原点 + 控件的容器本地坐标
		//    (容器本地坐标 = 有效坐标 - 内容容器原点; 两枚原点都由调用方取一次传入 ⇒ 不受控件绘制期间
		//    "当前窗口"变化(列表内部会进入自己的容器)影响)
		CONST vec2 mn(orgContent.x + (ctrl->position.x - orgPanel.x),
			orgContent.y + (ctrl->position.y - orgPanel.y));
		CONST vec2 mx(mn.x + ctrl->size.x, mn.y + ctrl->size.y);
		if (pRenderApi->IsRectClicked(mn, mx) == TRUE)
		{
			pfnClick(ctrl, OperCode::MakeCodeCommon(OPER_COMMON_CLICKED), (uint64_t)index, ctrl->bufferInfo.buffer);
		}
	}
}

// 函数: RenderLayoutCore
// 描述: 布局渲染核心(文件内部): 开窗 + 标题栏 + 内容容器, 并遍历渲染内容控件
static BOOL RenderLayoutCore(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, DWORD dwStates,
	pfnCtrlEvent pfnClick, pfnOverlayBefore pfnOverlay)
{
	// 变量声明
	BOOL bRet = FALSE;
	size_t num = 0;
	CtrlInfo* ctrl = NULL;
	CtrlInfo* pWnd = NULL;			// 基座窗口件(承载窗口几何)
	CtrlInfo* pCntr = NULL;			// 内容容器件(承载内容控件的坐标原点与裁剪区)
	vec2 orgContent(0.0f, 0.0f);	// 内容容器的内容原点(屏幕坐标; 进入容器后赋值)
	vec2 orgPanel(0.0f, 0.0f);		// 内容容器原点(窗口本地坐标; 内容控件的有效坐标即以此为基准之外移量)
	// 特殊情况: 渲染能力或控件绘制接口未注入 ⇒ 静默失败(渲染前置条件归宿主)
	if (pRenderApi == NULL || pCtrlApi == NULL)
	{
		goto RENDER_END;
	}
	if (layout.IsInvalid())
	{
		goto RENDER_END;
	}
	// 初始化
	num = layout.GetTotalNum();
	// 取两枚结构件: 窗口件的几何决定本次渲染的目标窗口; 内容容器件的原点决定内容控件的坐标换算基准
	// (两者都必须在控件库上下文之前取到 —— 控件库按容器原点把有效坐标换算成容器本地坐标)
	pWnd = layout.GetLayoutBase(CTRL_TYPE_WND);
	pCntr = layout.GetLayoutBase(CTRL_TYPE_CNTR);
	if (pWnd == NULL || pCntr == NULL)
	{
		goto RENDER_END;
	}
	orgPanel = vec2(0.0f, pCntr->position.y);
	// 控件库上下文: 参考尺寸 / 投影配色 / 窗口位置记录槽(标题栏一类窗口级控件据此取拖动基线) /
	// 内容容器原点(内容控件按它把有效坐标换算为容器本地坐标)
	pCtrlApi->UpdateContext(refsize, pProj, &pWnd->position, orgPanel);
	// 取得本次渲染的目标窗口: 期望位置、期望客户区尺寸与窗口标题都取窗口件的记录值(不符时由实现方主动调窗)
	// 说明: "渲染到哪个窗口"不属布局语义 —— 基座布局路径在此显式取得目标; 内嵌(flex)布局走
	//       RenderFlex, 沿用当前窗口, 不调用本项; 取不到目标即静默失败(不继续本次渲染)
	if (pRenderApi->AcquireWnd(pWnd->position, pWnd->size, pWnd->label) == FALSE)
	{
		goto RENDER_END;
	}
	// 开基座窗口(窗口名取 WND 控件自身 label): 摆放位置恒为布局原点 —— 基座窗口内容始终从客户区原点
	// 铺开, 与窗口的桌面位置无关(桌面位置只经 AcquireWnd 结算; 见 Render.h 的 AcquireWnd 说明)
	pRenderApi->OpenWnd(pWnd->label, vec2(0.0f, 0.0f), pWnd->size, dwStates);
	// 标题: 结构件标题经控件绘制接口表分派(其分派表已含 TITLE ⇒ 与该类型专用绘制同一实现)
	// 说明: 标题件属结构件, 不在下方内容控件遍历范围内(遍历自 GetShellNum 起) ⇒ 必须单独送入一次
	{
		ctrl = layout.GetLayoutBase(CTRL_TYPE_TITLE);
		if (ctrl)
		{
			pCtrlApi->DrawCtrl(ctrl);
		}
	}
	// 内容容器区域(容器从窗口水平原点开始, 纵向位于容器件的原点; 底色透明, 内容自绘)
	// 说明: 容器同时提供内容控件的裁剪与偏移 ⇒ 控件库按容器本地坐标绘制, 有效坐标里的父链累加只在此处
	//       被容器消费一次(见 Controls::DrawCtrl 的坐标换算)
	pRenderApi->OpenPanel(pCntr->label, orgPanel, pCntr->size, 0x00000000);
	// 内容容器的内容原点: 进入容器后立即取一次, 供复合控件矩形兜底换算
	orgContent = pRenderApi->GetContentOrigin();
	{
		// 遍历并渲染每个内容控件(结构件已由循环起点 GetShellNum 排除);
		// 列表(LIST_*)由控件绘制实现按自身缓冲绘制, 其意图交控件自身事件处理器消费
		for (size_t i = layout.GetShellNum(); i < num; i++)
		{
			// 视图叠加件插入点: 在索引 i 的控件绘制之前给叠加件一次绘制机会(见 FuncPtr.h 的 pfnOverlayBefore)
			// 说明: 本调用在该控件的 ID 压栈/提交之前、同一内容容器内发生 ⇒ 叠加件位于其"下层"绘制位置
			//       (应用层选中态表达件靠此露出外圈); 叠加件不是控件链成员 ⇒ 后续点击判定不涉及它
			if (pfnOverlay != NULL)
			{
				pfnOverlay(layout, i);
			}
			ctrl = layout.GetCtrlInfo(i);
			if (ctrl)
			{
				// 压入 ID
				pRenderApi->PushId(ctrl->id);
				// 本控件绘制前的末项表 ID(供"是否自行提交项"判定)
				CONST uint64_t qwItemIdBefore = pRenderApi->TakeItemId();
				pCtrlApi->DrawCtrl(ctrl);
				// 逐控件点击判定(仅预览布局启用: 记录器非空时; 矩形按内容容器原点换算)
				DispatchCtrlClick(ctrl, i, qwItemIdBefore, orgContent, orgPanel, pfnClick);
				// 弹出 ID
				pRenderApi->PopId();
			}
		}
	}
	pRenderApi->ClosePanel();	// 结束内容容器
	pRenderApi->CloseWnd();		// 结束窗口
	bRet = TRUE;
RENDER_END:
	return bRet;
}

// ================================
// 命名空间: LayoutRender(与 LayoutRender.h 的声明对应)
// ================================
namespace LayoutRender
{

// 函数: SetRenderApi
// 描述: 注入渲染能力表(幂等; 传 NULL 清除注入)
VOID SetRenderApi(CONST RenderApi* pApi)
{
	pRenderApi = pApi;
}

// 函数: GetRenderApi
// 描述: 取当前注入的渲染能力表
CONST RenderApi* GetRenderApi()
{
	return pRenderApi;
}

// 函数: SetCtrlApi
// 描述: 注入控件绘制接口表(幂等; 传 NULL 清除注入)
VOID SetCtrlApi(CONST CtrlApi* pApi)
{
	pCtrlApi = pApi;
}

// 函数: GetCtrlApi
// 描述: 取当前注入的控件绘制接口表
CONST CtrlApi* GetCtrlApi()
{
	return pCtrlApi;
}

// 函数: RenderBase
// 描述: 基础渲染: 渲染占一个标准窗口的基座布局
BOOL RenderBase(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, BOOL bNoInputs)
{
	// 窗口级无输入: 只在"提示展示期间"由宿主传 TRUE; 未开启时状态位与既有实现完全相同
	CONST DWORD dwStates = RENDER_WND_BASE | ((bNoInputs != FALSE) ? RENDER_WND_NOINPUT : 0);
	return RenderLayoutCore(layout, refsize, pProj, dwStates, NULL, NULL);
}

// 函数: RenderPreviewBase
// 描述: 基础渲染之上为内容控件逐枚做点击判定, 并可插入视图叠加件
BOOL RenderPreviewBase(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj, pfnCtrlEvent pfnClick,
	pfnOverlayBefore pfnOverlay)
{
	return RenderLayoutCore(layout, refsize, pProj, RENDER_WND_BASE, pfnClick, pfnOverlay);
}

// 函数: RenderFlex
// 描述: 渲染无独立窗口句柄的内嵌(flex)布局(沿用当前窗口)
BOOL RenderFlex(Layout& layout, CONST AutoSize& refsize, CONST ProjCR* pProj)
{
	// 变量声明
	BOOL bRet = FALSE;
	size_t num = 0;
	CtrlInfo* ctrl = NULL;
	CtrlInfo* pCntr = NULL;		// 内容容器件(内容控件的坐标原点与裁剪区)
	vec2 orgPanel(0.0f, 0.0f);	// 内容容器原点(窗口本地坐标; 内容控件按它把有效坐标换算为容器本地坐标)
	CHAR szWndName[128]{};		// 唯一化后的窗口名(label + 布局对象地址)
	// 特殊情况: 渲染能力或控件绘制接口未注入 ⇒ 静默失败(渲染前置条件归宿主)
	if (pRenderApi == NULL || pCtrlApi == NULL)
	{
		goto RENDER_END;
	}
	if (layout.IsInvalid())
	{
		goto RENDER_END;
	}
	// 初始化
	num = layout.GetTotalNum();
	pCntr = layout.GetLayoutBase(CTRL_TYPE_CNTR);
	if (pCntr == NULL)
	{
		goto RENDER_END;
	}
	orgPanel = vec2(0.0f, pCntr->position.y);
	// 控件库上下文: 内嵌窗口无独立句柄、也不参与窗口位置记录 ⇒ 位置记录槽传空
	// (标题栏等窗口级控件据此知道"本窗口没有可移动的窗口几何", 不建立拖动); 内容容器原点照常注入
	pCtrlApi->UpdateContext(refsize, pProj, NULL, orgPanel);
	// 开内嵌窗口(位置/尺寸 = 该布局窗体区的声明像素; 窗口名唯一化见说明③)
	ctrl = layout.GetLayoutBase(CTRL_TYPE_WND);
	if (ctrl == NULL || ctrl->label == NULL)
	{
		goto RENDER_END;
	}
	snprintf(szWndName, sizeof(szWndName), "%s##FLEX%p", ctrl->label, (void*)&layout);
	pRenderApi->OpenWnd(szWndName, ctrl->position, ctrl->size, RENDER_WND_BASE);
	// 控件 ID 空间隔离(同名控件(如 ##BTN_OK)在不同内嵌窗口内不冲突)
	pRenderApi->PushPtrId((CONST VOID*)&layout);
	// 标题: 结构件标题经控件绘制接口表分派(与 RenderBase 同法; 关闭钮恒绘制,
	//       关闭/拖动是否生效由该控件绑定的事件处理器决定)
	{
		ctrl = layout.GetLayoutBase(CTRL_TYPE_TITLE);
		if (ctrl)
		{
			pCtrlApi->DrawCtrl(ctrl);
		}
	}
	// 内容容器区域(容器从窗口水平原点开始, 纵向位于容器件的原点; 底色透明, 内容自绘)
	pRenderApi->OpenPanel(pCntr->label, orgPanel, pCntr->size, 0x00000000);
	{
		// 遍历并渲染每个内容控件(与 RenderBase 同法; 结构件已由循环起点 GetShellNum 排除)
		for (size_t i = layout.GetShellNum(); i < num; i++)
		{
			ctrl = layout.GetCtrlInfo(i);
			if (ctrl)
			{
				// 压入 ID
				pRenderApi->PushId(ctrl->id);
				pCtrlApi->DrawCtrl(ctrl);
				// 弹出 ID
				pRenderApi->PopId();
			}
		}
	}
	pRenderApi->ClosePanel();	// 结束内容容器
	pRenderApi->PopId();			// 弹出内嵌窗口的 ID 命名空间
	pRenderApi->CloseWnd();		// 结束内嵌窗口
	// 显式置顶: 内嵌窗口必须绘制在其宿主(基座)之上
	// 说明: UI 库的窗口绘制顺序 = 内部窗口表顺序(末尾在最上层), 该顺序会随焦点/激活调整 ⇒
	//       内嵌窗口与宿主在几何上完全重叠(同为宿主客户区坐标), 仅靠创建顺序不足以保证层级
	//       ⇒ 每帧把本内嵌窗口前置一次(多枚成员即按渲染顺序自下而上叠放)。
	pRenderApi->BringWndToFront();
	bRet = TRUE;
RENDER_END:
	return bRet;
}

// 函数: IsIMEDisabled
// 描述: 检查当前是否需要禁用 IME 输入法(本帧存在编辑中的输入控件即禁用)
BOOL IsIMEDisabled()
{
	// 控件绘制接口未注入 ⇒ 无从判断编辑态 ⇒ 按"不禁用"静默处理(与渲染入口同口径)
	if (pCtrlApi == NULL)
	{
		return FALSE;
	}
	return pCtrlApi->HasActiveInput();
}

}	// namespace LayoutRender
