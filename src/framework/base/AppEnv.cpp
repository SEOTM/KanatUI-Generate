// ================================
// 文件: AppEnv.cpp
// 描述: 应用环境件实现(接口契约见同目录 AppEnv.md)
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <windows.h>
#include <dwmapi.h>		// DWM 圆角属性(DWMWA_WINDOW_CORNER_PREFERENCE / DWMWCP_ROUND)
#include <stdio.h>

#include "framework/base/AppEnv.h"

// ================================
// 区域: 接口实现区
// ================================

// 函数: InitConsoleUtf8
// 描述: 把控制台输入与输出代码页都设为 UTF-8
VOID AppEnv::InitConsoleUtf8()
{
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
}

// 函数: IsSystemDarkMode
// 描述: 查询系统应用主题是否为深色
bool AppEnv::IsSystemDarkMode()
{
	HKEY hKey;
	LPCWSTR subKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
	LPCWSTR valueName = L"AppsUseLightTheme";
	DWORD value = 0;
	DWORD valueSize = sizeof(DWORD);
	DWORD dwType = 0;
	LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey);
	if (status != ERROR_SUCCESS) { return false; }
	status = RegQueryValueExW(hKey, valueName, nullptr, &dwType, (LPBYTE)&value, &valueSize);
	RegCloseKey(hKey);
	if (status == ERROR_SUCCESS && dwType == REG_DWORD) { return (value == 0); }
	return false;
}

// 函数: ApplyWindowCornerRounding
// 描述: 为窗口应用圆角(优先 DWM 属性, 失败回退圆角区域裁剪)
VOID AppEnv::ApplyWindowCornerRounding(HWND hWnd, DWORD w, DWORD h, DWORD space, BOOL* pbByRgn)
{
	if (hWnd == NULL || w < 1 || h < 1)
	{
		return;
	}
	// ① DWM 圆角属性
	{
		DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
		HRESULT hr = DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
		if (SUCCEEDED(hr))
		{
			if (pbByRgn != NULL) { *pbByRgn = FALSE; }
			printf("[MAIN] 圆角实现: DWM\n");
			fflush(stdout);
			return;
		}
	}
	// ② 回退: 以圆角矩形区域裁剪窗口
	{
		HRGN hRgn = CreateRoundRectRgn(0, 0, (int)w, (int)h, (int)space, (int)space);
		if (hRgn == NULL)
		{
			return;
		}
		if (SetWindowRgn(hWnd, hRgn, TRUE) == 0)
		{
			DeleteObject(hRgn);		// 设置失败 = 所有权未转移 ⇒ 由本处释放
			return;
		}
		if (pbByRgn != NULL) { *pbByRgn = TRUE; }
		printf("[MAIN] 圆角实现: RGN:%u\n", (unsigned)space);
		fflush(stdout);
	}
}
