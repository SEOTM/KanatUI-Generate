#pragma once
#include "framework/base/BasicDesign.h"
#include "framework/base/ControlSpec.h"
#include "framework/render/Render.h"		// CtrlApi(控件绘制接口表的类型)
// ================================
// 库名: Controls
// 描述: 封装 Dear ImGui 控件的自定义控件库(控件与部件本体)
// ================================

namespace Controls
{
	// 函数: UpdateContext
	// 描述: 更新绘制上下文(内容容器原点 / 窗口位置记录槽 / 参考尺寸 / 投影配色 / 圆角)
	BOOL UpdateContext(CONST AutoSize& refsize, CONST ProjCR* pProj, CONST vec2* pWndPos, CONST vec2& orgContent);

	// 函数: HasActiveInput
	// 描述: 本帧是否存在"处于编辑中(ImGui 项 active)的输入控件"
	BOOL HasActiveInput();

	// ================================
	// 区域: 控件绘制接口(供 Render 的布局遍历逐控件调用)
	// 说明: 逐类型绘制实现以类型语义名命名; DrawCtrl 自行定位控件矩形(ctrl->position);
	//       "声明高生效"由各部件自身负责(项高对齐的内边距口径见 Widgets.h 的 Widgets::Base::CenterOffset);
	//       本组接口只产出控件像素与派发自身事件。
	// ================================

	// 函数: DrawCtrl
	// 描述: 按控件类型分派绘制实现(逐控件绘制入口): 有效坐标换算内容容器本地坐标 + 定位矩形 + 查表分派;
	//       未列入分派表者走兜底路径(未知类型 = 矩形占位; 未使用槽位 = 只占位不画)
	BOOL DrawCtrl(CtrlInfo* ctrl);

