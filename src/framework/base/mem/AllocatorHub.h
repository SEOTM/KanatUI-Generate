// ================================
// 文件: AllocatorHub.h
// 描述: 会话分配器持有处 —— 应用统一线性分配器(Arena)与小对象池(MemPool)的建立、持有与
//   分配原语。
// ================================
#pragma once
#include "framework/base/mem/MemoryManager.h"

// ================================
// 命名空间: AllocatorHub
// 描述: 会话分配器的持有与分配原语(进程级单例语义; 与 KanatUIGenerateEditor / SelectionOverlay 同风格)
// ================================
namespace AllocatorHub
{
	// 函数: Init
	// 描述: 建立会话分配器(应用启动期调用一次; 已建立时先 Release 再重建)
	BOOL Init(size_t cbArena);

	// 函数: Release
	// 描述: 释放会话分配器(应用退出期调用一次; 幂等)
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
	BufferInfo AllocSmall(size_t size);

	// 函数: FreeSmall
	// 描述: 回收一次 AllocSmall 的产物 —— 仅当指针确属小对象池时才真正回收
	VOID FreeSmall(BufferInfo info);

	// 函数: CopyString
	// 描述: 把字符串复制到线性分配器并返回其指针(入参 NULL 或分配器未就绪 ⇒ NULL, 每次调用新分配)
	LPCSTR CopyString(LPCSTR text);
}
