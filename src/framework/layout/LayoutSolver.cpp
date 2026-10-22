#include "framework/layout/LayoutSolver.h"
#include "common/FileCodec.h"
#include <Windows.h>
#include <stdio.h>
#include <new>
// 使用 RapidJSON 库解析 JSON 数据
#include "rapidjson/document.h"

// ========================
// 区域: 辅助函数声明区
// ========================

// 函数: SolverFactor
// 描述: 解析 JSON 的尺寸因子(coeff[8] + offset)到 SizeFactor
BOOL SolverFactor(SizeFactor& dest, rapidjson::Value& src, LPCSTR szWhere);
// 函数: SolverRefSize
// 描述: 解析布局级 "autosize"(参考尺寸/经验参数)到 RefSize
VOID SolverRefSize(RefSize& dest, rapidjson::Value& src);
// 函数: SolverStr
// 描述: 把 JSON 字符串拷入中间态的定长字段(超长截断并打印诊断)
VOID SolverStr(char* dst, size_t cap, LPCSTR src, LPCSTR szWhere, LPCSTR szField);

// ========================
// 区域: bin 读方向(格式契约见 src\framework\layout\LayoutModel.h 的格式契约区; 校验算法见 src\common\FileCodec.h;
//       记录区整块拷贝, 不做逐字段解析)
// ========================

// ================================
// 库名: LayoutSolver
// 描述: 布局求解器 —— 把 json 文档解析为中间态(布局数据模型), 并按参考尺寸解算几何
// ================================

// 函数: LayoutSolver
// 描述: 构造文档读入器(记录分配器, 计数清零, 中间态置空)
LayoutSolver::LayoutSolver(Arena* arena)
{
	this->memArena = arena;
	this->dwLayouts = 0;
	this->state = FileMidState{};
}

// 函数: ~LayoutSolver
// 描述: 析构文档读入器(不释放分配器内存, 也不回收中间态块)
LayoutSolver::~LayoutSolver()
{
	memArena = nullptr;				// 不负责释放 memArena 的内存，由外部管理
	this->state = FileMidState{};	// 中间态各池由外部(Arena)管理
}
// 函数: LoadLayouts
// 描述: 从文件加载布局信息(内存映射后转交内存入口)
BOOL LayoutSolver::LoadLayouts(LPCSTR filename)
{
	// 读取文件后，通过指定内存解析的方式进行加载布局
	// 通过内存映射的方式打开文件

	BOOL bRet = FALSE;
	SIZE_T size = 0;
	HANDLE hMap = NULL;
	LPVOID data = NULL;
	// 创建文件 / 打开文件
	HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD err = GetLastError();
	if (hFile == INVALID_HANDLE_VALUE)
	{
		goto END;
	}
	// 判断文件是否为空
	size = GetFileSize(hFile, NULL);
	if (size == 0)
	{
		goto END;
	}
	// 创建文件映射
	hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, size, NULL);
	if (hMap == NULL)
	{
		goto END;
	}
	// 映射文件到内存
	data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, size);
	if (data == NULL)
	{
		goto CLOSEMAP;
	}
	// 加载布局信息
	bRet = LoadLayouts(data, size);
	// 标记
UNMAP:
	if (data != NULL)
	{
		UnmapViewOfFile(data);
	}
CLOSEMAP:
	if (hMap != NULL)
	{
		CloseHandle(hMap);
	}
CLOSEFILE:
	CloseHandle(hFile);
