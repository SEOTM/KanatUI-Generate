// ================================
// 文件: FlexHost.h
// 说明: 内嵌(flex)窗口装配与查询模块 —— 框架级"一个窗口内的多个内嵌窗口"的会话面持有者
//   边界:
//   - 只承载内嵌(flex)成员: 由外壳源文档装配条目(线性只读展开态 + 自身参考尺寸 + 中间态节点绑定) ·
//     活动开关(按需显示) · 渲染查询(活动布局指针集合) · 释放。
//   - "谁是内嵌窗口"的名单策略由本模块承载(布局文件不承载内嵌关系)
//     但成员的解析产物来自外壳源(与外壳快照同源同生命周期) ⇒ 不随被编辑文档的切换 / 新建变化。
//   - 基座归属判据(活动集合只对外壳布局成立)的外壳布局指针经 SetShellBase 由应用侧注入
//     (外壳快照构建 / 释放点原位设置; NULL = 无外壳基座 ⇒ 查询返回"无活动成员")。
//   - 会话分配器 / 控件装配 / 布局对象构建一律调 framework\base\mem\AllocatorHub · framework\Controls ·
//     framework\LayoutBuilder 的现有公开接口 ⇒ 不触锁定面。
//   依赖方向: app → framework(Layout / LayoutModel / LayoutBuilder / Controls / LayoutSolver / base\mem);
//     不依赖 app 其它模块 / editor ⇒ 不新增任何跨层反依赖。
// ================================

#pragma once
#include <minwindef.h>
#include "doc/Document.h"		// 条目形状 LayoutEntry(成员条目即一枚 LayoutEntry + 活动位)

struct AutoSize;
struct BufferInfo;
class Layout;
class LayoutSolver;

// ================================
// 常量: 内嵌(flex)窗口成员名
// 说明: "谁是内嵌窗口"由应用层决定, 布局文件不承载内嵌关系
//   成员来源 = 外壳源文档(与外壳快照同源同生命周期, 不随被编辑文档变化);
//   初版名单 = 「提示布局」 —— 按需显示(装配时非活动)
// ================================
constexpr LPCSTR KANATUI_GENERATE_HINT_LAYOUT_NAME = u8"提示布局";	// 提示布局(内嵌成员; 初版唯一)

// ================================
// 常量: 提示正文缓冲容量
// 说明: 提示正文可能较长(文件名 / 路径等) ⇒ 内嵌成员的正文控件(##TEXT_HINT)按本容量装配
//   (通用装配给文本类控件的容量 = CtrlBuffer.h 的 CTRL_TEXT_BUF)
// ================================
constexpr DWORD HINT_BODY_BUF = 512;

// ================================
// 结构: FlexEntry
// 说明: 一个内嵌(flex)窗口条目 —— 框架三类布局情形之"一个窗口内的多个内嵌窗口"
//   语义:
//   - 内嵌窗口不拥有独立 HWND: 由宿主(基座)窗口的渲染帧内以 LayoutRender::RenderFlex 渲染(同 context 同帧)
//   - entry.layout = 该成员的一份线性(只读)展开态; 来源 = 外壳源文档(会话级固定: 与外壳快照
//     同源、同生命周期)⇒ 不随被编辑文档的切换 / 新建而变化(提示布局不依赖用户正在编辑的数据)
//   - 内嵌窗口位置 = 该布局窗体区(WND)声明的像素位置(相对宿主客户区); 尺寸 = 该窗体区声明尺寸
//   - bActive = 是否活动(按需显示: 装配时恒 FALSE; 需要显示时经 SetFlexActive 置真, 用毕复位)
// ================================
struct FlexEntry
{
	LayoutEntry entry;			// 内嵌成员条目(线性展开态 + 自身参考尺寸 + 中间态节点绑定)
	BOOL bActive;				// 是否活动(渲染查询按此过滤)
};

// ================================
// 类名: FlexHost
// 说明: 内嵌(flex)窗口成员的装配 · 活动开关 · 渲染查询 · 释放(全静态类; 构造/析构私有化 ⇒ 进程级单例语义)
// ================================
class FlexHost
{
private:
	// 内嵌(flex)窗口成员状态(三字段同名同义)
	static FlexEntry* pFlexes;			// 内嵌(flex)窗口条目数组(按名单装配; 快照语义) | 8 字节
	static DWORD dwFlexes;				// 内嵌成员数量						| 4 字节
	static Layout** ppFlexActive;		// 活动成员布局指针缓存(渲染查询返回物; 池内) | 8 字节
	static Layout* pShellBase;			// 外壳(基座)布局指针(应用侧注入; NULL = 无基座 ⇒ 查询无活动成员) | 8 字节

