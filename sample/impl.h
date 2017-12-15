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

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D12Device;

struct Vertex {
    float position[2];
    float color[3];
};

struct ImplD3DBase;

struct ImplD3DBase {
    virtual ~ImplD3DBase() {}
    virtual bool Initialize(HWND hwnd) = 0;
    virtual void Finalize() = 0;
    virtual void Resize(UINT width, UINT height) = 0;
    virtual void Render(uint32_t resourcesIndex) = 0;
    virtual uint32_t WaitForResources() = 0;
    virtual ID3D11Device* GetD3D11Device() const = 0;
    virtual ID3D11DeviceContext* GetD3D11DeviceContext() const = 0;
    virtual ID3D12Device* GetD3D12Device() const = 0;
};

#ifdef NDEBUG
#define HR_CHECK(_HR) (void) _HR
#else
#define HR_CHECK(_HR) assert(SUCCEEDED(_HR))
#endif

template<typename T>
inline void SafeRelease(T*& t, ULONG expectedCount=0)
{
    if (t == nullptr) {
        return;
    }

    auto count = t->Release();
    if (count != expectedCount) {
        DebugBreak();
    }

    t = nullptr;
}
