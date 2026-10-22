// ================================
// 文件: AllocatorHub.h
// 描述: 会话分配器持有处 —— 应用统一线性分配器(Arena)与小对象池(MemPool)的建立、持有与
//   分配原语。
// ================================
#pragma once
#include "common/MemoryManager.h"

// ================================
// 命名空间: AllocatorHub
// 描述: 会话分配器的持有与分配原语(进程级单例语义)
// ================================
namespace AllocatorHub
{
	// 函数: Init
	// 描述: 建立会话分配器(应用启动期调用一次; 已建立时先 Release 再重建)
	// 参数:
	//		IN	cbArena:	线性分配器容量(字节; 0 ⇒ 用实现内默认值)
	// 返回:
	//		BOOL: TRUE = 已就绪; FALSE = 线性分配器分配失败(此后各分配原语返回空)
	BOOL Init(size_t cbArena);

	// 函数: Release
	// 描述: 释放会话分配器(应用退出期调用一次; 幂等)
	// 说明: 线性分配器整体释放(会话内对象随之失效); 小对象池复位为初值。
	VOID Release();

	// 函数: GetArena
	// 描述: 取应用统一线性分配器(文档/会话级; 未建立 ⇒ NULL)
	Arena* GetArena();

	// 函数: GetSmallPool
	// 描述: 取应用小对象池(不定生命周期小对象: 中间态块 / 链节点 / 旁挂表 / 控件缓冲; 恒非 NULL)
	MemPool* GetSmallPool();

	// 函数: AllocBuffer
	// 描述: 从线性分配器分配一段内存
	BufferInfo AllocBuffer(size_t size);

	// 函数: AllocSmall
	// 描述: 小对象分配 —— 池优先, 超限/失败回退线性分配器
	// 说明: 请求 <= MemPool::MAX_SMALL_SIZE 则先取小对象池(可被 FreeSmall 单独回收);
	//       超限或池分配失败 ⇒ 回落线性分配器(随整体释放)。
	BufferInfo AllocSmall(size_t size);

	// 函数: FreeSmall
	// 描述: 回收一次 AllocSmall 的产物 —— 仅当指针确属小对象池时才真正回收
	// 说明: 落回线性分配器的指针、字符串字面量、外部指针都安全忽略(MemPool::Contains 归属校验)。
	VOID FreeSmall(BufferInfo info);

	// 函数: CopyString
	// 描述: 把字符串复制到线性分配器并返回其指针(入参 NULL 或分配器未就绪 ⇒ NULL, 每次调用新分配)
	LPCSTR CopyString(LPCSTR text);
}
