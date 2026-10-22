// ================================
// 文件: EditorCommands.h
// 说明: 编辑器命令层 —— 文档编辑命令(增 / 删 / 移)的落点
//   边界:
//   - 只承载"编辑命令": 改中间态元素链 / 条目数据; 不含渲染 / 窗口 / 宿主能力 / 会话面状态。
//   - 文档承载者(doc\Document)经 SetDocument 由宿主注入(时机与铁律见 GenUIEditor::SetDocument 的注释)。
//   - 未注入(pDoc == NULL)⇒ 显式降级: 一律返回 FALSE / NULL, 不制造空态替身
//     (与 doc\Document 的"纯数据、无策略"定位一致)。
//   依赖方向: editor → doc / framework / app(仅应用层契约头 SessionContracts.h); 命令层不持会话状态。
// ================================

#pragma once
#include <minwindef.h>
#include "framework/layout/Layout.h"
#include "framework/layout/LayoutModel.h"
#include "app/SessionContracts.h"

struct Document;

// ================================
// 类名: EditorCommands
// 说明: 编辑器命令(全静态类; 构造/析构私有化 ⇒ 进程级单例语义)
// ================================
class EditorCommands
{
private:
	static Document* pDoc;					// 文档承载者(doc\Document; 宿主注入) —— NULL = 未注入 / 已复位
	// 会话面回调(宿主 / 应用侧注入)
	static pfnEnsurePreviewBase pfnEnsureBase;	// 取 / 建预览唯一基座(并让全部条目的 layout 指向它)
	static pfnSyncPreviewLayout pfnSyncPreview;	// 中间态 → 预览基座同步
	static pfnBumpDocRev pfnBumpRev;			// 文档结构版本号递增(UI 缓冲同步信号)
	// 函数: DocIndexOfLayout
	// 描述: 按布局指针反查文档索引(未命中 ⇒ -1)
	// 说明: 预览唯一基座被全部条目共享 ⇒ 归属当前 active 条目; 其余布局按条目数组线性反查。
	//       与承载者的 FindByLayout 刻意不同 —— 后者仅"条目数组线性反查", 对基座会命中 0 号条目,
	//       而命令的中间态元素链必须落在 active 条目上。
	static LONG DocIndexOfLayout(Layout* layout);

private:
	EditorCommands() = delete;
	~EditorCommands() = delete;

public:
	// 函数: SetDocument
	// 描述: 注入文档承载者(宿主启动期经 GenUIEditor::SetDocument 单点转发; NULL = 复位)
	static VOID SetDocument(Document* pDocument);
	// 函数: SetSessionHooks
	// 描述: 注入会话面回调三件套(与承载者注入同点调用; 三者可分别传 NULL ⇒ 该项不驱动)
	// 说明: 布局族命令(增 / 删 / 重排)改完中间态后须驱动会话面(基座建立 / 预览同步 / 版本号递增) ——
	//       命令层不持会话状态, 只经本组回调"请求"。
	static VOID SetSessionHooks(pfnEnsurePreviewBase pfnEnsure, pfnSyncPreviewLayout pfnSync, pfnBumpDocRev pfnBumpRev);
	// 函数: SetDocLayoutName
	// 描述: 写指定布局条目的显示名(label 与 name 同写; 空串/空指针拒绝)
	// 说明: 中间态(pNode->info)与条目副本两处同写(中间态才是保存写出物); 末尾经 pfnBumpDocRev
	// 返回: BOOL TRUE = 已写入; FALSE = 未注入 / 索引越界 / 名字为空 / 条目节点缺失
	static BOOL SetDocLayoutName(DWORD docIndex, LPCSTR name);
	// 函数: AddDocLayout
	// 描述: 追加一个布局(默认 窗口/标题栏/容器 三结构件 + 布局默认 autosize); 新布局随即成为 active
	// 参数: name=布局名(label 与 name 同写); NULL / 空串 ⇒ 取默认名 新布局
	// 返回: BOOL TRUE = 已追加; FALSE = 未注入 / 未就绪(分配器不可用) / 池分配或元素链操作失败
	static BOOL AddDocLayout(LPCSTR name);
	// 函数: RemoveDocLayout
	// 描述: 删除指定布局(文档保底 1 个; 删除激活项时激活切回 0)
	// 返回: BOOL TRUE = 已删除; FALSE = 未注入 / 条目数组空 / 保底 1 个 / 索引越界 / 中间态移除失败 / 池分配失败
	static BOOL RemoveDocLayout(DWORD index);
	// 函数: MoveDocLayout
	// 描述: 拖拽重排: 把指定布局移到新的相对位置(布局列表控件 LIST_INPUT 的拖拽意图消费者)
	// 参数:
	//		IN	dwFrom:	源布局索引
	//		IN	dwDest:	目标插入索引(删除源之后的口径; 0..dwEntries-1)
	// 返回:
	//		BOOL: TRUE = 已重排并重新装配预览基座; FALSE = 未注入 / 入参无效 / 索引越界 / 源目标同处 / 池分配失败
	static BOOL MoveDocLayout(DWORD dwFrom, DWORD dwDest);
	// 函数: SpecAt
	// 描述: 取指定布局内某文档索引的条目中间态元素规格(非条目 / 条目未挂接 / 索引越界 ⇒ NULL)
	static ElementSpec* SpecAt(Layout* layout, size_t index);
	// 函数: InsertSpecAt
	// 描述: 在指定布局内插入一枚中间态元素规格(与控件插入同步; 新元素以当前像素表达几何)
	// 返回: BOOL TRUE = 已插入; FALSE = 未注入 / 布局非条目 / 元素链操作失败
	static BOOL InsertSpecAt(Layout* layout, size_t index);
	// 函数: RemoveSpecAt
	// 描述: 移除指定布局内一枚中间态元素规格(与控件删除同步)
	// 返回: BOOL TRUE = 已移除; FALSE = 未注入 / 布局非条目 / 元素链操作失败
	static BOOL RemoveSpecAt(Layout* layout, size_t index);
};
