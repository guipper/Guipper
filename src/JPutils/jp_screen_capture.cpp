#include "jp_screen_capture.h"
#if !defined(__linux__) && !defined(_WIN32) && !defined(__APPLE__)
namespace jp_screen {
class Unsupported final : public Backend {
public:
    Unsupported() { mailbox->state(State::Unavailable,"Captura no disponible"); }
    void stop() override { mailbox->close(); }
};
std::unique_ptr<Backend> makeBackend(const std::string &) { return std::make_unique<Unsupported>(); }
std::unique_ptr<Backend> makeWindowBackend(const std::string &id) { return makeBackend(id); }
std::vector<Monitor> windows() { return {}; }
std::vector<Monitor> monitors() { return {}; }
}
#endif
