// ================================
// 文件: AllocatorHub.cpp
// 描述: 会话分配器的持有与分配原语实现(口径、分层与建立/释放时机见 AllocatorHub.h 的文件头说明)
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <string.h>
#include "common/AllocatorHub.h"

// ================================
// 区域: 文件内静态状态
// ================================
// 线性分配器(会话级: 由 Init 建立、Release 释放)与小对象池(值对象: 默认构造即就绪)
// 说明: 小对象池不随 Release 复位(只 delete 线性分配器; 池内 slab 随进程结束回收)
static Arena* sArena = NULL;
static MemPool sSmallPool;

// ================================
// 区域: 接口实现区
// ================================
// 函数: Init
// 描述: 建立会话分配器(见 .h; 已建立时先 Release 再重建)
BOOL AllocatorHub::Init(size_t cbArena)
{
	if (sArena != NULL)
	{
		Release();				// 幂等: 已建立 ⇒ 先释放再重建
	}
	sArena = new Arena(cbArena);
	return (sArena != NULL) ? TRUE : FALSE;
}
// 函数: Release
// 描述: 释放线性分配器(幂等; 小对象池不复位 —— 口径见文件内静态状态区)
VOID AllocatorHub::Release()
{
	if (sArena != NULL)
	{
		delete sArena;
		sArena = NULL;
	}
}
// 函数: GetArena
Arena* AllocatorHub::GetArena()
{
	return sArena;
}
// 函数: GetSmallPool
MemPool* AllocatorHub::GetSmallPool()
{
	return &sSmallPool;
}
// 函数: AllocBuffer
BufferInfo AllocatorHub::AllocBuffer(size_t size)
{
	BufferInfo ret{ nullptr, 0 };
	if (sArena != NULL)
	{
		ret = sArena->Allocate(size);
	}
	return ret;
}
// 函数: AllocSmall
// 描述: 小对象分配 —— 池优先, 超限/失败回退线性分配器
BufferInfo AllocatorHub::AllocSmall(size_t size)
{
	BufferInfo ret{ nullptr, 0 };
	if (size > 0 && size <= MemPool::MAX_SMALL_SIZE)
	{
		ret = sSmallPool.Allocate(size);
	}
	if (ret.buffer == nullptr)
	{
		ret = AllocBuffer(size);		// 超限 / 池分配失败 ⇒ 回落线性分配器
	}
	return ret;
}
// 函数: FreeSmall
// 描述: 回收一次 AllocSmall 的产物(仅当指针确属小对象池时才真正回收)
VOID AllocatorHub::FreeSmall(BufferInfo info)
{
	if (info.buffer != nullptr && sSmallPool.Contains(info.buffer) == TRUE)
	{
		sSmallPool.Free(info);
	}
}
// 函数: CopyString
// 描述: 把字符串复制到线性分配器并返回其指针
LPCSTR AllocatorHub::CopyString(LPCSTR text)
{
	LPCSTR ret = NULL;
	if (text != NULL && sArena != NULL)
	{
		CONST size_t len = strlen(text) + 1;
		BufferInfo buf = sArena->Allocate(len);
		if (buf.buffer != NULL)
		{
			strcpy_s((char*)buf.buffer, len, text);
			ret = (LPCSTR)buf.buffer;
		}
	}
	return ret;
}
