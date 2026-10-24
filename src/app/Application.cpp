#include <windows.h>
#include <new>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app/Application.h"
#include "app/HostContext.h"		// 宿主能力与窗口句柄
#include "framework/base/mem/AllocatorHub.h"	// 会话分配器
#include "framework/dispatch/ResourceLoader.h"	// exe 目录与路径拼接
#include "framework/layout/GeometrySolver.h"	// 几何解算与模式/基准换算
#include "framework/base/BasicDesign.h"
#include "framework/base/CtrlBuffer.h"		// 控件缓冲(数据结构 / 容量 / 取址 / 构造与回收)
#include "framework/dispatch/FrameEvent.h"
#include "app/DocumentSource.h"		// 启动加载链与内置资源取件
#include "app/DocumentIO.h"			// 文档文件 I/O 与格式分派
#include "app/FlexHost.h"			// 内嵌(flex)窗口装配与查询
#include "app/PreviewSession.h"		// 预览基座与帧间同步
#include "doc/RawAttributes.h"		// 旁挂表族: 原类型串 / 声明文本
#include "framework/layout/LayoutBuilder.h"	// 布局对象构建与深拷贝
#include <rapidjson/document.h>

// ================================
// 类名: Application
// 说明: 应用程序主类 (单例模式)
// 说明: 多布局 = "布局文件含 1..N 个布局, 每布局自带 autosize"
// ================================

// 静态成员变量初始化
Application::AppData* Application::data = NULL;

// 其他常量
constexpr size_t MEMARENA_SIZE = 128 * 1024 * 1024;	// 应用线性分配器容量(128MB 虚拟内存)
// 列表类控件缓冲的取值(渲染端只读缓冲头部的 容量/每行数据长度)


// ================================
// 区域: 文件内静态工具区
// ================================

// 函数: DefaultRefSize
// 描述: 返回参考尺寸/经验参数默认样例(屏高模式 + 千分比) —— 取自布局模型(中间态)的默认值
static RefSize DefaultRefSize()
{
	return LayoutModel::DefaultRefSize();
}

// AutoModeNameOf / AutoModeIndex / FactorAbsPx / ApplySpecGeometry / RefValueToRatio /





// ================================
// 区域: 公开接口实现区
// ================================




