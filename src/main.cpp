// ================================
// 文件: main.cpp (Kanat UI Generate 进程入口)
// 描述: 只保留进程入口 —— 宿主生命周期(启动初始化 / 主循环 / 清理链)已整体迁入 app\AppHost
//   (见该文件头说明)。
// 说明: 双窗口宿主的形态未变:
//   - 编辑窗口 : 承载 Kanat UI Generate 自身界面布局(KanatUIGenerateEditor 布局; 叫法"外壳"),
//       含标题带("编辑布局"可拖动 + 退出)与三段编辑 UI
//   - 预览窗口 : 当前 active 布局的真实窗口(POPUP 不可缩放,
//       客户区尺寸 = 布局 autosize 推导 wndW/H, 桌面位置 = 布局 wndX/Y; 内容 = 布局渲染)
//   两窗口各自独立 D3D11Manager + ImGui context + 平台/渲染后端, 同进程单线程交替渲染。
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include <minwindef.h>

#include "app/AppHost.h"

// ================================
// 区域: 进程入口实现区
// ================================

// 函数: main
// 描述: Kanat UI Generate 程序入口(宿主执行体见 app\AppHost.cpp 的 AppHost::Run)
int main(int, char**)
{
	return AppHost::Run();
}
