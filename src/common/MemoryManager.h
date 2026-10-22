#pragma once
// 包含 Windows 基础类型定义
// 具体类型：
// DWORD, BOOL, LPVOID, LPCVOID, BYTE, SIZE_T
#include <minwindef.h>

// ================================
// 模块: MemoryManager
// 描述: 统一内存管理模块, 含三个按用途分工的分配器
//       SecureBuffer - 敏感数据, 支持页级保护与安全清零
//       Arena        - 线性分配器, 批量/临时/超长生命周期
//       MemPool      - 分级固定块池, 不定生命周期小对象
// 无内部线程同步
// 完整使用说明见 MemoryManager.md
// ================================

// ================================
// 公共辅助函数
// ================================

// 函数: AlignUp
// 描述: 把大小向上对齐到指定对齐值
// 参数:
//		IN	size:	原始大小(字节)
//		IN	align:	对齐值(字节, 2 的幂)
// 返回:
//		size_t: 对齐后的大小(不小于 size)
inline size_t AlignUp(size_t size, size_t align)
{
	return (size + align - 1) & ~(align - 1);
}

// ================================
// 结构体: BufferInfo
// 描述: 分配结果信息, 三个分配器统一使用该结构表达分配结果
// 注意: size 是请求大小, 不是对齐后的实际占用
// ================================
struct BufferInfo
{
	VOID*	buffer;	// 内存首址(分配失败为 nullptr)
	size_t	size;	// 请求大小(分配失败为 0)
};

// ================================
// 类名: SecureBuffer
// 描述: 敏感数据专用缓冲区, 支持页级保护与安全清零
// 注意: 对齐粒度为 sizeof(void*)；析构时自动 SecureZeroAll
// 详见: MemoryManager.md 第 1 节
// ================================
class SecureBuffer
{
public:
	SecureBuffer(size_t requestedSize);
	~SecureBuffer();

	// 函数: Allocate
	// 描述: 从安全缓冲区申请内存(按 alignBytes 对齐, 返回请求大小)
	// 参数:
	//		IN	size:	请求大小(字节)
	// 返回:
	//		BufferInfo: 成功为 { 首址, size }; 越界或底层分配失败为 { nullptr, 0 }
	BufferInfo Allocate(size_t size);

	// 函数: Contains
	// 描述: 判定指针是否落在本缓冲区内(只比地址范围, 不解引用)
	// 参数:
	//		IN	p:	待判定指针(可为 nullptr)
	// 返回:
	//		BOOL: TRUE 表示落在本缓冲区地址区间内
	BOOL Contains(LPCVOID p) const;

	// 函数: Capacity
	// 描述: 取缓冲区总容量(已按系统页向上对齐)
	// 返回:
	//		size_t: 容量(字节)
	size_t Capacity() const;

	// 函数: Used
	// 描述: 取已分配字节数(含每块的对齐空隙)
	// 返回:
	//		size_t: 已用字节数
	size_t Used() const;

	// 函数: SecureZero
	// 描述: 安全对缓冲区内指定区域进行清零
	// 参数:
	//		IN	offset:	起始偏移(字节)
	//		IN	size:	清零长度(字节)
	VOID SecureZero(size_t offset, size_t size);

	// 函数: SecureZeroAll
	// 描述: 安全对整个缓冲区进行清零
	VOID SecureZeroAll();

	// 函数: Protect
	// 描述: 设置缓冲区保护标志(页级操作, 有系统调用开销)
	// 参数:
	//		IN	protectFlag:	PAGE_* 保护标志
	// 返回:
	//		BOOL: TRUE 表示设置成功
	BOOL Protect(DWORD protectFlag);

	// 函数: SetReadOnly
	// 描述: 将缓冲区设置为只读
	// 返回:
	//		BOOL: TRUE 表示设置成功
	BOOL SetReadOnly();

	// 函数: SetReadWrite
	// 描述: 将缓冲区设置为读写
	// 返回:
	//		BOOL: TRUE 表示设置成功
	BOOL SetReadWrite();

	// 函数: SetNoAccess
	// 描述: 将缓冲区设置为不可访问
	// 返回:
	//		BOOL: TRUE 表示设置成功
	BOOL SetNoAccess();

private:
	BYTE*	buffer;			// 缓冲区首址
	size_t	capacity;		// 缓冲区的容量
	size_t	used;			// 已使用的字节数(按对齐后占用记账)
	size_t	alignBytes;		// 默认分配对齐字节(sizeof(void*))
	DWORD	flagProtect;	// 当前保护标志

	// 禁止拷贝
	SecureBuffer(const SecureBuffer&) = delete;
	SecureBuffer& operator=(const SecureBuffer&) = delete;
};

// ================================
// 类名: Arena
// 描述: 线性分配器, 分配即移动指针, 通过 Reset 或 Mark/Rewind 回收
// 注意: 对齐 16 字节
// 详见: MemoryManager.md 第 2 节
// ================================
class Arena
{
public:
	Arena(size_t requestedSize);
	~Arena();

	// 函数: Allocate
	// 描述: 从 Arena 申请内存(按 16 字节对齐, 返回请求大小)
	// 参数:
	//		IN	size:	请求大小(字节)
	// 返回:
	//		BufferInfo: 成功为 { 首址, size }; 越界或底层分配失败为 { nullptr, 0 }
	BufferInfo Allocate(size_t size);

	// 函数: Contains
	// 描述: 判定指针是否落在本 Arena 内(只比地址范围, 不解引用)
	// 参数:
	//		IN	p:	待判定指针(可为 nullptr)
	// 返回:
	//		BOOL: TRUE 表示落在本 Arena 地址区间内
	BOOL Contains(LPCVOID p) const;

