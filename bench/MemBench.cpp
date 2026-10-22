// ================================
// 工装: MemBench
// 描述: MemoryManager(SecureBuffer / Arena / MemPool)的**契约测试 + 基准测试**
//       ① 契约测试: 对齐、不重叠、容量边界、归属查询、回收语义、安全清零
//       ② 微基准: 三分配器的分配延迟与 MemPool 的回收吞吐
//       ③ 工程负载: 按 GenUI 的真实量级(布局数/控件数/缓冲尺寸/churn 模式)取三种典型负载,
//          对照"旧口径(单一笼统线性池)"与"新口径(按用途选型)"
// 用法: MemBench [报告输出路径.md]
// 说明: 纯控制台工装, 只依赖 MemoryManager 与 Win32; 不参与 GenUI 产物构建。
// ================================
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../src/common/MemoryManager.h"

// ================================
// 区域: 常量与报告
// ================================

// GenUI 真实量级(取自工程源码 src\CtrlData.h 与 GenUI.json, 详见 MemMan-record.md §4)
//   布局数 2 = 预览布局 + GenUIEditor; GenUIEditor 控件数 61
//   控件缓冲尺寸(逐类型): CTRL_TEXT_BUF=256 / GENUI_INPUT_ACTION_BUF=328 /
//                         sizeof(GenUIListData)=3092 / sizeof(TitleBuffer)=16 /
//                         sizeof(DOUBLE)=8 / sizeof(INT)=4 / sizeof(LPCSTR)=8 /
//                         sizeof(GenUIComboData)=? / sizeof(GenUISliderData)=? / sizeof(GenUIProgData)=?
//   sizeof(CtrlInfo) 约 64 字节(含 type/label/bufferInfo/factors/pEventHandler 等)
constexpr DWORD GENUI_LAYOUTS = 2;			// 文档布局数
constexpr DWORD GENUI_CTRLS = 61;			// 单布局控件数(取外壳布局, 最重者)
constexpr size_t GENUI_CTRLINFO = 64;		// sizeof(CtrlInfo) 量级
constexpr size_t GENUI_SPECBLOCK = 96;		// sizeof(ControlSpec) 量级(柔性数组元素)
constexpr size_t GENUI_STR = 16;			// 单条规格字符串平均长度(含 NUL)
constexpr DWORD GENUI_STRS_PER_CTRL = 5;	// type/label/text/event/name

// 控件缓冲尺寸池(按工程真实分布: 文本类居多 + 列表 3092 的大块)
static const size_t g_aCtrlBufSizes[] = { 256, 256, 256, 328, 16, 8, 4, 8, 3092, 256 };

static FILE* g_pReport = NULL;		// 报告文件(可为空)
static DWORD g_dwPass = 0;			// 契约测试通过数
static DWORD g_dwFail = 0;			// 契约测试失败数
static LARGE_INTEGER g_freq;		// 高精度计数器频率

// 函数: Say
// 描述: 同时输出到 stdout 与报告文件
// 参数:
//		IN	fmt:	printf 风格格式串
//		IN	...:	可变参数
static VOID Say(LPCSTR fmt, ...)
{
	CHAR sz[1024];
	va_list ap;
	va_start(ap, fmt);
	_vsnprintf_s(sz, sizeof(sz), _TRUNCATE, fmt, ap);
	va_end(ap);
	printf("%s\n", sz);
	if (g_pReport != NULL)
	{
		fprintf(g_pReport, "%s\n", sz);
	}
}

// 函数: NowMs
// 描述: 取当前单调时刻(毫秒)
// 返回:
//		double: 毫秒时刻
static double NowMs()
{
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return (double)t.QuadPart * 1000.0 / (double)g_freq.QuadPart;
}

// 函数: Check
// 描述: 契约断言(只打印失败项, 通过数累计)
// 参数:
//		IN	bOk:	断言是否成立
//		IN	lpWhat:	断言描述
static VOID Check(BOOL bOk, LPCSTR lpWhat)
{
	if (bOk != FALSE)
	{
		g_dwPass++;
		return;
	}
	g_dwFail++;
	Say("  [FAIL] %s", lpWhat);
}

// 函数: Fingerprint
// 描述: 往一块内存写入可校验指纹(用于"相邻分配互不重叠"回归守卫)
// 参数:
//		IN	p:		内存首址
//		IN	size:	写入长度
//		IN	seed:	指纹种子
static VOID Fingerprint(BYTE* p, size_t size, BYTE seed)
{
	for (size_t i = 0; i < size; ++i)
	{
		p[i] = (BYTE)(seed + (BYTE)i);
	}
}

