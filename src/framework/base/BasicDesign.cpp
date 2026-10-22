#include <Windows.h>
#include "framework/base/BasicDesign.h"
// 函数: GetScreenWidth
// 描述: 取主显示器的屏幕宽度(像素)
DWORD ScreenRef::GetScreenWidth()
{
	return GetSystemMetrics(SM_CXSCREEN);
}
// 函数: GetScreenHeight
// 描述: 取主显示器的屏幕高度(像素)
DWORD ScreenRef::GetScreenHeight()
{
	return GetSystemMetrics(SM_CYSCREEN);
}

// ================================
// 区域: 显示器工作区查询(ScreenRef 续)
// 说明: 窗口位置声明需要回答两个系统侧问题 —— "主显示器的工作区在哪"(随机落点的范围)与
//       "某个矩形能否落在当前屏幕上"(声明位置是否越屏); 两问都归屏幕参考件
// ================================

// 结构: MonitorProbe
// 描述: 显示器工作区相交判定的探针(经 lParam 传入枚举回调)
struct MonitorProbe
{
	CONST RECT* pRect;		// 待判定矩形(桌面坐标)
	BOOL        bHit;		// 是否已与某显示器工作区相交(命中即停止枚举)
};

// 函数: ProbeMonitorProc
// 描述: EnumDisplayMonitors 的回调 —— 待判定矩形与某显示器工作区相交即置位探针并停止枚举
static BOOL CALLBACK ProbeMonitorProc(HMONITOR hMonitor, HDC hdc, LPRECT lprcClip, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(hdc);
	UNREFERENCED_PARAMETER(lprcClip);
	MonitorProbe* pProbe = (MonitorProbe*)lParam;
	MONITORINFO mi{};
	if (pProbe == NULL || pProbe->pRect == NULL)
	{
		return TRUE;		// 探针无效 ⇒ 继续枚举(无事可做)
	}
	mi.cbSize = sizeof(mi);
	if (GetMonitorInfoW(hMonitor, &mi) == FALSE)
	{
		return TRUE;		// 取不到该显示器信息 ⇒ 跳过它(继续枚举)
	}
	RECT rcHit{};
	if (IntersectRect(&rcHit, pProbe->pRect, &mi.rcWork) != FALSE)
	{
		pProbe->bHit = TRUE;
		return FALSE;		// 已判定 ⇒ 停止枚举
	}
	return TRUE;
}

// 函数: GetPrimaryWorkArea
// 描述: 取主显示器的工作区矩形(桌面坐标; 已排除任务栏等保留区)
BOOL ScreenRef::GetPrimaryWorkArea(RECT* pWork)
{
	RECT rcWork{};
	if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0) == FALSE)
	{
		return FALSE;
	}
	if (pWork != NULL)
	{
		*pWork = rcWork;
	}
	return TRUE;
}

// 函数: IsRectOnAnyMonitor
// 描述: 判断矩形是否与任一显示器的工作区相交
BOOL ScreenRef::IsRectOnAnyMonitor(CONST RECT& rc)
{
	MonitorProbe probe{};
	probe.pRect = &rc;
	probe.bHit = FALSE;
	EnumDisplayMonitors(NULL, NULL, ProbeMonitorProc, (LPARAM)&probe);
	return probe.bHit;
}
// 函数: GetContrlHeight
// 描述: 取控件高度基准(屏高 / 40)
DWORD ScreenRef::GetContrlHeight()
{
	return GetSystemMetrics(SM_CYSCREEN) / 40;
}
// 函数: GetFontSize
// 描述: 取字号基准(屏高 * 13 / 800)
DWORD ScreenRef::GetFontSize()
{
	return GetSystemMetrics(SM_CYSCREEN) * 13 / 800;
}
// 函数: GetSpace
// 描述: 取设计间隔基准(屏高 / 100)
DWORD ScreenRef::GetSpace()
{
	return GetSystemMetrics(SM_CYSCREEN) / 100;
}

