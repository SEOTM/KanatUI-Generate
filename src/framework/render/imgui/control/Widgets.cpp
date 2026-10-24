// ================================================
// 库名: Widgets
// 描述: 控件库的部件层实现(可复用绘制件 + 部件上下文与内部辅助)
// ================================================

// ================================
// 区域: 编译常量定义区
// ================================
// 说明: 本文件用 ImVec2 运算符重载 ⇒ 宏须在 imgui.h 之前定义(与 Controls.cpp 同法)。
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>					// DWORD / BOOL / INT32 等基础类型(winnt.h)
#include "framework/render/imgui/control/Widgets.h"
#include "framework/render/imgui/control/Controls.h"	// ListRowDropOper(行件返回约定的换算真源; 公开接口)
#include "framework/base/CtrlBuffer.h"			// 行件数据块(CtrlInputActionData / CtrlComboData 及其读取辅助)
#include "framework/base/TextWidth.h"		// CalcTextWidth(部件内按钮宽度推算)
#include "imgui.h"
#include "imgui_internal.h"
#include <stdio.h>						// sprintf_s(时码格式化)
#include <string.h>						// strlen(只读文本的容量口径)

// ================================
// 区域: 部件上下文与内部辅助(声明)
// ================================
namespace Controls
{
	namespace Widgets
	{

		// ================================
		// 区域: 上下文变量定义区(即时模式状态; 由 UpdateContext 每帧写入)
		// ================================
		static auto sizeinfo = AutoSize::SizeInfoFLOAT{};	// 当前帧参考尺寸的像素值(部件绘制按需取用)
		static FLOAT fRounding = 0.0f;			// 当前帧圆角半径(像素; 取值 = 设计内边距)
		static CONST ProjCR* pcrHander = NULL;		// 当前帧持有的投影配色(宿主持有; NULL = 未注入)
		static DWORD dwDisableIME = 0;				// 非 IME 输入需求计数(>0 = 本帧应禁用 IME; 由控件层查询)

		// ================================
		// 区域: 绝对档位常量(拖动辅助取色)
		// 说明: 列表拖动辅助(把手 / 插入线)取固定档位色, 与控件层次无关 —— 部件层不按控件层次取色;
		//       常量值与 Controls.cpp 的同名常量一致, 两处须同步(分段拆件后的部件层副本)。
		// ================================
		constexpr INT32 PROJ_L_BORDER_HOVER = 11;	// 边框悬停 / 拖拽把手常态
		constexpr INT32 PROJ_L_BORDER_ACTIVE = 13;	// 边框激活 / 强调(把手悬停 / 插入线)

		// ================================
		// 区域: 部件几何常量(单手柄滑块; 观感口径见 Widgets.md 的 SingleSlider 节)
		// 说明: 以控件声明高 size.y 为基准; 命中区不随这两个比例收缩(仍覆盖整个控件矩形)。
		// ================================
		constexpr FLOAT SLIDER_GRIP_RATIO = 0.28f;		// 手柄半径 = 该比例 × 控件高
		constexpr FLOAT SLIDER_TRACK_RATIO = 0.30f;		// 轨道半厚 = 该比例 × 手柄半径

		// 函数: ProjColor
		// 描述: 列表拖动辅助取色的唯一入口(投影未注入时返回 0)
		static inline DWORD ProjColor(INT32 nLayer, ProjCR::CR_SLOT eSlot = ProjCR::PROJ_SLOT_SURFACE)
		{
			// 防御钳制: 入参应已是"层号域内"的值, 但绘图路径不容越界读轴 ⇒ 此处再夹一次
			INT32 nLayerClamped = (nLayer < PROJ_LAYER_MIN) ? PROJ_LAYER_MIN :
				((nLayer > PROJ_LAYER_MAX) ? PROJ_LAYER_MAX : nLayer);
			return (pcrHander != NULL) ? pcrHander->Sample((DWORD)nLayerClamped, eSlot) : 0;
		}

		// 函数: UpdateContext
		// 描述: 更新部件层上下文(每帧一次; 复位编辑中计数, 并算当前帧圆角)
		BOOL UpdateContext(CONST AutoSize& refsize, CONST ProjCR* pProj)
		{
			dwDisableIME = 0;
			sizeinfo = refsize.GetSizeInfo<FLOAT>();
			fRounding = sizeinfo.padding;
			pcrHander = pProj;
			return TRUE;
		}

		// 函数: HasActiveInput
		// 描述: 查询本帧是否存在编辑中的输入项(计数非 0 即 TRUE)
		BOOL HasActiveInput()
		{
			return (dwDisableIME != 0) ? TRUE : FALSE;
		}

		// ================================
		// 区域: 内部辅助函数区(声明; 语义见 Widgets.md 的概述)
		// 说明: 部件层自持独立副本(与 Controls.cpp 的同名辅助互不相干); 函数体见文件末尾的同名定义区。
		// ================================
		// 函数: toImvec2
		// 描述: 内置 vec2 到 ImGui 类型的转换模板(safe = false 走 reinterpret_cast)
		// 模板定义随本区给出(模板须在使用点前可见)
		template<bool safe = false>
		auto toImvec2(const vec2& vec)
		{
			if constexpr (safe)
			{
				return ImVec2(vec.x, vec.y);
			}
			else
			{
				return reinterpret_cast<const ImVec2&>(vec);
			}
		}
		static VOID DrawRectFace(ImDrawList* dl, ImVec2 o, ImVec2 size, DWORD crBase, DWORD crBorder);	// 矩形外观绘制(底/描边色由调用方给出)
		// 下拉列表项表(定义见文件末尾辅助区)
		// 说明: Combo 与 ComboAction 两件共用 ⇒ 展开列表的行高 / 行距 / 文字对齐只有这一处定义
		static BOOL ComboItems(LPCSTR* lpItems, INT nCount, INT& nIndex);	// 提交项表; 返回 TRUE = 选中索引已改变
		// 列表行拖拽辅助(定义见文件末尾辅助区)
		constexpr LPCSTR KANATUI_GENERATE_LIST_DRAG_TYPE = "KANATUI_GENERATE_LIST_ROW";	// 拖拽 payload 类型串(≤32 字符, 不以 '_' 开头)
		// 结构: ListRowDragPayload
		struct ListRowDragPayload
		{
			DWORD dwRow;		// 源行号(发起拖拽的行)
		};
		static FLOAT ListRowDragHandle(int id, vec2 pos, vec2 size, DWORD dwRow);		// 行首拖拽把手(拖拽源)
		static int64_t ListRowDropTarget(int id, vec2 pos, vec2 size, DWORD dwRow);		// 整行放置目标(返回重排结论; 语义见 Controls::ListRowDropOper)
		static VOID ListCollectRowOper(int64_t qwRow, DWORD dwRow, int64_t& qwOper, uint64_t& qwParam);	// 行件返回约定 -> oper + param

	}	// namespace Widgets
}	// namespace Controls

// ================================
// 区域: 部件实现区(命名空间 Controls::Widgets; 声明见 Widgets.h)
// ================================
namespace Controls
{
	namespace Widgets
	{

