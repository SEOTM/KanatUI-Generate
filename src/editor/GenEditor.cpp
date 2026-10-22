// ================================================
// 文件: GenEditor.cpp (GenUI 多布局版编辑器逻辑)
// 描述: GenUI 自身界面布局(文件行/布局管理/经验参数/控件列表/控件编辑),
//   由布局文件声明 + 控件渲染装配(LayoutRender::RenderBase)渲染。
// ================================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <new>
#include "editor/GenEditor.h"
#include "app/Application.h"
#include "app/FlexHost.h"			// 内嵌(flex)窗口装配与查询
#include "app/PreviewSession.h"		// 预览基座与帧间同步 + 外壳快照
#include "app/DocumentIO.h"			// 文档文件 I/O 与格式分派
#include "framework/dispatch/UIEventHandler.h"
#include "framework/dispatch/FrameEvent.h"
#include "editor/LayoutWriter.h"
#include "editor/EditorCommands.h"	// 编辑器命令层
#include "editor/SelectionOverlay.h"	// 预览选中态视图叠加件(链外绘制)
#include "app/HostContext.h"			// 宿主能力与窗口句柄
#include "app/SessionContracts.h"		// 会话语义契约(提示展示 / 会话面回调)
#include "common/AllocatorHub.h"		// 会话分配器
#include "common/ResourceLoader.h"		// exe 目录与路径拼接
#include "framework/layout/GeometrySolver.h"	// 几何解算与模式/基准换算
#include "doc/Document.h"	// 文档承载者
#include "doc/DocConsistency.h"	// 文档数据一致性: 保存前体检 / ID 归一
#include "doc/RawAttributes.h"	// 旁挂表族: 原类型串 / 声明文本
#include "common/FileCodec.h"
#include "framework/base/CtrlBuffer.h"		// 控件缓冲(数据结构 / 容量 / 取址 / 构造与回收)
#include "framework/base/TextWidth.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

// ================================
// 区域: 常量定义区
// ================================
constexpr FLOAT MIN_CTRL_W = 8.0f;			// 控件最小宽度
constexpr FLOAT MIN_CTRL_H = 8.0f;			// 控件最小高度
// 注: "新增类型"下拉项数 PALETTE_NUM 由控件类型信息表派生(见下方"控件类型信息表"区)。

// ================================
// 区域: 文件内静态状态
// ================================
static BOOL bInited = FALSE;					// 初始化标记
// 索引口径(选中态): nSelected = 文档索引(与中间态元素 / 旁挂表 / 控件列表行号同口径) ——
static int nSelected = -1;						// 选中控件的文档索引(-1 表示无)
static int nSynced = -2;						// 上次同步到属性区的索引(-2 强制同步)
static int nAxis = 3;							// 属性区当前编辑目标轴(0=X,1=Y,2=宽,3=高)
static int nPaletteIdx = 0;						// 列表新增类型下拉当前项
static Layout* pMain = NULL;					// active 布局缓存(Update/Commit 中刷新)
static Document* pDoc = NULL;					// 文档承载者(doc\Document; 宿主启动期注入, 见 SetDocument) —— NULL = 未注入/已复位
static CHAR szFilePath[MAX_PATH] = "";			// 当前布局文件路径(外壳文件行显示)
static CHAR szPathShown[MAX_PATH] = "";			// 路径栏上次投影到控件的文本(两向同步的比对基准)

// 布局管理 UI 状态
// 说明: szNewLayout 由两条意图共用 —— ①意图消费者 ApplyListOper 的"新增布局"分支读取;
//       ②布局列表(##LAYOUTS)行内输入框提交(改名意图)时由 OnListRowEvent 写入(行文本即新名)。
static CHAR szNewLayout[64] = "";				// 布局名输入缓冲(新增布局名 / 行内改名的新名)

// 控件列表行状态
// 说明: 控件列表(##CONTROLS)行下拉变更(改类型意图)时由 OnListRowEvent 写入(值 = 类型信息表行序),
//       由意图消费者 ApplyListOper 的"改类型"分支读取。
static INT nRowTypeIdx = 0;						// 控件行下拉选中项(类型信息表行序)

// 选中控件属性编辑缓冲(与选中索引配对同步)
// 说明: 编辑缓冲容量 = 编辑器显示缓冲容量(与 CTRL_TEXT_BUF 同值 256)。
static const size_t ED_TEXT_CAP = CTRL_TEXT_BUF;	// 编辑器文本显示缓冲容量(字节)
static CHAR szLblBuf[CTRL_TEXT_BUF] = "";		// 标签编辑缓冲
static CHAR szTextBuf[CTRL_TEXT_BUF] = "";		// 文本编辑缓冲

// EXP "镜像同步"运行时选项表与状态
constexpr INT EXP_SLOT_MODE = 6;				// 提交槽位哨兵: 模式下拉(##MODE_SEL); 0..5 = 六数值格
constexpr INT EXP_SLOT_UNIT = 7;				// 提交槽位哨兵: 单位下拉(##UNIT_SEL)
static const LPCSTR aszExpRLabel[6] = {			// 外壳六个数值格控件 label
	"##EXP_WW_EDIT", "##EXP_WH_EDIT", "##EXP_CH_EDIT", "##EXP_FS_EDIT", "##EXP_SP_EDIT", "##EXP_PAD_EDIT"
};
static LPCSTR aszExpModeName[4] = {				// 模式下拉选项表(镜像写入 combo buffer, 只读使用)
	u8"AUTO_SIZE_BY_SCREEN_WIDTH", u8"AUTO_SIZE_BY_SCREEN_HEIGHT",
	u8"AUTO_SIZE_BY_WINDOW_WIDTH", u8"AUTO_SIZE_BY_WINDOW_HEIGHT"
};
static LPCSTR aszExpUnitName[2] = {				// 单位下拉选项表(镜像写入 combo buffer, 只读使用)
	u8"千分比（比例推导像素）", u8"像素（像素推导比例）"
};
static INT anExpAuth[6] = { -1, -1, -1, -1, -1, -1 };	// 每格"权威整数值"副本(整数输入类型化后: 缓冲值==权威值才回填, -1=未建立)
static DWORD ExpRefPx(INT ratio, DWORD base);			// 参考量像素复算(EXP 镜像显示与解算同式)
//   未注入(pDoc == NULL)⇒ 显式降级: 读给缺省、写静默跳过, 不制造空态替身)
static VOID MarkDocDirtyCore();						// 置文档"未保存改动"(经承载者)
static BOOL IsDocDirtyCore();						// 文档是否有未保存改动(未注入 ⇒ FALSE)
static BOOL NormalizeDocIdsCore();					// 元素 ID 按数组序重排 + 同步活动布局链式控件 id
static DWORD ActiveIndexCore();						// 当前激活索引(未注入 ⇒ 0)
static DWORD DocCountCore();						// 文档条目数(未注入 ⇒ 0)
static LayoutEntry* ActiveEntryCore();				// 活动条目地址(未命中 ⇒ NULL)
static Layout* ActiveLayoutCore();					// 活动条目的布局对象地址(未命中 ⇒ NULL)
static CONST AutoSize& ActiveRefCore();				// 活动条目参考尺寸(未命中 ⇒ 缺省样例)
static CONST RefSize& ActiveRefSizeCore();			// 活动条目参考尺寸/经验参数(未命中 ⇒ 缺省样例)
static LPCSTR DocPathCore();						// 文档关联文件路径(未注入 ⇒ NULL)
static CONST FileMidState& DocStateCore();			// 文档中间态(未注入 ⇒ 空态)
static BOOL DocLayoutLabelCore(DWORD index, LPSTR buf, size_t cap);	// 条目显示名拷入调用方缓冲
// 元素规格族的取值 / 增删助手(含"布局→文档索引"反查)已迁 editor\EditorCommands:
//   调用点直用 EditorCommands::SpecAt / InsertSpecAt / RemoveSpecAt(逻辑一处, 不再就地实现)。

// ED "镜像同步"运行时表与状态
// (外壳释放控件由声明式渲染(LayoutRender::RenderBase)直绘, 本区表按 label 定位这些控件,
//  每帧把选中控件属性镜像进它们的缓冲)
// ----------------------------------------------------------------
// 注: 结构件(窗口/标题/容器)按"位置 + 类型"(布局前 1~3 槽)识别, 不按 label;
//     锚点控件按 label 全等比较(lstrcmpA)匹配, 由启动期 ValidateShellAnchors() 校验。
// ----------------------------------------------------------------
static const LPCSTR aszEdTypeName = "##TYPE_SEL";		// 类型值(只读镜像文本)控件 label
static const LPCSTR aszEdLblName = "##LABEL_EDIT";			// 标签行输入控件 label
static const LPCSTR aszEdTxtName = "##TEXT_EDIT";			// 文本行输入控件 label
static const LPCSTR aszEdAxisName = "##AXIS_SEL";		// 尺寸轴 Combo 控件 label
static LPCSTR aszEdAxisOpt[4] = {						// 尺寸轴下拉选项表(镜像写入 combo buffer, 只读使用)
	u8"Left", u8"Top", u8"Width", u8"Height"
};
static const LPCSTR aszEdCName[8] = {					// 8 个系数格控件 label(顺序即因子系数索引)
	"##FAC_SW_EDIT", "##FAC_SH_EDIT", "##FAC_WW_EDIT", "##FAC_WH_EDIT",
	"##FAC_CH_EDIT", "##FAC_FS_EDIT", "##FAC_SP_EDIT", "##FAC_PAD_EDIT"
};
static const LPCSTR aszEdOfsName = "##FAC_OFS_EDIT";			// 像素固定偏移输入控件 label
static const LPCSTR aszEdEvtName = "##EVT_EDIT";		// 事件名输入控件 label
static const LPCSTR aszEdPrevName = "##PREV_EDIT";		// 前序 ID 行输入控件 label
// ED 提交槽位(OnEdSubmit 的帧间 param; 帧内按触发控件 label 定位)
constexpr INT ED_SLOT_LABEL = 0;						// 标签行(##LABEL_EDIT)
constexpr INT ED_SLOT_TEXT = 1;							// 文本行(##TEXT_EDIT)
constexpr INT ED_SLOT_EVENT = 2;						// 事件行(##EVT_EDIT)
constexpr INT ED_SLOT_AXIS = 3;							// 尺寸轴 Combo(##AXIS_SEL)
constexpr INT ED_SLOT_PREV = 4;							// 前序 ID 行(##PREV_EDIT)
// ED 九格格号(OnEdCell 的帧间 param; 0..7 = 系数格, 9 = 像素偏移格)
constexpr INT ED_CELL_OFS = 9;							// 9 = 像素固定偏移格(##FAC_OFS_EDIT); 0..7 = 系数格 ##FAC_SW_EDIT..##FAC_PAD_EDIT
static CHAR aszEdLblAuth[CTRL_TEXT_BUF] = "";			// 标签行"权威文本"副本(编辑中保护)
static CHAR aszEdTxtAuth[CTRL_TEXT_BUF] = "";			// 文本行"权威文本"副本(编辑中保护)
static CHAR aszEdEvtAuth[CTRL_TEXT_BUF] = "";			// 事件行"权威文本"副本(编辑中保护)
static DOUBLE adEdCAuth[8] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };		// 系数格权威数值副本(实数类型化后)
static BOOL abEdCAuth[8] = { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE };// 系数格权威副本是否已建立
static DOUBLE dEdOfsAuth = 0.0;							// 偏移格权威数值副本(实数类型化后)
static BOOL bEdOfsAuth = FALSE;							// 偏移格权威副本是否已建立
static INT nEdPrevAuth = 0;								// 前序 ID 行权威数值副本
static BOOL bEdPrevAuth = FALSE;						// 前序 ID 行权威副本是否已建立

// ----------------------------------------------------------------
// 说明: 编辑格提交写回该格权威值的来源(镜像回填该格时记录); 来源失效(控件/布局已不在, 或文档已替换)
//   时放弃本次提交并把权威副本归一为缓冲值。数值格提交由 Controls::InputReal 的
//   ImGui::InputDouble 返回值触发, 标签行由 IsItemDeactivatedAfterEdit 触发。
// 类型: EdCellOwner
// 描述: ED 编辑格权威值来源(提交目标)
// ----------------------------------------------------------------
struct EdCellOwner
{
	CtrlInfo* pCtrl;		// 来源控件(active 布局内; NULL = 尚未建立)
	INT nIdx;				// 来源控件在 active 布局内的索引(与 pCtrl 双校验)
	INT nAxis;				// 来源尺寸轴 0..3(Left/Top/Width/Height)
};
// 类型: ExpCellOwner
// 描述: EXP 数值格权威值来源(提交目标 = 来源布局)
struct ExpCellOwner
{
	DWORD dwLayout;				// 来源布局索引
	CONST Layout* pLayout;		// 来源布局对象(与索引双校验)
};
static EdCellOwner aEdCOwner[8];		// 系数格 0..7 权威值来源
static EdCellOwner edOfsOwner;			// 偏移格权威值来源
static EdCellOwner edLblOwner;			// 标签行权威值来源
static EdCellOwner edTxtOwner;			// 文本行权威值来源
static EdCellOwner edEvtOwner;			// 事件行权威值来源
static EdCellOwner edPrevOwner;			// 前序 ID 行权威值来源
static ExpCellOwner aExpOwner[6];		// EXP 六数值格权威值来源(来源布局)

// ================================
// 区域: 提示布局(HINT)交互状态
// 说明: 「提示布局」(内嵌(flex)成员; 无独立 HWND)三事件的处理结论落点 ——
//   HINT_TITLEBAR_EVENT(标题栏: 按住 = 拖动该内嵌窗口; 关闭钮 = "取消") /
//   HINT_OK(##BTN_OK 点击 = 确认) / HINT_CANCEL(##BTN_CANCEL 点击 = 取消)。
//   结论只落内存状态与一条 [HINT] 诊断: 不移动任何 HWND、不改文档、不写盘,
//   也尚未驱动任何提示流程。
// ================================
static INT nHintResult = GenUIEditor::HINT_RESULT_NONE;	// 最近一次交互结论(HintResult; 未交互 = 无)
static BOOL bHintDragging = FALSE;				// 提示标题栏拖动中(按住首帧建立偏移, 未按住清零)
static POINT ptHintDragOfs{};					// 拖动偏移 = 光标 - 该内嵌窗口左上角(宿主客户区坐标)

// ================================
// 区域: 预览布局内容控件点击状态
// 说明: 记录器 OnPreviewClick 的落点 —— 最近一次"预览布局内容控件点击"的索引与像素几何;
//   载体 = 编辑器内存(本区) + stdout 日志(记录器内即时打印)。字段口径 = 仅
//   索引 + 位置 + 大小; 结构件(WND/TITLE/CNTR)不参与判定。
// ================================
static BOOL bPreviewClickValid = FALSE;			// 是否已有点击记录
static INT nPreviewClickIndex = -1;				// 被点击控件在布局对象中的索引(-1 = 无)
static FLOAT fPreviewClickX = 0.0f;				// 被点击控件的像素位置 X
static FLOAT fPreviewClickY = 0.0f;				// 被点击控件的像素位置 Y
static FLOAT fPreviewClickW = 0.0f;				// 被点击控件的像素宽度
static FLOAT fPreviewClickH = 0.0f;				// 被点击控件的像素高度
static CtrlInfo* pPreviewClickLastCtrl = NULL;	// 最近一次登记的控件(同帧去重用)
static DWORD dwPreviewClickLastFrame = 0;		// 最近一次登记的 ImGui 帧号(同帧裁决用)
static BOOL bPreviewClickPrint = FALSE;			// 是否待打印一条点击日志(帧末由 Update 打印 ⇒ 只打"本帧赢家")

// ================================
// 区域: 内部函数声明区
// ================================
VOID ResetSelectionLocal();						// 内部重置选中
VOID SyncEditsToSel();							// 属性编辑缓冲 <- 选中控件
VOID ApplyGeometryToSel(FLOAT x, FLOAT y, FLOAT w, FLOAT h);	// 应用几何到选中
VOID DeleteSelectedCore();						// 删除选中控件(active 布局)
VOID ClearCanvasCore();							// 清空 active 布局内容控件
LPCSTR TypeDisplayName(CtrlType type);			// 类型的中文显示名
FLOAT ActiveRefVal(INT idx);					// active 布局参考量像素(系数语义用)
static CtrlInfo* ShellFindCtrl(LPCSTR label);	// 外壳布局中按 label 找内容控件(镜像/提交用)
static VOID SyncExpShell();						// EXP 镜像同步(外壳释放控件缓冲 <- active 配置, 带编辑中保护)
static VOID SyncListShell();					// 列表行数据填充(布局列表/控件列表 <- 文档现状, 带编辑中保护)
static VOID EdSyncShell();						// ED 镜像同步(外壳释放控件缓冲 <-> 选中控件, 带编辑中保护; 四类提交均失焦)
static VOID EdApplyCoeff(INT idx);				// 系数格提交核心(值来源格失焦提交; 提交后重算几何)
static VOID EdApplyOffset();					// 像素固定偏移格提交核心(值来源格失焦提交)
static VOID EdApplyLabel();						// 标签行提交核心(写来源控件的中间态元素标签)
static VOID EdApplyText();						// 文本行提交核心(写来源控件的中间态元素文本 + 控件缓冲)
static VOID EdApplyEvent();						// 事件行提交核心(写来源控件的中间态元素事件名)
static VOID EdApplyPrev();						// 前序 ID 行提交核心(写来源控件的中间态 parentID 并按父链重推)
static VOID EdApplyAxis();						// 尺寸轴提交核心(切换本编辑区当前编辑轴, 不改文档)
static ElementSpec* EdElSpecOf(CtrlInfo* sel, INT idx);	// 取来源控件对应的中间态元素规格(NULL = 无)
static VOID EdRecalcSelGeometry(CtrlInfo* sel);	// 选中控件几何 <- 按轴向系数重算
static CtrlInfo* EdSelCtrl();					// 取当前选中控件(越界/无选中返回 NULL)
static VOID EdOwnerSet(EdCellOwner& owner, CtrlInfo* sel, INT idx, INT axis);	// 记录编辑格来源
static CtrlInfo* EdOwnerCtrl(CONST EdCellOwner& owner);	// 校验并取来源控件(NULL = 失效, 放弃提交)
static INT EdOwnerAxis(CONST EdCellOwner& owner);		// 取来源尺寸轴(越界回退当前轴)
static char* EdShellTxtBuf(LPCSTR label, size_t* pCap);	// 取外壳释放控件的文本缓冲
// ---- 文件行: 文档载入与路径栏同步 ----
static BOOL OpenDocumentByPath(LPCSTR path);	// 载入整份文档并刷新路径栏(浏览/打开共用)
static VOID SyncFilePathShell();				// 路径栏两向同步(用户输入 -> 文档路径 / 文档路径 -> 控件)
static CtrlInfo* ShellFindPathCtrl();			// 路径控件(优先 INPUT_ACTION, 回退 INPUT_SINGLE)
static BOOL CheckSaveDataInteractive();			// 保存前数据体检(Label 重名 / ID 不连续) ⇒ 提示定夺
// ---- 列表交互意图协议(控件事件处理器解释操作码 → 改文档) ----
static ListBuffer* ListDataOf(CtrlInfo* pListCtrl);			// 取列表控件的缓冲头(无则 NULL)
static BOOL ApplyListOper(CtrlType type, DWORD dwOper);		// 解释意图并改文档(不含绘制)
static BOOL ApplyListMove(CtrlType type, DWORD dwSrc, DWORD dwDest);	// 解释拖拽重排意图并改文档
static DWORD ListIntentOper(DWORD dwRow, DWORD dwIntent);	// 打包"行号 + 编辑器意图"(仅本文件内使用)
static DWORD ListIntentRow(DWORD dwOper);					// 取编辑意图操作码中的行号
static DWORD ListIntentOf(DWORD dwOper);					// 取编辑意图操作码中的意图
// ---- 提示布局(HINT)三事件(处理器 + 结论记录) ----
static VOID SetHintResult(INT nResult, LPCSTR szSource);	// 记录提示结论并打一条 [HINT] 诊断
static Layout* FindFlexTitleLayout(VOID* obj);				// 由触发的标题控件反查所属内嵌(flex)布局

// ================================
// 区域: 内部工具函数实现区
// ================================

// 说明: 类型语义(是否文本缓冲控件等)经 CtrlBuffer.h 的属性查询(CtrlTypeIsTextKind 等)。
// ================================

// 操作码: 编辑族提交的规范操作码 = OperCode::MakeCodeOwn(OPER_EDIT_SUBMIT)
//       (拼装统一走 ControlSpec.h 的 OperCode 命名空间, 本文件不再自带拼装工具)
// 描述: 编辑族控件(INPUT_SINGLE / INPUT_REAL / INPUT_NUMERIC / COMBO)提交的规范操作码
// 说明: 帧间挂载时它只承担"控件来源"标记(三段至少一段非 0 ⇒ 非 OPER_NONE)与"编辑提交"结论;
//       槽位号 / 格号等业务数据一律走回调 param ⇒ 帧末分派不得依赖 oper(见 OnExpSubmit /
//       OnEdSubmit / OnEdCell 的时机说明)。与 FrameEvent::Post 的合并判据(handler + obj + oper +
//       param + data)配合: 同一帧内不同槽位的 param 不同 ⇒ 各挂一枚, 不会被误合并。
// 备注: 三个处理器内的下拉槽位(##MODE_SEL / ##UNIT_SEL / ##AXIS_SEL)按控件库约定上报的是
//       OPER_EDIT_CHANGED(值变更), 挂载处统一挂"提交"规范码 —— 帧末只按 param 分派且不解读该码
//       ⇒ 该差异不可观测; 若将来有消费者要读此码, 应改为按控件类型分派 SUBMIT / CHANGED。
// 段值: 公共段 = 无旗标且未搭载 · 专属段 = OPER_EDIT_SUBMIT · 搭载段 = 0
// 等价性: ControlSpec.h 的 static_assert 已固定 MakeCodeOwn(OPER_EDIT_SUBMIT) 与
//       MakeCode(MakeCommon(OPER_COMMON_NONE, OPER_PAYLOAD_NONE), OPER_EDIT_SUBMIT, 0) 等价 ⇒ 语义不变。

// ================================
// 区域: 控件类型信息表
// "类型 → 显示名 / 默认文本 / 新增下拉"由 CtrlTypeInfo 表承载
//       (nPalette 决定是否进入"新增类型"下拉及其顺序, 不依赖表内行序)。
// ================================

// 结构: CtrlTypeInfo
// 描述: 控件类型的编辑器侧信息
struct CtrlTypeInfo
{
	CtrlType type;		// 控件类型
	LPCSTR szName;		// 中文显示名(列表"类型"列 / 属性展示 / 新增下拉项文案共用)
	LPCSTR szDefText;	// 新增该类型控件时的默认文本
	INT nPalette;		// "新增类型"下拉序号(0 起顺序; -1 = 不进入下拉)
};