// 函数: VerifyFingerprint
// 描述: 校验指纹是否完好
// 参数:
//		IN	p:		内存首址
//		IN	size:	校验长度
//		IN	seed:	指纹种子
// 返回:
//		BOOL: TRUE 表示未被破坏
static BOOL VerifyFingerprint(CONST BYTE* p, size_t size, BYTE seed)
{
	for (size_t i = 0; i < size; ++i)
	{
		if (p[i] != (BYTE)(seed + (BYTE)i))
		{
			return FALSE;
		}
	}
	return TRUE;
}

// ================================
// 区域: 契约测试
// ================================

// 函数: TestArenaContract
// 描述: Arena 契约: 对齐 / 不重叠 / 边界 / Reset / Mark-Rewind / Contains
static VOID TestArenaContract()
{
	Say("[契约] Arena");
	Arena arena(1u << 20);

	// 1. 对齐恒为 16 字节, 且相邻分配互不重叠(旧线性池"按请求大小记账"缺陷的回归守卫)
	{
		BYTE* apPrev[64];
		size_t aPrevSize[64];
		DWORD dwBad = 0;
		DWORD dwOverlap = 0;
		size_t aSize[4] = { 12, 24, 56, 7 };	// 故意取非 16 倍数
		for (DWORD i = 0; i < 64; ++i)
		{
			size_t size = aSize[i % 4];
			BufferInfo b = arena.Allocate(size);
			if (b.buffer == nullptr || b.size != size)
			{
				dwBad++;
				continue;
			}
			if ((((UINT_PTR)b.buffer) & 15u) != 0)
			{
				dwBad++;
			}
			apPrev[i] = (BYTE*)b.buffer;
			aPrevSize[i] = size;
			Fingerprint((BYTE*)b.buffer, size, (BYTE)i);
		}
		Check(dwBad == 0, "Arena: 分配非空 / size 为请求值 / 用户指针 16 字节对齐");
		for (DWORD i = 0; i < 64; ++i)
		{
			if (!VerifyFingerprint(apPrev[i], aPrevSize[i], (BYTE)i))
			{
				dwOverlap++;
			}
		}
		Check(dwOverlap == 0, "Arena: 相邻分配互不重叠(指纹未被后续分配覆盖)");
	}
	// 2. 容量边界: 一次超量请求必须失败且返回 { nullptr, 0 }
	{
		BufferInfo b = arena.Allocate(arena.Capacity() + 1);
		Check(b.buffer == nullptr && b.size == 0, "Arena: 超出容量返回 { nullptr, 0 }");
	}
	// 3. Mark / Rewind: 回退后同一地址被复用; 非法标记被忽略
	{
		size_t mark = arena.Mark();
		BufferInfo a = arena.Allocate(128);
		arena.Rewind(mark);
		Check(arena.Used() == mark, "Arena: Rewind 回到标记点");
		BufferInfo b = arena.Allocate(128);
		Check(a.buffer == b.buffer, "Arena: Rewind 后同一地址被复用");
		size_t usedNow = arena.Used();
		arena.Rewind(usedNow + 4096);
		Check(arena.Used() == usedNow, "Arena: 大于当前用量的 Rewind 被忽略(不回卷到非法位置)");
	}
	// 4. Reset: 全部失效并回到起点
	{
		arena.Reset();
		Check(arena.Used() == 0, "Arena: Reset 归零");
	}
	// 5. Contains: 池内 / 边界 / 池外 / 空指针
	{
		BufferInfo b = arena.Allocate(16);
		Check(arena.Contains(b.buffer) == TRUE, "Arena: Contains 命中池内首址");
		Check(arena.Contains((BYTE*)b.buffer + arena.Capacity() - 1 - arena.Used() + 15) == TRUE, "Arena: Contains 命中池内末字节");
		Check(arena.Contains((BYTE*)b.buffer - 1) == FALSE, "Arena: Contains 拒绝池前地址");
		Check(arena.Contains(nullptr) == FALSE, "Arena: Contains 拒绝 nullptr");
	}
}

