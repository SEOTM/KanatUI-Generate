// ================================
// 文件: DocumentIO.h
// 说明: 文档 I/O 模块 —— 载入(外部文件 / 内存缓冲两入口) / 新建 / 保存 与写盘格式分派
//   边界:
//   - 只承载"文件 ↔ 文档"的进出与写盘策略: 载入族(解析 + 整份文档替换) · 新建空白文档 ·
//     保存族(按目标扩展名分派 bin / JSON) · 工程源保护 · 关联磁盘路径 · 未保存改动标记。
//   - 文档承载者(doc\Document)经 SetDocument 注入(NULL = 未注入 / 已复位; 未注入 ⇒ 显式降级)。
//   - 会话面状态(预览唯一基座 / 文档版本号 / 条目几何重算)不属本模块 ⇒ 经 SetSessionHooks
//     注入的回调原位请求; 未注入 ⇒ 该项不驱动(显式降级)。
//   - "追加布局"编辑命令的本体归 editor\EditorCommands ⇒ 本模块不引编辑器命令头, 只持函数指针
//     (宿主在命令面接线点注入; 未注入 ⇒ 新建的该步显式降级)。
//   - 读写格式判据经 framework\LayoutSolver(读方向, 内容嗅探)与 editor\LayoutWriter(写方向),
//     本模块只做"入口选择"与"扩展名分派"。
//   依赖方向: app → doc / framework / editor(写方向 LayoutWriter); 不新增跨层反依赖。
// ================================

#pragma once
#include <minwindef.h>
#include "app/SessionContracts.h"

struct Document;
class LayoutSolver;

// ================================
// 类名: DocumentIO
// 说明: 文档文件 I/O 与写盘格式分派(全静态类; 构造/析构私有化 ⇒ 进程级单例语义)
// ================================
class DocumentIO
{
private:
	static Document* pDoc;							// 文档承载者(doc\Document; 应用侧注入) —— NULL = 未注入 / 已复位
	// 会话面回调(应用侧注入; 本模块不持会话状态)
	static pfnEnsurePreviewBase pfnEnsureBase;		// 取 / 建预览唯一基座(并让全部条目的 layout 指向它)
	static pfnBumpDocRev pfnBumpRev;				// 文档结构版本号递增(UI 缓冲同步信号)
	static pfnRecalcEntryGeometry pfnRecalcEntry;	// 条目几何重算(依条目 autosize 重建窗口几何与 ref)
	// 命令面回调("追加布局"编辑命令; 宿主注入)
	static pfnAddDocLayout pfnAddLayout;

	// 私有化构造/析构
	DocumentIO() = delete;
	~DocumentIO() = delete;

public:
	// 函数: SetDocument
	// 描述: 注入文档承载者(应用侧注入; NULL = 复位)
	static VOID SetDocument(Document* pDocument);
	// 函数: SetSessionHooks
	// 描述: 注入会话面回调三件套(可分别传 NULL ⇒ 该项不驱动)
	static VOID SetSessionHooks(pfnEnsurePreviewBase pfnEnsure, pfnBumpDocRev pfnBumpRev, pfnRecalcEntryGeometry pfnRecalcEntry);
	// 函数: SetDocLayoutAdder
	// 描述: 注入"追加布局"编辑命令(宿主单点接线; NULL = 复位)
	static VOID SetDocLayoutAdder(pfnAddDocLayout pfnAdd);

	// -------- 载入族(文件 → 文档) --------
	// 函数: LoadDocument
	// 描述: 打开布局文件并整体替换当前文档(active = 0)
	static BOOL LoadDocument(LPCSTR path);
	// 函数: LoadDocumentFromMemory
	// 描述: 同 LoadDocument, 但数据来自内存缓冲(末尾须补 '\0')
	static BOOL LoadDocumentFromMemory(LPVOID pData, DWORD cbData);
	// 函数: BuildDocumentFromSolver
	// 描述: 由已解析的中间态(布局模型)构建整份文档(整体替换当前文档, active = 0)
	static BOOL BuildDocumentFromSolver(LayoutSolver* solver);
	// 函数: NewSolver
	// 描述: 从内存池分配并构造一枚布局解析器(解析产物长期持有)
	// 返回: 解析器指针; NULL = 池分配失败或 MemArena 未就绪
	static LayoutSolver* NewSolver();

	// -------- 新建 --------
	// 函数: NewDocument
	// 描述: 新建空白文档(1 个含 WND/TITLE/CNTR 的布局 NewLayout), 替换当前文档
	static BOOL NewDocument();

	// -------- 保存族(文档 → 文件) --------
	// 函数: SaveDocument
	// 描述: 把当前文档中间态写盘(整份文档: 全部布局 + 各自 autosize + 控件声明)
	// 说明: 出口格式按目标扩展名分派 —— .bin(大小写不敏感)⇒ bin 字节流, 其余 ⇒ 规范 JSON 文本
	static BOOL SaveDocument(LPCSTR path);
	// 函数: IsEngineSourcePath
	// 描述: 判断给定路径是否指向"工程源文档"(exe 目录 GenUI.json / GenUI.bin; 相对路径/大小写不敏感)
	static BOOL IsEngineSourcePath(LPCSTR path);
	// 函数: IsProtectedSavePath
	// 描述: 保存目标是否受保护 —— 目标是工程源文档, 且当前文档关联文件也是它
	static BOOL IsProtectedSavePath(LPCSTR path);

	// -------- 关联路径与未保存改动标记 --------
	// 函数: GetDocPath / SetDocPath
	// 描述: 当前文档关联的磁盘文件路径读写(载入成功置来源路径; 保存成功置保存路径; 新建/内存载入置空)
	static LPCSTR GetDocPath();
	static VOID SetDocPath(LPCSTR path);
	// 函数: MarkDocDirty / ClearDocDirty
	// 描述: 置 / 清"未保存改动"标记
	static VOID MarkDocDirty();
	static VOID ClearDocDirty();
};
