#include "common/MemoryManager.h"
#include <sysinfoapi.h>
#include <memoryapi.h>

// ================================
// 模块: MemoryManager 实现
// 描述: 三个分配器的实现, 底层统一 VirtualAlloc / VirtualFree, 内部不用 new/delete
// ================================

// ================================
// 类名: SecureBuffer
// 描述: 敏感数据专用缓冲区(页级保护 + 安全清零)
// ================================

// 函数: SecureBuffer
// 描述: 构造函数, 按系统页向上对齐容量并整块提交
// 参数:
//		IN	requestedSize:	请求容量(字节)
SecureBuffer::SecureBuffer(size_t requestedSize)
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	size_t pageSize = sysInfo.dwPageSize;
	capacity = AlignUp(requestedSize, pageSize);
	used = 0;
	buffer = (BYTE*)VirtualAlloc(nullptr, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	flagProtect = PAGE_READWRITE;
	// 计算默认分配对齐字节(这里使用系统指针大小作为对齐值)
	alignBytes = sizeof(void*);	// 一般为 4 或 8 字节
	// 底层分配失败时容量归零, 后续 Allocate 返回 { nullptr, 0 }
	if (buffer == nullptr)
	{
		capacity = 0;
	}
}

// 函数: ~SecureBuffer
// 描述: 析构函数: 先安全清零, 再整块释放
SecureBuffer::~SecureBuffer()
{
	// 释放缓冲区内存
	if (buffer)
	{
		// 在释放前清零缓冲区
		SecureZeroAll();
		VirtualFree(buffer, 0, MEM_RELEASE);
		buffer = nullptr;
	}
}

// 函数: Allocate
// 描述: 从安全缓冲区申请内存(按 alignBytes 对齐, 返回请求大小)
// 参数:
//		IN	size:	请求大小(字节)
// 返回:
//		BufferInfo: 成功为 { 首址, size }; 越界或底层分配失败为 { nullptr, 0 }
BufferInfo SecureBuffer::Allocate(size_t size)
{
	BufferInfo info = { nullptr, 0 };
	if (buffer == nullptr || size == 0)
	{
		return info;	// 底层未就绪或零大小请求, 返回空信息
	}
	size_t alignedSize = AlignUp(size, alignBytes);
	if (used + alignedSize > capacity)
	{
		return info;	// 超出容量, 返回空信息
	}
	// 返回分配信息(包括指针和大小, 但是大小为请求的大小)
	info.buffer = buffer + used;
	info.size = size;
	// used 按对齐后的占用累加
	used += alignedSize;
	return info;
}

// 函数: Contains
// 描述: 判定指针是否落在本缓冲区内(只比地址范围, 不解引用)
// 参数:
//		IN	p:	待判定指针(可为 nullptr)
// 返回:
//		BOOL: TRUE 表示落在本缓冲区地址区间内
BOOL SecureBuffer::Contains(LPCVOID p) const
{
	if (buffer == nullptr || p == nullptr)
	{
		return FALSE;
	}
	CONST BYTE* b = (CONST BYTE*)p;
	return (b >= buffer) && (b < buffer + capacity) ? TRUE : FALSE;
}

// 函数: Capacity
// 描述: 取缓冲区总容量(已按系统页向上对齐)
// 返回:
//		size_t: 容量(字节)
size_t SecureBuffer::Capacity() const
{
	return capacity;
}

// 函数: Used
// 描述: 取已分配字节数(含每块的对齐空隙)
// 返回:
//		size_t: 已用字节数
size_t SecureBuffer::Used() const
{
	return used;
}

// 函数: SecureZero
// 描述: 安全对缓冲区内指定区域进行清零
// 参数:
//		IN	offset:	起始偏移(字节)
//		IN	size:	清零长度(字节)
VOID SecureBuffer::SecureZero(size_t offset, size_t size)
{
	if (buffer == nullptr || offset + size > capacity)
	{
		return;	// 底层未就绪或超出容量, 直接返回
	}
	RtlSecureZeroMemory(buffer + offset, size);
}

// 函数: SecureZeroAll
// 描述: 安全对整个缓冲区进行清零
VOID SecureBuffer::SecureZeroAll()
{
	SecureZero(0, capacity);
}