END:
	return bRet;
}
// 函数: LoadLayouts
// 描述: 从内存缓冲解析 JSON, 产物 = 中间态
BOOL LayoutSolver::LoadLayouts(LPVOID data, DWORD size)
{
	// 使用 RapidJSON 库解析 JSON 数据	
	BOOL bRet = FALSE;
	// 入口守卫:
	//   ① data 为 NULL ⇒ doc.Parse((char*)data) 会解引用空指针;
	//   ② memArena 未注入(构造允许 NULL)⇒ 下方第一趟的 memArena->Allocate 会空指针解引用。
	//   二者一律按"加载失败"返回 FALSE —— 与 bin 路径 LoadLayoutsBin 的入口守卫(pData == NULL || memArena == NULL)同口径。
	if (data == NULL || memArena == NULL)
	{
		return FALSE;
	}
	// 解析 JSON 数据
	rapidjson::Document doc;
	// 读取数据
	doc.Parse((char*)data);
	if (doc.HasParseError())
	{
		printf("[JSON] 解析错误(偏移 %u, 代码 %u): 拒绝加载\n",
			(unsigned)doc.GetErrorOffset(), (unsigned)doc.GetParseError());
		goto END;
	}

	// 开始解析
	if (!doc.HasMember("layouts") || !doc["layouts"].IsArray())
	{
		printf("[JSON] 缺少 \"layouts\" 数组: 拒绝加载\n");
		goto END;
	}
	{
		// 布局数量(必需): 0 或超上限即拒绝
		const rapidjson::SizeType nLayouts = doc["layouts"].Size();
		if (nLayouts == 0 || nLayouts > LAYOUT_MAX)
		{
			printf("[JSON] \"layouts\" 数量非法(%u, 允许 1..%u): 拒绝加载\n",
				(unsigned)nLayouts, (unsigned)LAYOUT_MAX);
			goto END;
		}
		this->dwLayouts = (DWORD)nLayouts;
		// 第一趟: 校验布局结构 + 数出各布局元素数
		DWORD* Counts = NULL;
		{
			BufferInfo cbuf = memArena->Allocate(sizeof(DWORD) * this->dwLayouts);
			if (cbuf.buffer == NULL)
			{
				goto END;
			}
			Counts = (DWORD*)cbuf.buffer;
		}
		size_t qwTotalElements = 0;
		for (DWORD i = 0; i < this->dwLayouts; i++)
		{
			rapidjson::Value& layout = doc["layouts"][i];
			if (!layout.IsObject())
			{
				printf("[JSON] layouts[%u] 不是对象: 拒绝加载\n", (unsigned)i);
				goto END;
			}
			// 控件数组(必需): 缺失/非数组/超上限即拒绝
			if (!layout.HasMember("ctrls") || !layout["ctrls"].IsArray())
			{
				printf("[JSON] layouts[%u] 缺少 \"ctrls\" 数组: 拒绝加载\n", (unsigned)i);
				goto END;
			}
			const rapidjson::SizeType nCtrls = layout["ctrls"].Size();
			if (nCtrls > CTRL_MAX)
			{
				printf("[JSON] layouts[%u] 控件数非法(%u, 上限 %u): 拒绝加载\n",
					(unsigned)i, (unsigned)nCtrls, (unsigned)CTRL_MAX);
				goto END;
			}
			Counts[i] = (DWORD)nCtrls;
			qwTotalElements += (size_t)nCtrls;
		}
		// 总量溢出回查
		if (qwTotalElements > (0xFFFFFFFFull / sizeof(ElementSpec)))
		{
			printf("[JSON] 控件总数过大导致尺寸溢出: 拒绝加载\n");
			goto END;
		}
		// 初始化中间态(逐块建立布局链与元素链)
		LayoutModel::StateClear(this->state);
		if (!LayoutModel::StateInit(this->state, this->dwLayouts, Counts))
		{
			printf("[JSON] 中间态块分配失败: 拒绝加载\n");
			goto END;
		}
		// 第二趟: 逐布局、逐元素填充中间态
		for (DWORD i = 0; i < this->dwLayouts; i++)
		{
			rapidjson::Value& layout = doc["layouts"][i];
			LayoutNode* pNode = LayoutModel::LayoutAt(this->state, i);
			if ((pNode == NULL) || (pNode->info == NULL))
			{
				goto END;
			}
			LayoutInfo* info = pNode->info;
			CONST DWORD dwControls = info->dwElements;
			// --- 布局级字段(逐字段守卫, 可选字段缺失按缺省值) ---
			{
				CHAR szLay[64]{};
				LPCSTR lpName = (layout.HasMember("name") && layout["name"].IsString())
					? layout["name"].GetString() : "";
				LPCSTR lpLabel = (layout.HasMember("label") && layout["label"].IsString())
					? layout["label"].GetString() : "";
				info->IsMaxiable = (layout.HasMember("IsMaxiable") && layout["IsMaxiable"].IsBool())
					? layout["IsMaxiable"].GetBool() : FALSE;
				info->IsResizeable = (layout.HasMember("IsResizeable") && layout["IsResizeable"].IsBool())
					? layout["IsResizeable"].GetBool() : FALSE;
				// 读取布局级 autosize(各布局独立经验参数)
				SolverRefSize(info->refSize, layout);
				// 窗口桌面位置(缺省 0)
				info->wndX = 0;
				info->wndY = 0;
				if (layout.HasMember("wndX") && layout["wndX"].IsInt()) { info->wndX = layout["wndX"].GetInt(); }
				if (layout.HasMember("wndY") && layout["wndY"].IsInt()) { info->wndY = layout["wndY"].GetInt(); }
				// 名称/标签: 定长字段
				SolverStr(info->name, sizeof(info->name), lpName, "layouts[] 布局 name", "name");
				SolverStr(info->label, sizeof(info->label), lpLabel, "layouts[] 布局 label", "label");
				snprintf(szLay, sizeof(szLay), "layouts[%u] name=%s", (unsigned)i, info->name);
				// 读取布局尺寸因子(必需: 缺失/不合法即拒绝整份加载)
				if (!layout.HasMember("width") || !layout.HasMember("height"))
				{
					printf("[JSON] %s 缺少 width/height 因子: 拒绝加载\n", szLay);
					goto END;
				}
				if (!SolverFactor(info->width, layout["width"], szLay) ||
					!SolverFactor(info->height, layout["height"], szLay))
				{
					goto END;
				}
			}
			// --- 控件级字段 ---
			// 元素规格按元素链顺序逐枚取用
			ElementNode* pElemNode = LayoutModel::ElementFirst(pNode);
			for (DWORD j = 0; j < dwControls; j++)
			{
				rapidjson::Value& ctrl = layout["ctrls"][j];
				if ((pElemNode == NULL) || (pElemNode->data == NULL))
				{
					printf("[JSON] layouts[%u].ctrls[%u] 元素链长度不足: 拒绝加载\n", (unsigned)i, (unsigned)j);
					goto END;
				}
				ElementSpec& es = *(pElemNode->data);
				pElemNode = LayoutModel::ElementNext(pElemNode);
				if (!ctrl.IsObject())
				{
					printf("[JSON] layouts[%u].ctrls[%u] 不是对象: 拒绝加载\n", (unsigned)i, (unsigned)j);
					goto END;
				}
				// 类型为必需(缺失即无法构建控件)
				if (!ctrl.HasMember("type") || !ctrl["type"].IsString())
				{
					printf("[JSON] layouts[%u].ctrls[%u] 缺少字符串 \"type\": 拒绝加载\n",
						(unsigned)i, (unsigned)j);
					goto END;
				}
				es.id = (ctrl.HasMember("id") && ctrl["id"].IsInt())
					? ctrl["id"].GetInt() : (int)j;
				if (!(ctrl.HasMember("id") && ctrl["id"].IsInt()))
				{
					printf("[JSON] layouts[%u].ctrls[%u] 缺少整数 \"id\": 按数组下标 %u 兜底\n",
						(unsigned)i, (unsigned)j, (unsigned)j);
				}
				// 父容器 ID(可选): 缺省 -1 = 无父容器(0 是合法 ID ⇒ 缺省不能取 0);
				// 只声明不校验: 指向不存在的 ID 由装配期的事件委托解析静默降级(该控件无响应)
				es.parentID = (ctrl.HasMember("parent") && ctrl["parent"].IsInt())
					? ctrl["parent"].GetInt() : -1;
				if (es.parentID < -1)
				{
					es.parentID = -1;			// 负数一律归"无父容器"
				}
				// 字符串: 定长字段(超长截断并诊断)
				{
					CHAR szWhere[128]{};
					snprintf(szWhere, sizeof(szWhere), "layouts[%u].ctrls[%u]", (unsigned)i, (unsigned)j);
					SolverStr(es.type, sizeof(es.type), ctrl["type"].GetString(), szWhere, "type");
					SolverStr(es.label, sizeof(es.label),
						(ctrl.HasMember("label") && ctrl["label"].IsString()) ? ctrl["label"].GetString() : "",
						szWhere, "label");
					SolverStr(es.text, sizeof(es.text),
						(ctrl.HasMember("text") && ctrl["text"].IsString()) ? ctrl["text"].GetString() : "",
						szWhere, "text");
					SolverStr(es.event, sizeof(es.event),
						(ctrl.HasMember("event") && ctrl["event"].IsString()) ? ctrl["event"].GetString() : "",
						szWhere, "event");
					// 读取尺寸(必需: x/y/width/height)
					{
						CHAR szFac[160]{};
						snprintf(szFac, sizeof(szFac), "%s type=%s label=%s",
							szWhere, es.type, es.label);
						if (!ctrl.HasMember("x") || !ctrl.HasMember("y") ||
							!ctrl.HasMember("width") || !ctrl.HasMember("height"))
						{
							printf("[JSON] %s 缺少几何因子(x/y/width/height): 拒绝加载\n", szFac);
							goto END;
						}
						if (!SolverFactor(es.x, ctrl["x"], szFac) ||
							!SolverFactor(es.y, ctrl["y"], szFac) ||
							!SolverFactor(es.width, ctrl["width"], szFac) ||
							!SolverFactor(es.height, ctrl["height"], szFac))
						{
							goto END;
						}
					}
				}
			}
		}
	}
	// doc 在函数结束时自动销毁, 不需手动释放内存
	bRet = TRUE;
END:
	// 失败诊断立即落盘
	if (bRet == FALSE)
	{
		fflush(stdout);
	}
	return bRet;
}
// 函数: GetLayoutsCount
// 描述: 获取布局数量
DWORD LayoutSolver::GetLayoutsCount()
{
	return this->dwLayouts;
}
// 函数: GetLayout
// 描述: 获取指定索引布局的布局信息
CONST LayoutInfo* LayoutSolver::GetLayout(DWORD index)
{
	LayoutNode* pNode = LayoutModel::LayoutAt(this->state, index);
	return (pNode != NULL) ? pNode->info : NULL;
}
// 函数: GetState
// 描述: 取整份中间态(布局链 + 元素池)
CONST FileMidState& LayoutSolver::GetState()
{
	return this->state;
}