	// 私有化构造/析构
	FlexHost() = delete;
	~FlexHost() = delete;

public:
	// 函数: SetShellBase
	// 描述: 注入"内嵌成员的宿主基座"的外壳布局指针(应用侧在外壳快照构建 / 释放点原位设置; NULL = 复位)
	static VOID SetShellBase(Layout* pShell);
	// 函数: FindFlexEntry
	// 描述: 按窗口布局对象取内嵌成员条目的公开只读投影(未命中返回 NULL; 供条目族统一反查编排)
	static LayoutEntry* FindFlexEntry(Layout* layout);
	// 函数: BuildFlexMembersFromSolver
	// 描述: 由外壳源文档的解析结果装配全部内嵌(flex)成员(每个成员一份线性只读展开态)
	// 说明: ① 成员名单 = 文件内 aFlexMemberNames(应用层决定); 来源 = 外壳源(与外壳快照同一份解析产物)
	//          ⇒ 成员不随被编辑文档的切换 / 新建而变化(提示布局不依赖用户正在编辑的数据);
	//          节点取自 solver 的中间态(生存期 = 应用线性分配器) ⇒ 文档替换不会使其悬垂;
	//       ② 外壳源缺少某成员 ⇒ 跳过并逐条诊断, 不算失败(成员数随之减少);
	//       ③ 装配后 bActive 恒 FALSE —— 按需显示(由 SetFlexActive 控制: 显示前置真, 用毕复位);
	//       ④ 成员条目/活动指针缓存取自应用线性分配器(随分配器整体释放)。
	// 参数:
	//		IN	solver:	外壳源的解析器(与外壳快照同一份解析产物)
	// 返回:
	//		BOOL: TRUE = 装配完成(允许缺少部分或全部成员); FALSE = 应用未就绪 / 分配失败
	static BOOL BuildFlexMembersFromSolver(LayoutSolver* solver);
	// 函数: FreeFlexLayouts
	// 描述: 释放全部内嵌(flex)成员(布局对象显式析构; 条目与缓存内存由线性分配器持有)
	// 说明: 成员为线性展开态 ⇒ 析构只逐控件析构, 不涉及链节点池; 释放后活动集合为空
	static VOID FreeFlexLayouts();
	// 函数: GetActiveFlexLayouts
	// 描述: 查询指定基座布局当前活动的内嵌(flex)窗口集合(渲染接线入口)
	// 说明: ① 归属过滤: 初版内嵌成员的宿主恒为外壳布局(编辑窗口宿主) ⇒ 传入其它基座视为"无活动成员";
	//       ② 收集结果写入条目级的指针缓存(渲染期只读; 每次查询重算, 元素有效至下次查询);
	//       ③ 无活动成员(或全部非活动) ⇒ 返回 NULL 且数量置 0(调用方据此跳过渲染)。
	// 参数:
	//		IN	base:		基座(宿主)布局引用
	//		OUT	pdwCount:	活动成员数量(无成员置 0; 可为 NULL)
	// 返回:
	//		Layout: 活动成员的布局对象指针数组(二级指针); 无活动成员返回 NULL
	static Layout** GetActiveFlexLayouts(Layout& base, DWORD* pdwCount);
	// 函数: GetFlexRef
	// 描述: 取指定内嵌成员的参考尺寸(渲染需要; 未命中返回缺省样例)
	static CONST AutoSize& GetFlexRef(Layout* layout);
	// 函数: GetFlexCount
	// 描述: 内嵌成员总数(装配结果; 供诊断)
	static DWORD GetFlexCount();
	// 函数: SetFlexActive
	// 描述: 置某内嵌成员的活动状态(业务段驱动入口; 按布局名命中)
	// 参数:
	//		IN	name:		成员布局名(装配时取自中间态 name)
	//		IN	bActive:	TRUE = 活动(渲染) / FALSE = 非活动(不渲染)
	// 返回:
	//		BOOL: TRUE = 命中并已设置; FALSE = 未命中 / 入参无效 / 应用未就绪
	static BOOL SetFlexActive(LPCSTR name, BOOL bActive);
};
