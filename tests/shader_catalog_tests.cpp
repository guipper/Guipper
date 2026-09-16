#include "../src/JPutils/jp_shader_catalog.h"
#include <cassert>
#include <iostream>
using nlohmann::json;
json fixture(){return {{"format",1},{"approved",true},{"entries",json::array({{
 {"path","shaders/generative/original.frag"},{"sha256",std::string(64,'a')},{"category","generative"},
 {"name",{{"en","Color field"},{"es","Campo de color"}}},
 {"description",{{"en","A bright visual"},{"es","Una visual luminosa"}}},
 {"tags",{"abstract","geometria"}},{"inputs",json::array()},{"author","Test author"},
 {"license",{{"spdx","MIT"},{"source","LICENSE"},{"reviewed",true}}}
 }})}};}
int main(){
 auto j=fixture();auto e=jp_shader_catalog::parse(j).front();
 assert(e.path=="shaders/generative/original.frag");assert(e.name.get(true)=="Campo de color");
 for(const std::string q:{"original.frag","COLOR FIELD","luminosa","geometria"})assert(jp_shader_catalog::matches(e,q));
 assert(!jp_shader_catalog::matches(e,"camera"));
 for(const std::string p:{"/tmp/evil.frag","shaders/../evil.frag","shaders//x.frag","shaders/private/mix.frag","shaders/x.txt","shaders/./x.frag","shaders/a\\b.frag"})assert(!jp_shader_catalog::safePath(p));
 auto rejects=[](const json& value){try{jp_shader_catalog::parse(value);return false;}catch(const std::exception&){return true;}};
 j["entries"].push_back(j["entries"][0]);assert(rejects(j));
 j=fixture();j["format"]=2;assert(rejects(j));
 j=fixture();j["entries"][0]["license"]["reviewed"]=false;assert(rejects(j));
 j=fixture();j["entries"][0]["name"]["es"]="";assert(rejects(j));
 j=fixture();j["approved"]=false;assert(jp_shader_catalog::parse(j).empty());
 // Curation has no publication credentials and must fail closed on bad lists.
 auto curated=fixture();curated.erase("approved");
 for(auto& entry:curated["entries"]) {entry.erase("sha256");entry.erase("author");entry.erase("license");}
 assert(jp_shader_catalog::parseCurated(curated).size()==1);
 auto rejectsCurated=[](const json& value){try{jp_shader_catalog::parseCurated(value);return false;}catch(const std::exception&){return true;}};
 auto invalid=curated;invalid["entries"].push_back(invalid["entries"][0]);assert(rejectsCurated(invalid));
 invalid=curated;invalid["entries"][0]["path"]="shaders/../private.frag";assert(rejectsCurated(invalid));
 invalid=curated;invalid["entries"][0]["category"]="unknown";assert(rejectsCurated(invalid));
 invalid=curated;invalid["entries"][0]["name"]["es"]="";assert(rejectsCurated(invalid));
 invalid=curated;invalid["entries"][0]["inputs"]=42;assert(rejectsCurated(invalid));
 invalid=curated;invalid["format"]=2;assert(rejectsCurated(invalid));
 curated["entries"]=json::array();assert(jp_shader_catalog::parseCurated(curated).empty());
 std::cout<<"shader catalog tests passed\n";
}