// ================================
// 区域: bin 读方向实现(格式 v1; 契约常量 / 段尺寸公式 / 版本号位域见 src\framework\layout\LayoutModel.h,
//       校验算法见 src\common\FileCodec.h)
// 说明: 与 JSON 读方向同构(两趟: 先校验并数数, 再建中间态并逐项填充), 产物与失败语义完全一致;
//       bin 的写方向归 LayoutWriter(与 JSON 写同模块), 本模块只读不写 ——
//       两侧共用 LayoutModel 的格式契约与 FileCodec 的校验算法, 彼此无依赖。
// ================================

// 常量: BIN_SNIFF_BYTES
// 描述: 形态嗅探读取的文件头字节数(足够覆盖可选 UTF-8 BOM 与前导空白, 供 IsBinDocument 判定)
constexpr DWORD BIN_SNIFF_BYTES = 16;

// 函数: LoadLayoutsBin
// 描述: 从文件按 bin 格式加载布局信息(整体读入后转交内存入口)
BOOL LayoutSolver::LoadLayoutsBin(LPCSTR filename)
{
	BOOL bRet = FALSE;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD cbSize = 0;
	DWORD dwRead = 0;
	LPBYTE lpBuf = NULL;
	if (filename == NULL || filename[0] == '\0')
	{
		return bRet;
	}
	// 1. 打开文件并取大小(bin 按显式长度解析, 不依赖 '\0' 结尾)
	hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("[BIN] 打开文件失败(%s; GetLastError=%u): 拒绝加载\n", filename, (unsigned)GetLastError());
		goto END;
	}
	cbSize = GetFileSize(hFile, NULL);
	if ((cbSize == 0) || (cbSize == INVALID_FILE_SIZE))
	{
		printf("[BIN] 文件大小非法(%s; %u 字节): 拒绝加载\n", filename, (unsigned)cbSize);
		goto END;
	}
	// 2. 整体读入堆缓冲
	lpBuf = new (std::nothrow) BYTE[cbSize];
	if (lpBuf == NULL)
	{
		goto END;
	}
	if (!ReadFile(hFile, lpBuf, cbSize, &dwRead, NULL) || (dwRead != cbSize))
	{
		printf("[BIN] 读取文件未完成(%s; %u / %u 字节): 拒绝加载\n", filename, (unsigned)dwRead, (unsigned)cbSize);
		goto END;
	}
	// 3. 走内存重载解析
	bRet = LoadLayoutsBin((LPCVOID)lpBuf, cbSize);
