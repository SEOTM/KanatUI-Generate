// ================================
// 文件: GeometrySolver.cpp
// 描述: 几何解算与参考量换算的实现(口径与迁出动因见 GeometrySolver.h 的文件头说明)
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include "framework/layout/GeometrySolver.h"
#include <windows.h>		// RECT 体系与 GetTickCount64(随机落点的随机源播种)
#include <stdlib.h>			// rand / srand(随机落点)
#include <string.h>			// strcmp(AutoModeIndex 的模式串匹配)

// ================================
// 区域: 自动尺寸模式名 ↔ 枚举
// ================================
// 函数: AutoModeNameOf
// 描述: 取自动尺寸模式枚举对应的规范名(越界返回屏高模式名)
LPCSTR GeometrySolver::AutoModeNameOf(INT mode)
{
	static const LPCSTR Names[4] = {
		"AUTO_SIZE_BY_SCREEN_WIDTH",	// 0: 屏宽
		"AUTO_SIZE_BY_SCREEN_HEIGHT",	// 1: 屏高
		"AUTO_SIZE_BY_WINDOW_WIDTH",	// 2: 窗宽
		"AUTO_SIZE_BY_WINDOW_HEIGHT"	// 3: 窗高
	};
	if (mode < 0 || mode > 3)
	{
		mode = AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT;
	}
	return Names[mode];
}
// 函数: AutoModeIndex
// 描述: 取自动尺寸模式串对应的枚举索引(未命中/NULL ⇒ 屏高模式)
INT GeometrySolver::AutoModeIndex(LPCSTR mode)
{
	if (mode == NULL)
	{
		return AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT;
	}
	for (INT m = 0; m < 4; m++)
	{
		if (strcmp(mode, AutoModeNameOf(m)) == 0)
		{
			return m;
		}
	}
	return AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT;
}

// ================================
// 区域: 参考量 / 基准换算
// ================================
// 函数: EntryBasePx
// 描述: 取布局 autosize 的换算基准像素(见 .h; WINDOW_* 模式按屏兜底)
DWORD GeometrySolver::EntryBasePx(INT mode)
{
	switch (mode)
	{
	case AutoSize::AUTO_SIZE_BY_SCREEN_WIDTH:	return ScreenRef::GetScreenWidth();
	case AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT:	return ScreenRef::GetScreenHeight();
	case AutoSize::AUTO_SIZE_BY_WINDOW_WIDTH:	return ScreenRef::GetScreenWidth();
	case AutoSize::AUTO_SIZE_BY_WINDOW_HEIGHT:	return ScreenRef::GetScreenHeight();
	default:									return ScreenRef::GetScreenHeight();
	}
}
// 函数: EntryRefSizeInfo
// 描述: 依参考尺寸/经验参数推导各参考量像素值(窗口化: 布局窗口像素与控件量同源推导)
AutoSize::SizeInfo<DWORD> GeometrySolver::EntryRefSizeInfo(CONST RefSize& refSize, DWORD base, DWORD wndW, DWORD wndH)
{
	AutoSize::SizeInfo<DWORD> si{};
	si.screenWidth = ScreenRef::GetScreenWidth();
	si.screenHeight = ScreenRef::GetScreenHeight();
	// 窗口宽/高 = 该布局窗口客户区像素(预览窗口/宿主窗口的"真实大小")
	si.windowWidth = (wndW > 0) ? wndW : si.screenWidth;
	si.windowHeight = (wndH > 0) ? wndH : si.screenHeight;
	if (base < 1) { base = 1; }
	// 四项经验值像素 = 基准 x 千分比 / 1000(至少 1px)
	si.controlHeight = (DWORD)((unsigned __int64)base * refSize.controlHeight / 1000); if (si.controlHeight < 1) { si.controlHeight = 1; }
	si.fontSize = (DWORD)((unsigned __int64)base * refSize.fontSize / 1000); if (si.fontSize < 1) { si.fontSize = 1; }
	si.space = (DWORD)((unsigned __int64)base * refSize.space / 1000); if (si.space < 1) { si.space = 1; }
	si.padding = (DWORD)((unsigned __int64)base * refSize.padding / 1000); if (si.padding < 1) { si.padding = 1; }
	return si;
}
// 函数: RefValueToRatio
// 描述: 把"该条目单位下的参考量输入值"折算为中间态存储的千分比
DWORD GeometrySolver::RefValueToRatio(DWORD value, DWORD base, INT unit)
{
	DWORD v = value;
	if (unit != 0)
	{
		if (base < 1) { base = 1; }
		v = (DWORD)(((unsigned __int64)value * 1000 + (base / 2)) / base);	// 像素 -> 千分比(四舍五入)
	}
	if (v > 10000) { v = 10000; }
	return v;
}

