// Real Linux backend against a private D-Bus portal; no desktop permissions,
// dialogs, PipeWire streams or user session are touched.
#include "../src/JPutils/jp_screen_capture.h"
#include <gio/gio.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <filesystem>
using namespace jp_screen;
using namespace std::chrono_literals;
namespace {
const char *name="org.freedesktop.portal.Desktop";
const char *root="/org/freedesktop/portal/desktop";
const char *session="/org/freedesktop/portal/desktop/session/test";
enum Mode { Cancel, Wait, SessionClosed, OwnerLost, NoCreateResponse, Remember };
struct Portal {
    std::atomic<int> mode{Cancel}, starts{0}, closes{0};
    std::atomic<bool> ready{false}, reacquired{false};
    std::atomic<unsigned> version{5}, savedSerial{0};
    std::mutex recordedMutex;std::string restored;unsigned persistence=0,sourceType=0;
    GMainContext *context=nullptr;GMainLoop *loop=nullptr;GDBusConnection *bus=nullptr;
    GDBusNodeInfo *info=nullptr;std::thread thread;unsigned serial=0;
    static void invoke(GDBusConnection*,const gchar *sender,const gchar *path,const gchar *interface,const gchar *method,GVariant *args,GDBusMethodInvocation *reply,gpointer user){
        static_cast<Portal*>(user)->call(sender,path,interface,method,args,reply);
    }
    void own(){auto v=g_dbus_connection_call_sync(bus,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","RequestName",g_variant_new("(su)",name,0),nullptr,G_DBUS_CALL_FLAGS_NONE,2000,nullptr,nullptr);assert(v);g_variant_unref(v);}
    static GVariant *property(GDBusConnection*,const gchar*,const gchar*,const gchar*,const gchar*,GError**,gpointer user){return g_variant_new_uint32(static_cast<Portal*>(user)->version.load());}
    void call(const char *sender,const char*,const char *interface,const char *method,GVariant *args,GDBusMethodInvocation *reply){
        if(std::string(method)=="Close"){if(std::string(interface)=="org.freedesktop.portal.Request")++closes;g_dbus_method_invocation_return_value(reply,nullptr);return;}
        // Deliberately return a non-predicted handle, then emit immediately:
        // exercises the old ignored-handle/racing-response bug.
        std::string request=std::string(root)+"/request/legacy_"+std::to_string(++serial);
        static const GDBusInterfaceVTable table={invoke,nullptr,nullptr,{nullptr}};
        g_dbus_connection_register_object(bus,request.c_str(),info->interfaces[1],&table,this,nullptr,nullptr);
        g_dbus_method_invocation_return_value(reply,g_variant_new("(o)",request.c_str()));
        GVariantBuilder data;g_variant_builder_init(&data,G_VARIANT_TYPE_VARDICT);
        unsigned code=0;
        if(std::string(method)=="SelectSources"){
            auto options=g_variant_get_child_value(args,1);const char *token=nullptr;
            std::lock_guard<std::mutex> lock(recordedMutex);restored.clear();persistence=0;sourceType=0;
            if(g_variant_lookup(options,"restore_token","&s",&token))restored=token;
            g_variant_lookup(options,"persist_mode","u",&persistence);g_variant_lookup(options,"types","u",&sourceType);g_variant_unref(options);
        }
        if(std::string(method)=="CreateSession"){
            if(mode==NoCreateResponse)return;
            g_variant_builder_add(&data,"{sv}","session_handle",g_variant_new_string(session));
        }
        if(std::string(method)=="Start"){
            ++starts;
            if(mode==Wait)return;
            if(mode==SessionClosed){g_dbus_connection_emit_signal(bus,sender,session,"org.freedesktop.portal.Session","Closed",g_variant_new("(a{sv})",nullptr),nullptr);return;}
            if(mode==OwnerLost){auto v=g_dbus_connection_call_sync(bus,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","ReleaseName",g_variant_new("(s)",name),nullptr,G_DBUS_CALL_FLAGS_NONE,2000,nullptr,nullptr);assert(v);g_variant_unref(v);return;}
            if(mode==Remember)g_variant_builder_add(&data,"{sv}","restore_token",g_variant_new_string(("saved-"+std::to_string(++savedSerial)).c_str()));
            else code=1;
        }
        g_dbus_connection_emit_signal(bus,sender,request.c_str(),"org.freedesktop.portal.Request","Response",g_variant_new("(u@a{sv})",code,g_variant_builder_end(&data)),nullptr);
    }
    Portal(){thread=std::thread([this]{
        context=g_main_context_new();g_main_context_push_thread_default(context);loop=g_main_loop_new(context,false);
        bus=g_dbus_connection_new_for_address_sync(g_getenv("DBUS_SESSION_BUS_ADDRESS"),GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT|G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),nullptr,nullptr,nullptr);assert(bus);own();
        info=g_dbus_node_info_new_for_xml(R"(<node>
          <interface name='org.freedesktop.portal.ScreenCast'>
            <property name='version' type='u' access='read'/>
            <method name='CreateSession'><arg type='a{sv}' direction='in'/><arg type='o' direction='out'/></method>
            <method name='SelectSources'><arg type='o' direction='in'/><arg type='a{sv}' direction='in'/><arg type='o' direction='out'/></method>
            <method name='Start'><arg type='o' direction='in'/><arg type='s' direction='in'/><arg type='a{sv}' direction='in'/><arg type='o' direction='out'/></method>
          </interface>
          <interface name='org.freedesktop.portal.Request'><method name='Close'/><signal name='Response'><arg type='u'/><arg type='a{sv}'/></signal></interface>
          <interface name='org.freedesktop.portal.Session'><method name='Close'/><signal name='Closed'><arg type='a{sv}'/></signal></interface>
        </node>)",nullptr);assert(info);
        static const GDBusInterfaceVTable table={invoke,property,nullptr,{nullptr}};
        assert(g_dbus_connection_register_object(bus,root,info->interfaces[0],&table,this,nullptr,nullptr));
        assert(g_dbus_connection_register_object(bus,session,info->interfaces[2],&table,this,nullptr,nullptr));
        ready=true;g_main_loop_run(loop);
        g_dbus_connection_close_sync(bus,nullptr,nullptr);g_object_unref(bus);g_dbus_node_info_unref(info);
        g_main_loop_unref(loop);g_main_context_pop_thread_default(context);g_main_context_unref(context);
    });while(!ready)std::this_thread::sleep_for(1ms);}
    void reacquire(){g_main_context_invoke(context,[](gpointer p)->gboolean{auto &self=*static_cast<Portal*>(p);self.own();self.reacquired=true;return G_SOURCE_REMOVE;},this);while(!reacquired)std::this_thread::sleep_for(1ms);}
    ~Portal(){g_main_loop_quit(loop);thread.join();}
};
template<class Predicate> void until(Predicate predicate,int seconds=4){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);while(!predicate()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(5ms);}}
}
int main(){
    auto *dbus=g_test_dbus_new(G_TEST_DBUS_NONE);g_test_dbus_up(dbus);
    {
        Portal portal;
        Capture capture;
        for(int i=0;i<10;++i){capture.select();until([&]{return capture.update().state!=State::Selecting;});assert(capture.snapshot().state==State::Idle);}
        assert(portal.starts==10);
        portal.mode=Wait;capture.select();until([&]{return portal.starts==11;});auto start=std::chrono::steady_clock::now();capture.cancelSelection();
        assert(std::chrono::steady_clock::now()-start<2s);assert(portal.closes>=11);assert(capture.snapshot().state==State::Idle);
        portal.mode=SessionClosed;capture.select();until([&]{return capture.update().state==State::Error;});
        portal.mode=Cancel;capture.select();until([&]{return capture.update().state!=State::Selecting;});assert(portal.starts==13);
        portal.mode=OwnerLost;capture.select();until([&]{return capture.update().state==State::Error;});portal.reacquire();
        portal.mode=Cancel;capture.select();until([&]{return capture.update().state!=State::Selecting;});assert(portal.starts==15);
        portal.mode=NoCreateResponse;capture.select();until([&]{return capture.update().state==State::Error;},20);
        portal.mode=Cancel;capture.select();until([&]{return capture.update().state!=State::Selecting;});assert(portal.starts==16);
        Capture window(makeWindowBackend);window.select();until([&]{return window.update().state!=State::Selecting;});assert(portal.starts==17);
        capture.stop();window.stop();
        gchar *dir=g_dir_make_tmp("guipper-portal-restore-XXXXXX",nullptr);assert(dir);
        const std::string directory=dir;g_free(dir);
        bool resume=false;
        auto savedCapture=[&]{return std::make_unique<Capture>([&](const std::string &parent){return makePortalBackend(parent,true,directory,"saved-box",resume);});};
        auto remembered=savedCapture();portal.mode=Remember;
        // No streams in this fixture: Start succeeds and rotates the token,
        // then the backend reports an error without opening real PipeWire.
        auto startRemembered=[&]{remembered->select();until([&]{return remembered->update().state!=State::Selecting;});};
        startRemembered();assert(portal.savedSerial==1);
        {std::lock_guard<std::mutex> lock(portal.recordedMutex);assert(portal.persistence==2&&portal.restored.empty()&&portal.sourceType==2);}
        remembered.reset();resume=true;remembered=savedCapture();startRemembered();
        {std::lock_guard<std::mutex> lock(portal.recordedMutex);assert(portal.restored=="saved-1");}
        startRemembered();
        {std::lock_guard<std::mutex> lock(portal.recordedMutex);assert(portal.restored=="saved-2");}
        // Changing a source never sends the saved token. Cancelling the
        // chooser keeps the previous selection available for a later resume.
        resume=false;portal.mode=Cancel;startRemembered();
        {std::lock_guard<std::mutex> lock(portal.recordedMutex);assert(portal.restored.empty());}
        resume=true;portal.mode=Remember;startRemembered();
        {std::lock_guard<std::mutex> lock(portal.recordedMutex);assert(portal.restored=="saved-3");}
        portal.version=3;startRemembered();
        {std::lock_guard<std::mutex> lock(portal.recordedMutex);assert(portal.persistence==0&&portal.restored.empty());}
        remembered.reset();std::filesystem::remove_all(directory);
    }
    g_test_dbus_down(dbus);g_object_unref(dbus);
    std::cout<<"screen_portal_tests: PASS (selection, cancellation, restart, timeout, persistent restore, token rotation and old portals)\n";
}
