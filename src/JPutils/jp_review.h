#pragma once
#include "jp_shader_catalog.h"
#include "jp_app_paths.h"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <filesystem>
namespace jp_review {
using Json=nlohmann::json;
namespace fs=std::filesystem;
struct Field {
    Json value;
    std::vector<std::string> heads;
    bool conflict=false;
};
struct Snapshot {
    Json project;
    std::map<std::string,Json> events;
    std::map<std::string,std::map<std::string,Field>> fields;
    Json catalog;
    std::vector<std::string> errors;
    size_t pending=0, deferred=0;
    bool folderAvailable=false;
};
struct Config { fs::path shared, local, catalog; std::string author; };
std::string uuid();
std::string fingerprint(const fs::path& file);
std::vector<std::string> sharedFields();
Json initialValue(const Json& entry,const std::string& field);
void setValue(Json& entry,const std::string& field,const Json& value);
Snapshot reduce(const Json& project,const std::vector<Json>& events);
Json create(const fs::path& shared,const Json& catalog);
Json open(const Config& config);
Snapshot synchronize(const Config& config);
Json change(const Snapshot& snapshot,const std::string& shader,const std::string& field,const Json& value,const std::string& author,const std::string& hash);
Json comment(const Snapshot& snapshot,const std::string& shader,const std::string& kind,const std::string& text,const std::string& author,const std::string& hash);
void enqueue(const Config& config,const Json& event);
void materialize(const Snapshot& snapshot,const fs::path& catalog);
}
