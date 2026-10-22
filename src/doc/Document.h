// ================================
// 文件: Document.h
// 说明: 文档承载者 —— 一份布局文档的条目表 / 中间态 / 关联路径 / 未保存改动标记
//   边界:
//   - 只承载"当前文档"的数据: 条目数组(池内) + 中间态(布局链 + 元素池) + 激活索引 +
//     关联磁盘路径 + 脏标记; 不含窗口 / 视图 / 文件格式 / 保存策略知识。
//   - 不含会话面状态(预览唯一基座 / 外壳快照 / 内嵌(flex)成员 / 文档版本号) ⇒ 那些归会话侧。
//   依赖方向: doc → framework(只调公开接口); 不得依赖 app / editor。
// ================================

#pragma once
#include <minwindef.h>
#include "framework/layout/Layout.h"
#include "framework/base/BasicDesign.h"
#include "framework/layout/LayoutModel.h"

// ================================
// 结构: LayoutEntry
// 说明: 文档中的一个布局条目: 已构建布局 + 布局独立经验参数 + 推导参考尺寸 + 元信息
//   语义:
//   - 布局窗口尺寸(客户区像素) = autosize width/height 千分比 x 参考基准/1000
//     (参考基准=AutoSize 模式量, 默认屏高; 即 wndW/wndH)
//   - 布局窗口桌面位置 = WND(ctrl0) x/y 几何的像素偏移(wndX/wndY)
//   - 布局内部控件几何 = 依 ref(window 宽高 = 该窗口客户区) 对中间态元素的几何系数求解
//   - pNode = 中间态布局节点(布局数据模型): 元素规格(含几何系数)的存放处,
//     几何解算与编辑器系数格经它读取
// ================================
struct LayoutEntry
{
	Layout* layout;			// 条目布局对象(= 预览唯一基座 pPreviewBase; 内容随 active 装配)			| 8 字节
	AutoSize ref;			// 布局内部参考(窗口宽高 = 布局窗口客户区) | 96 字节
	RefSize refSize;		// 本布局参考尺寸/经验参数(中间态字段)		| 92 字节
	LPCSTR name;			// 布局名(内存池字符串)					| 8 字节
	LPCSTR label;			// 布局标签(内存池字符串)					| 8 字节
	LPCSTR* aRawType;		// 文档原类型串旁挂表(按文档索引; 池内自持)	| 8 字节
	LPCSTR* aRawText;		// 无缓冲控件的声明文本旁挂表(按文档索引; 池内自持)	| 8 字节
	LayoutNode* pNode;		// 中间态布局节点(元素规格/几何系数; 池内)	| 8 字节
	DWORD qwRawType;		// 原类型串旁挂表容量(== 建表时的文档控件数)		| 4 字节
	DWORD qwRawText;		// 声明文本旁挂表容量(== 建表时的文档控件数)		| 4 字节
	LONG wndX;				// 窗口桌面位置 X(像素)				| 4 字节
	LONG wndY;				// 窗口桌面位置 Y(像素)					| 4 字节
	DWORD wndW;				// 窗口客户区宽(autosize width x 基准/1000)| 4 字节
	DWORD wndH;				// 窗口客户区高							| 4 字节
	BOOL IsMaxiable;		// 布局是否可最大化(随文件保存)			| 4 字节
	BOOL IsResizeable;		// 布局是否可调整大小(随文件保存)		| 4 字节
};

// ================================
// 结构: Document
//   说明: 本结构按值内嵌于应用侧成员块 ⇒ 不引入任何额外分配 / 生命周期变化
// ================================
struct Document
{
	LayoutEntry* entries;		// 文档布局条目数组					| 8 字节
	FileMidState docState;		// 文档中间态(布局模型: 布局链 + 元素池; 编辑主体) | 32 字节
	DWORD dwEntries;			// 布局条目数量						| 4 字节
	DWORD dwActive;				// 当前激活(编辑/预览)布局索引		| 4 字节
	LPCSTR lpDocPath;			// 当前文档关联的磁盘文件路径(池内; NULL = 无关联文件) | 8 字节
	BOOL bDocDirty;				// 文档是否有未保存改动(载入/新建/保存成功时清零) | 4 字节