// 函数: Protect
// 描述: 设置缓冲区保护标志
// 参数:
//		IN	protectFlag:	PAGE_* 保护标志
// 返回:
//		BOOL: TRUE 表示设置成功
BOOL SecureBuffer::Protect(DWORD protectFlag)
{
	if (!buffer)
	{
		return FALSE;	// 缓冲区未分配, 返回 FALSE
	}
	DWORD oldProtect;
	BOOL result = VirtualProtect(buffer, capacity, protectFlag, &oldProtect);
	if (result)
	{
		flagProtect = protectFlag;
	}
	return result;
}

// 函数: SetReadOnly
// 描述: 将缓冲区设置为只读
// 返回:
//		BOOL: TRUE 表示设置成功
BOOL SecureBuffer::SetReadOnly()
{
	return Protect(PAGE_READONLY);
}

// 函数: SetReadWrite
// 描述: 将缓冲区设置为读写
// 返回:
//		BOOL: TRUE 表示设置成功
BOOL SecureBuffer::SetReadWrite()
{
	return Protect(PAGE_READWRITE);
}

// 函数: SetNoAccess
// 描述: 将缓冲区设置为不可访问
// 返回:
//		BOOL: TRUE 表示设置成功
BOOL SecureBuffer::SetNoAccess()
{
	return Protect(PAGE_NOACCESS);
}

// ================================
// 类名: Arena
// 描述: 线性分配器(16 字节对齐, Reset 或 Rewind 回收)
// ================================

// 函数: Arena
// 描述: 构造函数, 按系统页向上对齐容量并整块提交
// 参数:
//		IN	requestedSize:	请求容量(字节)
Arena::Arena(size_t requestedSize)
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	size_t pageSize = sysInfo.dwPageSize;
	capacity = AlignUp(requestedSize, pageSize);
	used = 0;
	buffer = (BYTE*)VirtualAlloc(nullptr, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	alignBytes = 16;
	// 底层分配失败时容量归零, 后续 Allocate 返回 { nullptr, 0 }
	if (buffer == nullptr)
	{
		capacity = 0;
	}
}

// 函数: ~Arena
// 描述: 析构函数, 整块释放(不清零)
Arena::~Arena()
{
	if (buffer)
	{
		VirtualFree(buffer, 0, MEM_RELEASE);
		buffer = nullptr;
	}
}

// 函数: Allocate
// 描述: 从 Arena 申请内存(按 16 字节对齐; 返回请求大小)
// 参数:
//		IN	size:	请求大小(字节)
// 返回:
//		BufferInfo: 成功为 { 首址, size }; 越界或底层分配失败为 { nullptr, 0 }
BufferInfo Arena::Allocate(size_t size)
{
	BufferInfo info = { nullptr, 0 };
	if (buffer == nullptr || size == 0)
	{
		return info;	// 底层未就绪或零大小请求, 返回空信息
	}
	// used 按对齐后的占用累加
	size_t alignedSize = AlignUp(size, alignBytes);
	if (used + alignedSize > capacity)
	{
		return info;	// 超出容量, 返回空信息
	}
	info.buffer = buffer + used;
	info.size = size;
	used += alignedSize;
	return info;
}

// 函数: Contains
// 描述: 判定指针是否落在本 Arena 内(只比地址范围, 不解引用)
// 参数:
//		IN	p:	待判定指针(可为 nullptr)
// 返回:
//		BOOL: TRUE 表示落在本 Arena 地址区间内
BOOL Arena::Contains(LPCVOID p) const
{
	if (buffer == nullptr || p == nullptr)
	{
		return FALSE;
	}
	CONST BYTE* b = (CONST BYTE*)p;
	return (b >= buffer) && (b < buffer + capacity) ? TRUE : FALSE;
}

// 函数: Capacity
// 描述: 取 Arena 总容量(已按系统页向上对齐)
// 返回:
//		size_t: 容量(字节)
size_t Arena::Capacity() const
{
	return capacity;
}

// 函数: Used
// 描述: 取已分配字节数(含每块的对齐空隙)
// 返回:
//		size_t: 已用字节数
size_t Arena::Used() const
{
	return used;
}

// 函数: Reset
// 描述: 重置整个 Arena(所有已分配内存失效; 内容不清零)
VOID Arena::Reset()
{
	used = 0;
}

