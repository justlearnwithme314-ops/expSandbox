#include "world.hpp"
#include <sqlite3.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <tuple>
using namespace vl;
namespace {
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
bool close(float a,float b,float e=.001f){return std::abs(a-b)<e;}
void finite(const Geometry& g){for(const auto& v:g.vertices)check(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z)&&std::isfinite(v.normal.x)&&std::isfinite(v.normal.y)&&std::isfinite(v.normal.z),"nonfinite mesh vertex");check(g.vertices.size()%3==0,"incomplete triangles");}
using Point=std::array<int,3>;
Point quant(Vec3 p){return {int(std::round(p.x*10000)),int(std::round(p.y*10000)),int(std::round(p.z*10000))};}
void seams(){
    // Count unmatched boundary edges from a fine mesh, a coarse mesh and the
    // transition patch. This catches table ordering and missing seam geometry.
    for(int axis=0;axis<3;++axis)for(int sign:{-1,1}){
        auto rotate=[=](Vec3 p){if(axis==0)return Vec3{p.x*sign,p.y,p.z};if(axis==1)return Vec3{p.y,p.x*sign,p.z};return Vec3{p.z,p.y,p.x*sign};};
        auto unrotate=[=](Vec3 p){if(axis==0)return Vec3{p.x*sign,p.y,p.z};if(axis==1)return Vec3{p.y*sign,p.x,p.z};return Vec3{p.z*sign,p.y,p.x};};
        Field base=[](Vec3 p){return p.y-7.231f+std::sin(p.z*.67f)*1.8f+std::cos(p.x*.19f)*.3f;};
        auto left=meshRegular(base,{-16,0,0},{16,16,16},1),right=meshRegular(base,{0,0,0},{16,16,16},2);
        Field f=[&](Vec3 p){return base(unrotate(p));};
        auto seam=meshTransition(f,rotate({0,0,0}),rotate({0,0,1}),rotate({0,1,0}),16,16,1);
        check(seam.transitionTriangles>0,"missing transition mesh");finite(seam);
        std::map<std::pair<Point,Point>,int> edges;
        auto collect=[&](const Geometry& g,bool transform){for(size_t i=0;i<g.vertices.size();i+=3)for(int j=0;j<3;++j){Vec3 a=g.vertices[i+j].position,b=g.vertices[i+(j+1)%3].position;if(transform){a=rotate(a);b=rotate(b);}auto aa=unrotate(a),bb=unrotate(b);if(std::abs(aa.x)>.0001f||std::abs(bb.x)>.0001f)continue;auto p=quant(a),q=quant(b);if(q<p)std::swap(p,q);if(p!=q)++edges[{p,q}];}};
        collect(left,true);collect(right,true);collect(seam,false);
        // A straight contour may span two fine edges but one coarse edge.
        // Split collinear segments before comparing coverage, so this test
        // detects holes rather than rejecting equivalent triangulations.
        std::set<Point> points;for(const auto& [edge,n]:edges){points.insert(edge.first);points.insert(edge.second);}
        std::map<std::pair<Point,Point>,int> atomic;
        for(const auto& [edge,n]:edges){std::vector<std::pair<double,Point>> cuts{{0,edge.first},{1,edge.second}};double length2=0;for(int k=0;k<3;++k)length2+=double(edge.second[k]-edge.first[k])*(edge.second[k]-edge.first[k]);
            for(const auto& p:points){double t=0;for(int k=0;k<3;++k)t+=double(p[k]-edge.first[k])*(edge.second[k]-edge.first[k]);t/=length2;if(t<=0.000001||t>=.999999)continue;double error=0;for(int k=0;k<3;++k){double delta=p[k]-edge.first[k]-t*(edge.second[k]-edge.first[k]);error+=delta*delta;}if(error<4)cuts.push_back({t,p});}
            std::sort(cuts.begin(),cuts.end());for(size_t i=1;i<cuts.size();++i){auto a=cuts[i-1].second,b=cuts[i].second;if(b<a)std::swap(a,b);atomic[{a,b}]+=n;}}
        for(const auto& [edge,n]:atomic)check(n%2==0,"fine/coarse Transvoxel boundary has unmatched coverage");
    }
}
void tables(){
    for(int code=0;code<256;++code){Field f=[=](Vec3 p){int x=p.x>=.5f,y=p.y>=.5f,z=p.z>=.5f;return (code&(1<<(x+2*y+4*z)))?-1.f:1.f;};auto g=meshRegular(f,{0,0,0},{1,1,1},1);finite(g);if(code==0||code==255)check(g.vertices.empty(),"uniform cell must be empty");}
    constexpr int order[9]={0,1,2,5,8,7,6,3,4};
    for(int code=0;code<512;++code){Field f=[=](Vec3 p){int x=std::clamp(int(std::round(p.x)),0,2),z=std::clamp(int(std::round(p.z)),0,2),bit=0;for(int i=0;i<9;++i)if(order[i]==x+3*z)bit=i;return (code&(1<<bit))?-1.f:1.f;};finite(meshTransition(f,{0,0,0},{1,0,0},{0,0,1},2,2,1));}
}
void run(){
    Registry r;r.load(std::filesystem::path(VOXEL_SOURCE_DIR)/"content/base.json");check(r.items.size()>=50,"content not loaded");
    check(floorDiv(-1,16)==-1&&floorDiv(-16,16)==-1&&floorDiv(-17,16)==-2,"negative chunk coordinates");
    Inventory inv;inv.add(r,"core:log",1);check(inv.craft(r,r.recipes[0]),"planks craft");check(inv.count("core:log")==0&&inv.count("building:planks")==4,"craft conservation");
    auto before=inv.json();check(!inv.craft(r,r.recipes[0]),"missing ingredients should refuse");check(inv.json()==before,"failed recipe changed inventory");
    Inventory full;for(auto& s:full.slots)s={"core:stone",64,0};full.slots[0]={"core:log",2,0};auto original=full.json();check(!full.craft(r,r.recipes[0]),"full inventory crafted");check(full.json()==original,"full-inventory craft consumed inputs");
    Inventory roundtrip;roundtrip.restore(inv.json());check(roundtrip.json()==inv.json(),"inventory JSON roundtrip");
    Recipe duplicate{"test:duplicate","Duplicate input","hand",{{"core:log",2},{"core:log",2}},{"building:planks",8}};Inventory small;small.add(r,"core:log",3);auto old=small.json();check(!small.craft(r,duplicate)&&small.json()==old,"duplicate inputs bypassed recipe cost");
    World a(733),b(733),other(734);check(close(a.height(-32,47),b.height(-32,47)),"world generation nondeterministic");check(!close(a.height(-32,47),other.height(-32,47)),"seed ignored");
    Vec3 p{2,a.height(2,3),3};float d=a.density(p);a.sculpt(p,2,false);check(a.density(p)>d,"digging did not remove density");World snapshot=a;a.sculpt(p,2,true);check(!close(snapshot.density(p),a.density(p)),"snapshot changed with live world");
    a.setBlock({2,40,3},"core:stone");auto hit=a.raycast({2.5f,42,3.5f},{0,-1,0},5,r);check(hit.hit&&!hit.terrain&&hit.id=="core:stone","block raycast");
    check(a.solid({2.5f,40.5f,3.5f},r),"placed block collision missing");
    auto path=std::filesystem::temp_directory_path()/("voxel-lab-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(path);
    Player player;player.inventory.add(r,"frontier:iron_pickaxe",1,17);player.position={8,30,7};
    {
        World w(733);w.open(path/"test.sqlite",player);w.sculpt(p,1.5f,false);w.setBlock({-1,35,4},"survival:chest");w.stations[{-1,35,4}]={"survival:chest",{},2,9};w.stations[{-1,35,4}].inventory.add(r,"core:log",7);w.crops[{3,20,3}]={25};w.entities.push_back({7,"sheep",{1,20,3},9,0,0});w.time=.77;w.elapsed=123;w.save(player);w.exportBackup(path/"backup.sqlite");
    }
    {
        Player loaded;World w;w.open(path/"test.sqlite",loaded);check(w.seed()==733,"seed reload");check(w.blocks.at({-1,35,4})=="survival:chest","block reload");check(!w.edits.empty(),"density reload");check(w.stations.at({-1,35,4}).inventory.count("core:log")==7,"station reload");check(w.crops.at({3,20,3}).age==25,"crop reload");check(w.entities.size()==1&&w.entities[0].type=="sheep","entity reload");check(close(loaded.position.x,8)&&loaded.inventory.slots[0].wear==17,"player reload");check(std::abs(w.time-.77)<.001,"time reload");
        Player copied;World backup;backup.open(path/"backup.sqlite",copied);check(backup.blocks==w.blocks,"backup mismatch");
        bool rejected=false;try{w.exportBackup(path/"test.sqlite");}catch(...){rejected=true;}check(rejected,"backup allowed same file");
        w.setBlock({0,30,0},"crafting:furnace");auto& furnace=w.stations[{0,30,0}];furnace.type="crafting:furnace";furnace.inventory.add(r,"core:iron_ore",1);furnace.inventory.add(r,"survival:coal",1);for(int i=0;i<10;++i)w.tick(.5f,loaded,r);check(furnace.inventory.count("frontier:iron_ingot")==1&&furnace.inventory.count("core:iron_ore")==0,"furnace did not process atomically");
        w.crops[{3,20,3}]={0};w.setBlock({3,20,3},"survival:crop");for(int i=0;i<125;++i)w.tick(.5f,loaded,r);check(w.crops.at({3,20,3}).age==60,"crop growth");
    }
    {sqlite3* db=nullptr;sqlite3_open((path/"foreign.sqlite").string().c_str(),&db);sqlite3_exec(db,"CREATE TABLE metadata(key TEXT,value TEXT); INSERT INTO metadata VALUES('generator','foreign')",nullptr,nullptr,nullptr);sqlite3_close(db);Player pp;World w;bool refused=false;try{w.open(path/"foreign.sqlite",pp);}catch(...){refused=true;}check(refused,"foreign save silently accepted");}
    Game game;game.loadContent(VOXEL_SOURCE_DIR);game.start(path/"game.sqlite",100,false);game.command("/give core:log 4");check(game.player.inventory.count("core:log")>=4,"give command");game.command("/gamemode creative");check(game.player.creative,"mode command");game.command("/tp 3 31 5");check(game.player.position.y==31,"teleport command");game.command("/summon sheep");check(game.world.entities.back().type=="sheep","summon command");
    game.player.creative=false;game.player.xp=19;game.command("/learn double_jump");check(!game.player.skills.contains("double_jump"),"skill unlocked without XP");game.player.xp=20;game.command("/learn double_jump");check(game.player.skills.contains("double_jump")&&game.player.xp==0,"skill unlock cost");
    game.player.inventory.slots[0]={"core:stone",1,0};game.player.selected=0;game.buildShape=3;game.buildSize=3;
    RayHit buildHit{true,true,{100,50,100},{0,1,0},{100,50,100},"core:stone",3};size_t beforeBlocks=game.world.blocks.size();
    check(!game.build(buildHit)&&game.world.blocks.size()==beforeBlocks,"brush partially built without materials");game.player.inventory.slots[0].count=32;check(game.build(buildHit)&&game.player.inventory.slots[0].count==5,"brush cost not atomic");
    game.dungeon();check(game.world.portals.size()==2,"dungeon missing return gates");check(!game.world.stations.empty(),"dungeon missing loot");
    auto gate=game.world.portals.begin();auto target=gate->second;game.world.useStation(gate->first,game.registry,game.player,game.message);check((game.player.position-target).length()<.01f,"gate travel");game.world.save(game.player);
    check(!game.world.water(target+Vec3{0,1.5f,0}),"sealed underground room incorrectly flooded");
    game.world.tick(.1f,game.player,game.registry);check(game.world.entities.back().position.y<game.world.height(game.world.entities.back().position.x,game.world.entities.back().position.z)-5,"dungeon creature teleported to the surface");
    for(auto& s:game.player.inventory.slots)s={"core:stone",64,0};game.player.inventory.slots[0]={"survival:wheat_seeds",63,0};
    Cell cropCell{200,50,200};game.world.setBlock(cropCell,"survival:crop");game.world.crops[cropCell]={60};auto cropInventory=game.player.inventory.json();
    check(!game.mine({true,false,center(cropCell),{0,1,0},cropCell,"survival:crop",3}),"harvest destroyed crop without output capacity");check(game.world.crops.contains(cropCell)&&game.player.inventory.json()==cropInventory,"failed harvest did not preserve crop and inventory");
    {World reloaded;Player pp;reloaded.open(path/"game.sqlite",pp);check(reloaded.portals.size()==2&&pp.skills.contains("double_jump"),"dungeon/skills did not persist");}
    {EventBus bus;int calls=0;bus.on("world_ready",[&](const Event&){++calls;});bus.emit({"world_ready",{}});bus.emit({"other",{}});check(calls==1,"event dispatch");}
    tables();seams();
    // Temporary test directory is intentionally retained for postmortem inspection.
    std::cout<<"PASS: "<<checks<<" checks; fixtures at "<<path<<"\n";
}
}
int main(){try{run();return 0;}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}}
