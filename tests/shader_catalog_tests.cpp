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
 std::cout<<"shader catalog tests passed\n";
}
