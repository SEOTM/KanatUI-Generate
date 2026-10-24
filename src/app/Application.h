// ================================
// 文件: Application.h
// 说明: 应用程序主类头文件 (Kanat UI Generate 多布局版)
//   文档模型:
//       - 布局文件 = 1..N 个布局(每个布局携带独立 autosize 经验参数)
//       - Kanat UI Generate 自身界面布局(文件行/布局管理/经验参数/控件列表/控件编辑)由 KanatUIGenerateEditor 布局渲染
//         ("外壳"即该布局)
//       - 预览 = 单画布, 随"布局管理"选中布局(active)切换
//       - 载入 = 整个文档(全部布局 + 各自 autosize)
// ================================

#pragma once
#include <minwindef.h>
#include <stdint.h>
#include "framework/layout/Layout.h"
#include "framework/base/BasicDesign.h"
#include "framework/layout/LayoutModel.h"
#include "doc/Document.h"		// 文档承载者
#include "framework/layout/LayoutSolver.h"
#include "framework/base/FuncPtr.h"		// 回调形状区(会话回调 / 命令面回调的类型来源)

// ================================
// 常量: 外壳布局保留名
// ================================
constexpr LPCSTR KANATUI_GENERATE_SHELL_LAYOUT_NAME = "KanatUIGenerateEditor";	// 编辑窗口外壳布局在布局文档中的保留名

// 内嵌(flex)窗口成员名(KANATUI_GENERATE_HINT_LAYOUT_NAME)/ 提示正文缓冲容量(HINT_BODY_BUF)/ 条目结构

// ================================
// 类名: Application
// 说明: 应用程序主类 (单例模式, 静态成员风格)
// ================================
class Application
{
private:
	// 结构: AppData
	// 描述: 应用程序成员变量集中存储结构
	struct AppData
	{
		Document doc;				// 当前文档承载者(条目表 / 中间态 / 关联路径 / 脏标记; 内嵌 ⇒ 不新增分配)
		LONG nDocRev;				// 文档结构变更版本号(UI 缓冲同步用)	| 4 字节
	};

	// 成员变量
	static AppData* data;			// 成员变量存储

	// 私有化构造/析构
	Application() = delete;
	~Application() = delete;

	// 函数: FindEntryByLayout
	// 描述: 按布局指针反查文档条目(含外壳快照条目与内嵌(flex)成员条目; 未命中返回 NULL)
	static LayoutEntry* FindEntryByLayout(Layout* layout);	// 外层编排; 条目数组反查段归 doc\Document::FindByLayout
	// 函数: RebindDocStateNodes
	// 描述: 把文档条目的中间态布局节点按索引重新挂接到当前 docState 的布局链上
	// 说明: 中间态是"池 + 链" ⇒ 结构性变更(增删布局/元素)会新建池, 旧节点指针失效;
	//       调用后 entry.pNode 重新指向新池内的对应节点
	static VOID RebindDocStateNodes();
	// 函数: FindDocIndex
	// 描述: 按布局指针反查文档条目索引(唯一基座 ⇒ 当前 active 条目; 未命中 -1)
	static LONG FindDocIndex(Layout* layout);
	// BuildEntryRef / ApplyEntryGeometry(条目几何: autosize → 参考尺寸 → 逐控件像素几何)
	//   (app\FlexHost::BuildFlexMembersFromSolver)与本类装配族共用同一份几何求解, 故自本区移出;
	//   判据与算式一字未改(仅可见性提升)。
	// 函数: FreeEntryLayout
	// 描述: 断开会话条目对布局对象的引用(唯一基座由 app\PreviewSession::FreePreviewBase 统一释放; 数组指针由内存池持有)
	static VOID FreeEntryLayout(LayoutEntry& entry);

public:
	// 函数: BuildEntryRef
	// 描述: 依条目 autosize(cfg) 重建条目参考尺寸 ref(窗口宽高 = 实际客户区)
	// 说明: 装配族共用件(外壳快照 / 文档条目 / 内嵌(flex)成员)
	static VOID BuildEntryRef(LayoutEntry& entry);
	// 函数: ApplyEntryGeometry
	// 描述: 依条目 ref 对全部控件的几何因子(pFactors)求解像素几何
	// 说明: 同上(BuildEntryRef 的配对件; 线性只读展开态与链式条目同法)。
	static VOID ApplyEntryGeometry(LayoutEntry& entry);

