// ================================
// 文件: DocumentIO.cpp
// 说明: 文档 I/O 模块的实现 —— 载入族 / 新建 / 保存族 / 工程源保护 / 关联路径 / 未保存改动标记
// ================================

#include <windows.h>
#include <new>
#include <stdio.h>
#include <string.h>
#include "app/DocumentIO.h"
#include "doc/DocConsistency.h"
#include "doc/Document.h"
#include "framework/base/mem/AllocatorHub.h"
#include "framework/dispatch/ResourceLoader.h"
#include "framework/layout/LayoutSolver.h"
#include "editor/LayoutWriter.h"

// ================================
// 区域: 文件内静态工具区
// ================================

// 函数: FreeEntryLayout
// 描述: 断开会话条目对布局对象的引用(条目数组本身由内存池持有, 不需释放)
// 说明: 文档条目的布局对象 = 预览唯一基座(全部条目共享);
//       基座由 app\PreviewSession::FreePreviewBase 释放, 外壳快照条目由该模块的 FreeShellSnapshot 释放。
static VOID FreeEntryLayout(LayoutEntry& entry)
{
	entry.layout = NULL;
}

// 函数: IsBinSavePath
// 描述: 保存目标是否按 bin 形态写盘(判据 = 目标扩展名, 大小写不敏感; 其余一律按规范 JSON 写盘)
// 说明: 只按文件名部分取扩展名, 避免目录名中的点干扰判定
// 参数:
//		IN	path:	保存目标路径
// 返回:
//		BOOL: TRUE = 按 bin 写盘; FALSE = 按 JSON 写盘(含路径非法)
static BOOL IsBinSavePath(LPCSTR path)
{
	BOOL bRet = FALSE;
	LPCSTR lpName = NULL;
	LPCSTR lpExt = NULL;
	if (path == NULL || path[0] == '\0')
	{
		return bRet;
	}
	lpName = strrchr(path, '\\');
	lpName = (lpName != NULL) ? (lpName + 1) : path;
	lpExt = strrchr(lpName, '.');
	if (lpExt != NULL)
	{
		bRet = (lstrcmpiA(lpExt, ".bin") == 0);
	}
	return bRet;
}

// 函数: IsSameFilePath
// 描述: 两条路径是否指向同一文件(各自 GetFullPathNameA 规范化后按大小写不敏感比较)
// 参数:
//		IN	a:	路径一
//		IN	b:	路径二
// 返回:
//		BOOL: TRUE = 指向同一文件
static BOOL IsSameFilePath(LPCSTR a, LPCSTR b)
{
	BOOL bRet = FALSE;
	CHAR szA[MAX_PATH]{};
	CHAR szB[MAX_PATH]{};
	if (a == NULL || a[0] == '\0' || b == NULL || b[0] == '\0')
	{
		return bRet;
	}
	if ((GetFullPathNameA(a, MAX_PATH, szA, NULL) > 0) &&
		(GetFullPathNameA(b, MAX_PATH, szB, NULL) > 0))
	{
		bRet = (lstrcmpiA(szA, szB) == 0);
	}
	return bRet;
}

// 成员变量
Document* DocumentIO::pDoc = NULL;					// 文档承载者(应用侧注入; NULL = 未注入 / 已复位)
// 会话面回调: 本模块不持会话状态 ⇒ 预览基座建立 / 版本号递增 / 条目几何重算一律经回调请求
pfnEnsurePreviewBase DocumentIO::pfnEnsureBase = NULL;
pfnBumpDocRev DocumentIO::pfnBumpRev = NULL;
pfnRecalcEntryGeometry DocumentIO::pfnRecalcEntry = NULL;
// 命令面回调: "追加布局"命令本体在 editor\EditorCommands ⇒ 本模块只持函数指针
pfnAddDocLayout DocumentIO::pfnAddLayout = NULL;

// 函数: SetDocument
// 描述: 注入文档承载者(NULL = 复位)
VOID DocumentIO::SetDocument(Document* pDocument)
{
	pDoc = pDocument;
}