		// ================================
		// 区域: 基础部件实现(Widgets::Base)
		// ================================
		namespace Base
		{
			// 函数: ExitSign
			// 描述: 关闭按钮部件: 两段交叉线符号 + 悬停底(标题栏用)
			BOOL ExitSign(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR label, FLOAT fPadding, FLOAT thickness)
			{
				if (way.eDesc != PALETTE)
				{
					return FALSE;
				}
				CONST Palette& palette = way.palette;
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				// 符号大小 = 短边让出内边距(内缩量 = 4 x fPadding); 符号居中 = (按钮尺寸 - 符号) / 2
				FLOAT fSize = (size.x < size.y ? size.x : size.y) - fPadding * 4;	// 符号大小
				ImVec2 sign = { pos.x + CenterOffset(size.x, fSize), pos.y + CenterOffset(size.y, fSize) };
				ImVec2 end = { sign.x + fSize, sign.y + fSize };
				ImVec2 btn_end = { pos.x + size.x, pos.y + size.y };
				ImGui::PushID(id);
				ImGui::SetCursorScreenPos(pos);
				ImGui::InvisibleButton(label ? label : "##EXITBUTTON", size);
				BOOL IsHovered = ImGui::IsItemHovered();
				BOOL IsClicked = ImGui::IsItemClicked();
				ImU32 crFore = StateColorU32(palette.crForeground, WIDGET_STATE_BASE);		// 符号色 = 底色面前景色
				if (IsHovered)
				{
					draw_list->AddRectFilled(pos, btn_end, StateColorU32(palette.crBackground, WIDGET_STATE_HOVERED), 0);	// 悬停底 = 底色面悬停色
				}
				draw_list->AddLine(ImVec2(sign.x, sign.y), ImVec2(end.x, end.y), crFore, thickness);
				draw_list->AddLine(ImVec2(sign.x, end.y), ImVec2(end.x, sign.y), crFore, thickness);
				ImGui::PopID();
				return IsClicked;
			}
			// 函数: InputText
			// 描述: 帧类单行文本件: 底与边框经 ImGui 原生帧绘制(内边距按声明高对齐), 本件不再自绘外框
			int64_t InputText(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CHAR* lpText, size_t nTextCap,
				LPCSTR lpHint)
			{
				int64_t qwOper = 0;							// 部件 → 调用方 返回约定(0 = 无操作)
				uint16_t wOwn = 0;							// bit[15:0]: 专属段码(互斥; 0 = 无)
				uint16_t wCommon = OPER_COMMON_NONE;		// bit[31:16]: 公共位图(0 = 无)
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				// 终止绘制
				if (lpText == NULL || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				// 项模式: 容量有效 = 可写(编辑); 容量无效 = 只读(按 C 串长度提交, 不会写回)
				const BOOL bEditable = (nTextCap > 0) ? TRUE : FALSE;
				const size_t nCap = bEditable ? nTextCap : (strlen(lpText) + 1);
				ImGui::PushID(id);
				ImGuiStyle& style = ImGui::GetStyle();
				// 垂直内边距: 帧高 = 设计字号 + 2 x 内边距 ⇒ 取 CenterOffset(声明高, 设计字号) 使帧高恰为 size.y
				// (文字随原生帧的内边距垂直居中; 水平内边距仍取样式当前值, 项自声明矩形左上起排)
				FLOAT padT = CenterOffset(size.y, sizeinfo.fontSize);
				if (padT < 1.0f)
				{
					padT = 1.0f;								// 下限 1px(内边距无法小于 1px)
				}
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, padT));
				// 底色 / 边框 / 文本: 逐控件推入本控件层次的配色方案(底与边框由 ImGui 原生帧绘制)
				ImGui::PushStyleColor(ImGuiCol_FrameBg, StateColorU32(palette.crBackground, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, StateColorU32(palette.crBackground, WIDGET_STATE_HOVERED));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, StateColorU32(palette.crBackground, WIDGET_STATE_ACTIVE));
				ImGui::PushStyleColor(ImGuiCol_Border, StateColorU32(palette.crBorder, WIDGET_STATE_BASE));
				// 文本前景 = 配色方案的前景槽
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(pos);
				ImGui::SetNextItemWidth(size.x);
				// 提示文本(占位符): 仅编辑模式下交给 ImGui(只读占位不显示提示, lpHint 为 NULL 时亦不显示)
				LPCSTR lpHintShown = (bEditable && lpHint != NULL) ? lpHint : "";
				ImGui::InputTextWithHint("##FRAMEDITEM_TEXT", lpHintShown, lpText, nCap,
					bEditable ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_ReadOnly);
				if (ImGui::IsItemActive())
				{
					dwDisableIME++;							// 编辑中: 本帧禁用 IME
				}
				if (ImGui::IsItemActivated())
				{
					wCommon |= OPER_COMMON_ACTIVE;			// 激活 ⇒ 公共位图
				}
				// 只读项不可能"编辑结束" ⇒ 提交码只在编辑模式置位
				if (bEditable && ImGui::IsItemDeactivatedAfterEdit())
				{
					wOwn = OPER_EDIT_SUBMIT;
				}
				ImGui::PopStyleColor(5);
				ImGui::PopStyleVar();
				ImGui::PopID();
				// 组装返回约定(位表见 Widgets.md 的 InputText 节)
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: InputTextMulti
			// 描述: 帧类多行文本件: 底与边框经 ImGui 原生帧绘制(项尺寸 = 声明矩形), 本件不再自绘外框
			int64_t InputTextMulti(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CHAR* lpText, size_t nTextCap,
				LPCSTR lpHint)
			{
				UNREFERENCED_PARAMETER(lpHint);				// ImGui 的多行项没有 WithHint 变体 ⇒ 占位提示通道待后续(见文档登记)
				int64_t qwOper = 0;							// 部件 → 调用方 返回约定(0 = 无操作)
				uint16_t wOwn = 0;							// bit[15:0]: 专属段码(互斥; 0 = 无)
				uint16_t wCommon = OPER_COMMON_NONE;		// bit[31:16]: 公共位图(0 = 无)
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				// 终止绘制
				if (lpText == NULL || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				// 项模式: 容量有效 = 可写(编辑); 容量无效 = 只读(按 C 串长度提交, 不会写回)
				const BOOL bEditable = (nTextCap > 0) ? TRUE : FALSE;
				const size_t nCap = bEditable ? nTextCap : (strlen(lpText) + 1);
				ImGui::PushID(id);
				// 底色 / 边框 / 文本: 逐控件推入本控件层次的配色方案(底与边框由 ImGui 原生帧绘制)
				ImGui::PushStyleColor(ImGuiCol_FrameBg, StateColorU32(palette.crBackground, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, StateColorU32(palette.crBackground, WIDGET_STATE_HOVERED));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, StateColorU32(palette.crBackground, WIDGET_STATE_ACTIVE));
				ImGui::PushStyleColor(ImGuiCol_Border, StateColorU32(palette.crBorder, WIDGET_STATE_BASE));
				// 文本前景 = 配色方案的前景槽(与单行件同口径)
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				// 多行项自身带文本内距与滚动 ⇒ 项尺寸 = 声明矩形(不做单行那样的垂直居中)
				ImGui::SetCursorPos(pos);
				ImGui::InputTextMultiline("##FRAMEDITEM_TEXT_MULTI", lpText, nCap, size,
					bEditable ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_ReadOnly);
				if (ImGui::IsItemActive())
				{
					dwDisableIME++;							// 编辑中: 本帧禁用 IME
				}
				if (ImGui::IsItemActivated())
				{
					wCommon |= OPER_COMMON_ACTIVE;			// 激活 ⇒ 公共位图
				}
				// 多行项内回车 = 换行(不提交) ⇒ 提交语义与单行件一致(失焦且有改动)
				if (bEditable && ImGui::IsItemDeactivatedAfterEdit())
				{
					wOwn = OPER_EDIT_SUBMIT;
				}
				ImGui::PopStyleColor(5);
				ImGui::PopID();
				// 组装返回约定(位表见 Widgets.md 的 InputText 节)
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: InputScalar
			// 描述: 帧类数值件: 底与边框经 ImGui 原生帧绘制(内边距按声明高对齐), 本件不再自绘外框
			int64_t InputScalar(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, VOID* pData, ImGuiDataType eType,
				LPCSTR lpFormat, LPCSTR lpHint)
			{
				UNREFERENCED_PARAMETER(lpHint);				// ImGui 的 InputScalar 无提示变体 ⇒ 本形参暂不消费
				int64_t qwOper = 0;
				uint16_t wOwn = 0;
				uint16_t wCommon = OPER_COMMON_NONE;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				// 终止绘制
				if (pData == NULL || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				ImGui::PushID(id);
				ImGuiStyle& style = ImGui::GetStyle();
				// 垂直内边距: 帧高 = 设计字号 + 2 x 内边距 ⇒ 取 CenterOffset(声明高, 设计字号) 使帧高恰为 size.y
				// (文字随原生帧的内边距垂直居中; 水平内边距仍取样式当前值, 项自声明矩形左上起排)
				FLOAT padT = CenterOffset(size.y, sizeinfo.fontSize);
				if (padT < 1.0f)
				{
					padT = 1.0f;								// 下限 1px(内边距无法小于 1px)
				}
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, padT));
				// 底色 / 边框 / 文本: 逐控件推入本控件层次的配色方案(底与边框由 ImGui 原生帧绘制)
				ImGui::PushStyleColor(ImGuiCol_FrameBg, StateColorU32(palette.crBackground, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, StateColorU32(palette.crBackground, WIDGET_STATE_HOVERED));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, StateColorU32(palette.crBackground, WIDGET_STATE_ACTIVE));
				ImGui::PushStyleColor(ImGuiCol_Border, StateColorU32(palette.crBorder, WIDGET_STATE_BASE));
				// 文本前景 = 配色方案的前景槽
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(pos);
				ImGui::SetNextItemWidth(size.x);
				const BOOL bChanged = ImGui::InputScalar("##FRAMEDITEM_SCALAR", eType, pData,
					NULL, NULL, lpFormat, ImGuiInputTextFlags_CharsDecimal);
				if (ImGui::IsItemActive())
				{
					dwDisableIME++;							// 编辑中: 本帧禁用 IME
				}
				if (ImGui::IsItemActivated())
				{
					wCommon |= OPER_COMMON_ACTIVE;
				}
				if (bChanged)
				{
					wOwn = OPER_EDIT_SUBMIT;				// 值变化(失焦/回车/箭头)
				}
				ImGui::PopStyleColor(5);
				ImGui::PopStyleVar();
				ImGui::PopID();
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: Combo
			// 描述: 帧类下拉件: 原生帧底 + 通道补画 1px 边框, 展开列表项表与 ComboAction 同源
			int64_t Combo(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR* lpItems, INT nCount,
				INT& nIndex)
			{
				int64_t qwOper = 0;
				uint16_t wOwn = 0;
				uint16_t wCommon = OPER_COMMON_NONE;
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				// 终止绘制(无选项 ⇒ 本件不产出像素; 与改前口径一致)
				if (lpItems == NULL || nCount <= 0 || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				// 选中越界: 按 0 夹取
				if (nIndex < 0 || nIndex >= nCount)
				{
					nIndex = 0;
				}
				ImGui::PushID(id);
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				ImGuiStyle& style = ImGui::GetStyle();
				// 垂直内边距: 帧高 = 设计字号 + 2 x 内边距 ⇒ 取 CenterOffset(声明高, 设计字号) 使帧高恰为 size.y
				// (文字随原生帧的内边距垂直居中; 水平内边距仍取样式当前值, 项自声明矩形左上起排)
				const FLOAT th = sizeinfo.fontSize;			// 内容高 = 设计字号(不读 ImGui 当前字体行高)
				FLOAT padT = CenterOffset(size.y, th);			// 目标内边距: 帧高 = th + 2 x padT 恰为声明高
				if (padT < 1.0f)
				{
					padT = 1.0f;								// 下限 1px(内边距无法小于 1px)
				}
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, padT));
				// 部件矩形(屏幕坐标; 口径见 InputText 处注释)
				ImGui::SetCursorPos(pos);
				const ImVec2 boot = ImGui::GetCursorScreenPos();
				const ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				const ImVec4 crsClear = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);	// 项自身边框全透明(边框由本部件补画)
				// 通道拆分: 通道 0 = 项(底与文字), 通道 1 = 补画的边框(合并时后入队 ⇒ 位于文字之上)
				drawlist->ChannelsSplit(2);
				drawlist->ChannelsSetCurrent(0);
				// 1. 下拉项: 底由 ImGui 原生帧绘制(三态色按本控件层次推入); 展开列表的行观感见 ComboItems
				ImGui::PushStyleColor(ImGuiCol_FrameBg, StateColorU32(palette.crBackground, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, StateColorU32(palette.crBackground, WIDGET_STATE_HOVERED));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, StateColorU32(palette.crBackground, WIDGET_STATE_ACTIVE));
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				// 文本前景 = 配色方案的前景槽(弹出列表同受本作用域约束)
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetNextItemWidth(size.x);
				// 展开列表与 ComboAction 同源(行高 = 声明控件高; 行距 0; 文字行内垂直居中)
				BOOL bChanged = FALSE;
				const char* preview = lpItems[nIndex];
				if (ImGui::BeginCombo("##FRAMEDITEM_COMBO", preview))
				{
					bChanged = ComboItems(lpItems, nCount, nIndex);
					ImGui::EndCombo();
				}
				WidgetState state = WIDGET_STATE_BASE;
				state |= ImGui::IsItemHovered() ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE;
				state |= ImGui::IsItemActive() ? WIDGET_STATE_ACTIVE : WIDGET_STATE_BASE;
				state |= ItemDisabled() ? WIDGET_STATE_DISABLE : WIDGET_STATE_BASE;
				if (ImGui::IsItemActivated())
				{
					wCommon |= OPER_COMMON_ACTIVE;
				}
				if (bChanged)
				{
					wOwn = OPER_EDIT_CHANGED;				// 值/选项已改变
				}
				ImGui::PopStyleColor(5);
				ImGui::PopStyleVar();
				// 2. 边框: 按 ComboAction 口径补画 1px(色随项状态; 不再依赖 FrameBorderSize)
				drawlist->ChannelsSetCurrent(1);
				drawlist->AddRect(boot, end, StateColorU32(palette.crBorder, state),
					fRounding, ImDrawFlags_None, 1.0f);
				drawlist->ChannelsMerge();
				ImGui::PopID();
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: TimeSpan
			// 描述: 时段显示件: 自绘圆角底 + 时码文本(无交互项, 恒返回 0)
			int64_t TimeSpan(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t nStartMs, int64_t nEndMs,
				int64_t nLimitMs)
			{
				const int64_t qwOper = 0;					// 本件无交互结论 ⇒ 恒 0
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				// 值域: 负值归零; 上限有效(nLimitMs > 0)时饱和到上限
				if (nStartMs < 0)
				{
					nStartMs = 0;
				}
				if (nEndMs < 0)
				{
					nEndMs = 0;
				}
				if (nLimitMs > 0)
				{
					if (nStartMs > nLimitMs)
					{
						nStartMs = nLimitMs;
					}
					if (nEndMs > nLimitMs)
					{
						nEndMs = nLimitMs;
					}
				}
				// 时码文本: 拆分口径同参考实现(毫秒 → 时 100 / 分 60 / 秒 60 / 毫秒 1000)
				CHAR szSpan[64] = {};
				sprintf_s(szSpan, sizeof(szSpan), "%02d:%02d:%02d.%03d - %02d:%02d:%02d.%03d",
					(int)(nStartMs / 3600000 % 100), (int)(nStartMs / 60000 % 60),
					(int)(nStartMs / 1000 % 60), (int)(nStartMs % 1000),
					(int)(nEndMs / 3600000 % 100), (int)(nEndMs / 60000 % 60),
					(int)(nEndMs / 1000 % 60), (int)(nEndMs % 1000));
				ImGui::PushID(id);
				ImGui::SetCursorPos(pos);
				const ImVec2 boot = ImGui::GetCursorScreenPos();
				const ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				// 占位项: 本件不产出可交互项, 但不提交任何项会触发"SetCursorPos 扩展窗口边界"告警 ⇒ 占住矩形
				ImGui::Dummy(size);
				// 自绘圆角底(按声明高 size.y) + 文本(纵向居中; 横向留一枚内距)
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				ImGuiStyle& style = ImGui::GetStyle();
				drawlist->AddRectFilled(boot, end, StateColorU32(palette.crBackground, WIDGET_STATE_BASE), fRounding);
				// 文本纵向居中 = CenterOffset(声明高, 设计字号)(下限 0 ⇒ 不会上越界, 无需再夹取)
				FLOAT fTextY = boot.y + CenterOffset(size.y, sizeinfo.fontSize);
				drawlist->AddText(ImVec2(boot.x + style.FramePadding.x, fTextY),
					StateColorU32(palette.crForeground, WIDGET_STATE_BASE), szSpan);	// 文本 = 本层对比槽(随控件层次)
				ImGui::PopID();
				return qwOper;
			}
			// 函数: InputAction
			// 描述: 两段件: 左段单行输入 + 右段按钮(两段底与边框在项提交后补画)
			int64_t InputAction(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, FLOAT fBtnW, CHAR* lpText, size_t nTextCap, LPCSTR lpHint, LPCSTR lpBtn)
			{
				int64_t qwOper = 0;							// 部件 → 调用方 返回约定(0 = 无操作)
				uint16_t wOwn = 0;							// bit[15:0]: 专属段码(互斥; 0 = 无)
				uint16_t wCommon = OPER_COMMON_NONE;		// bit[31:16]: 公共位图(0 = 无)
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				// 终止绘制
				if (lpText == NULL || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				// 输入项模式: 容量有效 = 可写(编辑); 容量无效 = 只读(按 C 串长度提交, 不会写回)
				const BOOL bEditable = (nTextCap > 0) ? TRUE : FALSE;
				const size_t nCap = bEditable ? nTextCap : (strlen(lpText) + 1);
				// 分段宽度: 按钮默认宽至少等于控件高(见方); 按钮过宽挤占输入段时退到该下限; 输入段至少 1px
				const FLOAT fBtnMin = size.y;							// 按钮宽下限 = 控件高(见方)
				FLOAT fBtn = (fBtnW > fBtnMin) ? fBtnW : fBtnMin;
				if (fBtn > size.x - sizeinfo.fontSize)
				{
					fBtn = fBtnMin;									// 左段不足文本行高 ⇒ 按钮退到下限(余量全部给输入段)
				}
				if (fBtn > size.x - 1.0f)
				{
					fBtn = size.x - 1.0f;							// 部件过窄时以"输入段至少 1px"优先(不再保证见方)
				}
				if (fBtn < 1.0f)
				{
					fBtn = 1.0f;
				}
				FLOAT fInW = size.x - fBtn;
				if (fInW < 1.0f)
				{
					fInW = 1.0f;
				}
				ImGui::PushID(id);
				// 部件矩形(屏幕坐标): 左段 [boot, mid) / 右段 [mid, end)
				ImDrawList* drawlist = ImGui::GetWindowDrawList();

				ImGuiStyle& style = ImGui::GetStyle();
				ImGui::SetCursorPos(pos);
				ImVec2 boot = ImGui::GetCursorScreenPos();
				ImVec2 mid = ImVec2{ boot.x + fInW, boot.y + size.y };
				ImVec2 end = ImVec2{ boot.x + size.x, boot.y + size.y };
				const ImVec4 crsClear = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);	// 项自身全透明(底与边框由本部件补画)
				// 部件状态
				WidgetState state = WIDGET_STATE_BASE;

				// 通道拆分: 通道 1 = 项(文字); 通道 0 = 补画的底与边框(合并时先入队 ⇒ 位于文字之下)
				drawlist->ChannelsSplit(3);
				drawlist->ChannelsSetCurrent(1);

				// 1. 左段: 单行输入(透明底/透明边框; 编辑或只读由缓冲两参数决定)
				ImGui::PushStyleColor(ImGuiCol_FrameBg, crsClear);
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, crsClear);
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, crsClear);
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				// 左段文字前景 = 窗体面层的对比槽(随控件层次)
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				// 垂直居中 = CenterOffset(声明高, 设计字号)(与右段按钮内的文字对齐)
				ImGui::SetCursorPos(ImVec2(pos.x, pos.y + CenterOffset(size.y, sizeinfo.fontSize) - style.FramePadding.y));
				ImGui::SetNextItemWidth(fInW);
				// 提示文本(占位符): 仅编辑模式下交给 ImGui(只读占位不显示提示, lpHint 为 NULL 时亦不显示)
				LPCSTR lpHintShown = (bEditable && lpHint != NULL) ? lpHint : "";
				ImGui::InputTextWithHint("##INPUTACTION_TEXT", lpHintShown, lpText, nCap, bEditable ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_ReadOnly);
				state |= ImGui::IsItemHovered() ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE;
				state |= ImGui::IsItemActive() ? WIDGET_STATE_ACTIVE : WIDGET_STATE_BASE;
				state |= ItemDisabled() ? WIDGET_STATE_DISABLE : WIDGET_STATE_BASE;
				if (ImGui::IsItemActivated())
				{
					// 激活 ⇒ 公共位图(不再用专属 *_ACTIVATED; 列表行的"点选"意图来源; 只读行同样可被点击)
					wCommon |= OPER_COMMON_ACTIVE;
				}
				// 只读模式不可能"编辑结束" ⇒ 输入段提交码只在编辑模式置位
				if (bEditable && ImGui::IsItemDeactivatedAfterEdit())
				{
					wOwn = OPER_CUSTOM_IA_INPUT_SUBMIT;
				}
				ImGui::PopStyleColor(5);