// 表: aCtrlTypeInfo
// 描述: 控件类型信息表(未收录类型由查询函数回退"未知"/空文本)
// nPalette 取值: -1 = 不进入"新增类型"下拉 —— WND / CNTR(默认添加且不可删除的结构件);
//       其余按追加顺序 0..N-1(LIST / TIMEBAR / RENDER / LIST_TIMESPAN 自 CL-4a 起放回下拉,
//       序号追加在既有项之后以保持既有下拉项顺序不变)。
static constexpr CtrlTypeInfo aCtrlTypeInfo[] = {
	{ CTRL_TYPE_WND,			u8"窗口",		"",			-1 },	// 结构件: 默认添加/不可删 ⇒ 不入下拉
	{ CTRL_TYPE_TITLE,			u8"标题",		u8"标题",	12 },	// 标题条(默认文本非空 ⇒ 加出来可见)
	{ CTRL_TYPE_CNTR,			u8"区域",		"",			-1 },	// 结构件: 默认添加/不可删 ⇒ 不入下拉
	{ CTRL_TYPE_RECT,			u8"矩形",		"",			8 },
	{ CTRL_TYPE_TEXT,			u8"文本",		u8"文本控件",	0 },
	{ CTRL_TYPE_BUTTON,			u8"按钮",		u8"按钮",		1 },
	{ CTRL_TYPE_INPUT_SINGLE,	u8"单行输入",	"",			2 },
	{ CTRL_TYPE_INPUT_REAL,		u8"实数输入",	"",			3 },
	{ CTRL_TYPE_INPUT_NUMERIC,	u8"整数输入",	"",			4 },
	{ CTRL_TYPE_INPUT_MULTI,	u8"多行输入",	"",			9 },	// 类型/缓冲/编辑可用; 渲染层不产出像素(见 Controls.cpp 分派表注释)
	{ CTRL_TYPE_INPUT_ACTION,	u8"输入动作",	u8"按钮",	13 },	// 无边框单行输入 + 右侧按钮; 引擎建组合缓冲(CtrlInputActionData)
	{ CTRL_TYPE_COMBO,			u8"组合控件",	u8"选项 1",	5 },
	{ CTRL_TYPE_COMBO_ACTION,	u8"下拉动作",	u8"选项 1",	14 },	// 无边框下拉框 + 右侧按钮; 选项表取控件缓冲
	{ CTRL_TYPE_SLIDER,			u8"滑块",		"",			6 },
	{ CTRL_TYPE_PROG,			u8"进度状态",	"",			7 },
	{ CTRL_TYPE_LIST,			u8"列表",		"",			17 },	// 无绘制实现 ⇒ 新增后有控件、无像素(见 Controls.cpp 分派表注释)
	{ CTRL_TYPE_LIST_INPUT,		u8"输入列表",	"",			10 },	// 行数据由列表缓冲/内容消费者提供
	{ CTRL_TYPE_LIST_COMBO,		u8"下拉列表",	"",			11 },	// 同上
	{ CTRL_TYPE_TIMEBAR,		u8"时间条",		"",			18 },	// 无缓冲语义 ⇒ 按 0 值绘制(新增后显示 00:00:00.000 - 00:00:00.000)
	{ CTRL_TYPE_RENDER,			u8"呈现器",		"",			19 },	// 纹理由宿主注入; 无图时画占位面 ⇒ 新增后有像素
	{ CTRL_TYPE_CHECKBOX,		u8"复选框",		u8"复选框",	15 },
	{ CTRL_TYPE_LIST_TIMESPAN,	u8"时段列表",	"",			16 },	// 无缓冲语义与绘制实现 ⇒ 新增后有控件、无像素
};
constexpr size_t CTRL_TYPE_INFO_NUM = sizeof(aCtrlTypeInfo) / sizeof(aCtrlTypeInfo[0]);

// 函数: CtrlTypePaletteNum
// 描述: 统计"进入新增下拉"的类型数(nPalette >= 0; PALETTE_NUM 由此派生)
// 返回: INT 下拉项数
constexpr INT CtrlTypePaletteNum()
{
	INT nRet = 0;
	for (size_t i = 0; i < CTRL_TYPE_INFO_NUM; i++)
	{
		if (aCtrlTypeInfo[i].nPalette >= 0)
		{
			nRet++;
		}
	}
	return nRet;
}
// 常量: PALETTE_NUM
// 描述: "新增类型"下拉项数(由控件类型信息表派生)
constexpr INT PALETTE_NUM = CtrlTypePaletteNum();

// 函数: FindCtrlTypeInfo
// 描述: 按控件类型查信息表
// 参数:
//		IN	type:	控件类型
// 返回:
//		const CtrlTypeInfo*: 命中的表项指针; 未收录返回 NULL
static const CtrlTypeInfo* FindCtrlTypeInfo(CtrlType type)
{
	for (size_t i = 0; i < CTRL_TYPE_INFO_NUM; i++)
	{
		if (aCtrlTypeInfo[i].type == type)
		{
			return &aCtrlTypeInfo[i];
		}
	}
	return NULL;
}
// 函数: PaletteType
// 描述: 取"新增类型"下拉第 idx 项对应的控件类型
// 参数:
//		IN	idx:	下拉序号(0 起)
// 返回:
//		CtrlType: 命中的类型; 越界/未收录返回 CTRL_TYPE_INVALID
static CtrlType PaletteType(INT idx)
{
	for (size_t i = 0; i < CTRL_TYPE_INFO_NUM; i++)
	{
		if (aCtrlTypeInfo[i].nPalette == idx)
		{
			return aCtrlTypeInfo[i].type;
		}
	}
	return CTRL_TYPE_INVALID;
}
// 函数: TypeDisplayName
// 描述: 控件类型中文显示名(供编辑器列表与属性展示; 取自类型信息表)
LPCSTR TypeDisplayName(CtrlType type)
{
	const CtrlTypeInfo* p = FindCtrlTypeInfo(type);
	return (p != NULL) ? p->szName : u8"未知";
}
// 函数: TypeInfoIndexOf
// 描述: 取控件类型在编辑器类型信息表内的行序(控件列表行下拉的选项表即本表 ⇒ 索引与类型互算; 未收录返回 -1)
static INT TypeInfoIndexOf(CtrlType type)
{
	const CtrlTypeInfo* p = FindCtrlTypeInfo(type);
	return (p != NULL) ? (INT)(p - aCtrlTypeInfo) : -1;
}
// 函数: GetDefaultCtrlText
// 描述: 取控件类型的默认文本(新增控件时写入其文本缓冲; 取自类型信息表)
static LPCSTR GetDefaultCtrlText(CtrlType type)
{
	const CtrlTypeInfo* p = FindCtrlTypeInfo(type);
	return (p != NULL) ? p->szDefText : "";
}
// 函数: ActiveRefVal
// 描述: 取 active 布局参考量(0..7 = 屏宽/屏高/窗宽/窗高/控件高/字号/间隙/内距)像素
FLOAT ActiveRefVal(INT idx)
{
	AutoSize::SizeInfo<FLOAT> ref = ActiveRefCore().GetSizeInfo<FLOAT>();
	const FLOAT aRef[GENFACTOR_NUM] = {
		ref.screenWidth, ref.screenHeight, ref.windowWidth, ref.windowHeight,
		ref.controlHeight, ref.fontSize, ref.space, ref.padding
	};
	if (idx >= 0 && idx < GENFACTOR_NUM)
	{
		return aRef[idx];
	}
	return 0.0f;
}
// 函数: CtrlIndexOf
// 描述: 按控件指针在指定布局内反查控件索引(未命中返回 -1)
// 参数:
//		IN	layout:	目标布局对象
//		IN	ctrl:	控件指针
// 返回: INT 控件索引(0 基); 布局或控件为空 / 未命中返回 -1
static INT CtrlIndexOf(Layout* layout, CONST CtrlInfo* ctrl)
{
	INT nRet = -1;
	if (layout == NULL || ctrl == NULL)
	{
		return nRet;
	}
	for (size_t i = 0; i < layout->GetTotalNum(); i++)
	{
		if (layout->GetCtrlInfo(i) == ctrl)
		{
			nRet = (INT)i;
			break;
		}
	}
	return nRet;
}
// 函数: DocCtrlCount
// 描述: 取 active 布局的控件数(布局为空 ⇒ 0)
// 说明: 编辑器全部索引口径 = 文档索引(中间态元素 / 旁挂表 / 控件列表行号同口径);
static size_t DocCtrlCount()
{
	return (pMain != NULL) ? pMain->GetTotalNum() : 0;
}
// 函数: CtrlOfDocIndex
// 描述: 文档索引 → 控件指针(active 布局; 越界返回 NULL)
// 说明: 链内只含文档控件 ⇒ 两套索引已合一, 直接按下标取。
static CtrlInfo* CtrlOfDocIndex(Layout* layout, INT docIndex)
{
	if (layout == NULL || docIndex < 0)
	{
		return NULL;
	}
	return layout->GetCtrlInfo((size_t)docIndex);
}
// 函数: DocIndexOfCtrl
// 描述: 控件指针 → 文档索引(active 布局; 未命中 ⇒ -1)
// 说明: 同 CtrlOfDocIndex —— 命中索引即文档索引。
static INT DocIndexOfCtrl(Layout* layout, CONST CtrlInfo* ctrl)
{
	if (layout == NULL || ctrl == NULL)
	{
		return -1;
	}
	return CtrlIndexOf(layout, ctrl);
}
// 函数: SelCtrlOf
// 描述: 取当前选中控件(active 布局; 无选中 / 索引失效 / 未命中返回 NULL)
// 说明: 结构件可选中: 选中态可落在任意文档控件上(含结构件; 结构件仅不画预览叠加件)
//       ⇒ 此处不再按结构件槽位拒绝。
static CtrlInfo* SelCtrlOf()
{
	if (pMain == NULL || nSelected < 0)
	{
		return NULL;
	}
	return CtrlOfDocIndex(pMain, nSelected);
}
// 函数: AxisFactorOf
// 描述: 取控件指定轴向的几何系数(来源 = 中间态元素规格, 按文档索引; 元素缺失 ⇒ NULL)
static SizeFactor* AxisFactorOf(CtrlInfo* ctrl, int axis)
{
	if (ctrl == NULL || pMain == NULL || axis < 0 || axis > 3)
	{
		return NULL;
	}
	CONST INT index = DocIndexOfCtrl(pMain, ctrl);
	if (index < 0)
	{
		return NULL;
	}
	ElementSpec* pSpec = EditorCommands::SpecAt(pMain, (size_t)index);
	if (pSpec == NULL)
	{
		return NULL;
	}
	switch (axis)
	{
	case 0: return &pSpec->x;
	case 1: return &pSpec->y;
	case 2: return &pSpec->width;
	case 3: return &pSpec->height;
	default: return NULL;
	}
}
// 函数: ApplyGeometryToSel
// 描述: 应用几何属性到当前选中控件(带边界约束)
VOID ApplyGeometryToSel(FLOAT x, FLOAT y, FLOAT w, FLOAT h)
{
	if (nSelected < 0 || pMain == NULL)
	{
		return;
	}
	CtrlInfo* sel = SelCtrlOf();
	if (sel == NULL)
	{
		return;
	}
	if (w < MIN_CTRL_W) { w = MIN_CTRL_W; }
	if (h < MIN_CTRL_H) { h = MIN_CTRL_H; }
	if (x < 0) { x = 0; }
	if (y < 0) { y = 0; }
	// 画布范围约束(窗口内)
	CtrlInfo* wnd = pMain->GetLayoutBase(CTRL_TYPE_WND);
	if (wnd)
	{
		if (x + w > wnd->size.x) { x = wnd->size.x - w; }
		if (y + h > wnd->size.y) { y = wnd->size.y - h; }
		if (x < 0) { x = 0; }
		if (y < 0) { y = 0; }
	}
	sel->position.x = x;
	sel->position.y = y;
	sel->size.x = w;
	sel->size.y = h;
	// 注: 几何系数不驻控件: 像素级编辑只改像素几何, 不回写系数。
}
// 函数: CanDeleteCtrl
// 描述: 该控件是否可经 UI 删除
// 说明: 不可删者只有必需结构件: 窗体区(WND)与内容区(CNTR) —— 删掉整个布局就没有承载;
//       其余控件一律可删 —— 含"列表"类型族(LIST_*)与 label 为 ##LAYOUTS / ##CONTROLS
//       的列表(label 是控件数据, 不作为删除保护判据)。
// 参数:
//		IN	ctrl:	控件信息(可为 NULL; NULL ⇒ 不可删)
// 返回:
//		BOOL: TRUE = 可删; FALSE = 必需结构件
static BOOL CanDeleteCtrl(CONST CtrlInfo* ctrl)
{
	if (ctrl == NULL)
	{
		return FALSE;
	}
	if (ctrl->type == CTRL_TYPE_WND || ctrl->type == CTRL_TYPE_CNTR)
	{
		return FALSE;		// 必需结构件
	}
	return TRUE;
}
// 函数: DeleteCtrlAtDocIndex
// 描述: 删除 active 布局内指定文档索引的控件(元素/控件链/旁挂表三处同步删除), 并复位选中
// 说明: ① 结构件按 CanDeleteCtrl 判定(必需结构件 WND/CNTR 拒删; TITLE 可删);
//       ② 链内只含文档控件 ⇒ 无须"先摘标记"
//          文档索引即布局控件索引;
//       ③ 删除后不保留被删控件的选中态(选中落空 ⇒ 无选中)。
// 参数:
//		IN	docIndex:	目标控件的文档索引
// 返回:
static BOOL DeleteCtrlAtDocIndex(INT docIndex)
{
	BOOL bRet = FALSE;
	if (pMain == NULL || docIndex < 0 || (size_t)docIndex >= DocCtrlCount())
	{
		return bRet;
	}
	{
		CtrlInfo* pTarget = CtrlOfDocIndex(pMain, docIndex);
		if (pTarget == NULL || CanDeleteCtrl(pTarget) == FALSE)
		{
			return bRet;				// 必需结构件不可删(可删性由 CanDeleteCtrl 判定; 与"结构件可选中"无关)
		}
	}
	CONST size_t qwCtrl = (size_t)docIndex;		// 文档索引 == 布局控件索引
	CtrlBuffer::FreeCtrlBuffer(pMain->GetCtrlInfo(qwCtrl));	// 删除前回收该控件的自身缓冲
	EditorCommands::RemoveSpecAt(pMain, (size_t)docIndex);	// 中间态元素与控件按同一索引同步移除
	pMain->RemoveCtrlInfo(qwCtrl);
	RawAttributes::RemoveRawTypeAt(pMain, (size_t)docIndex);
	RawAttributes::RemoveRawTextAt(pMain, (size_t)docIndex);
	PreviewSession::RequestPreviewSync();	// 改链式结构(删除控件) ⇒ 统一走帧末同步
	// 选中落空 ⇒ 无选中(不保留被删控件的选中态); 叠加件随选中索引失效自动不绘制
	nSelected = -1;
	nSynced = -2;
	bRet = TRUE;
	return bRet;
}
// 函数: DeleteSelectedCore
// 描述: 删除当前选中的控件(active 布局; 必需结构件 WND/CNTR 由 CanDeleteCtrl 拒绝)
// 说明: 选中态可落在任意文档控件上(含结构件) ⇒ 本函数只做"按当前选中的文档索引删除"
VOID DeleteSelectedCore()
{
	if (pMain == NULL || nSelected < 0)
	{
		return;
	}
	DeleteCtrlAtDocIndex(nSelected);
}
// 函数: ClearCanvasCore
// 描述: 清空 active 布局全部内容控件(仅保留结构件)
VOID ClearCanvasCore()
{
	if (pMain == NULL)
	{
		return;
	}
	// 链内只含文档控件 ⇒ 无须先摘除任何附加件; 文档索引 == 布局控件索引
	while (DocCtrlCount() > pMain->GetShellNum())
	{
		CONST size_t nDoc = DocCtrlCount() - 1;		// 文档索引 = 末尾内容控件(== 布局控件索引)
		CtrlBuffer::FreeCtrlBuffer(pMain->GetCtrlInfo(nDoc));	// 删除前回收该控件的自身缓冲
		EditorCommands::RemoveSpecAt(pMain, nDoc);	// 中间态元素与控件按同一索引同步移除
		pMain->RemoveCtrlInfo(nDoc);
		RawAttributes::RemoveRawTypeAt(pMain, nDoc);
		RawAttributes::RemoveRawTextAt(pMain, nDoc);
	}
	PreviewSession::RequestPreviewSync();	// 改链式结构(清空内容控件) ⇒ 统一走帧末同步
	nSelected = -1;
	nSynced = -2;
}
// 函数: ResetSelectionLocal
// 描述: 内部重置选中(不改动改名态, 改名提交由输入框失焦/回车处理)
VOID ResetSelectionLocal()
{
	nSelected = -1;
	nSynced = -2;
}
// 函数: SyncEditsToSel
// 描述: 属性编辑缓冲 <- 选中控件(选择变化时刷新一次)
VOID SyncEditsToSel()
{
	if (nSelected < 0 || pMain == NULL)
	{
		szLblBuf[0] = '\0';
		szTextBuf[0] = '\0';
		return;
	}
	CtrlInfo* sel = SelCtrlOf();
	if (sel == NULL)
	{
		return;
	}
	strncpy_s(szLblBuf, CTRL_TEXT_BUF, (sel->label ? sel->label : ""), _TRUNCATE);
	// 文本 = 控件缓冲自身(CtrlBuffer.h 的 CtrlTextBuffer 单点取址)
	{
		CHAR* lpText = CtrlTextBuffer(sel);
		strncpy_s(szTextBuf, sizeof(szTextBuf), (lpText != NULL) ? lpText : "", _TRUNCATE);
	}
}
// ================================
// 区域: 公开接口实现区
// ================================

// ================================
// 区域: 外壳锚点表 + 启动期校验
// 说明: EXP/ED 镜像桥与文件行同步按 label 匹配外壳控件(ShellFindCtrl), label 或类型不符时
//       对应编辑格在运行期静默失效; 启动期校验一次并打印未命中项。
// ================================

// 表: aShellAnchor
// 描述: 外壳布局锚点控件表(label → 期望类型); 启动期校验用
static const struct ShellAnchorSpec
{
	LPCSTR szLbl;		// 锚点 label(与 GenUI.json 中逐字一致)
	CtrlType type;		// 期望控件类型(决定缓冲布局与镜像桥解释方式)
} aShellAnchor[] = {
	{ "##MODE_SEL", CTRL_TYPE_COMBO },			// 模式下拉
	{ "##UNIT_SEL", CTRL_TYPE_COMBO },			// 单位下拉
	{ "##EXP_PARAM_RECT", CTRL_TYPE_RECT },		// 参数 RECT
	{ "##EXP_WW_EDIT", CTRL_TYPE_INPUT_NUMERIC },	// 六参数格首格
	{ "##EXP_PAD_EDIT", CTRL_TYPE_INPUT_NUMERIC },	// 六参数格末格
	{ "##FAC_PARAM_RECT", CTRL_TYPE_RECT },			// 系数 RECT
	{ "##TYPE_SEL", CTRL_TYPE_TEXT },			// 类型只读镜像
	{ "##LABEL_EDIT", CTRL_TYPE_INPUT_SINGLE },		// 标签行输入
	{ "##AXIS_SEL", CTRL_TYPE_COMBO },			// 尺寸轴下拉
	{ "##FAC_SW_EDIT", CTRL_TYPE_INPUT_REAL },		// 8 系数格首格
	{ "##FAC_PAD_EDIT", CTRL_TYPE_INPUT_REAL },		// 8 系数格末格
	{ "##FAC_OFS_EDIT", CTRL_TYPE_INPUT_REAL },		// 像素偏移格
	{ "##PREV_EDIT", CTRL_TYPE_INPUT_NUMERIC },		// 前序 ID 行数值输入
};
constexpr size_t SHELL_ANCHOR_NUM = sizeof(aShellAnchor) / sizeof(aShellAnchor[0]);

// 函数: ValidateShellAnchors
// 描述: 校验外壳布局锚点控件是否存在且类型相符(启动期调用一次; 只在发现问题时打印)
// 参数:
//		IN	bVerboseOK:	TRUE = 全部通过时也打印一行(供人工诊断)
// 返回:
//		BOOL: TRUE = 锚点全部就绪(外壳快照无效时直接返回 TRUE)
static BOOL ValidateShellAnchors(BOOL bVerboseOK)
{
	if (PreviewSession::GetShellValid() == FALSE)
	{
		if (bVerboseOK)
		{
			printf("[ANCHOR] 外壳快照无效: 跳过锚点契约校验\n");
			fflush(stdout);
		}
		return TRUE;
	}
	Layout& shell = PreviewSession::GetShellLayout();
	BOOL bRet = TRUE;
	size_t nBad = 0;
	for (size_t a = 0; a < SHELL_ANCHOR_NUM; a++)
	{
		CtrlInfo* pHit = NULL;
		for (size_t i = shell.GetShellNum(); i < shell.GetTotalNum(); i++)
		{
			CtrlInfo* c = shell.GetCtrlInfo(i);
			if (c && c->label && lstrcmpA(c->label, aShellAnchor[a].szLbl) == 0)
			{
				pHit = c;
				break;
			}
		}
		if (pHit == NULL)
		{
			printf("[ANCHOR] 外壳锚点缺失: label=%s (期望类型 %s) —— 对应编辑入口将在运行期静默失效\n",
				aShellAnchor[a].szLbl, GetTypeName(aShellAnchor[a].type));
			bRet = FALSE;
			nBad++;
		}
		else if (pHit->type != aShellAnchor[a].type)
		{
			printf("[ANCHOR] 外壳锚点类型不符: label=%s 期望=%s 实际=%s —— 缓冲布局可能被静默误解\n",
				aShellAnchor[a].szLbl, GetTypeName(aShellAnchor[a].type), GetTypeName(pHit->type));
			bRet = FALSE;
			nBad++;
		}
	}
	// 注: 标题栏的"事件声明 → 绑定"不在校验面内(校验面 = 结构件模板本身)
	if (bRet == FALSE)
	{
		printf("[ANCHOR] 外壳锚点校验失败: %zu 项异常\n", nBad);
		fflush(stdout);
	}
	else if (bVerboseOK)
	{
		printf("[ANCHOR] 外壳锚点校验通过(%zu 项)\n", SHELL_ANCHOR_NUM);
		fflush(stdout);
	}
	return bRet;
}

// 函数: SetDocument
// 描述: 注入文档承载者(宿主在应用主类 Init() 成功之后单点调用一次; NULL = 复位/未注入)
// 说明: 本步只做接收与保存(落文件内静态 pDoc), 不改动任何既有
//   Application 调用点 ⇒ 新通路建好但尚未被使用, 编辑器行为与注入前逐字不变。
//   时序与既有三处注入相反: 承载者内嵌在应用侧成员块 AppData 内, 而 AppData 到 Init() 内部
//   才分配 ⇒ Init 之前取不到承载者 ⇒ 注入必须晚于 Init()
//   (与 UIEventHandler::SetBindings / HostContext::SetInjectClickHandler / HostContext::SetWindowHandle
//   的"早于 Init"时机相反, 不得按维护惯例提前), 并在 Release() 之前以 NULL 复位
//   (否则承载者随 AppData 失效 ⇒ 编辑器侧留悬垂指针)。
//   NULL = 未注入 ⇒ 编辑器侧消费者须按"未注入 ⇒ 显式降级"处置, 不得解引用
// 参数:
//		IN	pDocument:	承载者地址(宿主侧对 data 内的承载者取址; NULL = 复位)
VOID GenUIEditor::SetDocument(Document* pDocument)
{
	pDoc = pDocument;
	EditorCommands::SetDocument(pDocument);		// 命令层同一承载者
}
// 函数: Init
// 描述: 初始化编辑器(启动期校验外壳锚点)
VOID GenUIEditor::Init()
{
	if (bInited)
	{
		return;
	}
	// 启动期校验外壳锚点(label 或类型不符时对应编辑格在运行期静默失效)
	ValidateShellAnchors(FALSE);
	bInited = TRUE;
}
// 函数: Update
// 描述: 渲染前调用: 处理画布点选 + 选中有效性校验
VOID GenUIEditor::Update()
{
	pMain = ActiveLayoutCore();
	if (!bInited)
	{
		Init();
	}
	// 预览布局点击记录: 打印上一条已定稿的记录(点击可能在预览帧的任意时刻派发 ⇒ 到本帧渲染前已无
	// 同帧更上层者 ⇒ 只打"赢家"一条; 字段 = 索引 + 像素位置 + 像素大小)
	if (bPreviewClickPrint)
	{
		bPreviewClickPrint = FALSE;
		printf("[PREVIEWCLICK] 点击: 索引=%d 位置=(%.1f, %.1f) 大小=(%.1f x %.1f)\n",
			nPreviewClickIndex, fPreviewClickX, fPreviewClickY, fPreviewClickW, fPreviewClickH);
		fflush(stdout);
	}
	// 外壳由 GenUIEditor 布局 JSON 声明式渲染 ⇒ 文件行路径动作控件(输入动作 ##FILE_PATH)
	// 与当前文档路径两向同步(用户可编辑: 输入段提交即写回文档路径; 路径动作钮/打开/创建改路径即回填控件);
	// 同路径追加 EXP 镜像同步(active 配置 -> 外壳释放的模式/单位/六数值格缓冲, 带编辑中保护)
	if (PreviewSession::GetShellValid())
	{
		SyncFilePathShell();	// 路径栏两向同步(渲染前, 幂等)
		SyncExpShell();		// EXP 镜像同步(渲染前, 幂等)
		EdSyncShell();		// ED 镜像同步(选中控件属性 -> 外壳释放控件缓冲; 文本实时写回)
		SyncListShell();	// 列表行数据填充(布局列表/控件列表 <- 文档现状)
	}
	// 0. 文档结构变化(增删/载入)后选中索引可能越界, 由下方"校验选中有效性"统一处理
	// 1. 校验选中有效性(索引口径: 上限 = 布局控件数 —— 链内只含文档控件, 两套索引已合一)
	//    结构件可选中: 选中落到结构件槽位不再视为失效 ⇒ 只校验越界与命中
	if (nSelected >= 0)
	{
		if ((size_t)nSelected >= DocCtrlCount() ||
			CtrlOfDocIndex(pMain, nSelected) == NULL)
		{
			nSelected = -1;
			nSynced = -2;
		}
	}
	// 2. 不做"无选中时默认选中首行"
	//    3. 选择变化时同步属性编辑缓冲(无选中 ⇒ 编辑区按"(未选中)"清空, 见 EdSyncShell)
	if (nSynced != nSelected)
	{
		SyncEditsToSel();
		nSynced = nSelected;
	}
	// 5. 预览选中态表达件: 把编辑器选中态交给链外的视图叠加件(SelectionOverlay) ——
	//    它按当前选中索引在预览渲染帧内就地绘制(位置 = 被选中控件之前, 几何 = 外扩一枚参考内距 padding);
	//    无选中 / 结构件 ⇒ 不绘制。叠加件不在控件链上 ⇒ 无须挂载/摘除/重建维护, 每帧幂等。
	SelectionOverlay::SetSelectedIndex(nSelected);
}
// 函数: GetHintResult
// 描述: 取「提示布局」最近一次交互结论
// 返回:
//		INT: 结论(HintResult; 未交互 = HINT_RESULT_NONE)
INT GenUIEditor::GetHintResult()
{
	return nHintResult;
}
// 函数: ResetHintResult
// 描述: 清除「提示布局」交互结论(接入提示流程时在弹出提示之前调用)
VOID GenUIEditor::ResetHintResult()
{
	nHintResult = HINT_RESULT_NONE;
	bHintDragging = FALSE;
}

