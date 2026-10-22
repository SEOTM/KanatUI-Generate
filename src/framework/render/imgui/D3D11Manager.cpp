#include "framework/render/imgui/D3D11Manager.h"
#include <versionhelpers.h>
#include <combaseapi.h>

bool D3D11Manager::Initialize(HWND hWnd)
{
    m_hWnd = hWnd;
    return CreateDeviceD3DInternal(hWnd);
}

bool D3D11Manager::CreateDeviceD3DInternal(HWND hWnd)
{
    // 设置交换链
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &m_pSwapChain,
        &m_pd3dDevice,
        &featureLevel,
        &m_pd3dDeviceContext
    );

    if (res == DXGI_ERROR_UNSUPPORTED)
    {
        // 如果硬件不可用(DXGI_ERROR_UNSUPPORTED)，尝试高性能 WARP 软件驱动程序
        res = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &m_pSwapChain,
            &m_pd3dDevice,
            &featureLevel,
            &m_pd3dDeviceContext
        );
    }

    if (res != S_OK)
        return false;

    CreateRenderTargetInternal();
    return true;
}

void D3D11Manager::CreateRenderTargetInternal()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer)
    {
        m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void D3D11Manager::CreateRenderTarget()
{
    CreateRenderTargetInternal();
}

void D3D11Manager::CleanupRenderTarget()
{
    if (m_mainRenderTargetView)
    {
        m_mainRenderTargetView->Release();
        m_mainRenderTargetView = nullptr;
    }
}

void D3D11Manager::SetResizeDimensions(UINT width, UINT height)
{
    m_resizeWidth = width;
    m_resizeHeight = height;
}

void D3D11Manager::ProcessResize()
{
    if (m_resizeWidth != 0 && m_resizeHeight != 0)
    {
        CleanupRenderTarget();
        m_pSwapChain->ResizeBuffers(0, m_resizeWidth, m_resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
        m_resizeWidth = m_resizeHeight = 0;
        CreateRenderTarget();
    }
}

HRESULT D3D11Manager::Present(UINT syncInterval, UINT flags)
{
    if (m_pSwapChain)
    {
        HRESULT hr = m_pSwapChain->Present(syncInterval, flags);
        m_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
        return hr;
    }
    return E_FAIL;
}

void D3D11Manager::ClearRenderTargetView(const FLOAT color[4])
{
    if (m_mainRenderTargetView && m_pd3dDeviceContext)
    {
        m_pd3dDeviceContext->ClearRenderTargetView(m_mainRenderTargetView, color);
    }
}

void D3D11Manager::Shutdown()
{
    CleanupRenderTarget();

    if (m_pSwapChain)
    {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }

    if (m_pd3dDeviceContext)
    {
        m_pd3dDeviceContext->Release();
        m_pd3dDeviceContext = nullptr;
    }

    if (m_pd3dDevice)
    {
        m_pd3dDevice->Release();
        m_pd3dDevice = nullptr;
    }

    m_hWnd = nullptr;
    m_swapChainOccluded = FALSE;
    m_resizeWidth = m_resizeHeight = 0;
}