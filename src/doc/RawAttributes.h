// ================================
// 文件: doc\RawAttributes.h
// 说明: 数据保真旁挂表族 —— 文档原类型串 / 无缓冲控件声明文本(全静态类)
//   边界:
//   - 两表挂在文档条目级(LayoutEntry::aRawType / aRawText; 池内自持), 均按文档索引键;
//     表惰性建立, 容量 = 首次记录时的文档控件数(只增不减: 删除控件时条目整体前移, 容量不变
//     ⇒ 与文档索引一一对应); 条目为空(NULL)表示"无该串"。
//   - 条目反查(布局指针 → 条目地址)属会话面编排 —— 四条来源(预览唯一基座归属 / 条目数组 /
//     外壳快照 / 内嵌(flex)成员)归应用侧 ⇒ 本模块只按注入的函数指针调用, 不认识应用主类。
//   依赖方向: doc → framework(Layout 只调其现有公开接口; 分配原语取 base\mem\AllocatorHub);
//     不依赖 app / editor ⇒ 无新增跨层反依赖。
// ================================
#pragma once
#include <minwindef.h>
#include <stddef.h>		// size_t(索引 / 容量形参)

// 前向声明: 入参布局对象与条目结构(实现处用完整类型)
class Layout;
struct LayoutEntry;

// ================================
// 类型: pfnFindLayoutEntry
// 描述: 条目反查函数指针类型(布局指针 → 文档条目地址; 未命中 ⇒ NULL)
// 说明: 反查的四条来源(预览唯一基座归属 / 条目数组 / 外壳快照 / 内嵌(flex)成员)全属会话面状态
//       ⇒ 本体留应用侧; 本模块只按注入的指针调用(未注入 ⇒ NULL)。
// 参数:
//		IN	layout:	目标布局对象
// 返回:
//		LayoutEntry*: 条目地址; NULL = 未命中
// ================================
using pfnFindLayoutEntry = LayoutEntry*(*)(Layout* layout);

// ================================
// 类名: RawAttributes
// 说明: 文档原类型串 / 无缓冲控件声明文本两类旁挂表的读写(全静态成员; 构造/析构私有化 ⇒ 进程级单例语义)
// ================================
class RawAttributes
{
private:
	// 注入的条目反查(反查本体属会话面 ⇒ 由应用侧"分配成员存储"之后注入; NULL = 未注入)
	static pfnFindLayoutEntry pfnFindEntry;		// 条目反查函数指针					| 8 字节

	// 私有化构造/析构
	RawAttributes() = delete;
	~RawAttributes() = delete;

	// 函数: IsReady
	// 描述: 本模块是否就绪(私有判据件) —— 应用未就绪判据的等价件
	static BOOL IsReady();
	// 函数: EnsureRawTable
	// 描述: 确保一张"字符串旁挂表"(原类型串表 / 声明文本表)的容量不小于 qwCap(不足则迁移扩容)
	// 说明: 两表同构(条目类型均为 LPCSTR, 容量字段为 DWORD) ⇒ 共用一个扩容实现;
	//       旧表确属小对象池时回收, 回落线性分配器的块安全忽略
	// 参数:
	//		IN/OUT	ppTable:	表首址(容量不足时就地替换为新表)
	//		IN/OUT	pdwCap:		表容量(随扩容更新)
	//		IN		qwCap:		需要的容量
	// 返回:
	//		BOOL: TRUE = 容量已满足; FALSE = 入参无效 / 池分配失败
	static BOOL EnsureRawTable(LPCSTR** ppTable, DWORD* pdwCap, size_t qwCap);

public:
	// 函数: SetEntryLookup
	// 描述: 注入条目反查函数指针(NULL = 复位)
	// 说明: 由应用侧在"分配成员存储"之后注入(承载者按值内嵌在成员块内 ⇒ 早于该步反查取不到条目)
	// 参数:
	//		IN	pfnLookup:	条目反查函数地址(应用未就绪 / 已释放 ⇒ NULL)
	static VOID SetEntryLookup(pfnFindLayoutEntry pfnLookup);

	// -------- 文档原类型串旁挂表(控件不持有原类型串) --------
	// 说明: 原类型串按文档索引(不含预览选中标记; 与中间态元素同口径)存放在文档条目级旁挂表
	//       (池内自持), 控件对象(CtrlInfo)不持有原类型串; 该表供保存/编辑层使用, 渲染层只认 CtrlInfo::type。
	// 函数: SetRawType
	// 描述: 记录/覆盖某布局内指定文档索引的"文档原类型串"(表按需惰性建立, 容量 = 文档控件数)
	// 返回: BOOL TRUE = 已记录; FALSE = 布局无效/索引越界/池分配失败(调用方按"回落规范类型名"处理)
	static BOOL SetRawType(Layout* layout, size_t index, LPCSTR rawType);
	// 函数: InsertRawTypeAt
	// 描述: 在指定文档索引处插入一枚"文档原类型串"条目(其后条目整体后移; 与控件重排同步)
	// 参数:
	//		IN	layout:		目标布局对象
	//		IN	index:		插入位置
	//		IN	rawType:	原类型串(NULL = 空条目)
	// 返回:
	//		BOOL: TRUE = 已插入; FALSE = 入参无效 / 索引越界 / 池分配失败
	static BOOL InsertRawTypeAt(Layout* layout, size_t index, LPCSTR rawType);
	// 函数: GetRawType
	// 描述: 取某布局内指定文档索引的"文档原类型串"(表内为空 ⇒ 返回 NULL)
	static LPCSTR GetRawType(Layout* layout, size_t index);
	// 函数: RemoveRawTypeAt
	// 描述: 移除某布局旁挂表中指定文档索引的条目(后续条目前移; 与控件删除同步)
	static BOOL RemoveRawTypeAt(Layout* layout, size_t index);
	// 函数: SetRawText / GetRawText / RemoveRawTextAt
	// 描述: 无缓冲控件的声明文本旁挂表(同类机制) —— 没有缓冲的类型(未知类型 / 未使用槽位 /
	//       视频专用类型 LIST·TIMEBAR·RENDER)在控件对象里没有文本存储, 但其 JSON text 键仍原样往返
	//       ⇒ 与文档原类型串旁挂表(SetRawType/GetRawType)同法,
	//       由文档条目级旁挂表按文档索引承载(池内自持)。
	//       控件一旦有了自身缓冲(正常文本类), 其文本即由缓冲承载, 本表条目不参与。
	static BOOL SetRawText(Layout* layout, size_t index, LPCSTR text);
	static LPCSTR GetRawText(Layout* layout, size_t index);
	static BOOL RemoveRawTextAt(Layout* layout, size_t index);
	// 函数: InsertRawTextAt
	// 描述: 在指定文档索引处插入一枚"无缓冲控件声明文本"条目(其后条目整体后移; 与控件重排同步)
	// 参数:
	//		IN	layout:	目标布局对象
	//		IN	index:	插入位置
	//		IN	text:	声明文本(NULL = 空条目)
	// 返回:
	//		BOOL: TRUE = 已插入; FALSE = 入参无效 / 索引越界 / 池分配失败
	static BOOL InsertRawTextAt(Layout* layout, size_t index, LPCSTR text);
};
