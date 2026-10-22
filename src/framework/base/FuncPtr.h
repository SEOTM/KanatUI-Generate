#pragma once
#include <minwindef.h>
#include <stdint.h>		// uint64_t(回调四元组的 oper/param)
// 前向声明 Layout
class Layout;

// ================================
// 描述: 预定义需要使用的函数指针与配套数据结构类型
// ================================

// 类型: pfnCtrlEvent
// 描述: 控件响应函数指针类型(控件事件回调签名)
// 说明: 回调为四元组 ——
//       · obj   = &ctrlinfo(触发/被绘制的控件对象);
//       · oper  = 操作码(64 位三段: 前 16 = 公共段 / 次 16 = 专属段 / 后 32 = 搭载段;
//                  见 ControlSpec.h 的"区域: 操作码(OPER)系统");
//       · param = 参数(业务数据: 行号 / 槽位号 / 新值等; 默认为空 ⇒ 传 0);
//       · data  = 附加数据指针(默认 nullptr; 有缓冲的控件传 bufferInfo.buffer)。
using pfnCtrlEvent = VOID(*)(VOID* obj, uint64_t oper, uint64_t param, void* data);

// 类型: pfnOverlayBefore
// 描述: 视图叠加件绘制回调(渲染插入点) —— 在布局内指定索引的控件绘制之前被调用一次
// 说明: 供"视图件"(如应用层的选中态表达件)画进正在渲染的布局, 而不必把该件寄生进布局的控件链:
//       实现方按索引自行决定是否绘制(不匹配即零动作), 渲染出口不感知其语义;
//       回调在内容子窗口内、该控件提交之前发生 ⇒ 叠加件自然处于"被绘制控件之下"的层级。
using pfnOverlayBefore = VOID(*)(Layout& layout, size_t index);

// 结构: EventInfo
// 描述: 事件信息结构体
struct EventInfo
{
	pfnCtrlEvent pfnEvent;	// 指向控件响应函数的指针
	DWORD bufferSize;		// 事件处理器要求的缓冲字节数(0 = 无需缓冲; 非 0 时按此为该控件分配缓冲)
};

// =================
// 二进制 RAW 数据: 编码后存储到计算机内存以及后续存储在磁盘文件中的数据
// 二进制 RAW 数据大小允许为零，表示空数据
// 编解码器使用的数据索引在受保护的内存中执行操作，允许对新增数据进行在原有的 RAW 数据基础上进行增量编码
// 特别的，此处定义一个结构体用于传递 RAW 数据和其大小

// 结构: RawDataInfo
// 描述: 二进制 RAW 数据信息结构体
struct RawDataInfo
{
	CONST BYTE * data;	// 指向二进制 RAW 数据的指针
	size_t size;		// 二进制 RAW 数据的大小（字节数）
	BOOL isUseful;		// 解码不影响 RAW 有效性; 编码后生成新 RAW ⇒ 影响原 RAW 有效性
						// 编码后 RAW 大小可变 ⇒ 影响原有数据的完整性
};
// 结构: OperDataInfo
// 描述: 编码/解码数据结构体
struct OperDataInfo
{
	BYTE* data;		// 指向编码/解码数据的指针
	size_t size;	// 编码/解码数据的大小（字节数）
};

// 类型: pfnEncoder
// 描述: 数据编码函数指针类型, 根据二进制 RAW 数据、32 字节操作哈希指针、编码数据和 INDEX 进行编码
using pfnEncoder = RawDataInfo(*)(RawDataInfo* pRawDataInfo, CONST BYTE* pOperHash, OperDataInfo* pOperDataInfo, DWORD index);

// 类型: pfnDecoder
// 描述: 数据解码函数指针类型, 根据二进制 RAW 数据、32 字节操作哈希指针和 INDEX 进行解码
using pfnDecoder = OperDataInfo(*)(RawDataInfo* pRawDataInfo, CONST BYTE* pOperHash, DWORD index);