// 函数: Mark
// 描述: 返回当前分配位置标记
// 返回:
//		size_t: 当前位置标记
size_t Arena::Mark() const
{
	return used;
}

// 函数: Rewind
// 描述: 回退到指定标记(传入大于当前用量的标记则忽略)
// 参数:
//		IN	mark:	先前由 Mark 取得的标记
VOID Arena::Rewind(size_t mark)
{
	if (mark <= used)
	{
		used = mark;
	}
}

// ================================
// 类名: MemPool
// 描述: 分级固定块内存池(任意顺序单独 Free)
// ================================

// 尺寸类表, 按升序排列, 均为 16 的倍数; 元素 = 块总大小(含 BlockHeader 16 字节)
// 末档 4112 = BlockHeader(16) + MAX_SMALL_SIZE(4096)
const size_t MemPool::SIZE_CLASSES[MemPool::NUM_SIZE_CLASSES] = {
	32, 48, 64, 80, 96, 112, 128,
	160, 192, 224, 256, 320, 384, 448, 512,
	640, 768, 896, 1024, 1280, 1536, 1792, 2048,
	2560, 3072, 3584, 4096, 4112
};

// 函数: MemPool
// 描述: 构造函数, 初始化尺寸类(sizeClasses 是内嵌数组, 不需要 new)
MemPool::MemPool()
	: alignBytes(16)
	, slabList(nullptr)
{
	InitSizeClasses();
}

// 函数: ~MemPool
// 描述: 析构函数, 释放所有 slab
MemPool::~MemPool()
{
	FreeSlabList();
}

// 函数: InitSizeClasses
// 描述: 初始化尺寸类数组
VOID MemPool::InitSizeClasses()
{
	for (size_t i = 0; i < NUM_SIZE_CLASSES; ++i)
	{
		sizeClasses[i].blockSize = SIZE_CLASSES[i];
		sizeClasses[i].freeList = nullptr;
	}
}

// 函数: FindSizeClass
// 描述: 根据请求大小找到合适的尺寸类(能容纳"块头 + 请求"的最小档)
// 参数:
//		IN	size:	请求大小(不含块头)
// 返回:
//		SizeClass*: 命中返回尺寸类指针; 无可用档返回 nullptr
MemPool::SizeClass* MemPool::FindSizeClass(size_t size)
{
	size_t need = sizeof(BlockHeader) + size;
	for (size_t i = 0; i < NUM_SIZE_CLASSES; ++i)
	{
		if (sizeClasses[i].blockSize >= need)
		{
			return &sizeClasses[i];
		}
	}
	return nullptr;
}

// 函数: Allocate
// 描述: 从内存池申请内存
// 参数:
//		IN	size:	请求大小(字节)
// 返回:
//		BufferInfo: 成功为 { 首址, size }; 失败为 { nullptr, 0 }
BufferInfo MemPool::Allocate(size_t size)
{
	BufferInfo info = { nullptr, 0 };

	if (size == 0)
	{
		return info;
	}
	if (size > MAX_SMALL_SIZE)
	{
		// 超过最大小对象尺寸, 返回空信息
		return info;
	}

	SizeClass* sc = FindSizeClass(size);
	if (!sc)
	{
		return info;
	}

	if (sc->freeList == nullptr)
	{
		// 空闲链表为空, 申请新的 slab
		if (!AllocateFromSlab(sc))
		{
			return info;
		}
	}

	// 从空闲链表取一个块
	FreeBlock* block = sc->freeList;
	sc->freeList = block->next;

	// 设置 BlockHeader
	BlockHeader* header = (BlockHeader*)block;
	header->sizeClass = sc;
	header->padding = 0;

	// 返回分配信息(跳过 BlockHeader, size 为请求的大小)
	info.buffer = (BYTE*)block + sizeof(BlockHeader);
	info.size = size;
	return info;
}