END:
	if (lpBuf != NULL)
	{
		delete[] lpBuf;
	}
	if (hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile);
	}
	return bRet;
}

// 函数: LoadLayoutsBin
// 描述: 从内存缓冲按 bin 格式解析, 产物 = 中间态
BOOL LayoutSolver::LoadLayoutsBin(LPCVOID data, DWORD size)
{
	BOOL bRet = FALSE;
	// 变量声明区, 避免 goto 跳过变量声明
	CONST BYTE* pData = (CONST BYTE*)data;
	DWORD* Counts = NULL;
	DWORD dwSegs = 0;
	DWORD dwVer = 0;
	DWORD dwStored = 0;
	DWORD dwCalc = 0;
	size_t cbOff = 0;
	if (pData == NULL || memArena == NULL)
	{
		goto END;		// 入参非法: 静默返回(与 JSON 读方向同口径, 不伪造"文件过短")
	}
	if (size < (DWORD)BIN_FILE_HEAD)
	{
		printf("[BIN] 文件过短(%u 字节, 至少 %u): 拒绝加载\n", (unsigned)size, (unsigned)BIN_FILE_HEAD);
		goto END;
	}
	// 1. 文件头三项: 校验码 / 文件版本号 / 段数量
	memcpy(&dwStored, pData + 0, 4);
	memcpy(&dwVer, pData + 4, 4);
	memcpy(&dwSegs, pData + 8, 4);
	// 校验码 = 对所有字节算 CRC32, 其中校验码字段自身按 0 参与
	// (流式: 先推进 4 个零字节, 再推进其余正文; 不修改调用方缓冲)
	{
		CONST BYTE Zeros[BIN_CRC_SIZE] = {};
		DWORD dwState = FileCodec::Crc32Update(FileCodec::Crc32Begin(), (LPCVOID)Zeros, (size_t)BIN_CRC_SIZE);
		dwState = FileCodec::Crc32Update(dwState, (LPCVOID)(pData + BIN_CRC_SIZE),
			(size_t)size - (size_t)BIN_CRC_SIZE);
		dwCalc = FileCodec::Crc32Finish(dwState);
	}
	if (dwStored != dwCalc)
	{
		printf("[BIN] 校验码不符(文件 %08X, 计算 %08X): 拒绝加载\n", (unsigned)dwStored, (unsigned)dwCalc);
		goto END;
	}
	// 版本号: 高 24 位代际版本号(须与本格式一致) + 低 8 位格式标记(压缩 / 加密尚未实现 ⇒ 须为 0)
	if ((dwVer & BIN_VER_GEN_MASK) != (BIN_VERSION & BIN_VER_GEN_MASK))
	{
		printf("[BIN] 文件代际版本号不支持(%u, 需 %u): 拒绝加载\n",
			(unsigned)((dwVer & BIN_VER_GEN_MASK) >> 8), (unsigned)((BIN_VERSION & BIN_VER_GEN_MASK) >> 8));
		goto END;
	}
	{
		DWORD dwFlags = dwVer & BIN_VER_FLAG_MASK;
		if (dwFlags != 0)
		{
			LPCSTR lpShape = "含未定义位的标记";
			if (dwFlags == BIN_VER_FLAG_COMPRESS)
			{
				lpShape = "压缩形态";
			}
			else if (dwFlags == BIN_VER_FLAG_ENCRYPT)
			{
				lpShape = "加密形态";
			}
			else if (dwFlags == (BIN_VER_FLAG_COMPRESS | BIN_VER_FLAG_ENCRYPT))
			{
				lpShape = "加密且压缩形态";
			}
			printf("[BIN] 文件格式标记不支持(0x%02X = %s, 尚未实现): 拒绝加载\n",
				(unsigned)dwFlags, lpShape);
			goto END;
		}
	}
	if (dwSegs == 0 || dwSegs > (DWORD)LAYOUT_MAX)
	{
		printf("[BIN] 段数量非法(%u, 允许 1..%u): 拒绝加载\n", (unsigned)dwSegs, (unsigned)LAYOUT_MAX);
		goto END;
	}
	this->dwLayouts = dwSegs;
	// 2. 第一趟: 逐段校验结构并数出控件数
	{
		BufferInfo cbuf = memArena->Allocate(sizeof(DWORD) * dwSegs);
		if (cbuf.buffer == NULL)
		{
			goto END;
		}
		Counts = (DWORD*)cbuf.buffer;
	}
	cbOff = (size_t)BIN_FILE_HEAD;
	for (DWORD i = 0; i < dwSegs; i++)
	{
		DWORD cbSeg = 0;
		DWORD dwHeadCtrls = 0;
		DWORD dwUse = 0;
		size_t cbRem = 0;
		size_t nSegCtrls = 0;
		if (cbOff + (size_t)BIN_SEG_HEAD > (size_t)size)
		{
			printf("[BIN] 段[%u] 段头越界(偏移 %u, 文件 %u 字节): 拒绝加载\n",
				(unsigned)i, (unsigned)cbOff, (unsigned)size);
			goto END;
		}
		memcpy(&cbSeg, pData + cbOff + 0, 4);
		// 本段大小: 须容得下段头 + 头部区, 且余量恰为控件记录的整数倍(余量不整除 = 非本格式)
		if ((size_t)cbSeg < (size_t)(BIN_SEG_HEAD + BIN_LAYOUT_SIZE))
		{
			printf("[BIN] 段[%u] 本段大小过小(声明 %u, 至少 %u): 拒绝加载\n",
				(unsigned)i, (unsigned)cbSeg, (unsigned)(BIN_SEG_HEAD + BIN_LAYOUT_SIZE));
			goto END;
		}
		if (cbOff + (size_t)cbSeg > (size_t)size)
		{
			printf("[BIN] 段[%u] 本段越界(偏移 %u, 声明 %u, 文件 %u 字节): 拒绝加载\n",
				(unsigned)i, (unsigned)cbOff, (unsigned)cbSeg, (unsigned)size);
			goto END;
		}
		cbRem = (size_t)cbSeg - (size_t)(BIN_SEG_HEAD + BIN_LAYOUT_SIZE);
		if ((cbRem % (size_t)BIN_CTRL_SIZE) != 0)
		{
			printf("[BIN] 段[%u] 本段大小与控件记录不符(余量 %u 非 %u 的整数倍): 拒绝加载\n",
				(unsigned)i, (unsigned)cbRem, (unsigned)BIN_CTRL_SIZE);
			goto END;
		}
		nSegCtrls = cbRem / (size_t)BIN_CTRL_SIZE;
		// 头部区尾部"控件数量": 与段大小反算值不一致时按较小者取用(诊断点名, 不拒绝)
		memcpy(&dwHeadCtrls, pData + cbOff + BIN_SEG_HEAD + BIN_LAYOUT_NELEM, 4);
		if (dwHeadCtrls > (DWORD)CTRL_MAX)
		{
			printf("[BIN] 段[%u] 控件数超上限(头部声明 %u, 上限 %u): 拒绝加载\n",
				(unsigned)i, (unsigned)dwHeadCtrls, (unsigned)CTRL_MAX);
			goto END;
		}
		dwUse = (dwHeadCtrls < (DWORD)nSegCtrls) ? dwHeadCtrls : (DWORD)nSegCtrls;
		if (dwHeadCtrls != (DWORD)nSegCtrls)
		{
			printf("[BIN] 段[%u] 控件数量与段大小不符(头部声明 %u, 段内容纳 %u) ⇒ 按较小值 %u 解析\n",
				(unsigned)i, (unsigned)dwHeadCtrls, (unsigned)nSegCtrls, (unsigned)dwUse);
		}
		Counts[i] = dwUse;
		cbOff += (size_t)cbSeg;
	}
	// 段区总长必须正好铺满文件(多余/缺失字节一律拒绝)
	if (cbOff != (size_t)size)
	{
		printf("[BIN] 段区总长与文件长度不符(%u / %u 字节): 拒绝加载\n", (unsigned)cbOff, (unsigned)size);
		goto END;
	}
	// 3. 初始化中间态(逐块建立布局链与元素链)
	LayoutModel::StateClear(this->state);
	if (!LayoutModel::StateInit(this->state, dwSegs, Counts))
	{
		printf("[BIN] 中间态块分配失败: 拒绝加载\n");
		goto END;
	}
	// 4. 第二趟: 整块拷贝记录区(头部区 = 一枚 LayoutInfo; 控件区 = 若干枚 ElementSpec)
	cbOff = (size_t)BIN_FILE_HEAD;
	for (DWORD i = 0; i < dwSegs; i++)
	{
		CONST BYTE* pSeg = pData + cbOff;
		LayoutNode* pNode = LayoutModel::LayoutAt(this->state, i);
		LayoutInfo* info = NULL;
		CONST BYTE* pCtrl = NULL;
		if ((pNode == NULL) || (pNode->info == NULL))
		{
			goto END;
		}
		info = pNode->info;
		// 头部区: 整块拷贝一枚 LayoutInfo(其尾部 dwElements 即文件格式的"控件数量")
		memcpy(info, pSeg + BIN_SEG_HEAD, sizeof(LayoutInfo));
		// 控件数量以实际加载枚数为准(头部声明与段大小不一致时, 第一趟已按较小值取用)
		info->dwElements = Counts[i];
		// 定长串兜底: 末字节置 0(防御外部构造的缺结尾符字段导致后续越界读)
		info->name[STR_NAME - 1] = '\0';
		info->label[STR_LABEL - 1] = '\0';
		info->refSize.mode[STR_MODE - 1] = '\0';
		// 控件记录区: 逐枚整块拷贝一枚 ElementSpec(顺序 = 元素链顺序)
		pCtrl = pSeg + BIN_SEG_HEAD + BIN_LAYOUT_SIZE;
		for (ElementNode* pElem = LayoutModel::ElementFirst(pNode);
			pElem != NULL; pElem = LayoutModel::ElementNext(pElem))
		{
			ElementSpec* es = pElem->data;
			if (es == NULL)
			{
				goto END;
			}
			memcpy(es, pCtrl, sizeof(ElementSpec));
			es->label[STR_LABEL - 1] = '\0';
			es->text[STR_TEXT - 1] = '\0';
			es->type[STR_TYPE - 1] = '\0';
			es->event[STR_EVENT - 1] = '\0';
			if (es->parentID < -1)
			{
				es->parentID = -1;			// 负数一律归"无父容器"
			}
			pCtrl += BIN_CTRL_SIZE;
		}
		cbOff += BinSegBytes(Counts[i]);
	}
	bRet = TRUE;
END:
	if (!bRet)
	{
		fflush(stdout);
	}
	return bRet;
}