// ================================
// 区域: 事件绑定表(供宿主注入 framework\UIEventHandler)
// ================================
static CONST UIEventHandler::EventBinding aEventBindings[] =
{
	// ---- 标题栏事件: 编辑窗口 = 移动 + 关闭 / 预览窗口 = 只移动 ----
	{ "TITLEBAR_EVENT",			GenUIEditor::OnTitleBarEvent,		'\0', '\0' },
	{ "PREVIEW_TITLEBAR_EVENT",	GenUIEditor::OnPreviewTitleBarEvent,	'\0', '\0' },
	// ---- 文件行: 浏览 / 创建 / 打开 / 保存(四项均为帧间事件; 处理器内部自行挂帧间队列) ----
	// 注: "创建"只新建中间态对象, 不生成真实文件; "保存" = 整文档写盘(工程源文档路径被拒)。
	{ "GENUI_BROWSE",			GenUIEditor::OnBrowse,				'\0', '\0' },
	{ "GENUI_CREATE",			GenUIEditor::OnCreate,				'\0', '\0' },
	{ "GENUI_OPEN",				GenUIEditor::OnOpen,				'\0', '\0' },
	{ "GENUI_SAVE",				GenUIEditor::OnSave,				'\0', '\0' },
	// 路径动作控件(INPUT_ACTION; 文件行"路径框 + 按钮"二合一): 按钮段 = 浏览 / 输入段提交 = 写回路径
	{ "GENUI_FILEPATH",			GenUIEditor::OnInputActionEvent,	'\0', '\0' },
	// ---- 经验参数区(EXP): 模式下拉 / 单位下拉 / 六数值格 ----
	// 注: 三者共用同一处理器(格身份由触发控件的 label 决定); 六数值格 = 前缀家族 R0..R5。
	{ "GENUI_EXP_MODE",			GenUIEditor::OnExpSubmit,			'\0', '\0' },
	{ "GENUI_EXP_UNIT",			GenUIEditor::OnExpSubmit,			'\0', '\0' },
	{ "GENUI_EXP_R",			GenUIEditor::OnExpSubmit,			'0',  '5'  },
	// ---- 控件编辑区(ED): 标签行 / 文本行 / 事件行 / 尺寸轴 ----
	// 注: 四者共用同一处理器(槽位由触发控件的 label 决定)。
	{ "GENUI_ED_LBL",			GenUIEditor::OnEdSubmit,			'\0', '\0' },
	{ "GENUI_ED_TEXT",			GenUIEditor::OnEdSubmit,			'\0', '\0' },
	{ "GENUI_ED_EVENT",			GenUIEditor::OnEdSubmit,			'\0', '\0' },
	{ "GENUI_ED_AXIS",			GenUIEditor::OnEdSubmit,			'\0', '\0' },
	{ "GENUI_ED_PREV",			GenUIEditor::OnEdPrev,				'\0', '\0' },
	// ---- 控件编辑区(ED)九格: 8 系数格(##FAC_SW_EDIT..##FAC_PAD_EDIT) + 像素偏移格(##FAC_OFS_EDIT) ----
	// 注: 九格共用同一处理器(格号由触发控件 label 决定); 系数格 = 前缀家族 C0..C7。
	{ "GENUI_ED_C",				GenUIEditor::OnEdCell,				'0',  '7'  },
	{ "GENUI_ED_OFS",			GenUIEditor::OnEdCell,				'\0', '\0' },
	// ---- 列表行事件: 布局列表(##LAYOUTS) / 控件列表(##CONTROLS) 两个列表共用同一处理器 ----
	{ "GENUI_LIST_LM",			GenUIEditor::OnListRowEvent,		'\0', '\0' },
	{ "GENUI_LIST_CL",			GenUIEditor::OnListRowEvent,		'\0', '\0' },
	// ---- 提示布局(HINT)事件: 标题栏 / 确认 / 取消 ----
	// 注: 三者由「提示布局」(内嵌(flex)成员; 无独立 HWND)的标题栏与两个按钮携带 ——
	//     标题栏按住 = 移动该内嵌窗口的像素位置(不动 HWND) / 关闭钮 = "取消"结论(与 ##BTN_CANCEL 同义);
	//     两个按钮 = 点击记录结论(确认 / 取消)。三个处理器均只落内存状态与诊断, 不改文档。
	{ "HINT_TITLEBAR_EVENT",	GenUIEditor::OnHintTitleBarEvent,	'\0', '\0' },
	{ "HINT_OK",				GenUIEditor::OnHintOk,				'\0', '\0' },
	{ "HINT_CANCEL",			GenUIEditor::OnHintCancel,			'\0', '\0' },
};

// 函数: GetEventBindings
// 描述: 取本模块的"事件名 → 处理函数"绑定表(宿主启动期注入 UIEventHandler::SetBindings; 见 GenEditor.h)
CONST UIEventHandler::EventBinding* GenUIEditor::GetEventBindings(size_t* pnCount)
{
	if (pnCount != NULL)
	{
		*pnCount = sizeof(aEventBindings) / sizeof(aEventBindings[0]);
	}
	return aEventBindings;
}

// ================================
// 区域: 事件处理函数实现区
// 说明: 处理函数按 FuncPtr.h 的回调四元组签名实现, 由 UIEventHandler 的绑定表按布局文件
//       声明的 event 名绑定。
// ================================

// 函数: ShowMessageBoxUtf8
// 描述: 弹窗统一入口 —— 优先走同步「提示布局」; 不可用时回落 MessageBoxW
// 说明: ①提示布局(内嵌(flex)成员; 来源 = 外壳源文档 ⇒ 与当前编辑的文档无关; 宿主已注入同步展示器
//         且成员已装配)⇒ 走宿主展示器(同步阻塞至用户选择);
//         ②其余场景(启动期诊断 / 字体警告 / 布局未就绪)⇒ 回落 MessageBoxW(UTF-8 正文先转宽串, 避免中文乱码);
//       返回码按 MessageBox 口径归一: 双按钮 ⇒ IDYES / IDNO, 单按钮 ⇒ IDOK / IDCANCEL。
// 参数:
//		IN	szText:			正文(UTF-8; NULL 按空串)
//		IN	szTitle:		标题(UTF-8; NULL 按空串)
//		IN	dwType:			按钮与图标标志(MB_YESNO / MB_OK | MB_ICONWARNING …)
//		IN	szOkText:		确认按钮文本(UTF-8; NULL = 用「提示布局」的默认文本; 回落路径忽略)
//		IN	szCancelText:	取消按钮文本(UTF-8; NULL = 用默认文本; 回落路径忽略)
// 返回:
//		INT: IDYES / IDNO(双按钮) 或 IDOK / IDCANCEL(单按钮)
static INT ShowMessageBoxUtf8(LPCSTR szText, LPCSTR szTitle, UINT dwType,
	LPCSTR szOkText = NULL, LPCSTR szCancelText = NULL)
{
	const BOOL bYesNo = ((dwType & MB_TYPEMASK) == MB_YESNO) ? TRUE : FALSE;
	// 优先: 同步「提示布局」(展示器由宿主注入; 提示布局已装配 ⇒ 展示可用)
	pfnHintSync pfnHint = HostContext::GetHintSyncHandler();
	if (pfnHint != NULL && FlexHost::GetFlexCount() > 0)
	{
		HintSpec spec{};
		spec.szTitle = szTitle;
		spec.szText = szText;
		spec.szOkText = szOkText;
		spec.szCancelText = szCancelText;
		spec.bHasCancel = bYesNo;
		BOOL bOk = pfnHint(spec);
		if (bYesNo)
		{
			return bOk ? IDYES : IDNO;
		}
		return bOk ? IDOK : IDCANCEL;
	}
	// 回落: 宿主 MessageBoxW
	WCHAR wszText[2048]{};
	WCHAR wszTitle[128]{};
	MultiByteToWideChar(CP_UTF8, 0, (szText != NULL) ? szText : "", -1, wszText, (int)(sizeof(wszText) / sizeof(wszText[0])));
	MultiByteToWideChar(CP_UTF8, 0, (szTitle != NULL) ? szTitle : "", -1, wszTitle, (int)(sizeof(wszTitle) / sizeof(wszTitle[0])));
	return MessageBoxW(HostContext::GetWindowHandle(), wszText, wszTitle, dwType);
}

// ================================
// 区域: 文档承载者访问助手区
// 说明: 编辑器对"文档数据面"的访问一律经宿主注入的承载者(pDoc; 注入点见 SetDocument, 时序见其注释)。
//   未注入(pDoc == NULL; 仅可能出现在注入前的异常路径)⇒ 显式降级: 读返回缺省、写静默跳过
//   —— 与 Application 未就绪时的对外口径一致(doc 层自身保持"纯数据、无策略", 不提供空态替身)。
// ================================

// 函数: MarkDocDirtyCore
// 描述: 置文档"未保存改动"标记(经承载者)
static VOID MarkDocDirtyCore()
{
	if (pDoc != NULL)
	{
		pDoc->MarkDirty();
	}
}

// 函数: IsDocDirtyCore
// 描述: 文档是否有未保存改动(未注入 ⇒ FALSE)
static BOOL IsDocDirtyCore()
{
	return (pDoc != NULL && pDoc->IsDirty()) ? TRUE : FALSE;
}

// 函数: NormalizeDocIdsCore
// 描述: 逐布局把元素 ID 按数组序重排为 0..N-1, 并同步活动布局已展开的链式控件 id
// 说明: 判据本体在 doc\DocConsistency(与应用侧 ID 归一入口同一实现路径与顺序 ——
//   ①全部布局的中间态重排 ②活动布局的链式控件 id 同步; 活动条目缺失 ⇒ 该步无对象, 自然跳过)
// 返回:
//		BOOL: TRUE = 已重排; FALSE = 承载者未注入
static BOOL NormalizeDocIdsCore()
{
	BOOL bRet = FALSE;
	if (pDoc == NULL)
	{
		return bRet;
	}
	DocConsistency::NormalizeElementIds(pDoc->GetState());
	{
		LayoutEntry* pEntry = pDoc->GetEntry(pDoc->GetActive());
		if (pEntry != NULL && pEntry->layout != NULL)
		{
			DocConsistency::SyncCtrlIdsToIndex(*pEntry->layout);
		}
	}
	bRet = TRUE;
	return bRet;
}

// 函数: ActiveIndexCore / DocCountCore
// 描述: 当前激活索引 / 文档条目数(未注入 ⇒ 0)
static DWORD ActiveIndexCore()
{
	return (pDoc != NULL) ? pDoc->GetActive() : 0;
}

static DWORD DocCountCore()
{
	return (pDoc != NULL) ? pDoc->GetCount() : 0;
}

// 函数: ActiveEntryCore
// 描述: 活动条目的地址(未注入 / 索引越界 ⇒ NULL)
static LayoutEntry* ActiveEntryCore()
{
	return (pDoc != NULL) ? pDoc->GetEntry(pDoc->GetActive()) : NULL;
}

// 函数: ActiveLayoutCore
// 描述: 活动条目的布局对象地址(未命中 / 条目无布局 ⇒ NULL)
static Layout* ActiveLayoutCore()
{
	LayoutEntry* pEntry = ActiveEntryCore();
	return (pEntry != NULL) ? pEntry->layout : NULL;
}

// 函数: ActiveRefCore / ActiveRefSizeCore
// 描述: 活动条目的参考尺寸(解算后像素) / 参考尺寸与经验参数(中间态字段)
// 说明: 未命中 ⇒ 缺省样例(与应用侧 GetDocRef / GetDocRefSize 的空态回落同值)
static CONST AutoSize& ActiveRefCore()
{
	static AutoSize sNull = AutoSize::CreateByRatio(
		622, 432, 25, 17, 10, 4, AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT);
	LayoutEntry* pEntry = ActiveEntryCore();
	if (pEntry == NULL)
	{
		return sNull;
	}
	return pEntry->ref;
}

static CONST RefSize& ActiveRefSizeCore()
{
	static RefSize sNull = LayoutModel::DefaultRefSize();
	LayoutEntry* pEntry = ActiveEntryCore();
	if (pEntry == NULL)
	{
		return sNull;
	}
	return pEntry->refSize;
}

// 函数: DocPathCore / DocStateCore
// 描述: 文档关联文件路径(未注入 ⇒ NULL) / 文档中间态(未注入 ⇒ 空态)
static LPCSTR DocPathCore()
{
	return (pDoc != NULL) ? pDoc->GetPath() : NULL;
}

static CONST FileMidState& DocStateCore()
{
	static CONST FileMidState sEmptyState{};
	if (pDoc == NULL)
	{
		return sEmptyState;
	}
	return pDoc->GetState();
}

// 函数: DocLayoutLabelCore
// 描述: 取指定条目的显示名(label 为空则回落 name)拷入调用方缓冲(未注入 ⇒ FALSE)
static BOOL DocLayoutLabelCore(DWORD index, LPSTR buf, size_t cap)
{
	return (pDoc != NULL) ? pDoc->LayoutLabelAt(index, buf, cap) : FALSE;
}

// 元素规格族的取值 / 增删助手(含"布局→文档索引"反查)已迁 editor\EditorCommands:

// 函数: ConfirmDiscardDirtyChanges
// 描述: 有"未保存改动"时的统一提醒(创建 / 打开新文件 / 关闭窗口前调用)
// 说明: 无改动 ⇒ 直接放行(零弹窗); 有改动 ⇒ 弹窗, "是" = 继续(丢弃改动) / "否" = 取消该动作
// 参数:
//		IN	szAction:	即将进行的动作名
// 返回:
//		BOOL: TRUE = 可继续; FALSE = 用户选择取消
BOOL GenUIEditor::ConfirmDiscardDirtyChanges(LPCSTR szAction)
{
	LPCSTR lpAct = (szAction != NULL) ? szAction : "该操作";
	if (!IsDocDirtyCore())
	{
		return TRUE;
	}
	{
		CHAR szMsg[1024]{};
		snprintf(szMsg, sizeof(szMsg),
			"当前文档有未保存的改动。\n\n继续「%s」将丢弃这些改动。",
			lpAct);
		if (ShowMessageBoxUtf8(szMsg, "GenUI 未保存改动提醒", MB_YESNO | MB_ICONWARNING,
			u8"继续", u8"取消") != IDYES)
		{
			printf("[APP] 未保存改动提醒: 用户取消「%s」\n", lpAct);
			fflush(stdout);
			return FALSE;
		}
	}
	printf("[APP] 未保存改动提醒: 用户确认继续「%s」(丢弃未保存改动)\n", lpAct);
	fflush(stdout);
	return TRUE;
}

// 函数: OpenDocumentByPath
// 描述: 载入整份文档并把路径栏指向该文件(浏览/打开共用; JSON / bin 双格式, 形态由文件内容嗅探判定)
// 说明: ①有未保存改动时先弹窗提醒(用户取消 ⇒ 不载入);
//       ②载入被拒(文件损毁 / 格式不支持)⇒ 弹窗提示 + 新建一个空白文档(路径栏随之清空)
// 参数:
//		IN	path:	布局文件路径(空指针/空串直接失败)
// 返回:
//		BOOL: TRUE = 载入成功且已刷新路径/选中; FALSE = 未载入目标文件(取消 / 被拒)
static BOOL OpenDocumentByPath(LPCSTR path)
{
	BOOL bRet = FALSE;
	if (path == NULL || path[0] == '\0')
	{
		return bRet;
	}
	// 打开新文件会替换当前文档: 有未保存改动先提醒(用户取消 ⇒ 不载入)
	if (!GenUIEditor::ConfirmDiscardDirtyChanges("打开文件"))
	{
		return bRet;
	}
	if (!DocumentIO::LoadDocument(path))
	{
		// 载入被拒(文件损毁 / 格式不支持): 提示后新建一个空白文档(原文档不再保留)
		CHAR szMsg[1024]{};
		snprintf(szMsg, sizeof(szMsg),
			"文件损毁或格式不支持, 无法打开:\n\n%s\n\n将新建一个空白文档。", path);
		ShowMessageBoxUtf8(szMsg, "GenUI 打开失败", MB_OK | MB_ICONWARNING, u8"确定", NULL);
		printf("[APP] 打开失败: %s ⇒ 新建空白文档\n", path);
		fflush(stdout);
		if (DocumentIO::NewDocument())
		{
			szFilePath[0] = '\0';		// 新文档尚未关联文件
			pMain = ActiveLayoutCore();
			ResetSelectionLocal();
			SyncFilePathShell();
		}
		return bRet;					// FALSE: 未载入目标文件
	}
	// 载入成功: 路径栏指向新文件(渲染前由 SyncFilePathShell 回填控件), 选中与镜像基座重置
	strncpy_s(szFilePath, MAX_PATH, path, _TRUNCATE);
	pMain = ActiveLayoutCore();
	ResetSelectionLocal();
	SyncFilePathShell();		// 立即同步路径栏(不等下一帧)
	bRet = TRUE;
	return bRet;
}
// 函数: SyncFilePathShell
// 描述: 文件行路径动作控件(输入动作 ##FILE_PATH)与当前文档路径的两向同步(渲染前每帧调用, 幂等)
// 说明: 路径栏可编辑 ⇒ 用户键入的内容逐帧写回文档路径(打开按钮据此取路径)。
static VOID SyncFilePathShell()
{
	if (!PreviewSession::GetShellValid())
	{
		return;
	}
	// ① 启动期默认路径(仅一次; 之后空路径不再被默认值覆盖)
	static BOOL bPathInit = FALSE;
	if (!bPathInit)
	{
		bPathInit = TRUE;
		if (szFilePath[0] == '\0')
		{
			// 优先取当前文档的关联文件(载入 bin 文档时即 .\GenUI.bin); 无关联再回落工程源 JSON 路径
			LPCSTR def = DocPathCore();
			if (def == NULL || def[0] == '\0')
			{
				def = ResourceLoader::GetFilePath("GenUI.json");
			}
			strncpy_s(szFilePath, MAX_PATH, (def ? def : ""), _TRUNCATE);
		}
	}
	// ② 定位路径栏控件(未命中则只维护文档路径, 不做同步)
	//    注: 路径控件可能为 INPUT_SINGLE 或 INPUT_ACTION(同名 label, 按类型区分; 见 ShellFindPathCtrl);
	//        两类控件的文本取址/容量统一走 CtrlTextBuffer / CtrlTextCapacity(INPUT_ACTION 的文本在 data + split)
	CtrlInfo* pCtrl = ShellFindPathCtrl();
	if (pCtrl == NULL || pCtrl->bufferInfo.buffer == NULL)
	{
		return;
	}
	char* lpText = CtrlTextBuffer(pCtrl);
	const size_t nCap = CtrlTextCapacity(pCtrl);
	if (lpText == NULL || nCap == 0)
	{
		return;
	}
	// ③ 用户改动优先: 控件缓冲与上次投影不同 ⇒ 采纳为用户输入(ImGui 输入框保证 NUL 结尾)
	if (strncmp(lpText, szPathShown, nCap) != 0)
	{
		strncpy_s(szFilePath, MAX_PATH, lpText, _TRUNCATE);
		strncpy_s(szPathShown, MAX_PATH, lpText, _TRUNCATE);
		return;
	}
	// ④ 外部改动: 文档路径与上次投影不同(浏览/打开/创建所致) ⇒ 回填控件缓冲
	if (lstrcmpA(szFilePath, szPathShown) != 0)
	{
		strncpy_s(lpText, nCap, szFilePath, _TRUNCATE);
		strncpy_s(szPathShown, MAX_PATH, szFilePath, _TRUNCATE);
	}
}
// 函数: OnBrowse
// 描述: GENUI_BROWSE(文件行"浏览"): 文件对话框选择布局文件(过滤器含 bin / JSON), 选中即载入并刷新路径栏
// 时机: 帧间事件 —— 由本函数自行代理: 渲染帧内触发(IsInFrame 为真)只把本函数挂到
//       帧间队列并返回; 帧末排空(IsInFrame 为假)时才弹对话框并载入文档
//       (载入会重建中间态池与预览基座内容控件)
VOID GenUIEditor::OnBrowse(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	// 回调四元组: 本处理器不需要其中任何一项(帧内挂载时按原值透传给排空入口)。
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);

	// 帧间事件代理: 渲染帧内触发 ⇒ 只入队并返回; 帧末排空 ⇒ 条件为假, 继续执行下方业务。
	if (FrameEvent::IsInFrame())
	{
		FrameEvent::Post(GenUIEditor::OnBrowse, obj, oper, 0, data);
		return;
	}

	CHAR szFile[MAX_PATH]{};
	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = HostContext::GetWindowHandle();
	ofn.lpstrFilter = "布局文档 (*.bin;*.json)\0*.bin;*.json\0布局 bin (*.bin)\0*.bin\0布局 JSON (*.json)\0*.json\0所有文件 (*.*)\0*.*\0\0";
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	// 初始目录 = exe 目录(取不到时交给系统默认)
	{
		LPCSTR lpDir = ResourceLoader::GetExeDir();
		ofn.lpstrInitialDir = (lpDir != NULL && lpDir[0] != '\0') ? lpDir : NULL;
	}
	// 默认扩展名 = bin(bin 为设计最终形态; 形态仍由文件内容嗅探判定, 扩展名只用于补全)
	ofn.lpstrDefExt = "bin";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	if (GetOpenFileNameA(&ofn))
	{
		OpenDocumentByPath(szFile);
	}
}
// 函数: OnCreate
// 描述: GENUI_CREATE(文件行"创建"): 新建空白文档 —— 只创建新的中间态对象, 不生成真实文件;
//       创建后清空路径栏(新文档尚未关联任何文件)
// 时机: 帧间事件 —— 帧内触发只挂载; 帧末排空时才新建文档(替换中间态池并重装预览基座内容控件)
VOID GenUIEditor::OnCreate(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	// 回调四元组: 本处理器不需要其中任何一项(帧内挂载时按原值透传给排空入口)。
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);

	// 帧间事件代理: 渲染帧内触发 ⇒ 只入队并返回; 帧末排空 ⇒ 条件为假, 继续执行下方业务。
	if (FrameEvent::IsInFrame())
	{
		FrameEvent::Post(GenUIEditor::OnCreate, obj, oper, 0, data);
		return;
	}

	// 新建文档会替换当前文档: 有未保存改动先提醒(用户取消 ⇒ 不新建)
	if (!GenUIEditor::ConfirmDiscardDirtyChanges("新建文档"))
	{
		return;
	}
	if (!DocumentIO::NewDocument())
	{
		return;
	}
	szFilePath[0] = '\0';		// 空路径 = 尚未关联文件(有效状态, 不再回填默认路径)
	pMain = ActiveLayoutCore();
	ResetSelectionLocal();
	SyncFilePathShell();		// 立即把路径栏清空(不等下一帧)
}
// 函数: OnOpen
// 描述: GENUI_OPEN(文件行"打开"): 载入路径栏当前所指文件(整文档); 路径为空时转为浏览
// 时机: 帧间事件 —— 帧内触发只挂载; 帧末排空时才载入(载入会重建文档与基座内容)
VOID GenUIEditor::OnOpen(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	// 回调四元组: 本处理器不需要其中任何一项(帧内挂载时按原值透传给排空入口)。
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);

	// 帧间事件代理: 渲染帧内触发 ⇒ 只入队并返回; 帧末排空 ⇒ 条件为假, 继续执行下方业务。
	if (FrameEvent::IsInFrame())
	{
		FrameEvent::Post(GenUIEditor::OnOpen, obj, oper, 0, data);
		return;
	}

	if (szFilePath[0] == '\0')
	{
		OnBrowse(NULL, 0, 0, NULL);		// 无路径 ⇒ 转浏览(帧末排空 ⇒ 直接弹对话框)
		return;
	}
	OpenDocumentByPath(szFilePath);
}
// 函数: CheckSaveDataInteractive
// 描述: 保存前数据体检的交互层 —— ①Label 重名 ⇒ 弹窗(取消保存 / 忽略重名);
//       ②ID 不连续 ⇒ 弹窗(自动重排 / 直接保存)
// 返回:
//		BOOL: TRUE = 体检通过或用户选择继续保存; FALSE = 用户选择"取消保存"
static BOOL CheckSaveDataInteractive()
{
	BOOL bRet = TRUE;
	BOOL bIdGap = FALSE;
	BOOL bLabelDup = FALSE;
	CHAR szReport[1024]{};
	DocConsistency::CheckSaveData(DocStateCore(), &bIdGap, &bLabelDup, szReport, sizeof(szReport));
	if (bLabelDup)
	{
		CHAR szMsg[1600]{};
		snprintf(szMsg, sizeof(szMsg),
			"保存前检查: 同一布局内存在 Label 重名(按 Label 定位控件会产生歧义)。\n\n%s",
			szReport);
		if (ShowMessageBoxUtf8(szMsg, "GenUI 保存前检查", MB_YESNO | MB_ICONWARNING,
			u8"忽略重名", u8"取消保存") != IDYES)
		{
			printf("[APP] 保存取消: 用户选择取消(Label 重名)\n");
			return FALSE;
		}
	}
	if (bIdGap)
	{
		CHAR szMsg[1600]{};
		snprintf(szMsg, sizeof(szMsg),
			"保存前检查: 存在元素 ID 未按数组序连续(0..N-1)的布局; 框架按 ID 访问元素。\n\n%s",
			szReport);
		if (ShowMessageBoxUtf8(szMsg, "GenUI 保存前检查", MB_YESNO | MB_ICONWARNING,
			u8"自动重排", u8"保持原样") == IDYES)
		{
			NormalizeDocIdsCore();
			MarkDocDirtyCore();	// 保存前自动重排改写了 ID = 文档改动
			printf("[APP] 保存前自动重排: 各布局元素 ID 已按数组序重排为 0..N-1\n");
		}
	}
	return bRet;
}
// 函数: OnSave
// 描述: GENUI_SAVE(文件行"保存"): 整文档写盘 —— 路径栏非空 ⇒ 写该路径(工程源文档路径被拒并转另存为);
//       路径栏为空 ⇒ 弹另存为对话框(默认名 GenUI.user.bin), 成功后新路径写回路径栏。
//       写盘格式按目标扩展名分派(.bin ⇒ bin / 其余 ⇒ 规范 JSON; 见应用侧 SaveDocument);
//       写盘前做数据体检(Label 重名 / ID 不连续 ⇒ 弹窗定夺; 见 CheckSaveDataInteractive)
// 时机: 帧间事件 —— 帧内触发只挂载; 帧末排空时才写盘/弹对话框
VOID GenUIEditor::OnSave(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	// 回调四元组: 本处理器不需要其中任何一项(帧内挂载时按原值透传给排空入口)。
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);

	// 帧间事件代理: 渲染帧内触发 ⇒ 只入队并返回; 帧末排空 ⇒ 条件为假, 继续执行下方业务。
	if (FrameEvent::IsInFrame())
	{
		FrameEvent::Post(GenUIEditor::OnSave, obj, oper, 0, data);
		return;
	}

	CHAR szTarget[MAX_PATH]{};
	BOOL bNeedDialog = (szFilePath[0] == '\0');
	if (!bNeedDialog)
	{
		strncpy_s(szTarget, MAX_PATH, szFilePath, _TRUNCATE);
		// 工程源保护: 当前文档来自 .\GenUI.json 加载路径且目标就是该文件 ⇒ 拒绝直存并转另存为
		if (DocumentIO::IsProtectedSavePath(szTarget))
		{
			printf("[APP] 保存被拒: 当前文档来自工程源文档(exe 目录 GenUI.json) ⇒ 转另存为\n");
			bNeedDialog = TRUE;
			szTarget[0] = '\0';
		}
	}
	if (bNeedDialog)
	{
		CHAR szFile[MAX_PATH]{};
		OPENFILENAMEA ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = HostContext::GetWindowHandle();
		ofn.lpstrFilter = "布局 bin (*.bin)\0*.bin\0布局 JSON (*.json)\0*.json\0所有文件 (*.*)\0*.*\0\0";
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = ResourceLoader::GetExeDir();
		// 默认扩展名 = bin(bin 为设计最终形态)⇒ 用户不改扩展名时落 bin 文件
		ofn.lpstrDefExt = "bin";
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
		strncpy_s(szFile, MAX_PATH, "GenUI.user.bin", _TRUNCATE);		// 默认名避开受保护的工程源文件
		if (!GetSaveFileNameA(&ofn))
		{
			return;						// 取消: 路径栏保持原值
		}
		strncpy_s(szTarget, MAX_PATH, szFile, _TRUNCATE);
	}
	// 位置 = 路径确定之后、写盘之前
	if (!CheckSaveDataInteractive())
	{
		return;							// 用户选择"取消保存": 路径栏保持原值
	}
	// 写盘: 用户显式选择了受保护路径(或写盘失败) ⇒ SaveDocument 拒绝并打印诊断, 路径栏不变
	if (!DocumentIO::SaveDocument(szTarget))
	{
		return;
	}
	strncpy_s(szFilePath, MAX_PATH, szTarget, _TRUNCATE);
	SyncFilePathShell();				// 立即刷新路径栏(不等下一帧)
}
// 函数: OnInputActionEvent
// 描述: GENUI_FILEPATH(文件行"路径动作"控件 = INPUT_ACTION): 按钮段点击 ⇒ 浏览(对话框选文件并载入);
//       输入段提交 ⇒ 把该控件输入文本写回文档路径(路径栏两向同步随后收敛)
// 时机: 帧间事件 —— 按钮段会弹模态对话框并载入文档; 输入段提交同走帧间队列
// 参数:
//		IN	oper:	规范操作码(独立控件的部件结论进专属段: OPER_CUSTOM_IA_INPUT_SUBMIT /
//					OPER_CUSTOM_IA_BTN_CLICKED; 帧间挂载时原样透传)
VOID GenUIEditor::OnInputActionEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	// 回调四元组: param/data 不使用(帧末按 label/类型重取该控件与其缓冲)。
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);

	// 帧间事件代理: 渲染帧内触发 ⇒ 只入队并返回; 帧末排空 ⇒ 条件为假, 继续执行下方业务。
	if (FrameEvent::IsInFrame())
	{
		FrameEvent::Post(GenUIEditor::OnInputActionEvent, obj, oper, 0, data);
		return;
	}

	// 部件结论取专属段(段内值; 解释专属码前已知本处理函数服务 INPUT_ACTION ⇒ 无需类型查表)
	CONST uint16_t wOwn = (uint16_t)OperCode::GetSegment(oper, OperCode::OPER_SEG_OWN);
	// 按钮段: 点击 ⇒ 浏览(帧末排空中执行 ⇒ OnBrowse 直接弹对话框并载入)
	if (wOwn == OPER_CUSTOM_IA_BTN_CLICKED)
	{
		OnBrowse(NULL, 0, 0, NULL);
		return;
	}
	// 输入段: 提交(回车/失焦且有改动) ⇒ 输入文本写回文档路径, 再由路径栏同步收敛显示
	if (wOwn == OPER_CUSTOM_IA_INPUT_SUBMIT)
	{
		CtrlInfo* pCtrl = ShellFindPathCtrl();
		if (pCtrl != NULL && pCtrl->type == CTRL_TYPE_INPUT_ACTION)
		{
			CHAR* lpText = CtrlTextBuffer(pCtrl);
			if (lpText != NULL && lpText[0] != '\0')
			{
				strncpy_s(szFilePath, MAX_PATH, lpText, _TRUNCATE);
			}
			else if (lpText != NULL)
			{
				szFilePath[0] = '\0';			// 提交空串 ⇒ 解绑当前文件(有效状态)
			}
			SyncFilePathShell();
		}
	}
}