	// 函数: Init
	// 描述: 初始化应用: 创建内存池, 走启动加载链(四级顺序探测)载入文档, 构建全部布局
	static BOOL Init();
	// 函数: Release
	// 描述: 释放应用单例资源(整体清零内存池)
	static VOID Release();
	// 函数: IsReady
	// 描述: 应用是否就绪(成员块 AppData 已分配; 即"分配成员存储"一步之后、Release / 失败清理之前)
	// 返回:
	//		BOOL: TRUE = 应用已就绪; FALSE = 未就绪(data == NULL)
	static BOOL IsReady();

	// -------- 会话面回调(供宿主注入命令层 editor\EditorCommands) --------





	// -------- 文档模型访问 --------
	// 函数: GetDocument
	// 描述: 取文档承载者地址(doc\Document; 供宿主在启动期经 KanatUIGenerateEditor::SetDocument 注入编辑器)
	// 说明: 承载者按值内嵌在成员块 AppData 内 ⇒ 应用就绪后地址终生稳定(Release 只置 data = NULL,
	//   不搬迁地址) ⇒ 可交出裸指针; NewDocument / LoadDocument 只重置其字段(条目数组 / 中间态),
	//   不改变地址, 但跨结构变更持有的条目/元素指针必然失效(调用方不得缓存)。
	//   取值时机硬约束: AppData 到 Init 内"分配成员存储"一步才分配 ⇒ Init 成功之前 data == NULL,
	//   本函数返回 NULL; 调用方(宿主)必须晚于 Application::Init() 取用, 并在 Application::Release()
	//   之前放弃持有(否则悬垂) —— 与 HostContext 的窗口句柄"随 Init 复位"口径一致。
	// 返回:
	//		Document*: 承载者地址; 应用未就绪(data == NULL)时返回 NULL(不回落空态替身)
	static Document* GetDocument();

	// 函数: GetDocCount / GetDocLayout
	// 描述: 文档布局条目访问
	static DWORD GetDocCount();
	// 函数: GetDocState
	// 描述: 取当前文档中间态(只读; 供序列化写出等只读消费)
	// 返回:
	//		CONST FileMidState&: 文档中间态; 应用未就绪时返回空态
	static CONST FileMidState& GetDocState();
	static Layout& GetDocLayout(DWORD index);

	// 函数: GetActiveIndex / SetActiveIndex
	// 描述: 当前激活布局索引读写(切换即 bump 文档版本号)
	static DWORD GetActiveIndex();
	static BOOL SetActiveIndex(DWORD index);

	// 函数: GetDocRef
	// 描述: 获取指定布局的参考尺寸(该布局几何求解/渲染基准)
	static CONST AutoSize& GetDocRef(DWORD index);
	// 函数: GetDocRefSize
	// 描述: 获取指定布局的参考尺寸/经验参数(中间态字段)
	static CONST RefSize& GetDocRefSize(DWORD index);
	// -------- 激活布局便捷访问(编辑器/渲染使用) --------
	// 函数: GetActiveLayout / GetActiveRef / GetActiveRefSize
	static Layout& GetActiveLayout();
	static CONST AutoSize& GetActiveRef();
	static CONST RefSize& GetActiveRefSize();

	// RecalcEntryGeometry(依条目 autosize 重建窗口几何与 ref 并重算该布局几何)

	// -------- 布局窗口(预览真实窗口)几何 --------
	// 函数: GetDocWndRect
	// 描述: 取指定布局的预览窗口矩形(桌面位置 wndX/wndY + 客户区 wndW/wndH)
	static BOOL GetDocWndRect(DWORD index, LONG* x, LONG* y, DWORD* w, DWORD* h);
	// 函数: SetDocWndPos
	// 描述: 更新指定布局窗口的桌面位置(预览窗口标题拖动写回; 条目字段与中间态布局信息同源)
	// 说明: 宿主 main 每帧按布局 wndX/wndY 摆放预览窗口; 仅内存生效, 不落盘
	static BOOL SetDocWndPos(DWORD index, LONG x, LONG y);