// 函数: IsBinDocument
// 描述: 嗅探缓冲的文档形态(TRUE = bin, FALSE = JSON)
BOOL LayoutSolver::IsBinDocument(LPCVOID data, DWORD size)
{
	CONST BYTE* pData = (CONST BYTE*)data;
	size_t i = 0;
	if (pData == NULL || size == 0)
	{
		return FALSE;
	}
	// 可选 UTF-8 BOM
	if ((size >= 3) && (pData[0] == 0xEF) && (pData[1] == 0xBB) && (pData[2] == 0xBF))
	{
		i = 3;
	}
	// 前导空白
	while ((i < (size_t)size) &&
		(pData[i] == ' ' || pData[i] == '\t' || pData[i] == '\r' || pData[i] == '\n'))
	{
		i++;
	}
	if (i >= (size_t)size)
	{
		return FALSE;
	}
	return (pData[i] == '{') ? FALSE : TRUE;
}

// 函数: LoadLayoutsAuto
// 描述: 嗅探文件形态后分派到 JSON / bin 读入口
BOOL LayoutSolver::LoadLayoutsAuto(LPCSTR filename)
{
	BOOL bRet = FALSE;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD dwRead = 0;
	BYTE Heads[BIN_SNIFF_BYTES]{};
	if (filename == NULL || filename[0] == '\0')
	{
		return bRet;
	}
	// 1. 只读文件头若干字节做形态嗅探
	//    (打不开/读不到 ⇒ 按 JSON 形态交给既有入口, 由它维持原有的失败与诊断口径)
	hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		if (!ReadFile(hFile, Heads, (DWORD)sizeof(Heads), &dwRead, NULL))
		{
			dwRead = 0;
		}
		CloseHandle(hFile);
	}
	// 2. 分派: bin 形态走 bin 入口(显式长度, 不补 '\0'); 其余走 JSON 入口
	if ((dwRead > 0) && IsBinDocument((LPCVOID)Heads, dwRead))
	{
		bRet = LoadLayoutsBin(filename);
	}
	else
	{
		bRet = LoadLayouts(filename);
	}
	return bRet;
}

