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

#include <d3d12.h>
#include <dxgi1_4.h>

enum {
    D3D12_NUM_BACK_BUFFERS     = 3,
    D3D12_NUM_FRAMES_IN_FLIGHT = 3,
};

struct D3D12RenderTargetInfo {
    ID3D12Resource*             Resource;
    D3D12_CPU_DESCRIPTOR_HANDLE Handle;
};

struct D3D12FrameContext {
    ID3D12CommandAllocator* CommandAllocator;

    UINT64                  FenceValue;
    bool                    FenceSignalled;
};

struct ImplD3D12 : public ImplD3DBase {
    HWND                        HWnd;
    UINT                        Width;
    UINT                        Height;

    IDXGIFactory4*              DxgiFactory4;
    IDXGISwapChain3*            DxgiSwapChain3;
    HANDLE                      SwapChainWaitableObject;

    ID3D12Device*               Device;
    ID3D12CommandQueue*         CommandQueue;

    ID3D12Fence*                Fence;
    HANDLE                      FenceEvent;
    UINT64                      LastSignalledFenceValue;

    D3D12RenderTargetInfo       BackBuffer[D3D12_NUM_BACK_BUFFERS];

    D3D12FrameContext           FrameCtxt[D3D12_NUM_FRAMES_IN_FLIGHT];
    UINT                        FrameIndex;

    ID3D12DescriptorHeap*       RTVHeap;
    ID3D12DescriptorHeap*       SRVHeap;
    ID3D12GraphicsCommandList*  CmdList;
    ID3D12PipelineState*        PipelineState;
    ID3D12RootSignature*        RootSignature;
    ID3D12Resource*             VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW    VertexBufferView;

    ImplD3D12();
    virtual ~ImplD3D12() {}
    virtual bool Initialize(HWND hwnd) override;
    virtual void Finalize() override;
    virtual void Resize(UINT width, UINT height) override;
    virtual void Render(uint32_t resourcesIndex) override;
    virtual uint32_t WaitForResources() override;
    virtual ID3D11Device* GetD3D11Device() const override { return nullptr; }
    virtual ID3D11DeviceContext* GetD3D11DeviceContext() const override { return nullptr; }
    virtual ID3D12Device* GetD3D12Device() const override { return Device; }

    void WaitForLastSubmittedFrame();
};