				// 2. 绘制按钮
				ImGui::PushStyleColor(ImGuiCol_Button, palette.crSurface.crBase);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.crSurface.crHovered);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.crSurface.crActive);
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				// 按钮文字前景 = 按钮面层的对比槽(随控件层次)
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crSurfaceForeground, WIDGET_STATE_BASE));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::SetCursorPos(ImVec2(pos.x + size.x - fBtn, pos.y));
				const BOOL bClicked = ImGui::Button(lpBtn ? lpBtn : "##INPUTACTION_BTN", ImVec2(fBtn, size.y));
				ImGui::PopStyleVar();
				ImGui::PopStyleColor(5);
				if (bClicked)
				{
					wOwn = OPER_CUSTOM_IA_BTN_CLICKED;
				}

				// 3. 绘制底色
				drawlist->ChannelsSetCurrent(0);
				const ImU32 crFaceBg = StateColorU32(palette.crBackground, state);
				drawlist->AddRectFilled(boot, end, crFaceBg, fRounding);

				// 绘制边框
				drawlist->ChannelsSetCurrent(2);
				const ImU32 crBorder = StateColorU32(palette.crBorder, state);
				const ImU32 crBorderInner = StateColorU32(palette.crBorderInner, state);
				drawlist->AddRect(boot, end, crBorder, fRounding, ImDrawFlags_None, 1.0f);
				drawlist->AddRect(ImVec2{ mid.x,boot.y }, end, crBorderInner, fRounding, ImDrawFlags_None, 1.0f);

				// 合并通道
				drawlist->ChannelsMerge();

				ImGui::PopID();
				// 组装返回约定(位表见 Widgets.md 的 InputText 节)
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: ComboAction
			// 描述: 两段件: 左段下拉框 + 右段按钮(补画口径与 InputAction 一致)
			int64_t ComboAction(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, FLOAT fBtnW,
				LPCSTR* lpItems, INT nCount, INT& nIndex, LPCSTR lpBtn)
			{
				int64_t qwOper = 0;							// 部件 → 调用方 返回约定(0 = 无操作)
				uint16_t wOwn = 0;							// bit[15:0]: 专属段码(互斥; 0 = 无)
				uint16_t wCommon = OPER_COMMON_NONE;		// bit[31:16]: 公共位图(0 = 无)
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				// 终止绘制
				if (lpItems == NULL || nCount <= 0 || size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				// 选中越界
				if (nIndex < 0 || nIndex >= nCount)
				{
					nIndex = 0;
				}
				// 分段宽度: 按钮默认宽至少等于控件高(见方); 按钮过宽挤占下拉段时退到该下限; 下拉段至少 1px
				const FLOAT fBtnMin = size.y;							// 按钮宽下限 = 控件高(见方)
				FLOAT fBtn = (fBtnW > fBtnMin) ? fBtnW : fBtnMin;
				if (fBtn > size.x - sizeinfo.fontSize)
				{
					fBtn = fBtnMin;									// 下拉段不足文本行高 ⇒ 按钮退到下限(余量全部给下拉段)
				}
				if (fBtn > size.x - 1.0f)
				{
					fBtn = size.x - 1.0f;							// 部件过窄时以"下拉段至少 1px"优先(不再保证见方)
				}
				if (fBtn < 1.0f)
				{
					fBtn = 1.0f;
				}
				FLOAT fComboW = size.x - fBtn - sizeinfo.padding;
				if (fComboW < 1.0f)
				{
					fComboW = 1.0f;
				}
				ImGui::PushID(id);
				// 部件坐标
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				ImGuiStyle& style = ImGui::GetStyle();
				ImGui::SetCursorPos(pos);
				struct
				{
					struct
					{
						ImVec2 boot;
						ImVec2 end;
					}combo, button;
				} position;
				position.combo.boot = ImGui::GetCursorScreenPos();
				position.combo.end = ImVec2{ position.combo.boot.x + fComboW, position.combo.boot.y + size.y };
				position.button.boot = ImVec2{ position.combo.end.x + sizeinfo.padding, position.combo.boot.y };
				position.button.end = ImVec2{ position.button.boot.x + fBtn, position.button.boot.y + size.y };

				const ImVec4 crsClear = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);	// 项自身全透明(底与边框由本部件补画)
				// 部件状态
				struct
				{
					WidgetState combo = WIDGET_STATE_BASE;
					WidgetState button = WIDGET_STATE_BASE;
				} state;

				// 通道拆分
				drawlist->ChannelsSplit(2);
				drawlist->ChannelsSetCurrent(0);

				// 1. 下拉框
				ImGui::PushStyleColor(ImGuiCol_FrameBg, palette.crBackground.crBase);
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, palette.crBackground.crHovered);
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, palette.crBackground.crActive);
				ImGui::PushStyleColor(ImGuiCol_Button, palette.crSurface.crBase);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.crSurface.crHovered);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.crSurface.crActive);
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				// 下拉段文字前景 = 本层对比槽(随控件层次; 弹出列表的选项文本同受本作用域约束)
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				// 垂直居中 = CenterOffset(声明高, 设计字号)(与右段按钮内的文字对齐)
				ImGui::SetCursorPos(ImVec2(pos.x, pos.y + CenterOffset(size.y, sizeinfo.fontSize) - style.FramePadding.y));
				ImGui::SetNextItemWidth(fComboW);
				BOOL bChanged = FALSE;
				{
					const char* preview = lpItems[nIndex];
					if (ImGui::BeginCombo("##COMBOACTION_COMBO", preview))
					{
						bChanged = ComboItems(lpItems, nCount, nIndex);
						ImGui::EndCombo();
					}
				}
				state.combo |= ImGui::IsItemHovered() ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE;
				state.combo |= ImGui::IsItemActive() ? WIDGET_STATE_ACTIVE : WIDGET_STATE_BASE;
				state.combo |= ItemDisabled() ? WIDGET_STATE_DISABLE : WIDGET_STATE_BASE;
				ImGui::PopStyleColor(8);
				if (ImGui::IsItemActivated())
				{
					// 激活 ⇒ 公共位图(不再用专属 *_ACTIVATED; 列表行的"点选"意图来源)
					wCommon |= OPER_COMMON_ACTIVE;
				}
				if (bChanged)
				{
					wOwn = OPER_CUSTOM_CA_COMBO_CHANGED;
				}

				// 2. 右段: 按钮(项自身全透明 —— 面与内框由第 3 步补画; hover/active/禁用只用于选色)
				ImGui::PushStyleColor(ImGuiCol_Button, palette.crSurface.crBase);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.crSurface.crHovered);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.crSurface.crActive);
				ImGui::PushStyleColor(ImGuiCol_Border, crsClear);
				// 按钮文字前景 = 按钮面层的对比槽(随控件层次)
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crSurfaceForeground, WIDGET_STATE_BASE));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::SetCursorPos(ImVec2(pos.x + size.x - fBtn, pos.y));
				const BOOL bClicked = ImGui::Button(lpBtn ? lpBtn : "##COMBOACTION_BTN", ImVec2(fBtn, size.y));
				state.button |= ImGui::IsItemHovered() ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE;
				state.button |= ImGui::IsItemActive() ? WIDGET_STATE_ACTIVE : WIDGET_STATE_BASE;
				state.button |= ItemDisabled() ? WIDGET_STATE_DISABLE : WIDGET_STATE_BASE;
				ImGui::PopStyleVar();
				ImGui::PopStyleColor(5);
				if (bClicked)
				{
					wOwn = OPER_CUSTOM_CA_BTN_CLICKED;
				}
				// 绘制边框
				drawlist->ChannelsSetCurrent(1);
				const ImU32 crCombo = StateColorU32(palette.crBorder, state.combo);
				const ImU32 crButton = StateColorU32(palette.crBorder, state.button);
				drawlist->AddRect(position.combo.boot, position.combo.end, crCombo, fRounding, ImDrawFlags_None, 1.0f);
				drawlist->AddRect(position.button.boot, position.button.end, crButton, fRounding, ImDrawFlags_None, 1.0f);
				drawlist->ChannelsMerge();

				ImGui::PopID();
				// 组装返回约定(位表见 Widgets.md 的 InputText 节)
				qwOper = (int64_t)((uint64_t)wOwn | ((uint64_t)wCommon << 16));
				return qwOper;
			}
			// 函数: SingleSlider
			// 描述: 单手柄滑动条: 命中区为覆盖控件矩形的无绘制项, 轨道与手柄自绘
			int64_t SingleSlider(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t& value, int64_t lMin, int64_t lMax)
			{
				int64_t qwOper = 0;							// 部件 → 调用方 返回约定(0 = 无操作)
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				// 终止绘制(值域非法时本件不产出像素)
				if (size.x <= 0.0f || size.y <= 0.0f || lMax <= lMin)
				{
					return qwOper;
				}
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				ImGuiStyle& style = ImGui::GetStyle();
				UNREFERENCED_PARAMETER(style);
				ImGui::PushID(id);
				// 1. 命中区: 覆盖整个控件矩形的无绘制项; 判定一律经该项(不用全局鼠标态)
				ImGui::SetCursorPos(pos);
				const ImVec2 boot = ImGui::GetCursorScreenPos();
				const ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				ImGui::InvisibleButton("##SLIDER_ITEM", size);
				const BOOL bHovered = ImGui::IsItemHovered();
				const BOOL bActive = ImGui::IsItemActive();
				// 2. 几何: 比例取自常量区 —— 两者都纵向居中
				//    手柄直径 = 0.56 × 控件高 < 控件高 ⇒ 一定落在控件矩形内(不外溢、不压相邻行)
				const FLOAT r = size.y * SLIDER_GRIP_RATIO;			// 手柄半径(0.28 × 控件高 ⇒ 直径 ≈ 30 px @54)
				const FLOAT trackHalf = r * SLIDER_TRACK_RATIO;		// 轨道半厚(0.30 × 手柄半径 ⇒ 厚 ≈ 9 px @54)
				const FLOAT cy = boot.y + CenterOffset(size.y, r * 2.0f) + r;	// 轨道中心线(内容高 = 手柄直径)
				const FLOAT usable = size.x - r * 2.0f;		// 手柄圆心可移动范围
				// 3. 交互: 项被激活期间按鼠标横坐标写回值 ⇒ "按下即跳到该处, 继续按住即拖动"
				if (bActive == TRUE)
				{
					const FLOAT mx = ImGui::GetIO().MousePos.x;
					FLOAT ratio = (usable > 0.0f) ? ((mx - boot.x - r) / usable) : 0.0f;
					if (ratio < 0.0f) { ratio = 0.0f; }
					if (ratio > 1.0f) { ratio = 1.0f; }
					// 四舍五入到值域上的整数刻度(缓冲为 int64 ⇒ 值域与刻度都由调用方给出, 如 0~100 百分比)
					const int64_t nValue = lMin + (int64_t)(ratio * (FLOAT)(lMax - lMin) + 0.5f);
					if (nValue != value)
					{
						value = nValue;
						qwOper = (int64_t)OPER_EDIT_CHANGED;	// 值已改变(专属段码; 由控件层落专属段)
					}
				}
				// 4. 绘制(时间轴语言): 圆角轨道底 → 已选填充段(左端 → 手柄) → 圆形手柄(带描边)
				{
					FLOAT ratio = (FLOAT)(value - lMin) / (FLOAT)(lMax - lMin);
					if (ratio < 0.0f) { ratio = 0.0f; }
					if (ratio > 1.0f) { ratio = 1.0f; }
					const FLOAT px = boot.x + r + usable * ratio;	// 手柄圆心横坐标
					const ImU32 crTrack = StateColorU32(palette.crBackground, WIDGET_STATE_BASE);
					const ImU32 crFill = StateColorU32(palette.crSurfaceForeground, WIDGET_STATE_BASE);
					const ImU32 crGrip = StateColorU32(palette.crBorder,
						(bHovered == TRUE || bActive == TRUE) ? WIDGET_STATE_HOVERED : WIDGET_STATE_BASE);
					const ImU32 crGripEdge = StateColorU32(palette.crBorder, WIDGET_STATE_BASE);
					drawlist->AddRectFilled(ImVec2(boot.x, cy - trackHalf), ImVec2(end.x, cy + trackHalf), crTrack, trackHalf);
					drawlist->AddRectFilled(ImVec2(boot.x, cy - trackHalf), ImVec2(px, cy + trackHalf), crFill, trackHalf);
					drawlist->AddCircleFilled(ImVec2(px, cy), r, crGrip);
					drawlist->AddCircle(ImVec2(px, cy), r, crGripEdge, 0, 1.0f);
				}
				ImGui::PopID();
				return qwOper;
			}
			// 函数: ProgressStatic
			// 描述: 进度状态显示件: 自绘圆角底 + 进度文本(恒返回 FALSE)
			BOOL ProgressStatic(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t current, int64_t expect, FLOAT fPadding)
			{
				if (way.eDesc != PALETTE)
				{
					return FALSE;
				}
				CONST Palette& palette = way.palette;
				const FLOAT padding = fPadding;
				// 计算进度
				double prog = expect ? (current * 100.0 / expect) : 0.0;
				ImGui::SetCursorPos(pos);
				ImVec2 cur = ImGui::GetCursorScreenPos();
				ImDrawList* drawlist = ImGui::GetWindowDrawList();
				// 面 = 本控件层次的表面色(原硬编码半透明覆盖 0x2FFFFFFF 已退役); 圆角取本帧圆角
				drawlist->AddRectFilled(cur, cur + size, StateColorU32(palette.crBackground, WIDGET_STATE_BASE), fRounding);
				// 文本纵向居中 = 矩形顶 + CenterOffset(声明高, 设计字号); 横向自矩形左缘内缩一枚内距
				ImGui::SetCursorPos(ImVec2(pos.x + padding, pos.y + CenterOffset(size.y, sizeinfo.fontSize)));
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::Text(u8"进度: %6.2f%%", prog);
				ImGui::PopStyleColor();
				// 自绘结束: 提交区域扩展边界, 避免 ImGui "SetCursorPos() to extend window/parent boundaries" 错误
				ImGui::SetCursorPos(pos);
				ImGui::Dummy(size);
				return FALSE;
			}
			// 函数: InputAction(列表行重载)
			// 描述: 列表行件: 解包行数据后转单件实现, 行首拖拽把手占位后行内容右移
			int64_t InputAction(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem)
			{
				int64_t qwOper = 0;
				if (lpItem == NULL)
				{
					return qwOper;
				}
				CtrlInputActionData* pData = (CtrlInputActionData*)lpItem;
				CHAR* lpText = CtrlInputActionText(pData);			// 输入文本区(data + split)
				CHAR* lpBtnText = CtrlInputActionBtnText(pData);	// 按钮文本区(data + 0)
				if (lpText == NULL)
				{
					return qwOper;								// 行数据未初始化(split 无效): 本行不画
				}
				LPCSTR lpBtn = (lpBtnText != NULL && lpBtnText[0] != '\0') ? lpBtnText : CTRL_DEFAULT_ACTION_BTN;
				// 按钮宽按文案定宽; 输入文本容量 = 行数据内文本区容量
				const FLOAT fBtnW = sizeinfo.fontSize * (CalcTextWidth(lpBtn) + 2) / 2;
				const size_t nTextCap = CTRL_TEXT_BUF;
				// 行首拖拽把手(拖拽源) + 整行放置目标(拖拽重排); 行内容按把手宽度让位
				const FLOAT fHandleW = ListRowDragHandle(id, pos, size, (DWORD)id);
				const vec2 posBody = vec2(pos.x + fHandleW, pos.y);
				const vec2 sizeBody = vec2((size.x > fHandleW) ? (size.x - fHandleW) : 0.0f, size.y);
				// 行内件结论(位表见 Widgets.md 的 InputText 节; 行件只报结论, 段别由调用方按"被携带"决定)
				qwOper = InputAction(id, toImvec2(posBody), toImvec2(sizeBody), way, fBtnW,
					lpText, nTextCap, NULL, lpBtn);
				{
					// 投放交付 = 本行的重排结论(优先于行内结论; 同一帧二者不会同时发生)
					const int64_t qwDrop = ListRowDropTarget(id, pos, size, (DWORD)id);
					if (qwDrop != 0)
					{
						qwOper = qwDrop;
					}
				}
				return qwOper;
			}
			// 函数: InputActionRO(列表行重载, 只读)
			// 描述: 列表行件只读形态: 文本容量按 0 传入, 单件走只读分支
			int64_t InputActionRO(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem)
			{
				int64_t qwOper = 0;
				if (lpItem == NULL)
				{
					return qwOper;
				}
				CtrlInputActionData* pData = (CtrlInputActionData*)lpItem;
				CHAR* lpText = CtrlInputActionText(pData);
				CHAR* lpBtnText = CtrlInputActionBtnText(pData);
				if (lpText == NULL)
				{
					return qwOper;								// 行数据未初始化(split 无效): 本行不画
				}
				LPCSTR lpBtn = (lpBtnText != NULL && lpBtnText[0] != '\0') ? lpBtnText : u8"-";
				const FLOAT fBtnW = sizeinfo.fontSize * (CalcTextWidth(lpBtn) + 2) / 2;
				// 行首拖拽把手(拖拽源) + 整行放置目标(拖拽重排); 行内容按把手宽度让位
				const FLOAT fHandleW = ListRowDragHandle(id, pos, size, (DWORD)id);
				const vec2 posBody = vec2(pos.x + fHandleW, pos.y);
				const vec2 sizeBody = vec2((size.x > fHandleW) ? (size.x - fHandleW) : 0.0f, size.y);
				// 只读: 文本容量按 0 传 ⇒ 单件走只读分支(不可编辑, 仍可被点击激活)
				qwOper = InputAction(id, toImvec2(posBody), toImvec2(sizeBody), way, fBtnW,
					lpText, 0, NULL, lpBtn);
				{
					// 投放交付 = 本行的重排结论(优先于行内结论; 同一帧二者不会同时发生)
					const int64_t qwDrop = ListRowDropTarget(id, pos, size, (DWORD)id);
					if (qwDrop != 0)
					{
						qwOper = qwDrop;
					}
				}
				return qwOper;
			}
			// 函数: ComboAction(列表行重载)
			// 描述: 列表行件: 解包选项表后转单件实现, 触发结论打包返回
			int64_t ComboAction(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem)
			{
				int64_t qwOper = 0;
				if (lpItem == NULL)
				{
					return qwOper;
				}
				CtrlComboData* pData = (CtrlComboData*)lpItem;
				if (pData->items == NULL || pData->count <= 0)
				{
					return qwOper;							// 行数据未初始化(无选项表): 本行不画
				}
				// 按钮宽按文案定宽(文案取自行块, 缺省用控件缺省文案)
				LPCSTR lpRowBtn = (pData->lpBtn != NULL) ? pData->lpBtn : CTRL_DEFAULT_ACTION_BTN;
				const FLOAT fBtnW = sizeinfo.fontSize * (CalcTextWidth(lpRowBtn) + 2) / 2;
				// 行首拖拽把手(拖拽源) + 整行放置目标(拖拽重排); 行内容按把手宽度让位
				const FLOAT fHandleW = ListRowDragHandle(id, pos, size, (DWORD)id);
				const vec2 posBody = vec2(pos.x + fHandleW, pos.y);
				const vec2 sizeBody = vec2((size.x > fHandleW) ? (size.x - fHandleW) : 0.0f, size.y);
				qwOper = ComboAction(id, toImvec2(posBody), toImvec2(sizeBody), way, fBtnW,
					pData->items, pData->count, pData->index, lpRowBtn);
				{
					// 投放交付 = 本行的重排结论(优先于行内结论; 同一帧二者不会同时发生)
					const int64_t qwDrop = ListRowDropTarget(id, pos, size, (DWORD)id);
					if (qwDrop != 0)
					{
						qwOper = qwDrop;
					}
				}
				return qwOper;
			}
			// 函数: Button
			// 描述: 按钮件: 三态面取自配色方案 + 面内文字(自带可交互项)
			int64_t Button(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText)
			{
				int64_t qwOper = 0;							// 部件 → 调用方 返回约定(0 = 无操作)
				uint16_t wCommon = OPER_COMMON_NONE;		// bit[31:16]: 公共位图(0 = 无)
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				ImGui::PushID(id);
				// 面三态与面内文字一律取自配色方案(不再取 ImGui 样式槽; 与改前"控件层直调按钮"像素一致)
				ImGui::PushStyleColor(ImGuiCol_Button, StateColorU32(palette.crSurface, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, StateColorU32(palette.crSurface, WIDGET_STATE_HOVERED));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, StateColorU32(palette.crSurface, WIDGET_STATE_ACTIVE));
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crSurfaceForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(pos);
				if (ImGui::Button(lpText ? lpText : "", size))
				{
					wCommon |= OPER_COMMON_CLICKED;			// 完整单击(ImGui 的按钮判定语义)
				}
				ImGui::PopStyleColor(4);
				ImGui::PopID();
				// 组装返回约定(位表见 Widgets.md 的 InputText 节)
				qwOper = (int64_t)((uint64_t)wCommon << 16);
				return qwOper;
			}
			// 函数: Text
			// 描述: 文本图元件: 部件矩形内垂直居中绘制一行文本, 裁剪到部件矩形
			int64_t Text(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText)
			{
				const int64_t qwOper = 0;					// 本件无交互结论 ⇒ 恒 0
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				// 终止绘制: 矩形非法(裁剪边界不可用) ⇒ 本件不产出像素(与其余部件的终止口径一致)
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				ImGui::PushID(id);
				// 部件矩形(屏幕坐标): 进件先按矩形原点 pos 定位, 再以 GetCursorScreenPos 为基准
				// (ImDrawList 与裁剪矩形都用屏幕绝对坐标, 不得把窗口本地坐标直接当屏幕坐标)
				ImGui::SetCursorPos(pos);
				const ImVec2 boot = ImGui::GetCursorScreenPos();
				const ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				// 纵向对齐: 一律垂直居中 = 矩形顶 + CenterOffset(矩形高, 设计字号)(唯一口径, 与内边距无关)
				CONST FLOAT fTextY = pos.y + CenterOffset(size.y, sizeinfo.fontSize);
				ImGui::SetCursorPos(ImVec2(pos.x, fTextY));
				// 绘制裁剪到部件矩形: 横向不越界(长文本被裁掉, 不外溢到相邻控件);
				// 裁剪只作用于本次绘制, 不改变项提交与光标推进(命中项与占位由控件层负责)
				ImGui::PushClipRect(boot, end, TRUE);
				// 文本色 = 配色方案的前景槽
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::Text("%s", lpText ? lpText : "");
				ImGui::PopStyleColor();
				ImGui::PopClipRect();
				ImGui::PopID();
				return qwOper;
			}
			// 函数: TextMulti
			// 描述: 块级多行文本件: 折行交给 ImGui 文本层(PushTextWrapPos + TextUnformatted), 本件不再自绘折行
			int64_t TextMulti(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText)
			{
				const int64_t qwOper = 0;					// 本件无交互结论 ⇒ 恒 0
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				// 终止绘制: 矩形非法(裁剪边界不可用) ⇒ 本件不产出像素
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}
				// 内距: 上下左右各取一枚设计内边距(不超过矩形短边的一半 ⇒ 文本区恒有可用宽高)
				FLOAT fPad = sizeinfo.padding;
				if (fPad < 0.0f)
				{
					fPad = 0.0f;
				}
				const FLOAT fMaxPad = ((size.x < size.y) ? size.x : size.y) * 0.5f;
				if (fPad > fMaxPad)
				{
					fPad = fMaxPad;
				}
				const FLOAT fWrapW = size.x - fPad * 2.0f;		// 文本区宽(折行宽度)
				const FLOAT fAreaH = size.y - fPad * 2.0f;		// 文本区高(可排行数 = 该高 / 设计字号)
				if (fWrapW < 1.0f || fAreaH < 1.0f)
				{
					return qwOper;
				}
				LPCSTR lpBody = (lpText != NULL) ? lpText : "";
				ImGui::PushID(id);
				ImGui::SetCursorPos(pos);
				const ImVec2 boot = ImGui::GetCursorScreenPos();
				const ImVec2 end = ImVec2(boot.x + size.x, boot.y + size.y);
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));
				ImGui::SetCursorPos(ImVec2(pos.x + fPad, pos.y + fPad));
				// 块级排版: 自文本区左上角起排, 按文本区宽折行; 超出部件矩形的行被裁掉(不纵向居中, 行数随内容变化)
				ImGui::PushTextWrapPos(pos.x + fPad + fWrapW);
				ImGui::PushClipRect(boot, end, TRUE);
				ImGui::TextUnformatted(lpBody);
				ImGui::PopClipRect();
				ImGui::PopTextWrapPos();
				ImGui::PopStyleColor();
				ImGui::PopID();
				return qwOper;
			}
			// 函数: CheckBox
			// 描述: 自定义复选框件
			int64_t CheckBox(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, BOOL& IsChecked, LPCSTR lpLabel)
			{
				int64_t qwOper = 0;
				uint16_t wOwn = 0;							// bit[15:0]: 专属段码(互斥; 0 = 无)
				if (way.eDesc != PALETTE)
				{
					return qwOper;
				}
				CONST Palette& palette = way.palette;
				if (size.x <= 0.0f || size.y <= 0.0f)
				{
					return qwOper;
				}

				LPCSTR lpText = (lpLabel != NULL && lpLabel[0] != '\0') ? lpLabel : "##CHECKBOX";
				bool bChecked = (IsChecked == TRUE);

				ImGui::PushID(id);

				ImGuiWindow* pWindow = ImGui::GetCurrentWindow();
				if (pWindow->SkipItems)
				{
					ImGui::PopID();
					return qwOper;
				}
				const ImGuiID itemId = pWindow->GetID(lpText);
				float squareSize = size.y - (float)sizeinfo.padding;
				struct
				{
					struct
					{
						ImVec2 pos;
						ImVec2 size;
					} rect, box, text;
				}geo;

				// 整体矩形: 左上角取光标屏幕坐标, 尺寸取声明尺寸
				ImGui::SetCursorPos(pos);
				geo.rect.pos = ImGui::GetCursorScreenPos();
				geo.rect.size = size;

				// 方框: 声明高内垂直居中, 紧贴左侧(复用工程居中辅助)
				geo.box.size = ImVec2(squareSize, squareSize);
				geo.box.pos = ImVec2(geo.rect.pos.x, geo.rect.pos.y + CenterOffset(size.y, squareSize));

				// 文字: 显示尺寸只统计 "##" 之前, 声明高内垂直居中, 接在方框右侧
				geo.text.size = ImGui::CalcTextSize(lpText, NULL, true);
				geo.text.pos = ImVec2(geo.box.pos.x + geo.box.size.x + sizeinfo.padding, geo.rect.pos.y + CenterOffset(size.y, geo.text.size.y));

				const ImRect totalRect(geo.rect.pos, geo.rect.pos + geo.rect.size);

				ImGui::ItemSize(size, 0.0f);
				if (!ImGui::ItemAdd(totalRect, itemId))
				{
					ImGui::PopID();
					return qwOper;
				}

				// 配色: 逐控件推入本层次方案, 自绘时从栈中取色
				ImGui::PushStyleColor(ImGuiCol_Border, StateColorU32(palette.crBorder, WIDGET_STATE_BASE));
				ImGui::PushStyleColor(ImGuiCol_Text, StateColorU32(palette.crForeground, WIDGET_STATE_BASE));

				// 交互: 整个声明区域可点, 键盘导航保留
				bool hovered = false;
				bool held = false;
				bool pressed = ImGui::ButtonBehavior(totalRect, itemId, &hovered, &held);
				if (pressed)
				{
					bChecked = !bChecked;
					IsChecked = bChecked ? TRUE : FALSE;
					wOwn = OPER_EDIT_CHANGED;
					ImGui::MarkItemEdited(itemId);
				}

				ImDrawList* pDrawList = pWindow->DrawList;
				
				WidgetState state = WIDGET_STATE_BASE;
				if( hovered){
					state |= WIDGET_STATE_HOVERED;
				}
				if (held && hovered)
				{
					state |= WIDGET_STATE_ACTIVE;
				}

				const ImU32 crBack = palette.crBackground.StateColor(state);
				const ImU32 crCheck = palette.crForeground.crBase;

				ImGui::RenderNavHighlight(totalRect, itemId);

				// 绘制: 方框面 + 描边, 勾选标记, 文字
				ImGui::RenderFrame(geo.box.pos, geo.box.pos + geo.box.size, crBack, true, fRounding);

				if (bChecked)
				{
					ImGui::RenderCheckMark(pDrawList, geo.box.pos + ImVec2(sizeinfo.padding, sizeinfo.padding), crCheck, geo.box.size.x - sizeinfo.padding * 2.0f);
				}

				if (geo.text.size.x > 0.0f)
				{
					ImGui::RenderText(geo.text.pos, lpText);
				}

				ImGui::PopStyleColor(2);
				ImGui::PopID();

				qwOper = (int64_t)wOwn;
				return qwOper;
			}
			// 函数: RenderSurface
			// 描述: 渲染面件: 有纹理时等比居中绘制, 无纹理时画占位面
			BOOL RenderSurface(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CONST VOID* pTexture, INT32 texWidth, INT32 texHeight)
			{
				if (way.eDesc != PALETTE)
				{
					return FALSE;
				}
				CONST Palette& palette = way.palette;
				const BOOL bHaveImage = (pTexture != NULL && texWidth > 0 && texHeight > 0) ? TRUE : FALSE;
				// 1. 有图: 等比居中绘制(只改绘制区, 不改控件矩形)
				if (bHaveImage == TRUE && size.x > 0.0f && size.y > 0.0f)
				{
					FLOAT scale = size.x / (FLOAT)texWidth;
					if ((size.y / (FLOAT)texHeight) < scale)
					{
						scale = size.y / (FLOAT)texHeight;
					}
					const FLOAT wDraw = (FLOAT)texWidth * scale;
					const FLOAT hDraw = (FLOAT)texHeight * scale;
					ImGui::PushID(id);
					ImGui::SetCursorPos(ImVec2(pos.x + CenterOffset(size.x, wDraw), pos.y + CenterOffset(size.y, hDraw)));
					// 纹理视图经 ImTextureRef 包装(1.92 起绘制入参即该类型; 其 void* 构造正是为"后端纹理指针"而设)
					ImGui::Image(ImTextureRef((void*)pTexture), ImVec2(wDraw, hDraw));
					ImGui::PopID();
					return TRUE;
				}
				// 2. 无图: 占位面(圆角底 + 描边, 取色与 CTRL_TYPE_RECT 同口径) + 居中占位文案
				ImGui::PushID(id);
				ImGui::SetCursorPos(pos);
				ImDrawList* dl = ImGui::GetWindowDrawList();
				CONST ImVec2 o = ImGui::GetCursorScreenPos();
				DrawRectFace(dl, o, size, StateColorU32(palette.crBackground, WIDGET_STATE_BASE),
					StateColorU32(palette.crBorder, WIDGET_STATE_BASE));
				{
					// 横向居中用实测文本宽(宽只能实测); 纵向用设计字号(容器高 = 部件矩形高)
					LPCSTR lpTip = u8"（等待结果图）";
					CONST ImVec2 ts = ImGui::CalcTextSize(lpTip);
					dl->AddText(ImVec2(o.x + CenterOffset(size.x, ts.x), o.y + CenterOffset(size.y, sizeinfo.fontSize)),
						StateColorU32(palette.crForeground, WIDGET_STATE_BASE), lpTip);
				}
				ImGui::PopID();
				return FALSE;
			}
		}
		// ================================
		// 区域: 命令部件实现(Widgets::Command)
		// ================================
		namespace Command
		{
			// 函数: List
			// 描述: 命令部件: 按列表描述符一次画完(六步), 逐行收集操作码并回调
			int64_t List(CONST ListSpec& spec)
			{
				int64_t qwOper = 0;						// 本帧收集到的操作码(最后一个非 0 值; 0 = 无码不回调)
				uint64_t qwParam = 0;					// 交回调的 param(来源行号; 拖拽重排 = 源行号)
				const FLOAT fInner = sizeinfo.padding;	// 列表内距
				const FLOAT fGap = sizeinfo.space;		// 行间距
				vec2 szItem{};							// 单项尺寸
				vec2 posCur{};							// 当前项位置(列表子窗本地坐标)
				FLOAT fStep = 0.0f;						// 递增步长(纵向)
				const BOOL bSpecial = (spec.pfnSpecial != NULL) ? TRUE : FALSE;
				const FLOAT fSpecialH = (FLOAT)((spec.dwSpecialFlag >> 16) & 0xFFFF);
				const BOOL bSpecialFirst = (bSpecial && ((spec.dwSpecialFlag & 0xFFFF) == LIST_SPECIAL_FIRST)) ? TRUE : FALSE;
				DWORD dwSpecialId = 0;					// 特殊行 ID(依次递增: 在前取 0, 在后取 dwCount)
				DWORD dwCount = spec.dwCount;			// 项目数量(无数据首址时按 0 处理)
				// 前置校验
				if (spec.pfnItem == NULL || spec.size.x <= 0.0f || spec.size.y <= 0.0f)
				{
					return qwOper;
				}
				if (spec.lpItemBase == NULL)
				{
					dwCount = 0;					// 无数据首址: 只画特殊行(若有)
				}
				// ① 压入列表样式(子窗圆角 + 壳体底色)
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, spec.fRounding);
				ImGui::SetCursorPos(toImvec2(spec.pos));
				ImGui::PushStyleColor(ImGuiCol_ChildBg, spec.crShell);
				// ② 创建 Child
				ImGui::BeginChild(spec.label ? spec.label : u8"##LISTPANEL", toImvec2(spec.size), true, 0);
				ImGui::PopStyleColor();
				// ③ 计算项目行几何，含递增数值
				{
					FLOAT fW = ImGui::GetWindowWidth() - fInner * 2.0f;
					if (spec.fRowH * dwCount > spec.size.y)
					{
						fW = fW - sizeinfo.padding * 2.0f;
					}
					if (fW < 1.0f)
					{
						fW = 1.0f;
					}
					szItem = vec2(fW, spec.fRowH);
					posCur = vec2(fInner, fInner);
					fStep = spec.fRowH + fGap;
				}
				// 行件绘制方式: 配色方案随描述符传入(spec.palette; 调用方按列表所属控件层次构造)
				CONST PaintWay way = Base::MakePaletteWay(spec.palette);
				// 特殊行: 优先绘制(位置 = 在前)
				if (bSpecial && bSpecialFirst)
				{
					int64_t qwSpecial = spec.pfnSpecial((int)dwSpecialId, posCur,
						vec2(szItem.x, fSpecialH), way, spec.lpSpecial);
					if (qwSpecial != 0)
					{
						// 被携带 ⇒ 折算进 oper(搭载段) + param(行号); 收集口径 = 最后一个非 0 值
						ListCollectRowOper(qwSpecial, dwSpecialId, qwOper, qwParam);
					}
					posCur.y += fSpecialH + fGap;
				}
				// 遍历项目数据, 逐项调用行件并管理 oper
				for (DWORD i = 0; i < dwCount; i++)
				{
					VOID* lpItem = (VOID*)((BYTE*)spec.lpItemBase + spec.cbItem * i);
					int64_t qwRow = spec.pfnItem((int)i, posCur, szItem, way, lpItem);
					if (qwRow != 0)
					{
						ListCollectRowOper(qwRow, i, qwOper, qwParam);	// 管理 oper: 收集(取最后一个非 0 值)
					}
					posCur.y += fStep;
				}
				// 特殊行: 位置 = 在后
				if (bSpecial && bSpecialFirst == FALSE)
				{
					dwSpecialId = dwCount;
					int64_t qwSpecial = spec.pfnSpecial((int)dwSpecialId, posCur,
						vec2(szItem.x, fSpecialH), way, spec.lpSpecial);
					if (qwSpecial != 0)
					{
						ListCollectRowOper(qwSpecial, dwSpecialId, qwOper, qwParam);
					}
					posCur.y += fSpecialH + fGap;
				}
				// ④ 关列表子窗
				ImGui::EndChild();
				// ⑤ 弹出样式
				ImGui::PopStyleVar();
				// ⑥ 执行回调: oper = 收集到的操作码(行件结论被携带 ⇒ 落搭载段), param = 来源行号
				//    (拖拽重排 = 源行号; 目标插入位置在搭载段里)
				if (qwOper != 0 && spec.pfnEvent != NULL)
				{
					spec.pfnEvent(spec.pEventObj, (uint64_t)qwOper, qwParam, spec.pEventData);
				}
				return qwOper;
			}
		}

		// ================================
		// 区域: 管道部件实现(Widgets::Pipe)
		// ================================
		namespace Pipe
		{
			// 函数: ListRow
			// 描述: 管道部件: 进入同一列表子窗逐行绘制(五步), 行游标与几何由调用方算好
			int64_t ListRow(CONST ListPipe& pipe, int nRowId, vec2 posRow, FLOAT fRowH,
				VOID* lpItem, pfnListWidget pfnItem, CONST PaintWay& way)
			{
				int64_t qwOper = 0;
				const FLOAT fInner = sizeinfo.space;	// 列表内距
				// 终止绘制: 列表尺寸非法(行件为 NULL 时仍维护列表本身 ⇒ 不在此提前返回)
				if (pipe.size.x <= 0.0f || pipe.size.y <= 0.0f)
				{
					return qwOper;
				}
				// ① 压入列表样式: 列表子窗圆角 + 子窗底色
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, pipe.fRounding);
				ImGui::SetCursorPos(toImvec2(pipe.pos));
				ImGui::PushStyleColor(ImGuiCol_ChildBg, pipe.crShell);
				// ② 进入列表子窗: 稳定 ID = label ⇒ 同帧重复进入同一子窗口(在已有空间上继续绘制)
				ImGui::BeginChild(pipe.label ? pipe.label : u8"##LISTPANEL", toImvec2(pipe.size), true, 0);
				ImGui::PopStyleColor();
				// ③ 调用行件绘制本行: 行件自行定位到 posRow; pfnItem 为 NULL 时只维护列表本身
				if (pfnItem != NULL)
				{
					FLOAT fW = ImGui::GetWindowWidth() - fInner * 2.0f;
					if (fRowH * pipe.dwCount > pipe.size.y)
					{
						fW = fW - sizeinfo.padding * 2.0f;
					}
					if (fW < 1.0f)
					{
						fW = 1.0f;
					}
					qwOper = pfnItem(nRowId, posRow, vec2(fW, fRowH), way, lpItem);
				}
				// ④ 关列表子窗
				ImGui::EndChild();
				// ⑤ 弹出样式
				ImGui::PopStyleVar();
				return qwOper;
			}
		}
	}	// namespace Widgets
}	// namespace Controls