	// -------- 激活布局经验参数(RefSize)编辑 --------
	// 说明: 经验参数(各布局独立设置)编辑的是"激活布局"的 refSize, 即时影响其几何
	// 函数: ActiveAutoSetRatio
	static BOOL ActiveAutoSetRatio(INT index, DWORD ratio);	// 写单项千分比(0..10000; 六项均即时重算激活布局几何)
	// 函数: SetDocAutoRatio
	// 描述: 写指定布局条目的 autosize 单项参考量并重算该条目几何(非 active 亦可)
	// 说明: ActiveAutoSetRatio 即本函数在 data->doc.dwActive 上的薄封装
	static BOOL SetDocAutoRatio(DWORD docIndex, INT index, DWORD ratio);
	// 函数: ApplyDocAutoRatio
	// 描述: 写指定布局条目的 autosize 单项参考量(0..10000)并按新参考尺寸就地更新预览基座控件几何
	// 说明: 与 SetDocAutoRatio 的差别 = 不做整体重装 —— 只重建该条目参考尺寸 ref, 再按中间态元素系数
	//       逐枚改写布局对象控件的像素几何(控件数量/类型缓冲/事件绑定均不动);
	//       非 active 条目只重建自身 ref(其几何在切为 active 时装配)。
	// 参数:
	//		IN	docIndex:	布局条目索引
	//		IN	index:		参考量序号(0..5 = 宽/高/控件高/字号/间隙/内距)
	//		IN	ratio:		参考量输入值(千分比或像素, 由该条目 unit 决定; 像素按换算基准反算为千分比,
	//						中间态恒存千分比; 结果 > 10000 按 10000 处理)
	// 返回:
	//		BOOL: TRUE = 已写入并就地更新; FALSE = 文档未就绪 / 索引越界
	static BOOL ApplyDocAutoRatio(DWORD docIndex, INT index, DWORD ratio);
	// 函数: ApplyDocAutoMode
	// 描述: 写指定布局条目的 autosize 模式串, 并按新换算基准就地更新预览基座控件几何
	// 说明: 与 ApplyDocAutoRatio 同法 —— 只重建该条目参考尺寸 ref, 再按中间态元素系数
	//       逐枚改写布局对象控件像素几何(控件数量/类型缓冲/事件绑定均不动)。
	// 参数:
	//		IN	docIndex:	布局条目索引
	//		IN	mode:		自动尺寸模式串(未知串按屏高模式处理, 同 AutoModeIndex)
	// 返回:
	//		BOOL: TRUE = 已写入并就地更新; FALSE = 文档未就绪 / 索引越界 / mode 为空
	static BOOL ApplyDocAutoMode(DWORD docIndex, LPCSTR mode);
	// 函数: ApplyDocAutoUnit
	// 描述: 写指定布局条目的 autosize 参考量单位(0 = 千分比 / 1 = 像素)
	// 说明: 单位只决定"参考量输入值如何解释"(见 ApplyDocAutoRatio), 中间态存储恒为千分比
	//       ⇒ 换单位不改参考量的像素几何(控件像素几何与预览窗口尺寸均不变)。
	// 参数:
	//		IN	docIndex:	布局条目索引
	//		IN	unit:		参考量单位(0 = 千分比; 1 = 像素)
	// 返回:
	//		BOOL: TRUE = 已写入; FALSE = 文档未就绪 / 索引越界 / 单位越界
	static BOOL ApplyDocAutoUnit(DWORD docIndex, INT unit);

	// -------- 编辑器几何提交的即时生效入口 --------
	// 函数: ApplyDocCtrlGeometry
	// 描述: 按中间态元素规格就地重解指定布局内一枚控件的像素几何(编辑器系数/偏移提交后调用)
	// 说明: 像素几何 = 中间态元素规格(系数/偏移)的解析产物;
	//       与 ApplyDocAutoRatio 同法(不整体重装: 控件数量/类型缓冲/事件绑定均不动)。
	// 参数:
	//		IN	layout:	目标布局对象(编辑器 = 当前 active 条目承载的预览基座)
	//		IN	index:	文档索引(与中间态元素索引一一对应)
	// 返回:
	//		BOOL: TRUE = 已就地重解; FALSE = 入参无效 / 条目或元素规格缺失
	static BOOL ApplyDocCtrlGeometry(Layout* layout, size_t index);



