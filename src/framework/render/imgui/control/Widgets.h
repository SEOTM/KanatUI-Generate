// ================================================
// 库名: Widgets
// 描述: 控件库的部件层(可复用绘制件; 不含控件分派与控件数据)
// ================================================

// ================================
// 区域: 编译常量定义区
// ================================
// 说明: 部件实现在本头文件的实现件(Widgets.cpp)内使用 ImVec2 运算符重载 ⇒ 宏须在 imgui.h 之前定义;
//       本处再包一层条件编译, 便于其他包含者按任意顺序包含本头文件。
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>					// DWORD / BOOL / INT32 / LPCSTR 等基础类型(winnt.h)
#include "framework/base/BasicDesign.h"		// AutoSize / ProjCR(部件上下文的注入面)
#include "framework/base/ControlSpec.h"		// vec2(部件几何入参)
#include "imgui.h"						// ImVec2 / ImGuiDataType(部件签名)
#include "imgui_internal.h"				// ImGuiItemFlags_Disabled(项禁用标志读取)

// ================================
// 命名空间: Controls::Widgets(部件 —— 控件库内部共享)
// 说明: 部件三型(基础 / 命令 / 管道)、固定参数与绘制方式、取色口径、返回约定与各件语义见 Widgets.md。
// ================================
namespace Controls
{
	namespace Widgets
	{
		// ================================
		// 区域: 基础部件(Widgets::Base; 控件库内部)
		// ================================
		namespace Base
		{
			// 类型: WidgetState
			enum WidgetState : DWORD
			{
				WIDGET_STATE_BASE = 0,		// 基础状态(默认)
				WIDGET_STATE_HOVERED = 1,	// 悬停
				WIDGET_STATE_ACTIVE = 2,		// 激活(按下)
				WIDGET_STATE_DISABLE = 4,	// 禁用
			};
			// 函数: WidgetState::operator |
			// 描述: 位或: 合并两枚部件状态位
			static inline WidgetState operator|(WidgetState lhs, WidgetState rhs)
			{
				return static_cast<WidgetState>(static_cast<DWORD>(lhs) | static_cast<DWORD>(rhs));
			}
			// 函数: WidgetState::operator &
			// 描述: 位与: 判定部件状态位是否置位
			static inline WidgetState operator&(WidgetState lhs, WidgetState rhs)
			{
				return static_cast<WidgetState>(static_cast<DWORD>(lhs) & static_cast<DWORD>(rhs));
			}
			// 函数: WidgetState::operator |=
			// 描述: 复合位或: 就地置位部件状态
			static inline WidgetState& operator|=(WidgetState& lhs, WidgetState rhs)
			{
				return lhs = lhs | rhs;
			}
			// 类型: StateColors
			struct StateColors
			{
				DWORD crBase;			// 常态色
				DWORD crHovered;		// 悬停色
				DWORD crActive;			// 激活(按下)色
				DWORD crDisable;		// 禁用色
				// 方法: StateColor
				DWORD StateColor(WidgetState state) const
				{
					if (state & WIDGET_STATE_DISABLE) return crDisable;
					if (state & WIDGET_STATE_ACTIVE)  return crActive;
					if (state & WIDGET_STATE_HOVERED) return crHovered;
					return crBase;
				}
			};
			// 类型: Palette
			struct Palette
			{
				StateColors crBackground;	// 底色(整条背景)
				StateColors crForeground;	// 底色面上的前景色(文本/图形)
				StateColors crSurface;		// 表面色(段面: 两段件的右段/按钮)
				StateColors crSurfaceForeground;	// 段面上的前景色(文本/图形)
				StateColors crBorder;		// 边框色(整条外框)
				StateColors crBorderInner;	// 内边框色
			};

			// 类型: desc
			enum desc
			{
				PALETTE = 0,	// 方式一: 配色方案
				IMAGE = 1		// 方式二: 纹理(预留)
			};

			// 类型: PaintWay
			struct PaintWay
			{
				desc eDesc;				// 绘制方式(判别位)
				union
				{
					Palette palette;	// 方式一: 配色方案(成品; 调用方按层次采样后传入)
					void* pImage;		// 方式二: 纹理载荷(预留; 纹理引用本体随控件缓冲传入)
				};
			};

			// 函数: MakePaletteWay
			// 描述: 构造 PALETTE 绘制方式(值返回, 供调用点直接作实参)
			static inline PaintWay MakePaletteWay(CONST Palette& palette)
			{
				PaintWay way{};
				way.eDesc = PALETTE;
				way.palette = palette;
				return way;
			}

