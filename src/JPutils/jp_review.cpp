#include "jp_review.h"
#include <random>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <functional>
#include <algorithm>
namespace jp_review {
namespace {
Json read(const fs::path& p) {
    if(fs::file_size(p)>16*1024*1024)throw std::runtime_error("Review file too large: "+p.filename().string());
    return Json::parse(jp::readBytes(p));
}
void immutable(const fs::path& p,const Json& value) {
    if(fs::exists(p)) {if(read(p)!=value)throw std::runtime_error("Conflicting immutable file: "+p.filename().string());return;}
    jp::atomicWrite(p,value.dump(2)+"\n");
}
void validateProject(const Json& p) {
    const auto id=p.at("id").get<std::string>();
    if(p.at("format")!=1 || id.size()!=32 || id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid review project");
    jp_shader_catalog::parseCurated(p.at("catalog"));
}
std::string key(const Json& e){return e.at("shader").get<std::string>()+"\n"+e.at("field").get<std::string>();}
void validateValue(const std::string& field,const Json& value) {
    if(field=="name.en" || field=="name.es") {if(!value.is_string() || value.get<std::string>().empty() || value.get<std::string>().size()>512)throw std::runtime_error("Invalid shared name");}
    else if(field=="preview_defaults") {jp_shader_catalog::previewDefaults(Json{{"preview_defaults",value}});}
    else if(!value.is_boolean())throw std::runtime_error("Invalid review flag");
}
void validateEvent(const Json& e,const Json& p,const std::set<std::string>& shaders) {
    const auto id=e.at("id").get<std::string>();
    if(e.at("format")!=1 || e.at("project")!=p.at("id") || id.size()!=32 || id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid review event identity");
    if(!shaders.count(e.at("shader").get<std::string>()))throw std::runtime_error("Unknown review shader");
    if(!e.at("author").is_string() || e.at("author").get<std::string>().empty() || e.at("author").get<std::string>().size()>128)throw std::runtime_error("Invalid reviewer");
    if(!e.at("time").is_number_integer() || !e.at("source_hash").is_string())throw std::runtime_error("Invalid review event metadata");
    const auto type=e.at("type").get<std::string>();
    if(type=="comment") {
        const auto kind=e.at("kind").get<std::string>();
        if(kind!="comment" && kind!="keep" && kind!="modify" && kind!="remove")throw std::runtime_error("Invalid proposal");
        const auto text=e.at("text").get<std::string>();
        if(text.empty() || text.size()>16384)throw std::runtime_error("Invalid comment length");
    } else if(type=="change") {
        const auto fields=sharedFields();const auto f=e.at("field").get<std::string>();
        if(std::find(fields.begin(),fields.end(),f)==fields.end())throw std::runtime_error("Unknown shared field");
        validateValue(f,e.at("value"));
        const auto parents=e.at("parents").get<std::vector<std::string>>();
        if(parents.size()>1024 || std::find(parents.begin(),parents.end(),id)!=parents.end())throw std::runtime_error("Invalid revision parents");
    } else throw std::runtime_error("Unknown review event type");
}
Json envelope(const Snapshot& s,const std::string& shader,const std::string& author,const std::string& hash) {
    return {{"format",1},{"project",s.project.at("id")},{"id",uuid()},{"shader",shader},{"author",author},
        {"time",std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()},{"source_hash",hash}};
}
}
std::string uuid() {std::random_device random;std::ostringstream out;for(int i=0;i<4;++i)out<<std::hex<<std::setw(8)<<std::setfill('0')<<uint32_t(random());return out.str();}
std::string fingerprint(const fs::path& file) {
    std::ifstream in(file,std::ios::binary);if(!in)return "missing";
    uint64_t hash=14695981039346656037ull;char buffer[8192];
    while(in){in.read(buffer,sizeof buffer);for(std::streamsize i=0;i<in.gcount();++i){hash^=static_cast<unsigned char>(buffer[i]);hash*=1099511628211ull;}}
    std::ostringstream out;out<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;return out.str();
}
std::vector<std::string> sharedFields(){return {"user_visible","needs_parameters","needs_improvement","ask_pupper","name.en","name.es","preview_defaults"};}
Json initialValue(const Json& e,const std::string& f) {
    if(f=="name.en")return e.at("name").at("en");
    if(f=="name.es")return e.at("name").at("es");
    if(f=="preview_defaults")return e.value(f,Json::object());
    return e.value(f,f=="user_visible");
}
void setValue(Json& e,const std::string& f,const Json& v) {if(f=="name.en")e["name"]["en"]=v;else if(f=="name.es")e["name"]["es"]=v;else e[f]=v;}
Snapshot reduce(const Json& project,const std::vector<Json>& input) {
    validateProject(project);Snapshot s;s.project=project;s.catalog=project.at("catalog");
    std::set<std::string> shaders,badIds;for(const auto& e:s.catalog.at("entries"))shaders.insert(e.at("path"));
    std::map<std::string,Json> valid;
    for(const auto& e:input)try {validateEvent(e,project,shaders);const std::string id=e.at("id");
        if(valid.count(id) && valid.at(id)!=e){badIds.insert(id);throw std::runtime_error("Duplicate revision ID with different contents");}valid[id]=e;
    }catch(const std::exception& error){s.errors.push_back(error.what());}
    for(const auto& id:badIds)valid.erase(id);
    std::map<std::string,std::set<std::string>> ancestors;
    bool progress=true;
    while(progress){progress=false;for(const auto& item:valid){const auto& id=item.first;const auto& e=item.second;if(s.events.count(id))continue;
        if(e.at("type")=="comment"){s.events[id]=e;progress=true;continue;}
        bool ready=true;std::set<std::string> a{id};
        for(const auto& parent:e.at("parents")){const auto p=parent.get<std::string>();
            if(!s.events.count(p) || s.events.at(p).at("type")!="change" || key(s.events.at(p))!=key(e)){ready=false;break;}
            a.insert(ancestors[p].begin(),ancestors[p].end());
        }
        if(ready){s.events[id]=e;ancestors[id]=std::move(a);progress=true;}
    }}
    s.deferred=valid.size()-s.events.size();
    std::map<std::string,std::set<std::string>> heads;
    for(const auto& item:s.events)if(item.second.at("type")=="change")heads[key(item.second)].insert(item.first);
    for(const auto& item:s.events)if(item.second.at("type")=="change")for(const auto& parent:item.second.at("parents"))heads[key(item.second)].erase(parent.get<std::string>());
    for(auto& entry:s.catalog["entries"])for(const auto& field:sharedFields()){
        const std::string path=entry.at("path");const Json baseline=initialValue(entry,field);const auto& h=heads[path+"\n"+field];
        Field result;result.heads.assign(h.begin(),h.end());
        std::function<Json(const std::set<std::string>&)> commonValue=[&](const std::set<std::string>& ids)->Json {
            if(ids.empty())return baseline;
            const Json first=s.events.at(*ids.begin()).at("value");bool equal=true;for(const auto& id:ids)equal=equal && s.events.at(id).at("value")==first;
            if(equal)return first;
            std::set<std::string> common=ancestors.at(*ids.begin());
            for(const auto& id:ids){std::set<std::string> intersection;const auto& a=ancestors.at(id);std::set_intersection(common.begin(),common.end(),a.begin(),a.end(),std::inserter(intersection,intersection.begin()));common=std::move(intersection);}
            auto maximal=common;for(const auto& a:common)for(const auto& b:common)if(a!=b && ancestors.at(b).count(a))maximal.erase(a);
            return commonValue(maximal);
        };
        if(!h.empty()){const Json first=s.events.at(*h.begin()).at("value");for(const auto& id:h)if(s.events.at(id).at("value")!=first)result.conflict=true;}
        result.value=commonValue(h);setValue(entry,field,result.value);s.fields[path][field]=result;
    }
    return s;
}
Json create(const fs::path& shared,const Json& catalog) {
    jp_shader_catalog::parseCurated(catalog);
    if(!fs::is_directory(shared) || fs::directory_iterator(shared)!=fs::directory_iterator())throw std::runtime_error("Choose an empty folder to create a review");
    Json clean=catalog;for(auto& e:clean["entries"])e["group_path"]="";
    if(!fs::create_directory(shared/".creating"))throw std::runtime_error("Another reviewer is creating this project");
    try {
        if(fs::exists(shared/"review-project.json"))throw std::runtime_error("Project already exists");
        Json project={{"format",1},{"id",uuid()},{"catalog",clean}};immutable(shared/"review-project.json",project);fs::create_directories(shared/"events");fs::remove(shared/".creating");return project;
    }catch(...){fs::remove(shared/".creating");throw;}
}
Json open(const Config& c) {
    Json p=read(c.shared/"review-project.json");validateProject(p);
    if(fs::exists(c.local/"review-project.json") && read(c.local/"review-project.json").at("id")!=p.at("id"))throw std::runtime_error("Different review in local cache");
    immutable(c.local/"review-project.json",p);return p;
}
void enqueue(const Config& c,const Json& e) {
    const Json p=read(c.local/"review-project.json");std::set<std::string> shaders;for(const auto& entry:p.at("catalog").at("entries"))shaders.insert(entry.at("path"));validateEvent(e,p,shaders);
    immutable(c.local/"outbox"/(e.at("id").get<std::string>()+".json"),e);
}
Snapshot synchronize(const Config& c) {
    const auto project=read(c.local/"review-project.json");validateProject(project);
    std::vector<std::string> errors;bool online=false;
    try {auto remote=read(c.shared/"review-project.json");validateProject(remote);if(remote!=project)throw std::runtime_error("Shared project changed; refusing to mix reviews");online=true;}
    catch(const std::exception& e){errors.push_back(e.what());}
    auto files=[&](const fs::path& dir){std::vector<fs::path> paths;std::error_code ec;for(fs::directory_iterator it(dir,ec),end;!ec && it!=end;it.increment(ec))if(it->is_regular_file() && it->path().extension()==".json")paths.push_back(it->path());if(ec && fs::exists(dir))errors.push_back("Cannot read "+dir.string()+": "+ec.message());return paths;};
    if(online)for(const auto& path:files(c.shared/"events"))try {
        const auto e=read(path);std::set<std::string> shaders;for(const auto& entry:project.at("catalog").at("entries"))shaders.insert(entry.at("path"));validateEvent(e,project,shaders);
        immutable(c.local/"events"/(e.at("id").get<std::string>()+".json"),e);
    }catch(const std::exception& e){errors.push_back(path.filename().string()+": "+e.what());}
    size_t pending=0;
    for(const auto& path:files(c.local/"outbox"))try {const auto e=read(path);immutable(c.local/"events"/path.filename(),e);
        if(online){immutable(c.shared/"events"/path.filename(),e);fs::remove(path);}else ++pending;
    }catch(const std::exception& e){++pending;errors.push_back(e.what());}
    std::vector<Json> events;for(const auto& path:files(c.local/"events"))try{events.push_back(read(path));}catch(const std::exception& e){errors.push_back(e.what());}
    auto result=reduce(project,events);result.pending=pending;result.folderAvailable=online;result.errors.insert(result.errors.end(),errors.begin(),errors.end());return result;
}
Json change(const Snapshot& s,const std::string& shader,const std::string& field,const Json& value,const std::string& author,const std::string& hash) {
    validateValue(field,value);Json e=envelope(s,shader,author,hash);e["type"]="change";e["field"]=field;e["value"]=value;e["parents"]=s.fields.at(shader).at(field).heads;return e;
}
Json comment(const Snapshot& s,const std::string& shader,const std::string& kind,const std::string& text,const std::string& author,const std::string& hash) {
    Json e=envelope(s,shader,author,hash);e["type"]="comment";e["kind"]=kind;e["text"]=text;return e;
}
void materialize(const Snapshot& s,const fs::path& path) {
    Json current=fs::exists(path)?read(path):s.catalog;jp_shader_catalog::parseCurated(current);
    for(const auto& entry:s.catalog.at("entries")) {
        Json* target=nullptr;for(auto& local:current["entries"])if(local.at("path")==entry.at("path")){target=&local;break;}
        if(!target){current["entries"].push_back(entry);target=&current["entries"].back();}
        for(const auto& field:sharedFields())setValue(*target,field,initialValue(entry,field));
    }
    const auto bytes=current.dump(2)+"\n";if(!fs::exists(path) || jp::readBytes(path)!=bytes)jp::atomicWrite(path,bytes);
}
}