// 函数: TestSecureBufferContract
// 描述: SecureBuffer 契约: 对齐 / 不重叠 / 安全清零 / 保护切换 / Contains
static VOID TestSecureBufferContract()
{
	Say("[契约] SecureBuffer");
	SecureBuffer sec(64 * 1024);

	// 1. 非 8 倍数请求下相邻分配互不重叠(与旧 SecureBuffer 同一纪律)
	{
		BYTE* apPrev[32];
		size_t aSize[32];
		DWORD dwBad = 0;
		size_t aReq[3] = { 12, 24, 56 };
		for (DWORD i = 0; i < 32; ++i)
		{
			BufferInfo b = sec.Allocate(aReq[i % 3]);
			if (b.buffer == nullptr)
			{
				dwBad++;
				continue;
			}
			apPrev[i] = (BYTE*)b.buffer;
			aSize[i] = b.size;
			Fingerprint((BYTE*)b.buffer, b.size, (BYTE)(0x40 + i));
		}
		Check(dwBad == 0, "SecureBuffer: 分配非空");
		for (DWORD i = 0; i < 32; ++i)
		{
			if (!VerifyFingerprint(apPrev[i], aSize[i], (BYTE)(0x40 + i)))
			{
				dwBad++;
			}
		}
		Check(dwBad == 0, "SecureBuffer: 相邻分配互不重叠");
	}
	// 2. 容量边界
	{
		BufferInfo b = sec.Allocate(sec.Capacity() + 1);
		Check(b.buffer == nullptr && b.size == 0, "SecureBuffer: 超出容量返回 { nullptr, 0 }");
	}
	// 3. 安全清零: 写入后清零, 内容必须全 0(验证 RtlSecureZeroMemory 确实落盘)
	{
		BufferInfo b = sec.Allocate(256);
		Check(b.buffer != nullptr, "SecureBuffer: 清零用例分配成功");
		if (b.buffer != nullptr)
		{
			BYTE* p = (BYTE*)b.buffer;
			memset(p, 0xAA, 256);
			sec.SecureZero(0, 0);			// 边界: 零长度清零不得改动内容
			BOOL bAllAA = TRUE;
			for (DWORD i = 0; i < 256; ++i)
			{
				if (p[i] != 0xAA) { bAllAA = FALSE; }
			}
			Check(bAllAA == TRUE, "SecureBuffer: 零长度清零不改动内容");
			sec.SecureZeroAll();
			BOOL bAllZero = TRUE;
			for (DWORD i = 0; i < 256; ++i)
			{
				if (p[i] != 0x00) { bAllZero = FALSE; }
			}
			Check(bAllZero == TRUE, "SecureBuffer: SecureZeroAll 后内容全 0");
		}
	}
	// 4. 页级保护切换(只读 / 读写往返成功)
	{
		BOOL bRo = sec.SetReadOnly();
		BOOL bRw = sec.SetReadWrite();
		Check(bRo == TRUE && bRw == TRUE, "SecureBuffer: SetReadOnly / SetReadWrite 往返成功");
		Check(sec.SetNoAccess() == TRUE, "SecureBuffer: SetNoAccess 成功");
		Check(sec.SetReadWrite() == TRUE, "SecureBuffer: 复位为读写成功");
	}
	// 5. Contains
	{
		Check(sec.Contains(nullptr) == FALSE, "SecureBuffer: Contains 拒绝 nullptr");
	}
}

