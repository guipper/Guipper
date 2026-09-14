#pragma once
#include <future>
#include <string>
#include "jp_app_paths.h"
class JPboxgroup;
namespace jp {
// Snapshots are captured on the main thread. The worker receives only bytes,
// never live nodes, parameters, OpenGL objects or ofXml handles.
class RecoveryService {
public:
    void tick(JPboxgroup& group, double now);
    std::string pending() const;
    void dismiss();
    void finish(bool clean);
    void markSaved(JPboxgroup& group, bool clearPending = true);
    ~RecoveryService();
private:
    double next = 120.0;
    std::string previous;
    std::future<void> writer;
};
}
