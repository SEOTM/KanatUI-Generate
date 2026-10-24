// ================================
// 文件: PreviewSession.h
// 说明: 会话面只读 / 装配件模块 —— 预览基座与帧间同步 · 外壳快照(全静态类; 构造/析构私有化
//   ⇒ 进程级单例语义)
//   边界:
//   - 第一面「预览基座与帧间同步」: 基座对象(链式布局; 全部文档条目共享同一对象, 内容随 active
//     条目变换) · 中间态当前条目 → 基座的按编辑情况更新 · 帧间事件队列上的同步请求 ·
//     条目 autosize 变更后的窗口几何与参考尺寸重算。
//   - 第二面「外壳快照」(pShell / bShellValid / LoadShellSnapshot 一族): 编辑窗口 KanatUIGenerateEditor
//     独立只读快照条目的解析 / 装载 / 失效复位 · 快照查询(布局 / 参考尺寸 / 经验参数 / 窗口矩形)·
//     建窗后的客户区几何校正。两面同属"会话面只读 / 装配件": 生命周期都随应用会话, 都与被编辑文档的
//     切换 / 新建无关; ⇒ 同落点。
//   - 文档面数据(条目数组 / 中间态 / 激活索引)经 SetSession 注入的承载者句柄读取; 文档结构
//     变更版本号经同点注入的地址句柄递增; 会话分配器的就绪判据经同点注入的分配器句柄
//     (三者都在应用侧"分配成员存储"之后注入)。
//   - 条目几何族(BuildEntryRef / ApplyEntryGeometry)仍属应用主类公开接口 ⇒ 本模块按其现有签名调用;
//     装配沿用的旁挂表接口(SetRawType / SetRawText / RemoveRawTypeAt / RemoveRawTextAt)
//   - 外壳快照装配的解析器工厂(DocumentIO::NewSolver)与"中间态 → 展开态"构建
//     (LayoutBuilder::BuildLayoutFromNode)与内嵌成员装配(FlexHost::SetShellBase /
//     FlexHost::BuildFlexMembersFromSolver / FlexHost::FreeFlexLayouts)一律按其现有公开接口调用。
//   依赖方向: app → framework(Layout / LayoutModel / Controls / GeometrySolver / FrameEvent / LayoutBuilder /
//     base\mem) / doc; 不依赖 editor ⇒ 不新增任何跨层反依赖。
// ================================

#pragma once
#include <minwindef.h>
#include <stdint.h>		// uint64_t(帧间同步请求处理函数的四元组参数)

class Layout;
struct Document;
struct LayoutEntry;
class LayoutSolver;
class Arena;

// ================================
// 类名: PreviewSession
// 说明: 预览基座(建立 / 释放 / 内容装配)与帧间同步请求(挂载 / 排空) + 外壳快照(装载 / 查询 / 几何校正 / 失效复位)
// ================================
class PreviewSession
{
private:
	// 会话面状态(基座自 Application::AppData 迁出; 三个句柄由应用侧注入)
	static Layout* pPreviewBase;		// 预览布局唯一基座(链式; 全部条目共享, 内容随 active 变换) | 8 字节
	static Document* pDoc;				// 文档承载者句柄(条目数组 / 中间态 / 激活索引; NULL = 未注入) | 8 字节
	static LONG* pnDocRev;				// 文档结构变更版本号地址句柄(UI 缓冲同步用; NULL = 未注入) | 8 字节
	static Arena* pArena;				// 会话分配器句柄(仅"是否就绪"判据; NULL = 未注入) | 8 字节
	// 外壳快照状态(自 Application::AppData 迁出; 由应用侧注入的分配器句柄决定其就绪面)
	static LayoutEntry* pShell;			// 外壳布局快照条目(编辑窗口驱动, 见本文件"外壳快照访问"区) | 8 字节
	static BOOL bShellValid;			// 外壳快照是否有效(载入含 KanatUIGenerateEditor)	| 4 字节

	// 私有化构造/析构
	PreviewSession() = delete;
	~PreviewSession() = delete;