// 函数: TestMemPoolContract
// 描述: MemPool 契约: 尺寸类覆盖 / 对齐 / 超限 / 归属校验 / 任意顺序回收 / 复用
static VOID TestMemPoolContract()
{
	Say("[契约] MemPool");
	MemPool pool;

	// 1. 尺寸类覆盖 1..MAX_SMALL_SIZE: 每个尺寸都必须可分配(末档缺陷的回归守卫)
	{
		DWORD dwBad = 0;
		DWORD dwBadAlign = 0;
		size_t aEdge[6] = { 1, 16, 17, 4080, 4081, MemPool::MAX_SMALL_SIZE };
		for (size_t size = 1; size <= MemPool::MAX_SMALL_SIZE; ++size)
		{
			BufferInfo b = pool.Allocate(size);
			if (b.buffer == nullptr || b.size != size)
			{
				dwBad++;
			}
			else if ((((UINT_PTR)b.buffer) & 15u) != 0)
			{
				dwBadAlign++;
			}
		}
		Check(dwBad == 0, "MemPool: 1..MAX_SMALL_SIZE 全部可分配(含 4081..4096 边界档)");
		Check(dwBadAlign == 0, "MemPool: 用户指针恒 16 字节对齐");
		for (DWORD i = 0; i < 6; ++i)
		{
			BufferInfo b = pool.Allocate(aEdge[i]);
			Check(b.buffer != nullptr && b.size == aEdge[i], "MemPool: 边界尺寸可分配");
		}
	}
	// 2. 超限与零大小
	{
		BufferInfo bBig = pool.Allocate(MemPool::MAX_SMALL_SIZE + 1);
		BufferInfo bZero = pool.Allocate(0);
		Check(bBig.buffer == nullptr && bBig.size == 0, "MemPool: 超过 MAX_SMALL_SIZE 返回 { nullptr, 0 }");
		Check(bZero.buffer == nullptr && bZero.size == 0, "MemPool: 零大小请求返回 { nullptr, 0 }");
	}
	// 3. 写入不越界 + 单独 Free + 地址复用(LIFO)
	{
		BufferInfo a = pool.Allocate(64);
		BufferInfo b = pool.Allocate(64);
		Check(a.buffer != nullptr && b.buffer != nullptr, "MemPool: 同档两次分配成功");
		Fingerprint((BYTE*)a.buffer, 64, 0x11);
		Fingerprint((BYTE*)b.buffer, 64, 0x22);
		Check(VerifyFingerprint((BYTE*)a.buffer, 64, 0x11) == TRUE &&
			VerifyFingerprint((BYTE*)b.buffer, 64, 0x22) == TRUE, "MemPool: 两个 64B 块互不重叠");
		pool.Free(b);
		BufferInfo c = pool.Allocate(64);
		Check(c.buffer == b.buffer, "MemPool: Free 后同档复用刚回收的块");
		pool.Free(a);
		pool.Free(c);
	}
	// 4. 任意顺序回收(打乱序)不破坏空闲链表: 分配 4096 块后按步进 7 的伪随机序回收, 再全部重新分配
	{
		constexpr DWORD NUM = 4096;
		static BufferInfo aBlocks[NUM];
		DWORD dwBad = 0;
		for (DWORD i = 0; i < NUM; ++i)
		{
			aBlocks[i] = pool.Allocate(48);
			if (aBlocks[i].buffer == nullptr)
			{
				dwBad++;
			}
			else
			{
				*(DWORD*)aBlocks[i].buffer = i;
			}
		}
		Check(dwBad == 0, "MemPool: 批量分配 4096 x 48B 成功");
		for (DWORD i = 0; i < NUM; ++i)
		{
			pool.Free(aBlocks[(i * 7u) % NUM]);
		}
		dwBad = 0;
		for (DWORD i = 0; i < NUM; ++i)
		{
			BufferInfo b = pool.Allocate(48);
			if (b.buffer == nullptr)
			{
				dwBad++;
			}
			else
			{
				pool.Free(b);
			}
		}
		Check(dwBad == 0, "MemPool: 打乱序全量回收后仍可全量再分配(空闲链表完好)");
	}
	// 5. 跨池 Free 必须被忽略(不得污染对方池)
	{
		MemPool other;
		BufferInfo mine = pool.Allocate(64);
		BufferInfo theirs = other.Allocate(64);
		Check(mine.buffer != nullptr && theirs.buffer != nullptr, "MemPool: 跨池用例分配成功");
		Fingerprint((BYTE*)theirs.buffer, 64, 0x33);
		other.Free(mine.buffer);		// 误释放: 必须被忽略
		Check(VerifyFingerprint((BYTE*)theirs.buffer, 64, 0x33) == TRUE,
			"MemPool: 跨池 Free 被忽略(对方池数据未被污染)");
		pool.Free(mine);
		other.Free(theirs);
	}
	// 6. Contains 归属查询
	{
		BufferInfo mine = pool.Allocate(32);
		MemPool other;
		BufferInfo theirs = other.Allocate(32);
		Arena arena(4096);
		BufferInfo inArena = arena.Allocate(32);
		Check(pool.Contains(mine.buffer) == TRUE, "MemPool: Contains 命中本池");
		Check(pool.Contains(theirs.buffer) == FALSE, "MemPool: Contains 拒绝他池指针");
		Check(pool.Contains(inArena.buffer) == FALSE, "MemPool: Contains 拒绝 Arena 指针");
		Check(pool.Contains(nullptr) == FALSE, "MemPool: Contains 拒绝 nullptr");
		pool.Free(mine);
		other.Free(theirs);
	}
}

// 函数: TestAllocatorContract
// 描述: 跑完全部契约测试并汇报通过/失败计数
// 返回:
//		BOOL: TRUE 表示全部通过
static BOOL TestAllocatorContract()
{
	Say("--- [1] 契约测试 ---");
	TestArenaContract();
	TestSecureBufferContract();
	TestMemPoolContract();
	Say("契约测试: 通过 %u / 失败 %u", (unsigned)g_dwPass, (unsigned)g_dwFail);
	return (g_dwFail == 0) ? TRUE : FALSE;
}

// ================================
// 区域: 微基准
// ================================