			// 函数: ExitSign
			// 描述: 关闭按钮部件: 两段交叉线符号 + 悬停底(标题栏用)
			BOOL ExitSign(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR label, FLOAT fPadding, FLOAT thickness);
			// 函数: StateColorU32
			// 描述: 把状态色换算为 ImDrawList 可用色(再按当前 Style.Alpha 乘算)
			static inline ImU32 StateColorU32(CONST StateColors& crs, WidgetState state)
			{
				return ImGui::GetColorU32((ImU32)crs.StateColor(state));
			}
			// 函数: ItemDisabled
			// 描述: 读最后一个项的禁用标志(TRUE = 该项本帧被禁用)
			static inline BOOL ItemDisabled()
			{
				return (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) ? TRUE : FALSE;
			}
			// 函数: CenterOffset
			// 描述: 计算容器与内容的居中偏移量(下限 0, 内容不小于容器时返回 0)
			static inline FLOAT CenterOffset(FLOAT parent, FLOAT child)
			{
				FLOAT fRet = (parent - child) * 0.5f;
				if (fRet < 0.0f)
				{
					fRet = 0.0f;			// 内容不小于容器: 不出现负偏移
				}
				return fRet;
			}
			// ================================
			// 区域: 帧类单件(无右段按钮的整格件; 共用口径见 Widgets.md 的 InputText 节)
			// 返回约定: 位表与产出清单见 Widgets.md 的 InputText 节
			// 说明: 底由 ImGui 原生帧绘制(内边距按声明高对齐); 边框画法按件分两类(见 Widgets.md 的 InputText 节)
			// ================================
			// 函数: InputText
			// 描述: 帧类单行文本件(底与边框由 ImGui 原生帧绘制, 帧高对齐声明高)
			int64_t InputText(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CHAR* lpText, size_t nTextCap,
				LPCSTR lpHint);
			// 函数: InputTextMulti
			// 描述: 帧类多行文本件(项尺寸取声明矩形, 不做单行那样的垂直居中)
			int64_t InputTextMulti(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CHAR* lpText, size_t nTextCap,
				LPCSTR lpHint);
			// 函数: InputScalar
			// 描述: 帧类数值件(底与边框由 ImGui 原生帧绘制, 文字经原生内边距居中)
			int64_t InputScalar(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, VOID* pData, ImGuiDataType eType,
				LPCSTR lpFormat, LPCSTR lpHint);
			// 函数: Combo
			// 描述: 帧类下拉件(展开列表的项表与 ComboAction 同源)
			int64_t Combo(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR* lpItems, INT nCount,
				INT& nIndex);
			// 函数: TimeSpan
			// 描述: 时段显示件: 自绘圆角底 + 起点到终点的时码文本(无交互项)
			int64_t TimeSpan(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t nStartMs, int64_t nEndMs,
				int64_t nLimitMs);
			// 函数: InputAction
			// 描述: 两段件: 左段单行输入 + 右段按钮(两段底与边框在项提交后补画)
			int64_t InputAction(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, FLOAT fBtnW, CHAR* lpText,
				size_t nTextCap, LPCSTR lpHint, LPCSTR lpBtn);
			// 函数: ComboAction
			// 描述: 两段件: 左段下拉框 + 右段按钮(补画口径与 InputAction 一致)
			int64_t ComboAction(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, FLOAT fBtnW,
				LPCSTR* lpItems, INT nCount, INT& nIndex, LPCSTR lpBtn);
			// 函数: SingleSlider
			// 描述: 单手柄滑动条件(命中区为覆盖整个控件矩形的无绘制项)
			int64_t SingleSlider(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t& value, int64_t lMin, int64_t lMax);
			// 函数: ProgressStatic
			// 描述: 进度状态显示件(底与文本取自配色方案)
			BOOL ProgressStatic(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, int64_t current, int64_t expect, FLOAT fPadding);

			// ================================
			// 区域: 同名部件(与控件同名的基础部件; 语义见 Widgets.md 的概述)
			// ================================

			// 函数: Button
			// 描述: 按钮件: 三态面 + 面内文字(自带可交互项)
			int64_t Button(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText);

			// 函数: Text
			// 描述: 文本图元件: 部件矩形内垂直居中绘制一行文本, 裁剪到部件矩形
			int64_t Text(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText);

			// 函数: TextMulti
			// 描述: 块级多行文本件: 文本区四边内缩一枚设计内边距, 按文本区宽折行
			int64_t TextMulti(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, LPCSTR lpText);

			// 函数: CheckBox
			// 描述: 复选框件: 方框/勾/文字由 ImGui 原生控件绘制, 勾选态经引用写回
			int64_t CheckBox(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, BOOL& IsChecked, LPCSTR lpLabel);

			// 函数: RenderSurface
			// 描述: 渲染面件: 有纹理时等比居中绘制, 无纹理时画占位面
			BOOL RenderSurface(int id, ImVec2 pos, ImVec2 size, CONST PaintWay& way, CONST VOID* pTexture, INT32 texWidth, INT32 texHeight);

			// ================================
			// 区域: 列表基础部件(行件; 语义见 Widgets.md 的 pfnListWidget 节)
			// ================================

			// 类型: pfnListWidget
			using pfnListWidget = int64_t(*)(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem);