// ================================
// 类名: AutoSize
// 描述: 根据参考大小自动调整尺寸的类 (与 ScreenRef 不同的是，可以传递经验参数)
// ================================
// 函数: CalculateSizeByRatio
// 描述: 根据比例参数计算尺寸
static AutoSize::SizeInfoDWORD CalculateSizeByRatio(DWORD dwBaseValue, const AutoSize::RatioInfo& ratioInfo)
{
	// 计算尺寸信息
	AutoSize::SizeInfoDWORD sizeInfo;
	sizeInfo.screenWidth = ScreenRef::GetScreenWidth();
	sizeInfo.screenHeight = ScreenRef::GetScreenHeight();
	sizeInfo.windowWidth = (dwBaseValue * ratioInfo.dwRatioWidth) / 1000;
	sizeInfo.windowHeight = (dwBaseValue * ratioInfo.dwRatioHeight) / 1000;
	sizeInfo.controlHeight = (dwBaseValue * ratioInfo.dwRatioControlHeight) / 1000;
	sizeInfo.fontSize = (dwBaseValue * ratioInfo.dwRatioFontSize) / 1000;
	sizeInfo.space = (dwBaseValue * ratioInfo.dwRatioSpace) / 1000;
	sizeInfo.padding = (dwBaseValue * ratioInfo.dwRatioPadding) / 1000;
	return sizeInfo;
}
// 函数: CalculateRatioBySize
// 描述: 根据尺寸参数计算比例
static AutoSize::RatioInfo CalculateRatioBySize(DWORD dwBaseValue, const AutoSize::SizeInfoDWORD& sizeInfo)
{
	AutoSize::RatioInfo ratioInfo;
	ratioInfo.dwRatioWidth = (sizeInfo.windowWidth * 1000) / dwBaseValue;
	ratioInfo.dwRatioHeight = (sizeInfo.windowHeight * 1000) / dwBaseValue;
	ratioInfo.dwRatioControlHeight = (sizeInfo.controlHeight * 1000) / dwBaseValue;
	ratioInfo.dwRatioFontSize = (sizeInfo.fontSize * 1000) / dwBaseValue;
	ratioInfo.dwRatioSpace = (sizeInfo.space * 1000) / dwBaseValue;
	ratioInfo.dwRatioPadding = (sizeInfo.padding * 1000) / dwBaseValue;
	return ratioInfo;
}
// 函数: ConvertSizeInfo
// 描述: 将 第一种 SizeInfo 类型 转换为 第二种 SizeInfo 类型
template<typename numerical_from, typename numerical_to>
static AutoSize::SizeInfo<numerical_to> ConvertSizeInfo(const AutoSize::SizeInfo<numerical_from>& sizeInfoFrom)
{
	AutoSize::SizeInfo<numerical_to> sizeInfoTo;
	sizeInfoTo.screenWidth = static_cast<numerical_to>(sizeInfoFrom.screenWidth);
	sizeInfoTo.screenHeight = static_cast<numerical_to>(sizeInfoFrom.screenHeight);
	sizeInfoTo.windowWidth = static_cast<numerical_to>(sizeInfoFrom.windowWidth);
	sizeInfoTo.windowHeight = static_cast<numerical_to>(sizeInfoFrom.windowHeight);
	sizeInfoTo.controlHeight = static_cast<numerical_to>(sizeInfoFrom.controlHeight);
	sizeInfoTo.fontSize = static_cast<numerical_to>(sizeInfoFrom.fontSize);
	sizeInfoTo.space = static_cast<numerical_to>(sizeInfoFrom.space);
	sizeInfoTo.padding = static_cast<numerical_to>(sizeInfoFrom.padding);
	return sizeInfoTo;
}
// 函数: AutoSize
// 描述: 默认构造函数: 全零占位(供按值持有 AutoSize 的结构零初始化后由 CreateBy* 赋值)
AutoSize::AutoSize()
	: sizeDWORD{}, sizeFLOAT{}, sizeMode(AUTO_SIZE_BY_SCREEN_WIDTH), dwBaseValue(0)
{
	ratioInfo = RatioInfo{};
}
// 函数: AutoSize
// 描述: 将输入尺寸保存，并自动扩展为其他尺寸类型，同时保存比例信息
AutoSize::AutoSize(const SizeInfoDWORD& sizeInfo, RatioInfo& ratioInfo, DWORD dwBaseValue, AutoSizeMode sizeMode)
{
	this->sizeDWORD = sizeInfo;
	// 计算浮点数尺寸信息
	this->sizeFLOAT = ConvertSizeInfo<DWORD, FLOAT>(sizeInfo);
	// 保存比例信息
	this->ratioInfo = CalculateRatioBySize(dwBaseValue, sizeInfo);
	this->sizeMode = sizeMode;
	this->dwBaseValue = dwBaseValue;
}
AutoSize::AutoSize(const SizeInfoFLOAT& sizeInfo, RatioInfo& ratioInfo, DWORD dwBaseValue, AutoSizeMode sizeMode)
{
	this->sizeFLOAT = sizeInfo;
	// 计算整数尺寸信息
	this->sizeDWORD = ConvertSizeInfo<FLOAT, DWORD>(sizeInfo);
	// 保存比例信息
	this->ratioInfo = CalculateRatioBySize(dwBaseValue, this->sizeDWORD);
	this->sizeMode = sizeMode;
	this->dwBaseValue = dwBaseValue;
}
// 函数: CreateByRatio
// 描述: 通过比例参数创建 AutoSize 对象
AutoSize	 AutoSize::CreateByRatio(const RatioInfo& ratioInfo, AutoSizeMode mode)
{
	// 确定参考基准数值
	DWORD dwBaseValue = 0;
	switch (mode)
	{
	case AUTO_SIZE_BY_SCREEN_WIDTH:
		dwBaseValue = ScreenRef::GetScreenWidth();
		break;
	case AUTO_SIZE_BY_SCREEN_HEIGHT:
		dwBaseValue = ScreenRef::GetScreenHeight();
		break;
	case AUTO_SIZE_BY_WINDOW_WIDTH:
		// 默认窗口宽度为屏幕宽度的 0.5 倍
		dwBaseValue = ScreenRef::GetScreenWidth() / 2;
		break;
	case AUTO_SIZE_BY_WINDOW_HEIGHT:
		// 默认窗口高度为屏幕高度的 0.5 倍
		dwBaseValue = ScreenRef::GetScreenHeight() / 2;
		break;
	default:
		dwBaseValue = ScreenRef::GetScreenWidth();
		break;
	}
	// 计算尺寸信息
	AutoSize::SizeInfoDWORD sizeInfo = CalculateSizeByRatio(dwBaseValue, ratioInfo);
	return AutoSize(sizeInfo, const_cast<RatioInfo&>(ratioInfo), dwBaseValue, mode);
}
AutoSize AutoSize::CreateByRatio(
	DWORD dwRatioWidth, DWORD dwRatioHeight,
	DWORD dwRatioControlHeight, DWORD dwRatioFontSize,
	DWORD dwRatioSpace, DWORD dwRatioPadding,
	AutoSizeMode mode)
{
	// 构造比例信息
	RatioInfo ratioInfo;
	ratioInfo.dwRatioWidth = dwRatioWidth;
	ratioInfo.dwRatioHeight = dwRatioHeight;
	ratioInfo.dwRatioControlHeight = dwRatioControlHeight;
	ratioInfo.dwRatioFontSize = dwRatioFontSize;
	ratioInfo.dwRatioSpace = dwRatioSpace;
	ratioInfo.dwRatioPadding = dwRatioPadding;
	return CreateByRatio(ratioInfo, mode);
}
// 函数: CreateByFixedSize
// 描述: 通过固定尺寸参数创建 AutoSize 对象 (但是需要指定 sizeMode)
AutoSize AutoSize::CreateByFixedSize(const SizeInfoDWORD& sizeInfo, AutoSizeMode mode)
{
	// 确定参考基准数值
	DWORD dwBaseValue = 0;
	switch (mode)
	{
	case AUTO_SIZE_BY_SCREEN_WIDTH:
		dwBaseValue = ScreenRef::GetScreenWidth();
		break;
	case AUTO_SIZE_BY_SCREEN_HEIGHT:
		dwBaseValue = ScreenRef::GetScreenHeight();
		break;
	case AUTO_SIZE_BY_WINDOW_WIDTH:
		dwBaseValue = sizeInfo.windowWidth;
		break;
	case AUTO_SIZE_BY_WINDOW_HEIGHT:
		dwBaseValue = sizeInfo.windowHeight;
		break;
	default:
		dwBaseValue = ScreenRef::GetScreenWidth();
		break;
	}
	// 计算比例信息
	RatioInfo ratioInfo = CalculateRatioBySize(dwBaseValue, sizeInfo);
	return AutoSize(sizeInfo, ratioInfo, dwBaseValue, mode);
}
AutoSize AutoSize::CreateByFixedSize(
	DWORD dwWindowWidth, DWORD dwWindowHeight,
	DWORD dwControlHeight, DWORD dwFontSize,
	DWORD dwSpace, DWORD dwPadding,
	AutoSizeMode mode)
{
	// 构造尺寸信息
	SizeInfoDWORD sizeInfo;
	sizeInfo.screenWidth = ScreenRef::GetScreenWidth();
	sizeInfo.screenHeight = ScreenRef::GetScreenHeight();
	sizeInfo.windowWidth = dwWindowWidth;
	sizeInfo.windowHeight = dwWindowHeight;
	sizeInfo.controlHeight = dwControlHeight;
	sizeInfo.fontSize = dwFontSize;
	sizeInfo.space = dwSpace;
	sizeInfo.padding = dwPadding;
	return CreateByFixedSize(sizeInfo, mode);
}