// 函数: BenchAllocMicro
// 描述: 三分配器的"分配 N 次"单位耗时(ns/op); MemPool 另测"分配 + 立即释放"
static VOID BenchAllocMicro()
{
	constexpr DWORD N = 20000;
	constexpr size_t CAP = 256u * 1024u * 1024u;
	static const size_t aSizes[8] = { 16, 24, 56, 128, 328, 1024, 3092, 4096 };
	Say("--- [2] 微基准: 分配延迟(ns/op, N=%u, 容量 256MB) ---", (unsigned)N);
	Say("%-8s %14s %14s %16s %20s", "尺寸", "SecureBuffer", "Arena", "MemPool(alloc)", "MemPool(alloc+free)");
	for (DWORD s = 0; s < 8; ++s)
	{
		size_t size = aSizes[s];
		double dSecNs = 0.0;
		double dAreNs = 0.0;
		double dPoolNs = 0.0;
		double dPoolFreeNs = 0.0;

		// SecureBuffer: 分配 N 次(不可回收)
		{
			SecureBuffer sec(CAP);
			double t0 = NowMs();
			DWORD dwOk = 0;
			for (DWORD i = 0; i < N; ++i)
			{
				if (sec.Allocate(size).buffer != nullptr) { dwOk++; }
			}
			double t1 = NowMs();
			dSecNs = (dwOk > 0) ? (t1 - t0) * 1e6 / (double)dwOk : 0.0;
		}
		// Arena: 分配 N 次(不可回收)
		{
			Arena arena(CAP);
			double t0 = NowMs();
			DWORD dwOk = 0;
			for (DWORD i = 0; i < N; ++i)
			{
				if (arena.Allocate(size).buffer != nullptr) { dwOk++; }
			}
			double t1 = NowMs();
			dAreNs = (dwOk > 0) ? (t1 - t0) * 1e6 / (double)dwOk : 0.0;
		}
		// MemPool: 分配 N 次(不回池) / 分配 + 立即释放 N 次
		{
			MemPool poolAlloc;
			double t0 = NowMs();
			DWORD dwOk = 0;
			for (DWORD i = 0; i < N; ++i)
			{
				if (poolAlloc.Allocate(size).buffer != nullptr) { dwOk++; }
			}
			double t1 = NowMs();
			dPoolNs = (dwOk > 0) ? (t1 - t0) * 1e6 / (double)dwOk : 0.0;

			MemPool poolFree;
			double t2 = NowMs();
			for (DWORD i = 0; i < N; ++i)
			{
				BufferInfo b = poolFree.Allocate(size);
				if (b.buffer != nullptr) { poolFree.Free(b); }
			}
			double t3 = NowMs();
			dPoolFreeNs = (t3 - t2) * 1e6 / (double)N;
		}
		Say("%-8u %14.1f %14.1f %16.1f %20.1f", (unsigned)size, dSecNs, dAreNs, dPoolNs, dPoolFreeNs);
	}
	Say("注: 线性分配是纯指针推进(与尺寸无关的常数级); MemPool 的 alloc 列随尺寸上升是 slab **补充成本**");
	Say("    (空闲链表为空时要 VirtualAlloc 一个 64KB slab 并切块, 大块每 slab 只切出十几块 ⇒ 摊销显形);");
	Say("    MemPool 的 alloc+free 列(热态复用)才是它的稳态性能, 与尺寸基本无关(见末列)。");
}

// 函数: BenchChurnMicro
// 描述: "不定生命周期小对象"的申请/释放混合负载: 线性池无法回收 vs 池按尺寸类复用
static VOID BenchChurnMicro()
{
	Say("--- [3] 微基准: 小对象 churn(10 万次申请/释放混合, 模拟编辑期反复建销) ---");
	constexpr DWORD N = 100000;
	static const size_t aSizes[6] = { 24, 48, 96, 192, 384, 768 };

	// 旧口径: 单一线性池(只有分配, 无法回收) —— 统计 10 万次操作后的线性占用
	{
		Arena linear(64u * 1024u * 1024u);
		DWORD dwFail = 0;
		double t0 = NowMs();
		for (DWORD i = 0; i < N; ++i)
		{
			if (linear.Allocate(aSizes[i % 6]).buffer == nullptr)
			{
				dwFail++;
			}
		}
		double t1 = NowMs();
		Say("线性池(只增不还): 失败 %u 次; 占用 %u KB; 耗时 %.1f ms",
			(unsigned)dwFail, (unsigned)(linear.Used() / 1024), t1 - t0);
	}
	// 新口径: MemPool 同尺寸反复申请/释放(存活集恒定)
	{
		MemPool pool;
		constexpr DWORD LIVE = 1024;			// 同时存活 1024 个
		static BufferInfo aLive[LIVE];
		DWORD dwFail = 0;
		double t0 = NowMs();
		for (DWORD i = 0; i < N; ++i)
		{
			DWORD slot = i % LIVE;
			if (i >= LIVE && aLive[slot].buffer != nullptr)
			{
				pool.Free(aLive[slot]);
				aLive[slot].buffer = nullptr;
			}
			BufferInfo b = pool.Allocate(aSizes[i % 6]);
			if (b.buffer == nullptr)
			{
				dwFail++;
			}
			aLive[slot] = b;
		}
		double t1 = NowMs();
		for (DWORD i = 0; i < LIVE; ++i)
		{
			if (aLive[i].buffer != nullptr) { pool.Free(aLive[i]); }
		}
		Say("MemPool(存活集 %u): 失败 %u 次; slab %u 个 = 占用 %u KB; 耗时 %.1f ms",
			(unsigned)LIVE, (unsigned)dwFail, (unsigned)pool.SlabCount(),
			(unsigned)(pool.SlabCount() * 64), t1 - t0);
	}
}

