#ifdef __APPLE__
#include "../jp_screen_capture.h"
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <chrono>

// The observer is retained by its backend; asynchronous completions never hold
// a C++ backend pointer. All picker operations occur on the main thread.
API_AVAILABLE(macos(14.0))
@interface JPScreenObserver : NSObject <SCContentSharingPickerObserver, SCStreamOutput, SCStreamDelegate> {
@public
    std::shared_ptr<jp_screen::Mailbox> mailbox;
    SCStream *stream;
    bool stopped;
    std::chrono::steady_clock::time_point lastFrame;
}
- (void)closeCapture;
@end
static JPScreenObserver *pendingScreenPicker = nil;
static unsigned screenObserverCount = 0;
@implementation JPScreenObserver
- (void)contentSharingPicker:(SCContentSharingPicker *)picker didCancelForStream:(SCStream *)cancelledStream {
    if(stopped || pendingScreenPicker!=self || (cancelledStream && cancelledStream!=stream))return;
    pendingScreenPicker=nil;
    mailbox->state(jp_screen::State::Cancelled);
}
- (void)contentSharingPickerStartDidFailWithError:(NSError *)error {
    if(!stopped && pendingScreenPicker==self){pendingScreenPicker=nil;mailbox->state(jp_screen::State::Error,error.localizedDescription.UTF8String);}
}
- (void)contentSharingPicker:(SCContentSharingPicker *)picker didUpdateWithFilter:(SCContentFilter *)filter forStream:(SCStream *)updatedStream {
    if(stopped || (updatedStream ? updatedStream!=stream : pendingScreenPicker!=self))return;
    pendingScreenPicker=nil;
    SCStreamConfiguration *config=[[SCStreamConfiguration alloc] init];
    config.width=MAX(1,NSInteger(filter.contentRect.size.width*filter.pointPixelScale));
    config.height=MAX(1,NSInteger(filter.contentRect.size.height*filter.pointPixelScale));
    config.pixelFormat=kCVPixelFormatType_32BGRA;
    config.minimumFrameInterval=CMTimeMake(1,30);config.queueDepth=3;
    config.showsCursor=YES;config.capturesAudio=NO;
    if(stream){
        SCStream *old=stream;stream=nil;
        [old removeStreamOutput:self type:SCStreamOutputTypeScreen error:nil];
        [self retain];
        [old stopCaptureWithCompletionHandler:^(NSError *error){[old release];[self release];}];
    }
    stream=[[SCStream alloc] initWithFilter:filter configuration:config delegate:self];
    [config release];
    NSError *error=nil;
    if(![stream addStreamOutput:self type:SCStreamOutputTypeScreen sampleHandlerQueue:dispatch_get_main_queue() error:&error]) {
        mailbox->state(jp_screen::State::Error,error.localizedDescription.UTF8String);return;
    }
    auto target=mailbox;
    [stream startCaptureWithCompletionHandler:^(NSError *failure){if(failure)target->state(jp_screen::State::Error,failure.localizedDescription.UTF8String);}];
}
- (void)stream:(SCStream *)source didStopWithError:(NSError *)error {
    if(!stopped && source==stream)mailbox->state(jp_screen::State::Error,error.localizedDescription.UTF8String);
}
- (void)stream:(SCStream *)source didOutputSampleBuffer:(CMSampleBufferRef)sample ofType:(SCStreamOutputType)type {
    if(stopped || source!=stream || type!=SCStreamOutputTypeScreen || !CMSampleBufferIsValid(sample))return;
    CFArrayRef attachments=CMSampleBufferGetSampleAttachmentsArray(sample,false);
    if(!attachments || !CFArrayGetCount(attachments))return;
    NSDictionary *metadata=(NSDictionary *)CFArrayGetValueAtIndex(attachments,0);
    if([metadata[SCStreamFrameInfoStatus] integerValue]!=SCFrameStatusComplete)return;
    auto now=std::chrono::steady_clock::now();if(now-lastFrame<std::chrono::milliseconds(33))return;
    CVPixelBufferRef pixels=CMSampleBufferGetImageBuffer(sample);if(!pixels)return;
    if(CVPixelBufferLockBaseAddress(pixels,kCVPixelBufferLock_ReadOnly)!=kCVReturnSuccess)return;
    jp_screen::Frame frame;frame.width=int(CVPixelBufferGetWidth(pixels));frame.height=int(CVPixelBufferGetHeight(pixels));
    frame.rgba.resize(size_t(frame.width)*frame.height*4);
    auto *base=static_cast<const unsigned char*>(CVPixelBufferGetBaseAddress(pixels));auto stride=CVPixelBufferGetBytesPerRow(pixels);
    for(int y=0;y<frame.height;++y){auto src=base+y*stride;auto dst=frame.rgba.data()+size_t(y)*frame.width*4;
        for(int x=0;x<frame.width;++x){dst[x*4]=src[x*4+2];dst[x*4+1]=src[x*4+1];dst[x*4+2]=src[x*4];dst[x*4+3]=255;}}
    CVPixelBufferUnlockBaseAddress(pixels,kCVPixelBufferLock_ReadOnly);lastFrame=now;mailbox->publish(std::move(frame));
}
- (void)closeCapture {
    if(stopped)return;
    stopped=true;mailbox->close();
    if(pendingScreenPicker==self)pendingScreenPicker=nil;
    [[SCContentSharingPicker sharedPicker] removeObserver:self];
    if(screenObserverCount && --screenObserverCount==0)[SCContentSharingPicker sharedPicker].active=NO;
    if(stream){
        SCStream *closing=stream;stream=nil;
        [closing removeStreamOutput:self type:SCStreamOutputTypeScreen error:nil];
        [self retain];
        [closing stopCaptureWithCompletionHandler:^(NSError *error){[closing release];[self release];}];
    }
}
- (void)dealloc { [self closeCapture];[super dealloc]; }
@end
namespace jp_screen {
namespace {
class MacCapture final:public Backend {
    JPScreenObserver *observer=nil;
public:
    explicit MacCapture(bool window = false){
        if(@available(macOS 14.0,*)){
            if(pendingScreenPicker){mailbox->state(State::Error,"Termina la seleccion de pantalla pendiente e intenta nuevamente.");return;}
            observer=[[JPScreenObserver alloc]init];pendingScreenPicker=observer;observer->mailbox=mailbox;observer->stopped=false;
            auto picker=[SCContentSharingPicker sharedPicker];
            // The system default is one stream; each box owns an independent feed.
            picker.maximumStreamCount=nil;
            SCContentSharingPickerConfiguration *config=[[SCContentSharingPickerConfiguration alloc]init];
            config.allowedPickerModes=window ? SCContentSharingPickerModeSingleWindow : SCContentSharingPickerModeSingleDisplay;
            config.allowsChangingSelectedContent=NO;
            picker.defaultConfiguration=config;[config release];
            [picker addObserver:observer];++screenObserverCount;picker.active=YES;[picker presentPickerUsingContentStyle:(window ? SCShareableContentStyleWindow : SCShareableContentStyleDisplay)];
        }else mailbox->state(State::Unavailable,"Captura no disponible: requiere macOS 14 o posterior");
    }
    ~MacCapture()override{stop();}
    void stop()override{
        if(@available(macOS 14.0,*)){if(observer){[observer closeCapture];[observer release];observer=nil;}}
    }
};
}
std::unique_ptr<Backend> makeBackend(const std::string &){return std::make_unique<MacCapture>();}
std::unique_ptr<Backend> makeWindowBackend(const std::string &){return std::make_unique<MacCapture>(true);}
std::vector<Monitor> windows(){return {};}
std::vector<Monitor> monitors(){return {};}
}
#endif
