// ================================================
// 库名: Controls
// 描述: 封装 Dear ImGui 控件的自定义控件库(控件绘制实现; 部件层见 Widgets.{h,cpp})
// ================================================

// ================================
// 区域: 编译常量定义区
// ================================
#define IMGUI_DEFINE_MATH_OPERATORS

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>					// DOUBLE / DWORD 等基础类型(winnt.h)
#include "framework/render/imgui/control/Controls.h"
#include "framework/render/imgui/control/Widgets.h"	// 部件层声明(逐控件绘制调用其部件)
#include "framework/base/CtrlBuffer.h"			// 控件缓冲约定(数据结构 / 容量常量 / 文本通道取址)
#include "framework/render/imgui/ImGuiHost.h"		// 当前渲染窗口与窗口桌面位置(窗口级交互以真实位置为基线)
#include "imgui.h"
#include "imgui_internal.h"
#include "framework/base/TextWidth.h"
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <algorithm>

// ================================
// 区域: 文件内辅助函数区
// 说明: 文件作用域 static; 前向声明在此, 函数体见文件末尾"文件内辅助函数区(定义)";
//       操作码拼装统一走 ControlSpec.h 的 OperCode 命名空间(本文件不再自带拼装工具)。
// ================================
// 以下两条直接写 ImGui 绘制列表(无 id、不入 ImGui 项栈)
static VOID DrawRectFace(ImDrawList* dl, ImVec2 o, ImVec2 size, DWORD crBase, DWORD crBorder);	// 矩形外观绘制(底/描边色由调用方按投影取; 定层见 Controls::Rect)
static VOID DrawUnknownCtrlRect(CtrlInfo* info);	// 未知类型矩形占位绘制
// 控件矩形交互辅助(定义见文件末尾辅助区)
static uint16_t CtrlClickCode(CtrlInfo* info, LPCSTR lpFallback);	// 控件矩形可交互项(单击/右键/双击) -> 公共码位图
// 列表控件辅助(定义见文件末尾辅助区)
static ListBuffer* ListBufferOf(CtrlInfo* info);	// 取列表控件的缓冲头(无则 NULL)
// 列表行返回约定折算(定义见文件末尾辅助区)
// 说明: 行首拖拽把手 / 整行放置目标 / 拖拽载荷已随部件层迁入 Widgets.cpp(部件层自持独立副本)
static VOID ListCollectRowOper(int64_t qwRow, DWORD dwRow, int64_t& qwOper, uint64_t& qwParam);	// 行件返回约定 -> oper + param

// 函数: toImvec2
// 描述: 内置类型转化为 imgui 可识别类型
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

// ================================
// 命名空间: Controls(控件库)
// 说明: 承载控件本体(部件层已独立成件: 声明见 Widgets.h、实现见 Widgets.cpp; 命名空间仍为
//       Controls::Widgets) —— 逐类型绘制实现(以类型语义名命名; TITLE 直用 TitleBar)、
//       逐控件分派入口(DrawCtrl: 定位 + 查表分派)、上下文(UpdateContext)
//       与对部件层的调用(基础部件 Base + 命令部件 Command + 管道部件 Pipe)。
//       控件接口统一为单参数 VOID Xxx(CtrlInfo* info)(外部一次性送入; 控件无"命令式/管道式"之分);
//       列表控件入口(ListInput/ListCombo)由控件自行拆解任务, 逐行送入内部管道部件(Widgets::Pipe::ListRow);
//       行内容由控件逐行传入的行件(如 Widgets::Base::InputAction)绘制。
//       命令部件(Widgets::Command::List)与管道部件(Widgets::Pipe::ListRow)并存 —— 二者使用情景
//         不一致(命令式 = 整份描述符一次画完; 管道式 = 调用方逐行送入); 本库当前仅管道式有控件调用点。
// ================================
namespace Controls
{

	// ================================
	// 区域: 常量定义区
	// ================================
	static CHAR ERRBUFFER[16] = u8"无效缓冲";	// 无效缓冲区

	// 注: 单手柄滑块的几何比例常量(SLIDER_GRIP_RATIO / SLIDER_TRACK_RATIO)只服务部件 SingleSlider,
	//     已随部件层迁入 Widgets.cpp(部件层自持副本)

	// 常量: GRAB_BASE_BY_WND_POS
	// 描述: 标题栏拖动基线的来源口径(只影响标题栏建立抓取偏移时"窗口当前位置"从哪读)
	// 说明: TRUE  = 读窗口位置记录槽(pWndPosSlot; 即 WND 件的 position) —— 位置闭环在布局数据里,
	//                控件不碰窗口句柄(现行口径; 用户 2026-10-16 手感判定 = 略差但可接受);
	//       FALSE = 读真实窗口矩形(ImGuiHost::GetWndPos) —— 以实际窗口位置为准, 属系统查询
	//                (手感对照时的备选口径)。
	//       两种口径的对照结论见项目历史记录; 切换只改本常量。
	constexpr BOOL GRAB_BASE_BY_WND_POS = TRUE;

	// ================================
	// 区域: 上下文变量定义区
	// 描述: 渲染时使用到的上下文变量(即时模式状态) —— 由 Controls::UpdateContext 每帧写入
	// ================================
	static auto sizeinfo = AutoSize::SizeInfoFLOAT{};	// 当前帧参考尺寸的像素值(控件绘制按需取用)
	static FLOAT fRounding = 0.0f;			// 当前框圆角半径(像素; 取值 = 设计内边距)
	static CONST ProjCR* pcrHander = NULL;		// 当前帧持有的投影配色(宿主持有; NULL = 未注入)
	static CONST vec2* pWndPosSlot = NULL;		// 本窗口的位置记录槽(WND 件的 position; NULL = 本窗口无记录)
	static vec2 orgContent(0.0f, 0.0f);			// 内容容器原点(窗口本地坐标; 内容控件声明坐标的零点)
	// 注: 编辑中输入项计数(原 dwDisableIME)随部件层迁入 Widgets.cpp —— 部件累加, 控件层经
	//     Widgets::HasActiveInput 查询

	// ================================
	// 区域: 控件层次与层号口径(控件库一侧)
	// 说明: 控件库逐枚无状态被调用, 看不到层级(谁在第几层只有调用方知道) ⇒ 层次由 Layout 按
	//       "父控件层次 + 自身类型步进"递推(见 ControlSpec.h 的"层次递推口径"), 随控件信息
	//       (CtrlInfo::nLayer, 有符号)送进绘制件; 本区把它换算为投影层号
	//       (唯一口径 = ControlSpec.h 的 CtrlLayerOf), 并做上下限钳制与首诊。
	//       ⇒ "调整层次观感"改层次递推口径(或控件的 parent 声明), 不再由本文件的类型常量决定。
	// ================================
	constexpr INT32 PROJ_L_DISABLED = 10;			// 禁用 / 次级(中灰) —— 与层次无关的绝对层号
	constexpr INT32 PROJ_L_BORDER = 9;			// 边框常态
	constexpr INT32 PROJ_L_BORDER_HOVER = 11;		// 边框悬停 / 拖拽把手常态
	constexpr INT32 PROJ_L_BORDER_ACTIVE = 13;	// 边框激活 / 强调(把手悬停 / 插入线)

	// 函数: CtrlLayer
	// 描述: 取某控件在投影上的绘图层号(= PROJ_LAYER_BACKGROUND + CtrlInfo::nLayer; 唯一换算口径)
	static INT32 CtrlLayer(CONST CtrlInfo* pCtrl)
	{
		static BOOL bClampReported = FALSE;		// 越界诊断只报一次(避免逐帧刷屏)
		INT32 nRel = (pCtrl != NULL) ? pCtrl->nLayer : 0;
		INT32 nRet = CtrlLayerOf(nRel);
		if (nRet != (PROJ_LAYER_BACKGROUND + nRel) && bClampReported == FALSE)
		{
			bClampReported = TRUE;
			printf(u8"[LAYER] 层次越界: nLayer=%d ⇒ 层号钳到 %d(层号域 [%d, %d])\n",
				(int)nRel, (int)nRet, (int)PROJ_LAYER_MIN, (int)PROJ_LAYER_MAX);
			fflush(stdout);
		}
		return nRet;
	}

	// 函数: CtrlRel
	// 描述: 取某控件的层次值(相对量; CtrlInfo::nLayer 原样; 有符号)
	static INT32 CtrlRel(CONST CtrlInfo* pCtrl)
	{
		return (pCtrl != NULL) ? pCtrl->nLayer : 0;
	}

	// 函数: ClampProjLayer
	// 描述: 把绘图层号夹到合法域(投影轴表的下标防御; 取色路径的统一钳制口径)
	static inline INT32 ClampProjLayer(INT32 nLayer)
	{
		return (nLayer < PROJ_LAYER_MIN) ? PROJ_LAYER_MIN :
			((nLayer > PROJ_LAYER_MAX) ? PROJ_LAYER_MAX : nLayer);
	}

	// 函数: ProjColor
	// 描述: 取本帧投影配色的槽位色(控件库取色的唯一入口; 投影未注入时返回 0)
	static inline DWORD ProjColor(INT32 nLayer, ProjCR::CR_SLOT eSlot = ProjCR::PROJ_SLOT_SURFACE)
	{
		// 防御钳制: 入参应已是"层号域内"的值(经 CtrlLayerOf), 但绘图路径不容越界读轴 ⇒ 此处再夹一次
		return (pcrHander != NULL) ? pcrHander->Sample((DWORD)ClampProjLayer(nLayer), eSlot) : 0;
	}

	// ================================
	// 区域: 控件取色区(控件层次 + 配色样式 → 成品配色方案)
	// 说明: 部件不接触层号 ⇒ 配色方案一律由本区按控件层次采样后构造, 再经部件的第四固定参数
	//       (Widgets::Base::PaintWay)传入; 统一入口 = MakePalette(层次, 配色样式) —— 各件
	//       只是配色样式的不同取值, 不再各立函数。
	// ================================

	// 枚举: PALETTE_STYLE
	// 描述: 配色样式(整条件相对控件本层的凸凹走向; 层号大者上浮、小者下沉)
	// 说明: 平坦 = 面与段面都取本层; 单向样式 = 面与段面同取本层 ± 一档;
	//       "含"样式 = 主体取一侧一档、段面回到本层(相对主体反向一档) ⇒ 同一条件上凸凹并存。
	enum PALETTE_STYLE
	{
		PALETTE_FLAT = 0,			// 平坦样式配色: 面与段面同取本层
		PALETTE_SUNKEN = 1,			// 下沉样式配色: 面与段面同取本层下沉一档
		PALETTE_RAISED = 2,			// 上浮样式配色: 面与段面同取本层上浮一档
		PALETTE_SUNKEN_RAISED = 3,	// 下沉含上浮样式配色: 面下沉一档, 段面回本层(相对面向上浮一档)
		PALETTE_RAISED_SUNKEN = 4	// 上浮含下沉样式配色: 面上浮一档, 段面回本层(相对面向下沉一档)
	};

	// 类型: PaletteStyleDesc
	// 描述: 配色样式的取色表项(样式 → 两侧面层的层次偏移与外框 / 内框取样口径)
	// 说明: 面层 / 段面层 = 本层层次值 + 偏移(经 CtrlLayerOf 换算并钳制);
	//       外框取样: 偏移非 0 时取"面层与面层 + 偏移"的边界色, 0 = 取与层次无关的边框槽位;
	//       内框取样: TRUE = 取"面层与段面层"的边界色(凸凹并存的分界线)。
	struct PaletteStyleDesc
	{
		PALETTE_STYLE eStyle;			// 样式标识
		LPCSTR lpName;					// 样式名(诊断用)
		INT32 nFaceOffset;				// 面层相对本层的偏移(正 = 上浮)
		INT32 nSurfaceOffset;			// 段面层相对本层的偏移(正 = 上浮)
		INT32 nBorderNeighborOffset;	// 外框取样: 面层与"面层 + 本偏移"的边界(0 = 边框槽位)
		BOOL IsInnerEdge;				// 内框是否取"面层与段面层"的边界色
	};