// 函数: SetWindowRect
// 描述: 设置窗口矩形 (用于更新窗口宽度和高度)
BOOL AutoSize::SetWindowRect(DWORD dwWindowWidth, DWORD dwWindowHeight)
{
	// 仅在基于窗口尺寸的模式下有效
	if (sizeMode != AUTO_SIZE_BY_WINDOW_WIDTH && sizeMode != AUTO_SIZE_BY_WINDOW_HEIGHT)
	{
		return FALSE;
	}
	// 更新窗口尺寸
	sizeDWORD.windowWidth = dwWindowWidth;
	sizeDWORD.windowHeight = dwWindowHeight;
	sizeFLOAT.windowWidth = static_cast<FLOAT>(dwWindowWidth);
	sizeFLOAT.windowHeight = static_cast<FLOAT>(dwWindowHeight);
	// 重新计算参考基准数值
	if (sizeMode == AUTO_SIZE_BY_WINDOW_WIDTH)
	{
		dwBaseValue = dwWindowWidth;
	}
	else
	{
		dwBaseValue = dwWindowHeight;
	}
	// 重新计算其他尺寸信息
	sizeDWORD = CalculateSizeByRatio(dwBaseValue, ratioInfo);
	sizeFLOAT = ConvertSizeInfo<DWORD, FLOAT>(sizeDWORD);
	return TRUE;
}
// 函数: SetRatio
// 描述: 设置比例参数 (用于更新比例参数)
BOOL AutoSize::SetRatio(const RatioInfo& ratioInfo)
{
	// 更新比例信息
	this->ratioInfo = ratioInfo;
	// 重新计算尺寸信息
	sizeDWORD = CalculateSizeByRatio(dwBaseValue, ratioInfo);
	sizeFLOAT = ConvertSizeInfo<DWORD, FLOAT>(sizeDWORD);
	return TRUE;
}
BOOL AutoSize::SetRatio(
	DWORD dwRatioWidth, DWORD dwRatioHeight,
	DWORD dwRatioControlHeight, DWORD dwRatioFontSize,
	DWORD dwRatioSpace, DWORD dwRatioPadding)
{
	// 构造比例信息
	RatioInfo ratioInfo;
	ratioInfo.dwRatioWidth = dwRatioWidth;
	ratioInfo.dwRatioHeight = dwRatioHeight;
	ratioInfo.dwRatioControlHeight = dwRatioControlHeight;
	ratioInfo.dwRatioFontSize = dwRatioFontSize;
	ratioInfo.dwRatioSpace = dwRatioSpace;
	ratioInfo.dwRatioPadding = dwRatioPadding;
	return SetRatio(ratioInfo);
}
// 函数: SetSize
// 描述: 设置固定尺寸参数 (用于更新固定尺寸参数)
BOOL AutoSize::SetSize(const SizeInfoDWORD& sizeInfo)
{
	// 更新尺寸信息
	this->sizeDWORD = sizeInfo;
	// 重新计算比例信息
	this->ratioInfo = CalculateRatioBySize(dwBaseValue, sizeInfo);
	// 计算浮点数尺寸信息
	this->sizeFLOAT = ConvertSizeInfo<DWORD, FLOAT>(sizeInfo);
	return TRUE;
}
BOOL AutoSize::SetSize(const SizeInfoFLOAT& sizeInfo)
{
	// 更新尺寸信息
	this->sizeFLOAT = sizeInfo;
	// 计算整数尺寸信息
	this->sizeDWORD = ConvertSizeInfo<FLOAT, DWORD>(sizeInfo);
	// 重新计算比例信息
	this->ratioInfo = CalculateRatioBySize(dwBaseValue, this->sizeDWORD);
	return TRUE;
}
// 模板实例化声明区 —— 本区所有模板函数(声明见 BasicDesign.h)只显式实例化两个常用类型 DWORD / FLOAT