// 函数: BuildEntryRef
// 描述: 依参考尺寸/经验参数(中间态 RefSize)重建条目窗口像素(wndW/wndH)与内部参考尺寸 ref
// 窗口化语义: 布局窗口客户区像素 = autosize width/height 千分比 x 基准/1000;
//       内部控件参考 ref 的 window 宽高 = 该窗口像素(控件按"窗口内"布局, 不相对屏幕画布)
VOID Application::BuildEntryRef(LayoutEntry& entry)
{
	// 1. 换算基准(mode 决定; WINDOW_* 模式在编辑器语境按屏兜底)
	CONST INT nMode = GeometrySolver::AutoModeIndex(entry.refSize.mode);
	DWORD base = GeometrySolver::EntryBasePx(nMode);
	if (base < 1) { base = 1; }
	// 2. 布局窗口客户区像素(千分比 x 基准/1000; 至少 16px)
	entry.wndW = (DWORD)((unsigned __int64)base * (DWORD)entry.refSize.width / 1000);
	entry.wndH = (DWORD)((unsigned __int64)base * (DWORD)entry.refSize.height / 1000);
	if (entry.wndW < 16) { entry.wndW = 16; }
	if (entry.wndH < 16) { entry.wndH = 16; }
	// 3. 内部参考尺寸(窗口宽高 = 该布局窗口像素)
	AutoSize::SizeInfo<DWORD> si = GeometrySolver::EntryRefSizeInfo(entry.refSize, base, entry.wndW, entry.wndH);
	entry.ref = AutoSize::CreateByFixedSize(si, (AutoSize::AutoSizeMode)nMode);
}
// 函数: FindEntryByLayout
// 描述: 按布局指针反查文档条目(唯一基座 ⇒ 当前 active 条目; 含外壳快照与内嵌(flex)成员条目; 未命中返回 NULL)
// 说明: 预览布局唯一基座被全部文档条目共享, 按指针逐条目匹配无法区分归属
//       ⇒ 基座归属"当前 active 条目"(取用路径 GetDocLayout(index) 不变)。
LayoutEntry* Application::FindEntryByLayout(Layout* layout)
{
	LayoutEntry* pRet = NULL;
	if (layout == NULL || data == NULL)
	{
		return pRet;
	}
	if (layout == PreviewSession::GetPreviewBase() && data->doc.entries != NULL && data->doc.dwActive < data->doc.dwEntries)
	{
		return &data->doc.entries[data->doc.dwActive];
	}
	// 条目数组反查段归承载者
	//   其余三条来源(预览基座归属 / 外壳快照 / 内嵌成员)属会话面 ⇒ 由本函数编排
	pRet = data->doc.FindByLayout(layout);
	//   ⇒ 判据经其只读入口取条目指针(调用位置与判据形态不变; 嵌套块限定作用域)
	{
		LayoutEntry* pShellEntry = PreviewSession::GetShellEntry();
		if (pRet == NULL && pShellEntry != NULL && pShellEntry->layout == layout)
		{
			pRet = pShellEntry;
		}
	}
	// 内嵌(flex)窗口成员: 与外壳快照同性质(独立条目; 几何系数同样存于中间态)
	if (pRet == NULL)
	{
		pRet = FlexHost::FindFlexEntry(layout);
	}
	return pRet;
}
// 函数: ApplyEntryGeometry
// 描述: 依条目 ref 对中间态元素规格(几何系数声明)求解像素几何(不依赖任何外部全局参考)
// 说明: 元素规格与文档索引一一对应(中间态链; 不含预览选中标记); 中间态元素均带系数(不存在"无系数"态) ——
//       编辑器新增控件时其元素以像素 offset + 零系数表达(见 InsertCtrlSpecAt),
//       几何重解结果与新增当刻的像素几何一致。
//       注: 文档条目(预览)的控件几何由 SyncPreviewLayout 逐枚解算 ⇒ 本函数服务外壳快照
//           (线性只读展开态)与 SetShellClientSize 的客户区校正。
VOID Application::ApplyEntryGeometry(LayoutEntry& entry)
{
	Layout* layout = entry.layout;
	if (layout == NULL)
	{
		return;
	}
	AutoSize::SizeInfo<FLOAT> ref = entry.ref.GetSizeInfo<FLOAT>();
	size_t num = layout->GetTotalNum();
	for (size_t i = 0; i < num; i++)
	{
		CtrlInfo* pCtrl = layout->GetCtrlInfo(i);
		ElementSpec* pSpec = Document::SpecAt(entry, i);
		if (pSpec != NULL && pCtrl != NULL)
		{
			GeometrySolver::ApplySpecGeometry(*pCtrl, pSpec, ref);
		}
		// 中间态元素缺失(尚未挂接) ⇒ 保持现状像素, 索引照常推进
	}
	// 有效坐标: 本批声明坐标已全量重解 ⇒ 按父链统一重推(结构件不参与; 幂等)
	layout->DeriveCtrlPositions();
}
// 函数: RebindDocStateNodes
// 描述: 把文档条目的中间态布局节点按索引重新挂接到当前 docState 的布局链上
// 说明: 条目索引与布局链索引一一对应; 中间态的增删不改动未涉及块的地址, 本函数用于条目数组
//       重建或布局集合变化之后按索引对齐条目节点
VOID Application::RebindDocStateNodes()
{
	// 条目节点按索引对齐的判据本体已下沉承载者(判空口径亦随之下沉) ⇒ 本函数为转发
	if (data == NULL)
	{
		return;
	}
	data->doc.RebindNodes();
}
// 函数: GetCtrlSpec
// 描述: 取某布局内指定文档索引的中间态元素规格(布局无效/索引越界 ⇒ NULL)
// 说明: 几何系数存于中间态元素规格; 控件对象只持像素几何;
//       索引口径 = 文档索引(不含预览选中标记; 与控件列表行号 / 旁挂表同口径)。
ElementSpec* Application::GetCtrlSpec(Layout* layout, size_t index)
{
	ElementSpec* pRet = NULL;
	LayoutEntry* pEntry = FindEntryByLayout(layout);
	if (pEntry != NULL)
	{
		pRet = Document::SpecAt(*pEntry, index);
	}
	return pRet;
}
// 函数: ApplyDocCtrlGeometry
// 描述: 按中间态元素规格就地重解指定布局内一枚控件的像素几何(编辑器系数/偏移提交后的生效入口)
// 说明: 与 ApplyDocAutoRatio 同法 —— 不做整体重装(控件数量/类型缓冲/事件绑定均不动),
//       只按中间态元素规格与条目参考尺寸重算该控件的 position/size。
// 参数:
//		IN	layout:	目标布局对象(编辑器 = 当前 active 条目承载的预览基座)
//		IN	index:	文档索引(与中间态元素索引一一对应)
// 返回:
//		BOOL: TRUE = 已就地重解; FALSE = 入参无效 / 条目或元素规格缺失
BOOL Application::ApplyDocCtrlGeometry(Layout* layout, size_t index)
{
	BOOL bRet = FALSE;
	if (layout == NULL)
	{
		return bRet;			// 入参无效(未挂接布局对象)
	}
	LayoutEntry* pEntry = FindEntryByLayout(layout);
	// 索引口径: 文档索引(链内只含文档控件 ⇒ 与布局控件索引同一口径, 无需换算)
	if (pEntry == NULL || index >= layout->GetTotalNum())
	{
		return bRet;			// 非文档条目(或索引越界): 无中间态规格可依
	}
	ElementSpec* pSpec = Document::SpecAt(*pEntry, index);
	if (pSpec == NULL)
	{
		return bRet;			// 中间态元素缺失 ⇒ 保持现状像素
	}
	GeometrySolver::ApplySpecGeometry(layout->GetCtrlInfoRef(index), pSpec, pEntry->ref.GetSizeInfo<FLOAT>());
	// 有效坐标: 该枚声明坐标已变 ⇒ 按父链重推(只改本枚与其后代的有效坐标; 幂等)
	layout->DeriveCtrlPositions();
	bRet = TRUE;
	return bRet;
}
// 函数: FindDocIndex
// 描述: 按布局指针反查文档条目索引(唯一基座 ⇒ 当前 active 条目; 不含外壳快照; 未命中返回 -1)
LONG Application::FindDocIndex(Layout* layout)
{
	LONG nRet = -1;
	if (layout == NULL || data == NULL || data->doc.entries == NULL)
	{
		return nRet;
	}
	// 预览唯一基座被全部条目共享 ⇒ 归属当前 active 条目(与 FindEntryByLayout 同法)
	if (layout == PreviewSession::GetPreviewBase())
	{
		return (data->doc.dwActive < data->doc.dwEntries) ? (LONG)data->doc.dwActive : -1;
	}
	for (DWORD i = 0; i < data->doc.dwEntries; i++)
	{
		if (data->doc.entries[i].layout == layout)
		{
			nRet = (LONG)i;
			break;
		}
	}
	return nRet;
}
// 函数: SetCtrlType
// 描述: 把指定布局内某控件改为指定类型(中间态元素类型串 + 布局对象控件类型 + 该控件缓冲按新类型重建)
// 说明: 控件像素几何由中间态元素规格决定 ⇒ 本函数不改几何; 旧缓冲留在分配器(不逐块释放)。
// 参数:
//		IN	layout:	目标布局对象
//		IN	index:	文档索引(与中间态元素索引一一对应)
//		IN	type:	新类型(不可构建类型 INVALID/UNKNOWN 拒绝)
// 返回:
//		BOOL: TRUE = 已改并重建缓冲; FALSE = 入参无效 / 元素规格或控件缺失 / 缓冲重建失败
BOOL Application::SetCtrlType(Layout* layout, size_t index, CtrlType type)
{
	BOOL bRet = FALSE;
	ElementSpec* es = NULL;
	CtrlInfo* info = NULL;
	if (layout == NULL || layout->IsInvalid())
	{
		return bRet;
	}
	if (type == CTRL_TYPE_INVALID || type == CTRL_TYPE_UNKNOWN)
	{
		return bRet;
	}
	// 索引口径: 文档索引(链内只含文档控件 ⇒ 与布局控件索引同一口径, 无需换算)
	es = GetCtrlSpec(layout, index);
	info = layout->GetCtrlInfo(index);
	if (es == NULL || info == NULL)
	{
		return bRet;
	}
	// 1. 中间态元素类型串(类型身份 = 类型表: 枚举 -> 串)
	LayoutModel::StrCopy(es->type, STR_TYPE, GetTypeName(type));
	// 2. 布局对象控件: 写新类型并按新类型重建缓冲(文本取中间态声明文本)
	info->type = type;
	CtrlBuffer::FreeCtrlBuffer(info);		// 旧类型缓冲回池(重建之前回收)
	if (CtrlBuffer::PrepareCtrlBuffer(info, es->text) == FALSE)
	{
		return bRet;
	}
	// 3. 旁挂"原类型串"随之失效(该索引现为已知类型)
	RawAttributes::RemoveRawTypeAt(layout, index);
	bRet = TRUE;
	return bRet;
}
// 函数: MoveCtrl
// 描述: 拖拽重排: 把指定控件移到同一布局内的新相对位置(控件顺序真源 = 中间态元素链)
// 说明: 按"取出源的三处副本(控件值 / 中间态元素规格 / 原类型串与声明文本旁挂表) → 三处同步删除
//       → 三处同步插入"实施(与 DeleteSelectedCore / AddControlCore 同法); 控件缓冲与 label 指针
//       随控件值整体搬移(池内自持, 不逐块释放); 控件 ID 不变(移动不涉及控件身份)。
// 参数: 见 Application.h
// 返回: BOOL TRUE = 已重排; FALSE = 入参无效 / 索引越界 / 源目标同处 / 中间态或控件链操作失败
BOOL Application::MoveCtrl(Layout* layout, size_t from, size_t dest)
{
	// 变量声明区(源副本: 控件值 + 中间态元素规格 + 旁挂两表)
	BOOL bRet = FALSE;
	LONG nDocIdx = -1;			// 文档条目索引
	size_t nDoc = 0;
	size_t qwFrom = 0;
	size_t qwDest = 0;
	CtrlInfo ci{};
	ElementSpec es{};
	LPCSTR szRawType = NULL;
	LPCSTR szRawText = NULL;
	if (layout == NULL || layout->IsInvalid() || data == NULL)
	{
		return bRet;
	}
	// 索引口径: from/dest 均为文档索引(链内只含文档控件 ⇒ 文档索引 == 布局控件索引;
	nDoc = layout->GetTotalNum();
	if (from >= nDoc || dest >= nDoc || from == dest)
	{
		return bRet;
	}
	qwFrom = from;
	qwDest = dest;
	// 1. 取出源的三处副本(控件值整体拷贝 —— 其 label 与缓冲指针指向池内数据, 删除不释放)
	ci = layout->GetCtrlInfoRef(qwFrom);
	{
		ElementSpec* pSpec = GetCtrlSpec(layout, from);
		if (pSpec == NULL)
		{
			goto END;
		}
		es = *pSpec;
	}
	szRawType = RawAttributes::GetRawType(layout, from);
	szRawText = RawAttributes::GetRawText(layout, from);
	// 2. 三处同步删除源(中间态元素 / 控件链 / 旁挂两表)
	nDocIdx = FindDocIndex(layout);
	if (nDocIdx < 0 || !data->doc.RemoveSpecAt((DWORD)nDocIdx, from))
	{
		goto END;
	}
	if (!layout->RemoveCtrlInfo(qwFrom))
	{
		goto END;
	}
	RawAttributes::RemoveRawTypeAt(layout, from);
	RawAttributes::RemoveRawTextAt(layout, from);
	// 3. 三处同步插入到目标位置(索引已按"删除源之后"口径; 链上无标记 ⇒ 文档索引 == 基座索引)
	if (!layout->AddCtrlInfo(qwDest, ci))
	{
		goto END;
	}
	if (nDocIdx < 0 || !data->doc.InsertSpecAt((DWORD)nDocIdx, layout, dest))
	{
		goto END;
	}
	{
		ElementSpec* pSpec = GetCtrlSpec(layout, dest);
		if (pSpec == NULL)
		{
			goto END;
		}
		*pSpec = es;			// 回写源元素规格(含 id / 文本 / 事件 / 四轴系数与偏移)
	}
	RawAttributes::InsertRawTypeAt(layout, dest, szRawType);
	RawAttributes::InsertRawTextAt(layout, dest, szRawText);
	bRet = TRUE;
END:
	// 出口统一: 层次整理(搬移改变了链序与前序包含关系 ⇒ 整体重推; 口径见 ControlSpec.h)
	if (bRet != FALSE)
	{
		layout->DeriveCtrlLayers();
	}
	return bRet;
}
// 函数: FreeEntryLayout
// 描述: 断开会话条目对布局对象的引用(条目数组本身由内存池持有, 不需释放)
// 说明: 文档条目的布局对象 = 预览唯一基座(全部条目共享);
//       基座由 app\PreviewSession::FreePreviewBase 释放, 外壳快照条目由该模块 FreeShellSnapshot 释放。
VOID Application::FreeEntryLayout(LayoutEntry& entry)
{
	entry.layout = NULL;
}
// ================================
// ================================

