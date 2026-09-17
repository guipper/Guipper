#include "../src/JPutils/jp_toast.h"
#include <cassert>
#include <iostream>
static jp::Toast toast(std::string id, jp::ToastState state = jp::ToastState::Info) {
    jp::Toast t; t.id = id; t.message = "message"; t.state = state; return t;
}
int main() {
    using namespace jp;
    for (int state=0; state<4; ++state) {
        ToastManager m; m.publish(toast("one", static_cast<ToastState>(state)));
        const double duration = 4 + state * 2;
        m.update(duration - .1); assert(!m.find("one")->closing);
        m.update(.2); assert(m.find("one")->closing);
        m.update(.17); assert(m.items().empty());
    }
    ToastManager m;
    m.publish(toast("save", ToastState::Success));
    m.update(3); m.update(20, "save"); assert(m.find("save")->elapsed == 3);
    m.update(20, {}, true); assert(m.find("save")->elapsed == 3);
    m.publish(toast("save", ToastState::Success));
    assert(m.items().size() == 1 && m.find("save")->elapsed == 0);
    m.update(4); m.publish(toast("save")); assert(!m.find("save")->closing);
    auto recovery=toast("recovery", ToastState::Warning); recovery.duration=0;
    recovery.actions={{"recover","Recover"},{"discard","Discard"},{"ignored","Ignored"}};
    recovery.dismissAction="discard"; m.publish(recovery);
    m.publish(toast("error", ToastState::Error)); m.publish(toast("update"));
    assert(m.items().size() == 3 && !m.find("save") && m.find("recovery"));
    assert(m.find("recovery")->actions.size() == 2);
    m.update(100); m.update(.2); assert(m.items().size() == 1);
    m.activate("recovery", "recover"); m.activate("recovery", "close");
    auto events=m.takeEvents(); assert(events.size()==2 && events[0].action=="recover" && events[1].action=="discard");
    assert(m.find("recovery") && m.takeEvents().empty());
    m.activate("recovery", "invented"); assert(m.takeEvents().empty());
    m.close("recovery"); m.activate("recovery","recover"); assert(m.takeEvents().empty());
    m.update(.17); assert(m.items().empty());
    for (const auto id : {"a","b","c"}) { recovery.id=id; assert(m.publish(recovery)); }
    assert(!m.publish(toast("overflow")) && m.items().size()==3);
    std::cout << "toast tests passed\n";
}