	// 函数: TitleBar
	// 描述: 标题栏控件(TITLE): 标题文本 + 右上关闭钮, 标题状态经操作码回传事件处理器
	VOID TitleBar(CtrlInfo* info);
	// 函数: Button
	// 描述: 按钮控件(BUTTON): 点击时调用控件自身事件处理器(操作码 = 公共码 OPER_COMMON_CLICKED)
	VOID Button(CtrlInfo* info);
	// 函数: Text
	// 描述: 静态文本控件(TEXT): 文本图元; 仅当绑定事件(pEventHandler != NULL)时在图元之后补一枚
	//       覆盖控件矩形的无绘制可交互项 ⇒ 可响应单击/右键/双击(操作码 = 公共码 OPER_COMMON_*)
	VOID Text(CtrlInfo* info);
	// 函数: Rect
	// 描述: RECT 控件(纯矩形绘制): 外观 = 控件声明矩形, 无文本; 仅当绑定事件(pEventHandler != NULL)时在
	//       图元之后补一枚覆盖控件矩形的无绘制可交互项 ⇒ 可响应单击/右键/双击(操作码 = 公共码 OPER_COMMON_*)
	VOID Rect(CtrlInfo* info);
	// 函数: InputSingle
	// 描述: 单行输入控件(INPUT_SINGLE): 失焦/回车提交时调用控件自身事件处理器
	//       (操作码 = 专属段 OPER_EDIT_SUBMIT)
	VOID InputSingle(CtrlInfo* info);
	// 函数: InputMulti
	// 描述: 多行文本输入控件(INPUT_MULTI): 长文本(提示词 / 负面提示词 / 参数文件内容)
	VOID InputMulti(CtrlInfo* info);
	// 函数: InputReal
	// 描述: 实数输入控件(INPUT_REAL): 缓冲为 DOUBLE; 提交操作码 = 专属段 OPER_EDIT_SUBMIT
	VOID InputReal(CtrlInfo* info);
	// 函数: InputNumeric
	// 描述: 整数输入控件(INPUT_NUMERIC): 缓冲为 INT; 提交操作码 = 专属段 OPER_EDIT_SUBMIT
	VOID InputNumeric(CtrlInfo* info);
	// 函数: InputAction
	// 描述: 输入动作控件(INPUT_ACTION): 底部矩形 + 无边框单行输入 + 右侧按钮; 文本取控件缓冲
	VOID InputAction(CtrlInfo* info);
	// 函数: Slider
	// 描述: 单手柄滑动条控件(SLIDER): 一个值 + 一个值域; 值变更操作码 = 专属段 OPER_EDIT_CHANGED
	VOID Slider(CtrlInfo* info);
	// 函数: Progress
	// 描述: 进度状态显示控件(PROG); 操作码 = 专属段 OPER_PROG_UPDATED(电平型: 可见期间每帧报一次)
	VOID Progress(CtrlInfo* info);
	// 函数: RenderSurface
	// 描述: 渲染面控件(RENDER): 有纹理时按控件矩形等比居中绘制, 无纹理时画"占位面"
	VOID RenderSurface(CtrlInfo* info);
	// 函数: CheckBox
	// 描述: 复选框控件(CHECKBOX): 方框 / 勾 / 文字由 ImGui 原生复选框绘制(文字取控件缓冲)
	VOID CheckBox(CtrlInfo* info);
	// 函数: Combo
	// 描述: 下拉框控件(COMBO); 值/选项变更操作码 = 专属段 OPER_EDIT_CHANGED
	VOID Combo(CtrlInfo* info);
	// 函数: ComboAction
	// 描述: 下拉动作控件(COMBO_ACTION): 无边框下拉框 + 右侧按钮; 选项表与选中索引取控件缓冲
	//       (绘制经部件 Widgets::Base::ComboAction)
	VOID ComboAction(CtrlInfo* info);
	// 函数: TimeBar
	// 描述: 可编辑时间条控件(TIMEBAR): 提示文本 + 时:分:秒.毫秒四段数字输入; 数据取控件缓冲
	//       (CtrlTimeBarData), 提交时派发专属段编辑提交码
	VOID TimeBar(CtrlInfo* info);
	// 函数: SliderRange
	// 描述: 双手柄范围滑块控件(SLIDER_RANGE): 左/右手柄分别派发专属段与编辑变更码
	VOID SliderRange(CtrlInfo* info);
	// 函数: ListTimeSpan
	// 描述: 时间片列表控件(LIST_TIMESPAN): 逐行显示起止时码, 行内删除钮与行选中经操作码回传
	VOID ListTimeSpan(CtrlInfo* info);
	// 函数: ListRowDropOper
	// 描述: 列表行拖拽落点 → 行件返回约定(纯换算, 不触碰 ImGui; 绘制侧唯一真源)
	int64_t ListRowDropOper(DWORD dwSrc, DWORD dwRow, BOOL IsAfter);

	// 函数: ListInput
	// 描述: 布局列表控件(LIST_INPUT)的绘制入口 —— 外部一次性送入(单参数), 控件自行拆解任务:
	//       按该类型的缓冲约定解析数据(ListBuffer 头 + 行数据区)后, 逐行送入列表管道部件
	//       (部件层 Widgets::Pipe::ListRow); 项目行 = 可编辑输入动作行件(Widgets::Base::InputAction)
	VOID ListInput(CtrlInfo* info);
	// 函数: ListCombo
	// 描述: 控件列表控件(LIST_COMBO)的绘制入口 —— 同上(外部一次性送入 + 控件自行拆解), 按该类型
	//       自己的行约定解析并逐行送入管道
	//       (项目行块 = INPUT_ACTION 块, 行件 = 只读输入动作件 Widgets::Base::InputActionRO;
	//        特殊行块 = CtrlComboData, 行件 = 部件 Widgets::Base::ComboAction)
	VOID ListCombo(CtrlInfo* info);

	// ================================
	// 区域: 控件绘制接口表(供布局渲染出口经 CtrlApi 调用)
	// 说明: 表内容 = 3 项能力(上下文 / 逐控件绘制 / 编辑中查询); 由宿主在 UI 设施初始化期
	//       注入布局层的渲染出口(注入点见宿主的 ImGui 单元初始化)。
	// ================================

	// 函数: GetCtrlApi
	// 描述: 取控件库的绘制接口表(进程级静态表; 重复调用幂等)
	CONST CtrlApi* GetCtrlApi();
}