// ================================
// ================================


// ================================
// ================================


// GetEmbeddedDoc / GetEmbeddedDocBin / ResolveDocumentSource 的实现已随本区迁出本文件
//   ⇒ 见 app\DocumentSource.cpp(前二者为内置资源取件的薄封装
//   第 3/4 级使用; 后者为四级探测本体)。随本区一并迁出的 include: app/resource.h
//   (本文件内唯一使用点即前二者的资源 ID)。

// 函数: Init
// 描述: 初始化应用单例: 内存分配器 -> 启动加载链(四级顺序探测, 见本文件"启动加载链区") -> 构建全部布局
// 说明: 无可用文档时诊断并启动失败
BOOL Application::Init()
{
	BOOL ret = FALSE;
	// 1. 建立会话分配器(文档/会话级线性分配器 + 不定生命周期小对象池)
	if (AllocatorHub::Init(MEMARENA_SIZE) == FALSE)
	{
		goto END;
	}
	// 一次性静态注入 Layout 与 LayoutModel 的分配器(本步即 Init 的第 1 步, 早于后续所有 CreateLayout):
	//   Layout      ⇒ Arena(布局对象自身 + 控件信息连续数组) + MemPool(链节点);
	//   LayoutModel ⇒ 同上两个分配器(中间态各池取自 Arena)。
	Layout::SetAllocators(AllocatorHub::GetArena(), AllocatorHub::GetSmallPool());
	LayoutModel::SetAllocators(AllocatorHub::GetArena(), AllocatorHub::GetSmallPool());
	ProjCR::SetAllocators(AllocatorHub::GetArena(), AllocatorHub::GetSmallPool());
	// 2. 分配成员存储
	data = (AppData*)AllocatorHub::GetArena()->Allocate(sizeof(AppData)).buffer;
	if (data == NULL)
	{
		goto ERR_POOL;
	}
	{
		// 成员初始化
		data->doc.entries = NULL;
		PreviewSession::ResetPreviewBase();
		data->doc.docState = FileMidState{};	// 文档中间态(载入文档时由解析产物填充)
		data->doc.dwEntries = 0;
		data->doc.dwActive = 0;
		HostContext::SetWindowHandle(NULL);
		HostContext::SetPreviewWindowHandle(NULL);	// 预览窗口(建窗后由宿主注册)
		data->nDocRev = 1;
		data->doc.ClearDirty();			// 尚未产生未保存改动
		// 内嵌(flex)窗口成员(随外壳快照装配): 三字段已迁 app\FlexHost
		FlexHost::SetShellBase(NULL);
		FlexHost::FreeFlexLayouts();
		ResourceLoader::Init();
	}
	// 文档 I/O 模块接线: 承载者与"会话面回调三件套" ——
	//   承载者按值内嵌在 data 内 ⇒ 必须在本步之后注入(否则 app\DocumentIO 取到 NULL);
	//   (预览唯一基座建立 / 文档版本号递增 / 条目几何重算)。
	DocumentIO::SetDocument(&data->doc);
	DocumentIO::SetSessionHooks(&PreviewSession::EnsurePreviewBaseHook, &PreviewSession::BumpDocRevHook, &PreviewSession::RecalcEntryGeometry);
	// 预览基座与帧间同步 + 外壳快照模块接线: 承载者 / 文档版本号地址 /
	//   会话分配器句柄 —— 与上一行同点同序(三者都经 data 与分配器直取 ⇒ 必须在本步之后注入,
	//   否则该模块取到 NULL); 分配器句柄同时是外壳快照区"应用未就绪"判据的来源。
	PreviewSession::SetSession(&data->doc, &data->nDocRev, AllocatorHub::GetArena());
	// 旁挂表族模块接线: 条目反查函数(本类 FindEntryByLayout, 其
	//   四条来源含承载者条目数组 / 预览基座归属 / 外壳快照 / 内嵌成员 ⇒ 必须在本步之后注入,
	//   否则该模块取到 NULL); 同时充当该模块"应用未就绪"判据的来源。
	RawAttributes::SetEntryLookup(&Application::FindEntryByLayout);
	// 3. 启动加载链(四级顺序探测, 命中即用)
	{
		DocSource src{};
		DWORD dwMask = 0xFu;		// 默认四级全放开
		BOOL bHit = FALSE;
		BOOL bDoc = FALSE;
		// 逐级尝试: 每一级"探测 → 尝试载入"; 载入被拒(格式 / 校验 / 构建失败)则点名诊断,
		// 并把该级并入"已试掩码"后继续探测后续级别; 全链无可用来源或全被拒 ⇒ 启动中止。
		{
			DWORD dwTried = 0;
			while ((dwMask & ~dwTried) != 0)
			{
				src = DocSource{};
				if (!DocumentSource::ResolveDocumentSource(&src, dwMask & ~dwTried))
				{
					break;		// 允许的级别里已无可用来源
				}
				bHit = TRUE;
				// 第 3/4 级 = 随 exe 内置的资源(内存入口); 第 1/2 级 = exe 目录外部文件(路径入口);
				// 两个入口内部按内容嗅探分派 JSON / bin ⇒ 此处无需按 bBin 分支。
				// 注: 路径入口读毕即关文件 ⇒ 载入被拒时不遗留句柄。
				bDoc = src.bEmbedded ? DocumentIO::LoadDocumentFromMemory(src.pData, src.cbData) : DocumentIO::LoadDocument(src.lpPath);
				if (bDoc)
				{
					break;
				}
				printf("[APP] 加载链: 第 %d 级(%s)载入被拒 ⇒ 继续探测后续级别\n",
					(int)src.nLevel, src.lpDesc ? src.lpDesc : "?");
				dwTried |= (1u << (src.nLevel - 1));
			}
		}
		if (!bDoc)
		{
			if (bHit)
			{
				printf("[APP] 加载链: 全部可用来源均载入被拒(解析/构建失败) ⇒ 启动中止(不内置代码回退)\n");
			}
			else
			{
				printf("[APP] 加载链: 无可用文档 ⇒ 启动中止(不内置代码回退)\n");
			}
			fflush(stdout);
			goto ERR_POOL;
		}
		printf("[APP] 加载链: 命中来源 = %s\n", src.lpDesc ? src.lpDesc : "?");
		// 4. 外壳快照: 与文档同一命中级别, 解析 name==KanatUIGenerateEditor 布局为独立副本,
		//    驱动编辑窗口内容(会话内固定, 不随打开其他文档变化); 文件缺失/无该名/失败 → 快照无效。
		//    本步非致命, 失败不阻断 Init。
		if (bHit)
		{
			if (src.bEmbedded)
			{
				PreviewSession::LoadShellSnapshotFromMemory(src.pData, src.cbData);
			}
			else
			{
				PreviewSession::LoadShellSnapshot(src.lpPath);
			}
		}
	}
	ret = TRUE;
	goto END;

ERR_POOL:
	// 清理会话分配器
	if (AllocatorHub::GetArena() != NULL)
	{
		Layout::SetAllocators(NULL, NULL);
		LayoutModel::SetAllocators(NULL, NULL);
		ProjCR::SetAllocators(NULL, NULL);
		AllocatorHub::Release();
		data = NULL;
		DocumentIO::SetDocument(NULL);	// 承载者随分配器失效 ⇒ 同点复位
		PreviewSession::SetSession(NULL, NULL, NULL);	// 会话句柄随承载者失效 ⇒ 同点复位
		RawAttributes::SetEntryLookup(NULL);	// 条目反查随承载者失效 ⇒ 同点复位
	}
END:
	return ret;
}
// 函数: Release
// 描述: 释放应用单例
VOID Application::Release()
{
	if (AllocatorHub::GetArena() != NULL)
	{
		// 成对释放: 中间态的块与预览唯一基座先回收(块/链节点回小对象池), 再整体释放线性分配器
		if (data != NULL)
		{
			LayoutModel::StateClear(data->doc.docState);
		}
		FlexHost::FreeFlexLayouts();		// 内嵌(flex)成员(布局对象显式析构; 条目内存随线性分配器)
		// 外壳快照(含内嵌成员 + 释快照条目 + 清有效位)
		PreviewSession::ResetShellSnapshot();
		FlexHost::FreeFlexLayouts();
		PreviewSession::FreePreviewBase();	// 预览唯一基座
		Layout::SetAllocators(NULL, NULL);
		LayoutModel::SetAllocators(NULL, NULL);
		ProjCR::SetAllocators(NULL, NULL);
		AllocatorHub::Release();
		data = NULL;
		DocumentIO::SetDocument(NULL);	// 承载者随分配器失效 ⇒ 同点复位
		PreviewSession::SetSession(NULL, NULL, NULL);	// 会话句柄随承载者失效 ⇒ 同点复位
		RawAttributes::SetEntryLookup(NULL);	// 条目反查随承载者失效 ⇒ 同点复位
	}
}
// 函数: IsReady
// 描述: 应用是否就绪(成员块 AppData 已分配; 即"分配成员存储"一步之后、Release / 失败清理之前)
BOOL Application::IsReady()
{
	return (data != NULL) ? TRUE : FALSE;
}