template<typename numerical>
CONST AutoSize::SizeInfo<numerical>& AutoSize::GetSizeInfoRef() const
{
	static AutoSize::SizeInfo<numerical> defaultSizeInfo;
	defaultSizeInfo = ConvertSizeInfo<DWORD, numerical>(sizeDWORD);
	return defaultSizeInfo;
}
// 模板特例化: 获取 DWORD 类型尺寸信息引用
template<>
CONST AutoSize::SizeInfo<DWORD>& AutoSize::GetSizeInfoRef<DWORD>() const
{
	return sizeDWORD;
}
// 模板特例化: 获取 FLOAT 类型尺寸信息引用
template<>
CONST AutoSize::SizeInfo<FLOAT>& AutoSize::GetSizeInfoRef<FLOAT>() const
{
	return sizeFLOAT;
}

// 函数: GetSizeInfo
// 描述: 获取尺寸信息
template<typename numerical>
AutoSize::SizeInfo<numerical> AutoSize::GetSizeInfo() const
{
	return GetSizeInfoRef<numerical>();
}
// 函数: GetScreenWidth
// 描述: 获取屏幕宽度
template<typename numerical>
numerical AutoSize::GetScreenWidth() const
{
	return GetSizeInfoRef<numerical>().screenWidth;
}
// 函数: GetScreenHeight
// 描述: 获取屏幕高度
template<typename numerical>
numerical AutoSize::GetScreenHeight() const
{
	return GetSizeInfoRef<numerical>().screenHeight;
}
// 函数: GetWindowWidth
// 描述: 获取窗口宽度
template<typename numerical>
numerical AutoSize::GetWindowWidth() const
{
	return GetSizeInfoRef<numerical>().windowWidth;
}
// 函数: GetWindowHeight
// 描述: 获取窗口高度
template<typename numerical>
numerical AutoSize::GetWindowHeight() const
{
	return GetSizeInfoRef<numerical>().windowHeight;
}
// 函数: GetControlHeight
// 描述: 获取控件高度
template<typename numerical>
numerical AutoSize::GetControlHeight() const
{
	return GetSizeInfoRef<numerical>().controlHeight;
}
// 函数: GetFontSize
// 描述: 获取字体大小
template<typename numerical>
numerical AutoSize::GetFontSize() const
{
	return GetSizeInfoRef<numerical>().fontSize;
}
// 函数: GetSpace
// 描述: 获取设计间隔
template<typename numerical>
numerical AutoSize::GetSpace() const
{
	return GetSizeInfoRef<numerical>().space;
}
// 函数: GetPadding
// 描述: 获取设计内边距
template<typename numerical>
numerical AutoSize::GetPadding() const
{
	return GetSizeInfoRef<numerical>().padding;
}

