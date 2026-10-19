#if defined(_WIN32)
#include "jp_screen_capture.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwmapi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <stdexcept>

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dwmapi.lib")
#pragma comment(lib,"windowsapp.lib")
namespace jp_screen {
namespace {
using namespace winrt;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
std::string utf8(const wchar_t *s) { return to_string(std::wstring_view(s)); }
class WindowsCapture final : public Backend {
    std::atomic<bool> stopping{false};
    std::thread worker;
    void run(std::string id, bool windowMode) {
        try {
            init_apartment(apartment_type::multi_threaded);
            struct Apartment { ~Apartment(){uninit_apartment();} } apartment;
            if(!GraphicsCaptureSession::IsSupported()) { mailbox->state(State::Unavailable,"Captura no disponible en este Windows");return; }
            HWND window=nullptr;
            if(windowMode) {
                const auto separator=id.find(':');
                if(separator==std::string::npos)throw std::runtime_error("Selecciona una ventana nuevamente");
                window=reinterpret_cast<HWND>(static_cast<uintptr_t>(std::stoull(id.substr(0,separator))));
                DWORD pid=0;GetWindowThreadProcessId(window,&pid);
                if(!IsWindow(window) || !pid || pid!=std::stoul(id.substr(separator+1)))throw std::runtime_error("La ventana ya no esta disponible");
            }
            HMONITOR monitor=nullptr;
            struct Search { std::string id; HMONITOR found=nullptr; } search{id};
            EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR h,HDC,LPRECT,LPARAM data)->BOOL {
                auto &s=*reinterpret_cast<Search*>(data);MONITORINFOEXW info{};info.cbSize=sizeof(info);
                if(GetMonitorInfoW(h,&info) && utf8(info.szDevice)==s.id)s.found=h;
                return TRUE;
            },reinterpret_cast<LPARAM>(&search));monitor=search.found;
            if(!windowMode && !monitor){mailbox->state(State::Error,"El monitor ya no esta disponible. Seleccionalo nuevamente.");return;}
            com_ptr<ID3D11Device> device;com_ptr<ID3D11DeviceContext> context;
            check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                nullptr,0,D3D11_SDK_VERSION,device.put(),nullptr,context.put()));
            auto dxgi=device.as<IDXGIDevice>();com_ptr<IInspectable> inspectable;
            check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(),inspectable.put()));
            auto runtimeDevice=inspectable.as<IDirect3DDevice>();
            auto interop=get_activation_factory<GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
            GraphicsCaptureItem item{nullptr};
            if(windowMode)check_hresult(interop->CreateForWindow(window,guid_of<GraphicsCaptureItem>(),put_abi(item)));
            else check_hresult(interop->CreateForMonitor(monitor,guid_of<GraphicsCaptureItem>(),put_abi(item)));
            auto pool=Direct3D11CaptureFramePool::CreateFreeThreaded(runtimeDevice,DirectXPixelFormat::B8G8R8A8UIntNormalized,2,item.Size());
            auto session=pool.CreateCaptureSession(item);
            auto closed=std::make_shared<std::atomic<bool>>(false);
            auto closedToken=item.Closed([closed](auto const&,auto const&){*closed=true;});
            session.StartCapture();
            com_ptr<ID3D11Texture2D> staging;
            int width=0,height=0;
            while(!stopping && !*closed) {
                auto frame=pool.TryGetNextFrame();
                if(frame) {
                    auto size=frame.ContentSize();
                    if(size.Width<=0 || size.Height<=0){frame.Close();continue;}
                    auto access=frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();com_ptr<ID3D11Texture2D> texture;
                    check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D),texture.put_void()));
                    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
                    // A resize frame may still use the previous pool's surface.
                    if(desc.Width!=UINT(size.Width) || desc.Height!=UINT(size.Height)) {
                        frame.Close();pool.Recreate(runtimeDevice,DirectXPixelFormat::B8G8R8A8UIntNormalized,2,size);continue;
                    }
                    if(width!=size.Width || height!=size.Height) {
                        width=size.Width;height=size.Height;staging=nullptr;
                        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
                        check_hresult(device->CreateTexture2D(&desc,nullptr,staging.put()));
                    }
                    context->CopyResource(staging.get(),texture.get());
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    check_hresult(context->Map(staging.get(),0,D3D11_MAP_READ,0,&mapped));
                    Frame output;output.width=width;output.height=height;output.rgba.resize(size_t(width)*height*4);
                    for(int y=0;y<height;++y){auto src=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch;auto dst=output.rgba.data()+size_t(y)*width*4;
                        for(int x=0;x<width;++x){dst[x*4]=src[x*4+2];dst[x*4+1]=src[x*4+1];dst[x*4+2]=src[x*4];dst[x*4+3]=255;}}
                    context->Unmap(staging.get(),0);frame.Close();mailbox->publish(std::move(output));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(34));
            }
            if(*closed)mailbox->state(State::Error,"Se dejo de compartir la fuente. Seleccionala nuevamente.");
            item.Closed(closedToken);session.Close();pool.Close();
        } catch(const hresult_error &e) { if(!stopping)mailbox->state(e.code()==REGDB_E_CLASSNOTREG || e.code()==E_NOINTERFACE ? State::Unavailable : State::Error,to_string(e.message())); }
        catch(const std::exception &e) {if(!stopping)mailbox->state(State::Error,e.what());}
    }
public:
    explicit WindowsCapture(std::string id, bool window = false):worker([this,id,window]{run(id,window);}){}
    ~WindowsCapture() override{stop();}
    void stop() override{stopping=true;if(worker.joinable())worker.join();}
};
}
std::vector<Monitor> monitors() {
    std::vector<Monitor> result;
    EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR monitor,HDC,LPRECT,LPARAM data)->BOOL {
        MONITORINFOEXW info{};info.cbSize=sizeof(info);
        if(GetMonitorInfoW(monitor,&info)) {
            auto &list=*reinterpret_cast<std::vector<Monitor>*>(data);
            list.push_back({utf8(info.szDevice),"Pantalla "+std::to_string(list.size()+1)+" - "+
                std::to_string(info.rcMonitor.right-info.rcMonitor.left)+" x "+std::to_string(info.rcMonitor.bottom-info.rcMonitor.top)});
        }return TRUE;
    },reinterpret_cast<LPARAM>(&result));return result;
}
std::vector<Monitor> windows() {
    std::vector<Monitor> result;
    EnumWindows([](HWND window,LPARAM data)->BOOL {
        if(!IsWindowVisible(window) || GetWindow(window,GW_OWNER) ||
            (GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))return TRUE;
        DWORD cloaked=0;
        if(SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_CLOAKED,&cloaked,sizeof(cloaked))) && cloaked)return TRUE;
        const int length=GetWindowTextLengthW(window);
        if(length<=0)return TRUE;
        std::wstring title(length+1,L'\0');GetWindowTextW(window,&title[0],length+1);
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        auto &list=*reinterpret_cast<std::vector<Monitor>*>(data);
        list.push_back({std::to_string(reinterpret_cast<uintptr_t>(window))+":"+std::to_string(pid),utf8(title.c_str())});
        return TRUE;
    },reinterpret_cast<LPARAM>(&result));
    return result;
}
std::unique_ptr<Backend> makeWindowBackend(const std::string &window){return std::make_unique<WindowsCapture>(window,true);}
std::unique_ptr<Backend> makeBackend(const std::string &monitor){return std::make_unique<WindowsCapture>(monitor);}
}
#endif