// ================================
// 区域: 文档模型访问区
// ================================

// 函数: GetDocument
// 描述: 取文档承载者地址(doc\Document; 供宿主在启动期注入编辑器)
// 说明: 承载者按值内嵌在 data 内 ⇒ 地址 = data + offsetof(AppData, doc), 应用就绪后终生稳定;
//   data == NULL(Init 未成功 / 已 Release)时返回 NULL —— 不回落空态替身(承载者保持"纯数据、无策略"定位,
//   空态降级由调用方自行处置)。
// 返回:
//		Document*: 承载者地址; 应用未就绪时返回 NULL
Document* Application::GetDocument()
{
	return (data != NULL) ? &data->doc : NULL;
}
// -------- 会话面回调(供宿主注入命令层 editor\EditorCommands) --------
// 函数: GetDocCount
// 描述: 文档布局数量
DWORD Application::GetDocCount()
{
	return (data != NULL) ? data->doc.GetCount() : 0;
}
// 函数: GetDocState
// 描述: 取当前文档中间态(只读; 供序列化写出等只读消费)
// 返回:
//		CONST FileMidState&: 文档中间态; 应用未就绪时返回空态
CONST FileMidState& Application::GetDocState()
{
	static CONST FileMidState sEmptyState{};
	if (data == NULL)
	{
		return sEmptyState;
	}
	return data->doc.GetState();
}
// 函数: GetDocLayout
// 描述: 获取指定索引布局引用
Layout& Application::GetDocLayout(DWORD index)
{
	LayoutEntry* pEntry = (data != NULL) ? data->doc.GetEntry(index) : NULL;
	if (pEntry != NULL && pEntry->layout != NULL)
	{
		return *(pEntry->layout);
	}
	return Layout::GetNullObject();
}
// 函数: GetActiveIndex
// 描述: 当前激活布局索引
DWORD Application::GetActiveIndex()
{
	return (data != NULL) ? data->doc.GetActive() : 0;
}
// 函数: SetActiveIndex
// 描述: 切换激活布局(布局管理选中; 编辑器/预览目标随之变化)
BOOL Application::SetActiveIndex(DWORD index)
{
	BOOL bRet = FALSE;
	DWORD dwCount = 0;
	if (data == NULL)
	{
		return bRet;
	}
	// 文档未就绪(条目数组未挂接 / 条目数为 0): GetEntry(0) 为 NULL ⇒ 拒绝
	if (data->doc.GetEntry(0) == NULL)
	{
		return bRet;
	}
	dwCount = data->doc.GetCount();
	if (index >= dwCount) { index = dwCount - 1; }
	if (data->doc.GetActive() != index)
	{
		data->doc.SetActive(index);
		data->nDocRev++;
		// 预览唯一基座的内容随 active 更新(布局对象身份不变)
		PreviewSession::SyncPreviewLayout();
	}
	bRet = TRUE;
	return bRet;
}
// 函数: GetDocRef / GetDocRefSize
// 描述: 布局参考尺寸(解算后像素) / 参考尺寸与经验参数(中间态) 访问
CONST AutoSize& Application::GetDocRef(DWORD index)
{
	static AutoSize sNull = AutoSize::CreateByRatio(
		622, 432, 25, 17, 10, 4, AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT);
	LayoutEntry* pEntry = (data != NULL) ? data->doc.GetEntry(index) : NULL;
	if (pEntry != NULL)
	{
		return pEntry->ref;
	}
	return sNull;
}
CONST RefSize& Application::GetDocRefSize(DWORD index)
{
	static RefSize sNull = DefaultRefSize();
	LayoutEntry* pEntry = (data != NULL) ? data->doc.GetEntry(index) : NULL;
	if (pEntry != NULL)
	{
		return pEntry->refSize;
	}
	return sNull;
}


