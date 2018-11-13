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
#include <imgui_impl_dx12.h>
#include <stdint.h>
#include <windows.h>

#include "impl.h"
#include "impl_d3d12.h"

#include <vs.hlsl.h>
#include <ps.hlsl.h>

ImplD3D12::ImplD3D12()
    : HWnd                   (NULL)
    , Width                  (0)
    , Height                 (0)
    , DxgiFactory4           (nullptr)
    , DxgiSwapChain3         (nullptr)
    , SwapChainWaitableObject(NULL)
    , Device                 (nullptr)
    , CommandQueue           (nullptr)
    , Fence                  (nullptr)
    , FenceEvent             (NULL)
    , LastSignalledFenceValue(0)
    , FrameIndex             (0)
    , RTVHeap                (nullptr)
    , SRVHeap                (nullptr)
    , CmdList                (nullptr)
    , PipelineState          (nullptr)
    , RootSignature          (nullptr)
    , VertexBuffer           (nullptr)
{
    memset(BackBuffer, 0, sizeof(BackBuffer));
    memset(FrameCtxt,  0, sizeof(FrameCtxt));
}

bool ImplD3D12::Initialize(
    HWND hwnd)
{
    HWnd = hwnd;

    {
        auto hModule = LoadLibrary("d3d12.dll");
        if (hModule == NULL) {
            return false;
        }

        FreeLibrary(hModule);
    }

#ifdef _DEBUG
    {
        ID3D12Debug* debugController = nullptr;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            debugController->Release();
        }
    }
#endif

    HR_CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&DxgiFactory4)));
    HR_CHECK(DxgiFactory4->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

    IDXGIAdapter1* adapter = nullptr;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != DxgiFactory4->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter->Release();
            continue;
        }

        if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
            adapter->Release();
            continue;
        }

        break;
    }

    if (adapter == nullptr) {
        return false;
    }

    HR_CHECK(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&Device)));

    adapter->Release();

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = D3D12_NUM_BACK_BUFFERS;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask       = 1;
        HR_CHECK(Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&RTVHeap)));

        auto rtvDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        auto rtvHandle = RTVHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < D3D12_NUM_BACK_BUFFERS; ++i) {
            BackBuffer[i].Handle = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        desc.NodeMask       = 1;
        HR_CHECK(Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&SRVHeap)));
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        HR_CHECK(Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&CommandQueue)));
    }

    {
        /*
        D3D12_ROOT_PARAMETER param[] = {
        };
        */

        D3D12_ROOT_SIGNATURE_DESC desc = {};
        /*
        desc.NumParameters = _countof(param);
        desc.pParameters   = param;
        */
        desc.Flags         =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        ID3DBlob* blob = nullptr;
        HR_CHECK(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr));
        HR_CHECK(Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&RootSignature)));
        blob->Release();
    }

    {
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.NodeMask              = 1;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.InputLayout           = { inputElementDescs, _countof(inputElementDescs) };
        desc.pRootSignature        = RootSignature;
        desc.VS                    = { vs, _countof(vs) };
        desc.PS                    = { ps, _countof(ps) };
        desc.SampleMask            = UINT_MAX;
        desc.NumRenderTargets      = 1;
        desc.RTVFormats[0]         = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count      = 1;
        desc.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE;

        desc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode              = D3D12_CULL_MODE_BACK;
        desc.RasterizerState.FrontCounterClockwise = FALSE;
        desc.RasterizerState.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
        desc.RasterizerState.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        desc.RasterizerState.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        desc.RasterizerState.DepthClipEnable       = TRUE;
        desc.RasterizerState.MultisampleEnable     = FALSE;
        desc.RasterizerState.AntialiasedLineEnable = FALSE;
        desc.RasterizerState.ForcedSampleCount     = 0;
        desc.RasterizerState.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        desc.BlendState.AlphaToCoverageEnable                 = FALSE;
        desc.BlendState.IndependentBlendEnable                = FALSE;
        desc.BlendState.RenderTarget[0].BlendEnable           = FALSE;
        desc.BlendState.RenderTarget[0].LogicOpEnable         = FALSE;
        desc.BlendState.RenderTarget[0].SrcBlend              = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].DestBlend             = D3D12_BLEND_ZERO;
        desc.BlendState.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
        desc.BlendState.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_ZERO;
        desc.BlendState.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
        desc.BlendState.RenderTarget[0].LogicOp               = D3D12_LOGIC_OP_NOOP;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        desc.DepthStencilState.DepthEnable   = FALSE;
        desc.DepthStencilState.StencilEnable = FALSE;

        HR_CHECK(Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&PipelineState)));
    }

    {
        Vertex triangle[] = {
            { {  0.0f,  0.5f }, { 1.f, 0.f, 0.f } },
            { {  0.5f, -0.5f }, { 0.f, 1.f, 0.f } },
            { { -0.5f, -0.5f }, { 0.f, 0.f, 1.f } },
        };

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type                 = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask     = 1;
        heapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment          = 0;
        desc.Width              = sizeof(triangle);
        desc.Height             = 1;
        desc.DepthOrArraySize   = 1;
        desc.MipLevels          = 1;
        desc.Format             = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags              = D3D12_RESOURCE_FLAG_NONE;

        HR_CHECK(Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&VertexBuffer)));

        {
            D3D12_RANGE readRange = {};
            D3D12_RANGE writeRange = { 0, sizeof(triangle) };
            void* ptr = nullptr;
            HR_CHECK(VertexBuffer->Map(0, &readRange, &ptr));
            memcpy(ptr, triangle, sizeof(triangle));
            VertexBuffer->Unmap(0, &writeRange);
        }

        VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
        VertexBufferView.SizeInBytes    = sizeof(triangle);
        VertexBufferView.StrideInBytes  = sizeof(Vertex);
    }

    for (UINT i = 0; i < D3D12_NUM_FRAMES_IN_FLIGHT; ++i) {
        HR_CHECK(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&FrameCtxt[i].CommandAllocator)));
    }

    HR_CHECK(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, FrameCtxt[0].CommandAllocator, nullptr, IID_PPV_ARGS(&CmdList)));

    HR_CHECK(CmdList->Close());

    HR_CHECK(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));

    FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (FenceEvent == nullptr) {
        HR_CHECK(HRESULT_FROM_WIN32(GetLastError()));
    }

    ImGui_ImplDX12_Init(Device, D3D12_NUM_FRAMES_IN_FLIGHT,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        SRVHeap->GetCPUDescriptorHandleForHeapStart(),
        SRVHeap->GetGPUDescriptorHandleForHeapStart());
    ImGui_ImplDX12_CreateDeviceObjects();

    return true;
}