// ================================
// 区域: 标题栏事件处理(两窗口标题各自绑定独立事件名)
// ================================

// 函数: OnTitleBarEvent
// 描述: TITLEBAR_EVENT(编辑窗口标题栏): 按住拖动 = 记录编辑窗口的期望位置; 右上关闭钮点击 = 退出应用
// 说明: 窗口的实质移动不在此处发生 —— 标题栏控件把"窗口期望坐标"经 param 通用槽带来, 本处理器只把它
//       记进外壳 WND 件的位置记录(position); 渲染出口在下次取得该窗口渲染目标时经 AcquireWnd 结算
//       (绘制侧口径已由框架统一: 绘制期内不改窗口几何, 此处不再调用任何窗口 API)。
// 参数:
//		IN	obj:	触发的标题控件(CtrlInfo*; 本处理器不需要)
//		IN	oper:	规范操作码(公共位 = OPER_COMMON_HOLD 按住中(电平) · 专属段 = OPER_STRUCT_TITLE_CLOSE)
//		IN	param:	按住期间 = 窗口期望坐标(PackWndPos 打包; 未按住或未建立拖动 = 0)
//		IN	data:	该控件缓冲(标题文本; 本处理器不需要)
VOID GenUIEditor::OnTitleBarEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(data);

	// 右上关闭钮(结构件专属码 = 请求关闭窗口): 编辑窗口销毁即结束整个应用
	// (宿主 WndProc 的 WM_DESTROY → PostQuitMessage); 关闭优先于拖动 ⇒ 命中即返回, 不进入拖动分支
	if ((uint16_t)OperCode::GetSegment(oper, OperCode::OPER_SEG_OWN) == OPER_STRUCT_TITLE_CLOSE)
	{
		HWND hWnd = HostContext::GetWindowHandle();
		if (hWnd != NULL)
		{
			PostMessage(hWnd, WM_CLOSE, 0, 0);
		}
		return;
	}
	// 按住中(电平位: 条件成立期间每帧置位)⇒ 把期望坐标记进编辑窗口的位置记录
	// 说明: 拖动基线与期望坐标(光标 - 抓取偏移)都由标题栏控件侧算出 ⇒ 本处理器不认识句柄, 也不做换算
	if (OperCode::HasCommon(oper, OPER_COMMON_HOLD))
	{
		CtrlInfo* pWnd = PreviewSession::GetShellLayout().GetLayoutBase(CTRL_TYPE_WND);
		if (pWnd != NULL)
		{
			CONST vec2 posWant = UnpackWndPos(param);
			pWnd->position = posWant;
			// 声明坐标同步为位置记录: 坐标重推(父链累加)只读声明坐标 ⇒ 不同步会把位置记录改回旧值
			pWnd->posDeclared = posWant;
		}
	}
}
// 函数: OnPreviewTitleBarEvent
// 描述: PREVIEW_TITLEBAR_EVENT(预览窗口标题栏): 按住拖动 = 记录预览窗口的期望位置并写回该布局的
//       wndX/wndY; 右上关闭钮在本事件下不生效
// 说明: 与编辑窗口同法 —— 本处理器只写位置记录(预览基座 WND 件的 position + 条目 wndX/wndY),
//       实质移动由渲染出口在下次取得预览窗口渲染目标时经 AcquireWnd 结算(绘制期内不改窗口几何)。
// 参数:
//		IN	obj:	触发的标题控件(CtrlInfo*; 本处理器不需要)
//		IN	oper:	规范操作码(公共位 = OPER_COMMON_HOLD 按住中(电平); 关闭专属码在本事件下不处理)
//		IN	param:	按住期间 = 窗口期望坐标(PackWndPos 打包; 未按住或未建立拖动 = 0)
//		IN	data:	该控件缓冲(标题文本; 本处理器不需要)
VOID GenUIEditor::OnPreviewTitleBarEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(data);

	// 注: 本事件只处理"按住中"(OPER_COMMON_HOLD; 电平位)⇒ 逐帧记录预览窗口的期望位置
	if (OperCode::HasCommon(oper, OPER_COMMON_HOLD))
	{
		CONST vec2 posWant = UnpackWndPos(param);
		CtrlInfo* pWnd = Application::GetActiveLayout().GetLayoutBase(CTRL_TYPE_WND);
		if (pWnd != NULL)
		{
			pWnd->position = posWant;
			pWnd->posDeclared = posWant;
			// 写回该布局的窗口桌面位置(条目字段 + 中间态布局信息); 预览基座下次同步时以它为准
			Application::SetDocWndPos(ActiveIndexCore(), (LONG)posWant.x, (LONG)posWant.y);
		}
	}
}

// ================================
// 区域: 提示布局(HINT)三事件实现区
// 说明: 「提示布局」= 内嵌(flex)成员(无独立 HWND, 由编辑窗口的渲染帧内以 LayoutRender::RenderFlex 渲染);
//       三个事件由该布局的标题栏(##TITLEBAR)与两个按钮(##BTN_OK / ##BTN_CANCEL)携带。
//       标题栏拖动不得沿用 SetWindowPos 移动 HWND 的既有做法(内嵌窗口没有 HWND) ——
//         本区改为改写该布局窗体区(WND)的像素位置(渲染层每帧按此 Begin 内嵌窗口)。
// ================================

// 函数: SetHintResult
// 描述: 记录「提示布局」交互结论并打一条 [HINT] 诊断(供接入提示流程前的人工/日志核对)
// 参数:
//		IN	nResult:	结论(HINT_RESULT_*)
//		IN	szSource:	结论来源(诊断文案; NULL 按空串)
static VOID SetHintResult(INT nResult, LPCSTR szSource)
{
	nHintResult = nResult;
	LPCSTR szName = (nResult == GenUIEditor::HINT_RESULT_OK) ? u8"确认" :
		((nResult == GenUIEditor::HINT_RESULT_CANCEL) ? u8"取消" : u8"无");
	printf("[HINT] 结论 = %s(%s)\n", szName, (szSource != NULL) ? szSource : "");
	fflush(stdout);
}
// 函数: FindFlexTitleLayout
// 描述: 由触发的标题控件反查其所属的内嵌(flex)布局(渲染中的成员 ⇒ 只查当前活动集合)
// 说明: 按标题控件指针比对定位(不解引用挂载期持有的指针, 与列表定位列表归属同法);
//       非活动成员不渲染 ⇒ 不可能产生交互, 故不进本判据。
// 参数:
//		IN	obj:	触发的标题控件(CtrlInfo*)
// 返回:
//		Layout*: 命中的内嵌成员布局; 未命中返回 NULL
static Layout* FindFlexTitleLayout(VOID* obj)
{
	Layout* pRet = NULL;
	if (obj == NULL)
	{
		return pRet;
	}
	DWORD dwFlex = 0;
	Layout** ppFlex = FlexHost::GetActiveFlexLayouts(PreviewSession::GetShellLayout(), &dwFlex);
	for (DWORD i = 0; i < dwFlex && pRet == NULL; i++)
	{
		if (ppFlex[i] != NULL && ppFlex[i]->GetLayoutBase(CTRL_TYPE_TITLE) == (CtrlInfo*)obj)
		{
			pRet = ppFlex[i];
		}
	}
	return pRet;
}







										




// 函数: OnHintTitleBarEvent
// 描述: HINT_TITLEBAR_EVENT(「提示布局」标题栏): 按住拖动 = 移动该内嵌窗口; 关闭钮 = 记录"取消"结论
// 参数:
//		IN	obj:	触发的标题控件(CtrlInfo*)
//		IN	oper:	规范操作码(公共位 = OPER_COMMON_HOLD 按住中(电平);
//					专属段 = OPER_STRUCT_TITLE_CLOSE 请求关闭提示)
//		IN	param:	未用(事件回调四元组约定)
//		IN	data:	该控件缓冲(标题文本; 本处理器不需要)
VOID GenUIEditor::OnHintTitleBarEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);

	// 右上关闭钮(结构件专属码): 命中即记录"取消"结论并返回, 不进入拖动分支
	// 注: "提示的关闭钮 = 取消"(与 ##BTN_CANCEL 同义) ——
	//     此处只落结论, 既不关闭/隐藏提示, 也不触发任何文档动作
	if ((uint16_t)OperCode::GetSegment(oper, OperCode::OPER_SEG_OWN) == OPER_STRUCT_TITLE_CLOSE)
	{
		SetHintResult(HINT_RESULT_CANCEL, u8"标题栏关闭钮");
		bHintDragging = FALSE;
		return;
	}
	// 按住中(电平位: 条件成立期间每帧置位)⇒ 逐帧跟随光标移动内嵌窗口的像素位置
	if (OperCode::HasCommon(oper, OPER_COMMON_HOLD))
	{
		Layout* pFlex = FindFlexTitleLayout(obj);
		CtrlInfo* pWnd = (pFlex != NULL) ? pFlex->GetLayoutBase(CTRL_TYPE_WND) : NULL;
		HWND hHost = HostContext::GetWindowHandle();		// 宿主 = 编辑窗口(内嵌窗口的位置基准 = 其客户区)
		if (pWnd != NULL && hHost != NULL)
		{
			POINT cur{};
			GetCursorPos(&cur);
			ScreenToClient(hHost, &cur);
			if (!bHintDragging)
			{
				// 按下首帧: 记录光标相对该内嵌窗口左上角的偏移(拖动全程保持)
				ptHintDragOfs.x = cur.x - (LONG)pWnd->position.x;
				ptHintDragOfs.y = cur.y - (LONG)pWnd->position.y;
				bHintDragging = TRUE;
			}
			else
			{
				pWnd->position.x = (FLOAT)(cur.x - ptHintDragOfs.x);
				pWnd->position.y = (FLOAT)(cur.y - ptHintDragOfs.y);
			}
		}
	}
	else
	{
		bHintDragging = FALSE;		// 未按住 ⇒ 结束本次拖动
	}
}
// 函数: OnHintOk
// 描述: HINT_OK(「提示布局」的 ##BTN_OK 点击): 记录"确认"结论
// 参数:
//		IN	obj:	触发的按钮控件(CtrlInfo*; 本处理器不需要)
//		IN	oper:	规范操作码(公共码 OPER_COMMON_CLICKED)
//		IN	param:	未用(事件回调四元组约定)
//		IN	data:	该控件缓冲(按钮文本; 本处理器不需要)
VOID GenUIEditor::OnHintOk(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);

	if (OperCode::HasCommon(oper, OPER_COMMON_CLICKED))
	{
		SetHintResult(HINT_RESULT_OK, "##BTN_OK");
	}
}
// 函数: OnHintCancel
// 描述: HINT_CANCEL(「提示布局」的 ##BTN_CANCEL 点击): 记录"取消"结论
// 参数:
//		IN	obj:	触发的按钮控件(CtrlInfo*; 本处理器不需要)
//		IN	oper:	规范操作码(公共码 OPER_COMMON_CLICKED)
//		IN	param:	未用(事件回调四元组约定)
//		IN	data:	该控件缓冲(按钮文本; 本处理器不需要)
VOID GenUIEditor::OnHintCancel(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);

	if (OperCode::HasCommon(oper, OPER_COMMON_CLICKED))
	{
		SetHintResult(HINT_RESULT_CANCEL, "##BTN_CANCEL");
	}
}
// 函数: OnHintKey
// 描述: 提示展示期间的键盘结论(Enter = 确认 / ESC = 取消; 由宿主 WndProc 的 WM_KEYDOWN 调用)
// 说明: 与 OnHintOk / OnHintCancel 的差别只在结论来源文案(诊断可区分"按钮点击"与"键盘");
// 参数:
//		IN	bOk:	TRUE = 确认(Enter); FALSE = 取消(ESC)
VOID GenUIEditor::OnHintKey(BOOL bOk)
{
	SetHintResult(bOk ? HINT_RESULT_OK : HINT_RESULT_CANCEL, bOk ? "Enter" : "ESC");
}

// ================================
// 区域: 核心模型实现区(UI 与意图消费者共用)
// ================================

// 函数: DefaultAddSize
// 描述: 新增控件默认尺寸(系数表达): 宽 = 3.0*控件高, 高 = 1.0*控件高
static vec2 DefaultAddSize()
{
	FLOAT ch = ActiveRefVal(4);
	if (ch < 1.0f) { ch = 1.0f; }
	return vec2(3.0f * ch, ch);
}
// 函数: GetAutoPlacePos
// 描述: 依据当前内容控件数量自动排布新控件位置(逐行自动换行, 避免重叠)
static vec2 GetAutoPlacePos(const vec2& sz)
{
	vec2 ret(16.0f, 16.0f);
	if (pMain == NULL)
	{
		return ret;
	}
	CtrlInfo* wnd = pMain->GetLayoutBase(CTRL_TYPE_WND);
	if (wnd == NULL || wnd->size.x < 80.0f || wnd->size.y < 80.0f)
	{
		return ret;
	}
	FLOAT W = wnd->size.x;
	FLOAT H = wnd->size.y;
	size_t shellC = pMain->GetShellNum();
	size_t docCount = pMain->GetTotalNum();		// 布局控件数(链内只含文档控件 ⇒ 即文档面控件数)
	size_t content = docCount > shellC ? docCount - shellC : 0;
	INT rowsPerCol = (INT)((H - 40.0f) / 40.0f);
	if (rowsPerCol < 1) { rowsPerCol = 1; }
	INT col = (INT)(content / rowsPerCol);
	INT row = (INT)(content % rowsPerCol);
	FLOAT x = 16.0f + col * (sz.x + 40.0f);
	if (x + sz.x > W - 16.0f) { x = 16.0f; }
	FLOAT y = 16.0f + row * 40.0f;
	if (y + sz.y > H - 16.0f) { y = 16.0f; }
	ret.x = x;
	ret.y = y;
	return ret;
}
// 函数: AddControlCore
// 描述: 向 active 布局添加控件并选中新控件
BOOL GenUIEditor::AddControlCore(CtrlType type)
{
	BOOL bRet = FALSE;
	if (pMain == NULL)
	{
		pMain = ActiveLayoutCore();
	}
	if (pMain == NULL || pMain->IsInvalid())
	{
		return bRet;
	}
	size_t qwCtrl = pMain->GetTotalNum();		// 布局尾部(链式追加)
	CtrlInfo ci{};
	// 计算新的控件 ID(链内只含文档控件 ⇒ 全员参与 id 分配)
	int nMaxId = 0;
	for (size_t i = 0; i < pMain->GetTotalNum(); i++)
	{
		CONST CtrlInfo* pc = pMain->GetCtrlInfo(i);
		if (pc == NULL)
		{
			continue;
		}
		if (pc->id > nMaxId)
		{
			nMaxId = pc->id;
		}
	}
	ci.type = type;
	ci.id = nMaxId + 1;
	{
		CHAR szLabel[32]{};
		snprintf(szLabel, sizeof(szLabel), "##G%d", ci.id);
		ci.label = AllocatorHub::CopyString(szLabel);
	}
	// 缓冲区初始化(失败则不入表)
	if (!CtrlBuffer::PrepareCtrlBuffer(&ci, GetDefaultCtrlText(type)))
	{
		return bRet;
	}
	// 位置与尺寸: 直接给像素几何(系数不驻控件; 记录里不为新增控件建条目 ⇒ 重解时保持像素)
	// 注: 声明坐标与有效坐标同值 —— AddCtrlInfo 会按父链重推有效坐标(读 posDeclared),
	//     只写 position 的新增控件会在入链后被重推成 0
	ci.size = DefaultAddSize();
	ci.position = GetAutoPlacePos(ci.size);
	ci.posDeclared = ci.position;
	// 追加位置的文档索引(== 布局控件索引; 须在 AddCtrlInfo 之前取: 控件数随后 +1)
	CONST size_t nDoc = pMain->GetTotalNum();
	// 按索引插入到尾部(链式模式)
	if (!pMain->AddCtrlInfo(qwCtrl, ci))
	{
		return bRet;
	}
	// 中间态元素规格: 在插入位置腾出空位(其后元素整体后移), 保持元素与文档索引一一对应
	EditorCommands::InsertSpecAt(pMain, nDoc);
	// 中间态元素补齐"类型串 + 标签"
	RawAttributes::SetRawType(pMain, nDoc, GetTypeName(type));		// 文档原类型串(旁挂表)
	{
		ElementSpec* es = (ci.label != NULL) ? EditorCommands::SpecAt(pMain, nDoc) : NULL;
		if (es != NULL)
		{
			LayoutModel::StrCopy(es->type, sizeof(es->type), GetTypeName(type));
			LayoutModel::StrCopy(es->label, sizeof(es->label), ci.label);
		}
	}
	// 选中新控件(文档索引)
	nSelected = (int)nDoc;
	nSynced = -2;
	SyncEditsToSel();
	SelectionOverlay::SetSelectedIndex(nSelected);	// 选中态即刻就位(叠加件随预览渲染绘制)
	PreviewSession::RequestPreviewSync();	// 改链式结构(新增控件) ⇒ 统一走帧末同步
	MarkDocDirtyCore();		// 新增控件 = 文档改动(未保存)
	bRet = TRUE;
	return bRet;
}