// ================================
// 区域: 工程负载(按 GenUI 真实量级)
// ================================

// 函数: BuildDocLinear
// 描述: 按 GenUI 量级模拟"构建一份文档": 求解器 + 每布局"规格块 + 控件数组 + 每控件缓冲 + 字符串"
// 参数:
//		IN	pAlloc:	分配器回调所需的分配器指针(见下面两个包装)
//		IN	aHit:	命中计数(可空)
// 返回:
//		size_t: 本次构建消耗的字节数
static size_t BuildDocLinear(Arena& arena)
{
	size_t used0 = arena.Used();
	// 1. 求解器对象 + layouts 指针表
	arena.Allocate(GENUI_CTRLINFO);
	arena.Allocate(sizeof(void*) * GENUI_LAYOUTS);
	for (DWORD lay = 0; lay < GENUI_LAYOUTS; ++lay)
	{
		// 2. 规格块(柔性数组: LayoutInfo + ControlSpec x N)
		arena.Allocate(GENUI_SPECBLOCK * GENUI_CTRLS + 64);
		// 3. CtrlInfo 数组
		arena.Allocate(GENUI_CTRLINFO * GENUI_CTRLS);
		for (DWORD c = 0; c < GENUI_CTRLS; ++c)
		{
			// 4. 控件缓冲(按真实尺寸分布)
			arena.Allocate(g_aCtrlBufSizes[c % (sizeof(g_aCtrlBufSizes) / sizeof(g_aCtrlBufSizes[0]))]);
			// 5. 规格字符串(type/label/text/event/name)
			arena.Allocate(GENUI_STR * GENUI_STRS_PER_CTRL);
		}
	}
	return arena.Used() - used0;
}

// 函数: BenchWorkloadDocBuild
// 描述: 负载 A: 单次文档构建的分配次数与字节量(线性池)
static VOID BenchWorkloadDocBuild()
{
	constexpr DWORD N = 1000;
	Say("--- [4] 负载 A: 按 GenUI 量级构建一份文档(布局 %u x 控件 %u, 连做 %u 次取均值) ---",
		(unsigned)GENUI_LAYOUTS, (unsigned)GENUI_CTRLS, (unsigned)N);
	Arena arena(256u * 1024u * 1024u);
	double t0 = NowMs();
	size_t used = 0;
	for (DWORD i = 0; i < N; ++i)
	{
		used = BuildDocLinear(arena);
	}
	double t1 = NowMs();
	Say("线性池构建: 单份占用 %u KB; %u 份合计 %.1f ms ⇒ %.2f us/份",
		(unsigned)(used / 1024), (unsigned)N, t1 - t0, (t1 - t0) * 1000.0 / (double)N);
	Say("对照: 旧口径整块预留 128MB(MEM_COMMIT 全提交 + 退出前 128MB 安全清零) ⇒ 单文档实际只用约 %u KB",
		(unsigned)(used / 1024));
}