// 函数: SetSessionHooks
// 描述: 注入会话面回调三件套(可分别传 NULL ⇒ 该项不驱动)
VOID DocumentIO::SetSessionHooks(pfnEnsurePreviewBase pfnEnsure, pfnBumpDocRev pfnBumpRev, pfnRecalcEntryGeometry pfnRecalcEntry)
{
	DocumentIO::pfnEnsureBase = pfnEnsure;
	DocumentIO::pfnBumpRev = pfnBumpRev;
	DocumentIO::pfnRecalcEntry = pfnRecalcEntry;
}

// 函数: SetDocLayoutAdder
// 描述: 注入"追加布局"编辑命令(宿主单点接线; NULL = 复位)
VOID DocumentIO::SetDocLayoutAdder(pfnAddDocLayout pfnAdd)
{
	pfnAddLayout = pfnAdd;
}

// ================================
// 区域: 载入族(文件 / 内存 → 文档)
// ================================

// 函数: LoadDocument
// 描述: 打开布局文件并整体替换当前文档(active=0)
// 说明: 文件格式 = { "layouts": [ {name,label,IsMaxiable,IsResizeable,width,height,autosize,ctrls[]}, ... ] } 或
//       bin 文档(格式 v1, 见 src\framework\LayoutModel.h 的"bin 文件格式契约"区); 形态由文件内容嗅探判定(见 LoadLayoutsAuto)
BOOL DocumentIO::LoadDocument(LPCSTR path)
{
	BOOL bRet = FALSE;
	LayoutSolver* solver = NULL;
	if (pDoc == NULL || AllocatorHub::GetArena() == NULL || path == NULL)
	{
		return bRet;
	}
	// 1. 解析文件(布局规格全部拷贝进内存池, 可长期持有; JSON / bin 由嗅探分派)
	solver = NewSolver();
	if (solver == NULL || !solver->LoadLayoutsAuto(path))
	{
		return bRet;
	}
	if (!BuildDocumentFromSolver(solver))
	{
		return bRet;
	}
	SetDocPath(path);				// 载入成功 ⇒ 当前文档关联文件 = 该路径
	ClearDocDirty();				// 载入即"无未保存改动"
	bRet = TRUE;
	return bRet;
}

// 函数: LoadDocumentFromMemory
// 描述: 同 LoadDocument, 但数据来自内存缓冲(启动加载链第 3/4 级的内置资源走此入口)
// 说明: 形态同样由内容嗅探判定; JSON 形态要求缓冲以 '\0' 结尾(LayoutSolver 的 Parse(char*) 需要 NUL), 见 DocumentSource::GetEmbeddedDoc
BOOL DocumentIO::LoadDocumentFromMemory(LPVOID pData, DWORD cbData)
{
	BOOL bRet = FALSE;
	LayoutSolver* solver = NULL;
	if (pDoc == NULL || AllocatorHub::GetArena() == NULL || pData == NULL || cbData == 0)
	{
		return bRet;
	}
	solver = NewSolver();
	if (solver == NULL || !solver->LoadLayoutsAuto((LPCVOID)pData, cbData))
	{
		return bRet;
	}
	if (!BuildDocumentFromSolver(solver))
	{
		return bRet;
	}
	SetDocPath(NULL);				// 内存载入(内置资源) ⇒ 无关联磁盘文件
	ClearDocDirty();				// 载入即"无未保存改动"
	bRet = TRUE;
	return bRet;
}

