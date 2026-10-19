#if defined(__linux__)
#include "jp_screen_capture.h"
#include "jp_capture_resize.h"
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <stdexcept>

namespace jp_screen {
namespace {
const char *service = "org.freedesktop.portal.Desktop";
const char *desktop = "/org/freedesktop/portal/desktop";
const char *screen = "org.freedesktop.portal.ScreenCast";
std::atomic<unsigned long> sequence{0};
class LinuxCapture final : public Backend {
    std::atomic<bool> stopping{false};
    GCancellable *cancel = g_cancellable_new();
    std::thread worker;
    GDBusConnection *bus = nullptr;
    GMainContext *context = nullptr;
    std::string session;
    GstElement *pipeline = nullptr;
    int fd = -1;
    guint node=0;
    std::atomic<uint64_t> sourceSize{0};
    guint closedSubscription = 0;
    bool sessionClosed = false;
    bool windowMode = false;
    std::string parentWindow;
    struct Response { bool done = false; guint code = 2; GVariant *data = nullptr; ~Response(){ if(data) g_variant_unref(data); } };
    std::string token() { return "guipper_" + std::to_string(getpid()) + "_" + std::to_string(++sequence); }
    void iterate() { while(g_main_context_pending(context)) g_main_context_iteration(context, false); }
    GVariant *call(const char *path, const char *interface, const char *method, GVariant *args) {
        GError *error = nullptr;
        auto result = g_dbus_connection_call_sync(bus, service, path, interface, method, args,
            nullptr, G_DBUS_CALL_FLAGS_NONE, 120000, cancel, &error);
        if (!result) { std::string message = error ? error->message : "Portal no disponible"; if(error) g_error_free(error); throw std::runtime_error(std::string(method)+": "+message); }
        return result;
    }
    GVariant *request(const char *method, GVariantBuilder &options, bool withSession, bool withParent = false) {
        const auto t = token();
        g_variant_builder_add(&options,"{sv}","handle_token",g_variant_new_string(t.c_str()));
        std::string sender = g_dbus_connection_get_unique_name(bus); sender.erase(0,1);
        for(char &c:sender) if(c=='.') c='_';
        const auto path = "/org/freedesktop/portal/desktop/request/" + sender + "/" + t;
        Response response;
        guint subscription = g_dbus_connection_signal_subscribe(bus,service,"org.freedesktop.portal.Request","Response",path.c_str(),nullptr,G_DBUS_SIGNAL_FLAGS_NONE,
            [](GDBusConnection*,const gchar*,const gchar*,const gchar*,const gchar*,GVariant *args,gpointer data){
                auto &r=*static_cast<Response*>(data); g_variant_get(args,"(u@a{sv})",&r.code,&r.data); r.done=true;
            },&response,nullptr);
        try {
            GVariant *args = withParent ? g_variant_new("(os@a{sv})",session.c_str(),parentWindow.c_str(),g_variant_builder_end(&options)) :
                withSession ? g_variant_new("(o@a{sv})",session.c_str(),g_variant_builder_end(&options)) :
                g_variant_new("(@a{sv})",g_variant_builder_end(&options));
            auto result=call(desktop,screen,method,args); g_variant_unref(result);
            auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
            while(!response.done && !stopping && std::chrono::steady_clock::now()<deadline){ iterate(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if(!response.done || response.code != 0) {
                // Close outstanding selectors before releasing their callback data.
                g_dbus_connection_call(bus,service,path.c_str(),"org.freedesktop.portal.Request","Close",nullptr,nullptr,G_DBUS_CALL_FLAGS_NONE,1000,nullptr,nullptr,nullptr);
                if(response.code==1 || stopping) mailbox->state(State::Cancelled);
                else mailbox->state(State::Error,"No se pudo compartir la pantalla. Volve a seleccionarla.");
                g_dbus_connection_signal_unsubscribe(bus,subscription); return nullptr;
            }
            g_dbus_connection_signal_unsubscribe(bus,subscription);
            auto data=response.data; response.data=nullptr; return data;
        } catch(...) {
            g_dbus_connection_call(bus,service,path.c_str(),"org.freedesktop.portal.Request","Close",nullptr,nullptr,G_DBUS_CALL_FLAGS_NONE,1000,nullptr,nullptr,nullptr);
            g_dbus_connection_signal_unsubscribe(bus,subscription); throw;
        }
    }
    bool setup() {
        GVariantBuilder options;
        g_variant_builder_init(&options,G_VARIANT_TYPE_VARDICT);
        const auto st=token();
        g_variant_builder_add(&options,"{sv}","session_handle_token",g_variant_new_string(st.c_str()));
        auto data=request("CreateSession",options,false); if(!data) return false;
        const char *path=nullptr; g_variant_lookup(data,"session_handle","&s",&path);
        if(path) session=path;
        g_variant_unref(data);
        if(session.empty()) throw std::runtime_error("El portal no devolvio una sesion");
        closedSubscription=g_dbus_connection_signal_subscribe(bus,service,"org.freedesktop.portal.Session","Closed",session.c_str(),nullptr,G_DBUS_SIGNAL_FLAGS_NONE,
            [](GDBusConnection*,const gchar*,const gchar*,const gchar*,const gchar*,GVariant*,gpointer p){static_cast<LinuxCapture*>(p)->sessionClosed=true;},this,nullptr);
        g_variant_builder_init(&options,G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&options,"{sv}","types",g_variant_new_uint32(windowMode ? 2 : 1));
        g_variant_builder_add(&options,"{sv}","multiple",g_variant_new_boolean(false));
        g_variant_builder_add(&options,"{sv}","cursor_mode",g_variant_new_uint32(2));
        data=request("SelectSources",options,true); if(!data)return false; g_variant_unref(data);
        g_variant_builder_init(&options,G_VARIANT_TYPE_VARDICT);
        data=request("Start",options,true,true); if(!data)return false;
        auto streams=g_variant_lookup_value(data,"streams",G_VARIANT_TYPE("a(ua{sv})"));
        node=0;
        if(streams && g_variant_n_children(streams)) { auto item=g_variant_get_child_value(streams,0); g_variant_get_child(item,0,"u",&node); g_variant_unref(item); }
        if(streams)g_variant_unref(streams);
        g_variant_unref(data);
        if(!node)throw std::runtime_error("El portal no devolvio un monitor");
        return openStream();
    }
    void closeStream() {
        if(pipeline){gst_element_set_state(pipeline,GST_STATE_NULL);gst_object_unref(pipeline);pipeline=nullptr;}
        if(fd>=0){::close(fd);fd=-1;}
    }
    bool openStream() {
        GUnixFDList *fds=nullptr; GError *error=nullptr;
        auto result=g_dbus_connection_call_with_unix_fd_list_sync(bus,service,desktop,screen,"OpenPipeWireRemote",
            g_variant_new("(oa{sv})",session.c_str(),nullptr),G_VARIANT_TYPE("(h)"),G_DBUS_CALL_FLAGS_NONE,120000,nullptr,&fds,cancel,&error);
        if(!result) { std::string message=error?std::string("OpenPipeWireRemote: ")+error->message:"PipeWire no disponible";if(error)g_error_free(error);throw std::runtime_error(message); }
        gint index;g_variant_get(result,"(h)",&index);g_variant_unref(result);
        fd=g_unix_fd_list_get(fds,index,&error);g_object_unref(fds);
        if(fd<0){if(error)g_error_free(error);throw std::runtime_error("No se pudo abrir PipeWire");}
        // Release PipeWire-owned buffers before downstream can retain them.
        // Otherwise resizing/fullscreen may wait forever for the old pool.
        // The mailbox already owns a CPU copy; no sink last-sample is needed.
        std::string desc="pipewiresrc name=source fd="+std::to_string(fd)+" path="+std::to_string(node)+
            " do-timestamp=true always-copy=true ! video/x-raw ! videoconvert ! video/x-raw,format=RGBA ! appsink name=frames max-buffers=1 drop=true sync=false enable-last-sample=false";
        pipeline=gst_parse_launch(desc.c_str(),&error);
        if(error){std::string message=error->message;g_error_free(error);throw std::runtime_error(message);}
        if(!pipeline)throw std::runtime_error("No se pudo crear la captura");
        auto source=gst_bin_get_by_name(GST_BIN(pipeline),"source");
        auto pad=gst_element_get_static_pad(source,"src");
        gst_pad_add_probe(pad,GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
            [](GstPad*,GstPadProbeInfo *info,gpointer data)->GstPadProbeReturn {
                auto event=GST_PAD_PROBE_INFO_EVENT(info);
                if(GST_EVENT_TYPE(event)==GST_EVENT_CAPS){
                    GstCaps *caps=nullptr;gst_event_parse_caps(event,&caps);
                    int w=0,h=0;auto structure=gst_caps_get_structure(caps,0);
                    if(gst_structure_get_int(structure,"width",&w) && gst_structure_get_int(structure,"height",&h) && w>0 && h>0)
                        static_cast<LinuxCapture*>(data)->sourceSize.store((uint64_t(w)<<32)|uint32_t(h));
                }
                return GST_PAD_PROBE_OK;
            },this,nullptr);
        gst_object_unref(pad);gst_object_unref(source);
        if(gst_element_set_state(pipeline,GST_STATE_PLAYING)==GST_STATE_CHANGE_FAILURE)throw std::runtime_error("No se pudo iniciar la captura");
        return true;
    }
    void run() {
        context=g_main_context_new();g_main_context_push_thread_default(context);
        try {
            gst_init(nullptr,nullptr);
            GError *error=nullptr;
            // Each capture owns a connection. Closing it also revokes sessions
            // created by a portal reply arriving after cancellation.
            auto address=g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION,cancel,&error);
            if(address){
                bus=g_dbus_connection_new_for_address_sync(address,
                    GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT|G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
                    nullptr,cancel,&error);
                g_free(address);
            }
            if(!bus){std::string m=error?error->message:"Portal no disponible";if(error)g_error_free(error);throw std::runtime_error(m);}
            g_dbus_connection_set_exit_on_close(bus,false);
            if(setup()) {
                auto sink=GST_APP_SINK(gst_bin_get_by_name(GST_BIN(pipeline),"frames"));
                auto gstbus=gst_element_get_bus(pipeline);
                auto last=std::chrono::steady_clock::time_point{};
                auto started=std::chrono::steady_clock::now();
                ResizeRecovery recovery;
                auto millis=[] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();};
                while(!stopping && !sessionClosed) {
                    iterate();
                    const auto size=sourceSize.load();
                    recovery.size(int(size>>32),int(uint32_t(size)),millis());
                    if(recovery.reconnect(millis()) && !stopping && !sessionClosed) {
                        g_message("Guipper: reconnecting capture after resize to %u x %u",unsigned(size>>32),unsigned(uint32_t(size)));
                        gst_object_unref(gstbus);gst_object_unref(sink);gstbus=nullptr;sink=nullptr;
                        closeStream();
                        if(stopping || sessionClosed)break;
                        openStream(); // Fresh remote FD, same portal session and node.
                        sink=GST_APP_SINK(gst_bin_get_by_name(GST_BIN(pipeline),"frames"));
                        gstbus=gst_element_get_bus(pipeline);
                        last={};started=std::chrono::steady_clock::now();
                    }
                    if(last==std::chrono::steady_clock::time_point{} && std::chrono::steady_clock::now()-started>std::chrono::seconds(15)){mailbox->state(State::Error,"La pantalla no entrega imagen. Volve a seleccionarla.");break;}
                    // Throttle consumption, not delivery: dropping a damage-only
                    // window's last sample can leave its preview black forever.
                    // appsink retains the latest frame while we wait.
                    if(std::chrono::steady_clock::now()-last<std::chrono::duration<double>(1.0/30.0)) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));continue;
                    }
                    auto sample=gst_app_sink_try_pull_sample(sink,30*GST_MSECOND);
                    if(sample) {
                        auto now=std::chrono::steady_clock::now();
                        if(now-last>=std::chrono::duration<double>(1.0/30.0)) {
                            GstVideoInfo info;gst_video_info_init(&info);GstVideoFrame mapped;
                            if(gst_video_info_from_caps(&info,gst_sample_get_caps(sample)) && gst_video_frame_map(&mapped,&info,gst_sample_get_buffer(sample),GST_MAP_READ)) {
                                Frame frame;frame.width=GST_VIDEO_INFO_WIDTH(&info);frame.height=GST_VIDEO_INFO_HEIGHT(&info);
                                frame.rgba.resize(size_t(frame.width)*frame.height*4);
                                auto *bytes=static_cast<unsigned char*>(GST_VIDEO_FRAME_PLANE_DATA(&mapped,0));
                                for(int y=0;y<frame.height;++y) memcpy(frame.rgba.data()+size_t(y)*frame.width*4,bytes+y*GST_VIDEO_FRAME_PLANE_STRIDE(&mapped,0),size_t(frame.width)*4);
                                gst_video_frame_unmap(&mapped);mailbox->publish(std::move(frame));last=now;recovery.frame(millis());
                            }
                        }
                        gst_sample_unref(sample);
                    }
                    auto msg=gst_bus_pop_filtered(gstbus,GstMessageType(GST_MESSAGE_ERROR|GST_MESSAGE_EOS));
                    if(msg){
                        std::string detail="La fuente dejo de entregar imagen. Seleccionala nuevamente.";
                        if(GST_MESSAGE_TYPE(msg)==GST_MESSAGE_ERROR){
                            GError *failure=nullptr;gchar *debug=nullptr;gst_message_parse_error(msg,&failure,&debug);
                            if(failure){detail=failure->message;g_error_free(failure);}g_free(debug);
                        }
                        gst_message_unref(msg);mailbox->state(State::Error,detail);break;
                    }
                }
                if(sessionClosed)mailbox->state(State::Error,"Se dejo de compartir la pantalla. Seleccionala nuevamente.");
                if(gstbus)gst_object_unref(gstbus);
                if(sink)gst_object_unref(sink);
            }
        } catch(const std::exception &e) { if(!stopping)mailbox->state(State::Error,e.what()); }
        closeStream();
        if(bus){
            if(closedSubscription)g_dbus_connection_signal_unsubscribe(bus,closedSubscription);
            if(!session.empty())g_dbus_connection_call_sync(bus,service,session.c_str(),"org.freedesktop.portal.Session","Close",nullptr,nullptr,G_DBUS_CALL_FLAGS_NONE,1000,nullptr,nullptr);
            g_dbus_connection_close_sync(bus,nullptr,nullptr);
            g_object_unref(bus);
        }
        g_main_context_pop_thread_default(context);g_main_context_unref(context);
    }
public:
    explicit LinuxCapture(bool window = false, std::string parent = {}) : windowMode(window), parentWindow(std::move(parent)) { worker=std::thread([this]{run();}); }
    ~LinuxCapture() override { stop();g_object_unref(cancel); }
    void stop() override { stopping=true;g_cancellable_cancel(cancel);if(worker.joinable())worker.join(); }
};
}
std::unique_ptr<Backend> makeBackend(const std::string &parent) { return std::make_unique<LinuxCapture>(false,parent); }
std::unique_ptr<Backend> makeWindowBackend(const std::string &parent) { return std::make_unique<LinuxCapture>(true,parent); }
std::vector<Monitor> windows() { return {}; }
std::vector<Monitor> monitors() { return {}; }
}
#endif
