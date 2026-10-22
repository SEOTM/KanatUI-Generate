// ================================
// 文件: DocumentSource.h
// 描述: 启动加载链(四级顺序探测)与内置文档资源取件 —— 应用启动策略的落点。
// ================================
#pragma once
#include <windows.h>

// ================================
// 结构: DocSource
// 描述: 一次文档源探测的结果形状(自 Application 的公开嵌套类型迁出为本文件级类型)。
// 说明: 唯一消费点 = Application::Init 的"启动加载链"段(ResolveDocumentSource 填、Init 读)。
// ================================
struct DocSource
{
	INT nLevel;			// 命中级(1..4); 0 = 四级均未命中
	BOOL bBin;			// 该级目标是否为 bin 形态(第 1/3 级)
	BOOL bEmbedded;		// 是否来自随 exe 内置的资源(第 3/4 级); FALSE = exe 目录下的外部文件
	LPCSTR lpDesc;		// 来源描述(诊断文案)
	LPCSTR lpPath;		// 外部文件路径(第 1/2 级命中时有效; 内存池字符串)
	LPVOID pData;		// 内置资源数据(第 3/4 级命中时有效; 内存池副本, 末尾已补 '\0')
	DWORD cbData;		// 内置资源数据字节数(不含补的 '\0')
};

// ================================
// 命名空间: DocumentSource
// 描述: 启动加载链的四级顺序探测与内置资源取件(启动期一次; 无状态)
// 说明: 顺序 = 第 1 级 .\GenUI.bin → 第 2 级 .\GenUI.json → 第 3 级 resource.res\GenUI.bin
//       → 第 4 级 resource.res\GenUI.json; 命中即用(命中后不再探测后续级别)。
//       第 1/2 级 = exe 目录下的外部文件; 第 3/4 级 = 随 exe 内置的资源(见 src\app\resource.h);
//       第 1/3 级为 bin 形态、第 2/4 级为 JSON 形态, 实际解析按文件内容嗅探分派
//       (见 LayoutSolver::IsBinDocument / LoadLayoutsAuto)⇒ 级序才是口径, 形态由内容决定。
// ================================
namespace DocumentSource
{
	// 函数: ResolveDocumentSource
	// 描述: 按四级顺序探测可用文档源(命中即用), 逐级打印诊断行; 四级均不可用返回 FALSE
	// 参数: dwLevelMask = 允许尝试的级别掩码(bit0..bit3 = 第 1..4 级; 默认 0xF = 全部)
	BOOL ResolveDocumentSource(DocSource* pOut, DWORD dwLevelMask = 0xFu);
	// 函数: GetEmbeddedDoc
	// 描述: 取随 exe 内置的文档资源(资源节的 JSON 文档), 拷贝进内存池并补 '\0'(JSON 解析需 NUL 结尾)
	// 返回: TRUE 命中(ppData/pcbData 有效; pcbData 不含补的 '\0'); FALSE 未内嵌/不可用
	// 说明: 唯一消费点 = 本模块的 ResolveDocumentSource 第 4 级(不对外)。
	BOOL GetEmbeddedDoc(LPVOID* ppData, DWORD* pcbData);
	// 函数: GetEmbeddedDocBin
	// 描述: 取随 exe 内置的 bin 文档资源(加载链第 3 级 resource.res\GenUI.bin; 同样补 '\0' 以便统一持有)
	// 返回: TRUE 命中(ppData/pcbData 有效; pcbData 不含补的 '\0'); FALSE 未内嵌/不可用
	// 说明: 唯一消费点 = 本模块的 ResolveDocumentSource 第 3 级(不对外)。
	BOOL GetEmbeddedDocBin(LPVOID* ppData, DWORD* pcbData);
}