// ================================
// 区域: 内部辅助函数区(定义)
// 说明: 本区为部件层内部辅助的函数体(前向声明见文件上方), 文件作用域 static。
// ================================
namespace Controls
{
	namespace Widgets
	{

		// 函数: DrawRectFace
		// 描述: 画矩形外观(圆角底 + 圆角描边; 色值由调用方给出, 本函数只画不取色)
		static VOID DrawRectFace(ImDrawList* dl, ImVec2 o, ImVec2 size, DWORD crBase, DWORD crBorder)
		{
			if (dl == NULL)
			{
				return;
			}
			ImVec2 e(o.x + size.x, o.y + size.y);
			dl->AddRectFilled(o, e, crBase, fRounding);
			dl->AddRect(o, e, crBorder, fRounding, 0, 1.0f);
		}
		// 函数: ComboItems
		// 描述: 在已打开的下拉列表内提交项表(Combo 与 ComboAction 共用, 展开列表观感由此单一处定义)
		static BOOL ComboItems(LPCSTR* lpItems, INT nCount, INT& nIndex)
		{
			const INT nOld = nIndex;
			// 行内文字垂直居中 + 行间距 0: 行高由项尺寸给出(= 声明控件高)
			ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
			for (INT i = 0; i < nCount; i++)
			{
				const BOOL IsSelected = (nIndex == i) ? TRUE : FALSE;
				const ImVec2 item_size(0.0f, sizeinfo.controlHeight);	// 宽 0 = 自动撑满列表宽; 高 = 声明控件高
				if (ImGui::Selectable(lpItems[i], IsSelected != FALSE, 0, item_size))
				{
					nIndex = i;
				}
				if (IsSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::PopStyleVar(2);
			return (nOld != nIndex) ? TRUE : FALSE;
		}
		// 函数: ListRowDragHandle
		// 描述: 行首拖拽把手(拖拽源): 只登记拖拽并自绘图标, 返回占用宽度供调用方让位
		static FLOAT ListRowDragHandle(int id, vec2 pos, vec2 size, DWORD dwRow)
		{
			FLOAT fRet = 0.0f;
			if (size.x <= 0.0f || size.y <= 0.0f)
			{
				return fRet;
			}
			// 把手宽度 = 行高, 但不超过行宽一半(窄列表下保证行内容仍可见)
			fRet = sizeinfo.controlHeight;
			if (fRet > size.x * 0.5f)
			{
				fRet = size.x * 0.5f;
			}
			if (fRet < 1.0f)
			{
				fRet = 1.0f;
			}
			ImGui::PushID(id);
			ImGui::SetCursorPos(toImvec2(pos));
			const ImVec2 handleMin = ImGui::GetCursorScreenPos();	// 把手区屏幕原点(图标绘制用)
			ImGui::InvisibleButton(u8"##DRAGHANDLE", ImVec2(fRet, size.y));
			const BOOL bHovered = ((ImGui::IsItemHovered() != false) || (ImGui::IsItemActive() != false)) ? TRUE : FALSE;
			// 拖拽源: 按下把手后拖动即开始拖拽(只点一下不动 = 无操作)
			if (ImGui::BeginDragDropSource())
			{
				ListRowDragPayload payload{};
				payload.dwRow = dwRow;
				ImGui::SetDragDropPayload(KANATUI_GENERATE_LIST_DRAG_TYPE, &payload, sizeof(payload));
				ImGui::Text(u8"第 %u 行", (unsigned)dwRow);
				ImGui::EndDragDropSource();
			}
			// 把手图标: 三枚横线(自绘, 不占项); 本区为部件层内部副本 ⇒ 内部上下文直接取本层变量
			{
				ImDrawList* dl = ImGui::GetWindowDrawList();
				Base::StateColors crHandle{};
				crHandle.crBase = ProjColor(PROJ_L_BORDER_HOVER);
				crHandle.crHovered = ProjColor(PROJ_L_BORDER_ACTIVE);
				const ImU32 crLine = Base::StateColorU32(crHandle,
					(bHovered != FALSE) ? Base::WIDGET_STATE_HOVERED : Base::WIDGET_STATE_BASE);
				const FLOAT fLineW = fRet * 0.5f;
				const FLOAT fLineH = 2.0f;
				const FLOAT fLineGap = sizeinfo.padding;
				const FLOAT fIconH = fLineH * 3.0f + fLineGap * 2.0f;	// 图标总高(三线 + 两缝隙)
				const FLOAT cx = handleMin.x + Base::CenterOffset(fRet, fLineW);
				const FLOAT cy = handleMin.y + Base::CenterOffset(size.y, fIconH) + fIconH * 0.5f;
				for (INT k = -1; k <= 1; k++)
				{
					const FLOAT ly = cy + (FLOAT)k * (fLineH + fLineGap) - fLineH * 0.5f;
					dl->AddRectFilled(ImVec2(cx, ly), ImVec2(cx + fLineW, ly + fLineH), crLine);
				}
			}
			ImGui::PopID();
			return fRet;
		}
		// 函数: ListRowDropTarget
		// 描述: 整行登记为放置目标并在该侧自绘插入线(落点换算走公开件 ListRowDropOper; 返回 0 = 无投放)
		static int64_t ListRowDropTarget(int id, vec2 pos, vec2 size, DWORD dwRow)
		{
			int64_t qwRet = 0;
			if (size.x <= 0.0f || size.y <= 0.0f)
			{
				return qwRet;
			}
			ImGui::PushID(id);
			ImGui::SetCursorPos(toImvec2(pos));
			const ImVec2 cur = ImGui::GetCursorScreenPos();
			const ImRect drop = { cur , cur + toImvec2(size) };
			if (ImGui::BeginDragDropTargetCustom(drop, ImGui::GetID(u8"##LISTROWDROP")))
			{
				const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(KANATUI_GENERATE_LIST_DRAG_TYPE, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
				if (payload != NULL && payload->DataSize == (int)sizeof(ListRowDragPayload))
				{
					// 状态变量: 源行号取拖拽载荷; 目标行号取本行入参 dwRow —— 二者语义不同,
					// 不得同名遮蔽(否则落点恒为"源行 / 源行 + 1"⇒ 经消费者换算后恒判"拖回原位"⇒ 拖拽失效)
					const DWORD dwSrc = ((const ListRowDragPayload*)payload->Data)->dwRow;
					const FLOAT fMid = (drop.Min.y + drop.Max.y) * 0.5f;
					const BOOL IsAfter = (ImGui::GetIO().MousePos.y > fMid) ? TRUE : FALSE;
					// 外观变量(内缩量取半枚内边距; 圆角取本帧圆角)
					const float padding = sizeinfo.padding / 2;
					const float rounding = fRounding;
					const float thickness = 2.0f;
					const DWORD crEmphasis = ProjColor(PROJ_L_BORDER_ACTIVE);
					float vert = IsAfter ? cur.y + size.y - thickness : cur.y;
					ImDrawList* drawlist = ImGui::GetWindowDrawList();
					ImRect border = { cur - ImVec2(padding,padding), cur + toImvec2(size) + ImVec2(padding,padding) };
					ImRect line = { {cur.x, vert}, {cur.x + size.x, vert + thickness} };
					// 开始绘制
					drawlist->AddRectFilled(line.Min, line.Max, crEmphasis, rounding, ImDrawFlags_RoundCornersAll);
					drawlist->AddRect(border.Min, border.Max, crEmphasis, rounding, ImDrawFlags_None, 2.0f);
					// 投放: 落点换算走公开件 ListRowDropOper(目标位置只随本行与上/下半而定)
					if (payload->IsDelivery())
					{
						qwRet = Controls::ListRowDropOper(dwSrc, dwRow, IsAfter);
					}
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
			return qwRet;
		}
		// 函数: ListCollectRowOper
		// 描述: 把行件返回约定折算进控件的 oper 与回调 param(收集口径 = 取最后一个非 0 值)
		static VOID ListCollectRowOper(int64_t qwRow, DWORD dwRow, int64_t& qwOper, uint64_t& qwParam)
		{
			if (qwRow == 0)
			{
				return;
			}
			CONST uint16_t wOwn = (uint16_t)((uint64_t)qwRow & 0xFFFF);				// bit[15:0]: 专属段码
			CONST uint16_t wCommon = (uint16_t)(((uint64_t)qwRow >> 16) & 0xFFFF);	// bit[31:16]: 公共位图
			CONST uint32_t dwExtra1 = (uint32_t)(((uint64_t)qwRow >> 32) & 0xFFFF);	// bit[47:32]: 附属数据 1
			CONST uint32_t dwExtra2 = (uint32_t)(((uint64_t)qwRow >> 48) & 0xFFFF);	// bit[63:48]: 附属数据 2
			if (wOwn == OPER_LIST_ROW_MOVED)
			{
				// 重排: 专属段 = 列表自己的结论; 搭载段 = 目标插入位置(数据); param = 源行号
				qwOper = (int64_t)OperCode::MakeCode(OperCode::MakeCommon(OPER_COMMON_NONE, OPER_PAYLOAD_DATA),
					OPER_LIST_ROW_MOVED, dwExtra1);
				qwParam = (uint64_t)dwExtra2;
				return;
			}
			// 普通行内事件: 不占专属段; 搭载段 = 行内部件码(操作码); param = 本行行号
			qwOper = (int64_t)OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_OPCODE), 0, (uint32_t)wOwn);
			qwParam = (uint64_t)dwRow;
		}

	}	// namespace Widgets
}	// namespace Controls