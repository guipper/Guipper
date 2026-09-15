#include "jp_uniform_parser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char**argv) {
 if(argc!=2)return 2;
 std::ifstream f(argv[1]);if(!f)return 2;
 std::string source((std::istreambuf_iterator<char>(f)),{});
 auto parsed=jp_uniform_parser::parse(source);
 nlohmann::json j={{"uniforms",nlohmann::json::array()},{"diagnostics",nlohmann::json::array()},{"has_errors",parsed.hasErrors()}};
 for(const auto& d:parsed.declarations){
  nlohmann::json v={{"name",d.name},{"type",d.typeName},{"internal",d.internal},{"array",d.array},{"line",d.location.line},{"annotations",d.annotations}};
  if(d.floatDefault)v["default"]=*d.floatDefault;
  if(d.boolDefault)v["default"]=*d.boolDefault;
  j["uniforms"].push_back(v);
 }
 for(const auto& d:parsed.diagnostics)j["diagnostics"].push_back({{"message",d.message},{"line",d.location.line},{"severity",d.severity==jp_uniform_parser::Severity::Error?"error":"warning"}});
 std::cout<<j.dump();
}