// ================================
// 区域: EXP 释放控件数据桥(镜像同步)
// 说明: GenUIEditor 布局中 EXP 区的释放控件为同层平铺(模式/单位 Combo + 参数 RECT
//       区域 + 六格名称 TEXT/数值 INPUT_SINGLE), 显示数据源 = active 布局的参考尺寸/经验参数(中间态 RefSize),
//       经本区域函数在"外壳渲染路径"每帧桥接: Update() 调 SyncExpShell() 把配置换算成
//       显示文本写回外壳控件缓冲(权威文本副本保护: 用户编辑中的格不被打断)。
// ================================

// 函数: ShellFindCtrl
// 描述: 在外壳快照布局中按 label 查找内容控件(未命中返回 NULL)
static CtrlInfo* ShellFindCtrl(LPCSTR label)
{
	if (label == NULL || !PreviewSession::GetShellValid())
	{
		return NULL;
	}
	Layout& sh = PreviewSession::GetShellLayout();
	for (size_t i = sh.GetShellNum(); i < sh.GetTotalNum(); i++)
	{
		CtrlInfo* c = sh.GetCtrlInfo(i);
		if (c && c->label && lstrcmpA(c->label, label) == 0)
		{
			return c;
		}
	}
	return NULL;
}
// 函数: ShellFindPathCtrl
// 描述: 取外壳"路径控件" —— 同名 label(##FILE_PATH)可能存在多个(INPUT_SINGLE / INPUT_ACTION);
//       文档内唯一性由 id 决定 ⇒ 本函数按类型区分: 优先命中输入动作, 否则回退输入框
// 返回:
//		CtrlInfo*: 命中的路径控件; 无命中返回 NULL
static CtrlInfo* ShellFindPathCtrl()
{
	if (!PreviewSession::GetShellValid())
	{
		return NULL;
	}
	Layout& sh = PreviewSession::GetShellLayout();
	CtrlInfo* pFallback = NULL;
	for (size_t i = sh.GetShellNum(); i < sh.GetTotalNum(); i++)
	{
		CtrlInfo* c = sh.GetCtrlInfo(i);
		if (c == NULL || c->label == NULL || lstrcmpA(c->label, "##FILE_PATH") != 0)
		{
			continue;
		}
		if (c->type == CTRL_TYPE_INPUT_ACTION)
		{
			return c;					// 输入动作优先
		}
		if (c->type == CTRL_TYPE_INPUT_SINGLE && pFallback == NULL)
		{
			pFallback = c;				// 输入框兜底
		}
	}
	return pFallback;
}

// ================================
// 区域: 列表行数据填充桥(外壳两列表 <- 文档状态)
// 说明: 外壳列表的行数据由内容消费者写入其列表缓冲; 本区域函数在 Update() 渲染前每帧重填 ——
//       ##LAYOUTS(布局管理) 行 = 各布局显示名 + 行尾按钮文案, 选中项 = active 布局;
//       ##CONTROLS(控件列表) 行 = active 布局的内容控件(跳过前端结构件)类型下拉, 选中项 = 选中控件。
//       编辑中保护: 每列表每行维护"权威副本"(上次写入的值), 仅当缓冲仍等于权威副本(未被用户改动)
//       才覆盖回填, 否则跳过该行(用户正在行内编辑或改选, 不打断)。
// ================================
constexpr DWORD SHELL_LIST_MAX_ROWS = 128;			// 权威副本行数上限(实际行数还受缓冲容量夹取)
constexpr size_t SHELL_LIST_AUTH_CHARS = CTRL_TEXT_BUF;	// 权威行文本副本字节数(与行文本区同容量)

static CHAR s_aszLmRowAuth[SHELL_LIST_MAX_ROWS][SHELL_LIST_AUTH_CHARS]{};	// 布局列表每行权威文本
static CHAR s_aszClRowAuth[SHELL_LIST_MAX_ROWS][SHELL_LIST_AUTH_CHARS]{};	// 控件列表每行权威文本
static DWORD s_dwLmRowsAuth = 0;											// 布局列表上次写入的行数(0 = 未建立)
static DWORD s_dwClRowsAuth = 0;											// 控件列表上次写入的行数(0 = 未建立)
static BOOL s_bClSpecialInit = FALSE;										// 控件列表"新增行"下拉是否已初始化(首次指向首个可新增类型)
static LPCSTR s_aszTypeItems[CTRL_TYPE_INFO_NUM]{};							// 类型显示名表(与 aCtrlTypeInfo 同源)
static BOOL s_bTypeItemsReady = FALSE;

// 函数: EnsureTypeItems
// 描述: 首次调用时把类型显示名表填为 aCtrlTypeInfo 的 szName 视图
static VOID EnsureTypeItems()
{
	if (s_bTypeItemsReady == FALSE)
	{
		for (size_t i = 0; i < CTRL_TYPE_INFO_NUM; i++)
		{
			s_aszTypeItems[i] = aCtrlTypeInfo[i].szName;
		}
		s_bTypeItemsReady = TRUE;
	}
}
// 函数: FillLayoutListRows
// 描述: 按文档布局现状重填布局列表(##LAYOUTS)的项目行(行 = 一个布局; 行文本 = 该布局显示名)
// 参数:
//		IN/OUT	pListCtrl:	布局列表控件(类型/缓冲不符时无操作)
static VOID FillLayoutListRows(CtrlInfo* pListCtrl)
{
	ListBuffer* pList = ListDataOf(pListCtrl);
	DWORD dwRows = 0;
	DWORD k = 0;
	if (pList == NULL || pListCtrl->type != CTRL_TYPE_LIST_INPUT)
	{
		return;
	}
	CONST size_t cbItem = (size_t)pList->cbItem;
	BYTE* lpItemBase = (BYTE*)(pList + 1) + cbItem;			// 项目区(跳过特殊行块)
	CONST DWORD dwCap = pList->dwCapacity;
	CONST DWORD dwDocs = DocCountCore();
	dwRows = (dwDocs <= dwCap) ? dwDocs : dwCap;
	if (dwRows > SHELL_LIST_MAX_ROWS) { dwRows = SHELL_LIST_MAX_ROWS; }
	for (k = 0; k < dwRows; k++)
	{
		CtrlInputActionData* pData = (CtrlInputActionData*)(lpItemBase + cbItem * k);
		CHAR* lpBtn = NULL;
		CHAR* lpText = NULL;
		pData->split = CTRL_INPUT_ACTION_SPLIT;			// 行块初始化标志(文本取址依赖它)
		lpBtn = CtrlInputActionBtnText(pData);
		lpText = CtrlInputActionText(pData);
		if (lpBtn != NULL)
		{
			strcpy_s(lpBtn, CTRL_INPUT_ACTION_ACTION, u8"-");			// 行尾删除钮文案
		}
		// 编辑中保护: 缓冲文本仍等于权威副本(或该行首次填充)才覆盖回填
		if (lpText != NULL &&
			((k >= s_dwLmRowsAuth) || (strncmp(lpText, s_aszLmRowAuth[k], SHELL_LIST_AUTH_CHARS) == 0)))
		{
			DocLayoutLabelCore(k, lpText, CTRL_TEXT_BUF);
			strncpy_s(s_aszLmRowAuth[k], SHELL_LIST_AUTH_CHARS, lpText, _TRUNCATE);
		}
	}
	// 行数减少: 清理残余行(不参与绘制; 仅保证行块无陈旧串)
	for (k = dwRows; (k < s_dwLmRowsAuth) && (k < dwCap); k++)
	{
		memset(lpItemBase + cbItem * k, 0, cbItem);
	}
	// 特殊行块(紧随缓冲头, 长度 = cbItem): 新增行 = 单行输入 + 按钮(文案 "+")
	// 说明: 行件在"文本取址无效"时不产像素 ⇒ 需置 split; 输入文本不在此清写(留给用户填写新增名)
	if (cbItem >= sizeof(CtrlInputActionData))
	{
		CtrlInputActionData* pSpec = (CtrlInputActionData*)(pList + 1);
		CHAR* lpSpecBtn = CtrlInputActionBtnText(pSpec);
		pSpec->split = CTRL_INPUT_ACTION_SPLIT;
		if (lpSpecBtn != NULL)
		{
			strcpy_s(lpSpecBtn, CTRL_INPUT_ACTION_ACTION, u8"+");
		}
	}
	pList->dwCount = dwRows;
	pList->dwSelected = (ActiveIndexCore() < dwRows) ? ActiveIndexCore() : LIST_SEL_NONE;
	s_dwLmRowsAuth = dwRows;
}
// 函数: BuildCtrlRowText
// 描述: 构造"控件列表"项目行的行文本 —— 三段式 = 控件 id + 类型显示名 + 标签名
// 说明: id 取控件自身的 id(文档 JSON 的 id 字段; 两位补零, 超过两位按实际位数显示),
//       类型取编辑器类型信息表的中文显示名(与"新增类型"下拉同源), 标签取控件 label;
//       多段之间分隔符 = 全角空格(U+3000)。
// 参数:
//		IN	ctrl:	目标控件(可为 NULL)
//		OUT	buf:	输出缓冲
//		IN	cap:	输出缓冲字节数
static VOID BuildCtrlRowText(CONST CtrlInfo* ctrl, LPSTR buf, size_t cap)
{
	buf[0] = '\0';
	if (ctrl == NULL)
	{
		return;
	}
	LPCSTR type = TypeDisplayName(ctrl->type);
	int width = CalcTextWidth(type);
	// 类型名段的最宽显示宽度(列): 首次调用时扫类型信息表求出后常驻
	static int max = 0;
	if (max == 0)
	{
		for (size_t i = 0; i < CTRL_TYPE_INFO_NUM; i++)
		{
			CONST int w = CalcTextWidth(aCtrlTypeInfo[i].szName);
			if (w > max)
			{
				max = w;
			}
		}
	}
	// 先绘制 ID + 类型名
	snprintf(buf, cap, u8"%02d\u3000%s", (INT)ctrl->id, type);
	// 标签为空: 只显示前两段
	if (ctrl->label != NULL && ctrl->label[0] != '\0')
	{
		// 类型名和标签名之间默认间隔 1 个全角空格
		int full = 1;
		int half = 0;
		// 当 width <= max 时, 标签前补空格以对齐
		half = (width <= max) ? max - width : 0;
		full += (half == 0) ? 0 : half / 2;
		half %= 2;
		// 先追加全角符号
		for (int i = 0; i < full; i++)
		{
			strncat_s(buf, cap, u8"\u3000", _TRUNCATE);
		}
		// 再追加半角符号
		if (half != 0)
		{
			strncat_s(buf, cap, u8" ", _TRUNCATE);
		}

		// 最后追加标签
		strncat_s(buf, cap, ctrl->label, _TRUNCATE);
	}
}
// 函数: FillCtrlListRows
// 描述: 按 active 布局的控件现状重填控件列表(##CONTROLS)的项目行(行 = 一个控件; 行文本 = id + 类型名 + 标签名)
// 参数:
//		IN/OUT	pListCtrl:	控件列表控件(类型/缓冲不符时无操作)
static VOID FillCtrlListRows(CtrlInfo* pListCtrl)
{
	ListBuffer* pList = ListDataOf(pListCtrl);
	Layout* pMain = NULL;
	DWORD dwRows = 0;
	DWORD k = 0;
	if (pList == NULL || pListCtrl->type != CTRL_TYPE_LIST_COMBO)
	{
		return;
	}
	pMain = ActiveLayoutCore();
	if (pMain->IsInvalid())
	{
		return;
	}
	EnsureTypeItems();
	{
		// 行数 = 布局控件数(含结构件; 链内只含文档控件)
		CONST DWORD dwCtrls = (DWORD)pMain->GetTotalNum();
		CONST size_t cbItem = (size_t)pList->cbItem;
		BYTE* lpItemBase = (BYTE*)(pList + 1) + cbItem;		// 项目区(跳过特殊行块)
		CONST DWORD dwCap = pList->dwCapacity;
		dwRows = (dwCtrls <= dwCap) ? dwCtrls : dwCap;
		if (dwRows > SHELL_LIST_MAX_ROWS) { dwRows = SHELL_LIST_MAX_ROWS; }
		for (k = 0; k < dwRows; k++)
		{
			// 行 = 一个控件(含结构件; 行号 = 文档索引): 行块 = INPUT_ACTION 块,
			// 输入段只读(文本 = id + 类型名 + 标签名) + 行尾按钮
			CtrlInfo* c = CtrlOfDocIndex(pMain, (INT)k);
			CtrlInputActionData* pData = (CtrlInputActionData*)(lpItemBase + cbItem * k);
			CHAR* lpBtn = NULL;
			CHAR* lpText = NULL;
			pData->split = CTRL_INPUT_ACTION_SPLIT;			// 行块初始化标志(文本取址依赖它)
			lpBtn = CtrlInputActionBtnText(pData);
			lpText = CtrlInputActionText(pData);
			if (lpBtn != NULL)
			{
				strcpy_s(lpBtn, CTRL_INPUT_ACTION_ACTION, u8"-");		// 行尾删除钮文案(结构件点击被拒)
			}
			// 编辑中保护: 缓冲文本仍等于权威副本(或该行首次填充)才覆盖回填
			if ((lpText != NULL) &&
				((k >= s_dwClRowsAuth) || (strncmp(lpText, s_aszClRowAuth[k], SHELL_LIST_AUTH_CHARS) == 0)))
			{
				// 行文本 = 控件 id + 类型显示名 + 标签名(三段式; 见 BuildCtrlRowText)
				CHAR szRow[CTRL_TEXT_BUF]{};
				BuildCtrlRowText(c, szRow, sizeof(szRow));
				strncpy_s(lpText, CTRL_TEXT_BUF, szRow, _TRUNCATE);
				strncpy_s(s_aszClRowAuth[k], SHELL_LIST_AUTH_CHARS, lpText, _TRUNCATE);
			}
		}
		// 行数减少: 清理残余行(不参与绘制; 仅保证行块无陈旧内容)
		for (k = dwRows; (k < s_dwClRowsAuth) && (k < dwCap); k++)
		{
			memset(lpItemBase + cbItem * k, 0, cbItem);
		}
		pList->dwSelected = ((nSelected >= 0) && ((DWORD)nSelected < dwCtrls)) ?
			(DWORD)nSelected : LIST_SEL_NONE;
	}
	// 特殊行块(新增行): 下拉选项表 = 类型显示名表(与类型表同源) ⇒ 选中项即"要新增的类型";
	// 首次把选中项指到首个可新增类型; 此后不覆盖用户选择
	{
		CtrlComboData* pSpec = (CtrlComboData*)(pList + 1);
		pSpec->items = s_aszTypeItems;
		pSpec->count = (INT)CTRL_TYPE_INFO_NUM;
		pSpec->lpBtn = u8"+";			// 新增钮文案(行块自持; 静态串)
		if (s_bClSpecialInit == FALSE)
		{
			for (INT i = 0; i < (INT)CTRL_TYPE_INFO_NUM; i++)
			{
				if (aCtrlTypeInfo[i].nPalette >= 0)
				{
					pSpec->index = i;
					break;
				}
			}
			s_bClSpecialInit = TRUE;
		}
		if (pSpec->index < 0 || pSpec->index >= pSpec->count)
		{
			pSpec->index = 0;			// 越界兜底(仅防行件取址越界)
		}
	}
	pList->dwCount = dwRows;
	pList->dwSelected = ((nSelected >= 0) && ((size_t)nSelected < pMain->GetTotalNum())) ?
		(DWORD)nSelected : LIST_SEL_NONE;
	s_dwClRowsAuth = dwRows;
}
// 函数: SyncListShell
// 描述: 外壳列表行数据填充(渲染前, 幂等): 布局列表 <- 文档布局; 控件列表 <- active 布局内容控件
static VOID SyncListShell()
{
	FillLayoutListRows(ShellFindCtrl("##LAYOUTS"));
	FillCtrlListRows(ShellFindCtrl("##CONTROLS"));
}

// 函数: ExpRefPx
// 描述: 按"基准 x 千分比 / 1000(至少 1px)"复算参考量像素(与 GeometrySolver::EntryRefSizeInfo 同式)
// 参数:
//		IN	ratio:	参考量(千分比)
//		IN	base:	换算基准像素
// 返回: DWORD 参考量像素(至少 1)
static DWORD ExpRefPx(INT ratio, DWORD base)
{
	if (base < 1) { base = 1; }
	DWORD v = (DWORD)((unsigned __int64)base * (DWORD)ratio / 1000);
	if (v < 1) { v = 1; }
	return v;
}

// 函数: SyncExpShell
// 描述: EXP 镜像同步(外壳渲染路径, Update 渲染前每帧调用, 幂等):
//       active 配置(模式/单位/六千分比) -> 换算显示文本 -> 外壳释放控件缓冲;
//       编辑中保护: 每格维护"权威文本副本"(提交后/同步后 = 最近写入的规范显示串),
//       仅当 shell 缓冲仍等于权威副本(未被用户改动)才覆盖回填, 否则跳过(用户编辑中);
//       模式/单位 Combo 另把选项表与选中索引同步(选项表为文件级静态数组, 只读引用)。
static VOID SyncExpShell()
{
	if (!PreviewSession::GetShellValid())
	{
		return;
	}
	const RefSize& refSize = ActiveRefSizeCore();
	CONST INT nMode = GeometrySolver::AutoModeIndex(refSize.mode);
	const DWORD dwBase = GeometrySolver::EntryBasePx(nMode);
	// 1. 模式 Combo: 选项表 + 索引(镜像自 active 配置 nMode)
	{
		CtrlInfo* c = ShellFindCtrl("##MODE_SEL");
		if (c && c->bufferInfo.buffer && c->bufferInfo.size >= sizeof(CtrlComboData))
		{
			CtrlComboData* cb = (CtrlComboData*)c->bufferInfo.buffer;
			cb->count = 4;
			cb->items = aszExpModeName;
			INT idx = nMode;
			if (idx < 0 || idx > 3) { idx = 0; }
			cb->index = idx;
		}
	}
	// 2. 单位 Combo: 选项表 + 索引(镜像自 active 配置 nUnit)
	{
		CtrlInfo* c = ShellFindCtrl("##UNIT_SEL");
		if (c && c->bufferInfo.buffer && c->bufferInfo.size >= sizeof(CtrlComboData))
		{
			CtrlComboData* cb = (CtrlComboData*)c->bufferInfo.buffer;
			cb->count = 2;
			cb->items = aszExpUnitName;
			INT idx = refSize.unit;
			if (idx < 0 || idx > 1) { idx = 0; }
			cb->index = idx;
		}
	}
	// 3. 六数值格(CTRL_TYPE_INPUT_NUMERIC, 整数缓冲): 换算显示整数 + 权威值回填
	//    显示规则(与提交侧 GeometrySolver::RefValueToRatio 互逆; 存储恒千分比):
	//    unit=0 显示千分比原值 / unit=1 显示该千分比按基准推导的像素值
	//    编辑中保护: 每格维护"权威整数值"(同步/提交后 = 最近写入的规范值), 仅当缓冲
	//    仍等于权威值(未被用户输入改动)才覆盖回填, 否则跳过(用户正在编辑, 不打断)
	for (INT i = 0; i < 6; i++)
	{
		INT vNew = (INT)LayoutModel::RefSizeValueAt(refSize, i);	// unit=0: 千分比原值
		if (refSize.unit != 0 && vNew > 0)
		{
			vNew = (INT)ExpRefPx(vNew, dwBase);						// unit=1: 像素显示值(与解算侧同式)
		}
		CtrlInfo* c = ShellFindCtrl(aszExpRLabel[i]);
		if (c == NULL || c->bufferInfo.buffer == NULL || c->bufferInfo.size < sizeof(INT))
		{
			anExpAuth[i] = -1;		// 布局缺该格: 复位权威副本
			continue;
		}
		INT* pv = (INT*)c->bufferInfo.buffer;
		// 编辑中保护(判据 = 缓冲是否仍等于权威值, 见本区说明)
		if (anExpAuth[i] < 0 || *pv == anExpAuth[i])
		{
			// 缓冲仍等于权威值(未被用户改动): 覆盖为按当前配置换算的规范整数
			*pv = vNew;
			anExpAuth[i] = vNew;
			// 记录该格来源布局(提交目标; 与 ED 格同一机制)
			aExpOwner[i].dwLayout = ActiveIndexCore();
			aExpOwner[i].pLayout = ActiveLayoutCore();
		}
		// 否则: 缓冲 != 权威(用户正在编辑该格): 跳过, 不打断
	}
}

// 函数: OnExpSubmit
// 描述: GENUI_EXP_R0..R5(经验参数区六数值格)/ GENUI_EXP_MODE(模式下拉)/ GENUI_EXP_UNIT(单位下拉)提交:
//       数值格 = 把该格的值按该条目单位折算写回其来源布局的 autosize, 并按新参考尺寸就地更新
//       预览基座控件几何; 模式 = 写该布局的换算基准模式串并就地重解; 单位 = 写该布局的参考量单位
//       (单位只决定入参解释, 存储恒千分比 ⇒ 几何不变)
// 时机: 帧间事件 —— 帧内触发只把"槽位序号"挂入帧间队列(以 param 承载, 操作码恒为规范码);
//       帧末排空时按 label 重新取该控件与其缓冲
// 参数:
//		IN	obj:	触发控件(CtrlInfo*; 帧内用于定位槽位序号, 帧末不使用)
//		IN	oper:	规范操作码(帧末分派不使用; 槽位一律走 param)
//		IN	param:	帧末执行时承载"槽位序号"(0..5 = 六数值格; 6 = 模式; 7 = 单位)
//		IN	data:	该控件缓冲(帧内不读取; 帧末按 label 重取)
VOID GenUIEditor::OnExpSubmit(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(data);
	// 1. 帧内: 由触发控件的 label 定位格序号, 并以 param 承载之挂入帧间队列
	if (FrameEvent::IsInFrame())
	{
		CtrlInfo* c = (CtrlInfo*)obj;
		INT nSlot = -1;
		if (c != NULL && c->label != NULL)
		{
			if (lstrcmpA(c->label, "##MODE_SEL") == 0)
			{
				nSlot = EXP_SLOT_MODE;
			}
			else if (lstrcmpA(c->label, "##UNIT_SEL") == 0)
			{
				nSlot = EXP_SLOT_UNIT;
			}
			else
			{
				for (INT i = 0; i < 6; i++)
				{
					if (lstrcmpA(c->label, aszExpRLabel[i]) == 0)
					{
						nSlot = i;
						break;
					}
				}
			}
		}
		if (nSlot < 0)
		{
			return;
		}
		// 挂载: oper = 编辑族规范码(标注来源与结论) · param = 槽位序号
		// (与 FrameEvent::Post 的合并判据配合: 同帧不同槽位 param 不同 ⇒ 不会被误合并)
		// 注: 模式/单位两槽是 COMBO(控件库报"值变更")⇒ 与六数值格共用同一规范码
		//     (该规范码 = OperCode::MakeCodeOwn(OPER_EDIT_SUBMIT); 语义见"内部工具函数实现区"说明)
		FrameEvent::Post(GenUIEditor::OnExpSubmit, NULL, OperCode::MakeCodeOwn(OPER_EDIT_SUBMIT), (uint64_t)nSlot, NULL);
		return;
	}
	// 2. 帧末: 按格序号(param)重新取该格控件与缓冲(帧内持有的对象引用已不可依赖)
	CONST INT nSlot = (INT)param;
	// 2-bis. 模式下拉(##MODE_SEL): 取该控件下拉选中项 -> 模式串 -> 按新基准就地重解
	//        (目标 = 当前 active 布局: 模式属布局级换算基准, 无"来源格"记录)
	if (nSlot == EXP_SLOT_MODE)
	{
		CtrlInfo* pCombo = ShellFindCtrl("##MODE_SEL");
		if (pCombo == NULL || pCombo->bufferInfo.buffer == NULL ||
			pCombo->bufferInfo.size < sizeof(CtrlComboData))
		{
			return;
		}
		CONST CtrlComboData* pData = (CONST CtrlComboData*)pCombo->bufferInfo.buffer;
		if (pData->index < 0 || pData->index >= 4)
		{
			return;
		}
		Application::ApplyDocAutoMode(ActiveIndexCore(), aszExpModeName[pData->index]);
		MarkDocDirtyCore();		// 换算基准改动 = 文档改动(未保存)
		return;
	}
	// 2-ter. 单位下拉(##UNIT_SEL): 取该控件下拉选中项 -> 单位值(0 千分比 / 1 像素) -> 写回当前 active 布局
	//        (单位属布局级参考量, 无"来源格"记录; 存储恒千分比 ⇒ 不改几何, 六格显示值随之换算)
	if (nSlot == EXP_SLOT_UNIT)
	{
		CtrlInfo* pCombo = ShellFindCtrl("##UNIT_SEL");
		if (pCombo == NULL || pCombo->bufferInfo.buffer == NULL ||
			pCombo->bufferInfo.size < sizeof(CtrlComboData))
		{
			return;
		}
		CONST CtrlComboData* pData = (CONST CtrlComboData*)pCombo->bufferInfo.buffer;
		if (pData->index < 0 || pData->index > 1)
		{
			return;
		}
		Application::ApplyDocAutoUnit(ActiveIndexCore(), (INT)pData->index);
		MarkDocDirtyCore();		// 单位改动 = 文档改动(未保存)
		return;
	}
	if (nSlot < 0 || nSlot >= 6)
	{
		return;
	}
	CtrlInfo* pCell = ShellFindCtrl(aszExpRLabel[nSlot]);
	if (pCell == NULL || pCell->bufferInfo.buffer == NULL || pCell->bufferInfo.size < sizeof(INT))
	{
		return;
	}
	// 3. 提交目标 = 该格的来源布局(镜像回填时记录; 越界/未建立 ⇒ 当前 active)
	DWORD dwDoc = ActiveIndexCore();
	if (aExpOwner[nSlot].dwLayout < DocCountCore())
	{
		dwDoc = aExpOwner[nSlot].dwLayout;
	}
	// 4. 值处理: 负数按 0; 按新参考尺寸就地更新预览几何
	INT v = *(INT*)pCell->bufferInfo.buffer;
	if (v < 0)
	{
		v = 0;
	}
	if (Application::ApplyDocAutoRatio(dwDoc, nSlot, (DWORD)v))
	{
		anExpAuth[nSlot] = v;		// 权威副本随提交更新(镜像下一帧按新配置回填)
		MarkDocDirtyCore();	// 参考量改动 = 文档改动(未保存)
	}
}