// 模板实例化声明
template AutoSize::SizeInfo<DWORD> AutoSize::GetSizeInfo<DWORD>() const;
template AutoSize::SizeInfo<FLOAT> AutoSize::GetSizeInfo<FLOAT>() const;
template DWORD AutoSize::GetScreenWidth<DWORD>() const;
template FLOAT AutoSize::GetScreenWidth<FLOAT>() const;
template DWORD AutoSize::GetScreenHeight<DWORD>() const;
template FLOAT AutoSize::GetScreenHeight<FLOAT>() const;
template DWORD AutoSize::GetWindowWidth<DWORD>() const;
template FLOAT AutoSize::GetWindowWidth<FLOAT>() const;
template DWORD AutoSize::GetWindowHeight<DWORD>() const;
template FLOAT AutoSize::GetWindowHeight<FLOAT>() const;
template DWORD AutoSize::GetControlHeight<DWORD>() const;
template FLOAT AutoSize::GetControlHeight<FLOAT>() const;
template DWORD AutoSize::GetFontSize<DWORD>() const;
template FLOAT AutoSize::GetFontSize<FLOAT>() const;
template DWORD AutoSize::GetSpace<DWORD>() const;
template FLOAT AutoSize::GetSpace<FLOAT>() const;
template DWORD AutoSize::GetPadding<DWORD>() const;
template FLOAT AutoSize::GetPadding<FLOAT>() const;

// ================================
// 文件作用域状态: 分配器(一次性静态注入; 仿 Layout/LayoutModel 的同源注入口径)
// 说明: 轴位置表是"不定生命周期的小对象" ⇒ 首选小对象池; 超出池上限时回落线性分配器
//       (回落的表无单块回收接口, 随该分配器整体释放 —— 与 LM-1 的回落项同性质)
// ================================
static Arena* pMemArena = NULL;		// 线性分配器(表回落用)
static MemPool* pMemPool = NULL;	// 小对象池(表的首选来源)

// ================================
// 类名: ProjCR
// 描述: 投影颜色实现(轴位置表 / 槽位派生 / 边框 / 工厂与内存; 声明见 BasicDesign.h)
// ================================

// 函数: ProjCR
// 描述: 私有构造(默认口径: 线性均匀采样 + 两个派生强度的默认值)
ProjCR::ProjCR()
{
	crBase = 0;
	crLight = 0;
	eDirection = PROJ_DARK_TO_LIGHT;
	dwLayers = 0;
	fContrastStrength = DEFAULT_CONTRAST_STRENGTH;
	fBorderStrength = DEFAULT_BORDER_STRENGTH;
	pfnSample = NULL;
	layers = BufferInfo{ nullptr, 0 };
}

// 函数: ~ProjCR
// 描述: 私有析构: 归还轴位置表(不归还对象本体 —— 由 Release 释放)
ProjCR::~ProjCR()
{
	FreeLayers(layers);
	layers = BufferInfo{ nullptr, 0 };
}

// 函数: SetAllocators
// 描述: 一次性静态注入两个分配器(全进程一份)
VOID ProjCR::SetAllocators(Arena* arena, MemPool* pool)
{
	pMemArena = arena;
	pMemPool = pool;
}

// 函数: IsSampleFuncValid
// 描述: 逐层调用校验采样函数: ①每个返回值 ∈ [0,1] ②序列非严格单调(不得既升又降)
BOOL ProjCR::IsSampleFuncValid(SampleFunc pfnSample, DWORD dwLayers)
{
	// 变量声明区
	BOOL bAsc = FALSE;		// 序列中存在上升步
	BOOL bDesc = FALSE;		// 序列中存在下降步
	FLOAT fPrev = 0.0f;		// 上一层返回值
	// 1. 线性均匀采样必然单调且在值域内 ⇒ 直接通过
	if (pfnSample == NULL)
	{
		return TRUE;
	}
	// 2. 层数不足两层时无有效参数点集(构造侧已单独拒绝, 此处防御)
	if (dwLayers < 2)
	{
		return FALSE;
	}
	// 3. 逐层调用: 先判值域, 再记升降步
	for (DWORD k = 1; k <= dwLayers; k++)
	{
		CONST FLOAT fValue = pfnSample(k, dwLayers);
		// 值域判据: 必须落在 [0,1](NaN 亦在此被拒 —— 两个比较皆为假)
		if (!(fValue >= 0.0f && fValue <= 1.0f))
		{
			return FALSE;
		}
		if (k > 1)
		{
			if (fValue > fPrev)
			{
				bAsc = TRUE;
			}
			else if (fValue < fPrev)
			{
				bDesc = TRUE;
			}
		}
		fPrev = fValue;
	}
	// 4. 单调判据: 既升又降 = 振荡 ⇒ 拒绝(允许相等 = 非严格)
	if (bAsc && bDesc)
	{
		return FALSE;
	}
	return TRUE;
}