// 函数: BuildDocumentFromSolver
// 描述: 由已解析的中间态(布局模型)构建整份文档(整体替换当前文档, active=0)
BOOL DocumentIO::BuildDocumentFromSolver(LayoutSolver* solver)
{
	BOOL bRet = FALSE;
	DWORD dwNum = 0;
	LayoutEntry* pEntries = NULL;
	DWORD dwWinW = 0;
	DWORD dwWinH = 0;
	if (pDoc == NULL || AllocatorHub::GetArena() == NULL || solver == NULL)
	{
		goto END;
	}
	dwNum = solver->GetLayoutsCount();
	if (dwNum == 0)
	{
		goto END;
	}
	// 0. 结构件排布校验(载入前的唯一关卡; 口径见 doc\DocConsistency::CheckStructure):
	//    结构件缺失 / 错序 / 落在内容控件之后 ⇒ 该布局渲染为空, 或把结构件当内容控件渲染;
	//    违规整份拒载(不替换当前文档), 由调用方按既有"载入被拒"口径回落下一级来源。
	{
		CHAR szStruct[1024]{};
		if (!DocConsistency::CheckStructure(solver->GetState(), szStruct, sizeof(szStruct)))
		{
			printf("[STRUCT] 结构件校验未通过 ⇒ 整份文档载入被拒(违规布局无法正确渲染):\n%s", szStruct);
			goto END;
		}
	}
	// 1. 文档中间态 = 解析产物(块 + 链; 编辑期的增删都在这份中间态上进行);
	//    旧中间态持有的块随本次替换一并回收(块取自小对象池)
	//    注: 内嵌(flex)成员不随文档变化(其来源 = 外壳源, 节点生存期 = 应用线性分配器)
	//        ⇒ 本处不释放、不重建它们(见 BuildFlexMembersFromSolver)。
	LayoutModel::StateClear(pDoc->docState);
	pDoc->docState = solver->GetState();
	// 2. 分配新条目数组
	{
		BufferInfo arrbuf = AllocatorHub::GetArena()->Allocate(sizeof(LayoutEntry) * dwNum);
		if (arrbuf.buffer == NULL)
		{
			goto END;
		}
		pEntries = (LayoutEntry*)arrbuf.buffer;
	}
	// 3. 逐个中间态布局节点构建条目(构建失败即整体放弃, 保持旧文档不变)
	{
		// 新条目 ref 依当前客户区(构建成功后才整体替换文档)
		for (DWORD i = 0; i < dwNum; i++)
		{
			CONST LayoutNode* pNode = LayoutModel::LayoutAt(pDoc->docState, i);
			CONST LayoutInfo* spec = (pNode != NULL) ? pNode->info : NULL;
			if (spec == NULL)
			{
				// 回滚已建条目
				for (DWORD k = 0; k < i; k++)
				{
					FreeEntryLayout(pEntries[k]);
				}
				goto END;
			}
			LayoutEntry& entry = pEntries[i];
			entry = LayoutEntry{};			// 零初始化
			entry.name = AllocatorHub::CopyString(spec->name);
			entry.label = AllocatorHub::CopyString(spec->label);
			entry.IsMaxiable = spec->IsMaxiable;
			entry.IsResizeable = spec->IsResizeable;
			entry.refSize = spec->refSize;
			entry.wndX = spec->wndX;		// 布局窗口桌面位置(文件可省略, 缺省 0)
			entry.wndY = spec->wndY;
			entry.pNode = (LayoutNode*)pNode;	// 中间态节点(元素规格/几何系数)
			entry.layout = NULL;	// 布局对象 = 预览唯一基座(全部条目共享; 内容按 active 装配)
			// 注: 几何系数已在中间态元素规格内(ElementSpec 的 x/y/width/height) ⇒ 无需逐枚写入;
			//     控件缓冲/事件绑定/几何由 SyncPreviewLayout 按中间态装配
		}
	}
	// 4. 整体替换文档(旧条目布局对象释放)
	if (pDoc->entries)
	{
		for (DWORD i = 0; i < pDoc->dwEntries; i++)
		{
			FreeEntryLayout(pDoc->entries[i]);
		}
	}
	pDoc->ReplaceEntries(pEntries, dwNum, 0);
	if (pfnBumpRev != NULL)
	{
		pfnBumpRev();		// 文档结构版本号递增
	}
	// 4-bis. 预览唯一基座: 建立(若尚未建立)并让全部文档条目的 layout 指向它;
	//        两枚旁挂表(文档原类型串 / 无缓冲控件声明文本)在装配控件时按索引写入
	if (pfnEnsureBase != NULL)
	{
		pfnEnsureBase();	// 会话面: 取 / 建预览唯一基座
	}
	// 4-ter. 预览选中态表达件在链外(编辑器自持, 见 editor\SelectionOverlay) ⇒ 文档整体替换时
	//        应用层无须清理任何附加件(编辑器侧选中索引由它自己的有效性校验收敛; KanatUIGenerateEditor::Update 第 1 步)
	// 5. 依客户区重建每个布局 ref; active 条目的控件内容由 SyncPreviewLayout 装配
	for (DWORD i = 0; i < pDoc->dwEntries; i++)
	{
		if (pfnRecalcEntry != NULL)
		{
			pfnRecalcEntry(i);	// 会话面: 条目几何重算
		}
	}
	bRet = TRUE;
END:
	return bRet;
}