// 函数: GetActiveLayout / GetActiveRef / GetActiveRefSize
Layout& Application::GetActiveLayout()
{
	return GetDocLayout(GetActiveIndex());
}
CONST AutoSize& Application::GetActiveRef()
{
	return GetDocRef(GetActiveIndex());
}
CONST RefSize& Application::GetActiveRefSize()
{
	return GetDocRefSize(GetActiveIndex());
}

// RecalcEntryGeometry(条目几何重算: 依 autosize 重建 ref + 仅 active 条目按中间态更新基座)

// ================================
// 区域: 激活布局经验参数(RefSize)编辑区
// ================================

// 函数: ActiveAutoSetRatio
// 描述: 写激活布局单项参考量; 全部六项即时生效(0/1=窗口宽高直接改窗口像素)
// 说明: 入参按该条目 unit 解释(见 SetDocAutoRatio)
BOOL Application::ActiveAutoSetRatio(INT index, DWORD ratio)
{
	// 转调索引版 SetDocAutoRatio(取当前激活索引)
	if (data == NULL)
	{
		return FALSE;
	}
	return SetDocAutoRatio(data->doc.GetActive(), index, ratio);
}
// 函数: SetDocAutoRatio
// 描述: 写指定布局条目的 autosize 单项参考量并重算该条目几何(非 active 亦可)
// 说明: 入参按该条目 unit 解释(0 = 千分比原值; 1 = 像素, 按基准反算为千分比); 中间态恒存千分比
BOOL Application::SetDocAutoRatio(DWORD docIndex, INT index, DWORD ratio)
{
	BOOL bRet = FALSE;
	if (data == NULL || index < 0 || index >= 6)
	{
		return bRet;
	}
	LayoutEntry* pEntry = data->doc.GetEntry(docIndex);
	if (pEntry == NULL)
	{
		return bRet;			// 越界(文档已替换/布局已删): 拒绝写入
	}
	LayoutEntry& entry = *pEntry;
	// 入参按该条目单位解释(千分比原值 / 像素按基准反算), 中间态恒存千分比
	CONST DWORD dwRatio = GeometrySolver::RefValueToRatio(ratio, GeometrySolver::EntryBasePx(GeometrySolver::AutoModeIndex(entry.refSize.mode)), entry.refSize.unit);
	// 参考量 = 条目字段与中间态布局信息同源(两处同写; 中间态才是保存写出物 ⇒ 只写副本会丢改动)
	if (entry.pNode != NULL && entry.pNode->info != NULL)
	{
		LayoutModel::SetRefSizeValueAt(entry.pNode->info->refSize, index, (INT)dwRatio);
		entry.refSize = entry.pNode->info->refSize;		// 条目副本跟随(内存显示一致)
	}
	else
	{
		LayoutModel::SetRefSizeValueAt(entry.refSize, index, (INT)dwRatio);	// 无中间态节点 ⇒ 只改副本
	}
	data->nDocRev++;
	bRet = TRUE;
	PreviewSession::RecalcEntryGeometry(docIndex);
	return bRet;
}
// 函数: ApplyDocAutoRatio
// 描述: 写指定布局条目的 autosize 单项参考量并按新参考尺寸就地更新预览基座控件几何
// 说明: 与 SetDocAutoRatio 的差别 = 不做整体重装 —— 只重建该条目 ref, 再按中间态元素系数逐枚改写
//       布局对象控件的像素几何(控件数量/类型缓冲/事件绑定均不动); 非 active 条目只重建自身 ref;
//       入参按该条目 unit 解释(0 = 千分比原值; 1 = 像素, 按基准反算为千分比), 中间态恒存千分比
BOOL Application::ApplyDocAutoRatio(DWORD docIndex, INT index, DWORD ratio)
{
	BOOL bRet = FALSE;
	if (data == NULL || index < 0 || index >= 6)
	{
		return bRet;
	}
	LayoutEntry* pEntry = data->doc.GetEntry(docIndex);
	if (pEntry == NULL)
	{
		return bRet;			// 越界(文档已替换/布局已删): 拒绝写入
	}
	LayoutEntry& entry = *pEntry;
	// 入参按该条目单位解释(千分比原值 / 像素按基准反算), 中间态恒存千分比
	CONST DWORD dwRatio = GeometrySolver::RefValueToRatio(ratio, GeometrySolver::EntryBasePx(GeometrySolver::AutoModeIndex(entry.refSize.mode)), entry.refSize.unit);
	// 参考量 = 条目字段与中间态布局信息同源(两处同写; 中间态才是保存写出物 ⇒ 只写副本会丢改动)
	if (entry.pNode != NULL && entry.pNode->info != NULL)
	{
		LayoutModel::SetRefSizeValueAt(entry.pNode->info->refSize, index, (INT)dwRatio);
		entry.refSize = entry.pNode->info->refSize;		// 条目副本跟随(内存显示一致)
	}
	else
	{
		LayoutModel::SetRefSizeValueAt(entry.refSize, index, (INT)dwRatio);	// 无中间态节点 ⇒ 只改副本
	}
	data->nDocRev++;
	// 按新参考尺寸重建该条目 ref; 预览唯一基座只承载 active 条目 ⇒ 仅 active 就地改写像素几何
	BuildEntryRef(entry);
	if (docIndex == data->doc.GetActive())
	{
		ApplyEntryGeometry(entry);
	}
	bRet = TRUE;
	return bRet;
}
// 函数: ApplyDocAutoMode
// 描述: 写指定布局条目的 autosize 模式串, 并按新换算基准就地更新预览基座控件几何
// 说明: 与 ApplyDocAutoRatio 同法 —— 只重建该条目 ref, 再按中间态元素系数逐枚改写
//       布局对象控件像素几何(控件数量/类型缓冲/事件绑定均不动)。
BOOL Application::ApplyDocAutoMode(DWORD docIndex, LPCSTR mode)
{
	BOOL bRet = FALSE;
	if (data == NULL || mode == NULL || mode[0] == '\0')
	{
		return bRet;
	}
	LayoutEntry* pEntry = data->doc.GetEntry(docIndex);
	if (pEntry == NULL)
	{
		return bRet;			// 越界(文档已替换/布局已删): 拒绝写入
	}
	LayoutEntry& entry = *pEntry;
	// 模式串 = 条目字段与中间态布局信息同源(两处同写; 中间态才是保存写出物)
	if (entry.pNode != NULL && entry.pNode->info != NULL)
	{
		LayoutModel::StrCopy(entry.pNode->info->refSize.mode, sizeof(entry.pNode->info->refSize.mode), mode);
		entry.refSize = entry.pNode->info->refSize;		// 条目副本跟随
	}
	else
	{
		LayoutModel::StrCopy(entry.refSize.mode, sizeof(entry.refSize.mode), mode);	// 无中间态节点 ⇒ 只改副本
	}
	data->nDocRev++;
	// 基准随模式变 ⇒ 重建 ref 并就地改写像素几何(仅 active 条目承载于预览基座)
	BuildEntryRef(entry);
	if (docIndex == data->doc.GetActive())
	{
		ApplyEntryGeometry(entry);
	}
	bRet = TRUE;
	return bRet;
}
// 函数: ApplyDocAutoUnit
// 描述: 写指定布局条目的 autosize 参考量单位(0 = 千分比 / 1 = 像素)
// 说明: 单位只决定"参考量输入值如何解释"(见 RefValueToRatio), 中间态存储为千分比
//       ⇒ 换单位不改参考量的像素几何(控件像素几何与预览窗口尺寸均不变)。
BOOL Application::ApplyDocAutoUnit(DWORD docIndex, INT unit)
{
	BOOL bRet = FALSE;
	if (data == NULL || unit < 0 || unit > 1)
	{
		return bRet;
	}
	LayoutEntry* pEntry = data->doc.GetEntry(docIndex);
	if (pEntry == NULL)
	{
		return bRet;			// 越界(文档已替换/布局已删): 拒绝写入
	}
	LayoutEntry& entry = *pEntry;
	// 单位 = 条目字段与中间态布局信息同源(两处同写; 中间态才是保存写出物)
	if (entry.pNode != NULL && entry.pNode->info != NULL)
	{
		entry.pNode->info->refSize.unit = unit;
		entry.refSize = entry.pNode->info->refSize;		// 条目副本跟随
	}
	else
	{
		entry.refSize.unit = unit;						// 无中间态节点 ⇒ 只改副本
	}
	data->nDocRev++;
	bRet = TRUE;
	return bRet;
}