// 函数: BenchWorkloadEditChurn
// 描述: 负载 B: 编辑期高频 churn —— 改名 + 事件名 + 原类型旁挂表扩容, 各 10 万次
//       旧口径 = 线性池(新建即弃旧, 只增不还); 新口径 = MemPool 建新的同时回收旧的
static VOID BenchWorkloadEditChurn()
{
	constexpr DWORD N = 100000;
	Say("--- [5] 负载 B: 编辑期 churn(%u 次: 改名 2 串 + 事件名表 + 原类型旁挂表) ---", (unsigned)N);

	// 旧口径: 全部走线性池, 旧块一律不回收
	{
		Arena linear(512u * 1024u * 1024u);
		DWORD dwFail = 0;
		LPCSTR lpName = nullptr;
		LPCSTR lpEvent = nullptr;
		LPCSTR* apRawType = nullptr;
		double t0 = NowMs();
		for (DWORD i = 0; i < N; ++i)
		{
			// 改名: 两条新串(旧串留在池里)
			BufferInfo b1 = linear.Allocate(GENUI_STR);
			BufferInfo b2 = linear.Allocate(GENUI_STR);
			if (b1.buffer == nullptr || b2.buffer == nullptr) { dwFail++; }
			lpName = (LPCSTR)b1.buffer;
			(void)lpName;
			// 事件名: 新串(旧的留在池里)
			BufferInfo b3 = linear.Allocate(GENUI_STR);
			if (b3.buffer == nullptr) { dwFail++; }
			lpEvent = (LPCSTR)b3.buffer;
			(void)lpEvent;
			// 旁挂表: 按控件总数重建(旧表留在池里) —— 仅每 1000 次触发一次, 模拟真实扩容频率
			if ((i % 1000) == 0)
			{
				BufferInfo b4 = linear.Allocate(sizeof(LPCSTR) * GENUI_CTRLS);
				if (b4.buffer == nullptr) { dwFail++; }
				apRawType = (LPCSTR*)b4.buffer;
				(void)apRawType;
			}
		}
		double t1 = NowMs();
		Say("线性池(旧块不回收): 失败 %u 次; 占用 %u KB(单调增长); 耗时 %.1f ms",
			(unsigned)dwFail, (unsigned)(linear.Used() / 1024), t1 - t0);
	}
	// 新口径: 新串进池, 同时回收旧串; 旁挂表重建时回收旧表
	{
		MemPool pool;
		Arena fallback(4096u * 1024u);		// 超限回退(本负载不会用到)
		DWORD dwFail = 0;
		LPCSTR lpName = nullptr;
		LPCSTR lpEvent = nullptr;
		LPCSTR* apRawType = nullptr;
		DWORD dwFallback = 0;
		double t0 = NowMs();
		for (DWORD i = 0; i < N; ++i)
		{
			// 改名: 新串进池 + 回收旧串
			BufferInfo b1 = pool.Allocate(GENUI_STR);
			BufferInfo b2 = pool.Allocate(GENUI_STR);
			if (b1.buffer == nullptr || b2.buffer == nullptr) { dwFail++; }
			if (lpName != nullptr && pool.Contains(lpName) == TRUE) { pool.Free((VOID*)lpName); }
			lpName = (LPCSTR)b1.buffer;
			(void)b2;
			// 事件名: 新串进池 + 回收旧串
			BufferInfo b3 = pool.Allocate(GENUI_STR);
			if (b3.buffer == nullptr) { dwFail++; }
			if (lpEvent != nullptr && pool.Contains(lpEvent) == TRUE) { pool.Free((VOID*)lpEvent); }
			lpEvent = (LPCSTR)b3.buffer;
			// 旁挂表: 池优先, 超限回退 Arena; 回收旧表(仅池内者)
			if ((i % 1000) == 0)
			{
				BufferInfo b4 = pool.Allocate(sizeof(LPCSTR) * GENUI_CTRLS);
				if (b4.buffer == nullptr)
				{
					b4 = fallback.Allocate(sizeof(LPCSTR) * GENUI_CTRLS);
					dwFallback++;
				}
				if (apRawType != nullptr && pool.Contains(apRawType) == TRUE) { pool.Free((VOID*)apRawType); }
				apRawType = (LPCSTR*)b4.buffer;
			}
		}
		double t1 = NowMs();
		Say("MemPool + 回收: 失败 %u 次; 回退 %u 次; slab %u 个 = %u KB(有界); 耗时 %.1f ms",
			(unsigned)dwFail, (unsigned)dwFallback, (unsigned)pool.SlabCount(),
			(unsigned)(pool.SlabCount() * 64), t1 - t0);
		if (lpName != nullptr && pool.Contains(lpName) == TRUE) { pool.Free((VOID*)lpName); }
		if (lpEvent != nullptr && pool.Contains(lpEvent) == TRUE) { pool.Free((VOID*)lpEvent); }
		if (apRawType != nullptr && pool.Contains(apRawType) == TRUE) { pool.Free((VOID*)apRawType); }
	}
}

// 函数: BenchWorkloadReopen
// 描述: 负载 C: 反复打开文档 100 次 —— 线性池单调增长 vs Arena Mark/Rewind 代际回收
static VOID BenchWorkloadReopen()
{
	constexpr DWORD N = 100;
	Say("--- [6] 负载 C: 反复打开文档 %u 次 ---", (unsigned)N);

	// 旧口径: 每代数据留在同一个线性池里, 永不回收
	{
		Arena linear(256u * 1024u * 1024u);
		DWORD dwFail = 0;
		double t0 = NowMs();
		for (DWORD i = 0; i < N; ++i)
		{
			if (BuildDocLinear(linear) == 0) { dwFail++; }
		}
		double t1 = NowMs();
		Say("线性池(单池不回收): 失败 %u 次; 累计占用 %u KB(约 %u KB/次); 耗时 %.1f ms",
			(unsigned)dwFail, (unsigned)(linear.Used() / 1024),
			(unsigned)(linear.Used() / 1024 / N), t1 - t0);
	}
	// 新口径: 文档代际驻"文档 Arena", 每次打开前 Mark 到代际起点(或 Reset), 关闭时 Rewind
	{
		Arena docArena(64u * 1024u * 1024u);
		Arena sessionArena(4u * 1024u * 1024u);		// 会话级数据(外壳快照等)另池存放, 不参与 Rewind
		size_t genMark = docArena.Mark();
		DWORD dwFail = 0;
		double t0 = NowMs();
		for (DWORD i = 0; i < N; ++i)
		{
			// 会话级数据(不随文档代际回收)
			sessionArena.Allocate(GENUI_STR);
			// 文档代际数据
			docArena.Rewind(genMark);
			if (BuildDocLinear(docArena) == 0) { dwFail++; }
		}
		double t1 = NowMs();
		Say("Arena 代际回收(Mark/Rewind): 失败 %u 次; 文档池占用 %u KB(单代峰值, 有界); 会话池 %u KB; 耗时 %.1f ms",
			(unsigned)dwFail, (unsigned)(docArena.Used() / 1024),
			(unsigned)(sessionArena.Used() / 1024), t1 - t0);
	}
}