	// 表: PaletteStyles
	// 描述: 五种配色样式的取色表(顺序与 PALETTE_STYLE 一致)
	static constexpr PaletteStyleDesc PaletteStyles[] = {
		{ PALETTE_FLAT,				u8"平坦",		0,	0,	0,	FALSE },
		{ PALETTE_SUNKEN,			u8"下沉",		-1,	-1,	1,	FALSE },
		{ PALETTE_RAISED,			u8"上浮",		1,	1,	-1,	FALSE },
		{ PALETTE_SUNKEN_RAISED,	u8"下沉含上浮",	-1,	0,	1,	TRUE },
		{ PALETTE_RAISED_SUNKEN,	u8"上浮含下沉",	1,	0,	-1,	TRUE },
	};
	constexpr size_t PALETTE_STYLE_NUM = sizeof(PaletteStyles) / sizeof(PaletteStyles[0]);
	// 样式表完整性(编译期): 五种样式逐一在位, 且"含"样式的两面确实错开
	static_assert(PALETTE_STYLE_NUM == 5, "配色样式表必须覆盖五种样式");
	static_assert(PaletteStyles[PALETTE_FLAT].eStyle == PALETTE_FLAT, "配色样式表: 平坦样式行错位");
	static_assert(PaletteStyles[PALETTE_SUNKEN].eStyle == PALETTE_SUNKEN, "配色样式表: 下沉样式行错位");
	static_assert(PaletteStyles[PALETTE_RAISED].eStyle == PALETTE_RAISED, "配色样式表: 上浮样式行错位");
	static_assert(PaletteStyles[PALETTE_SUNKEN_RAISED].eStyle == PALETTE_SUNKEN_RAISED, "配色样式表: 下沉含上浮样式行错位");
	static_assert(PaletteStyles[PALETTE_RAISED_SUNKEN].eStyle == PALETTE_RAISED_SUNKEN, "配色样式表: 上浮含下沉样式行错位");
	static_assert(PaletteStyles[PALETTE_SUNKEN_RAISED].nFaceOffset != PaletteStyles[PALETTE_SUNKEN_RAISED].nSurfaceOffset,
		"配色样式表: 下沉含上浮样式必须两层面错开");
	static_assert(PaletteStyles[PALETTE_RAISED_SUNKEN].nFaceOffset != PaletteStyles[PALETTE_RAISED_SUNKEN].nSurfaceOffset,
		"配色样式表: 上浮含下沉样式必须两层面错开");

	// 函数: PaletteStyleOf
	// 描述: 取配色样式的表项(样式值未知 ⇒ 回退平坦样式, 取色路径不因样式越界而失败)
	static CONST PaletteStyleDesc& PaletteStyleOf(PALETTE_STYLE eStyle)
	{
		for (size_t i = 0; i < PALETTE_STYLE_NUM; i++)
		{
			if (PaletteStyles[i].eStyle == eStyle)
			{
				return PaletteStyles[i];
			}
		}
		return PaletteStyles[0];
	}

	// 函数: MakePalette
	// 描述: 构造部件配色方案(各绘制件共用的统一入口; 按控件层次与配色样式采样)
	static Widgets::Base::Palette MakePalette(INT32 nRel, PALETTE_STYLE eStyle)
	{
		CONST PaletteStyleDesc& style = PaletteStyleOf(eStyle);
		CONST INT32 nFaceLayer = CtrlLayerOf(nRel + style.nFaceOffset);			// 面层(整条底)
		CONST INT32 nSurfaceLayer = CtrlLayerOf(nRel + style.nSurfaceOffset);		// 段面层(右段 / 按钮)
		CONST INT32 nBorderNearLayer = ClampProjLayer(nFaceLayer + style.nBorderNeighborOffset);	// 外框取样邻层(面层 ± 一档)
		CONST DWORD crFaceContrast = ProjColor(nFaceLayer, ProjCR::PROJ_SLOT_CONTRAST);		// 面层对比槽(面内前景)
		CONST DWORD crSurfaceContrast = ProjColor(nSurfaceLayer, ProjCR::PROJ_SLOT_CONTRAST);	// 段面层对比槽(段面前景)
		CONST DWORD crDisabled = ProjColor(PROJ_L_DISABLED);						// 禁用 = 中灰(与层次无关的绝对层号)
		Widgets::Base::Palette palette{};
		// 1. 面(整条底): 三态走投影槽位; 前景取面层对比槽
		palette.crBackground.crBase = ProjColor(nFaceLayer);
		palette.crBackground.crHovered = ProjColor(nFaceLayer, ProjCR::PROJ_SLOT_HOVER);
		palette.crBackground.crActive = ProjColor(nFaceLayer, ProjCR::PROJ_SLOT_ACTIVE);
		palette.crBackground.crDisable = crDisabled;
		palette.crForeground.crBase = crFaceContrast;
		palette.crForeground.crHovered = crFaceContrast;
		palette.crForeground.crActive = crFaceContrast;
		palette.crForeground.crDisable = crDisabled;
		// 2. 段面(两段件的右段 / 按钮): 层次由样式表给出; 前景取段面层对比槽
		palette.crSurface.crBase = ProjColor(nSurfaceLayer);
		palette.crSurface.crHovered = ProjColor(nSurfaceLayer, ProjCR::PROJ_SLOT_HOVER);
		palette.crSurface.crActive = ProjColor(nSurfaceLayer, ProjCR::PROJ_SLOT_ACTIVE);
		palette.crSurface.crDisable = crDisabled;
		palette.crSurfaceForeground.crBase = crSurfaceContrast;
		palette.crSurfaceForeground.crHovered = crSurfaceContrast;
		palette.crSurfaceForeground.crActive = crSurfaceContrast;
		palette.crSurfaceForeground.crDisable = crDisabled;
		// 3. 外框: 平坦样式取边框三级槽位(与层次无关); 其余样式取面层与相邻层的边界色
		if (style.nBorderNeighborOffset == 0)
		{
			palette.crBorder.crBase = ProjColor(PROJ_L_BORDER);
			palette.crBorder.crHovered = ProjColor(PROJ_L_BORDER_HOVER);
			palette.crBorder.crActive = ProjColor(PROJ_L_BORDER_ACTIVE);
			palette.crBorder.crDisable = crDisabled;
			// 内框: 平坦样式的两层面同层, 无凸凹分界线 ⇒ 不画(与改前的单行文本件一致)
			return palette;
		}
		// 边界色由投影的"边框取样"给出; 投影未注入时不着色(与 ProjColor 同防御口径)
		palette.crBorder.crBase = (pcrHander != NULL) ? pcrHander->SampleBorder((DWORD)nFaceLayer, (DWORD)nBorderNearLayer) : 0;
		palette.crBorder.crHovered = ProjColor(PROJ_L_BORDER_HOVER);
		palette.crBorder.crActive = ProjColor(PROJ_L_BORDER_ACTIVE);
		palette.crBorder.crDisable = ProjColor(nFaceLayer);
		// 4. 内框: "含"样式取面层与段面层的边界色; 其余样式常态不画(与改前列表行件一致)
		if (style.IsInnerEdge != FALSE)
		{
			palette.crBorderInner.crBase = (pcrHander != NULL) ? pcrHander->SampleBorder((DWORD)nFaceLayer, (DWORD)nSurfaceLayer) : 0;
		}
		palette.crBorderInner.crHovered = ProjColor(PROJ_L_BORDER_HOVER);
		palette.crBorderInner.crActive = ProjColor(PROJ_L_BORDER_ACTIVE);
		palette.crBorderInner.crDisable = ProjColor(nFaceLayer);
		return palette;
	}

	// ================================
	// 区域: 控件绘制前向声明区
	// ================================
		// 函数: TitleBar
		// 描述: 标题控件: 绘制标题文本与右上关闭钮
	VOID TitleBar(CtrlInfo* info);
	// 函数: InputReal
	// 描述: 实数输入框
	VOID InputReal(CtrlInfo* info);
	// 函数: InputNumeric
	// 描述: 整数数字输入框(仅限整数; CTRL_TYPE_INPUT_NUMERIC 渲染)
	VOID InputNumeric(CtrlInfo* info);
	// 函数: InputAction
	// 描述: 输入动作控件: 无边框单行输入 + 右侧按钮(绘制经部件 Widgets::Base::InputAction)
	VOID InputAction(CtrlInfo* info);
	// 函数: Slider
	// 描述: 单手柄滑动条(CTRL_TYPE_SLIDER): 一个值 + 一个值域
	//       (手柄由两枚减为一枚)
	VOID Slider(CtrlInfo* info);
	// 函数: Progress
	// 描述: 进度状态显示
	VOID Progress(CtrlInfo* info);
	// 函数: RenderSurface
	// 描述: 渲染面控件(CTRL_TYPE_RENDER): 有纹理时按控件矩形等比居中绘制, 无纹理时画"占位面"
	//       纹理视图与尺寸取控件缓冲(CtrlRenderData; 由宿主注入 —— 控件库不持设备、不管纹理生命周期)
	VOID RenderSurface(CtrlInfo* info);
	// 函数: CheckBox
	// 描述: 复选框控件(CTRL_TYPE_CHECKBOX): 方框 / 勾 / 文字由 ImGui 原生复选框绘制
	//       ①缓冲 = 勾选态 + 显示文字(CtrlCheckData; 见 CtrlBuffer.h);
	//       ②命中区 = 控件矩形(整行可点, 不限于方框); 命中切换勾选态后落专属段 OPER_EDIT_CHANGED
	//         (与 COMBO / SLIDER 的"值已改变"同口径, 不额外派发公共码 CLICKED);
	//       ③配色随控件层次 —— 方框面取本控件层次投影色, 勾 / 文字取该层对比槽, 描边取边框槽位。
	VOID CheckBox(CtrlInfo* info);
	// 函数: Combo
	// 描述: 下拉框
	VOID Combo(CtrlInfo* info);
	// 函数: ComboAction
	// 描述: 下拉动作控件: 无边框下拉框 + 右侧按钮(绘制经部件 Widgets::Base::ComboAction)
	VOID ComboAction(CtrlInfo* info);
	// 函数: TimeBar
	// 描述: 可编辑时间条控件(CTRL_TYPE_TIMEBAR): 提示文本 + 时/分/秒/毫秒四段数字输入;
	//       数据取控件缓冲(CtrlTimeBarData), 提交时派发专属段编辑提交码
	VOID TimeBar(CtrlInfo* info);
	// 函数: SliderRange
	// 描述: 双手柄范围滑块控件(SLIDER_RANGE): 左/右手柄分别派发专属段与编辑变更码
	VOID SliderRange(CtrlInfo* info);
	// 函数: ListTimeSpan
	// 描述: 时间片列表控件(LIST_TIMESPAN): 逐行显示起止时码, 行内删除钮与行选中经操作码回传
	VOID ListTimeSpan(CtrlInfo* info);

	// 函数: ListInput
	// 描述: 布局列表控件(LIST_INPUT)的绘制入口: 外部一次性送入(单参数), 控件自行拆解任务 ——
	//       解析该类型缓冲(ListBuffer 头 + INPUT_ACTION 行块)后逐行送入列表管道部件(Widgets::Pipe::ListRow);
	//       项目行 = 可编辑输入动作行件(Widgets::Base::InputAction), 特殊行(新增行)同件
	VOID ListInput(CtrlInfo* info);
	// 函数: ListCombo
	// 描述: 控件列表控件(LIST_COMBO)的绘制入口: 同构 —— 外部一次性送入 + 控件自行拆解, 行块同为 INPUT_ACTION 块(项目行只读);
	//       项目行 = 只读输入动作行件(Widgets::Base::InputActionRO), 特殊行(新增行) = 下拉动作行件
	//       (Widgets::Base::ComboAction, 特殊行块按 CtrlComboData 解释)
	VOID ListCombo(CtrlInfo* info);