	// -------- 基础工具 --------

	// -------- 中间态元素规格(几何系数存放处; Layout/CtrlInfo 不持系数) --------
	// 说明: 四轴系数(系数 + 像素偏移) = 中间态元素规格(ElementSpec 的 x/y/width/height),
	//       供 ①几何解算/重解(ApplyEntryGeometry) ②编辑器镜像格的显示来源;
	//       控件对象只保留像素几何(position/size); 增删控件时元素规格随索引同步(中间态池重建)。
	// 本区全部接口按文档索引解释(与中间态元素链同口径)。
	//   无需换算; 编辑器的选中态表达件 = 链外的视图叠加件(editor\SelectionOverlay)。
	// 函数: GetCtrlSpec
	// 描述: 取某布局内指定文档索引的中间态元素规格(布局无效/索引越界 ⇒ NULL)
	static ElementSpec* GetCtrlSpec(Layout* layout, size_t index);
	// 函数: SetCtrlType
	// 描述: 把指定布局内某控件改为指定类型(中间态元素类型串 + 布局对象控件类型 + 该控件缓冲按新类型重建)
	// 说明: 控件像素几何由中间态元素规格决定 ⇒ 不改几何; 旧缓冲在重建之前回池。
	// 参数:
	//		IN	layout:	目标布局对象
	//		IN	index:	文档索引(与中间态元素索引一一对应)
	//		IN	type:	新类型(不可构建类型 INVALID/UNKNOWN 拒绝)
	// 返回:
	//		BOOL: TRUE = 已改并重建缓冲; FALSE = 入参无效 / 元素规格或控件缺失 / 缓冲重建失败
	static BOOL SetCtrlType(Layout* layout, size_t index, CtrlType type);
	// 函数: MoveCtrl
	// 描述: 拖拽重排: 把指定控件移到同一布局内的新相对位置(控件列表控件 LIST_COMBO 的拖拽意图消费者)
	// 说明: 控件顺序真源 = 中间态元素链 ⇒ 本函数按"取出源的三处副本(控件值 / 中间态元素规格 /
	//       原类型串与声明文本旁挂表) → 三处同步删除 → 三处同步插入"实施(与 DeleteSelectedCore /
	//       AddControlCore 同法); 控件缓冲与 label 指针随控件值整体搬移(池内自持, 不逐块释放);
	//       过程中先摘除预览选中标记、出口按应用侧记录重建 ⇒ 调用方无需自行维护标记。
	// 参数:
	//		IN	layout:	目标布局对象(链式)
	//		IN	from:	源文档索引
	//		IN	dest:	目标插入文档索引(删除源之后的口径; 0..文档控件数-1)
	// 返回:
	//		BOOL: TRUE = 已重排; FALSE = 入参无效 / 索引越界 / 源目标同处 / 中间态或控件链操作失败
	static BOOL MoveCtrl(Layout* layout, size_t from, size_t dest);


	// -------- 布局模型(中间态)接口 --------
	// 函数: BuildLayoutFromState
	// 描述: 把布局模型中指定索引的布局展开为布局对象(自由函数 FormatToUI 的实现落点)
	// 参数:
	//		IN	state:		布局模型中间态
	//		IN	index:		布局索引
	//		IN	IsLinkmode:	展开态的储存模式
	// 返回:
	//		Layout*: 展开出的布局对象; 失败返回 NULL
	static Layout* BuildLayoutFromState(CONST FileMidState& state, int index = 0, BOOL IsLinkmode = FALSE);


	// SyncPreviewLayout(中间态当前内容 → 预览唯一基座的按编辑情况更新)与 RequestPreviewSync
	//   (编辑动作改完中间态后的统一同步入口: 渲染帧内挂帧间队列 / 帧末排空执行中当场同步)

};