	// 函数: Capacity
	// 描述: 取 Arena 总容量(已按系统页向上对齐)
	// 返回:
	//		size_t: 容量(字节)
	size_t Capacity() const;

	// 函数: Used
	// 描述: 取已分配字节数(含每块的对齐空隙)
	// 返回:
	//		size_t: 已用字节数
	size_t Used() const;

	// 函数: Reset
	// 描述: 重置整个 Arena, 所有已分配内存失效
	VOID Reset();

	// 函数: Mark
	// 描述: 记录当前分配位置, 用于后续 Rewind
	// 返回:
	//		size_t: 当前位置标记
	size_t Mark() const;

	// 函数: Rewind
	// 描述: 回退到指定标记位置, 释放标记之后分配的内存
	// 参数:
	//		IN	mark:	先前由 Mark 取得的标记
	VOID Rewind(size_t mark);

private:
	BYTE*	buffer;			// Arena 首址
	size_t	capacity;		// Arena 的容量
	size_t	used;			// 已使用的字节数(按对齐后占用记账)
	size_t	alignBytes;		// 默认分配对齐字节(16)

	// 禁止拷贝
	Arena(const Arena&) = delete;
	Arena& operator=(const Arena&) = delete;
};

// ================================
// 类名: MemPool
// 描述: 分级固定块内存池, 支持任意顺序单独 Free
//
// 注意: Free 会做归属校验；用户指针 16 字节对齐
// 详见: MemoryManager.md 第 3 节
// ================================
class MemPool
{
public:
	MemPool();
	~MemPool();

	// 函数: Allocate
	// 描述: 从内存池申请内存
	// 参数:
	//		IN	size:	请求大小(字节)
	// 返回:
	//		BufferInfo: 成功为 { 首址, size }；size 为 0、超限或分配失败时为 { nullptr, 0 }
	BufferInfo Allocate(size_t size);

	// 函数: Free
	// 描述: 释放内存回内存池(BufferInfo 版本)
	// 参数:
	//		IN	info:	Allocate 返回的分配信息(非本池分配者忽略)
	VOID Free(BufferInfo info);

	// 函数: Free
	// 描述: 释放内存回内存池(便利重载, 接收裸指针)
	// 参数:
	//		IN	ptr:	Allocate 返回的用户指针(非本池分配者忽略, nullptr 忽略)
	VOID Free(VOID* ptr);

	// 函数: Contains
	// 描述: 判定指针是否属于本池
	// 参数:
	//		IN	p:	待判定指针(可为 nullptr)
	// 返回:
	//		BOOL: TRUE 表示落在本池某个 slab 的数据区范围内
	BOOL Contains(LPCVOID p) const;

	// 函数: SlabCount
	// 描述: 取已分配的 slab 数量(遍历 slab 链表, O(n))
	// 返回:
	//		size_t: slab 数量
	size_t SlabCount() const;

	// 用户可请求的最大尺寸
	static constexpr size_t MAX_SMALL_SIZE = 4096;

private:
	// 禁止拷贝
	MemPool(const MemPool&) = delete;
	MemPool& operator=(const MemPool&) = delete;

	static constexpr size_t SLAB_SIZE = 64 * 1024;		// 每个 slab 64KB
	static constexpr size_t NUM_SIZE_CLASSES = 28;		// 尺寸类数量(与 SIZE_CLASSES 表对应)

	struct FreeBlock
	{
		FreeBlock*	next;		// 下一个空闲块
	};

	struct SizeClass
	{
		size_t		blockSize;	// 块总大小(包含 BlockHeader)
		FreeBlock*	freeList;	// 空闲块链表
	};

	struct BlockHeader
	{
		SizeClass*	sizeClass;	// 指向所属尺寸类
		size_t		padding;	// 填充到 16 字节对齐
	};

	// Slab 头, 内嵌于 slab 内存开头, 不额外分配
	struct alignas(16) Slab
	{
		Slab*	next;			// 下一个 slab
	};

	// 尺寸类表(块总大小, 升序, 均为 16 的倍数; 末档保证 MAX_SMALL_SIZE 可服务)
	static const size_t SIZE_CLASSES[NUM_SIZE_CLASSES];

	SizeClass	sizeClasses[NUM_SIZE_CLASSES];	// 静态数组, 非 new
	size_t		alignBytes;						// 用户数据对齐字节(16)
	Slab*		slabList;						// slab 链表

	// 函数: InitSizeClasses
	// 描述: 初始化尺寸类数组
	VOID InitSizeClasses();

	// 函数: FindSizeClass
	// 描述: 依请求大小找到能容纳它的最小尺寸类
	// 参数:
	//		IN	size:	请求大小(不含块头)
	// 返回:
	//		SizeClass*: 命中返回尺寸类指针; 无可用档返回 nullptr
	SizeClass* FindSizeClass(size_t size);

	// 函数: AllocateFromSlab
	// 描述: 申请新的 slab, 切分成块并挂入该尺寸类的空闲链表
	// 参数:
	//		IN	sc:	目标尺寸类
	// 返回:
	//		VOID*: 成功返回 slab 首址; 失败返回 nullptr
	VOID* AllocateFromSlab(SizeClass* sc);

	// 函数: FreeSlabList
	// 描述: 释放全部 slab(Slab 头内嵌在 slab 内存中, 无需 delete)
	VOID FreeSlabList();

	// 函数: IsOwnedPointer
	// 描述: 检查指针是否落在本池某个 slab 的用户数据区范围内
	// 参数:
	//		IN	ptr:	待检查指针
	// 返回:
	//		BOOL: TRUE 表示属于本池
	BOOL IsOwnedPointer(VOID* ptr) const;
};