			// 函数: InputAction(列表行重载)
			// 描述: 列表行件: 解包行数据后转单件实现(行首拖拽把手占位)
			int64_t InputAction(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem);

			// 函数: InputActionRO(列表行重载, 只读)
			// 描述: 列表行件只读形态: 文本容量按 0 传入, 单件走只读分支
			int64_t InputActionRO(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem);

			// 函数: ComboAction(列表行重载)
			// 描述: 列表行件: 解包选项表后转单件实现, 触发结论打包返回
			int64_t ComboAction(int id, vec2 pos, vec2 size, CONST PaintWay& way, VOID* lpItem);
		}

		// ================================
		// 区域: 命令部件(Widgets::Command; 语义见 Widgets.md 的 List 节)
		// ================================
		namespace Command
		{
			using Base::pfnListWidget;		// 行件函数指针(描述符字段的类型)
			using Base::Palette;			// 部件配色(描述符字段与实现体内构造行件配色用)
			using Base::PaintWay;			// 绘制方式(行件调用时的第四固定参数)

			// 结构: ListSpec
			struct ListSpec
			{
				int id;						// 列表 ID
				vec2 pos;					// 列表矩形左上角(窗口本地坐标)
				vec2 size;					// 列表矩形尺寸
				FLOAT fRowH;				// 行高(控件取 sizeinfo.controlHeight)
				DWORD dwSelected;			// 选中项(LIST_SEL_NONE = 未选); 命令部件不消费
				DWORD dwCount;				// 项目数量
				Palette palette;			// 行件配色方案(调用方按列表所属控件层次构造)
				DWORD crShell;				// 列表壳体底色(调用方按列表所属控件层次采样)
				pfnListWidget pfnItem;		// 行件(NULL = 不绘制任何行)
				VOID* lpItemBase;			// 项目数据首址
				size_t cbItem;				// 单项目数据长度(步进)
				pfnListWidget pfnSpecial;	// 特殊行行件(NULL = 无特殊行)
				VOID* lpSpecial;			// 特殊行数据首址
				size_t cbSpecial;			// 特殊行数据长度; 命令部件不消费(行件按自身约定解释行块)
				DWORD dwSpecialFlag;		// 特殊行标志(LOW16 = 位置; HIGH16 = 高度(像素))
				pfnCtrlEvent pfnEvent;		// 事件回调(第 6 步; NULL = 无事件出口)
				VOID* pEventObj;			// 回调 obj(通常 = CtrlInfo*)
				void* pEventData;			// 回调 data(通常 = 控件缓冲)
				LPCSTR label;				// 列表子窗标签(NULL = 默认)
				FLOAT fRounding;			// 列表子窗圆角(= 当前帧圆角; 设计内边距)
			};

			// 函数: List
			// 描述: 命令部件: 按列表描述符一次画完并逐行收集操作码(六步见 Widgets.md 的 List 节)
			int64_t List(CONST ListSpec& spec);
		}

		// ================================
		// 区域: 管道部件(Widgets::Pipe; 语义见 Widgets.md 的 ListRow 节)
		// ================================
		namespace Pipe
		{
			using Base::pfnListWidget;		// 行件函数指针(逐行送入时的行绘制入口)
			using Base::Palette;			// 部件配色(调用方构造一次后逐行送入)
			using Base::PaintWay;			// 绘制方式(行件调用时的第四固定参数)

			// 结构: ListPipe
			struct ListPipe
			{
				int id;					// 列表 ID(送入方控件 id)
				vec2 pos;				// 列表矩形左上角(父窗口本地坐标)
				vec2 size;				// 列表矩形尺寸
				FLOAT fRowH;			// 行高(控件取 sizeinfo.controlHeight)
				DWORD dwCount;			// 项目数量(仅用于判定是否需要收窄让位)
				DWORD crShell;			// 列表壳体底色(调用方按列表所属控件层次采样)
				LPCSTR label;			// 列表子窗标签(稳定 ID 的一部分; NULL = 默认)
				FLOAT fRounding;		// 列表子窗圆角(= 当前帧圆角; 设计内边距)
			};

			// 函数: ListRow
			// 描述: 管道部件: 逐行送入并绘制一行(五步见 Widgets.md 的 ListRow 节)
			int64_t ListRow(CONST ListPipe& pipe, int nRowId, vec2 posRow, FLOAT fRowH,
				VOID* lpItem, pfnListWidget pfnItem, CONST PaintWay& way);
		}

		// ================================
		// 区域: 部件上下文(本层即时模式状态; 语义见 Widgets.md 的 UpdateContext 节)
		// ================================

		// 函数: UpdateContext
		// 描述: 更新部件层上下文(每帧渲染布局之前调用一次, 并复位编辑中计数)
		BOOL UpdateContext(CONST AutoSize& refsize, CONST ProjCR* pProj);

		// 函数: HasActiveInput
		// 描述: 查询本帧是否存在处于编辑中的输入项(供控件层判定 IME 开关)
		BOOL HasActiveInput();
	}
}