	// 类型: pfnGenericCtrlDraw
	// 描述: 控件绘制分派表的函数指针(类型 → 绘制实现; 表见后方"控件绘制分派表")
	using pfnGenericCtrlDraw = VOID(*)(CtrlInfo* info);

	// ================================
	// 区域: 控件绘制分派表
	// 描述: 控件绘制分派表(控件类型 → 绘制实现), 供 Controls::DrawCtrl 查表。
	// ================================

	// 函数: Button
	// 描述: 按钮: 绘制经同名部件 Widgets::Base::Button(三态面 + 面内文字), 点击时调用该控件的事件处理器
	//       (pEventHandler); 操作码 = 公共码 OPER_COMMON_CLICKED
	VOID Button(CtrlInfo* info)
	{
		LPCSTR lpText = (LPCSTR)info->bufferInfo.buffer;
		// 绘制 + 取回部件结论(部件 → 调用方 返回约定; 0 = 无操作)
		const int64_t qwPart = Widgets::Base::Button(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT)), lpText);
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);	// bit[31:16]: 公共位图
		// 独立使用 ⇒ 公共码落公共段(与改前直调按钮同口径)
		if (wCommon != OPER_COMMON_NONE && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCodeCommon(wCommon), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: Text
	// 描述: 静态文本: 文本图元经同名部件 Widgets::Base::Text 绘制; 仅当绑定事件(pEventHandler != NULL)时
	//       在图元之后补一枚覆盖控件矩形的无绘制可交互项 ⇒ 可响应单击/右键/双击(操作码 = 公共码 OPER_COMMON_*)
	VOID Text(CtrlInfo* info)
	{
		LPCSTR text = (LPCSTR)info->bufferInfo.buffer;
		// 文本图元(经同名部件; 部件不接触层号 ⇒ 配色方案由控件层按本控件层次构造)
		// 对齐: 部件一律在控件矩形内垂直居中(容器高 = 控件声明高; 内容高 = 设计字号); 横向自矩形左缘起排
		Widgets::Base::Text(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT)), text);
		// 图元绘制之后: 光标移回控件矩形原点, 再覆盖可交互项(仅绑定事件时)
		if (info->pEventHandler != NULL)
		{
			const uint16_t wCommon = CtrlClickCode(info, u8"##TEXT_ITEM");
			if (wCommon != OPER_COMMON_NONE)
			{
				info->pEventHandler(info, OperCode::MakeCodeCommon(wCommon), 0, info->bufferInfo.buffer);
			}
		}
	}
	// 函数: Rect
	// 描述: RECT 控件: 圆角底面 + 圆角描边(色值全取投影), 无文本/无子内容 —— 绘制矩形 = 控件声明矩形;
	//       仅当绑定事件(pEventHandler != NULL)时补一枚覆盖控件矩形的无绘制可交互项 ⇒ 可响应单击/右键/双击
	VOID Rect(CtrlInfo* info)
	{
		// 变量声明区
		ImVec2 o{};							// 控件矩形原点(屏幕坐标)
		CONST INT32 nLayer = CtrlLayer(info);	// 绘图层号(控件自身层次 ⇒ 层号)
		// 1. 定层: 控件矩形(屏幕坐标; 原点由分派入口 SetCursorPos 定位)
		o = ImGui::GetCursorScreenPos();
		// 2. 画底面与描边(取色 = 投影; 描边取边框常态层)
		DrawRectFace(ImGui::GetWindowDrawList(), o, toImvec2(info->size),
			ProjColor(nLayer), ProjColor(PROJ_L_BORDER));
		// 3. 图元绘制之后: 光标移回控件矩形原点, 再覆盖可交互项(仅绑定事件时)
		if (info->pEventHandler != NULL)
		{
			const uint16_t wCommon = CtrlClickCode(info, u8"##RECT_ITEM");
			if (wCommon != OPER_COMMON_NONE)
			{
				info->pEventHandler(info, OperCode::MakeCodeCommon(wCommon), 0, info->bufferInfo.buffer);
			}
		}
	}
	// 函数: InputSingle
	// 描述: 单行输入控件(INPUT_SINGLE): 绘制经部件 Widgets::Base::InputText(底与边框由 ImGui 原生框绘制,
	//       内边距按声明高对齐); 部件结论"独立使用" ⇒ 落专属段(编辑结束提交 = OPER_EDIT_SUBMIT)
	VOID InputSingle(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		// 文本区 = 首址 + 容量, 取自 CtrlBuffer.h 的 CtrlTextBuffer / CtrlTextCapacity;
		// 无专用缓冲 ⇒ 只读占位(首址有效 + 容量 0)
		CHAR* lpText = CtrlTextBuffer(info);
		size_t nTextCap = CtrlTextCapacity(info);
		if (lpText == NULL || nTextCap == 0)
		{
			lpText = ERRBUFFER;
			nTextCap = 0;
		}
		// 绘制 + 取回部件结论(部件 → 调用方 返回约定; 0 = 无操作)
		// 配色 = 下沉样式: 输入域相对所在层次凹陷一档(试行调用点)
		const int64_t qwPart = Widgets::Base::InputText(info->id, toImvec2(info->position),
			toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)), lpText, nTextCap, NULL);

		// 独立使用 ⇒ 放专属段: 取四位段的高低两段, 搭载段恒 0
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);				// bit[15:0]: 专属段码
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);	// bit[31:16]: 公共位图
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: InputMulti
	// 描述: 多行文本输入控件(INPUT_MULTI): 长文本(提示词 / 负面提示词 / 参数文件内容)
	//       绘制经部件 Widgets::Base::InputTextMulti(底与边框由 ImGui 原生框绘制; 多行项尺寸 = 声明矩形);
	//       缓冲 = 文本类缓冲(首址即 C 串; 容量随缓冲头走 —— 装配期按 CtrlBuffer.h 的
	//       CTRL_TEXT_MULTI_BUF 分配, 见 CtrlBuffer::PrepareCtrlBuffer 的 INPUT_MULTI 分支);
	//       行内回车 = 换行(不提交), 提交 = 失焦且有改动 ⇒ 专属段 OPER_EDIT_SUBMIT(与单行件同口径)。
	VOID InputMulti(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		// 文本区 = 首址 + 容量, 取自 CtrlBuffer.h 的 CtrlTextBuffer / CtrlTextCapacity;
		// 无专用缓冲 ⇒ 只读占位(首址有效 + 容量 0)
		CHAR* lpText = CtrlTextBuffer(info);
		size_t nTextCap = CtrlTextCapacity(info);
		if (lpText == NULL || nTextCap == 0)
		{
			lpText = ERRBUFFER;
			nTextCap = 0;
		}
		// 绘制 + 取回部件结论(部件 → 调用方 返回约定; 0 = 无操作)
		// 配色 = 下沉样式(与单行输入同口径)
		const int64_t qwPart = Widgets::Base::InputTextMulti(info->id, toImvec2(info->position),
			toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)), lpText, nTextCap, NULL);
		// 独立使用 ⇒ 放专属段: 取四位段的高低两段, 搭载段恒 0
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);				// bit[15:0]: 专属段码
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);	// bit[31:16]: 公共位图
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: ListInput
	// 描述: 布局列表控件(LIST_INPUT)的绘制入口 —— 外部(渲染层经 DrawCtrl)一次性送入(单参数);
	//       本控件自行拆解任务: 解析该类型缓冲(ListBuffer 头 + [特殊行块] + [项目块 × 容量]),
	//       再逐行送入列表管道部件(Widgets::Pipe::ListRow); 行游标 / 几何递增 / oper 累积由本控件维护,
	//       列表子窗的容器状态由管道部件按稳定 ID 复用
	VOID ListInput(CtrlInfo* info)
	{
		// 变量声明区(业务状态: 行游标 / 几何递增 / oper 累积 —— 管道部件无状态, 均归本控件 = 管道部件的调用方)
		int64_t qwOper = 0;						// 本帧收集到的操作码(最后一个非 0 值; 0 = 无码不回调)
		uint64_t qwParam = 0;					// 交回调的 param(来源行号; 拖拽重排 = 源行号)
		DWORD dwCount = 0;						// 项目数量(无数据首址时按 0 处理)
		DWORD dwSpecialFlag = 0;				// 特殊行标志(LOW16 = 位置; HIGH16 = 高度(像素))
		BOOL bSpecialFirst = FALSE;				// 特殊行是否在项目行之前
		FLOAT fGap = 0.0f;						// 行间距
		FLOAT fStep = 0.0f;						// 递增步长(纵向)
		FLOAT fSpecialH = 0.0f;					// 特殊行高度
		size_t cbItem = 0;						// 单一项目数据长度(步进)
		VOID* lpSpecial = NULL;					// 特殊行数据首址
		VOID* lpItemBase = NULL;				// 项目数据首址
		vec2 posCur{};							// 当前项位置(列表子窗本地坐标)
		ListBuffer* pList = NULL;
		Widgets::Pipe::ListPipe pipe{};
		Widgets::Pipe::PaintWay way{};
		if (info == NULL)
		{
			return;
		}
		pList = ListBufferOf(info);
		// 列表管道形式(每次送入均携带; 部件不持有)
		pipe.id = info->id;
		pipe.pos = info->position;
		pipe.size = info->size;
		pipe.label = info->label ? info->label : u8"##LISTPANEL";
		pipe.fRounding = fRounding;
		pipe.fRowH = sizeinfo.controlHeight;		// 行高 = 控件高
		pipe.crShell = ProjColor(CtrlLayerOf(CtrlRel(info)));	// 列表壳体底色(控件层采样 ⇒ 部件不接触层号)
		way = Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_RAISED));	// 行件配色方案(按控件层次 + 上浮样式构造)
		// 无缓冲: 与原有列表路径等价 —— 仍维护列表本身(只画列表, 无行)
		if (pList == NULL)
		{
			Widgets::Pipe::ListRow(pipe, 0, vec2(0.0f, 0.0f), pipe.fRowH, NULL, NULL, way);
			return;
		}
		cbItem = (size_t)pList->cbItem;
		dwCount = (pList->dwCount <= pList->dwCapacity) ? pList->dwCount : pList->dwCapacity;
		pipe.dwCount = dwCount;
		// 行数据区(该类型的解析约定): [特殊行块 cbItem] + [项目块 cbItem × 容量]
		lpSpecial = (VOID*)(pList + 1);
		lpItemBase = (VOID*)((BYTE*)(pList + 1) + cbItem);
		// 特殊行 = 新增行(INPUT_ACTION 件; 位置 = 最后, 高度 = 行高)
		dwSpecialFlag = LIST_SPECIAL_LAST | ((DWORD)(sizeinfo.controlHeight) << 16);
		fSpecialH = (FLOAT)((dwSpecialFlag >> 16) & 0xFFFF);
		bSpecialFirst = ((dwSpecialFlag & 0xFFFF) == LIST_SPECIAL_FIRST) ? TRUE : FALSE;
		fGap = sizeinfo.space;
		fStep = pipe.fRowH + fGap;
		posCur = vec2(sizeinfo.space, sizeinfo.space);
		// 特殊行: 位置 = 在项目行之前(类型约定为 LAST ⇒ 本分支当前不触发; 与原有列表路径同构保留)
		if (bSpecialFirst)
		{
			// 管理 oper: 收集(取最后一个非 0 值); 行件结论被携带 ⇒ 在此折算为 oper(搭载段) + param(行号)
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, 0, posCur, fSpecialH, lpSpecial,
				Widgets::Base::InputAction, way), 0, qwOper, qwParam);
			posCur.y += fSpecialH + fGap;
		}
		// 项目行: 逐行送入管道(行件 = 可编辑输入动作行)
		for (DWORD i = 0; i < dwCount; i++)
		{
			VOID* lpItem = (VOID*)((BYTE*)lpItemBase + cbItem * i);
			// 管理 oper: 收集(取最后一个非 0 值); 行件结论被携带 ⇒ 在此折算为 oper(搭载段) + param(行号)
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, (int)i, posCur, pipe.fRowH, lpItem,
				Widgets::Base::InputAction, way), i, qwOper, qwParam);
			posCur.y += fStep;
		}
		// 特殊行: 位置 = 在项目行之后
		if (bSpecialFirst == FALSE)
		{
			// 管理 oper: 收集(取最后一个非 0 值); 行件结论被携带 ⇒ 在此折算为 oper(搭载段) + param(行号)
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, (int)dwCount, posCur, fSpecialH, lpSpecial,
				Widgets::Base::InputAction, way), dwCount, qwOper, qwParam);
			posCur.y += fSpecialH + fGap;
		}
		// 回调: oper = 行件结论(被携带 ⇒ 搭载段 = 行内部件码 / 重排 = 专属段 + 搭载段目标位置),
		// param = 来源行号(重排 = 源行号); 与事件回调四元组约定一致
		if (qwOper != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, (uint64_t)qwOper, qwParam, info->bufferInfo.buffer);
		}
	}
	// 函数: ListCombo
	// 描述: 控件列表控件(LIST_COMBO)的绘制入口 —— 同构: 外部一次性送入(单参数), 控件自行拆解任务;
	//       行块长度 = 头部 cbItem(项目行 = INPUT_ACTION 块), 逐行送入列表管道部件(Widgets::Pipe::ListRow),
	//       业务状态归本控件维护
	VOID ListCombo(CtrlInfo* info)
	{
		// 变量声明区(业务状态: 行游标 / 几何递增 / oper 累积 —— 管道部件无状态, 均归本控件 = 管道部件的调用方)
		int64_t qwOper = 0;						// 本帧收集到的操作码(最后一个非 0 值; 0 = 无码不回调)
		uint64_t qwParam = 0;					// 交回调的 param(来源行号; 拖拽重排 = 源行号)
		DWORD dwCount = 0;						// 项目数量(无数据首址时按 0 处理)
		DWORD dwSpecialFlag = 0;				// 特殊行标志(LOW16 = 位置; HIGH16 = 高度(像素))
		BOOL bSpecialFirst = FALSE;				// 特殊行是否在项目行之前
		FLOAT fGap = 0.0f;						// 行间距
		FLOAT fStep = 0.0f;						// 递增步长(纵向)
		FLOAT fSpecialH = 0.0f;					// 特殊行高度
		size_t cbItem = 0;						// 单一项目数据长度(步进)
		VOID* lpSpecial = NULL;					// 特殊行数据首址
		VOID* lpItemBase = NULL;				// 项目数据首址
		vec2 posCur{};							// 当前项位置(列表子窗本地坐标)
		ListBuffer* pList = NULL;
		Widgets::Pipe::ListPipe pipe{};
		Widgets::Pipe::PaintWay way{};
		if (info == NULL)
		{
			return;
		}
		pList = ListBufferOf(info);
		// 列表管道形式(每次送入均携带; 部件不持有)
		pipe.id = info->id;
		pipe.pos = info->position;
		pipe.size = info->size;
		pipe.label = info->label ? info->label : u8"##LISTPANEL";
		pipe.fRounding = fRounding;
		pipe.fRowH = sizeinfo.controlHeight;		// 行高 = 控件高
		pipe.crShell = ProjColor(CtrlLayerOf(CtrlRel(info)));	// 列表壳体底色(控件层采样 ⇒ 部件不接触层号)
		way = Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_RAISED));	// 行件配色方案(按控件层次 + 上浮样式构造)
		// 无缓冲: 与原有列表路径等价 —— 仍维护列表本身(只画列表, 无行)
		if (pList == NULL)
		{
			Widgets::Pipe::ListRow(pipe, 0, vec2(0.0f, 0.0f), pipe.fRowH, NULL, NULL, way);
			return;
		}
		cbItem = (size_t)pList->cbItem;
		dwCount = (pList->dwCount <= pList->dwCapacity) ? pList->dwCount : pList->dwCapacity;
		pipe.dwCount = dwCount;
		// 行数据区(该类型的解析约定): [特殊行块 cbItem] + [项目块 cbItem × 容量]
		lpSpecial = (VOID*)(pList + 1);
		lpItemBase = (VOID*)((BYTE*)(pList + 1) + cbItem);
		// 特殊行 = 新增行(下拉动作件; 位置 = 最后, 高度 = 行高)
		dwSpecialFlag = LIST_SPECIAL_LAST | ((DWORD)(sizeinfo.controlHeight) << 16);
		fSpecialH = (FLOAT)((dwSpecialFlag >> 16) & 0xFFFF);
		bSpecialFirst = ((dwSpecialFlag & 0xFFFF) == LIST_SPECIAL_FIRST) ? TRUE : FALSE;
		fGap = sizeinfo.space;
		fStep = pipe.fRowH + fGap;
		posCur = vec2(sizeinfo.space, sizeinfo.space);
		// 特殊行: 位置 = 在项目行之前(类型约定为 LAST ⇒ 本分支当前不触发; 与原有列表路径同构保留)
		if (bSpecialFirst)
		{
			// 管理 oper: 收集(取最后一个非 0 值); 行件结论被携带 ⇒ 在此折算为 oper(搭载段) + param(行号)
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, 0, posCur, fSpecialH, lpSpecial,
				Widgets::Base::ComboAction, way), 0, qwOper, qwParam);
			posCur.y += fSpecialH + fGap;
		}
		// 项目行: 逐行送入管道(行件 = 只读输入动作行)
		for (DWORD i = 0; i < dwCount; i++)
		{
			VOID* lpItem = (VOID*)((BYTE*)lpItemBase + cbItem * i);
			// 管理 oper: 收集(取最后一个非 0 值); 行件结论被携带 ⇒ 在此折算为 oper(搭载段) + param(行号)
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, (int)i, posCur, pipe.fRowH, lpItem,
				Widgets::Base::InputActionRO, way), i, qwOper, qwParam);
			posCur.y += fStep;
		}
		// 特殊行: 位置 = 在项目行之后
		if (bSpecialFirst == FALSE)
		{
			// 管理 oper: 收集(取最后一个非 0 值); 行件结论被携带 ⇒ 在此折算为 oper(搭载段) + param(行号)
			ListCollectRowOper(Widgets::Pipe::ListRow(pipe, (int)dwCount, posCur, fSpecialH, lpSpecial,
				Widgets::Base::ComboAction, way), dwCount, qwOper, qwParam);
			posCur.y += fSpecialH + fGap;
		}
		// 回调: oper = 行件结论(被携带 ⇒ 搭载段 = 行内部件码 / 重排 = 专属段 + 搭载段目标位置),
		// param = 来源行号(重排 = 源行号); 与事件回调四元组约定一致
		if (qwOper != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, (uint64_t)qwOper, qwParam, info->bufferInfo.buffer);
		}
	}

	// 函数: ListRowDropCalc
	// 描述: 列表行拖拽落点 → 行件返回约定的纯算式(与 ListRowDropOper 同一真源; 常量表达式,
	//       供编译期不变量校验复用)
	static constexpr int64_t ListRowDropCalc(DWORD dwSrc, DWORD dwRow, BOOL IsAfter)
	{
		CONST DWORD dwDest = (IsAfter != FALSE) ? (dwRow + 1) : dwRow;	// 目标插入位置(本行之前 / 本行之后)
		// 字段显式掩到 16 位: 任一段越界都不会进位污染相邻字段(行号/位置本身远小于 16 位)
		return (int64_t)((uint64_t)OPER_LIST_ROW_MOVED |
			((uint64_t)(dwDest & 0xFFFF) << 32) |
			((uint64_t)(dwSrc & 0xFFFF) << 48));
	}
	// 函数: ListRowDropOper
	// 描述: 列表行拖拽落点 → 行件返回约定(纯换算, 不触碰 ImGui; 绘制侧唯一真源)
	int64_t ListRowDropOper(DWORD dwSrc, DWORD dwRow, BOOL IsAfter)
	{
		return ListRowDropCalc(dwSrc, dwRow, IsAfter);
	}
	// 操作码层不变量(编译期校验; 供"落点换算真源下沉到控件层"后的回归守卫 —— 应用侧已无法复核):
	// ①专属段码 = OPER_LIST_ROW_MOVED; ②目标插入位置只由"放置目标行 + 上/下半"决定(下半 = 行号 + 1),
	// 与源行号无关; ③源行号只落在 bit[63:48]; ④公共段恒 0(无公共码)
	static_assert((uint16_t)((uint64_t)ListRowDropCalc(5, 2, FALSE) & 0xFFFF) == OPER_LIST_ROW_MOVED, "列表重排: 专属段码错");
	static_assert((uint32_t)(((uint64_t)ListRowDropCalc(5, 2, FALSE) >> 32) & 0xFFFF) == 2, "列表重排: 上半落点应为该行之前");
	static_assert((uint32_t)(((uint64_t)ListRowDropCalc(5, 2, TRUE) >> 32) & 0xFFFF) == 3, "列表重排: 下半落点应为该行之后");
	static_assert((uint32_t)(((uint64_t)ListRowDropCalc(5, 2, FALSE) >> 48) & 0xFFFF) == 5, "列表重排: 源行号应落 bit[63:48]");
	static_assert((uint32_t)(((uint64_t)ListRowDropCalc(99, 2, TRUE) >> 32) & 0xFFFF) ==
		(uint32_t)(((uint64_t)ListRowDropCalc(5, 2, TRUE) >> 32) & 0xFFFF), "列表重排: 落点不得随源行号变化");
	// 公共位图在 bit[31:16](行件返回约定第 2 字段); bit[15:0] 是专属段码 ⇒ 两者不得混判
	static_assert((uint16_t)(((uint64_t)ListRowDropCalc(5, 2, TRUE) >> 16) & 0xFFFF) == OPER_COMMON_NONE,
		"列表重排: 公共位图必须为 0(无公共码)");

	// 结构: GenericCtrlDrawEntry
	// 描述: 控件绘制分派表项(类型 → 绘制实现)
	struct GenericCtrlDrawEntry
	{
		CtrlType type;					// 控件类型
		pfnGenericCtrlDraw pfnDraw;		// 绘制实现
	};

	// 表: GenericCtrlDraws
	// 描述: 控件绘制分派表(控件类型 → 绘制实现): 在表内 = 有绘制实现, 未列入者 = 无绘制实现
	static const GenericCtrlDrawEntry GenericCtrlDraws[] = {
		{ CTRL_TYPE_BUTTON,			Button },
		{ CTRL_TYPE_TEXT,			Text },
		{ CTRL_TYPE_INPUT_SINGLE,	InputSingle },
		{ CTRL_TYPE_INPUT_MULTI,	InputMulti },	// 多行输入(长文本; 缓冲按 CTRL_TEXT_MULTI_BUF 分配)
		{ CTRL_TYPE_INPUT_REAL,		InputReal },
		{ CTRL_TYPE_INPUT_NUMERIC,	InputNumeric },
		{ CTRL_TYPE_INPUT_ACTION,	InputAction },	// 输入动作(无边框单行输入 + 右侧按钮; 部件 Widgets::Base::InputAction)
		{ CTRL_TYPE_SLIDER,			Slider },
		{ CTRL_TYPE_PROG,			Progress },
		{ CTRL_TYPE_RENDER,			RenderSurface },	// 渲染面(图像显示; 纹理由宿主注入, 无图时占位面)
		{ CTRL_TYPE_CHECKBOX,		CheckBox },		// 复选框(原生复选框; 显示文字取控件缓冲)
		{ CTRL_TYPE_COMBO,			Combo },
		{ CTRL_TYPE_COMBO_ACTION,	ComboAction },	// 下拉动作(无边框下拉框 + 右侧按钮; 部件 Widgets::Base::ComboAction)
		{ CTRL_TYPE_TIMEBAR,		TimeBar },		// 可编辑时间条(提示文本 + 四段时码输入; 部件 Widgets::Base::TimeBar)
		{ CTRL_TYPE_SLIDER_RANGE,	SliderRange },	// 双手柄范围滑块(部件 Widgets::Base::RangeSlider)
		{ CTRL_TYPE_LIST_TIMESPAN,	ListTimeSpan },	// 时间片列表(行块 = TimeSpanItem; 行件 TextAction)
		{ CTRL_TYPE_RECT,			Rect },
		{ CTRL_TYPE_TITLE,			TitleBar },		// 标题条(结构件标题走专用入口; 分派表路径与其共用同一实现)
		{ CTRL_TYPE_LIST_INPUT,		ListInput },	// 布局列表(行 = INPUT_ACTION 块)
		{ CTRL_TYPE_LIST_COMBO,		ListCombo },	// 控件列表(项目行 = 只读输入动作行块; 特殊行 = 下拉动作块)
	};
	constexpr size_t GENERIC_CTRL_DRAW_NUM = sizeof(GenericCtrlDraws) / sizeof(GenericCtrlDraws[0]);

	// 函数: GenericCtrlDrawOf
	// 描述: 按类型查控件绘制实现(查上方分派表)
	static Controls::pfnGenericCtrlDraw GenericCtrlDrawOf(CtrlType type)
	{
		for (size_t i = 0; i < GENERIC_CTRL_DRAW_NUM; i++)
		{
			if (GenericCtrlDraws[i].type == type)
			{
				return GenericCtrlDraws[i].pfnDraw;
			}
		}
		return NULL;
	}

	// 函数: DrawCtrl
	// 描述: 按控件类型分派绘制实现(逐控件绘制入口): 换算绘制坐标 + 定位控件矩形, 再按类型调绘制实现
	BOOL DrawCtrl(CtrlInfo* ctrl)
	{
		const BOOL bUnknownRect = (ctrl->type == CTRL_TYPE_UNKNOWN) ? TRUE : FALSE;
		const BOOL bUnusedSlot = (ctrl->type == CTRL_TYPE_INVALID) ? TRUE : FALSE;
		const pfnGenericCtrlDraw pfnDraw = bUnknownRect ? NULL : GenericCtrlDrawOf(ctrl->type);
		// 结构件不走本入口的换算: 它们的坐标已是窗口本地坐标(WND 为桌面位置, TITLE / CNTR 由布局编排消费)
		const BOOL bShell = (ctrl->type == CTRL_TYPE_WND || ctrl->type == CTRL_TYPE_TITLE ||
			ctrl->type == CTRL_TYPE_CNTR) ? TRUE : FALSE;
		// 绘制坐标 = 有效坐标(窗口本地: 已含父链累计偏移) - 内容容器原点 ——
		// 说明: 内容容器(由 LayoutRender 按 CNTR 件开启)已提供裁剪与原点偏移 ⇒ 控件库按容器本地坐标绘制,
		//       不得把父链累加再叠加一次; 逐类型实现一律从 CtrlInfo::position 取原点 ⇒ 进入分派前把该字段
		//       临时换算为容器本地坐标, 分派结束还原为有效坐标(声明坐标始终保留在 posDeclared)
		const vec2 posEffective = ctrl->position;
		if (bShell == FALSE)
		{
			ctrl->position = posEffective - orgContent;
		}
		// 控件矩形定位(光标起点 = 矩形左上角)
		ImGui::SetCursorPos(toImvec2(ctrl->position));
		// 控件实现统一签名 VOID(CtrlInfo*) —— 矩形原点已定位于光标处
		if (bUnknownRect)
		{
			DrawUnknownCtrlRect(ctrl);
		}
		else if (bUnusedSlot)
		{
			ImGui::Dummy(toImvec2(ctrl->size));
		}
		else if (pfnDraw != NULL)
		{
			pfnDraw(ctrl);
		}
		else
		{
			// 已知类型但无绘制实现(如 LIST / LIST_TIMESPAN): 零像素, 但提交一枚不可见占位项占住区域
			// (不提交项时 ImGui 会因 SetCursorPos 扩展窗口/父级边界而逐帧报错; 与"未使用槽位"分支同法)
			ImGui::Dummy(toImvec2(ctrl->size));
		}
		// 还原有效坐标(绘制期的容器本地坐标只在本次分派内有效)
		if (bShell == FALSE)
		{
			ctrl->position = posEffective;
		}
		return (bUnknownRect || pfnDraw != NULL) ? TRUE : FALSE;
	}

	// 函数: UpdateContext
	// 描述: 更新绘制上下文(每帧渲染布局之前调用一次)
	BOOL UpdateContext(CONST AutoSize& refsize, CONST ProjCR* pProj, CONST vec2* pWndPos, CONST vec2& posContent)
	{
		// 部件层上下文(参考尺寸 / 投影配色 / 编辑中计数)转调一次 ⇒ 每帧单一写点
		Widgets::UpdateContext(refsize, pProj);
		sizeinfo = refsize.GetSizeInfo<FLOAT>();
		fRounding = sizeinfo.padding;
		// 全局样式: WindowPadding = (0,0); FramePadding = 设计内边距(横纵同值), 像素值取自本帧参考尺寸;
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowPadding = ImVec2(0.0f, 0.0f);
		style.FramePadding = ImVec2(sizeinfo.padding, sizeinfo.padding);
		style.FrameRounding = fRounding;
		pcrHander = pProj;
		pWndPosSlot = pWndPos;
		orgContent = posContent;
		return TRUE;
	}

	// 函数: HasActiveInput
	// 描述: 本帧是否存在"处于编辑中(ImGui 项 active)的输入控件"
	BOOL HasActiveInput()
	{
		// 计数归部件层(输入部件在其项 active 时累加) ⇒ 控件层只转调
		return Widgets::HasActiveInput();
	}

	// ================================
	// 区域: 控件绘制接口表
	// 说明: 表为进程级静态, 内容恒为下方三枚实现 ⇒ 宿主重复注入同一张表无副作用。
	// ================================
	static CONST CtrlApi CtrlApiTable = {
		UpdateContext,		// 每次渲染前设一次控件上下文
		DrawCtrl,			// 按控件类型内部分派(含 TITLE, 不单列 TitleBar)
		HasActiveInput,		// 是否存在编辑中的输入控件
	};

	// 函数: GetCtrlApi
	// 描述: 取控件库的绘制接口表(进程级静态表; 重复调用幂等)
	CONST CtrlApi* GetCtrlApi()
	{
		return &CtrlApiTable;
	}

}	// namespace Controls

