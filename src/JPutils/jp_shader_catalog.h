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
struct Text {
    std::string en,es;
    const std::string& get(bool spanish) const {return spanish?es:en;}
};
struct Entry {
    std::string path,sha256,category,author,license;
    Text name,description;
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
    return id;
}
inline std::vector<Entry> parse(const nlohmann::json& root) {
    if(root.at("format")!=1)throw std::runtime_error("Unsupported shader catalog format");
    if(!root.at("approved").get<bool>())return {};
    if(!root.at("entries").is_array())throw std::runtime_error("Catalog entries must be an array");
    std::vector<Entry> result;std::set<std::string> paths;
    for(const auto& value:root.at("entries")) {
        Entry e;e.path=value.at("path");e.sha256=value.at("sha256");e.category=value.at("category");
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
inline bool matches(const Entry& e,const std::string& query) {
    auto lower=[](std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::tolower(c);});return s;};
    return lower(e.searchable()).find(lower(query))!=std::string::npos;
}
}