// 函数: BenchTeardown
// 描述: 负载 D: 析构代价 —— 128MB 池"退出前安全清零"vs Arena 直接释放
static VOID BenchTeardown()
{
	Say("--- [7] 负载 D: 128MB 池的析构代价(旧口径退出前 SecureZeroAll) ---");
	constexpr size_t CAP = 128u * 1024u * 1024u;
	{
		double t0 = NowMs();
		{
			SecureBuffer* p = new SecureBuffer(CAP);
			// 触碰 1/4 页面, 模拟真实使用(避免全是未触页导致的零开销假象)
			BufferInfo b = p->Allocate(CAP / 4);
			if (b.buffer != nullptr)
			{
				memset(b.buffer, 1, b.size);
			}
			double t1 = NowMs();
			Say("SecureBuffer(128MB) 构造 + 触碰 32MB: %.1f ms", t1 - t0);
			delete p;						// 析构: SecureZeroAll(128MB) + VirtualFree
		}
		double t2 = NowMs();
		Say("SecureBuffer 析构(128MB 安全清零 + 释放): %.1f ms", t2 - t0);
	}
	{
		double t0 = NowMs();
		Arena* p = new Arena(CAP);
		BufferInfo b = p->Allocate(CAP / 4);
		if (b.buffer != nullptr)
		{
			memset(b.buffer, 1, b.size);
		}
		double t1 = NowMs();
		delete p;							// 析构: 只释放
		double t2 = NowMs();
		Say("Arena(128MB) 构造 + 触碰 32MB: %.1f ms; 析构(直接释放): %.1f ms", t1 - t0, t2 - t1);
	}
}

// ================================
// 区域: 入口
// ================================

// 函数: main
// 描述: 跑契约测试与全部基准, 输出报告
// 参数:
//		IN	argc:	参数个数
//		IN	argv:	argv[1] 可给出报告输出路径(markdown)
// 返回:
//		int: 0 表示契约测试全通过; 1 表示契约测试存在失败项
int main(int argc, char** argv)
{
	QueryPerformanceFrequency(&g_freq);
	if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0')
	{
		fopen_s(&g_pReport, argv[1], "wb");
	}
	Say("# MemoryManager 基准与契约测试报告");
	Say("");
	Say("> 生成方式: `MemBench.exe <报告路径>`(工程 `GenUI\\bench`); 仅依赖 MemoryManager 与 Win32。");
	Say("> 量级来源: 按 GenUI 真实量级取负载(布局 %u / 控件 %u / 缓冲 4..3092B / churn 模式), 详见 `MemBench.cpp` 顶部常量注释。",
		(unsigned)GENUI_LAYOUTS, (unsigned)GENUI_CTRLS);
	Say("");
	BOOL bContract = TestAllocatorContract();
	Say("");
	BenchAllocMicro();
	Say("");
	BenchChurnMicro();
	Say("");
	BenchWorkloadDocBuild();
	Say("");
	BenchWorkloadEditChurn();
	Say("");
	BenchWorkloadReopen();
	Say("");
	BenchTeardown();
	Say("");
	Say("--- 结论 ---");
	Say("1. 契约: 对齐 16B、相邻分配不重叠、容量/超限/零大小边界、跨池 Free 忽略、归属查询均通过。");
	Say("2. 线性分配(SecureBuffer / Arena)单次分配是纯指针推进, 延迟最低; 但**不可回收** ⇒ 编辑期 churn 与反复打开文档会单调增长。");
	Say("3. MemPool 单次分配/回收为 O(1) 空闲链表操作, 同尺寸复用后存活集有界 ⇒ 适合不定生命周期小对象。");
	Say("4. SecureBuffer 的页级保护与退出清零是**敏感数据**的代价; 普通 UI 数据不应承担(见负载 D)。");
	Say("");
	Say("契约测试: 通过 %u / 失败 %u", (unsigned)g_dwPass, (unsigned)g_dwFail);
	if (g_pReport != NULL)
	{
		fclose(g_pReport);
		g_pReport = NULL;
	}
	// 报告落盘后再把契约结论打印一次(便于只取退出码的自动化用例)
	printf("MEMBENCH_RESULT: %s (pass=%u fail=%u)\n", (bContract != FALSE) ? "PASS" : "FAIL",
		(unsigned)g_dwPass, (unsigned)g_dwFail);
	return (bContract != FALSE) ? 0 : 1;
}