// 函数: LoadLayoutsAuto
// 描述: 嗅探内存缓冲形态后分派到 JSON / bin 读入口
BOOL LayoutSolver::LoadLayoutsAuto(LPCVOID data, DWORD size)
{
	if ((data != NULL) && (size > 0) && IsBinDocument(data, size))
	{
		return LoadLayoutsBin(data, size);
	}
	return LoadLayouts((LPVOID)data, size);
}

// 函数: SolverRefSize
// 描述: 解析布局级 autosize(参考尺寸与经验参数)到 RefSize
VOID SolverRefSize(RefSize& dest, rapidjson::Value& src)
{
	// 1. 默认值: 屏高模式, 千分比
	dest = LayoutModel::DefaultRefSize();
	// 2. 无 autosize 段则直接返回默认
	if (!src.HasMember("autosize") || !src["autosize"].IsObject())
	{
		return;
	}
	rapidjson::Value& a = src["autosize"];
	// 3. mode: 支持字符串名或整数索引(字符串未命中/整数越界 ⇒ 回落到 0)
	{
		static const LPCSTR Modes[4] = {
			"AUTO_SIZE_BY_SCREEN_WIDTH",	// 0: 屏宽
			"AUTO_SIZE_BY_SCREEN_HEIGHT",	// 1: 屏高
			"AUTO_SIZE_BY_WINDOW_WIDTH",	// 2: 窗宽
			"AUTO_SIZE_BY_WINDOW_HEIGHT"	// 3: 窗高
		};
		INT nMode = AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT;		// 未命中 ⇒ 默认(屏高)
		if (a.HasMember("mode"))
		{
			if (a["mode"].IsString())
			{
				LPCSTR szMode = a["mode"].GetString();
				for (INT m = 0; m < 4; m++)
				{
					if (strcmp(szMode, Modes[m]) == 0)
					{
						nMode = m;
						break;
					}
				}
			}
			else if (a["mode"].IsInt())
			{
				nMode = a["mode"].GetInt();
			}
		}
		if (nMode < 0 || nMode > 3) { nMode = 0; }
		LayoutModel::StrCopy(dest.mode, sizeof(dest.mode), Modes[nMode]);
	}
	// 4. unit
	if (a.HasMember("unit") && a["unit"].IsInt())
	{
		INT unit = a["unit"].GetInt();
		if (unit >= 0 && unit <= 1) { dest.unit = unit; }
	}
	// 5. 六个千分比字段(0..10000)
	static const LPCSTR Flds[6] = {
		"width", "height", "controlHeight", "fontSize", "space", "padding"
	};
	for (int i = 0; i < 6; i++)
	{
		if (a.HasMember(Flds[i]) && a[Flds[i]].IsInt())
		{
			INT v = a[Flds[i]].GetInt();
			if (v < 0) { v = 0; }
			if (v > 10000) { v = 10000; }
			LayoutModel::SetRefSizeValueAt(dest, i, v);
		}
	}
}