// 函数: PushToFarEnd
// 描述: 远端法: 把轴位置朝"离它更远的那一端"推进 ρ 比例
FLOAT ProjCR::PushToFarEnd(FLOAT fAxis, FLOAT fStrength)
{
	// 变量声明区(远端端点: 轴位置 >= 0.5 者离底色端(0)更远; 平局固定取底色端)
	CONST FLOAT fFar = (fAxis >= 0.5f) ? 0.0f : 1.0f;
	return fAxis + (fFar - fAxis) * fStrength;
}

// 函数: AxisToColor
// 描述: 轴位置 → 颜色(按底色端/光色端逐字节线性插值)
DWORD ProjCR::AxisToColor(FLOAT fAxis) const
{
	// 变量声明区
	DWORD dwRet = 0;
	// 逐字节插值: 同字节位置之间插值 ⇒ 与 ARGB / RGBA 的命名口径无关(含 α 字节)
	for (INT i = 0; i < 4; i++)
	{
		CONST DWORD dwShift = (DWORD)(i * 8);
		CONST FLOAT fBase = (FLOAT)((crBase >> dwShift) & 0xFFu);
		CONST FLOAT fLight = (FLOAT)((crLight >> dwShift) & 0xFFu);
		FLOAT fMix = fBase + (fLight - fBase) * fAxis;
		if (fMix < 0.0f)
		{
			fMix = 0.0f;
		}
		if (fMix > 255.0f)
		{
			fMix = 255.0f;
		}
		CONST DWORD dwByte = (DWORD)(fMix + 0.5f);
		dwRet |= ((dwByte & 0xFFu) << dwShift);
	}
	return dwRet;
}

// 函数: BlendColor
// 描述: 两色逐字节线性插值(比例 0 = 前者; 1 = 后者; 与 AxisToColor 同口径, 含 α 字节)
DWORD ProjCR::BlendColor(DWORD crFrom, DWORD crTo, FLOAT fRatio)
{
	// 变量声明区
	DWORD dwRet = 0;
	// 逐字节插值: 同字节位置之间插值 ⇒ 与 ARGB / RGBA 的命名口径无关(含 α 字节)
	for (INT i = 0; i < 4; i++)
	{
		CONST DWORD dwShift = (DWORD)(i * 8);
		CONST FLOAT fA = (FLOAT)((crFrom >> dwShift) & 0xFFu);
		CONST FLOAT fB = (FLOAT)((crTo >> dwShift) & 0xFFu);
		FLOAT fMix = fA + (fB - fA) * fRatio;
		if (fMix < 0.0f)
		{
			fMix = 0.0f;
		}
		if (fMix > 255.0f)
		{
			fMix = 255.0f;
		}
		CONST DWORD dwByte = (DWORD)(fMix + 0.5f);
		dwRet |= ((dwByte & 0xFFu) << dwShift);
	}
	return dwRet;
}

// 函数: AllocLayers
// 描述: 按层数分配轴位置表(小对象池优先, 超出池上限回落线性分配器)
BufferInfo ProjCR::AllocLayers(DWORD dwLayers)
{
	// 变量声明区
	BufferInfo info{ nullptr, 0 };
	CONST size_t qwSize = sizeof(FLOAT) * (size_t)dwLayers;
	if (pMemPool != NULL && qwSize <= MemPool::MAX_SMALL_SIZE)
	{
		info = pMemPool->Allocate(qwSize);
	}
	else if (pMemArena != NULL)
	{
		info = pMemArena->Allocate(qwSize);
	}
	return info;
}

// 函数: FreeLayers
// 描述: 归还轴位置表(仅池分配者可单独回收)
VOID ProjCR::FreeLayers(BufferInfo info)
{
	if (info.buffer == NULL)
	{
		return;
	}
	if (pMemPool != NULL && pMemPool->Contains(info.buffer) == TRUE)
	{
		pMemPool->Free(info);
	}
}

// 函数: RebuildLayers
// 描述: 重算轴位置表(底色/光色/方向/层数/采样函数任一变更后调用)
VOID ProjCR::RebuildLayers()
{
	// 变量声明区
	FLOAT* pAxis = NULL;
	// 表未就绪或层数非法 ⇒ 无表可算
	if (layers.buffer == NULL || dwLayers < 2)
	{
		return;
	}
	pAxis = (FLOAT*)layers.buffer;
	for (DWORD k = 1; k <= dwLayers; k++)
	{
		// 方向反射: 在喂给采样函数之前完成(暗到亮: 层号即参数 / 亮到暗: 参数 = 层数 + 1 − 层号)
		CONST DWORD dwParam = (eDirection == PROJ_DARK_TO_LIGHT) ? k : (dwLayers + 1 - k);
		FLOAT fAxis = 0.0f;
		if (pfnSample != NULL)
		{
			fAxis = pfnSample(dwParam, dwLayers);
		}
		else
		{
			fAxis = (FLOAT)(dwParam - 1) / (FLOAT)(dwLayers - 1);
		}
		pAxis[k - 1] = fAxis;
	}
}

