#include "../src/JPutils/jp_review.h"
#include <cassert>
#include <iostream>
using namespace jp_review;
Json catalog(){return {{"format",1},{"entries",Json::array({{{"path","shaders/generative/a.frag"},{"category","generative"},{"name",{{"en","A"},{"es","A"}}},{"description",{{"en","A"},{"es","A"}}},{"inputs",Json::array()},{"tags",Json::array()},{"user_visible",true},{"group_path","local.xml"}}})}};}
int main(){
 auto root=fs::temp_directory_path()/("guipper-review-"+uuid());fs::create_directories(root/"shared");
 Config a{root/"shared",root/"a",root/"a-catalog.json","Nico"},b{root/"shared",root/"b",root/"b-catalog.json","JPupper"};
 auto p=create(a.shared,catalog());open(a);open(b);auto s=reduce(p,{});const std::string shader="shaders/generative/a.frag";
 auto x=change(s,shader,"name.en","One","Nico","hash1"),y=change(s,shader,"name.en","Two","JPupper","hash2");
 auto conflict=reduce(p,{x,y});assert(conflict.fields[shader]["name.en"].conflict);assert(conflict.fields[shader]["name.en"].value=="A");
 auto r=change(conflict,shader,"name.en","Two","Nico","hash2");auto done=reduce(p,{r,y,x,x});assert(!done.fields[shader]["name.en"].conflict && done.fields[shader]["name.en"].value=="Two");
 auto r2=change(conflict,shader,"name.en","One","JPupper","hash2");auto again=reduce(p,{x,y,r,r2});assert(again.fields[shader]["name.en"].conflict && again.fields[shader]["name.en"].value=="A");
 auto same=change(s,shader,"name.en","One","JPupper","hash1");assert(!reduce(p,{x,same}).fields[shader]["name.en"].conflict);
 auto deferred=reduce(p,{r});assert(deferred.deferred==1 && deferred.fields[shader]["name.en"].value=="A");
 auto flag=change(s,shader,"ask_pupper",true,"Nico","hash1");auto merged=reduce(p,{x,flag});assert(merged.fields[shader]["ask_pupper"].value==true && merged.fields[shader]["name.en"].value=="One");
 auto note=comment(s,shader,"remove","Discuss before deleting","JPupper","hash2");enqueue(a,x);enqueue(b,y);enqueue(a,note);
 auto first=synchronize(a);assert(first.pending==0);auto second=synchronize(b);assert(second.fields[shader]["name.en"].conflict);assert(synchronize(a).events.size()==3);
 fs::rename(root/"shared",root/"offline");enqueue(a,flag);auto offline=synchronize(a);assert(offline.pending==1 && offline.fields[shader]["ask_pupper"].value==true);assert(!offline.folderAvailable);
 fs::rename(root/"offline",root/"shared");assert(synchronize(a).pending==0);assert(synchronize(b).fields[shader]["ask_pupper"].value==true);
 jp::atomicWrite(root/"shared/events/broken.json","{");assert(!synchronize(b).errors.empty());
 jp::atomicWrite(a.catalog,catalog().dump());materialize(second,a.catalog);auto local=Json::parse(jp::readBytes(a.catalog));assert(local["entries"][0]["group_path"]=="local.xml");assert(local["entries"][0]["name"]["en"]=="A");
 auto bad=x;bad["project"]="wrong";assert(reduce(p,{bad}).events.empty());
 auto duplicate=x;duplicate["value"]="different";assert(reduce(p,{x,duplicate}).events.empty());
 Config blocked=a;blocked.local=root/"blocker";jp::atomicWrite(blocked.local,"file");bool refused=false;try{enqueue(blocked,x);}catch(...){refused=true;}assert(refused);
 auto defaults=change(s,shader,"preview_defaults",Json{{"amount",.23},{"enabled",false}},"Nico","hash1");assert(reduce(p,{defaults}).fields[shader]["preview_defaults"].value["enabled"]==false);
 // Remote write failure keeps the durable outbox across a fresh client instance.
 auto commentA=comment(s,shader,"comment","Nico offline note","Nico","hash1");
 auto commentB=comment(s,shader,"keep","Pupper offline note","JPupper","hash2");
 fs::rename(root/"shared/events",root/"saved-events");jp::atomicWrite(root/"shared/events","blocked");
 enqueue(a,commentA);enqueue(b,commentB);assert(synchronize(a).pending==1 && synchronize(b).pending==1);
 Config restarted=a;assert(synchronize(restarted).pending==1);
 fs::remove(root/"shared/events");fs::rename(root/"saved-events",root/"shared/events");
 assert(synchronize(restarted).pending==0);assert(synchronize(b).pending==0);
 auto both=synchronize(restarted);assert(both.events.count(commentA.at("id")) && both.events.count(commentB.at("id")));
 // An incomplete delivery is retried after the same file becomes complete.
 auto late=comment(s,shader,"comment","Arrived later","JPupper","hash2");
 jp::atomicWrite(root/"shared/events/partial.json","{");synchronize(a);
 jp::atomicWrite(root/"shared/events/partial.json",late.dump());assert(synchronize(a).events.count(late.at("id")));
 bool nonempty=false;try{create(a.shared,catalog());}catch(...){nonempty=true;}assert(nonempty);
 fs::remove_all(root);std::cout<<"review tests passed\n";
}
