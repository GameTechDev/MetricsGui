/*
Copyright 2017-2018 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <assert.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <stdint.h>
#include <windows.h>

#include "impl.h"
#include "impl_d3d11.h"

#include <vs.hlsl.h>
#include <ps.hlsl.h>

ImplD3D11::ImplD3D11()
    : HWnd             (NULL)
    , Width            (0)
    , Height           (0)
    , DXGIFactory1     (nullptr)
    , Device           (nullptr)
    , DeviceContext    (nullptr)
    , DXGISwapChain1   (nullptr)
    , BackBufferRTV    (nullptr)
    , InputLayout      (nullptr)
    , VertexShader     (nullptr)
    , PixelShader      (nullptr)
    , RasterizerState  (nullptr)
    , BlendState       (nullptr)
    , DepthStencilState(nullptr)
    , VertexBuffer     (nullptr)
{
}

bool ImplD3D11::Initialize(
    HWND hwnd)
{
    HWnd = hwnd;

    HR_CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory1)));
    HR_CHECK(DXGIFactory1->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

    IDXGIAdapter1* adapter = nullptr;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != DXGIFactory1->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter->Release();
            continue;
        }

        break;
    }

    if (adapter == nullptr) {
        return false;
    }

    UINT d3dFlags = 0;
#ifdef _DEBUG
    d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HR_CHECK(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, d3dFlags,
        nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &DeviceContext));

    adapter->Release();

    {
        D3D11_INPUT_ELEMENT_DESC desc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        HR_CHECK(Device->CreateInputLayout(desc, _countof(desc), vs, _countof(vs), &InputLayout));
    }

    HR_CHECK(Device->CreateVertexShader(vs, _countof(vs), nullptr, &VertexShader));
    HR_CHECK(Device->CreatePixelShader(ps, _countof(ps), nullptr, &PixelShader));

    {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FillMode              = D3D11_FILL_SOLID;
        desc.CullMode              = D3D11_CULL_BACK;
        desc.FrontCounterClockwise = FALSE;
        desc.DepthBias             = D3D11_DEFAULT_DEPTH_BIAS;
        desc.DepthBiasClamp        = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
        desc.SlopeScaledDepthBias  = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        desc.DepthClipEnable       = TRUE;
        desc.ScissorEnable         = TRUE;
        desc.MultisampleEnable     = FALSE;
        desc.AntialiasedLineEnable = FALSE;

        HR_CHECK(Device->CreateRasterizerState(&desc, &RasterizerState));
    }

    {
        D3D11_BLEND_DESC desc = {};
        desc.AlphaToCoverageEnable                 = FALSE;
        desc.IndependentBlendEnable                = FALSE;
        desc.RenderTarget[0].BlendEnable           = FALSE;
        desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend             = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        HR_CHECK(Device->CreateBlendState(&desc, &BlendState));
    }

    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable   = FALSE;
        desc.StencilEnable = FALSE;

        HR_CHECK(Device->CreateDepthStencilState(&desc, &DepthStencilState));
    }

    {
        Vertex triangle[] = {
            { {  0.0f,  0.5f }, { 1.f, 0.f, 0.f } },
            { {  0.5f, -0.5f }, { 0.f, 1.f, 0.f } },
            { { -0.5f, -0.5f }, { 0.f, 0.f, 1.f } },
        };

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(triangle);
        desc.Usage     = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = triangle;

        HR_CHECK(Device->CreateBuffer(&desc, &data, &VertexBuffer));
    }

    ImGui_ImplDX11_Init(Device, DeviceContext);
    ImGui_ImplDX11_CreateDeviceObjects();

    return true;
}

void ImplD3D11::Finalize()
{
    ImGui_ImplDX11_Shutdown();

    SafeRelease(InputLayout);
    SafeRelease(VertexShader);
    SafeRelease(PixelShader);
    SafeRelease(RasterizerState);
    SafeRelease(BlendState);
    SafeRelease(DepthStencilState);
    SafeRelease(VertexBuffer);
    SafeRelease(BackBufferRTV);
    SafeRelease(DeviceContext);
    SafeRelease(DXGISwapChain1);
    SafeRelease(Device);
    SafeRelease(DXGIFactory1);
}

void ImplD3D11::Resize(
    UINT width,
    UINT height)
{
    Width  = width;
    Height = height;

    SafeRelease(BackBufferRTV);
    SafeRelease(DXGISwapChain1);

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width              = width;
    desc.Height             = height;
    desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo             = FALSE;
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount        = 3;
    desc.Scaling            = DXGI_SCALING_STRETCH;
    desc.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;
    desc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;

    IDXGIFactory2* factory2 = nullptr;
    HR_CHECK(DXGIFactory1->QueryInterface(IID_PPV_ARGS(&factory2)));

    HR_CHECK(factory2->CreateSwapChainForHwnd(Device, HWnd,
        &desc, nullptr, nullptr, &DXGISwapChain1));

    factory2->Release();

    ID3D11Texture2D* backBuffer = nullptr;
    HR_CHECK(DXGISwapChain1->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format        = DXGI_FORMAT_B8G8R8A8_UNORM;
    HR_CHECK(Device->CreateRenderTargetView(backBuffer, &rtvDesc, &BackBufferRTV));

    backBuffer->Release();
}

void ImplD3D11::Render(
    uint32_t resourcesIndex)
{
    (void) resourcesIndex;

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    DeviceContext->ClearRenderTargetView(BackBufferRTV, clearColor);

    D3D11_VIEWPORT viewport = {};
    viewport.Width    = (float) Width;
    viewport.Height   = (float) Height;
    viewport.MaxDepth = 1.f;

    D3D11_RECT scissorRect = {};
    scissorRect.right  = Width;
    scissorRect.bottom = Height;

    UINT vbStride = sizeof(Vertex);
    UINT vbOffset = 0;

    FLOAT blendFactor[] = { 1.f, 1.f, 1.f, 1.f };
    UINT sampleMask = UINT_MAX;
    UINT stencilRef = 0;

    DeviceContext->IASetInputLayout(InputLayout);
    DeviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &vbStride, &vbOffset);
    DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    DeviceContext->RSSetState(RasterizerState);
    DeviceContext->RSSetViewports(1, &viewport);
    DeviceContext->RSSetScissorRects(1, &scissorRect);
    DeviceContext->PSSetShader(PixelShader, nullptr, 0);
    DeviceContext->OMSetBlendState(BlendState, blendFactor, sampleMask);
    DeviceContext->OMSetDepthStencilState(DepthStencilState, stencilRef);
    DeviceContext->OMSetRenderTargets(1, &BackBufferRTV, nullptr);

    DeviceContext->DrawInstanced(3, 1, 0, 0);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    HR_CHECK(DXGISwapChain1->Present(1, 0));
}