// ================================
// 区域: ED 释放控件数据桥(镜像同步 + 事件提交)
// 说明: GenUIEditor 布局中 ED 区的释放控件为同层平铺(类型只读 TEXT + 标签/文本/事件
//       单行输入 + 尺寸轴 Combo + 系数 RECT区域 + 8 系数格 + 偏移格), 显示数据源 = active
//       布局选中控件(nSelected/pMain), 经本区域函数每帧桥接: Update() 调 EdSyncShell() 把
//       选中控件属性换算成显示文本写回外壳释放控件缓冲 —— 与 SyncExpShell 同构的"权威文本
//       副本"编辑中保护(仅当缓冲 == 权威副本才回填, 用户编辑中的格不被打断); 四类提交
//       (GENUI_ED_LBL/TEXT/EVENT/AXIS)均为失焦提交, 由 OnEdSubmit 处理器经帧间队列
//       分派到 EdApplyLabel/EdApplyText/EdApplyEvent/EdApplyAxis 落库 —— 标签/文本/事件名
//       写中间态元素规格后请求预览同步, 尺寸轴只切本编辑区当前编辑轴;
//       系数/偏移提交(EdApplyCoeff/EdApplyOffset)后按系数重算选中控件几何。
// ================================

// 函数: EdSelCtrl
// 描述: 取当前选中控件(active 文档; pMain 由 Update 每帧刷新, 越界/无选中返回 NULL)
static CtrlInfo* EdSelCtrl()
{
	// 选中索引 = 文档索引(链内只含文档控件 ⇒ 即布局控件索引) ⇒ 统一经 SelCtrlOf 取控件
	return SelCtrlOf();
}

// 函数: EdOwnerSet
// 描述: 记录某编辑格的权威值来源(镜像回填该格时调用; 提交目标绑定)
// 参数:
//		OUT	owner:	该格的来源记录
//		IN	sel:	本帧镜像回填所用控件(可为 NULL = 无选中)
//		IN	idx:	该控件在 active 布局内的索引(sel 非空时有效)
//		IN	axis:	本帧镜像回填所用尺寸轴
static VOID EdOwnerSet(EdCellOwner& owner, CtrlInfo* sel, INT idx, INT axis)
{
	owner.pCtrl = sel;
	owner.nIdx = (sel != NULL) ? idx : -1;
	owner.nAxis = axis;
}
// 函数: EdOwnerCtrl
// 描述: 校验"编辑格来源"仍有效并返回来源控件(失效返回 NULL ⇒ 调用方放弃本次提交)
// 说明: 校验索引范围与"该索引此刻仍指向同一控件"两项, 任一不符返回 NULL
static CtrlInfo* EdOwnerCtrl(CONST EdCellOwner& owner)
{
	if (pMain == NULL || owner.pCtrl == NULL || owner.nIdx < 0)
	{
		return NULL;			// 尚未建立来源(或来源为"无选中")
	}
	if ((size_t)owner.nIdx >= DocCtrlCount())
	{
		return NULL;			// 控件已被删除
	}
	if (CtrlOfDocIndex(pMain, owner.nIdx) != owner.pCtrl)
	{
		return NULL;			// 同索引已指向别的控件 ⇒ 来源失效
	}
	return owner.pCtrl;
}
// 函数: EdOwnerAxis
// 描述: 取来源尺寸轴(越界回退当前轴)
static INT EdOwnerAxis(CONST EdCellOwner& owner)
{
	return (owner.nAxis >= 0 && owner.nAxis <= 3) ? owner.nAxis : nAxis;
}
// 函数: EdShellTxtBuf
// 描述: 取外壳释放控件的文本缓冲(镜像/提交读写用; 未命中返回 NULL)
// 参数:
//		IN	label:	外壳控件 label
//		OUT	pCap:	缓冲字节数(可空)
static char* EdShellTxtBuf(LPCSTR label, size_t* pCap)
{
	if (pCap)
	{
		*pCap = 0;
	}
	if (label == NULL || !PreviewSession::GetShellValid())
	{
		return NULL;
	}
	CtrlInfo* c = ShellFindCtrl(label);
	if (c == NULL || c->bufferInfo.buffer == NULL || c->bufferInfo.size < 2)
	{
		return NULL;
	}
	if (pCap)
	{
		*pCap = c->bufferInfo.size;
	}
	return (char*)c->bufferInfo.buffer;
}

// 函数: EdSetBufAuth
// 描述: 把显示文本写入外壳缓冲并同步"权威文本副本"(镜像回填/提交归一统一出口)
static VOID EdSetBufAuth(char* buf, size_t cap, CHAR* auth, size_t authCap, LPCSTR text)
{
	if (buf == NULL || auth == NULL)
	{
		return;
	}
	strncpy_s(buf, cap, (text ? text : ""), _TRUNCATE);
	strncpy_s(auth, authCap, (text ? text : ""), _TRUNCATE);
}

// 函数: EdWriteTextToBuffer
// 描述: 把文本写回选中控件的文本缓冲(文本类 → 其文本缓冲; TITLE 亦属文本类)
// 说明: 只写控件缓冲。
// 参数:
//		IN	sel:	选中控件
//		IN	text:	新文本(可为 NULL, 按空串处理)
// 返回:
//		BOOL: TRUE = 已写入; FALSE = sel 为空或该类型无文本缓冲(未写入)
static BOOL EdWriteTextToBuffer(CtrlInfo* sel, LPCSTR text)
{
	if (sel == NULL)
	{
		return FALSE;
	}
	// 文本缓冲回写(取址经 CtrlBuffer.h 的 CtrlTextBuffer: 文本类 = 其文本缓冲首址)
	{
		char* lpBuf = CtrlTextBuffer(sel);
		if (lpBuf == NULL)
		{
			return FALSE;
		}
		strncpy_s(lpBuf, sel->bufferInfo.size, (text ? text : ""), _TRUNCATE);
	}
	return TRUE;
}

// 函数: EdRecalcSelGeometry
// 描述: 选中控件几何 <- 中间态元素规格的当前轴系数 + 像素偏移(系数/偏移改动即时生效)
// 参数:
//		IN	sel:	目标控件(提交核心记录的来源控件; 可为 NULL)
static VOID EdRecalcSelGeometry(CtrlInfo* sel)
{
	if (sel == NULL || pMain == NULL)
	{
		return;
	}
	// 按指针反查文档索引(编辑器侧只持控件指针, 几何重解按索引经 Application 落地)
	CONST INT index = DocIndexOfCtrl(pMain, sel);
	if (index < 0)
	{
		return;			// 来源已失效(控件已被删除 / 文档已替换): 无从重解
	}
	Application::ApplyDocCtrlGeometry(pMain, (size_t)index);
}

// 函数: EdSyncShell
// 描述: ED 镜像同步(外壳渲染路径, Update 渲染前每帧调用, 幂等):
//       active 选中控件(类型/标签/文本/尺寸轴 nAxis/当前轴因子系数与偏移) -> 换算显示
//       值写回外壳释放控件缓冲; 编辑中保护: 文本行(标签/文本 INPUT_SINGLE)维护"权威文本
//       副本", 数值行(系数/偏移格 INPUT_REAL, double 缓冲)维护"权威数值副本"(含有效标志),
//       仅当缓冲仍等于权威值(未被用户改动)才覆盖回填, 否则跳过(用户编辑中);
//       文本为实时写回: 缓冲 != 权威副本视为用户正在键入, 反向写回文档
//       并刷新权威副本; 无选中 → 类型行给"(未选中)"提示, 其余编辑缓冲清空。
static VOID EdSyncShell()
{
	if (!PreviewSession::GetShellValid())
	{
		return;
	}
	CtrlInfo* sel = EdSelCtrl();
	// 1. 类型行(只读镜像): 有选中显示类型枚举名, 无选中显示提示文本
	{
		char* buf = EdShellTxtBuf(aszEdTypeName, NULL);
		if (buf)
		{
			strncpy_s(buf, CTRL_TEXT_BUF, sel ? GetTypeName(sel->type) : u8"(未选中)", _TRUNCATE);
		}
	}
	// 2. 尺寸轴 Combo: 选项表 + 索引(nAxis 为文件级状态, 供本区镜像与提交核心共用)
	{
		CtrlInfo* c = ShellFindCtrl(aszEdAxisName);
		if (c && c->bufferInfo.buffer && c->bufferInfo.size >= sizeof(CtrlComboData))
		{
			CtrlComboData* cb = (CtrlComboData*)c->bufferInfo.buffer;
			cb->count = 4;
			cb->items = aszEdAxisOpt;
			INT idx = nAxis;
			if (idx < 0 || idx > 3)
			{
				idx = 3;
			}
			cb->index = idx;
		}
	}
	// 3. 无选中: 清空各编辑缓冲(权威副本同步为"", 镜像不再视为编辑中)
	if (sel == NULL)
	{
		CHAR* aBuf = NULL;
		size_t aCap = 0;
		if ((aBuf = EdShellTxtBuf(aszEdLblName, &aCap)) != NULL)
		{
			EdSetBufAuth(aBuf, aCap, aszEdLblAuth, sizeof(aszEdLblAuth), "");
		}
		if ((aBuf = EdShellTxtBuf(aszEdTxtName, &aCap)) != NULL)
		{
			EdSetBufAuth(aBuf, aCap, aszEdTxtAuth, sizeof(aszEdTxtAuth), "");
		}
		if ((aBuf = EdShellTxtBuf(aszEdEvtName, &aCap)) != NULL)
		{
			EdSetBufAuth(aBuf, aCap, aszEdEvtAuth, sizeof(aszEdEvtAuth), "");
		}
		for (INT i = 0; i < 8; i++)
		{
			CtrlInfo* cc = ShellFindCtrl(aszEdCName[i]);
			if (cc && cc->bufferInfo.buffer && cc->bufferInfo.size >= sizeof(DOUBLE))
			{
				*(DOUBLE*)cc->bufferInfo.buffer = 0.0;
				adEdCAuth[i] = 0.0;
				abEdCAuth[i] = TRUE;
			}
			else
			{
				abEdCAuth[i] = FALSE;	// 布局缺该格: 复位权威标志
			}
		}
		{
			CtrlInfo* cc = ShellFindCtrl(aszEdOfsName);
			if (cc && cc->bufferInfo.buffer && cc->bufferInfo.size >= sizeof(DOUBLE))
			{
				*(DOUBLE*)cc->bufferInfo.buffer = 0.0;
				dEdOfsAuth = 0.0;
				bEdOfsAuth = TRUE;
			}
			else
			{
				bEdOfsAuth = FALSE;
			}
		}
		{
			CtrlInfo* cc = ShellFindCtrl(aszEdPrevName);
			if (cc && cc->bufferInfo.buffer && cc->bufferInfo.size >= sizeof(INT))
			{
				*(INT*)cc->bufferInfo.buffer = -1;
				nEdPrevAuth = -1;
				bEdPrevAuth = TRUE;
			}
			else
			{
				bEdPrevAuth = FALSE;	// 布局缺该行: 复位权威标志
			}
		}
		return;
	}
	// 4. 标签行(失焦提交: 编辑中不回写, 提交由 GENUI_ED_LBL 事件落库)
	{
		char* buf = NULL;
		size_t cap = 0;
		if ((buf = EdShellTxtBuf(aszEdLblName, &cap)) != NULL)
		{
			LPCSTR docLbl = sel->label ? sel->label : "";
			if (strcmp(buf, aszEdLblAuth) == 0)
			{
				// 缓冲 == 权威副本(未被用户改动): 回填选中控件当前标签
				EdSetBufAuth(buf, cap, aszEdLblAuth, sizeof(aszEdLblAuth), docLbl);
				EdOwnerSet(edLblOwner, sel, nSelected, nAxis);	// 记录该行来源(提交目标)
			}
			// 否则: 该行未被回填(缓冲 != 权威), 跳过(不改来源)
		}
	}
	// 5. 文本行(失焦提交: 编辑中不回写, 提交由 GENUI_ED_TEXT 事件落库到中间态)
	{
		char* buf = NULL;
		size_t cap = 0;
		CHAR* lpSelTxt = CtrlTextBuffer(sel);
		LPCSTR docTxt = (lpSelTxt != NULL && lpSelTxt[0]) ? lpSelTxt : "";
		if ((buf = EdShellTxtBuf(aszEdTxtName, &cap)) != NULL)
		{
			if (strcmp(buf, aszEdTxtAuth) == 0)
			{
				// 缓冲 == 权威副本(未被用户改动): 回填选中控件当前文本
				EdSetBufAuth(buf, cap, aszEdTxtAuth, sizeof(aszEdTxtAuth), docTxt);
				EdOwnerSet(edTxtOwner, sel, nSelected, nAxis);	// 记录该行来源(提交目标)
			}
			// 否则: 该行未被回填(缓冲 != 权威), 跳过(不改来源)
		}
	}
	// 5-bis. 事件行(失焦提交: 事件名取自中间态元素(控件对象不持有事件名),
	//       回填取值经承载者条目访问(Document::SpecAt)查该控件的中间态元素规格)
	{
		char* buf = NULL;
		size_t cap = 0;
		ElementSpec* es = EdElSpecOf(sel, nSelected);
		LPCSTR docEvt = (es != NULL && es->event[0]) ? es->event : "";
		if ((buf = EdShellTxtBuf(aszEdEvtName, &cap)) != NULL)
		{
			if (strcmp(buf, aszEdEvtAuth) == 0)
			{
				// 缓冲 == 权威副本(未被用户改动): 回填该控件当前事件名
				EdSetBufAuth(buf, cap, aszEdEvtAuth, sizeof(aszEdEvtAuth), docEvt);
				EdOwnerSet(edEvtOwner, sel, nSelected, nAxis);	// 记录该行来源(提交目标)
			}
			// 否则: 该行未被回填(缓冲 != 权威), 跳过(不改来源)
		}
	}
	// 5-ter. 前序 ID 行(整数缓冲, 失焦提交): 取值 = 中间态元素 parentID(与布局对象控件同值)
	{
		CtrlInfo* cc = ShellFindCtrl(aszEdPrevName);
		if (cc == NULL || cc->bufferInfo.buffer == NULL ||
			cc->bufferInfo.size < sizeof(INT))
		{
			bEdPrevAuth = FALSE;	// 布局缺该行: 复位权威标志
		}
		else
		{
			ElementSpec* es = EdElSpecOf(sel, nSelected);
			INT vNew = (es != NULL) ? es->parentID : sel->parentID;
			INT* pv = (INT*)cc->bufferInfo.buffer;
			if (!bEdPrevAuth || *pv == nEdPrevAuth)
			{
				// 缓冲 == 权威副本(未被用户改动): 回填该控件当前前序 ID + 记录来源
				*pv = vNew;
				nEdPrevAuth = vNew;
				bEdPrevAuth = TRUE;
				EdOwnerSet(edPrevOwner, sel, nSelected, nAxis);
			}
			// 否则: 用户正在编辑该行, 跳过(不改来源)
		}
	}
	// 6. 系数格/偏移格(INPUT_REAL, double 缓冲; 失焦提交):
	//    按当前编辑轴因子回填数值; 选中控件无因子 → 显示 0(镜像), 提交处理器会忽略
	{
		SizeFactor* fac = AxisFactorOf(sel, nAxis);
		for (INT i = 0; i < 8; i++)
		{
			CtrlInfo* cc = ShellFindCtrl(aszEdCName[i]);
			if (cc == NULL || cc->bufferInfo.buffer == NULL ||
				cc->bufferInfo.size < sizeof(DOUBLE))
			{
				abEdCAuth[i] = FALSE;	// 布局缺该格: 复位权威标志
				continue;
			}
			DOUBLE* pv = (DOUBLE*)cc->bufferInfo.buffer;
			DOUBLE vNew = (fac != NULL) ? (DOUBLE)fac->coeff.RefCoeffs[i] : 0.0;
			if (!abEdCAuth[i] || *pv == adEdCAuth[i])
			{
				*pv = vNew;
				adEdCAuth[i] = vNew;
				abEdCAuth[i] = TRUE;
				EdOwnerSet(aEdCOwner[i], sel, nSelected, nAxis);	// 记录该格来源(提交目标)
				continue;
			}
			// 否则: 该格未被回填(缓冲 != 权威 = 仍在编辑) ⇒ 跳过:
			//       既不改缓冲, 也不挪用该格的提交目标(提交目标在建立来源那次回填时已记下)
			continue;
		}
		{
			CtrlInfo* cc = ShellFindCtrl(aszEdOfsName);
			if (cc == NULL || cc->bufferInfo.buffer == NULL ||
				cc->bufferInfo.size < sizeof(DOUBLE))
			{
				bEdOfsAuth = FALSE;
			}
			else
			{
				DOUBLE* pv = (DOUBLE*)cc->bufferInfo.buffer;
				DOUBLE vNew = (fac != NULL) ? (DOUBLE)fac->offset : 0.0;
				if (!bEdOfsAuth || *pv == dEdOfsAuth)
				{
					*pv = vNew;
					dEdOfsAuth = vNew;
					bEdOfsAuth = TRUE;
					EdOwnerSet(edOfsOwner, sel, nSelected, nAxis);	// 记录该格来源(提交目标)
				}
				// 否则: 用户正在编辑该格, 跳过
			}
		}
	}
}

// 函数: EdApplyCoeff
// 描述: GENUI_ED_C0..C7 处理器核心(系数格 INPUT_REAL 提交事件触发): 读该格 double 缓冲
//       值(InputDouble 已把用户输入解析为数值)写选中控件当前轴因子系数并重算
//       几何; 无因子或未选中 → 忽略并把该格显示还原为 0
// 参数:
//		IN	idx:	系数索引 0..7(倍屏宽/倍屏高/倍窗宽/倍窗高/倍控件高/倍字号/倍间隙/倍内距)
static VOID EdApplyCoeff(INT idx)
{
	if (idx < 0 || idx >= 8)
	{
		return;
	}
	CtrlInfo* cc = ShellFindCtrl(aszEdCName[idx]);
	if (cc == NULL || cc->bufferInfo.buffer == NULL ||
		cc->bufferInfo.size < sizeof(DOUBLE))
	{
		return;
	}
	DOUBLE* pv = (DOUBLE*)cc->bufferInfo.buffer;
	// 提交目标 = 该格权威值的来源控件(镜像回填时记录)
	CtrlInfo* sel = EdOwnerCtrl(aEdCOwner[idx]);
	if (sel == NULL)
	{
		// 来源失效(尚未建立来源/控件已被删除/文档已替换): 放弃提交并归一权威副本, 下帧镜像刷新显示
		adEdCAuth[idx] = *pv;
		abEdCAuth[idx] = TRUE;
		return;
	}
	SizeFactor* fac = AxisFactorOf(sel, EdOwnerAxis(aEdCOwner[idx]));
	if (fac == NULL)
	{
		// 来源控件无几何因子: 忽略并把该格显示还原为 0
		*pv = 0.0;
		adEdCAuth[idx] = 0.0;
		abEdCAuth[idx] = TRUE;
		return;
	}
	// 数值有限性防护: 非有限值(超界/异常)不入库, 保持当前因子值(显示归一)
	if (_finite(*pv))
	{
		fac->coeff.RefCoeffs[idx] = (FLOAT)(*pv);
		EdRecalcSelGeometry(sel);
	}
	// 提交完成: 权威值 = 当前因子值(缓冲同步归一, 镜像不再视为编辑中)
	DOUBLE vNew = (DOUBLE)fac->coeff.RefCoeffs[idx];
	*pv = vNew;
	adEdCAuth[idx] = vNew;
	abEdCAuth[idx] = TRUE;
}

// 函数: EdApplyOffset
// 描述: GENUI_ED_OFS 处理器核心(像素固定偏移格 INPUT_REAL 提交事件触发): 读该格 double
//       缓冲值, 取整写选中控件当前轴因子 offset 并重算几何;
//       无因子/未选中语义同 EdApplyCoeff(还原 0); 偏移恒为整数(LONG)
static VOID EdApplyOffset()
{
	CtrlInfo* cc = ShellFindCtrl(aszEdOfsName);
	if (cc == NULL || cc->bufferInfo.buffer == NULL ||
		cc->bufferInfo.size < sizeof(DOUBLE))
	{
		return;
	}
	DOUBLE* pv = (DOUBLE*)cc->bufferInfo.buffer;
	// 提交目标 = 该格权威值的来源控件
	CtrlInfo* sel = EdOwnerCtrl(edOfsOwner);
	if (sel == NULL)
	{
		dEdOfsAuth = *pv;
		bEdOfsAuth = TRUE;
		return;
	}
	SizeFactor* fac = AxisFactorOf(sel, EdOwnerAxis(edOfsOwner));
	if (fac == NULL)
	{
		*pv = 0.0;
		dEdOfsAuth = 0.0;
		bEdOfsAuth = TRUE;
		return;
	}
	if (_finite(*pv))
	{
		fac->offset = (LONG)(*pv);
		EdRecalcSelGeometry(sel);
	}
	DOUBLE vNew = (DOUBLE)fac->offset;
	*pv = vNew;
	dEdOfsAuth = vNew;
	bEdOfsAuth = TRUE;
}

// 函数: EdElSpecOf
// 描述: 取来源控件对应的中间态元素规格(标签/文本/事件名的取写落点)
// 说明: 事件名不驻控件对象(控件只持像素几何与类型缓冲)⇒ 事件行的显示与提交都经中间态元素规格;
//       索引口径 = 文档索引(链内只含文档控件 ⇒ 即布局控件索引), 与中间态元素一一对应 ⇒ 直接用
//       承载者条目访问 Document::SpecAt(经条目反查)。
// 参数:
//		IN	sel:	来源控件(可为 NULL)
//		IN	idx:	该控件的文档索引
// 返回:
//		ElementSpec*: 该控件的中间态元素规格; sel 为空/索引越界/元素缺失时返回 NULL
static ElementSpec* EdElSpecOf(CtrlInfo* sel, INT idx)
{
	if (sel == NULL || idx < 0)
	{
		return NULL;
	}
	return EditorCommands::SpecAt(pMain, (size_t)idx);
}

