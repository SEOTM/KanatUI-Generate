# D3D11Manager 模块 (src\framework\render\imgui\D3D11Manager.h)

## 概述

单个窗口的 D3D11 设施管理器:建立并持有设备、设备上下文、交换链与主渲染目标, 处理窗口尺寸变化,
执行呈现与清屏。每个窗口一个实例, 实例之间互不共享资源。

本模块属 **framework 层**的 `render\imgui` 技术目录, 是 `ImGuiHost` 的 D3D11 图形后端设施; 只依赖
D3D11 / DXGI, 不持有窗口过程、不参与帧流程编排(帧流程归 `ImGuiHost`), 也不 include 任何 ImGui 头。
构造后须先 `Initialize` 成功, 其余渲染接口才可用。

2026-10-22 起本模块自 `src\common\` 归位到 `src\framework\render\imgui\`(与 `ImGuiHost` 平铺): 只改路径与
include 前缀, 类的定义、成员布局、函数实现与依赖均未变; 唯一包含方是同目录的 `ImGuiHost`。

本模块包含以下编程接口:

### 类

| 类 | 说明 |
| --- | --- |
| `D3D11Manager` | 单窗口 D3D11 设备 / 交换链 / 渲染目标管理器。 |

---

## D3D11Manager 类 (D3D11Manager)

单窗口 D3D11 设施管理器。

### 语法

```cpp
class D3D11Manager
{
public:
    D3D11Manager() = default;
    ~D3D11Manager();

    bool Initialize(HWND hWnd);
    void Shutdown();

    void CreateRenderTarget();
    void CleanupRenderTarget();

    void SetResizeDimensions(UINT width, UINT height);
    void ProcessResize();

    ID3D11Device* GetDevice() const;
    ID3D11DeviceContext* GetDeviceContext() const;
    ID3D11RenderTargetView* GetMainRenderTargetView() const;
    IDXGISwapChain* GetSwapChain() const;

    BOOL IsSwapChainOccluded() const;
    void SetSwapChainOccluded(BOOL occluded);

    HRESULT Present(UINT syncInterval = 1, UINT flags = 0);
    void ClearRenderTargetView(const FLOAT color[4]);
};
```

### 成员

| 成员 | 说明 |
| --- | --- |
| `D3D11Manager()` | 默认构造; 不建立任何 D3D 资源。 |
| `~D3D11Manager()` | 析构时调用 `Shutdown` 统一释放资源。 |
| `Initialize(HWND)` | 以目标窗口建立设备、交换链与主渲染目标; 硬件不可用时回退 WARP 软件驱动。 |
| `Shutdown()` | 释放渲染目标、交换链、设备上下文与设备, 并把窗口句柄与尺寸状态清零; 幂等。 |
| `CreateRenderTarget()` | 从交换链后台缓冲重新建立主渲染目标视图。 |
| `CleanupRenderTarget()` | 释放主渲染目标视图; 未建立时无副作用。 |
| `SetResizeDimensions(UINT, UINT)` | 记录待应用的客户区尺寸; 不立即改变交换链。 |
| `ProcessResize()` | 应用已记录的尺寸: 重建缓冲并重建渲染目标; 无待应用尺寸时无副作用。 |
| `GetDevice()` | 取 D3D11 设备; 未初始化时为 `nullptr`。 |
| `GetDeviceContext()` | 取设备上下文; 未初始化时为 `nullptr`。 |
| `GetMainRenderTargetView()` | 取主渲染目标视图; 未建立时为 `nullptr`。 |
| `GetSwapChain()` | 取交换链; 未初始化时为 `nullptr`。 |
| `IsSwapChainOccluded()` | 取当前遮挡状态(最近一次 `Present` 的结论)。 |
| `SetSwapChainOccluded(BOOL)` | 写入遮挡状态(供调用方在自行判定后同步)。 |
| `Present(UINT = 1, UINT = 0)` | 呈现一帧; 同步更新遮挡状态并返回 `HRESULT`。 |
| `ClearRenderTargetView(const FLOAT[4])` | 以给定 RGBA 清空主渲染目标; 资源未就绪时无副作用。 |

### 注解

**初始化与回退**: `Initialize` 保存窗口句柄后调用内部建链流程。交换链为双缓冲、
`DXGI_FORMAT_B8G8R8A8_UNORM`、`DXGI_SWAP_EFFECT_DISCARD`、窗口模式, 特性等级取 11.0 / 10.0。
硬件驱动返回 `DXGI_ERROR_UNSUPPORTED` 时改用 `D3D_DRIVER_TYPE_WARP` 重试; 仍失败返回 `false`,
此时实例保持未就绪状态。建链成功后立即建立主渲染目标视图。

**尺寸变化**: 窗口过程侧只调用 `SetResizeDimensions` 记账, 由下一次 `ProcessResize` 统一应用——
先释放旧渲染目标, 再 `ResizeBuffers` 重建, 然后把记录尺寸清零并重建渲染目标。先记账后应用的两段式
保证尺寸变更发生在帧边界, 不与绘制交错。

**呈现与遮挡**: `Present` 成功时按结果同步 `m_swapChainOccluded`(`DXGI_STATUS_OCCLUDED` ⇒ 遮挡);
交换链为空时返回 `E_FAIL` 且不改遮挡状态。调用方据此跳过被遮挡窗口的绘制。

**所有权**: 设备、上下文、交换链与渲染目标视图均由实例持有, 调用方只借用指针, 不得 `Release`;
释放统一走 `Shutdown` / 析构。实例禁止拷贝与赋值。

**线程**: 实例由渲染线程独占使用; 不提供内部同步。


---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `framework/render/imgui/D3D11Manager.h`(`src\framework\render\imgui\D3D11Manager.h`) |
| 实现 | `src\framework\render\imgui\D3D11Manager.cpp` |
| 命名空间 | 全局(类) |
| 依赖 | `d3d11.lib` · `d3d11.dll` |
| 平台 | Windows 7 及以上(需 D3D11 运行时); 无硬件 D3D11 时回退 WARP |
| 线程安全 | 否; 每个实例由单一渲染线程独占 |
