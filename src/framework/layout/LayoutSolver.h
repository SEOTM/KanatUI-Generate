#pragma once
#include "framework/layout/LayoutModel.h"
#include "framework/base/BasicDesign.h"
#include "framework/base/mem/MemoryManager.h"

// ================================
// 区域: bin 格式契约的来源
// 说明: ① bin 文件格式的字节布局常量(BIN_* / BinSegBytes)、版本号位域与规模上限
//          (LAYOUT_MAX / CTRL_MAX)声明在 src\framework\layout\LayoutModel.h 的"bin 文件格式契约"区
//          (与中间态结构体布局同源); 校验算法取 src\framework\base\util\FileCodec.h 的 CRC32;
//       ② 本模块只负责读方向(bin / JSON 文档 → 中间态) ⇒ 不依赖写方向模块 LayoutWriter
//          (只装载读方向时无须链接写方向)。
// ================================

// ================================
// 库名: LayoutSolver
// 描述: 布局求解器 —— 把 json / bin 文档解析为中间态(布局数据模型 FileMidState), 并按参考尺寸解算几何
// ================================
class LayoutSolver
{
private:
	// 成员变量
	DWORD dwLayouts;		// 布局数量(与中间态的布局链一致)
	FileMidState state;		// 中间态(解析产物: 布局链 + 元素规格池 + 元素链)
	Arena* memArena;		// 承载中间态各池的线性分配器
public:
	// 函数: LayoutSolver
	// 描述: 构造文档读入器(记录分配器, 计数清零, 中间态置空)
	LayoutSolver(Arena* arena);
	// 函数: ~LayoutSolver
	// 描述: 析构文档读入器(不释放分配器内存, 也不回收中间态块)
	~LayoutSolver();
	// 函数: LoadLayouts
	// 描述: 从文件加载布局信息(内存映射后转交内存入口)
	BOOL LoadLayouts(LPCSTR filename);
	// 函数: LoadLayouts
	// 描述: 从内存缓冲解析 JSON, 产物 = 中间态
	BOOL LoadLayouts(LPVOID data, DWORD size);
	// 函数: GetLayoutsCount
	// 描述: 获取布局数量
	DWORD GetLayoutsCount();
	// 函数: GetLayout
	// 描述: 获取指定索引布局的布局信息
	CONST LayoutInfo* GetLayout(DWORD index);
	// 函数: GetState
	// 描述: 取整份中间态(布局链 + 元素池)
	CONST FileMidState& GetState();

	// -------- bin 读方向(格式契约见 LayoutModel.h 的格式契约区; 校验算法见 FileCodec.h; 与 JSON 读同文件, 不另立模块) --------
	// 函数: LoadLayoutsBin
	// 描述: 从文件按 bin 格式加载布局信息(整体读入后转交内存入口)
	BOOL LoadLayoutsBin(LPCSTR filename);
	// 函数: LoadLayoutsBin
	// 描述: 从内存缓冲按 bin 格式解析, 产物 = 中间态
	BOOL LoadLayoutsBin(LPCVOID data, DWORD size);
	// 函数: IsBinDocument
	// 描述: 嗅探缓冲的文档形态(TRUE = bin, FALSE = JSON)
	static BOOL IsBinDocument(LPCVOID data, DWORD size);

	// -------- 形态自动分派(嗅探后择读入口; 应用侧的「打开」与启动加载链统一走此处) --------
	// 函数: LoadLayoutsAuto
	// 描述: 嗅探文件形态后分派到 JSON / bin 读入口
	BOOL LoadLayoutsAuto(LPCSTR filename);
	// 函数: LoadLayoutsAuto
	// 描述: 嗅探内存缓冲形态后分派到 JSON / bin 读入口
	BOOL LoadLayoutsAuto(LPCVOID data, DWORD size);
};