// ================================
// 命名空间: Controls(续) —— 自定义控件实现
// ================================
namespace Controls
{

	// ================================
	// 区域: 自定义控件实现区
	// ================================

	// 函数: TitleBar
	// 描述: 标题控件(CTRL_TYPE_TITLE): 绘制标题文本与右上关闭钮, 并回传标题状态
	VOID TitleBar(CtrlInfo* info)
	{
		// 拖动状态(函数级静态): bDrag = 本次拖动是否已建立; hDragWnd = 拖动所属窗口(复位判据);
		// ptGrab = 抓取偏移(光标相对窗口左上角; 拖动首帧建立, 拖动全程保持)
		static BOOL bDrag = FALSE;
		static HWND hDragWnd = NULL;
		static vec2 ptGrab(0.0f, 0.0f);

		CHAR* lpText = CtrlTextBuffer(info);		// 文本 = 控件缓冲自身
		LPCSTR label = (lpText != NULL) ? lpText : "";
		ImVec2 size = toImvec2(info->size);
		ImVec2 pos = toImvec2(info->position);		// 控件矩形原点(窗口本地坐标)
		// 配色方案(控件层采样 ⇒ 部件不接触层号; 两件均用平坦样式, 只差层次值):
		//   标题底与标题文本取本控件层下沉一档(结构件层次恒 0 ⇒ 取窗口底之下一档);
		//   关闭钮取本控件层(比标题底亮一档 —— 与改前 CtrlLayerOf(CtrlRel(info)) 口径一致)
		CONST Widgets::Base::PaintWay wayTitle = Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info) - 1, PALETTE_FLAT));
		CONST Widgets::Base::PaintWay wayExit = Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT));
		FLOAT exitwidth = 1.5f;
		FLOAT padding = sizeinfo.padding;
		uint16_t wCommon = OPER_COMMON_NONE;		// 公共码位图(组合位)
		uint16_t wOwn = 0;							// 专属段码(结构件族; 0 = 无)
		// 1. 标题底: 先取屏幕坐标(布局本地坐标会随窗口 / 容器原点偏移), 再自绘内缩 1 像素的底条
		ImGui::SetCursorPos(pos);
		ImVec2 position = ImGui::GetCursorScreenPos();
		ImDrawList* drawlist = ImGui::GetWindowDrawList();
		drawlist->AddRectFilled(position + ImVec2(1, 1), position + size - ImVec2(1, 0),
			wayTitle.palette.crBackground.crBase, sizeinfo.space + 1, ImDrawFlags_RoundCornersTop);
		// 此时会破坏原边框, 所以补画(人工核对留痕, 默认注释)
		//drawlist->AddRect(position, position + size, 0xFF00FF00, sizeinfo.space, 1.0, ImDrawFlags_RoundCornersTop);
		//drawlist->AddRect(position, position + size, 0xFFFF0000, sizeinfo.space + 2, 1.0, ImDrawFlags_RoundCornersTop);
		//drawlist->AddRectFilled(position + ImVec2(1, 1), position + size, 0xFF0000FF, sizeinfo.space + 1, ImDrawFlags_RoundCornersTop);
		// 2. 可交互标题区(右侧关闭钮区让给 ExitSign; 命中项不下沉 ⇒ 由本控件提交)
		ImGui::InvisibleButton(label, size - ImVec2{ size.y * exitwidth, 0 });
		// 公共码: 生命周期(按住 = 电平; 激活/失去激活 = 沿)
		if (ImGui::IsItemActive())    wCommon |= OPER_COMMON_HOLD;
		if (ImGui::IsItemActivated()) wCommon |= OPER_COMMON_ACTIVE;
		if (ImGui::IsItemDeactivated()) wCommon |= OPER_COMMON_DEACTIVATED;
		// 双击: 必须限定本控件命中区(不得直接取全局鼠标状态)
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			wCommon |= OPER_COMMON_DOUBLECLICK;
		}
		// 3. 标题文本(经同名部件 Widgets::Base::Text; 部件矩形 = 横向留一枚设计间隔后的矩形,
		//    纵向由部件在声明高内垂直居中 ⇒ 居中量不再由控件层计算, 与改前同一位置)
		Widgets::Base::Text(info->id, ImVec2(pos.x + sizeinfo.space, pos.y),
			ImVec2(size.x - sizeinfo.space, size.y), wayTitle, label);
		// 4. 关闭钮(经部件 Widgets::Base::ExitSign; 部件内部走 SetCursorScreenPos ⇒ 传屏幕坐标)
		if (Widgets::Base::ExitSign(info->id, position + ImVec2{ size.x - size.y * exitwidth, 0 }, { size.y * exitwidth, size.y },
			wayExit, u8"##TITLEBAR_EXIT", padding, 1.0f))
		{
			wOwn = OPER_STRUCT_TITLE_CLOSE;
		}
		// 参数槽(通用槽): 按住期间 = 窗口期望位置(只上报坐标, 不做任何窗口调用, 也不写位置记录)
		// 说明: 期望位置由"标题栏绑定的事件"记进窗口的位置槽(见 ControlSpec.h 的 CTRL_TYPE_TITLE 约定;
		//       未绑定事件的窗口因此不可拖动), 再由渲染出口在下次取得渲染目标时结算成实质移动。
		// 拖动基线: 抓取偏移在拖动首帧建立(光标相对窗口左上角; 拖动全程保持) ⇒ 期望位置 = 光标 - 抓取偏移
		//   (与窗口当前位置无关: 窗口随后被移动也不会让期望位置逐帧累加)
		uint64_t param = 0;
		CONST HWND hWnd = ImGuiHost::GetCurrentWnd();		// 当前渲染窗口(拖动状态归属判据)
		if ((wCommon & OPER_COMMON_HOLD) != 0)
		{
			POINT cur{};
			if (hWnd != NULL && GetCursorPos(&cur) != FALSE)
			{
				if (bDrag == FALSE || hDragWnd != hWnd)
				{
					// 拖动首帧: 取窗口当前坐标(来源见常量 GRAB_BASE_BY_WND_POS), 建立抓取偏移
					// (两个来源都取不到 ⇒ 不建立拖动)
					vec2 posWnd;
					BOOL bGot = FALSE;
					if (GRAB_BASE_BY_WND_POS != FALSE && pWndPosSlot != NULL)
					{
						posWnd = *pWndPosSlot;
						bGot = TRUE;
					}
					else
					{
						bGot = ImGuiHost::GetWndPos(&posWnd);
					}
					if (bGot != FALSE)
					{
						ptGrab = vec2((FLOAT)cur.x - posWnd.x, (FLOAT)cur.y - posWnd.y);
						hDragWnd = hWnd;
						bDrag = TRUE;
					}
				}
				if (bDrag != FALSE && hDragWnd == hWnd)
				{
					param = PackWndPos(vec2((FLOAT)cur.x - ptGrab.x, (FLOAT)cur.y - ptGrab.y));
				}
			}
		}
		else if (bDrag != FALSE && hDragWnd == hWnd)
		{
			// 松开(且拖动状态属于本窗口)⇒ 结束本次拖动
			bDrag = FALSE;
			hDragWnd = NULL;
		}
		// 三段拼装: 公共段 = 公共码 + "未搭载"标志; 专属段 = 结构件结论; 搭载段 = 恒 0
		uint64_t oper = OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0);
		if (info->pEventHandler && oper != OPER_NONE)
		{
			info->pEventHandler(info, oper, param, info->bufferInfo.buffer);
		}
	}
	// 函数: InputReal
	// 描述: 实数输入控件(INPUT_REAL): 缓冲为 DOUBLE; 绘制经部件 Widgets::Base::InputScalar
	//       (底与边框由 ImGui 原生框绘制, 内边距按声明高对齐); 提交操作码 = 专属段 OPER_EDIT_SUBMIT
	VOID InputReal(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		BufferInfo& bufInfo = info->bufferInfo;
		if (bufInfo.buffer == NULL || bufInfo.size < sizeof(double))
		{
			// 缓冲区缺失时以只读占位(与 InputSingle 占位口径一致)
			Widgets::Base::InputText(info->id, toImvec2(info->position), toImvec2(info->size),
				Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)), ERRBUFFER, 0, NULL);
			return;
		}
		// 绘制 + 取回部件结论(独立使用 ⇒ 放专属段; 部件返回 0 则不回调)
		const int64_t qwPart = Widgets::Base::InputScalar(info->id, toImvec2(info->position),
			toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)),
			(VOID*)bufInfo.buffer, ImGuiDataType_Double, "%05.2f", NULL);
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);				// bit[15:0]: 专属段码
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);	// bit[31:16]: 公共位图
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: InputNumeric
	// 描述: 与 InputReal 同构: 缓冲为单个 INT; 绘制经部件 Widgets::Base::InputScalar
	//       (ImGuiDataType_S32, 格式 "%d"; 仅允许整数文本输入); 激活期禁 IME;
	//       值变化(失焦/回车/箭头)时提交(专属段 OPER_EDIT_SUBMIT)
	VOID InputNumeric(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		BufferInfo& bufInfo = info->bufferInfo;
		if (bufInfo.buffer == NULL || bufInfo.size < sizeof(INT))
		{
			// 缓冲区缺失时以只读占位(与 InputSingle 占位口径一致)
			Widgets::Base::InputText(info->id, toImvec2(info->position), toImvec2(info->size),
				Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)), ERRBUFFER, 0, NULL);
			return;
		}
		// 绘制 + 取回部件结论(独立使用 ⇒ 放专属段; 部件返回 0 则不回调)
		const int64_t qwPart = Widgets::Base::InputScalar(info->id, toImvec2(info->position),
			toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)),
			(VOID*)bufInfo.buffer, ImGuiDataType_S32, "%d", NULL);
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);				// bit[15:0]: 专属段码
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);	// bit[31:16]: 公共位图
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: InputAction
	// 描述: 输入动作控件(CTRL_TYPE_INPUT_ACTION): 按控件缓冲取输入文本与按钮文案, 调 Widgets::Base::InputAction
	//       绘制(底部矩形 + 无边框单行输入 + 右侧按钮)
	VOID InputAction(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		// 输入文本: 控件缓冲(CtrlBuffer.h 的 CtrlTextBuffer/CtrlTextCapacity);
		// 无专用缓冲 ⇒ 只读占位(首址有效 + 容量 0)
		CHAR* lpText = CtrlTextBuffer(info);
		size_t nTextCap = CtrlTextCapacity(info);
		if (lpText == NULL || nTextCap == 0)
		{
			lpText = ERRBUFFER;
			nTextCap = 0;
		}
		// 按钮文案: 缓冲内按钮文本区(data + 0; 见 CtrlInputActionData); 无缓冲时用缺省文案
		LPCSTR lpBtn = CTRL_DEFAULT_ACTION_BTN;
		if (info->bufferInfo.buffer != NULL && info->bufferInfo.size >= CTRL_INPUT_ACTION_BUF)
		{
			CHAR* lpBufBtn = CtrlInputActionBtnText((CtrlInputActionData*)info->bufferInfo.buffer);
			if (lpBufBtn != NULL && lpBufBtn[0] != '\0')
			{
				lpBtn = lpBufBtn;
			}
		}
		Widgets::Base::Palette palette{};
		// 配色口径(层次与层号见 ControlSpec.h 的"控件层次与层号换算"区):
		//   窗体面 = 本控件的绘图层;按钮面 = 窗体面之上上浮一层;
		//   两面状态 = 投影槽位(与库内其它件一致 ⇒ 悬停/激活只做 ±0.1 层间差值的微调);边框与禁用态同列表行件;
		//   两面各自的前景(输入段文字 / 按钮文字) = 各自面所在层的对比槽 ⇒ 前景随层次
		CONST DWORD dwCtlLayer = CtrlLayer(info);			// 窗体面 = 本控件层次对应的层号
		CONST INT32 nBtnLayer = CtrlLayerOf(info->nLayer + 1);	// 按钮面 = 窗体面之上上浮一档
		palette.crBackground.crBase = ProjColor(dwCtlLayer);
		palette.crBackground.crHovered = ProjColor(dwCtlLayer, ProjCR::PROJ_SLOT_HOVER);
		palette.crBackground.crActive = ProjColor(dwCtlLayer, ProjCR::PROJ_SLOT_ACTIVE);
		palette.crBackground.crDisable = ProjColor(PROJ_L_DISABLED);
		palette.crForeground.crBase = ProjColor(dwCtlLayer, ProjCR::PROJ_SLOT_CONTRAST);	// 输入段文字 = 窗体面层的对比槽
		palette.crForeground.crHovered = palette.crForeground.crBase;
		palette.crForeground.crActive = palette.crForeground.crBase;
		palette.crForeground.crDisable = ProjColor(PROJ_L_DISABLED);
		palette.crSurface.crBase = ProjColor(nBtnLayer);
		palette.crSurface.crHovered = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_HOVER);
		palette.crSurface.crActive = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_ACTIVE);
		palette.crSurface.crDisable = ProjColor(PROJ_L_DISABLED);
		palette.crSurfaceForeground.crBase = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_CONTRAST);	// 按钮文字 = 按钮面层的对比槽
		palette.crSurfaceForeground.crHovered = palette.crSurfaceForeground.crBase;
		palette.crSurfaceForeground.crActive = palette.crSurfaceForeground.crBase;
		palette.crSurfaceForeground.crDisable = ProjColor(PROJ_L_DISABLED);
		palette.crBorder.crBase = ProjColor(PROJ_L_BORDER);
		palette.crBorder.crHovered = ProjColor(PROJ_L_BORDER_HOVER);
		palette.crBorder.crActive = ProjColor(PROJ_L_BORDER_ACTIVE);
		palette.crBorder.crDisable = ProjColor(dwCtlLayer);		// 禁用态边框 = 本控件层(与列表行件同口径: 取自身层)
		palette.crBorderInner = palette.crBorder;		// 按钮段内框与外框同色
		// 绘制 + 取回部件结论(部件 → 调用方 返回约定; 0 = 无操作)
		const int64_t qwPart = Widgets::Base::InputAction(info->id, toImvec2(info->position), toImvec2(info->size), Widgets::Base::MakePaletteWay(palette), sizeinfo.fontSize * (CalcTextWidth(lpBtn) + 2) / 2, lpText, nTextCap, NULL, lpBtn);
		// 独立使用 ⇒ 放专属段: 取四位段的高低两段, 搭载段恒 0(常量与宏见 ControlSpec.h 的"操作码(OPER)系统"区)
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);				// bit[15:0]: 专属段码
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);	// bit[31:16]: 公共位图
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: Slider
	// 描述: 单手柄滑动条(CTRL_TYPE_SLIDER): 一个值 + 一个值域, 值变更回传专属段 OPER_EDIT_CHANGED
	VOID Slider(CtrlInfo* info)
	{
		BufferInfo buf = info->bufferInfo;
		if (buf.buffer == NULL || buf.size < sizeof(CtrlSliderData))
		{
			return;
		}
		CtrlSliderData& slider = *(CtrlSliderData*)buf.buffer;
		// 绘制 + 取回部件结论(部件 → 调用方 返回约定; 0 = 无操作)
		// 配色 = 下沉含上浮样式: 轨道(整条底)下沉一档, 填充段与手柄(段面)回到本层(试行调用点)
		const int64_t qwPart = Widgets::Base::SingleSlider(
			info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN_RAISED)),
			slider.vMin, slider.lMin, slider.lMax);
		// 独立使用 ⇒ 放专属段: 取四位段的高低两段, 搭载段恒 0(与 InputSingle / Combo 同法)
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);				// bit[15:0]: 专属段码
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);	// bit[31:16]: 公共位图
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}
	// 函数: Progress
	// 描述: 进度状态显示控件(CTRL_TYPE_PROG): 可见期间每帧回传专属段 OPER_PROG_UPDATED
	VOID Progress(CtrlInfo* info)
	{
		if (info->bufferInfo.buffer == NULL || info->bufferInfo.size < sizeof(CtrlProgData))
		{
			return;
		}
		CtrlProgData& prog = *(CtrlProgData*)info->bufferInfo.buffer;
		if (prog.IsVisible)
		{
			if (info->pEventHandler)
			{
				info->pEventHandler(info, OperCode::MakeCodeOwn(OPER_PROG_UPDATED), 0, info->bufferInfo.buffer);
			}
			// 配色口径: 底取本控件层次的表面色、文本取该层对比槽(见部件 ProgressStatic)
			Widgets::Base::ProgressStatic(info->id, toImvec2(info->position), toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT)), prog.current, prog.expect, sizeinfo.padding);
		}
	}
	// 函数: RenderSurface
	// 描述: 渲染面控件(CTRL_TYPE_RENDER): 有纹理时按控件矩形等比居中绘制, 无纹理时画"占位面"
	//       ①纹理视图与尺寸取控件缓冲(CtrlRenderData) —— 纹理由宿主创建并注入, 控件库不持设备、
	//         不管理纹理生命周期(创建与释放一律归宿主);
	//       ②等比规则: 缩放比 = min(矩形宽 / 纹理宽, 矩形高 / 纹理高) ⇒ 图不变形, 在控件矩形内居中;
	//       ③无纹理(或尺寸非法) ⇒ 与 CTRL_TYPE_RECT 同一视觉语言(圆角底 + 描边) + 居中占位文案,
	//         保证"声明即有像素"(已知类型若不提交任何项, ImGui 会因 SetCursorPos 扩展窗口边界逐帧报错)。
	VOID RenderSurface(CtrlInfo* info)
	{
		// 变量声明区
		CtrlRenderData* pData = NULL;
		BOOL bDrawn = FALSE;						// 部件是否已按纹理绘制(TRUE = 已提交图项 ⇒ 无需区域占位项)
		if (info == NULL)
		{
			return;
		}
		// 1. 缓冲取用(无缓冲/尺寸不足 ⇒ 按"无图"处理, 仍画占位面)
		if (info->bufferInfo.buffer != NULL && info->bufferInfo.size >= sizeof(CtrlRenderData))
		{
			pData = (CtrlRenderData*)info->bufferInfo.buffer;
		}
		// 2. 绘制经同名部件 Widgets::Base::RenderSurface(有纹理按控件矩形等比居中 / 无纹理画占位面);
		//    配色方案由控件层按本控件层次构造 ⇒ 部件不接触层号
		bDrawn = Widgets::Base::RenderSurface(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT)),
			(pData != NULL) ? pData->pTexture : NULL,
			(pData != NULL) ? pData->texWidth : 0,
			(pData != NULL) ? pData->texHeight : 0);
		// 3. 提交区域: 无图时部件只画占位面(不提交任何项) ⇒ 由本控件占住控件矩形
		//    (与未知类型占位同法, 避免 ImGui "SetCursorPos() to extend window boundaries" 逐帧报错)
		if (bDrawn == FALSE)
		{
			ImGui::SetCursorPos(toImvec2(info->position));
			ImGui::Dummy(toImvec2(info->size));
		}
	}
	// 函数: CheckBox
	// 描述: 复选框控件(CTRL_TYPE_CHECKBOX): 方框 / 勾 / 文字一律由 ImGui 原生复选框绘制
	//       ①缓冲 = 勾选态 + 显示文字(CtrlCheckData; 见 CtrlBuffer.h) —— 缓冲缺失/尺寸不足时按
	//         "未勾选 + 无文字"降级, 仍画方框(保证"声明即有像素");
	//       ②命中区 = 原生复选框项(方框 + 项内距 + 文字), 不再是整行矩形;
	//       ③点击即切换勾选态(部件写回 IsChecked, 本件写回缓冲首字段)并落专属段 OPER_EDIT_CHANGED ——
	//         与 COMBO / SLIDER 的"值已改变"同口径; 本件不派发公共码 CLICKED(点击事实由渲染出口按需判定);
	//       ④配色随控件层次: 方框面 / 描边 / 勾 / 文字由控件层按本控件层次构造配色方案后经第四固定参数交给部件
	VOID CheckBox(CtrlInfo* info)
	{
		// 变量声明区
		CtrlCheckData* pData = NULL;			// 控件缓冲(勾选态 + 显示文字)
		LPCSTR lpLabel = "";					// 右侧显示文字(无缓冲时为空串)
		BOOL IsChecked = FALSE;					// 勾选态
		int64_t qwPart = 0;						// 部件返回约定(0 = 勾选态未改变)
		if (info == NULL)
		{
			return;
		}
		// 1. 缓冲取用(无缓冲/尺寸不足 ⇒ 按"未勾选 + 无文字"降级, 仍画方框)
		if (info->bufferInfo.buffer != NULL && info->bufferInfo.size >= sizeof(CtrlCheckData))
		{
			pData = (CtrlCheckData*)info->bufferInfo.buffer;
			IsChecked = pData->IsChecked;
			lpLabel = pData->szLabel;
		}
		// 2. 绘制经同名部件 Widgets::Base::CheckBox(部件自带可交互项 ⇒ 点击时写回勾选态并给出专属段码)
		//    配色 = 上浮含下沉样式: 方框面(整条底)上浮一档, 勾与文字取该面层的对比槽(试行调用点)
		qwPart = Widgets::Base::CheckBox(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_RAISED_SUNKEN)), IsChecked, lpLabel);
		// 3. 勾选态改变: 写回缓冲首字段并回传结论(专属段 OPER_EDIT_CHANGED; 无缓冲时只回传、不改值)
		if (qwPart != 0)
		{
			if (pData != NULL)
			{
				pData->IsChecked = IsChecked;
			}
			if (info->pEventHandler != NULL)
			{
				info->pEventHandler(info, OperCode::MakeCodeOwn(OPER_EDIT_CHANGED), 0, info->bufferInfo.buffer);
			}
		}
	}
	// 函数: Combo
	// 描述: 下拉框控件(CTRL_TYPE_COMBO): 绘制经同名部件, 值/选项变更时回传专属段 OPER_EDIT_CHANGED
	//       展开列表与 ComboAction 同源; 部件会写回选中索引
	VOID Combo(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		if (info->bufferInfo.buffer == NULL || info->bufferInfo.size < sizeof(CtrlComboData))
		{
			return;
		}
		CtrlComboData& combo = *(CtrlComboData*)info->bufferInfo.buffer;
		// 绘制 + 取回部件结论(无选项时部件不产出像素 ⇒ 结论为 0, 与改前"不绘制"口径一致)
		// 配色 = 下沉样式(下拉同属输入域, 与输入框同口径)
		const int64_t qwPart = Widgets::Base::Combo(info->id, toImvec2(info->position),
			toImvec2(info->size), Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)), combo.items, combo.count, combo.index);
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);				// bit[15:0]: 专属段码
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);	// bit[31:16]: 公共位图
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}

	// 函数: TimeBar
	// 描述: 可编辑时间条控件(CTRL_TYPE_TIMEBAR): 取控件缓冲的提示文本与毫秒值, 绘制时码四段输入;
	//       提交时派发专属段编辑提交码(缓冲已由部件按上限钳制并写回)
	VOID TimeBar(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		if (info->bufferInfo.buffer == NULL || info->bufferInfo.size < sizeof(CtrlTimeBarData))
		{
			ImGui::SetCursorPos(toImvec2(info->position));
			ImGui::Dummy(toImvec2(info->size));
			return;
		}
		CtrlTimeBarData* pData = (CtrlTimeBarData*)info->bufferInfo.buffer;
		const int64_t qwPart = Widgets::Base::TimeBar(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_SUNKEN)),
			pData->szHint, pData->valueMs, pData->limitMs);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			CONST uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
			CONST uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
			info->pEventHandler(info,
				OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0),
				0, info->bufferInfo.buffer);
		}
	}

	// 函数: FormatTimeSpanText
	// 描述: 把起止毫秒格式化为时码文本(时:分:秒.毫秒 - 时:分:秒.毫秒)
	static VOID FormatTimeSpanText(int64_t nStartMs, int64_t nEndMs, LPSTR lpText, size_t cbText)
	{
		if (lpText == NULL || cbText == 0)
		{
			return;
		}
		if (nStartMs < 0) { nStartMs = 0; }
		if (nEndMs < 0) { nEndMs = 0; }
		CONST int64_t h1 = nStartMs / 3600000;
		CONST int64_t m1 = (nStartMs / 60000) % 60;
		CONST int64_t s1 = (nStartMs / 1000) % 60;
		CONST int64_t ms1 = nStartMs % 1000;
		CONST int64_t h2 = nEndMs / 3600000;
		CONST int64_t m2 = (nEndMs / 60000) % 60;
		CONST int64_t s2 = (nEndMs / 1000) % 60;
		CONST int64_t ms2 = nEndMs % 1000;
		sprintf_s(lpText, cbText, u8"%02lld:%02lld:%02lld.%03lld - %02lld:%02lld:%02lld.%03lld",
			(long long)h1, (long long)m1, (long long)s1, (long long)ms1,
			(long long)h2, (long long)m2, (long long)s2, (long long)ms2);
	}

	// 函数: SliderRange
	// 描述: 双手柄范围滑块控件(SLIDER_RANGE): 左/右手柄分别派发专属段与编辑变更码
	VOID SliderRange(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		if (info->bufferInfo.buffer == NULL || info->bufferInfo.size < sizeof(CtrlSliderRangeData))
		{
			ImGui::SetCursorPos(toImvec2(info->position));
			ImGui::Dummy(toImvec2(info->size));
			return;
		}
		CtrlSliderRangeData* pData = (CtrlSliderRangeData*)info->bufferInfo.buffer;
		int64_t qwPart = Widgets::Base::RangeSlider(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_FLAT)),
			pData->vMin, pData->vMax, pData->lMin, pData->lMax);
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			CONST uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);
			CONST uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);
			info->pEventHandler(info,
				OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0),
				0, info->bufferInfo.buffer);
		}
	}

	// 函数: TimeSpanRow
	// 描述: 时间片列表项部件: 行点击区 + 时码文本 + 行尾删除钮(返回列表项部件操作约定)
	static int64_t TimeSpanRow(int nRowId, vec2 pos, vec2 size, CONST Widgets::Base::PaintWay& way, VOID* lpItem)
	{
		using namespace Widgets::Base;

		int64_t qwOper = 0;
		TimeSpanItem* pItem = (TimeSpanItem*)lpItem;
		if (pItem)
		{
			// 准备文本
			CHAR szText[96]{}, szDelete[4] = { '-' };
			FormatTimeSpanText(pItem->startMs, pItem->endMs, szText, sizeof(szText));
			qwOper = TextAction(nRowId, toImvec2(pos), toImvec2(size), way, size.y, szText, szDelete);
		}

		return qwOper;
	}

	// 函数: ListTimeSpan
	// 描述: 时间片列表控件(LIST_TIMESPAN): 逐行显示起止时码, 行内删除钮与行选中经操作码回传
	VOID ListTimeSpan(CtrlInfo* info)
	{
		int64_t qwOper = 0;
		uint64_t qwParam = 0;
		if (info == NULL)
		{
			return;
		}
		if (info->bufferInfo.buffer == NULL || info->bufferInfo.size < sizeof(ListBuffer))
		{
			ImGui::SetCursorPos(toImvec2(info->position));
			ImGui::Dummy(toImvec2(info->size));
			return;
		}
		ListBuffer* pList = (ListBuffer*)info->bufferInfo.buffer;
		DWORD dwCount = (pList->dwCount <= pList->dwCapacity) ? pList->dwCount : pList->dwCapacity;
		size_t cbItem = (pList->cbItem >= sizeof(TimeSpanItem)) ? (size_t)pList->cbItem : sizeof(TimeSpanItem);
		CONST FLOAT fInner = sizeinfo.padding;
		CONST FLOAT fRowH = sizeinfo.controlHeight;
		CONST FLOAT fStep = fRowH;
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, fRounding);
		ImGui::SetCursorPos(toImvec2(info->position));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ProjColor(CtrlLayerOf(CtrlRel(info))));
		ImGui::BeginChild(info->label ? info->label : u8"##TIMESPANLIST", toImvec2(info->size), true,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleColor();
		DWORD dwVisible = (DWORD)((info->size.y - fInner) / fStep);
		if (dwVisible < 1) { dwVisible = 1; }
		if (dwCount > 0 && pList->dwTop >= dwCount)
		{
			pList->dwTop = dwCount - 1;
		}
		if (dwCount > dwVisible && pList->dwTop + dwVisible > dwCount)
		{
			pList->dwTop = dwCount - dwVisible;
		}
		if (pList->dwTop > dwCount)
		{
			pList->dwTop = 0;
		}
		if (ImGui::IsWindowHovered())
		{
			FLOAT fWheel = ImGui::GetIO().MouseWheel;
			if (fWheel < 0.0f && pList->dwTop + dwVisible < dwCount)
			{
				pList->dwTop++;
			}
			else if (fWheel > 0.0f && pList->dwTop > 0)
			{
				pList->dwTop--;
			}
		}
		CONST Widgets::Base::PaintWay way = Widgets::Base::MakePaletteWay(MakePalette(CtrlRel(info), PALETTE_RAISED));
		FLOAT fW = ImGui::GetWindowWidth() - fInner * 2.0f;

		if (fW < 1.0f) { fW = 1.0f; }
		for (DWORD i = 0; i < dwVisible && (pList->dwTop + i) < dwCount; i++)
		{
			DWORD dwRow = pList->dwTop + i;
			VOID* lpItem = (VOID*)((BYTE*)(pList + 1) + cbItem * (size_t)dwRow);
			CONST vec2 posRow(fInner, fInner + (fInner + fStep) * (FLOAT)i);
			if (dwRow == pList->dwSelected)
			{
				ImGui::SetCursorPos(toImvec2(posRow));
				CONST ImVec2 o = ImGui::GetCursorScreenPos();
				ImGui::GetWindowDrawList()->AddRectFilled(o, ImVec2(o.x + fW, o.y + fRowH),
					ImGui::GetColorU32(ImGuiCol_Header), fRounding * 0.5f);
			}
			int64_t qwRow = TimeSpanRow((int)dwRow, posRow, vec2(fW, fRowH), way, lpItem);
			if (qwRow != 0)
			{
				ListCollectRowOper(qwRow, dwRow, qwOper, qwParam);
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
		if (qwOper != 0)
		{
			CONST uint32_t dwRowOper = (uint32_t)(OperCode::GetSegment((uint64_t)qwOper,
				OperCode::OPER_SEG_PAYLOAD) & 0xFFFF);
			if (dwRowOper == OPER_LIST_ROW_ACTIVATED)
			{
				pList->dwSelected = (DWORD)qwParam;
				qwOper = (int64_t)OperCode::MakeCode(
					OperCode::MakeCommon(OPER_COMMON_NONE, OPER_PAYLOAD_OPCODE),
					0, OPER_LIST_ROW_SELECTION_CHANGED);
				qwParam = 0;
			}
			if (info->pEventHandler != NULL)
			{
				info->pEventHandler(info, (uint64_t)qwOper, qwParam, info->bufferInfo.buffer);
			}
		}
	}

	// 函数: ComboAction
	// 描述: 下拉动作控件(CTRL_TYPE_COMBO_ACTION): 按控件缓冲(CtrlComboData)取选项表与选中索引,
	//       调 Widgets::Base::ComboAction 绘制(底部矩形 + 无边框下拉框 + 右侧按钮; 展开列表与 Combo 同源)
	VOID ComboAction(CtrlInfo* info)
	{
		if (info == NULL)
		{
			return;
		}
		CtrlComboData* pCombo = NULL;
		LPCSTR* lpItems = NULL;
		INT nCount = 0;
		INT nIndex = 0;
		if (info->bufferInfo.buffer != NULL && info->bufferInfo.size >= sizeof(CtrlComboData))
		{
			pCombo = (CtrlComboData*)info->bufferInfo.buffer;
			lpItems = pCombo->items;
			nCount = pCombo->count;
			nIndex = pCombo->index;
		}
		// 无缓冲/无选项: 部件不产出像素 ⇒ 提交空白项占住控件矩形
		//   (不提交项时 ImGui 会因 SetCursorPos 扩边界而逐帧报错)
		if (lpItems == NULL || nCount <= 0)
		{
			ImGui::SetCursorPos(toImvec2(info->position));
			ImGui::Dummy(toImvec2(info->size));
			return;
		}
		// 配色板: 整体背景透明且无整条外框 ——
		// 部件补画的"整条底 + 整条外框"取全 0(alpha = 0 ⇒ 不产出像素), 只剩右段按钮按主题
		// 三态画面(crSurface)与内框(crBorderInner); 左段下拉项的面/框已压透明。
		const DWORD crDisabled = ProjColor(PROJ_L_DISABLED);
		CONST INT32 nBtnLayer = CtrlLayer(info);		// 按钮面 = 本控件层次对应的层号(下拉箭头与右段按钮同层)
		Widgets::Base::Palette palette{};
		palette.crBackground = Widgets::Base::StateColors{};	// 全 0: 整条底透明(左段下拉面全透明)
		palette.crForeground.crBase = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_CONTRAST);	// 左段下拉文字 = 本层对比槽
		palette.crForeground.crHovered = palette.crForeground.crBase;
		palette.crForeground.crActive = palette.crForeground.crBase;
		palette.crForeground.crDisable = crDisabled;
		palette.crSurface.crBase = ProjColor(nBtnLayer);	// 按钮/箭头面 = 本控件层次
		palette.crSurface.crHovered = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_HOVER);
		palette.crSurface.crActive = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_ACTIVE);
		palette.crSurface.crDisable = crDisabled;
		palette.crSurfaceForeground.crBase = ProjColor(nBtnLayer, ProjCR::PROJ_SLOT_CONTRAST);	// 按钮文字 = 本层对比槽
		palette.crSurfaceForeground.crHovered = palette.crSurfaceForeground.crBase;
		palette.crSurfaceForeground.crActive = palette.crSurfaceForeground.crBase;
		palette.crSurfaceForeground.crDisable = crDisabled;
		palette.crBorder = Widgets::Base::StateColors{};		// 全 0: 无整条外框
		// 内框随投影"边框三级"(常态 / 悬停 / 激活) —— 与同屏其它自绘件的边框口径一致
		palette.crBorderInner.crBase = ProjColor(PROJ_L_BORDER);
		palette.crBorderInner.crHovered = ProjColor(PROJ_L_BORDER_HOVER);
		palette.crBorderInner.crActive = ProjColor(PROJ_L_BORDER_ACTIVE);
		palette.crBorderInner.crDisable = crDisabled;
		// 按钮宽按文案定宽(缺省文案 = 控件缺省值, 应用可经缓冲按钮文本区覆盖)
		LPCSTR lpBtn = CTRL_DEFAULT_ACTION_BTN;
		const int64_t qwPart = Widgets::Base::ComboAction(info->id, toImvec2(info->position), toImvec2(info->size),
			Widgets::Base::MakePaletteWay(palette),
			sizeinfo.fontSize * (CalcTextWidth(lpBtn) + 2) / 2,
			lpItems, nCount, nIndex, lpBtn);
		// 选中索引写回缓冲(下拉项变更时部件已改写 nIndex)
		pCombo->index = nIndex;
		// 独立使用 ⇒ 放专属段: 取四位段的高低两段, 搭载段恒 0; 触发结论经控件自身事件处理器回传
		const uint16_t wOwn = (uint16_t)((uint64_t)qwPart & 0xFFFF);				// bit[15:0]: 专属段码
		const uint16_t wCommon = (uint16_t)(((uint64_t)qwPart >> 16) & 0xFFFF);	// bit[31:16]: 公共位图
		if (qwPart != 0 && info->pEventHandler != NULL)
		{
			info->pEventHandler(info, OperCode::MakeCode(OperCode::MakeCommon(wCommon, OPER_PAYLOAD_NONE), wOwn, 0), 0, info->bufferInfo.buffer);
		}
	}

}	// namespace Controls