// ================================
// 区域: 布局窗口(预览窗口)几何访问区
// ================================

// 函数: GetDocWndRect
// 描述: 取指定布局的预览窗口矩形
BOOL Application::GetDocWndRect(DWORD index, LONG* x, LONG* y, DWORD* w, DWORD* h)
{
	BOOL bRet = FALSE;
	LayoutEntry* pEntry = (data != NULL) ? data->doc.GetEntry(index) : NULL;
	if (pEntry != NULL)
	{
		if (x) { *x = pEntry->wndX; }
		if (y) { *y = pEntry->wndY; }
		if (w) { *w = pEntry->wndW; }
		if (h) { *h = pEntry->wndH; }
		bRet = TRUE;
	}
	return bRet;
}
// 函数: SetDocWndPos
// 描述: 更新指定布局窗口的桌面位置(预览窗口标题拖动写回)
// 说明: 条目字段与中间态布局信息同源(两处同写); 位置未变时不改动、不打版本号。
BOOL Application::SetDocWndPos(DWORD index, LONG x, LONG y)
{
	BOOL bRet = FALSE;
	LayoutEntry* pEntry = (data != NULL) ? data->doc.GetEntry(index) : NULL;
	if (pEntry != NULL)
	{
		LayoutEntry& entry = *pEntry;
		if (entry.wndX != x || entry.wndY != y)
		{
			entry.wndX = x;
			entry.wndY = y;
			// 中间态布局信息同步(窗口桌面位置; 与条目字段同源)
			if (entry.pNode != NULL && entry.pNode->info != NULL)
			{
				entry.pNode->info->wndX = x;
				entry.pNode->info->wndY = y;
			}
			data->nDocRev++;
		}
		bRet = TRUE;
	}
	return bRet;
}
// ================================
// 区域: 客户区 / 窗口访问区
// ================================