// ================================
// 区域: 像素几何解算
// ================================
// 函数: FactorAbsPx
// 描述: 由控件几何因子 + 参考尺寸求像素: Σ(coeff[i]*ref[i]) + offset
FLOAT GeometrySolver::FactorAbsPx(CONST SizeFactor& f, CONST AutoSize::SizeInfo<FLOAT>& ref)
{
	CONST FLOAT Refs[GENFACTOR_NUM] = {
		ref.screenWidth, ref.screenHeight, ref.windowWidth, ref.windowHeight,
		ref.controlHeight, ref.fontSize, ref.space, ref.padding
	};
	DOUBLE sum = f.offset;
	for (int i = 0; i < GENFACTOR_NUM; i++)
	{
		sum += (DOUBLE)f.coeff.RefCoeffs[i] * Refs[i];
	}
	return (FLOAT)sum;
}
// 函数: ApplySpecGeometry
// 描述: 依中间态元素规格与参考尺寸就地解算一枚控件的像素几何
VOID GeometrySolver::ApplySpecGeometry(CtrlInfo& info, CONST ElementSpec* pSpec, CONST AutoSize::SizeInfo<FLOAT>& ref)
{
	if (pSpec == NULL)
	{
		return;
	}
	info.posDeclared.x = FactorAbsPx(pSpec->x, ref);
	info.posDeclared.y = FactorAbsPx(pSpec->y, ref);
	info.size.x = FactorAbsPx(pSpec->width, ref);
	info.size.y = FactorAbsPx(pSpec->height, ref);
	// 有效坐标: 解算当下先取声明值(结构件即其最终坐标; 内容控件由随后的坐标推导按父链累加,
	// 见 Layout::DeriveCtrlPositions) —— 解算后立刻读 position 的调用方拿到的值与声明值一致
	info.position = info.posDeclared;
}

// ================================
// 区域: 窗口位置解算
// 说明: 窗口位置声明 → 位置记录的唯一落点(见 GeometrySolver.h 的 ResolveWndPos)
// ================================

// 常量: WND_POS_RANDOM
// 描述: 位置声明里的"随机位置"哨兵(x 与 y 同时为该值 ⇒ 由解析期在主显示器工作区内随机落点)
constexpr FLOAT WND_POS_RANDOM = -1.0f;

// 函数: RandRange
// 描述: 取 [lo, hi] 内的均匀随机整数(区间为空/倒置 ⇒ 返回 lo)
static INT RandRange(INT lo, INT hi)
{
	if (hi <= lo)
	{
		return lo;
	}
	return lo + (INT)((unsigned)rand() % (unsigned)(hi - lo + 1));
}

// 函数: MakeRandomWndPos
// 描述: 在主显示器工作区内取一枚随机落点(窗口完整落在工作区内)
static vec2 MakeRandomWndPos(CONST vec2& size)
{
	static BOOL bSeeded = FALSE;		// 随机源是否已播种(进程内一次)
	if (bSeeded == FALSE)
	{
		srand((unsigned int)GetTickCount64());
		bSeeded = TRUE;
	}
	RECT rcWork{};
	if (ScreenRef::GetPrimaryWorkArea(&rcWork) == FALSE)
	{
		return vec2(0.0f, 0.0f);
	}
	// 落点范围 = [工作区左, 工作区右 - 窗口宽] x [工作区上, 工作区下 - 窗口高]
	// (窗口尺寸不小于工作区时区间倒置 ⇒ RandRange 夹到工作区左上角)
	CONST INT nX = RandRange((INT)rcWork.left, (INT)rcWork.right - (INT)size.x);
	CONST INT nY = RandRange((INT)rcWork.top, (INT)rcWork.bottom - (INT)size.y);
	return vec2((FLOAT)nX, (FLOAT)nY);
}

// 函数: ResolveWndPos
// 描述: 解算窗口件的位置记录(解析期几何解算的最后一步; 三分支)
VOID GeometrySolver::ResolveWndPos(CtrlInfo* pWnd)
{
	if (pWnd == NULL)
	{
		return;
	}
	// 1. 声明为"随机位置" ⇒ 直接随机落点
	if (pWnd->position.x == WND_POS_RANDOM && pWnd->position.y == WND_POS_RANDOM)
	{
		pWnd->position = MakeRandomWndPos(pWnd->size);
	}
	else
	{
		// 2. 声明位置能落在当前屏幕上 ⇒ 直接采用(部分可见即算可见: 多屏交界处不强行归位)
		RECT rcWnd{};
		rcWnd.left = (LONG)pWnd->position.x;
		rcWnd.top = (LONG)pWnd->position.y;
		rcWnd.right = rcWnd.left + (LONG)pWnd->size.x;
		rcWnd.bottom = rcWnd.top + (LONG)pWnd->size.y;
		// 3. 声明位置落在所有屏幕之外(显示器配置变化等) ⇒ 降级为随机位置
		if (ScreenRef::IsRectOnAnyMonitor(rcWnd) == FALSE)
		{
			pWnd->position = MakeRandomWndPos(pWnd->size);
		}
	}
	// 声明坐标与位置记录同步(结构件不参与父链累加, 但两者保持一致: 重推坐标时不把窗口位置写回旧值)
	pWnd->posDeclared = pWnd->position;
}
