// ================================
// 文件: GenEditor.h
// 描述: GenUI 布局编辑器逻辑(多布局版)
//   术语: "外壳" = GenUI 自身界面的那个布局(GenUIEditor 布局)。
// ================================
#pragma once
#include <minwindef.h>
#include <stdint.h>		// uint64_t(事件回调四元组的 oper/param)
#include "framework/layout/Layout.h"
#include "framework/dispatch/UIEventHandler.h"	// EventBinding(事件绑定表项; 供宿主注入)

struct Document;	// 文档承载者(doc\Document; 前向声明即够 ⇒ 头层不引 app/ 头, 保持 editor → doc 单向)

// ================================
// 命名空间: GenUIEditor
// 描述: 布局编辑器静态逻辑(函数均为进程级单例语义)
// ================================
namespace GenUIEditor
{
	// 函数: SetDocument
	// 描述: 注入文档承载者(宿主在 Application::Init() 成功之后单点调用一次; NULL = 复位/未注入)
	// 说明: 本步只做接收与保存(落文件内静态 pDoc), 不改动任何既有
	// 参数:
	//		IN	pDocument:	承载者地址(宿主侧 Application::GetDocument(); NULL = 复位)
	VOID SetDocument(Document* pDocument);

	// 函数: Init
	// 描述: 初始化编辑器(注册控件选中回调 / 校验外壳锚点)
	VOID Init();

	// 函数: Update
	// 描述: 渲染前调用: 处理画布点选 + 选中有效性校验(结构件可选中 ⇒ 只校验越界与命中, 不再按结构件槽位拒绝;
	//       且不做"无选中时默认选中首行") + 选中标记维护(预览布局) + 路径栏两向同步
	VOID Update();

	// ================================
	// 区域: 提示布局(HINT)交互结论
	// 说明: 「提示布局」(内嵌(flex)成员)三事件 HINT_TITLEBAR_EVENT / HINT_OK / HINT_CANCEL 的处理结论;
	//       结论只落内存状态 + 一条诊断 —— 不移动任何 HWND、不改文档、不写盘, 也尚未驱动提示流程
	// ================================

	// 枚: HintResult
	// 描述: 「提示布局」最近一次交互结论
	enum HintResult
	{
		HINT_RESULT_NONE = 0,		// 无结论(尚未交互)
		HINT_RESULT_OK = 1,			// 确认(##BTN_OK 被点击)
		HINT_RESULT_CANCEL = 2		// 取消(##BTN_CANCEL 被点击 / 标题栏关闭钮)
	};

	// 函数: GetHintResult
	// 描述: 取「提示布局」最近一次交互结论(接入提示流程时读取)
	// 返回:
	//		INT: 结论(HintResult; 未交互 = HINT_RESULT_NONE)
	INT GetHintResult();
	// 函数: ResetHintResult
	// 描述: 清除「提示布局」交互结论(接入提示流程时在弹出提示之前调用)
	VOID ResetHintResult();

	// ================================
	// 区域: 交互辅助
	// ================================

	// 函数: ConfirmDiscardDirtyChanges
	// 描述: 有"未保存改动"时的统一提醒 —— 弹窗确认是否继续(「创建」/「打开新文件」/「关闭窗口」前调用)
	// 说明: 弹窗走宽字符 API(MessageBoxW; UTF-8 正文先转宽串) ⇒ 中文不乱码; 无改动时零弹窗
	// 参数:
	//		IN	szAction:	即将进行的动作名(文案与诊断用)
	// 返回:
	//		BOOL: TRUE = 可继续(无改动 / 用户选"是"); FALSE = 用户选"否" ⇒ 调用方取消该动作
	BOOL ConfirmDiscardDirtyChanges(LPCSTR szAction);

	// ================================
	// 区域: 事件处理函数
	// 说明: 由 UIEventHandler 的绑定表按布局文件声明的 event 名绑定。
	// ================================

