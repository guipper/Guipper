#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <cctype>

// Paths remain the persistent identity. Catalog labels never rename parameters,
// files, node names, MIDI bindings or favorites.
namespace jp_shader_catalog {
// Case- and accent-insensitive keys for the supported EN/ES display names.
inline std::string nameSortKey(std::string text) {
    const std::vector<std::pair<std::string,std::string>> folds = {
        {"Á","a"},{"á","a"},{"É","e"},{"é","e"},{"Í","i"},{"í","i"},
        {"Ó","o"},{"ó","o"},{"Ú","u"},{"ú","u"},{"Ü","u"},{"ü","u"},
        {"Ñ","n~"},{"ñ","n~"}};
    for (const auto& fold : folds) {
        size_t pos=0;
        while ((pos=text.find(fold.first,pos))!=std::string::npos) {
            text.replace(pos,fold.first.size(),fold.second); pos+=fold.second.size();
        }
    }
    for (char& c:text) if (static_cast<unsigned char>(c)<128) c=std::tolower(static_cast<unsigned char>(c));
    return text;
}
struct Text {
    std::string en,es;
    const std::string& get(bool spanish) const {return spanish?es:en;}
};
struct Entry {
    std::string path,sha256,category,author,license;
    Text name,description;
    bool userVisible = true, needsParameters = false;
    bool needsImprovement = false, askPupper = false;
    std::string groupPath;
    nlohmann::json previewDefaults = nlohmann::json::object();
    std::vector<std::string> tags,inputs;
    std::string searchable() const {
        std::string out=path+" "+name.en+" "+name.es+" "+description.en+" "+description.es+" "+category;
        for(const auto& tag:tags)out+=" "+tag;
        return out;
    }
};
inline bool safePath(const std::string& path) {
    if(path.rfind("shaders/",0)!=0 || path.size()<6 || path.substr(path.size()-5)!=".frag" ||
       path.find('\\')!=std::string::npos || path.find(':')!=std::string::npos || path.find('\0')!=std::string::npos)return false;
    size_t begin=0;
    while(begin<path.size()) {
        size_t end=path.find('/',begin);if(end==std::string::npos)end=path.size();
        const auto part=path.substr(begin,end-begin);
        if(part.empty()||part=="."||part=="..")return false;
        begin=end+1;
    }
    return path.rfind("shaders/private/",0)!=0;
}
inline std::string categoryName(const std::string& id,bool es) {
    if(id=="generative")return es?"Generativos":"Generative";
    if(id=="effects")return es?"Efectos":"Effects";
    if(id=="mixers")return es?"Mezcladores":"Mixers";
    if(id=="contrib")return es?"Contribuciones":"Contributions";
    if(id=="combo")return es?"Combinados":"Combined";
    if(id=="internal")return es?"Auxiliares / internos":"Helpers / internal";
    return id;
}
inline nlohmann::json previewDefaults(const nlohmann::json& value) {
    const auto defaults=value.value("preview_defaults",nlohmann::json::object());
    if(!defaults.is_object()) throw std::runtime_error("Invalid preview defaults");
    for(const auto& item:defaults.items()) if(!item.value().is_boolean() && !item.value().is_number())
        throw std::runtime_error("Invalid preview parameter value");
    return defaults;
}
inline std::vector<Entry> parse(const nlohmann::json& root) {
    if(root.at("format")!=1)throw std::runtime_error("Unsupported shader catalog format");
    if(!root.at("approved").get<bool>())return {};
    if(!root.at("entries").is_array())throw std::runtime_error("Catalog entries must be an array");
    std::vector<Entry> result;std::set<std::string> paths;
    for(const auto& value:root.at("entries")) {
        Entry e;e.previewDefaults=previewDefaults(value);e.path=value.at("path");e.sha256=value.at("sha256");e.category=value.at("category");
        if(!safePath(e.path)||!paths.insert(e.path).second)throw std::runtime_error("Unsafe or duplicate catalog path");
        if(e.sha256.size()!=64||e.sha256.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid catalog hash");
        if(e.category!="generative"&&e.category!="effects"&&e.category!="mixers")throw std::runtime_error("Unknown shader category");
        auto text=[](const nlohmann::json& j){Text t{j.at("en"),j.at("es")};if(t.en.empty()||t.es.empty())throw std::runtime_error("Missing catalog translation");return t;};
        e.name=text(value.at("name"));e.description=text(value.at("description"));
        e.author=value.at("author");e.license=value.at("license").at("spdx");
        if(e.author.empty()||e.license.empty()||!value.at("license").at("reviewed").get<bool>()||value.at("license").at("source").get<std::string>().empty())throw std::runtime_error("Unreviewed catalog attribution");
        e.tags=value.at("tags").get<std::vector<std::string>>();e.inputs=value.at("inputs").get<std::vector<std::string>>();
        result.push_back(e);
    }
    return result;
}
// Local curation is deliberately separate from publication approval.
inline std::vector<Entry> parseCurated(const nlohmann::json& root) {
    if (!root.at("format").is_number_integer() || root.at("format") != 1 ||
        !root.at("entries").is_array()) throw std::runtime_error("Invalid curated list");
    std::vector<Entry> result;
    std::set<std::string> paths;
    for (const auto& value : root.at("entries")) {
        Entry e;
        e.previewDefaults=previewDefaults(value);
        e.path = value.at("path").get<std::string>();
        if (!(safePath(e.path) || (e.path.rfind("shaders/private/",0)==0 && safePath("shaders/review/"+e.path.substr(16)))) || !paths.insert(e.path).second)
            throw std::runtime_error("Unsafe or duplicate curated path");
        e.category = value.at("category").get<std::string>();
        if (e.category != "generative" && e.category != "effects" && e.category != "mixers" && e.category != "contrib" && e.category != "combo" && e.category != "internal")
            throw std::runtime_error("Unknown curated category");
        auto text = [](const nlohmann::json& j) {
            Text t{j.at("en").get<std::string>(), j.at("es").get<std::string>()};
            if (t.en.empty() || t.es.empty()) throw std::runtime_error("Missing curated translation");
            return t;
        };
        e.userVisible = value.value("user_visible", true);
        e.needsParameters = value.value("needs_parameters", false);
        e.needsImprovement = value.value("needs_improvement", false);
        e.askPupper = value.value("ask_pupper", false);
        e.groupPath = value.value("group_path", std::string());
        if (e.groupPath.find('\0') != std::string::npos || (!e.groupPath.empty() &&
            (e.groupPath.size()<4 || e.groupPath.substr(e.groupPath.size()-4)!=".xml")))
            throw std::runtime_error("Invalid curated group path");
        e.name = text(value.at("name"));
        e.description = text(value.at("description"));
        e.tags = value.at("tags").get<std::vector<std::string>>();
        e.inputs = value.at("inputs").get<std::vector<std::string>>();
        result.push_back(e);
    }
    return result;
}
inline bool matches(const Entry& e,const std::string& query) {
    auto lower=[](std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::tolower(c);});return s;};
    return lower(e.searchable()).find(lower(query))!=std::string::npos;
}
}