void ImplD3D12::Finalize()
{
    WaitForLastSubmittedFrame();

    ImGui_ImplDX12_Shutdown();

    for (UINT i = 0; i < D3D12_NUM_FRAMES_IN_FLIGHT; ++i) {
        SafeRelease(FrameCtxt[i].CommandAllocator);
    }
    SafeRelease(RTVHeap);
    SafeRelease(SRVHeap);
    SafeRelease(CmdList);
    SafeRelease(PipelineState);
    SafeRelease(RootSignature);
    SafeRelease(VertexBuffer);
    SafeRelease(Fence);
    for (UINT i = 0; i < D3D12_NUM_BACK_BUFFERS; ++i) {
        SafeRelease(BackBuffer[i].Resource, D3D12_NUM_BACK_BUFFERS - i - 1);
    }
    SafeRelease(DxgiSwapChain3);
    SafeRelease(CommandQueue);
    SafeRelease(Device);
    SafeRelease(DxgiFactory4);
    CloseHandle(SwapChainWaitableObject);
    CloseHandle(FenceEvent);
}

void ImplD3D12::Resize(
    UINT width,
    UINT height)
{
    WaitForLastSubmittedFrame();

    Width  = width;
    Height = height;

    if (DxgiSwapChain3 != nullptr) {
        DxgiSwapChain3->Release();
        DxgiSwapChain3 = nullptr;
        for (UINT i = 0; i < D3D12_NUM_BACK_BUFFERS; ++i) {
            BackBuffer[i].Resource->Release();
            BackBuffer[i].Resource = nullptr;
        }
        CloseHandle(SwapChainWaitableObject);
    }

    {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width              = width;
        desc.Height             = height;
        desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Stereo             = FALSE;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount        = D3D12_NUM_BACK_BUFFERS;
        desc.Scaling            = DXGI_SCALING_STRETCH;
        desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
        desc.Flags              = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        IDXGISwapChain1* swapChain1 = nullptr;
        HR_CHECK(DxgiFactory4->CreateSwapChainForHwnd(CommandQueue, HWnd, &desc, nullptr, nullptr, &swapChain1));
        HR_CHECK(swapChain1->QueryInterface(IID_PPV_ARGS(&DxgiSwapChain3)));
        swapChain1->Release();

        HR_CHECK(DxgiSwapChain3->SetMaximumFrameLatency(D3D12_NUM_BACK_BUFFERS));

        SwapChainWaitableObject = DxgiSwapChain3->GetFrameLatencyWaitableObject();
        if (SwapChainWaitableObject == NULL) {
            HR_CHECK(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    for (UINT i = 0; i < D3D12_NUM_BACK_BUFFERS; ++i) {
        HR_CHECK(DxgiSwapChain3->GetBuffer(i, IID_PPV_ARGS(&BackBuffer[i].Resource)));
        Device->CreateRenderTargetView(BackBuffer[i].Resource, nullptr, BackBuffer[i].Handle);
    }
}

void ImplD3D12::Render(
    uint32_t resourcesIndex)
{
    auto frameCtxt = &FrameCtxt[resourcesIndex];

    auto backBufferIdx = DxgiSwapChain3->GetCurrentBackBufferIndex();

    HR_CHECK(frameCtxt->CommandAllocator->Reset());
    HR_CHECK(CmdList->Reset(frameCtxt->CommandAllocator, PipelineState));

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = BackBuffer[backBufferIdx].Resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    CmdList->ResourceBarrier(1, &barrier);

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    CmdList->ClearRenderTargetView(BackBuffer[backBufferIdx].Handle, clearColor, 0, nullptr);

    D3D12_VIEWPORT viewport = {};
    viewport.Width    = (float) Width;
    viewport.Height   = (float) Height;
    viewport.MaxDepth = 1.f;

    D3D12_RECT scissorRect = {};
    scissorRect.right  = Width;
    scissorRect.bottom = Height;

    CmdList->SetGraphicsRootSignature(RootSignature);
    CmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    CmdList->IASetVertexBuffers(0, 1, &VertexBufferView);
    CmdList->RSSetViewports(1, &viewport);
    CmdList->OMSetRenderTargets(1, &BackBuffer[backBufferIdx].Handle, FALSE, nullptr);
    CmdList->RSSetScissorRects(1, &scissorRect);

    CmdList->DrawInstanced(3, 1, 0, 0);

    CmdList->SetDescriptorHeaps(1, &SRVHeap);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), CmdList);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    CmdList->ResourceBarrier(1, &barrier);

    HR_CHECK(CmdList->Close());

    CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*) &CmdList);

    HR_CHECK(DxgiSwapChain3->Present(1, 0));

    auto fenceValue = LastSignalledFenceValue + 1;
    HR_CHECK(CommandQueue->Signal(Fence, fenceValue));
    LastSignalledFenceValue = fenceValue;
    frameCtxt->FenceValue = fenceValue;
}