// 函数: AllocateFromSlab
// 描述: 申请一个新的 slab, 切分成块并加入空闲链表
//       Slab 头内嵌于 slab 内存开头, 不额外分配
// 参数:
//		IN	sc:	目标尺寸类
// 返回:
//		VOID*: 成功返回 slab 首址; 失败返回 nullptr
VOID* MemPool::AllocateFromSlab(SizeClass* sc)
{
	VOID* mem = VirtualAlloc(nullptr, SLAB_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!mem)
	{
		return nullptr;
	}

	// 把 Slab 头直接放在 slab 内存开头, 用链表串起来
	Slab* slab = (Slab*)mem;
	slab->next = slabList;
	slabList = slab;

	// 从 Slab 头之后开始切分块(起始偏移按 16 对齐 ⇒ 用户指针保持 16 字节对齐)
	size_t startOffset = AlignUp(sizeof(Slab), alignBytes);
	size_t available = SLAB_SIZE - startOffset;
	size_t blockSize = sc->blockSize;
	size_t numBlocks = available / blockSize;

	BYTE* ptr = (BYTE*)mem + startOffset;
	for (size_t i = 0; i < numBlocks; ++i)
	{
		FreeBlock* block = (FreeBlock*)ptr;
		block->next = sc->freeList;
		sc->freeList = block;
		ptr += blockSize;
	}
	return mem;
}

// 函数: IsOwnedPointer
// 描述: 检查指针是否落在本池某个 slab 的用户数据区范围内
// 参数:
//		IN	ptr:	待检查指针
// 返回:
//		BOOL: TRUE 表示属于本池
BOOL MemPool::IsOwnedPointer(VOID* ptr) const
{
	BYTE* p = (BYTE*)ptr;
	size_t startOffset = AlignUp(sizeof(Slab), alignBytes);

	for (Slab* s = slabList; s; s = s->next)
	{
		BYTE* begin = (BYTE*)s + startOffset;	// 跳过 Slab 头及对齐填充
		BYTE* end = (BYTE*)s + SLAB_SIZE;
		if (p >= begin && p < end)
		{
			return TRUE;
		}
	}
	return FALSE;
}

// 函数: Contains
// 描述: 判定指针是否属于本池(归属查询)
// 参数:
//		IN	p:	待判定指针(可为 nullptr)
// 返回:
//		BOOL: TRUE 表示落在本池某个 slab 的数据区范围内
BOOL MemPool::Contains(LPCVOID p) const
{
	if (p == nullptr)
	{
		return FALSE;
	}
	return IsOwnedPointer((VOID*)p);
}

// 函数: SlabCount
// 描述: 取已分配的 slab 数量
// 返回:
//		size_t: slab 数量
size_t MemPool::SlabCount() const
{
	size_t count = 0;
	for (Slab* s = slabList; s; s = s->next)
	{
		count++;
	}
	return count;
}

// 函数: Free
// 描述: 释放内存回内存池(BufferInfo 版本)
// 参数:
//		IN	info:	Allocate 返回的分配信息
VOID MemPool::Free(BufferInfo info)
{
	Free(info.buffer);
}

// 函数: Free
// 描述: 释放内存回内存池(裸指针版本)
//       校验指针是否属于本池
// 参数:
//		IN	ptr:	Allocate 返回的用户指针
VOID MemPool::Free(VOID* ptr)
{
	if (!ptr)
	{
		return;
	}

	// 校验: 指针是否落在本池的某个 slab 范围内
	if (!IsOwnedPointer(ptr))
	{
		return;	// 非本池分配的指针, 直接忽略
	}

	// 获取 BlockHeader
	BlockHeader* header = (BlockHeader*)((BYTE*)ptr - sizeof(BlockHeader));
	SizeClass* sc = header->sizeClass;

	// 二次校验: sizeClass 是否指向本池的 sizeClasses 数组
	if (sc < sizeClasses || sc >= sizeClasses + NUM_SIZE_CLASSES)
	{
		return;	// 数据损坏, 拒绝释放
	}

	// 将块放回空闲链表
	FreeBlock* block = (FreeBlock*)header;
	block->next = sc->freeList;
	sc->freeList = block;
}

// 函数: FreeSlabList
// 描述: 释放所有 slab(Slab 头内嵌在 slab 内存中, 无需 delete)
VOID MemPool::FreeSlabList()
{
	Slab* current = slabList;
	while (current)
	{
		Slab* next = current->next;
		VirtualFree(current, 0, MEM_RELEASE);
		current = next;
	}
	slabList = nullptr;
}