// 函数: EdApplyLabel
// 描述: GENUI_ED_LBL 处理器核心(标签行失焦提交): 读该行文本缓冲, 写来源控件的中间态元素标签;
//       来源失效 ⇒ 放弃提交并把权威副本归一为缓冲值(下帧镜像按当前选中重填)
static VOID EdApplyLabel()
{
	char* buf = NULL;
	size_t cap = 0;
	// 1. 取该行文本缓冲(标签行缓冲容量 = 编辑器显示缓冲容量)
	buf = EdShellTxtBuf(aszEdLblName, &cap);
	if (buf == NULL)
	{
		return;
	}
	// 2. 提交目标 = 该行权威值来源控件(镜像回填时记录)
	CtrlInfo* sel = EdOwnerCtrl(edLblOwner);
	if (sel == NULL)
	{
		EdSetBufAuth(buf, cap, aszEdLblAuth, sizeof(aszEdLblAuth), buf);
		return;
	}
	// 3. 写中间态元素标签(布局对象侧的指针在同步时重指)
	ElementSpec* es = EdElSpecOf(sel, edLblOwner.nIdx);
	if (es != NULL)
	{
		strncpy_s(es->label, sizeof(es->label), buf, _TRUNCATE);
		// 预览同步: 内容控件重装会带上新标签(结构件就地刷新)
		PreviewSession::RequestPreviewSync();
	}
	// 4. 提交完成: 权威副本归一(镜像不再视为编辑中)
	EdSetBufAuth(buf, cap, aszEdLblAuth, sizeof(aszEdLblAuth), buf);
	nSynced = -2;
}

// 函数: EdApplyText
// 描述: GENUI_ED_TEXT 处理器核心(文本行失焦提交): 读该行文本缓冲, 写来源控件的中间态元素文本,
//       并调用中间态 -> 布局对象的同步刷新预览基座
// 说明: 预览同步(结构件就地刷新/内容控件重装)按 es->text 写控件缓冲。
static VOID EdApplyText()
{
	char* buf = NULL;
	size_t cap = 0;
	buf = EdShellTxtBuf(aszEdTxtName, &cap);
	if (buf == NULL)
	{
		return;
	}
	CtrlInfo* sel = EdOwnerCtrl(edTxtOwner);
	if (sel == NULL)
	{
		EdSetBufAuth(buf, cap, aszEdTxtAuth, sizeof(aszEdTxtAuth), buf);
		return;
	}
	ElementSpec* es = EdElSpecOf(sel, edTxtOwner.nIdx);
	if (es != NULL)
	{
		strncpy_s(es->text, sizeof(es->text), buf, _TRUNCATE);
		// 控件对象侧一并刷新(取址经 CtrlBuffer.h 的 CtrlTextBuffer: 文本类 = 该控件文本缓冲首址)
		EdWriteTextToBuffer(sel, buf);
		PreviewSession::RequestPreviewSync();
	}
	EdSetBufAuth(buf, cap, aszEdTxtAuth, sizeof(aszEdTxtAuth), buf);
	nSynced = -2;
}

// 函数: EdApplyEvent
// 描述: GENUI_ED_EVENT 处理器核心(事件行失焦提交): 读该行文本缓冲, 写来源控件的中间态元素 event 名
// 说明: 事件名存于中间态元素规格(控件对象只持事件处理函数指针, 不持名字);
//       内容控件重装即按新名重绑(结构件的事件绑定保留)。
static VOID EdApplyEvent()
{
	char* buf = NULL;
	size_t cap = 0;
	buf = EdShellTxtBuf(aszEdEvtName, &cap);
	if (buf == NULL)
	{
		return;
	}
	CtrlInfo* sel = EdOwnerCtrl(edEvtOwner);
	if (sel == NULL)
	{
		EdSetBufAuth(buf, cap, aszEdEvtAuth, sizeof(aszEdEvtAuth), buf);
		return;
	}
	ElementSpec* es = EdElSpecOf(sel, edEvtOwner.nIdx);
	if (es != NULL)
	{
		strncpy_s(es->event, sizeof(es->event), buf, _TRUNCATE);
		PreviewSession::RequestPreviewSync();
	}
	EdSetBufAuth(buf, cap, aszEdEvtAuth, sizeof(aszEdEvtAuth), buf);
	nSynced = -2;
}

// 函数: EdApplyPrev
// 描述: GENUI_ED_PREV 处理器核心(前序 ID 行失焦提交): 读该行整数缓冲, 写来源控件的中间态元素
//       parentID 与布局对象控件 parentID(两处同义), 并按父链整体重推层次与有效坐标
// 说明: ①取值归一: 负数一律归 -1(无父容器; 0 是合法 ID);
//       ②自指(前序 ID = 自身 ID)拒绝入库(保持现值), 成环仍允许声明(模型按无父降级, 见 Layout);
//       ③父链是层次与坐标的共同依据, 改后两推都必须做, 否则列表层序与内容控件位置不同步。
static VOID EdApplyPrev()
{
	CtrlInfo* cc = ShellFindCtrl(aszEdPrevName);
	if (cc == NULL || cc->bufferInfo.buffer == NULL ||
		cc->bufferInfo.size < sizeof(INT))
	{
		return;
	}
	INT* pv = (INT*)cc->bufferInfo.buffer;
	// 提交目标 = 该行权威值的来源控件(镜像回填时记录)
	CtrlInfo* sel = EdOwnerCtrl(edPrevOwner);
	if (sel == NULL)
	{
		// 来源失效(尚未建立来源/控件已被删除/文档已替换): 放弃提交并归一权威副本, 下帧镜像刷新显示
		nEdPrevAuth = *pv;
		bEdPrevAuth = TRUE;
		return;
	}
	ElementSpec* es = EdElSpecOf(sel, edPrevOwner.nIdx);
	if (es == NULL)
	{
		nEdPrevAuth = *pv;
		bEdPrevAuth = TRUE;
		return;
	}
	// 1. 归一与自指防护
	INT nNew = *pv;
	if (nNew < -1)
	{
		nNew = -1;			// 负数一律归"无父容器"
	}
	if (nNew >= 0 && nNew == sel->id)
	{
		nNew = es->parentID;	// 自指: 保持现值(不入库)
	}
	// 2. 落库(中间态元素规格 + 布局对象控件; 两处同义)
	es->parentID = nNew;
	sel->parentID = nNew;
	// 3. 父链重推(层次: 父层 + 自身类型步进; 坐标: 声明坐标 + 父链累加; 两推幂等)
	if (pMain != NULL)
	{
		pMain->DeriveCtrlLayers();
		pMain->DeriveCtrlPositions();
	}
	// 4. 目标缺失仅作诊断(声明值照存; 装配期按无父降级)
	if (nNew >= 0 && pMain != NULL && pMain->GetCtrlInfoByID(nNew) == NULL)
	{
		printf("[ED] 前序 ID 行: 目标 id=%d 不在本布局内(仅声明, 层次与坐标按无父处理)\n", nNew);
		fflush(stdout);
	}
	// 5. 显示归一(镜像下一帧不再视为编辑中) + 预览同步(内容控件按新父链重装)
	*pv = nNew;
	nEdPrevAuth = nNew;
	bEdPrevAuth = TRUE;
	PreviewSession::RequestPreviewSync();
}

// 函数: EdApplyAxis
// 描述: GENUI_ED_AXIS 处理器核心(尺寸轴下拉提交): 写本编辑区当前编辑轴(nAxis)
// 说明: 尺寸轴是编辑区的显示/取值轴, 不是文档数据(中间态元素恒有四个轴系数)
//       ⇒ 不改中间态、不请求预览同步; 系数格与偏移格下帧按新轴回填。
static VOID EdApplyAxis()
{
	CtrlInfo* cc = ShellFindCtrl(aszEdAxisName);
	if (cc == NULL || cc->bufferInfo.buffer == NULL ||
		cc->bufferInfo.size < sizeof(CtrlComboData))
	{
		return;
	}
	CONST CtrlComboData* cb = (CONST CtrlComboData*)cc->bufferInfo.buffer;
	if (cb->index < 0 || cb->index > 3)
	{
		return;
	}
	nAxis = (INT)cb->index;			// 属性区镜像/系数格下帧按新轴回填
	nSynced = -2;					// 强制属性区整块刷新
}

// 函数: OnEdSubmit
// 描述: GENUI_ED_LBL / GENUI_ED_TEXT / GENUI_ED_EVENT / GENUI_ED_AXIS 提交:
//       标签行 = 写来源控件的中间态元素标签; 文本行 = 写中间态元素文本并刷新控件缓冲;
//       事件行 = 写中间态元素 event 名; 尺寸轴 = 切换本编辑区当前编辑轴(不改文档数据)
// 时机: 帧间事件 —— 帧内按触发控件 label 定位槽位并把槽位进 param 挂帧间队列(操作码恒为规范码);
//       帧末排空时按 label 重取该控件与其缓冲(帧内持有的对象引用在帧末可能失效)
// 参数:
//		IN	obj:	触发控件(帧内用于定位槽位)
//		IN	oper:	规范操作码(帧末分派不使用; 槽位一律走 param)
//		IN	param:	帧末执行时承载槽位序号(ED_SLOT_*)
//		IN	data:	该控件缓冲(帧内不读取; 帧末按 label 重取)
VOID GenUIEditor::OnEdSubmit(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(data);
	// 1. 帧内: 由触发控件的 label 定位槽位, 并以 param 承载之挂入帧间队列
	if (FrameEvent::IsInFrame())
	{
		CtrlInfo* c = (CtrlInfo*)obj;
		INT nSlot = -1;
		if (c != NULL && c->label != NULL)
		{
			if (lstrcmpA(c->label, aszEdLblName) == 0)
			{
				nSlot = ED_SLOT_LABEL;
			}
			else if (lstrcmpA(c->label, aszEdTxtName) == 0)
			{
				nSlot = ED_SLOT_TEXT;
			}
			else if (lstrcmpA(c->label, aszEdEvtName) == 0)
			{
				nSlot = ED_SLOT_EVENT;
			}
			else if (lstrcmpA(c->label, aszEdAxisName) == 0)
			{
				nSlot = ED_SLOT_AXIS;
			}
		}
		if (nSlot < 0)
		{
			return;
		}
		// 挂载: oper = 编辑族规范码 · param = 槽位序号(同帧不同槽位 param 不同 ⇒ 不会被误合并)
		FrameEvent::Post(GenUIEditor::OnEdSubmit, NULL, OperCode::MakeCodeOwn(OPER_EDIT_SUBMIT), (uint64_t)nSlot, NULL);
		return;
	}
	// 2. 帧末: 按槽位(param)分派到各提交核心(核心内部按 label 重取控件与其缓冲)
	switch ((INT)param)
	{
	case ED_SLOT_LABEL:
		EdApplyLabel();
		MarkDocDirtyCore();		// 标签落库 = 文档改动(未保存)
		break;
	case ED_SLOT_TEXT:
		EdApplyText();
		MarkDocDirtyCore();		// 文本落库 = 文档改动(未保存)
		break;
	case ED_SLOT_EVENT:
		EdApplyEvent();
		MarkDocDirtyCore();		// 事件名落库 = 文档改动(未保存)
		break;
	case ED_SLOT_AXIS:
		EdApplyAxis();						// 只切本编辑区当前轴(界面临时状态, 非文档改动)
		break;
	default:
		break;
	}
}

// 函数: OnEdPrev
// 描述: GENUI_ED_PREV 提交(控件编辑区前序 ID 行失焦提交)
// 时机: 帧间事件 —— 帧内只挂帧间队列(操作码恒为规范码); 帧末排空时按 label 重取该行控件与其缓冲
// 参数:
//		IN	obj:	触发控件(帧内不使用; 帧末按 label 重取)
//		IN	oper:	规范操作码(帧末分派不使用)
//		IN	param:	帧末执行时承载槽位序号(恒 ED_SLOT_PREV)
//		IN	data:	该控件缓冲(帧内不读取)
VOID GenUIEditor::OnEdPrev(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(obj);
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(param);
	UNREFERENCED_PARAMETER(data);
	// 1. 帧内: 只挂帧间队列(槽位固定, 不依赖触发控件)
	if (FrameEvent::IsInFrame())
	{
		FrameEvent::Post(GenUIEditor::OnEdPrev, NULL, OperCode::MakeCodeOwn(OPER_EDIT_SUBMIT),
			(uint64_t)ED_SLOT_PREV, NULL);
		return;
	}
	// 2. 帧末: 提交核心(内部按 label 重取该行控件与其缓冲)
	EdApplyPrev();
	MarkDocDirtyCore();		// 前序 ID 落库 = 文档改动(未保存)
}

// ================================
// 区域: 控件编辑区(ED) 九格提交(8 系数 + 像素偏移)
//   与 ED 四类同一时机模型: 帧内只按触发控件 label 定位格号并挂帧间队列;
//   帧末执行提交核心 EdApplyCoeff/EdApplyOffset —— 写中间态元素规格(几何来源)后
//   就地重解该控件像素几何(见应用侧 ApplyDocCtrlGeometry 提交入口) ⇒ 预览窗口下一帧即生效。
//   触发源 = 控件库 Controls::InputReal 的 ImGui::InputDouble 返回值(值提交/失焦);
//   绑定表(UIEventHandler.cpp)把 ##FAC_SW_EDIT..##FAC_PAD_EDIT / ##FAC_OFS_EDIT 的 event 名映射到本区域处理器。
// ================================

// 函数: OnEdCell
// 描述: GENUI_ED_C0..C7 / GENUI_ED_OFS 提交(系数格/像素偏移格失焦提交)
// 时机: 帧间事件 —— 帧内按 label 定位格号并挂载(格号走 param); 帧末按格号分派到系数/偏移提交核心
// 参数:
//		IN	obj:	触发控件(帧内用于定位格号)
//		IN	oper:	规范操作码(帧末分派不使用; 格号一律走 param)
//		IN	param:	帧末执行时承载格号(0..7 = 系数; 9 = 像素偏移)
//		IN	data:	该控件缓冲(帧内不读取; 帧末按 label 重取)
VOID GenUIEditor::OnEdCell(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(oper);
	UNREFERENCED_PARAMETER(data);
	// 1. 帧内: 由触发控件 label 定位格号, 并以 param 承载之挂入帧间队列
	if (FrameEvent::IsInFrame())
	{
		CtrlInfo* c = (CtrlInfo*)obj;
		INT nCell = -1;
		if (c != NULL && c->label != NULL)
		{
			if (lstrcmpA(c->label, aszEdOfsName) == 0)
			{
				nCell = ED_CELL_OFS;
			}
			else
			{
				for (INT i = 0; i < 8; i++)
				{
					if (lstrcmpA(c->label, aszEdCName[i]) == 0)
					{
						nCell = i;
						break;
					}
				}
			}
		}
		if (nCell < 0)
		{
			return;			// 非九格触发(不该发生): 不挂载
		}
		// 挂载: oper = 编辑族规范码 · param = 格号(同帧不同格 param 不同 ⇒ 不会被误合并)
		FrameEvent::Post(GenUIEditor::OnEdCell, NULL, OperCode::MakeCodeOwn(OPER_EDIT_SUBMIT), (uint64_t)nCell, NULL);
		return;
	}
	// 2. 帧末: 按格号(param)分派到提交核心(核心内部按 label 重取控件与其缓冲)
	if ((INT)param == ED_CELL_OFS)
	{
		EdApplyOffset();
		MarkDocDirtyCore();		// 偏移落库 = 文档改动(未保存)
	}
	else
	{
		EdApplyCoeff((INT)param);
		MarkDocDirtyCore();		// 系数落库 = 文档改动(未保存)
	}
}

// ================================
// 区域: 列表交互意图协议
//   ① 列表控件(Controls::ListInput / ListCombo)绘制时收集基础部件返回的操作码, 帧末经该控件
//      自身的 pEventHandler 回传(oper = 规范操作码: 专属段 = 行件结论 / 搭载段 = 行内部件码;
//      param = 来源行号);
//   ② 事件处理器(经 UIEventHandler 绑定表装配)把规范操作码解释为编辑器业务意图并改文档。
// ================================

// --------------------------------
// 编辑器列表交互意图码
// 说明: 控件层只上报"原始交互结论"(OPER 操作码); 编辑器把结论映射为自身业务意图再改文档
//       (点选 / 删除 / 新增 / 改名 / 改类型 / 重排)。本区意图码是编辑器业务码 —— 不是控件操作码,
//       不进 OPER 操作码空间、不与 OPER_* 混用、只在本文件内由 ApplyListOper / ApplyListMove 消费。
//       重排(拖拽)不设意图码: 由 OnListRowEvent 直接调 ApplyListMove(源行号, 目标插入位置)。
// --------------------------------
constexpr DWORD LIST_INTENT_NONE = 0;		// 无意图(不产生任何改动)
constexpr DWORD LIST_INTENT_SELECT = 1;		// 点选行(布局列表 = 切 active 布局; 控件列表 = 选中控件)
constexpr DWORD LIST_INTENT_DELETE = 2;		// 删除行
constexpr DWORD LIST_INTENT_ADD = 3;		// 尾行新增(新增名 / 新增类型取自装配方既有输入态)
constexpr DWORD LIST_INTENT_RENAME = 4;		// 行文本提交 = 改名(行文本 = 新名; 布局列表用)
constexpr DWORD LIST_INTENT_SETTYPE = 5;	// 行下拉变更 = 改类型(行下拉选中项 = 新类型; 控件列表用)

// 函数: ListIntentOper
// 描述: 把"行号 + 编辑器意图"打包成意图操作码(低 8 位 = 意图; 便于同一入口携带行号)
// 参数:
//		IN	dwRow:		行号(0 起)
//		IN	dwIntent:	意图码(LIST_INTENT_*)
// 返回:
//		DWORD: 意图操作码
static DWORD ListIntentOper(DWORD dwRow, DWORD dwIntent)
{
	return (dwRow << 8) | (dwIntent & 0xFF);
}
// 函数: ListIntentRow
// 描述: 取意图操作码中的行号
// 参数:
//		IN	dwOper:	意图操作码(ListIntentOper 打包)
// 返回:
//		DWORD: 行号
static DWORD ListIntentRow(DWORD dwOper)
{
	return dwOper >> 8;
}
// 函数: ListIntentOf
// 描述: 取意图操作码中的意图
// 参数:
//		IN	dwOper:	意图操作码(ListIntentOper 打包)
// 返回:
//		DWORD: 意图码(LIST_INTENT_*)
static DWORD ListIntentOf(DWORD dwOper)
{
	return dwOper & 0xFF;
}

// 函数: ListDataOf
// 描述: 取列表控件的缓冲头(ListBuffer)
// 参数:
//		IN	pListCtrl:	列表控件指针(可为 NULL)
// 返回: ListBuffer* 缓冲头; 无控件/无缓冲/尺寸不足时返回 NULL
static ListBuffer* ListDataOf(CtrlInfo* pListCtrl)
{
	if (pListCtrl == NULL)
	{
		// 无控件对象时无承载缓冲
		return NULL;
	}
	if (pListCtrl->bufferInfo.buffer == NULL ||
		pListCtrl->bufferInfo.size < sizeof(ListBuffer))
	{
		return NULL;
	}
	return (ListBuffer*)pListCtrl->bufferInfo.buffer;
}

// 注: 两列表的列表归属不再编码进操作码 —— 列表归属由控件自身承载(##LAYOUTS / ##CONTROLS 标签唯一,
//     帧末以"挂载时保存的控件指针"与壳内两列表指针比对判定; 见 OnListRowEvent)。