void ImplD3D12::WaitForLastSubmittedFrame()
{
    auto frameCtxt = &FrameCtxt[FrameIndex % D3D12_NUM_FRAMES_IN_FLIGHT];
    auto fenceValue = frameCtxt->FenceValue;

    if (fenceValue == 0) { // means no fence was signalled
        return;
    }

    frameCtxt->FenceValue = 0;

    if (Fence->GetCompletedValue() >= fenceValue) {
        return;
    }

    HR_CHECK(Fence->SetEventOnCompletion(fenceValue, FenceEvent));
    WaitForSingleObject(FenceEvent, INFINITE);
}

uint32_t ImplD3D12::WaitForResources()
{
    auto nextFrameIndex = FrameIndex + 1;
    auto nextResourcesIndex = nextFrameIndex % D3D12_NUM_FRAMES_IN_FLIGHT;
    FrameIndex = nextFrameIndex;

    auto frameCtxt = &FrameCtxt[nextResourcesIndex];
    auto fence      = Fence;
    auto fenceEvent = FenceEvent;
    auto fenceValue = frameCtxt->FenceValue;

    HANDLE waitableObjects[] = {
        SwapChainWaitableObject,
        NULL,
    };
    DWORD numWaitableObjects = 1;

    if (fenceValue != 0) { // means no fence was signalled
        frameCtxt->FenceValue = 0;

        HR_CHECK(fence->SetEventOnCompletion(fenceValue, fenceEvent));
        waitableObjects[1] = fenceEvent;
        numWaitableObjects = 2;
    }

    HR_CHECK(WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE));

    return nextResourcesIndex;
}