	// 函数: IsReady
	// 描述: 会话面是否就绪(私有判据件) —— 外壳快照区的"应用未就绪"判据
	static BOOL IsReady();
	// 函数: FreeShellSnapshot
	// 描述: 释放外壳快照条目(布局对象销毁; 条目内存由内存池持有)
	static VOID FreeShellSnapshot();
	// 函数: BuildShellSnapshotFromSolver
	// 描述: 由已解析的布局规格构建外壳快照(取 name==KanatUIGenerateEditor 的布局)
	static BOOL BuildShellSnapshotFromSolver(LayoutSolver* solver);

public:
	// 函数: SetSession
	// 描述: 注入文档承载者 / 文档版本号地址 / 会话分配器句柄(三者可分别传 NULL ⇒ 复位)
	// 说明: 由应用侧在"分配成员存储"之后注入(承载者按值内嵌在成员块内 ⇒ 早于该步取不到地址)
	// 参数:
	//		IN	pDocument:	承载者地址(应用未就绪 / 已释放 ⇒ NULL)
	//		IN	pnRev:		文档版本号地址(同上)
	//		IN	pArenaHub:	会话分配器地址(仅作"本模块是否已就绪"的判据; 同上)
	static VOID SetSession(Document* pDocument, LONG* pnRev, Arena* pArenaHub);
	// 函数: GetPreviewBase
	// 描述: 取预览唯一基座(未建立 ⇒ NULL; 供条目族反查编排判定"该布局是否为预览基座")
	static Layout* GetPreviewBase();
	// 函数: ResetPreviewBase
	// 描述: 复位基座指针(只丢弃指针, 不释放对象); 由应用侧"分配成员存储"一步原位复位
	static VOID ResetPreviewBase();
	// 函数: EnsurePreviewBase
	// 描述: 取预览布局唯一基座(不存在则建立空链式布局), 并把全部文档条目的 layout 指向它
	// 说明: 基座对象身份随应用生命周期; 其控件内容由 SyncPreviewLayout 按中间态装配
	// 返回:
	//		Layout*: 基座地址; NULL = 未注入承载者或建立失败
	static Layout* EnsurePreviewBase();
	// 函数: FreePreviewBase
	// 描述: 释放预览布局唯一基座(仅 Release 时调用; 条目数组由内存池持有, 只断引用)
	static VOID FreePreviewBase();
	// 函数: EnsurePreviewBaseHook
	// 描述: 取预览唯一基座(不存在则建立), 并把全部文档条目的 layout 指向它 —— 命令层会话回调的实现
	// 返回:
	//		Layout*: 基座地址; NULL = 应用未就绪或建立失败
	static Layout* EnsurePreviewBaseHook();
	// 函数: BumpDocRevHook
	// 描述: 递增文档结构变更版本号(UI 缓冲同步信号) —— 命令层会话回调的实现
	static VOID BumpDocRevHook();
	// 函数: SyncPreviewLayout
	// 描述: 把中间态当前内容部分更新到预览布局唯一基座(布局对象身份不变, 不重建对象)
	// 说明: ① 结构件区(基座前 GetShellNum() 槽)就地刷新几何/标签/文本, 事件绑定原样保留;
	//       ② 内容控件整体卸载后, 按中间态元素链重新全部追加(含缓冲分配与事件绑定);
	//       ③ 部分失败逐条诊断并继续 —— 已装配部分保留(尽力而为), 返回 FALSE 表示未完整完成。
	// 返回:
	//		BOOL: TRUE = 完整同步; FALSE = 无文档/无中间态节点/存在失败项
	static BOOL SyncPreviewLayout();
	// 函数: RequestPreviewSync
	// 描述: 请求把中间态同步到预览布局唯一基座 —— 编辑动作改完中间态后的统一入口
	// 说明: ① 渲染帧内调用 ⇒ 经帧间事件队列挂载, 帧末排空时执行(同一帧内多次请求由队列合并为一枚);
	//       ② 帧末排空执行中调用 ⇒ 当场同步(此时两窗口均不在渲染帧内);
	//       ③ 挂载失败(队列满)时同步请求丢失, 返回 FALSE(诊断已由队列打印)。
	// 返回:
	//		BOOL: TRUE = 已挂载或已当场同步(含同步本身未完整完成的尽力而为结果); FALSE = 挂载失败
	static BOOL RequestPreviewSync();
	// 函数: OnPreviewSyncRequest
	// 描述: 帧间同步请求的处理函数(帧末排空时执行) —— 由 RequestPreviewSync 挂载到帧间事件队列
	// 参数:
	//		IN	obj/oper/param/data: 帧间事件四元组(本请求不使用: 同步作用于全局唯一基座)
	static VOID OnPreviewSyncRequest(VOID* obj, uint64_t oper, uint64_t param, void* data);
	// 函数: RecalcEntryGeometry
	// 描述: 依条目 autosize 重建窗口几何与 ref 并重算该布局几何(autosize 变更后调用)
	// 参数:
	//		IN	index:	文档条目索引
	static VOID RecalcEntryGeometry(DWORD index);