// 函数: NewSolver
// 描述: 从内存池分配并构造一枚布局解析器(解析产物长期持有)
// 返回: 解析器指针; NULL = 池分配失败或 MemArena 未就绪
LayoutSolver* DocumentIO::NewSolver()
{
	LayoutSolver* solver = NULL;
	if (AllocatorHub::GetArena() == NULL)
	{
		return solver;
	}
	BufferInfo buf = AllocatorHub::GetArena()->Allocate(sizeof(LayoutSolver));
	if (buf.buffer == NULL || buf.size < sizeof(LayoutSolver))
	{
		return solver;
	}
	solver = new (buf.buffer) LayoutSolver(AllocatorHub::GetArena());
	return solver;
}

// ================================
// 区域: 新建
// ================================

// 函数: NewDocument
// 描述: 新建空白文档(1 个含 WND/TITLE/CNTR 的布局 NewLayout), 替换当前文档
BOOL DocumentIO::NewDocument()
{
	BOOL bRet = FALSE;
	if (pDoc == NULL)
	{
		return bRet;
	}
	// 清空旧条目
	if (pDoc->entries)
	{
		for (DWORD i = 0; i < pDoc->dwEntries; i++)
		{
			FreeEntryLayout(pDoc->entries[i]);
		}
		pDoc->entries = NULL;
		pDoc->dwEntries = 0;
	}
	// 清空文档中间态(新建文档不带旧布局模型; 旧中间态的块随本次清理回收)
	// 注: 内嵌(flex)成员不随文档变化(来源 = 外壳源) ⇒ 本处不释放它们(见 BuildFlexMembersFromSolver)
	LayoutModel::StateClear(pDoc->docState);
	pDoc->docState = FileMidState{};
	SetDocPath(NULL);				// 新建文档 ⇒ 无关联磁盘文件
	ClearDocDirty();				// 新建 = 空白文档, 无未保存改动
	// 预览选中态表达件在链外(编辑器自持, 见 editor\SelectionOverlay) ⇒ 新建文档时应用层无须清理附加件
	// 新建默认布局并激活
	// 命令本体归 editor\EditorCommands::AddDocLayout ⇒ 本模块不引编辑器命令头, 改经注入的命令面回调请求;
	//   未注入 ⇒ 显式降级(该步不做 ⇒ 新建中止, 不制造空态替身)。
	if (pfnAddLayout == NULL || !pfnAddLayout(u8"NewLayout"))
	{
		return bRet;
	}
	pDoc->dwActive = 0;
	if (pfnBumpRev != NULL)
	{
		pfnBumpRev();		// 文档结构版本号递增
	}
	bRet = TRUE;
	return bRet;
}

// ================================
// 区域: 保存族(文档 → 文件)
// ================================

