/*
Copyright 2017 Intel Corporation

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
#include <d3d11.h>
#include <dxgi1_2.h>

struct ImplD3D11 : public ImplD3DBase {
    HWND                     HWnd;
    UINT                     Width;
    UINT                     Height;

    IDXGIFactory1*           DXGIFactory1;

    ID3D11Device*            Device;
    ID3D11DeviceContext*     DeviceContext;

    IDXGISwapChain1*         DXGISwapChain1;
    ID3D11RenderTargetView*  BackBufferRTV;

    ID3D11InputLayout*       InputLayout;
    ID3D11VertexShader*      VertexShader;
    ID3D11PixelShader*       PixelShader;
    ID3D11RasterizerState*   RasterizerState;
    ID3D11BlendState*        BlendState;
    ID3D11DepthStencilState* DepthStencilState;
    ID3D11Buffer*            VertexBuffer;

    ImplD3D11();
    virtual ~ImplD3D11() {}
    virtual bool Initialize(HWND hwnd) override;
    virtual void Finalize() override;
    virtual void Resize(UINT width, UINT height) override;
    virtual void Render(uint32_t resourcesIndex) override;
    virtual uint32_t WaitForResources() override { return 0; }
    virtual ID3D11Device* GetD3D11Device() const override { return Device; }
    virtual ID3D11DeviceContext* GetD3D11DeviceContext() const override { return DeviceContext; }
    virtual ID3D12Device* GetD3D12Device() const override { return nullptr; }
};