	// 函数: OnTitleBarEvent
	// 描述: TITLEBAR_EVENT(编辑窗口标题栏): 按住拖动 = 把期望坐标记进编辑窗口的位置记录
	//       (外壳 WND 件的 position; 窗口由渲染出口在下次取得渲染目标时结算); 右上关闭钮点击 = 退出应用
	// 参数:
	//		IN	obj:	触发的标题控件(CtrlInfo*)
	//		IN	oper:	规范操作码(公共位 = OPER_COMMON_HOLD 按住中(电平, 拖动逐帧判据);
	//					专属段 = OPER_STRUCT_TITLE_CLOSE 请求关闭窗口)
	//		IN	param:	按住期间 = 窗口期望坐标(PackWndPos 打包; 未按住或未建立拖动 = 0)
	//		IN	data:	该控件缓冲(标题文本)
	VOID OnTitleBarEvent(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnPreviewTitleBarEvent
	// 描述: PREVIEW_TITLEBAR_EVENT(预览窗口标题栏): 按住拖动 = 把期望坐标记进预览基座 WND 件的位置记录
	//       并把新桌面位置写回该布局 wndX/wndY(窗口由渲染出口在下次取得渲染目标时结算);
	//       右上关闭钮在本事件下不生效
	// 参数:
	//		IN	obj:	触发的标题控件(CtrlInfo*)
	//		IN	oper:	规范操作码(公共位 = OPER_COMMON_HOLD 按住中(电平, 拖动逐帧判据))
	//		IN	param:	按住期间 = 窗口期望坐标(PackWndPos 打包; 未按住或未建立拖动 = 0)
	//		IN	data:	该控件缓冲(标题文本)
	VOID OnPreviewTitleBarEvent(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnHintTitleBarEvent
	// 描述: HINT_TITLEBAR_EVENT(「提示布局」标题栏; 内嵌(flex)成员, 无独立 HWND):
	//       按住中(电平) = 逐帧跟随光标移动该内嵌窗口 —— 改的是该布局窗体区(WND)的像素位置
	//       (位置基准 = 宿主编辑窗口客户区; 中间态/文档数据不动, 也不 SetWindowPos 任何 HWND);
	//       右上关闭钮点击(专属段 OPER_STRUCT_TITLE_CLOSE) = "取消"结论
	// 参数:
	//		IN	obj:	触发的标题控件(CtrlInfo*)
	//		IN	oper:	规范操作码(公共位 = OPER_COMMON_HOLD 按住中(电平);
	//					专属段 = OPER_STRUCT_TITLE_CLOSE 请求关闭提示)
	//		IN	param:	未用(事件回调四元组约定)
	//		IN	data:	该控件缓冲(标题文本; 本处理器不需要)
	VOID OnHintTitleBarEvent(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnHintOk
	// 描述: HINT_OK(「提示布局」的 ##BTN_OK 点击): 记录"确认"结论
	// 参数:
	//		IN	obj:	触发的按钮控件(CtrlInfo*; 本处理器不需要)
	//		IN	oper:	规范操作码(公共码 OPER_COMMON_CLICKED)
	//		IN	param:	未用(事件回调四元组约定)
	//		IN	data:	该控件缓冲(按钮文本; 本处理器不需要)
	VOID OnHintOk(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnHintCancel
	// 描述: HINT_CANCEL(「提示布局」的 ##BTN_CANCEL 点击): 记录"取消"结论
	// 参数:
	//		IN	obj:	触发的按钮控件(CtrlInfo*; 本处理器不需要)
	//		IN	oper:	规范操作码(公共码 OPER_COMMON_CLICKED)
	//		IN	param:	未用(事件回调四元组约定)
	//		IN	data:	该控件缓冲(按钮文本; 本处理器不需要)
	VOID OnHintCancel(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnHintKey
	// 描述: 提示展示期间的键盘结论(由宿主 WndProc 的 WM_KEYDOWN 调用):
	//       Enter = 确认 / ESC = 取消 —— 与两个按钮的点击结论同源, 差别只在结论来源
	// 说明: 严格模态期间编辑窗口不接收 ImGui 层输入(见 LayoutRender::RenderBase 的 bNoInputs),
	//       而键盘结论仍由 Win32 消息层给出(方案 B 的分工) ⇒ 本入口是宿主侧的唯一调用点;
	//       结论为覆盖式(重复调用以最后一次为准), 是否被消费由 RunHintSync 的等待循环决定
	//       (提示不在展示中时宿主不会调用它)。
	// 参数:
	//		IN	bOk:	TRUE = 确认(Enter); FALSE = 取消(ESC)
	VOID OnHintKey(BOOL bOk);
	// 函数: OnBrowse
	// 描述: GENUI_BROWSE(文件行"浏览"): 打开文件对话框选择布局文件, 选中即载入并刷新路径栏
	// 时机: 帧间事件 —— 本函数自行代理: 帧内触发只把自身挂到帧间队列并返回;
	//       帧末排空时才弹对话框并载入(载入会重建中间态池与预览基座内容控件)
	// 参数: 回调四元组不使用(显式忽略)
	VOID OnBrowse(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnCreate
	// 描述: GENUI_CREATE(文件行"创建"): 新建空白文档 —— 只创建新的中间态对象, 不生成真实文件;
	//       创建后清空路径栏(新文档尚未关联任何文件)
	// 时机: 帧间事件 —— 帧内触发只挂载; 帧末排空时才新建文档(替换中间态池并重装预览基座内容控件)
	// 参数: 回调四元组不使用(显式忽略)
	VOID OnCreate(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnOpen
	// 描述: GENUI_OPEN(文件行"打开"): 载入路径栏当前所指文件(整文档); 路径为空时转为浏览
	// 时机: 帧间事件 —— 帧内触发只挂载; 帧末排空时才载入(载入会重建文档与基座内容)
	// 参数: 回调四元组不使用(显式忽略)
	VOID OnOpen(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnSave
	// 描述: GENUI_SAVE(文件行"保存"): 整文档写盘 —— 路径栏非空 ⇒ 写该路径(工程源文档路径被拒并转另存为);
	//       路径栏为空 ⇒ 弹另存为对话框(默认名 GenUI.user.json), 成功后新路径写回路径栏
	// 时机: 帧间事件 —— 帧内触发只挂载; 帧末排空时才写盘/弹对话框(与 浏览/创建/打开 同组)
	// 参数: 回调四元组不使用(显式忽略)
	VOID OnSave(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnInputActionEvent
	// 描述: GENUI_FILEPATH(文件行"路径动作"控件 = INPUT_ACTION)的部件结论:
	//       按钮段被点击 ⇒ 浏览(弹文件对话框选文件并载入, 路径栏随之刷新);
	//       输入段提交 ⇒ 把该控件输入文本写回文档路径(路径栏两向同步随后收敛)
	// 时机: 帧间事件 —— 按钮段会弹模态对话框并载入文档
	// 参数:
	//		IN	oper:	规范操作码(独立控件的部件结论进专属段: OPER_CUSTOM_IA_INPUT_SUBMIT(输入段提交) /
	//					OPER_CUSTOM_IA_BTN_CLICKED(按钮段点击); 帧间挂载时原样透传)
	VOID OnInputActionEvent(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnExpSubmit
	// 描述: GENUI_EXP_R0..R5(经验参数区六数值格)/ GENUI_EXP_MODE(模式下拉)/ GENUI_EXP_UNIT(单位下拉)提交:
	//       数值格 = 把该格的值按该条目单位折算写回其来源布局的 autosize, 并按新参考尺寸就地
	//       更新预览基座控件几何(不做整体重装); 模式 = 写该布局的换算基准模式串并就地重解;
	//       单位 = 写该布局的参考量单位(单位只决定入参解释, 中间态存储恒千分比)
	// 时机: 帧间事件 —— 帧内触发只把"槽位序号"挂入帧间队列(槽位走 param, 操作码恒为规范码
	//       OPER_EDIT_SUBMIT; 回调四元组的 obj/data 在帧末可能失效, 故帧末按 label 重取控件与其缓冲)
	// 参数:
	//		IN	obj:	触发控件(帧内用于定位槽位序号)
	//		IN	oper:	规范操作码(帧末分派不使用; 槽位一律走 param)
	//		IN	param:	帧末执行时承载槽位序号(0..5 = 六数值格; 6 = 模式; 7 = 单位)
	//		IN	data:	该控件缓冲(帧内不读取; 帧末按 label 重取)
	VOID OnExpSubmit(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnEdSubmit
	// 描述: GENUI_ED_LBL / GENUI_ED_TEXT / GENUI_ED_EVENT / GENUI_ED_AXIS(控件编辑区四类提交):
	//       标签行 = 写来源控件的中间态元素标签; 文本行 = 写该元素的文本并刷新控件缓冲;
	//       事件行 = 写该元素的 event 名; 尺寸轴 = 只切本编辑区当前轴
	//       (轴不是文档数据 ⇒ 不改中间态, 只改镜像/系数格的取值轴)
	// 时机: 帧间事件 —— 帧内按触发控件 label 定位槽位并把槽位进 param 挂帧间队列(操作码恒为规范码
	//       OPER_EDIT_SUBMIT); 帧末排空时按 label 重取该控件与其缓冲(帧内持有的对象引用在帧末可能失效)
	// 参数:
	//		IN	obj:	触发控件(帧内用于定位槽位)
	//		IN	oper:	规范操作码(帧末分派不使用; 槽位一律走 param)
	//		IN	param:	帧末执行时承载槽位序号(0 = 标签 / 1 = 文本 / 2 = 事件 / 3 = 尺寸轴)
	//		IN	data:	该控件缓冲(帧内不读取; 帧末按 label 重取)
	VOID OnEdSubmit(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnEdPrev
	// 描述: GENUI_ED_PREV(控件编辑区前序 ID 行提交): 读该行整数缓冲, 写来源控件的中间态
	//       parentID 与布局对象控件的 parentID, 并按父链整体重推层次与有效坐标
	// 时机: 帧间事件 —— 帧内只挂帧间队列(操作码恒为规范码 OPER_EDIT_SUBMIT);
	//       帧末排空时按 label 重取该行控件与其缓冲(帧内持有的对象引用在帧末可能失效)
	// 参数:
	//		IN	obj:	触发控件(帧内不使用; 帧末按 label 重取)
	//		IN	oper:	规范操作码(帧末分派不使用)
	//		IN	param:	帧末执行时承载槽位序号(恒 ED_SLOT_PREV)
	//		IN	data:	该控件缓冲(帧内不读取)
	VOID OnEdPrev(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnEdCell
	// 描述: GENUI_ED_C0..C7 / GENUI_ED_OFS(控件编辑区九格: 8 系数格 + 像素偏移格):
	//       读该格 double 缓冲值, 写来源控件当前轴的因子系数/像素偏移,
	//       并就地重解该控件像素几何 ⇒ 预览窗口下一帧即按新几何渲染
	// 时机: 帧间事件 —— 帧内按触发控件 label 定位格号并把格号进 param 挂帧间队列(操作码恒为规范码
	//       OPER_EDIT_SUBMIT); 帧末排空时按 label 重取该控件与其缓冲(帧内持有的对象引用在帧末可能失效)
	// 参数:
	//		IN	obj:	触发控件(帧内用于定位格号)
	//		IN	oper:	规范操作码(帧末分派不使用; 格号一律走 param)
	//		IN	param:	帧末执行时承载格号(0..7 = 系数格 ##FAC_SW_EDIT..##FAC_PAD_EDIT; 9 = 像素偏移格 ##FAC_OFS_EDIT)
	//		IN	data:	该控件缓冲(帧内不读取; 帧末按 label 重取)
	VOID OnEdCell(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: OnListRowEvent
	// 描述: GENUI_LIST_LM / GENUI_LIST_CL(外壳两个列表的行事件): 把行件上报的规范操作码解释为列表意图,
	//       交意图消费者改文档(布局列表: 行激活=切 active / 部件提交=改名 / 部件按钮=删除;
	//       控件列表: 行激活=选中控件 / 部件变更=改类型 / 部件按钮=删除); 拖拽重排交重排消费者
	// 时机: 帧间事件 —— 帧内只做"来源控件是两列表之一"的准入判定, 随后把 控件指针(obj) + 规范操作码(oper)
	//       + 行号(param) 原样挂帧间队列(四元组的 obj 在帧末可能失效 ⇒ 帧末按 label 查到两列表后以
	//       指针比对定位列表归属 —— 只比较指针值, 不解引用挂载时的指针; 列表归属不编码进操作码)
	// 参数:
	//		IN	obj:	触发控件(帧内准入; 帧末用于定位列表归属)
	//		IN	oper:	规范操作码(三段编码; 帧内忽略, 帧末解码)
	//		IN	param:	帧末 = 来源行号(拖拽重排 = 源行号)
	//		IN	data:	未用(帧间挂载时传 NULL; 帧末按控件指针重取该列表缓冲)
	VOID OnListRowEvent(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: AddControlCore
	// 描述: 向 active 布局添加指定类型控件并选中(外壳"新增行"提交与行下拉按钮共用)
	BOOL AddControlCore(CtrlType type);

	// ================================
	// 区域: 事件绑定表(供宿主注入 framework\UIEventHandler)
	// ================================

	// 函数: GetEventBindings
	// 描述: 取本模块的"事件名 → 处理函数"绑定表(由宿主启动期注入 UIEventHandler::SetBindings)
	// 说明: 表为进程级常驻的静态数组(调用方只存指针、不复制); 条目数恒 > 0;
	//   注入必须早于首次装配控件(Application::Init 即解析 event 名)。
	// 参数:
	//		OUT	pnCount:	条目数(可空)
	// 返回:
	//		CONST UIEventHandler::EventBinding*: 表首址(恒非 NULL)
	CONST UIEventHandler::EventBinding* GetEventBindings(size_t* pnCount);

	// ================================
	// 区域: 预览布局内容控件点击记录
	// 说明: 「注入预览布局」= 由 app\PreviewSession::SyncPreviewLayout 装配进预览布局基座的内容控件;
	//   ① 事件绑定: 启动期经 HostContext::SetInjectClickHandler(GenUIEditor::OnPreviewClick)
	//      注入 ⇒ 装配点把每枚内容控件的 pEventHandler 一律改绑为下方记录器(覆盖其文档
	//      event 的原处理器, 不转调) —— 预览是展示面, 点控件只产生"点击记录", 不触发真实
	//      业务事件; 结构件不覆盖(预览标题栏的拖动等既有绑定保留) ② 判定范围: 由渲染出口
	//      (LayoutRender::RenderPreviewBase)对"控件库自身不派发点击"的类型统一判定并派发到同一记录器。
	//   记录 = 被点击控件的文档索引 + 像素位置 + 像素大小(载体 = stdout 日志 + 编辑器内存);
	//   预览点击 = 选择: 记录的同时把编辑器选中(编辑布局的编辑区)切到该控件, 预览布局中的
	//   选中态叠加件(SelectionOverlay)随之在下一帧画到该控件之前
	//   (预览点击不选结构件: 判定范围本就只含内容控件 ⇒ 点结构件不改变选中; 结构件只可经编辑布局/控件列表选中)。
	// ================================

	// 函数: OnPreviewClick
	// 描述: 预览布局内容控件的点击事件处理器(统一记录器): 记录被点击控件的文档索引与像素几何,
	//       打印一条 [PREVIEWCLICK] 日志(stdout), 并把编辑器选中切到该控件(预览点击 = 选择)
	// 说明: ① 两条派发来源共用本函数 —— 控件库自身派发(BUTTON / 绑事件的 TEXT/RECT)与渲染出口
	//          派发(其余类型) ⇒ 索引以"按控件指针在预览基座内反查"为准, param 仅作兜底;
	//       ② 只登记点击类操作码(提交 / 变更 / 电平类一律忽略), 结构件与选中标记控件不参与;
	//       ③ 索引口径 = 文档索引(不含预览布局的选中标记; 与控件列表行号 / 中间态元素同口径);
	//       ④ 载体 = 编辑器内存(GetPreviewClickRecord 可查) + stdout 日志(即时打印并 flush);
	//       ⑤ 记录为覆盖式(只保留最近一次); 点击只改编辑器选中与记录, 不改文档、不写盘;
	//       ⑥ 本函数在预览渲染帧内被调用 ⇒ 不改控件链(选中标记由下一帧 Update 统一维护)。
	// 参数:
	//		IN	obj:	被点击的控件(CtrlInfo*)
	//		IN	oper:	规范操作码(公共段含 OPER_COMMON_CLICKED 时才登记)
	//		IN	param:	渲染出口派发时携带的控件索引(控件库派发时为 0)
	//		IN	data:	该控件缓冲(本记录器不使用)
	VOID OnPreviewClick(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: GetPreviewClickRecord
	// 描述: 取最近一次"预览布局内容控件点击"的记录(文档索引 / 像素位置 / 像素大小)
	// 参数:
	//		OUT	pnIndex:	控件的文档索引(不含预览选中标记; 可空)
	//		OUT	pfX/pfY:	像素位置(可空)
	//		OUT	pfW/pfH:	像素大小(可空)
	// 返回:
	//		BOOL: TRUE = 已有记录; FALSE = 尚无点击记录
	BOOL GetPreviewClickRecord(INT* pnIndex, FLOAT* pfX, FLOAT* pfY, FLOAT* pfW, FLOAT* pfH);
	// 函数: ResetPreviewClickRecord
	// 描述: 清除点击记录
	VOID ResetPreviewClickRecord();
}