// ================================
// 区域: 布局模型(中间态)转换区
// ================================

// 函数: BuildLayoutFromState
// 描述: 把布局模型中指定索引的布局展开为布局对象(自由函数 FormatToUI 的实现落点)
// 参数:
//		IN	state:		布局模型中间态
//		IN	index:		布局索引
//		IN	IsLinkmode:	展开态的储存模式
// 返回:
//		Layout*: 展开出的布局对象; 失败返回 NULL
Layout* Application::BuildLayoutFromState(CONST FileMidState& state, int index, BOOL IsLinkmode)
{
	if (index < 0)
	{
		return NULL;
	}
	return LayoutBuilder::BuildLayoutFromNode(LayoutModel::LayoutAt(state, (DWORD)index), IsLinkmode);
}

// ================================
// ================================

// ================================
// ================================


// 函数: FormatToUI
// 描述: 把布局模型中指定索引的布局展开为 UI 布局对象(实现落在应用侧)
// 参数:
//		IN	state:		布局模型中间态
//		IN	index:		布局索引
//		IN	IsLinkmode:	展开态的储存模式
// 返回:
//		Layout*: 展开出的布局对象; 失败返回 NULL
Layout* FormatToUI(FileMidState state, int index, BOOL IsLinkmode)
{
	return Application::BuildLayoutFromState(state, index, IsLinkmode);
}