// 函数: SolverStr
// 描述: 把 JSON 字符串拷入中间态的定长字段, 超长按截断处理并打印诊断
VOID SolverStr(char* dst, size_t cap, LPCSTR src, LPCSTR szWhere, LPCSTR szField)
{
	if (!LayoutModel::StrCopy(dst, cap, src))
	{
		printf("[JSON] %s 字段 \"%s\" 超长(上限 %u 字节): 已截断\n",
			(szWhere != NULL) ? szWhere : "?", (szField != NULL) ? szField : "?", (unsigned)cap);
	}
}

// 函数: SolverFactor
// 描述: 解析尺寸因子(coeff[8] 与整数 offset)到 SizeFactor
BOOL SolverFactor(SizeFactor& dest, rapidjson::Value& src, LPCSTR szWhere)
{
	BOOL bRet = TRUE;
	LPCSTR szAt = (szWhere != NULL) ? szWhere : "?";
	// 1. 先把输出清零(任何缺项都保持 0)
	FLOAT* pCoeff = (FLOAT*)&dest.coeff;
	for (INT i = 0; i < 8; i++)
	{
		pCoeff[i] = 0.0f;
	}
	dest.offset = 0;
	// 2. 因子对象本身
	if (!src.IsObject())
	{
		printf("[JSON] %s 尺寸因子不是对象: 按 0 处理\n", szAt);
		return FALSE;
	}
	// 3. coeff 数组: 需要 8 个数字
	if (src.HasMember("coeff") && src["coeff"].IsArray())
	{
		rapidjson::Value& srcCoeffs = src["coeff"];
		const rapidjson::SizeType nSize = srcCoeffs.Size();
		for (rapidjson::SizeType i = 0; i < 8; i++)
		{
			if (i >= nSize)
			{
				printf("[JSON] %s coeff 只有 %u 项(需 8): 缺项按 0 处理\n", szAt, (unsigned)nSize);
				bRet = FALSE;
				break;
			}
			if (!srcCoeffs[i].IsNumber())
			{
				printf("[JSON] %s coeff[%u] 不是数字: 按 0 处理\n", szAt, (unsigned)i);
				bRet = FALSE;
				continue;
			}
			pCoeff[i] = srcCoeffs[i].GetFloat();
		}
	}
	else
	{
		printf("[JSON] %s 缺少 coeff 数组: 按 0 处理\n", szAt);
		bRet = FALSE;
	}
	// 4. offset: 需要整数
	if (src.HasMember("offset") && src["offset"].IsInt())
	{
		dest.offset = src["offset"].GetInt();
	}
	else
	{
		printf("[JSON] %s 缺少整数 offset: 按 0 处理\n", szAt);
		bRet = FALSE;
	}
	return bRet;
}