// 函数: Create
// 描述: 工厂构造(先校验再分配表并构造; 任一失败即不构造)
BOOL ProjCR::Create(ProjCR*& pOut, DWORD crBase, DWORD crLight, SAMPLE_DIRECTION eDirection,
	DWORD dwLayers, SampleFunc pfnSample)
{
	// 变量声明区
	BOOL bRet = FALSE;
	ProjCR* pObj = NULL;
	BufferInfo info{ nullptr, 0 };
	pOut = NULL;
	// 1. 层数校验: 层间差值需要至少两层 ⇒ 不足即阻止构造
	if (dwLayers < 2)
	{
		return bRet;
	}
	// 2. 采样函数校验(逐层调用): 失败即阻止构造
	if (IsSampleFuncValid(pfnSample, dwLayers) == FALSE)
	{
		return bRet;
	}
	// 3. 分配轴位置表
	info = AllocLayers(dwLayers);
	if (info.buffer == NULL)
	{
		return bRet;
	}
	// 4. 构造对象并落初值(校验已过 ⇒ 无需再判)
	pObj = new ProjCR();
	if (pObj == NULL)
	{
		FreeLayers(info);
		return bRet;
	}
	pObj->crBase = crBase;
	pObj->crLight = crLight;
	pObj->eDirection = eDirection;
	pObj->dwLayers = dwLayers;
	pObj->pfnSample = pfnSample;
	pObj->layers = info;
	pObj->RebuildLayers();
	pOut = pObj;
	bRet = TRUE;
	return bRet;
}

// 函数: Release
// 描述: 与 Create 成对: 归还表并释放对象本体
VOID ProjCR::Release(ProjCR*& pOut)
{
	if (pOut == NULL)
	{
		return;
	}
	delete pOut;		// 析构内归还轴位置表
	pOut = NULL;
}

// 函数: Sample
// 描述: 取某层的槽位色(取色正门)
DWORD ProjCR::Sample(DWORD dwLayer, CR_SLOT eSlot) const
{
	// 变量声明区
	CONST FLOAT* pAxis = NULL;
	DWORD dwNearLayer = dwLayer;		// 相邻层的绘图层号(悬停 = 本层 +1 / 激活 = 本层 −1)
	FLOAT fAxis = 0.0f;
	DWORD crSurf = 0;					// 本层表面色
	DWORD crNear = 0;					// 相邻层表面色
	// 未构造成功(工厂被阻止 / 表分配失败) ⇒ 返回 0(与 IsInvalid 口径一致;
	// 这不是"层号边界处理" —— 层号越界仍属调用方违约, 本类不做处理)
	if (layers.buffer == NULL)
	{
		return 0;
	}
	pAxis = (CONST FLOAT*)layers.buffer;
	fAxis = pAxis[dwLayer - 1];							// 层号合法域由调用方保证(本类不做边界处理)
	// 对比槽: 远端法(轴位置空间); 其余非附属槽位: 本层表面色
	if (eSlot == PROJ_SLOT_CONTRAST)
	{
		return AxisToColor(PushToFarEnd(fAxis, fContrastStrength));
	}
	if (eSlot != PROJ_SLOT_HOVER && eSlot != PROJ_SLOT_ACTIVE)
	{
		return AxisToColor(fAxis);
	}
	// 悬停 / 激活: 相邻层取绘图层号的 ±1(其轴位置由表给出, 方向反射已在建表时完成)
	dwNearLayer = (eSlot == PROJ_SLOT_HOVER) ? (dwLayer + 1) : (dwLayer - 1);
	// 端点层缺该侧相邻层(顶层 +1 / 底层 −1) ⇒ 取回本层 ⇒ 两者色差为 0
	if (dwNearLayer < 1)
	{
		dwNearLayer = 1;
	}
	if (dwNearLayer > dwLayers)
	{
		dwNearLayer = dwLayers;
	}
	// 先取两层表面色(各自取整), 再按两者色差的比例混合
	crSurf = AxisToColor(fAxis);
	crNear = AxisToColor(pAxis[dwNearLayer - 1]);
	return BlendColor(crSurf, crNear, SLOT_OFFSET_RATIO);
}