// 函数: OnListRowEvent
// 描述: GENUI_LIST_LM / GENUI_LIST_CL(外壳两个列表的行事件): 把行件上报的规范操作码解释为列表意图,
//       交意图消费者 ApplyListOper 改文档(点选 / 改名 / 删除 / 改类型); 拖拽重排经同一入口交 ApplyListMove
// 时机: 帧间事件 —— 帧内只做"来源控件是两列表之一"的准入判定, 随后把 控件指针(obj) + 规范操作码(oper)
//       + 行号(param) 原样挂帧间队列; 帧末按控件指针定位列表归属并重取该列表控件与其缓冲
// 说明: ①列表归属不塞进操作码: 操作码三段只放规范结论(专属段 = 行件结论 / 搭载段 = 行内部件码);
//         列表归属在帧末由"挂载时保存的控件指针与当前壳内 ##LAYOUTS / ##CONTROLS 指针比对"判定(只比较指针值,
//         不解引用 ⇒ 即便帧末指针已失效也不会读脏内存); 两列表标签在外壳内唯一, 指针亦唯一 ⇒ 判定无歧义。
//       ②操作码解码: 专属段 = 行件结论(OPER_LIST_ROW_MOVED = 拖拽重排; 0 = 普通行内事件);
//         搭载段(搭载标志 = OPER_PAYLOAD_OPCODE 时)= 行内部件码(OPER_CUSTOM_IA_* / OPER_CUSTOM_CA_*);
//         点选(行被激活)由公共位 OPER_COMMON_ACTIVE 上报(沿用 OPER_LIST_ROW_ACTIVATED 亦可识别)。
// 参数:
//		IN	obj:	触发控件(帧内用于准入; 帧末用于定位列表归属, 只比较指针值)
//		IN	oper:	规范操作码(三段编码, 帧内忽略, 帧末解码)
//		IN	param:	来源行号(普通行 = 行号; 重排 = 源行号)
//		IN	data:	未用(帧间挂载时传 NULL; 帧末按控件指针重取该列表缓冲)
VOID GenUIEditor::OnListRowEvent(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(data);
	// 1. 帧内: 准入判定(只认两列表)⇒ 原样挂帧间队列; 操作码保持规范编码(列表归属不进 oper)
	if (FrameEvent::IsInFrame())
	{
		CtrlInfo* c = (CtrlInfo*)obj;
		if (c == NULL || c->label == NULL ||
			(lstrcmpA(c->label, "##LAYOUTS") != 0 && lstrcmpA(c->label, "##CONTROLS") != 0))
		{
			return;
		}
		// 挂载: obj = 列表控件(帧末据此定位列表归属; 同时使两列表同帧事件不被合并) · param = 行号
		// (合并判据含 oper/param ⇒ 同帧不同行 param 不同, 不会被误合并)
		FrameEvent::Post(GenUIEditor::OnListRowEvent, obj, oper, param, NULL);
		return;
	}
	// 2. 帧末: 列表归属定位(指针比对, 不解引用) + 操作码解码 + 按控件指针重取该列表与其缓冲
	{
		CONST uint16_t wOwn = (uint16_t)OperCode::GetSegment(oper, OperCode::OPER_SEG_OWN);		// 专属段: 行件结论(重排 / 0)
		CONST uint16_t wKind = OperCode::GetPayloadKind(oper);				// 公共段搭载标志
		// 行内部件码一律取搭载段(搭载标志 = 操作码时的低 16 位);
		// 不得回落专属段: 专属段允许重码(OPER_LIST_ROW_ACTIVATED == OPER_CUSTOM_IA_INPUT_SUBMIT == 0x0001)
		//   ⇒ 回落会把"点选"误读为"提交(改名)"
		CONST DWORD dwPart = (wKind == OPER_PAYLOAD_OPCODE)
			? (DWORD)(uint16_t)(OperCode::GetSegment(oper, OperCode::OPER_SEG_PAYLOAD) & 0xFFFF)	// 行内部件码(低 16 位)
			: 0;													// 未搭载操作码 ⇒ 无部件码
		CONST DWORD dwRow = (DWORD)param;							// 行号(重排 = 源行号)
		CtrlInfo* pLM = ShellFindCtrl("##LAYOUTS");
		CtrlInfo* pCL = ShellFindCtrl("##CONTROLS");
		CONST BOOL bLayoutList = (pLM != NULL && pLM == (CtrlInfo*)obj) ? TRUE : FALSE;
		CtrlInfo* pListCtrl = NULL;
		if (bLayoutList)
		{
			pListCtrl = pLM;
		}
		else if (pCL != NULL && pCL == (CtrlInfo*)obj)
		{
			pListCtrl = pCL;
		}
		else
		{
			// 两列表指针都不符(理论上仅出现在"外壳在两挂载之间被重建"): 行号已失去所指缓冲 ⇒ 丢弃并诊断
			printf("[GENUI] 列表行事件: 列表归属无法判定(挂载控件指针已失效) ⇒ 丢弃该行事件\n");
			return;
		}
		ListBuffer* pList = ListDataOf(pListCtrl);
		BYTE* lpItem = NULL;
		BOOL bSpecial = FALSE;
		if (pList == NULL)
		{
			return;
		}
		// 拖拽重排(专属段 = OPER_LIST_ROW_MOVED): 目标插入位置在搭载段(数据搭载), 源行号在 param
		// ⇒ 不进"行块"路径, 直接交重排消费者
		if (wOwn == OPER_LIST_ROW_MOVED)
		{
			ApplyListMove((bLayoutList != FALSE) ? CTRL_TYPE_LIST_INPUT : CTRL_TYPE_LIST_COMBO,
				dwRow, (DWORD)OperCode::GetSegment(oper, OperCode::OPER_SEG_PAYLOAD));
			return;
		}
		// 特殊行(新增行)行号 = 项目数量; 其余行号须落在项目行内(列表可能已被重填)
		bSpecial = (dwRow == pList->dwCount) ? TRUE : FALSE;
		if (!bSpecial && dwRow >= pList->dwCount)
		{
			return;
		}
		lpItem = bSpecial ? (BYTE*)(pList + 1) : ((BYTE*)(pList + 1) + (size_t)pList->cbItem * (dwRow + 1));
		// 点选(行被激活): 控件层两列表的行件(Widgets::Base::InputActionRO / ComboAction)一律以公共位
		// OPER_COMMON_ACTIVE 上报(见 Controls.cpp InputAction/ComboAction 部件) ⇒ 主判据为公共位;
		// 兼容列表族专属码 OPER_LIST_ROW_ACTIVATED(该码当前无控件层产出点, 仅为将来行件改以专属码
		// 上报点选时预留)
		CONST BOOL bActivated =
			(OperCode::HasCommon(oper, OPER_COMMON_ACTIVE) || (wOwn == OPER_LIST_ROW_ACTIVATED)) ? TRUE : FALSE;
		if (bSpecial)
		{
			// ---- 新增行: LM = 输入布局名后提交/点 "+" ⇒ 新增布局; CL = 选类型后点按钮 ⇒ 新增控件 ----
			if (bLayoutList != FALSE)
			{
				CtrlInputActionData* pSpec = (CtrlInputActionData*)lpItem;
				CHAR* lpText = CtrlInputActionText(pSpec);
				CONST BOOL bSubmit = (dwPart == OPER_CUSTOM_IA_INPUT_SUBMIT) ? TRUE : FALSE;
				CONST BOOL bClick = (dwPart == OPER_CUSTOM_IA_BTN_CLICKED) ? TRUE : FALSE;
				if ((bSubmit || bClick) && lpText != NULL && lpText[0] != '\0')
				{
					strncpy_s(szNewLayout, sizeof(szNewLayout), lpText, _TRUNCATE);
					if (ApplyListOper(CTRL_TYPE_LIST_INPUT, ListIntentOper(0, LIST_INTENT_ADD)))
					{
						lpText[0] = '\0';			// 新增成功: 清空新增行输入(等下一个名字)
					}
					szNewLayout[0] = '\0';
				}
			}
			else
			{
				CtrlComboData* pSpec = (CtrlComboData*)lpItem;
				if (dwPart == OPER_CUSTOM_CA_BTN_CLICKED)
				{
					// 该行下拉选中项 = 目标类型(类型表行序) ⇒ 映射为"新增下拉序号"后交意图消费者
					CONST INT idx = pSpec->index;
					if (idx >= 0 && idx < (INT)CTRL_TYPE_INFO_NUM && (aCtrlTypeInfo[idx].nPalette >= 0))
					{
						nPaletteIdx = aCtrlTypeInfo[idx].nPalette;
						ApplyListOper(CTRL_TYPE_LIST_COMBO, ListIntentOper(0, LIST_INTENT_ADD));
					}
				}
			}
			return;
		}
		if (bLayoutList != FALSE)
		{
			// 布局行(行块 = INPUT_ACTION 块): 行激活 = 切 active / 部件提交 = 改名 / 部件按钮 = 删除
			CtrlInputActionData* pData = (CtrlInputActionData*)lpItem;
			if (bActivated != FALSE)
			{
				ApplyListOper(CTRL_TYPE_LIST_INPUT, ListIntentOper(dwRow, LIST_INTENT_SELECT));
			}
			if (dwPart == OPER_CUSTOM_IA_INPUT_SUBMIT)
			{
				// 行文本 = 新布局名(装配方输入态 = szNewLayout): 交意图消费者改名
				CHAR* lpText = CtrlInputActionText(pData);
				if (lpText != NULL && lpText[0] != '\0' &&
					(dwRow < (DWORD)SHELL_LIST_MAX_ROWS))
				{
					strncpy_s(szNewLayout, sizeof(szNewLayout), lpText, _TRUNCATE);
					if (ApplyListOper(CTRL_TYPE_LIST_INPUT, ListIntentOper(dwRow, LIST_INTENT_RENAME)))
					{
						// 权威副本随提交更新(该行缓冲已是新名 ⇒ 后续回填以新名为准)
						strncpy_s(s_aszLmRowAuth[dwRow], SHELL_LIST_AUTH_CHARS, szNewLayout, _TRUNCATE);
					}
					szNewLayout[0] = '\0';
				}
			}
			if (dwPart == OPER_CUSTOM_IA_BTN_CLICKED)
			{
				ApplyListOper(CTRL_TYPE_LIST_INPUT, ListIntentOper(dwRow, LIST_INTENT_DELETE));
			}
		}
		else
		{
			// 控件行(行块 = COMBO 块): 行激活 = 选中控件 / 部件变更 = 改类型 / 部件按钮 = 删除
			CtrlComboData* pData = (CtrlComboData*)lpItem;
			if (bActivated != FALSE)
			{
				ApplyListOper(CTRL_TYPE_LIST_COMBO, ListIntentOper(dwRow, LIST_INTENT_SELECT));
			}
			if (dwPart == OPER_CUSTOM_CA_COMBO_CHANGED)
			{
				// 该行下拉选中项 = 目标类型(选项表 = 类型信息表视图 ⇒ 索引即表内行序)
				if (pData->index >= 0 && pData->index < (INT)CTRL_TYPE_INFO_NUM &&
					(dwRow < (DWORD)SHELL_LIST_MAX_ROWS))
				{
					nRowTypeIdx = pData->index;
					if (ApplyListOper(CTRL_TYPE_LIST_COMBO, ListIntentOper(dwRow, LIST_INTENT_SETTYPE)))
					{
						// 权威副本随提交更新(该行下拉已是新类型 ⇒ 后续回填以新类型为准)
						(void)nRowTypeIdx;
					}
				}
			}
			if (dwPart == OPER_CUSTOM_CA_BTN_CLICKED)
			{
				ApplyListOper(CTRL_TYPE_LIST_COMBO, ListIntentOper(dwRow, LIST_INTENT_DELETE));
			}
		}
	}
}
// 函数: ApplyListOper
// 描述: 解释意图操作码并改文档(不含任何绘制; 供列表行事件消费者调用)
//       越界行号/空意图返回 FALSE(不改文档); 布局 ≥1 在此二次校验
// 参数:
//		IN	type:	列表控件类型(CTRL_TYPE_LIST_INPUT / CTRL_TYPE_LIST_COMBO)
//		IN	dwOper:	编辑器意图操作码(ListIntentOper 打包: 行号 + LIST_INTENT_*)
// 返回: BOOL TRUE = 意图已应用(文档发生变化); FALSE = 无意图/被拒
static BOOL ApplyListOper(CtrlType type, DWORD dwOper)
{
	BOOL bRet = FALSE;
	const DWORD dwAct = ListIntentOf(dwOper);
	const DWORD dwRow = ListIntentRow(dwOper);
	if (dwAct == LIST_INTENT_NONE)
	{
		return bRet;					// 无意图: 直接返回(无副作用)
	}
	if (type == CTRL_TYPE_LIST_INPUT)
	{
		// 布局列表(##LAYOUTS): 点选 = 切 active 布局; 删除 = 移除布局(保底 1); 新增 = 取新增行输入态
		if (dwAct == LIST_INTENT_SELECT)
		{
			if (dwRow < DocCountCore())
			{
				Application::SetActiveIndex(dwRow);
				pMain = ActiveLayoutCore();
				ResetSelectionLocal();
				bRet = TRUE;
			}
		}
		else if (dwAct == LIST_INTENT_DELETE)
		{
			if (DocCountCore() > 1 && dwRow < DocCountCore())
			{
				EditorCommands::RemoveDocLayout(dwRow);	// 布局族命令已归命令层
				pMain = ActiveLayoutCore();
				ResetSelectionLocal();
				MarkDocDirtyCore();	// 删除布局 = 文档改动(未保存)
				bRet = TRUE;
			}
		}
		else if (dwAct == LIST_INTENT_RENAME)
		{
			// 改名: 新名 = szNewLayout(装配方输入态 = 行内输入框提交文本);
			// 布局显示名由填行桥每帧重读 ⇒ 无需重解几何
			if (dwRow < DocCountCore() && szNewLayout[0] != '\0')
			{
				bRet = EditorCommands::SetDocLayoutName(dwRow, szNewLayout);
				if (bRet)
				{
					MarkDocDirtyCore();	// 布局改名 = 文档改动(未保存)
				}
			}
		}
		else if (dwAct == LIST_INTENT_ADD)
		{
			if (szNewLayout[0] != '\0')
			{
				EditorCommands::AddDocLayout(szNewLayout);	// 布局族命令已归命令层
				szNewLayout[0] = '\0';		// 新增后清空输入缓冲
				pMain = ActiveLayoutCore();
				ResetSelectionLocal();
				MarkDocDirtyCore();	// 新增布局 = 文档改动(未保存)
				bRet = TRUE;
			}
		}
	}
	else if (type == CTRL_TYPE_LIST_COMBO)
	{
		// 控件列表(##CONTROLS): 点选 = 选中控件(同步属性区); 删除 = 删除该行控件; 新增 = 取下拉类型
		if (pMain == NULL)
		{
			return bRet;
		}
		if (dwAct == LIST_INTENT_SELECT)
		{
			if (dwRow < (DWORD)pMain->GetTotalNum())
			{
				// 行号 = 文档索引(链内只含文档控件 ⇒ 即布局控件索引);
				// 结构件可选中 ⇒ 点结构件行同样建立选中(仅不画预览叠加件)
				nSelected = (INT)dwRow;
				nSynced = -2;				// 触发属性区重新同步
				SelectionOverlay::SetSelectedIndex(nSelected);	// 选中态即刻就位(结构件/无选中 ⇒ 不绘制)
				bRet = TRUE;
			}
		}
		else if (dwAct == LIST_INTENT_DELETE)
		{
			// 删除: 直接按文档索引定位(不再"先选中再删");
			// 可删性由核心 CanDeleteCtrl 判定 —— 必需结构件 WND/CNTR 拒绝, 其余(含 TITLE)可删
			if (DeleteCtrlAtDocIndex((INT)dwRow))
			{
				MarkDocDirtyCore();	// 删除控件 = 文档改动(未保存)
				bRet = TRUE;
			}
		}
		else if (dwAct == LIST_INTENT_SETTYPE)
		{
			// 改类型: 目标类型 = nRowTypeIdx(类型信息表行序; 装配方输入态 = 行下拉选中项);
			// 结构件与列表沿用删除保护(不可改类型)
			if (dwRow < (DWORD)pMain->GetTotalNum() &&
				nRowTypeIdx >= 0 && nRowTypeIdx < (INT)CTRL_TYPE_INFO_NUM)
			{
				CtrlInfo* c = CtrlOfDocIndex(pMain, (INT)dwRow);
				const CtrlType tNew = aCtrlTypeInfo[nRowTypeIdx].type;
				if (c != NULL && CanDeleteCtrl(c) &&
					Application::SetCtrlType(pMain, dwRow, tNew))
				{
					// 改类型后选中跟随该行(结构件槽位亦允许选中)
					nSelected = (INT)dwRow;
					nSynced = -2;				// 属性区重新同步(类型行随之刷新)
					SelectionOverlay::SetSelectedIndex(nSelected);	// 选中态即刻就位
					PreviewSession::RequestPreviewSync();	// 改链式结构(改类型) ⇒ 统一走帧末同步
					MarkDocDirtyCore();	// 改类型 = 文档改动(未保存)
					bRet = TRUE;
				}
			}
		}
		else if (dwAct == LIST_INTENT_ADD)
		{
			if (nPaletteIdx < 0 || nPaletteIdx >= PALETTE_NUM) { nPaletteIdx = 0; }
			// 结构件准入: ①窗口区/内容区不入"新增下拉"(nPalette < 0 ⇒ 选不到);
			// ②新增行选中 TITLE 时拒绝添加
			if (PaletteType(nPaletteIdx) == CTRL_TYPE_TITLE)
			{
				return bRet;			// 拒绝新增标题栏
			}
			bRet = GenUIEditor::AddControlCore(PaletteType(nPaletteIdx));
		}
	}
	return bRet;
}
// 函数: ApplyListMove
// 描述: 拖拽重排意图的消费者: 把"源行 -> 目标位置"落到文档(布局顺序 / 控件顺序)
// 说明: ①布局列表(##LAYOUTS): 行号 = 布局索引 ⇒ 命令层 MoveDocLayout(整份布局链重排,
//          激活索引随之跟随);
//       ②控件列表(##CONTROLS): 行号 = 控件索引(该列表含结构件) ⇒ 结构件保护: 结构件(WND/TITLE/CNTR,
//          即前 GetShellNum 槽)不可被拖动, 普通控件亦不得插到结构件区之前(结构件须占前若干槽);
//       ③索引口径换算: 目标位置 = 插入位置(可等于行数, 表示末尾), 因源已被移出 ⇒ 其后的目标位置自减一位;
//          换算后与源同处 ⇒ 视为无操作(拖回原位);
//       ④控件列表移动成功后选中跟随被移动项(与"新增控件即选中"口径一致)并触发属性区重新同步。
// 参数:
//		IN	type:	列表控件类型(CTRL_TYPE_LIST_INPUT / CTRL_TYPE_LIST_COMBO)
//		IN	dwSrc:	源行号(= 布局索引 / 控件索引)
//		IN	dwDest:	目标插入位置(0..行数; 行数 = 末尾)
// 返回: BOOL TRUE = 已重排(文档发生变化); FALSE = 无意图/被拒
static BOOL ApplyListMove(CtrlType type, DWORD dwSrc, DWORD dwDest)
{
	BOOL bRet = FALSE;
	if (type == CTRL_TYPE_LIST_INPUT)
	{
		// 布局列表: 源/目标都是布局索引; 保底 1 个布局(不可移到空)
		CONST DWORD dwCount = DocCountCore();
		if (dwCount <= 1 || dwSrc >= dwCount)
		{
			return bRet;
		}
		if (dwDest > dwCount)
		{
			dwDest = dwCount;					// 末尾
		}
		{
			DWORD dwTo = dwDest;
			if (dwTo > dwSrc)
			{
				dwTo--;							// 删除源之后的插入索引
			}
			if (dwTo == dwSrc)
			{
				return bRet;					// 拖回原位: 无操作
			}
			if (dwTo >= dwCount)
			{
				dwTo = dwCount - 1;
			}
			bRet = EditorCommands::MoveDocLayout(dwSrc, dwTo);	// 布局族命令已归命令层
			if (bRet) { PreviewSession::RequestPreviewSync(); }	// 改链式结构(搬移布局) ⇒ 统一走帧末同步
		}
		if (bRet)
		{
			MarkDocDirtyCore();		// 布局重排 = 文档改动(未保存)
		}
		return bRet;
	}
	if (type == CTRL_TYPE_LIST_COMBO)
	{
		if (pMain == NULL)
		{
			pMain = ActiveLayoutCore();
		}
		if (pMain == NULL || pMain->IsInvalid())
		{
			return bRet;
		}
		CONST size_t nDoc = pMain->GetTotalNum();	// 文档面控件数(链内只含文档控件 ⇒ 即布局控件数)
		CONST size_t nShell = pMain->GetShellNum();
		if (dwSrc >= nDoc)
		{
			return bRet;
		}
		if ((size_t)dwSrc < nShell)
		{
			return bRet;						// 结构件不可拖动(须占布局头部)
		}
		if (dwDest < (DWORD)nShell)
		{
			dwDest = (DWORD)nShell;				// 不得插到结构件区之前
		}
		if (dwDest > (DWORD)nDoc)
		{
			dwDest = (DWORD)nDoc;				// 末尾
		}
		{
			size_t dwTo = (size_t)dwDest;
			if (dwTo > (size_t)dwSrc)
			{
				dwTo--;							// 删除源之后的插入索引
			}
			if (dwTo == (size_t)dwSrc)
			{
				return bRet;					// 拖回原位: 无操作
			}
			if (dwTo < nShell)
			{
				return bRet;					// 落点落到结构件区之前 ⇒ 拒绝(结构件须占前若干槽)
			}
			if (dwTo >= nDoc)
			{
				dwTo = nDoc - 1;
			}
			if (Application::MoveCtrl(pMain, (size_t)dwSrc, dwTo))
			{
				PreviewSession::RequestPreviewSync();	// 改链式结构(搬移控件) ⇒ 统一走帧末同步
				nSelected = (INT)dwTo;			// 选中跟随被移动的控件(文档索引)
				nSynced = -2;					// 属性区重新同步
				SelectionOverlay::SetSelectedIndex(nSelected);	// 选中态即刻就位
				MarkDocDirtyCore();	// 控件重排 = 文档改动(未保存)
				bRet = TRUE;
			}
		}
	}
	return bRet;
}

// ================================
// 区域: 预览布局内容控件点击记录器实现
// 说明: ① 本处理器经 HostContext::SetInjectClickHandler 注入装配点 ⇒ 预览布局的每枚内容控件
//   装配时其 pEventHandler 一律被改绑到本函数(覆盖文档 event 的原处理器, 不转调) ——
//   预览是展示面, 点控件只记录点击, 不触发真实业务事件(保存/新建/打开一类不得由预览驱动);
//   ② 另有渲染出口(LayoutRender::RenderPreviewBase)对"控件库自身不派发点击"的类型统一派发到本函数;
//   ③ 记录字段 = 索引 + 像素位置 + 像素大小; 载体 = 编辑器内存 + stdout 日志。
// ================================

// 函数: OnPreviewClick
// 描述: 预览布局内容控件的点击事件处理器(注入给 Application 的统一记录器): 记录被点击控件在布局
//       对象中的索引与像素几何, 并即时打印一条 [PREVIEWCLICK] 日志(stdout)
// 说明: ① 两条派发来源共用本函数 —— (a) 控件库自身派发(BUTTON / 绑定了事件的 TEXT/RECT;
//          param 不含索引) (b) 渲染出口派发(其余类型; param = 控件索引) ⇒ 索引一律以"按控件指针
//          在预览基座内反查"为准, param 仅作兜底;
//       ② 只登记点击类事件(控件库还会派发提交 / 变更 / 电平类操作码 ⇒ 一律忽略);
//       ③ 同帧同控件去重(两条来源理论上互斥, 此处仅作防御); ④ 记录为覆盖式(只保留最近一次);
//       ⑤ 不改文档、不写盘、不移动任何窗口; ⑥ 结构件(索引 < GetShellNum())不参与。
// 参数:
//		IN	obj:	被点击的控件(CtrlInfo*)
//		IN	oper:	规范操作码(公共段含 OPER_COMMON_CLICKED 时才登记)
//		IN	param:	渲染出口派发时携带的控件索引(控件库派发时为 0; 反查失败时作兜底)
//		IN	data:	该控件缓冲(本处理器不使用)
VOID GenUIEditor::OnPreviewClick(VOID* obj, uint64_t oper, uint64_t param, void* data)
{
	UNREFERENCED_PARAMETER(data);
	CtrlInfo* c = (CtrlInfo*)obj;
	if (c == NULL)
	{
		return;
	}
	// 1. 只登记"点击"(控件库的提交 / 变更 / 电平类操作码一律忽略)
	if (OperCode::HasCommon(oper, OPER_COMMON_CLICKED) == FALSE)
	{
		return;
	}
	// 2. 反查基座索引(权威 = 控件在预览布局基座内的位置; 反查失败回落渲染出口携带的 param)
	Layout* pLay = ActiveLayoutCore();
	if (pLay == NULL || pLay->IsInvalid())
	{
		return;
	}
	INT nCtrl = CtrlIndexOf(pLay, c);
	if (nCtrl < 0)
	{
		nCtrl = (INT)param;
	}
	if (nCtrl < 0 || (size_t)nCtrl >= pLay->GetTotalNum())
	{
		return;
	}
	// 3. 结构件不参与(判定范围只含内容控件)
	if ((size_t)nCtrl < pLay->GetShellNum())
	{
		return;
	}
	// 4. 索引口径: 链内只含文档控件(选中态表达件已在链外) ⇒ 布局控件索引即文档索引
	//    (与中间态元素 / 控件列表行号同口径)
	CONST INT nIndex = nCtrl;
	// 6. 同帧裁决: 一次点击只保留视觉最上层的一枚(绘制顺序升序 ⇒ 索引最大 = 最后绘制 = 最上层)
	//    说明: 控件库自身派发点击的类型(BUTTON / 绑事件的 TEXT/RECT, 完整单击 = 释放帧口径)在
	//    重叠区可能各派发一次(实测: 重叠的 TEXT 与 RECT 同帧各来一条) ⇒ 此处按索引比较取最大者,
	//    保证"被覆盖者不登记"; 同帧同一控件重复派发同样被此规则吸收。
	//    日志不在本函数打印(此时不知后面是否还有更上层者), 只置打印标志 ⇒ 由 Update 在帧末打印一条。
	{
		CONST DWORD dwFrame = (DWORD)ImGui::GetFrameCount();
		if (dwFrame == dwPreviewClickLastFrame && nIndex <= nPreviewClickIndex)
		{
			return;						// 本帧已有更上层(或同一枚)者 ⇒ 忽略
		}
		pPreviewClickLastCtrl = c;
		dwPreviewClickLastFrame = dwFrame;
	}
	// 7. 记录(覆盖式; 字段 = 文档索引 + 像素位置 + 像素大小)
	bPreviewClickValid = TRUE;
	bPreviewClickPrint = TRUE;
	nPreviewClickIndex = nIndex;
	fPreviewClickX = c->position.x;
	fPreviewClickY = c->position.y;
	fPreviewClickW = c->size.x;
	fPreviewClickH = c->size.y;
	// 8. 预览点击 = 选择该控件: 同步编辑器选中(编辑布局的编辑区随之切换到该控件)
	//    注: 本函数在预览渲染帧内被调用 ⇒ 此处不得改控件链(遍历中增删会打乱本帧的绘制与命中);
	//    选中态表达件(链外视图叠加件)由下一帧的 GenUIEditor::Update(第 5 步)交给 SelectionOverlay
	nSelected = nIndex;
	nSynced = -2;						// 选择变化 ⇒ 触发属性区重新同步
	SyncEditsToSel();
}
// 函数: GetPreviewClickRecord
// 描述: 取最近一次"预览布局内容控件点击"的记录(索引 / 像素位置 / 像素大小)
// 参数:
//		OUT	pnIndex:	控件在布局对象中的索引(可空)
//		OUT	pfX/pfY:	像素位置(可空)
//		OUT	pfW/pfH:	像素大小(可空)
// 返回:
//		BOOL: TRUE = 已有记录; FALSE = 尚无点击记录
BOOL GenUIEditor::GetPreviewClickRecord(INT* pnIndex, FLOAT* pfX, FLOAT* pfY, FLOAT* pfW, FLOAT* pfH)
{
	if (!bPreviewClickValid)
	{
		return FALSE;
	}
	if (pnIndex != NULL) { *pnIndex = nPreviewClickIndex; }
	if (pfX != NULL) { *pfX = fPreviewClickX; }
	if (pfY != NULL) { *pfY = fPreviewClickY; }
	if (pfW != NULL) { *pfW = fPreviewClickW; }
	if (pfH != NULL) { *pfH = fPreviewClickH; }
	return TRUE;
}
// 函数: ResetPreviewClickRecord
// 描述: 清除点击记录
VOID GenUIEditor::ResetPreviewClickRecord()
{
	bPreviewClickValid = FALSE;
	bPreviewClickPrint = FALSE;		// 待打印标志一并复位(否则下一帧会打印一条"空记录")
	nPreviewClickIndex = -1;
	fPreviewClickX = 0.0f;
	fPreviewClickY = 0.0f;
	fPreviewClickW = 0.0f;
	fPreviewClickH = 0.0f;
}