	// 函数: FindByLayout
	// 描述: 按布局指针在文档条目数组内线性反查(未命中返回 NULL)
	// 说明: 只覆盖"条目数组"这一条来源; 预览唯一基座的归属判定 / 外壳快照条目 / 内嵌(flex)成员条目
	//       由调用方(会话侧)另行编排 —— 本模块不承载会话面状态。
	LayoutEntry* FindByLayout(Layout* layout);
	// 函数: SpecAt
	// 描述: 取条目中间态内指定索引的元素规格(不判几何系数有效标志; 越界/未挂接返回 NULL)
	static ElementSpec* SpecAt(LayoutEntry& entry, size_t index);
	// 函数: LayoutLabelAt
	// 描述: 取指定条目的显示名(label 为空则回落 name)拷入调用方缓冲
	// 说明: label 真源 = 条目中间态节点的布局信息(池内); 缓冲无效或越界 ⇒ FALSE(缓冲写空串)
	BOOL LayoutLabelAt(DWORD index, LPSTR buf, size_t cap);
	// 函数: RebindNodes
	// 描述: 把全部条目的中间态布局节点按索引重新挂接到当前中间态布局链上
	// 说明: 条目索引与布局链索引一一对应; 中间态增删不改动未涉及块的地址 ⇒ 条目数组重建或
	VOID RebindNodes();
	// 函数: InsertSpecAt / RemoveSpecAt
	// 描述: 在指定布局的指定文档索引处插入 / 移除一枚元素规格(中间态就地链接/解链; 其后索引整体位移)
	// 说明: 只服务文档条目(外壳快照是只读展开态, 不参与增删); 插入后按索引对齐条目节点;
	//       新元素以当前像素几何表达(零系数 + 像素 offset) —— 几何重解结果与插入当刻的像素几何一致
	// 参数:
	//		IN	docIndex:	文档条目索引(布局→条目索引的反查归调用方: 唯一基座 ⇒ 当前 active 条目)
	//		IN	layout:		目标布局对象(取同索引控件的当前像素; RemoveSpecAt 不需要)
	//		IN	index:		文档索引(与中间态元素索引一一对应)
	// 返回:
	//		BOOL: TRUE = 已插入/移除; FALSE = 中间态元素链操作失败
	BOOL InsertSpecAt(DWORD docIndex, Layout* layout, size_t index);
	BOOL RemoveSpecAt(DWORD docIndex, size_t index);
	// 函数: ReplaceEntries
	// 描述: 整体替换条目数组与激活索引(条目数组由调用方(内存池)持有 ⇒ 本结构只接指针与计数)
	// 说明: 供"重建条目数组"的布局命令(增 / 删 / 重排)统一提交; 本函数不做任何池分配 / 回收 / 释放
	//       (旧数组的归属与释放由调用方负责) ⇒ 纯字段写入。
	// 参数:
	//		IN	pEntries:	新条目数组首址(池内; NULL = 空文档)
	//		IN	dwCount:	新条目数
	//		IN	dwActive:	新激活索引(越界钳制归调用方策略)
	VOID ReplaceEntries(LayoutEntry* pEntries, DWORD dwCount, DWORD dwActive);
	// 函数: GetCount / GetActive / SetActive
	// 描述: 条目数量与激活索引的读写原语
	// 说明: 只做字段存取 —— 越界钳制 / 版本号递增 / 预览同步等策略归调用方(应用侧编排)
	DWORD GetCount();
	DWORD GetActive();
	VOID SetActive(DWORD index);
	// 函数: GetEntry
	// 描述: 取指定索引的条目地址(条目数组未挂接或索引越界 ⇒ NULL)
	LayoutEntry* GetEntry(DWORD index);
	// 函数: GetState
	// 描述: 取文档中间态引用(装配 / 序列化等路径共用; 中间态本体由 LayoutModel 持有)
	FileMidState& GetState();
	// 函数: GetPath / SetPath
	// 描述: 关联磁盘路径的读写
	// 说明: SetPath 只做字段赋值 —— 非空串须由调用方保证池内自持(路径池化属应用侧策略)
	LPCSTR GetPath();
	VOID SetPath(LPCSTR path);
	// 函数: IsDirty
	// 描述: 当前文档是否有未保存改动
	BOOL IsDirty();
	// 函数: MarkDirty / ClearDirty
	// 描述: 置/清"未保存改动"标记
	VOID MarkDirty();
	VOID ClearDirty();
};