// 函数: SaveDocument
// 描述: 把当前文档中间态写盘(整份文档: 全部布局 + 各自 autosize + 控件声明)
// 说明: 出口方向 = 中间态 → 文件; 格式按目标扩展名分派 —— .bin ⇒ bin 字节流(格式 v1),
//       其余 ⇒ 规范 JSON 文本(两者的写盘骨架相同: 备份 + 临时文件 + 原子替换, 见 LayoutWriter)
// 参数:
//		IN	path:	目标文件路径(空指针/空串 ⇒ 拒绝)
// 返回:
//		BOOL: TRUE = 已写入; FALSE = 文档未就绪 / 路径非法 / 生成或写盘失败
BOOL DocumentIO::SaveDocument(LPCSTR path)
{
	BOOL bRet = FALSE;
	if (pDoc == NULL || path == NULL || path[0] == '\0')
	{
		return bRet;
	}
	// 工程源保护: 仅当当前文档来自 .\KanatUIGenerate.json / .\KanatUIGenerate.bin 加载路径时, 保护该文件
	if (IsProtectedSavePath(path))
	{
		printf("[APP] 保存被拒: 当前文档来自工程源文档(exe 目录 KanatUIGenerate.json / KanatUIGenerate.bin), 该文件受保护(只读输入)\n");
		return bRet;
	}
	if (IsBinSavePath(path))
	{
		bRet = LayoutWriter::SaveBinToFile(pDoc->docState, path);
	}
	else
	{
		bRet = LayoutWriter::SaveToFile(pDoc->docState, path);
	}
	if (bRet)
	{
		SetDocPath(path);			// 保存成功 ⇒ 当前文档关联文件 = 保存路径
		ClearDocDirty();			// 写盘成功 ⇒ 清除未保存改动标记
	}
	return bRet;
}

// 函数: IsEngineSourcePath
// 描述: 判断给定路径是否指向"工程源文档"(exe 目录 KanatUIGenerate.json / KanatUIGenerate.bin; 相对路径/大小写不敏感)
// 说明: 两个文件名都是启动加载链的 exe 目录级来源(第 1/2 级)⇒ 同为"工程源只读输入"候选
// 参数:
//		IN	path:	待判断路径
// 返回:
//		BOOL: TRUE = 是工程源文档路径
BOOL DocumentIO::IsEngineSourcePath(LPCSTR path)
{
	return IsSameFilePath(path, ResourceLoader::GetFilePath("KanatUIGenerate.json")) ||
		IsSameFilePath(path, ResourceLoader::GetFilePath("KanatUIGenerate.bin"));
}

// 函数: IsProtectedSavePath
// 描述: 保存目标是否受保护 —— 目标是工程源文档, 且当前文档关联文件也是它
// 说明: 判据 = "文档就来自该工程源文件" ⇒ 拒写该文件; 其余来源(其它文件 / 内置资源 / 新建)不保护
// 参数:
//		IN	path:	保存目标路径
// 返回:
//		BOOL: TRUE = 受保护(SaveDocument 拒绝写盘)
BOOL DocumentIO::IsProtectedSavePath(LPCSTR path)
{
	BOOL bRet = FALSE;
	LPCSTR lpDocPath = NULL;
	if (pDoc == NULL || !IsEngineSourcePath(path))
	{
		return bRet;
	}
	// 仅当文档来自该加载路径(或已保存到该路径)才保护
	lpDocPath = pDoc->GetPath();
	bRet = (lpDocPath != NULL) && (lpDocPath[0] != '\0') &&
		IsSameFilePath(lpDocPath, path);
	return bRet;
}

// ================================
// 区域: 关联路径与未保存改动标记
// ================================

// 函数: GetDocPath
// 描述: 取当前文档关联的磁盘文件路径(无关联返回 NULL)
LPCSTR DocumentIO::GetDocPath()
{
	return (pDoc != NULL) ? pDoc->GetPath() : NULL;
}

// 函数: SetDocPath
// 描述: 写当前文档关联的磁盘文件路径(池内自持; NULL/空串 = 清空关联)
VOID DocumentIO::SetDocPath(LPCSTR path)
{
	if (pDoc == NULL)
	{
		return;
	}
	if (path == NULL || path[0] == '\0')
	{
		pDoc->SetPath(NULL);
		return;
	}
	pDoc->SetPath(AllocatorHub::CopyString(path));
}

// 函数: MarkDocDirty
// 描述: 置"有未保存改动"(编辑器侧改动文档内容的入口在改动生效后调用)
VOID DocumentIO::MarkDocDirty()
{
	if (pDoc != NULL)
	{
		pDoc->MarkDirty();
	}
}

// 函数: ClearDocDirty
// 描述: 清"未保存改动"标记(载入 / 新建 / 保存成功时调用)
VOID DocumentIO::ClearDocDirty()
{
	if (pDoc != NULL)
	{
		pDoc->ClearDirty();
	}
}