// 函数: SampleBorder
// 描述: 取两个绘图层之间的边框色(表面色取中后复用对比槽的远端法)
DWORD ProjCR::SampleBorder(DWORD dwLayerA, DWORD dwLayerB) const
{
	// 变量声明区
	CONST FLOAT* pAxis = NULL;
	FLOAT fMix = 0.0f;
	// 未构造成功 ⇒ 返回 0(同 Sample 说明)
	if (layers.buffer == NULL)
	{
		return 0;
	}
	pAxis = (CONST FLOAT*)layers.buffer;
	// 取中在轴位置空间进行 ⇒ 参数天然对称(与两层的先后次序无关)
	fMix = (pAxis[dwLayerA - 1] + pAxis[dwLayerB - 1]) * 0.5f;
	return AxisToColor(PushToFarEnd(fMix, fBorderStrength));
}

// 函数: SetBase
// 描述: 设置底色端(重算表)
VOID ProjCR::SetBase(DWORD crBase)
{
	this->crBase = crBase;
	RebuildLayers();
}

// 函数: SetLight
// 描述: 设置光色端(重算表)
VOID ProjCR::SetLight(DWORD crLight)
{
	this->crLight = crLight;
	RebuildLayers();
}

// 函数: SetDirection
// 描述: 设置采样方向(重算表; 亮暗模式切换入口)
VOID ProjCR::SetDirection(SAMPLE_DIRECTION eDirection)
{
	this->eDirection = eDirection;
	RebuildLayers();
}

// 函数: SetLayers
// 描述: 设置层数(先重校验采样函数, 再换块并重算; 失败则沿用旧值)
BOOL ProjCR::SetLayers(DWORD dwLayers)
{
	// 变量声明区
	BufferInfo info{ nullptr, 0 };
	if (dwLayers < 2)
	{
		return FALSE;
	}
	// 新层数改变了采样函数的被校验点集 ⇒ 必须先按新层数重校验(失败则旧值原样保留)
	if (IsSampleFuncValid(pfnSample, dwLayers) == FALSE)
	{
		return FALSE;
	}
	// 先申请新表: 申请失败也不破坏旧状态
	info = AllocLayers(dwLayers);
	if (info.buffer == NULL)
	{
		return FALSE;
	}
	FreeLayers(this->layers);
	this->layers = info;
	this->dwLayers = dwLayers;
	RebuildLayers();
	return TRUE;
}

// 函数: SetSampleFunc
// 描述: 设置单调采样函数(校验失败则沿用旧值)
BOOL ProjCR::SetSampleFunc(SampleFunc pfnSample)
{
	if (IsSampleFuncValid(pfnSample, dwLayers) == FALSE)
	{
		return FALSE;
	}
	this->pfnSample = pfnSample;
	RebuildLayers();
	return TRUE;
}

// 函数: SetContrastStrength
// 描述: 设置对比槽强度 ρ(钳到 0..1)
VOID ProjCR::SetContrastStrength(FLOAT fStrength)
{
	if (fStrength < 0.0f)
	{
		fStrength = 0.0f;
	}
	if (fStrength > 1.0f)
	{
		fStrength = 1.0f;
	}
	fContrastStrength = fStrength;
}

// 函数: SetBorderStrength
// 描述: 设置边框强度 ρ(钳到 0..1)
VOID ProjCR::SetBorderStrength(FLOAT fStrength)
{
	if (fStrength < 0.0f)
	{
		fStrength = 0.0f;
	}
	if (fStrength > 1.0f)
	{
		fStrength = 1.0f;
	}
	fBorderStrength = fStrength;
}

// 函数: GetBase
// 描述: 取底色端
DWORD ProjCR::GetBase() const
{
	return crBase;
}

// 函数: GetLight
// 描述: 取光色端
DWORD ProjCR::GetLight() const
{
	return crLight;
}

// 函数: GetDirection
// 描述: 取采样方向
ProjCR::SAMPLE_DIRECTION ProjCR::GetDirection() const
{
	return eDirection;
}

// 函数: GetLayers
// 描述: 取层数
DWORD ProjCR::GetLayers() const
{
	return dwLayers;
}

// 函数: GetContrastStrength
// 描述: 取对比槽强度
FLOAT ProjCR::GetContrastStrength() const
{
	return fContrastStrength;
}

// 函数: GetBorderStrength
// 描述: 取边框强度
FLOAT ProjCR::GetBorderStrength() const
{
	return fBorderStrength;
}

// 函数: GetSampleFunc
// 描述: 取采样函数
ProjCR::SampleFunc ProjCR::GetSampleFunc() const
{
	return pfnSample;
}

// 函数: GetAxis
// 描述: 取某层的轴位置(层号合法域由调用方保证)
FLOAT ProjCR::GetAxis(DWORD dwLayer) const
{
	CONST FLOAT* pAxis = NULL;
	// 未构造成功 ⇒ 返回 0(同 Sample 说明)
	if (layers.buffer == NULL)
	{
		return 0.0f;
	}
	pAxis = (CONST FLOAT*)layers.buffer;
	return pAxis[dwLayer - 1];
}

// 函数: IsInvalid
// 描述: 取无效标志(未构造成功者恒 TRUE)
BOOL ProjCR::IsInvalid() const
{
	return (layers.buffer == NULL) ? TRUE : FALSE;
}