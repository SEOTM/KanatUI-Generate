// ================================
// 文件: AppEnv.h
// 描述: 应用环境件 —— 与控制台 / 系统设置 / 窗口外观相关的无状态通用原语。
// ================================
#pragma once
#include <windows.h>

// ================================
// 命名空间: AppEnv
// 描述: 应用环境原语的读写(无状态)
// ================================
namespace AppEnv
{
	// 函数: InitConsoleUtf8
	// 描述: 初始化 UTF-8 控制台环境(输入输出代码页均设为 UTF-8)
	// 说明: 进程入口首件事 —— 源码以 /utf-8 编译, 中文诊断经 stdout 直出时须与代码页一致
	VOID InitConsoleUtf8();

	// 函数: IsSystemDarkMode
	// 描述: 读取系统深色模式设置
	// 返回:
	//		bool: TRUE = 系统当前为深色(应用)模式; FALSE = 浅色 / 读不到该设置
	// 说明: 读 HKCU\...\Themes\Personalize 的 AppsUseLightTheme(0 = 深色); 键或值不可用 ⇒ FALSE
	bool IsSystemDarkMode();

	// 函数: ApplyWindowCornerRounding
	// 描述: 设置窗口圆角 —— ①优先 DWM 圆角属性(DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND);
	//       ②该属性不可用(HRESULT 失败, 如系统不支持)时回退为"按圆角矩形区域裁剪窗口"
	//       (SetWindowRgn + CreateRoundRectRgn, 半径 = 传入的设计间隔)
	// 参数:
	//		IN	hWnd:	目标窗口句柄
	//		IN	w/h:	窗口外框尺寸(px; 窗口为 WS_POPUP 无边框 ⇒ 外框即客户区)
	//		IN	space:	圆角半径(px)
	//		OUT	pbByRgn:	TRUE = 本次由 RGN 实现(DWM 不可用, 尺寸变化后需按新尺寸重设);
	//					FALSE = 由 DWM 实现; NULL = 不接收该状态
	VOID ApplyWindowCornerRounding(HWND hWnd, DWORD w, DWORD h, DWORD space, BOOL* pbByRgn);
}