	// -------- 外壳快照访问(编辑窗口 = KanatUIGenerateEditor 布局渲染源) --------
	// 说明: 外壳快照 = 启动载入 KanatUIGenerate.json 中 name==KanatUIGenerateEditor 布局的独立副本(会话内固定,
	//       不随编辑文档切换/打开变化); 渲染 hWndEdit 内容用。文件无该布局时快照无效 ⇒
	//       编辑窗口无可渲染内容(调用方诊断后中止启动)。
	// 函数: LoadShellSnapshot
	// 描述: 从布局文件解析 name==KanatUIGenerateEditor 布局为外壳快照(替换既有快照; 非致命)
	// 说明: 快照窗口像素(hWndEdit 的建窗尺寸来源)由该布局 autosize 推导 —— 与文档条目/预览窗口
	//       同法(布局 = 一个窗口), 载入即 BuildEntryRef + ApplyEntryGeometry 求解完毕
	static BOOL LoadShellSnapshot(LPCSTR path);
	// 函数: LoadShellSnapshotFromMemory
	// 描述: 与 LoadShellSnapshot 同语义, 但数据来自内存缓冲(启动加载链命中内置资源时走此入口)
	static BOOL LoadShellSnapshotFromMemory(LPVOID pData, DWORD cbData);
	// 函数: GetShellValid
	// 描述: 外壳快照是否有效(KanatUIGenerate.json 载入且含 KanatUIGenerateEditor 布局)
	static BOOL GetShellValid();
	// 函数: GetShellLayout / GetShellRef / GetShellRefSize
	// 描述: 外壳布局/参考尺寸/经验参数访问(无效时返回空对象/默认值)
	// 说明: GetShellRef = 解算后的参考像素(AutoSize); GetShellRefSize = 中间态的参考尺寸/经验参数
	static Layout& GetShellLayout();
	static CONST AutoSize& GetShellRef();
	static CONST RefSize& GetShellRefSize();
	// 函数: GetShellEntry
	// 描述: 取外壳快照条目指针(未载入 ⇒ NULL; 供应用侧条目族反查编排判定"该布局是否为外壳快照")
	static LayoutEntry* GetShellEntry();
	// 函数: GetShellWndRect
	// 描述: 取外壳快照布局的窗口矩形(尺寸 = 该布局 autosize 推导; 快照无效返回 FALSE)
	// 说明: 宿主 main 以 w/h 创建编辑窗口(与预览窗口取 GetDocWndRect 同法)
	static BOOL GetShellWndRect(LONG* x, LONG* y, DWORD* w, DWORD* h);
	// 函数: SetShellClientSize
	// 描述: 编辑窗口建窗后调用: 以实际客户区像素校正外壳 ref 并重算外壳布局几何
	// 说明: WS_POPUP 无边框 ⇒ 实际客户区 == 建窗尺寸(== autosize 推导值), 此步为幂等校正;
	//       系统夹取/DPI 等使二者不等时以实际客户区为准
	static VOID SetShellClientSize(LONG width, LONG height);
	// 函数: ResetShellSnapshot
	// 描述: 复位外壳快照状态(释放快照条目 + 复位内嵌成员 + 清有效位) —— 由应用侧 Release 原位调用
	static VOID ResetShellSnapshot();
};