// ================================
// 区域: 文件内辅助函数区(定义)
// 说明: 本区为文件内辅助函数的函数体(前向声明见文件上方), 文件作用域 static。
// ================================
// 函数: DrawRectFace
// 描述: 画"矩形外观"(圆角底面 + 圆角描边)。供"通用矩形绘制"(CTRL_TYPE_RECT / 未知类型矩形占位)共用。
//       色值由调用方按投影取(RECT 口径: 层级由绘制图层决定 —— 画在背景之上下沉一层 / 画在 RECT 之内上浮一层; 见 Controls::Rect),
//       本函数只画不取色。
static VOID DrawRectFace(ImDrawList* dl, ImVec2 o, ImVec2 size, DWORD crBase, DWORD crBorder)
{
	if (dl == NULL)
	{
		return;
	}
	ImVec2 e(o.x + size.x, o.y + size.y);
	dl->AddRectFilled(o, e, crBase, Controls::fRounding);
	dl->AddRect(o, e, crBorder, Controls::fRounding, 0, 1.0f);
}
// 函数: DrawUnknownCtrlRect
// 描述: 未知类型(文档类型串未命中 = CTRL_TYPE_UNKNOWN)的矩形占位绘制 —— 未知类型在被送入 Render 之前
//       替换为矩形呈现(不改写其类型串)。
static VOID DrawUnknownCtrlRect(CtrlInfo* info)
{
	if (info == NULL)
	{
		return;
	}
	const vec2& pos = info->position;
	const vec2& size = info->size;
	// 1. 定位到控件矩形原点(调用方已定位, 此处再显式一次)
	ImGui::SetCursorPos(toImvec2(pos));
	const ImVec2 o = ImGui::GetCursorScreenPos();
	// 2. 以"矩形外观"占位(与 CTRL_TYPE_RECT 同一视觉语言; 层次取控件自身的 nLayer)
	DrawRectFace(ImGui::GetWindowDrawList(), o, toImvec2(size),
		Controls::CtrlLayer(info), Controls::ProjColor(Controls::PROJ_L_BORDER));
	// 3. 提交区域: 占住控件矩形, 避免 ImGui "SetCursorPos() to extend window/parent boundaries" 逐帧报错
	ImGui::SetCursorPos(toImvec2(pos));
	ImGui::Dummy(toImvec2(size));
}
// 函数: CtrlClickCode
// 描述: 在图元绘制之后, 为"本身不回调的图元控件"(CTRL_TYPE_TEXT / CTRL_TYPE_RECT)补一枚覆盖
//       控件矩形的无绘制可交互项(ImGui::InvisibleButton), 并取回本帧指针点击的公共码
static uint16_t CtrlClickCode(CtrlInfo* info, LPCSTR lpFallback)
{
	uint16_t wCommon = OPER_COMMON_NONE;
	if (info == NULL || info->size.x <= 0.0f || info->size.y <= 0.0f)
	{
		return wCommon;
	}
	// 光标移回控件矩形原点(图元绘制已推进光标, 须复位后提交可交互项)
	ImGui::SetCursorPos(toImvec2(info->position));
	ImGui::PushID(info->id);
	ImGui::InvisibleButton(info->label ? info->label : lpFallback, toImvec2(info->size));
	// 单击 / 右键 / 双击(一律限定本控件命中区, 不得直接取全局鼠标状态)
	// 单击 = 完整单击(左键在本项命中区内释放; 与 Button 的判定语义一致, 按下帧不报, 避免"按下即响应"歧义)
	if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		wCommon |= OPER_COMMON_CLICKED;
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		wCommon |= OPER_COMMON_RIGHTCLICK;
	}
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		wCommon |= OPER_COMMON_DOUBLECLICK;
	}
	ImGui::PopID();
	return wCommon;
}
// 函数: ListBufferOf
// 描述: 取列表控件(LIST_INPUT/LIST_COMBO)的缓冲头
static ListBuffer* ListBufferOf(CtrlInfo* info)
{
	if (info == NULL || info->bufferInfo.buffer == NULL ||
		info->bufferInfo.size < sizeof(ListBuffer))
	{
		return NULL;
	}
	return (ListBuffer*)info->bufferInfo.buffer;
}

// 函数: ListCollectRowOper
// 描述: 把"本行行件返回约定"折算进控件的"oper + 回调 param"(ListInput/ListCombo/Command::List
//       逐行收集共用)
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
