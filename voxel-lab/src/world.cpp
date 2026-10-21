#include "world.hpp"
#include "FastNoiseLite.h"
#include <sqlite3.h>
#include <fstream>
#include <stdexcept>
#include <sstream>
namespace vl {
struct Noise {FastNoiseLite terrain,detail,caves; explicit Noise(int seed):terrain(seed),detail(seed+73),caves(seed+117) {
    terrain.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);terrain.SetFrequency(0.008f);
    terrain.SetFractalType(FastNoiseLite::FractalType_FBm);terrain.SetFractalOctaves(4);
    detail.SetFrequency(0.05f);caves.SetFrequency(0.042f);
}};
static std::uint32_t hashXZ(int x,int z,int seed) {
    auto h=std::uint32_t(x)*0x8da6b343u ^ std::uint32_t(z)*0xd8163841u ^ std::uint32_t(seed);
    h^=h>>13;h*=0x85ebca6bu;h^=h>>16;return h;
}
size_t CellHash::operator()(Cell p) const noexcept {return size_t(hashXZ(p.x,p.z,p.y));}
static void sql(sqlite3* db,const char* text) {
    char* error=nullptr;
    if(sqlite3_exec(db,text,nullptr,nullptr,&error)!=SQLITE_OK) {std::string msg=error?error:"SQLite failure";sqlite3_free(error);throw std::runtime_error(msg);}
}
struct Statement {
    sqlite3_stmt* stmt=nullptr;
    explicit Statement(sqlite3* db,const char* text) {if(sqlite3_prepare_v2(db,text,-1,&stmt,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));}
    ~Statement(){sqlite3_finalize(stmt);}
    void bind(int index,const std::string& value){if(sqlite3_bind_text(stmt,index,value.c_str(),-1,SQLITE_TRANSIENT)!=SQLITE_OK)throw std::runtime_error("SQLite text binding failed");}
    void number(int i,int n){sqlite3_bind_int(stmt,i,n);}
    void real(int i,double n){sqlite3_bind_double(stmt,i,n);}
    void write(){if(sqlite3_step(stmt)!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(stmt)));sqlite3_reset(stmt);sqlite3_clear_bindings(stmt);}
};
static std::string text(sqlite3_stmt* s,int col) {const auto p=sqlite3_column_text(s,col);return p?reinterpret_cast<const char*>(p):"";}
static std::string utf8Path(const std::filesystem::path& p){auto s=p.u8string();return {reinterpret_cast<const char*>(s.data()),s.size()};}
void Registry::load(const std::filesystem::path& file) {
    std::ifstream in(file);if(!in)throw std::runtime_error("Cannot read content: "+file.string());Json j;in>>j;
    for(const auto& x:j.value("items",Json::array())) {
        ItemDef d;d.id=x.at("id");d.name=x.value("name",d.id);d.kind=x.value("kind",std::string("block"));
        if(d.id.find(':')==std::string::npos)throw std::runtime_error("Content ID needs a namespace: "+d.id);
        if(items.contains(d.id))throw std::runtime_error("Duplicate content ID: "+d.id);
        if(x.contains("color"))d.color=x["color"].get<std::array<unsigned char,3>>();
        d.solid=x.value("solid",true);d.placeable=x.value("placeable",d.kind=="block");d.hardness=x.value("hardness",1.f);
        d.maxStack=x.value("stack",64);d.durability=x.value("durability",0);d.damage=x.value("damage",1.f);d.speed=x.value("speed",1.f);
        d.food=x.value("food",0);d.drop=x.value("drop",d.id);
        if(d.maxStack<1||d.maxStack>9999||d.hardness<0||d.speed<=0)throw std::runtime_error("Invalid content values: "+d.id);
        items.emplace(d.id,std::move(d));
    }
    for(const auto& x:j.value("recipes",Json::array())) {
        Recipe r;r.id=x.at("id");r.name=x.at("name");r.station=x.value("station",std::string("hand"));
        r.output={x.at("output").at(0).get<std::string>(),x.at("output").at(1).get<int>()};
        for(const auto& a:x.at("inputs"))r.inputs.push_back({a.at(0).get<std::string>(),a.at(1).get<int>()});
        if(!contains(r.output.id)||r.output.count<1||r.inputs.empty())throw std::runtime_error("Invalid recipe output: "+r.id);
        for(const auto& a:r.inputs)if(!contains(a.id)||a.count<1)throw std::runtime_error("Invalid recipe input: "+r.id);
        recipes.push_back(std::move(r));
    }
}
const ItemDef& Registry::item(const std::string& id) const {auto i=items.find(id);if(i!=items.end())return i->second;static const ItemDef unknown{"core:unknown","Unknown block","block","core:stone"};return unknown;}
int Inventory::count(const std::string& id) const {int n=0;for(const auto& s:slots)if(s.id==id&&!s.empty())n+=s.count;return n;}
int Inventory::add(const Registry& r,std::string id,int n,int wear) {
    if(n<=0||!r.contains(id))return n;
    int cap=r.item(id).maxStack;
    for(auto& s:slots)if(!s.empty()&&s.id==id&&s.wear==wear){int d=std::min(n,cap-s.count);s.count+=d;n-=d;}
    for(auto& s:slots)if(s.empty()&&n>0){int d=std::min(n,cap);s={id,d,wear};n-=d;}
    return n;
}
bool Inventory::remove(const std::string& id,int n) {if(n<0||count(id)<n)return false;for(auto& s:slots)if(s.id==id){int d=std::min(n,s.count);n-=d;s.count-=d;if(s.count==0)s={};}return true;}
bool Inventory::craft(const Registry& r,const Recipe& recipe) {
    Inventory candidate=*this;
    for(const auto& a:recipe.inputs)if(!candidate.remove(a.id,a.count))return false;
    if(candidate.add(r,recipe.output.id,recipe.output.count)>0)return false;
    *this=std::move(candidate);return true;
}
Json Inventory::json() const {Json a=Json::array();for(const auto& s:slots)a.push_back({{"id",s.id},{"count",s.count},{"wear",s.wear}});return a;}
void Inventory::restore(const Json& a){slots={};if(!a.is_array())return;size_t i=0;for(const auto& s:a){if(i==slots.size())break;slots[i++]={s.value("id",std::string()),std::clamp(s.value("count",0),0,9999),std::max(0,s.value("wear",0))};}}
Json Player::json() const {return {{"position",{position.x,position.y,position.z}},{"yaw",yaw},{"pitch",pitch},{"health",health},{"hunger",hunger},{"stamina",stamina},{"oxygen",oxygen},{"creative",creative},{"flying",flying},{"selected",selected},{"xp",xp},{"skills",skills},{"inventory",inventory.json()}};}
void Player::restore(const Json& j) {
    if(j.contains("position")){auto p=j.at("position");position={p.at(0),p.at(1),p.at(2)};}
    yaw=j.value("yaw",0.f);pitch=j.value("pitch",0.f);health=std::clamp(j.value("health",20.f),0.f,20.f);hunger=std::clamp(j.value("hunger",20.f),0.f,20.f);
    stamina=std::clamp(j.value("stamina",100.f),0.f,100.f);oxygen=std::clamp(j.value("oxygen",20.f),0.f,20.f);
    creative=j.value("creative",true);flying=j.value("flying",false);selected=std::clamp(j.value("selected",0),0,9);xp=j.value("xp",0);
    inventory.restore(j.value("inventory",Json::array()));
    skills=j.value("skills",std::set<std::string>{});
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))position={4,30,4};
}
World::World(int seed):seed_(seed),noise_(std::make_shared<Noise>(seed)){}
World::~World(){if(db_)sqlite3_close(db_);}
World::World(const World& b):revision(b.revision),time(b.time),elapsed(b.elapsed),blocks(b.blocks),edits(b.edits),stations(b.stations),crops(b.crops),portals(b.portals),entities(b.entities),seed_(b.seed_),noise_(b.noise_){}
World& World::operator=(const World& b){if(this!=&b){if(db_)sqlite3_close(db_);db_=nullptr;seed_=b.seed_;noise_=b.noise_;revision=b.revision;time=b.time;elapsed=b.elapsed;blocks=b.blocks;edits=b.edits;stations=b.stations;crops=b.crops;portals=b.portals;entities=b.entities;savePath.clear();}return *this;}
float World::height(float x,float z) const {return 11.f+noise_->terrain.GetNoise(x,z)*25.f+noise_->detail.GetNoise(x,z)*1.8f;}
float World::baseDensity(Vec3 p) const {
    float h=height(p.x,p.z),d=p.y-h;
    if(p.y<h-4&&p.y>-26){float cave=std::abs(noise_->caves.GetNoise(p.x,p.y,p.z))-0.065f;d=std::max(d,-cave*22);}
    if(p.y<-29)d=std::min(d,p.y+29.f);
    return d;
}
float World::density(Vec3 p) const {
    Cell c=cellAt(p);float tx=p.x-c.x,ty=p.y-c.y,tz=p.z-c.z,delta=0;
    if(!edits.empty())for(int i=0;i<8;++i){Cell k{c.x+(i&1),c.y+((i>>1)&1),c.z+((i>>2)&1)};auto e=edits.find(k);if(e!=edits.end())delta+=e->second*((i&1)?tx:1-tx)*((i&2)?ty:1-ty)*((i&4)?tz:1-tz);}
    return baseDensity(p)+delta;
}
Vec3 World::normal(Vec3 p) const {float e=0.15f;return normalized({density(p+Vec3{e,0,0})-density(p-Vec3{e,0,0}),density(p+Vec3{0,e,0})-density(p-Vec3{0,e,0}),density(p+Vec3{0,0,e})-density(p-Vec3{0,0,e})});}
std::string World::terrainMaterial(Vec3 p) const {
    float h=height(p.x,p.z);float depth=h-p.y;
    if(depth<1.8f){if(h<6.8f)return "core:sand";if(h>23)return "core:snow";return "core:grass";}
    if(depth<4)return "core:dirt";
    auto hsh=hashXZ(int(std::floor(p.x/3)),int(std::floor(p.z/3)),seed_+int(std::floor(p.y/3))*7);
    if(depth>7&&hsh%43==0)return "core:coal_ore";
    if(depth>9&&hsh%61==0)return "core:iron_ore";
    if(p.y<-12&&hsh%97==0)return "core:diamond_ore";
    return "core:stone";
}
std::string World::generatedBlock(Cell p) const {
    if(p.y<5||p.y>45)return "core:air";
    int gx=floorDiv(p.x,12),gz=floorDiv(p.z,12);auto h=hashXZ(gx,gz,seed_);
    int tx=gx*12+3+int(h%6),tz=gz*12+3+int((h>>5)%6),ground=int(std::ceil(height(float(tx)+0.5f,float(tz)+0.5f)));
    if(ground<8||ground>23||h%5==0)return "core:air";
    int dx=std::abs(p.x-tx),dz=std::abs(p.z-tz),dy=p.y-ground;
    if(dx==0&&dz==0&&dy>=0&&dy<5)return "core:log";
    if(dx<=2&&dz<=2&&dy>=3&&dy<=6&&dx+dz+(dy==6?1:0)<5)return "core:leaves";
    return "core:air";
}
std::string World::block(Cell p) const {auto i=blocks.find(p);return i==blocks.end()?generatedBlock(p):i->second;}
bool World::solid(Vec3 p,const Registry& r) const {if(density(p)<0)return true;auto id=block(cellAt(p));return id!="core:air"&&r.item(id).solid;}
void World::setBlock(Cell p,const std::string& id){blocks[p]=id;++revision;}
void World::sculpt(Vec3 p,float radius,bool add){
    if(radius<0.25f||radius>8)return;
    for(int z=int(std::floor(p.z-radius));z<=int(std::ceil(p.z+radius));++z)
    for(int y=int(std::floor(p.y-radius));y<=int(std::ceil(p.y+radius));++y)
    for(int x=int(std::floor(p.x-radius));x<=int(std::ceil(p.x+radius));++x){Vec3 q{float(x),float(y),float(z)};float dist=(q-p).length();if(dist<radius){float change=(radius-dist)*2.5f;edits[{x,y,z}]+=(add?-change:change);}}
    ++revision;
}
RayHit World::raycast(Vec3 o,Vec3 d,float reach,const Registry& r) const {
    d=normalized(d);float previous=density(o);
    for(float t=0.08f;t<=reach;t+=0.08f){Vec3 p=o+d*t;auto c=cellAt(p);auto id=block(c);
        if(id!="core:air"&&r.contains(id)){
            auto prev=cellAt(p-d*0.08f);Vec3 n{float(prev.x-c.x),float(prev.y-c.y),float(prev.z-c.z)};
            if(n.length()<0.5f)n=d*-1;return {true,false,p,normalized(n),c,id,t};
        }
        float current=density(p);
        if(current<0&&previous>=0){float lo=t-0.08f,hi=t;for(int k=0;k<8;++k){float mid=(lo+hi)/2;if(density(o+d*mid)<0)hi=mid;else lo=mid;}p=o+d*((lo+hi)/2);return {true,true,p,normal(p),cellAt(p),terrainMaterial(p),t};}
        previous=current;
    }return {};
}
void World::open(const std::filesystem::path& path,Player& player) {
    if(db_){sqlite3_close(db_);db_=nullptr;}
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    blocks.clear();edits.clear();stations.clear();crops.clear();portals.clear();entities.clear();time=.28;elapsed=0;
    if(sqlite3_open(utf8Path(path).c_str(),&db_)!=SQLITE_OK)throw std::runtime_error("Cannot open world database");
    sqlite3_busy_timeout(db_,3000);
    try {
        bool existing=false;
        {Statement s(db_,"SELECT count(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'");
        sqlite3_step(s.stmt);existing=sqlite3_column_int(s.stmt,0)>0;}
        if(existing){Statement g(db_,"SELECT value FROM metadata WHERE key='generator'");if(sqlite3_step(g.stmt)!=SQLITE_ROW||text(g.stmt,0)!="voxel-lab-native-v1")throw std::runtime_error("This world uses another generator/format. Open a new native world; original saves are preserved.");}
        sql(db_,"PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; CREATE TABLE IF NOT EXISTS metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL); CREATE TABLE IF NOT EXISTS blocks(x INTEGER,y INTEGER,z INTEGER,id TEXT NOT NULL,PRIMARY KEY(x,y,z)); CREATE TABLE IF NOT EXISTS density(x INTEGER,y INTEGER,z INTEGER,value REAL NOT NULL,PRIMARY KEY(x,y,z)); CREATE TABLE IF NOT EXISTS players(identity TEXT PRIMARY KEY,profile TEXT NOT NULL); CREATE TABLE IF NOT EXISTS runtime(key TEXT PRIMARY KEY,value TEXT NOT NULL); PRAGMA user_version=2;");
        savePath=utf8Path(path);if(existing)loadState(player);else save(player);
    }catch(...){sqlite3_close(db_);db_=nullptr;throw;}
}
void World::loadState(Player& p) {
    {Statement s(db_,"SELECT key,value FROM metadata");while(sqlite3_step(s.stmt)==SQLITE_ROW){auto k=text(s.stmt,0),v=text(s.stmt,1);if(k=="seed")seed_=std::stoi(v);if(k=="time")time=std::stod(v);if(k=="elapsed")elapsed=std::stod(v);}noise_=std::make_shared<Noise>(seed_);}
    {Statement s(db_,"SELECT x,y,z,id FROM blocks");while(sqlite3_step(s.stmt)==SQLITE_ROW)blocks[{sqlite3_column_int(s.stmt,0),sqlite3_column_int(s.stmt,1),sqlite3_column_int(s.stmt,2)}]=text(s.stmt,3);}
    {Statement s(db_,"SELECT x,y,z,value FROM density");while(sqlite3_step(s.stmt)==SQLITE_ROW)edits[{sqlite3_column_int(s.stmt,0),sqlite3_column_int(s.stmt,1),sqlite3_column_int(s.stmt,2)}]=float(sqlite3_column_double(s.stmt,3));}
    {Statement s(db_,"SELECT profile FROM players WHERE identity='local'");if(sqlite3_step(s.stmt)==SQLITE_ROW)p.restore(Json::parse(text(s.stmt,0)));}
    {Statement s(db_,"SELECT key,value FROM runtime");while(sqlite3_step(s.stmt)==SQLITE_ROW){auto k=text(s.stmt,0);auto j=Json::parse(text(s.stmt,1));
        if(k=="entities")for(const auto& a:j){Entity e;e.id=a.at("id");e.type=a.at("type");e.position={a["p"][0],a["p"][1],a["p"][2]};e.health=a.at("health");entities.push_back(e);}
        if(k=="stations")for(const auto& a:j){Cell c{a["p"][0],a["p"][1],a["p"][2]};Station st;st.type=a.at("type");st.inventory.restore(a.at("inventory"));st.progress=a.value("progress",0.f);st.energy=a.value("energy",0.f);stations[c]=st;}
        if(k=="crops")for(const auto& a:j)crops[{a["p"][0],a["p"][1],a["p"][2]}]={a.at("age")};
        if(k=="portals")for(const auto& a:j)portals[{a["p"][0],a["p"][1],a["p"][2]}]={a["target"][0],a["target"][1],a["target"][2]};
    }}++revision;
}
void World::save(const Player& p) {
    if(!db_)return;
    sql(db_,"BEGIN IMMEDIATE");
    try {
        Statement meta(db_,"INSERT OR REPLACE INTO metadata(key,value) VALUES(?,?)");
        for(const auto& [k,v]:std::map<std::string,std::string>{{"generator","voxel-lab-native-v1"},{"seed",std::to_string(seed_)},{"time",std::to_string(time)},{"elapsed",std::to_string(elapsed)}}){meta.bind(1,k);meta.bind(2,v);meta.write();}
        sql(db_,"DELETE FROM blocks; DELETE FROM density; DELETE FROM runtime;");
        Statement b(db_,"INSERT INTO blocks VALUES(?,?,?,?)");for(const auto& [c,id]:blocks){b.number(1,c.x);b.number(2,c.y);b.number(3,c.z);b.bind(4,id);b.write();}
        Statement d(db_,"INSERT INTO density VALUES(?,?,?,?)");for(const auto& [c,v]:edits){d.number(1,c.x);d.number(2,c.y);d.number(3,c.z);d.real(4,v);d.write();}
        Statement player(db_,"INSERT OR REPLACE INTO players VALUES('local',?)");player.bind(1,p.json().dump());player.write();
        Json es=Json::array(),ss=Json::array(),cs=Json::array(),ps=Json::array();
        for(const auto& e:entities)es.push_back({{"id",e.id},{"type",e.type},{"p",{e.position.x,e.position.y,e.position.z}},{"health",e.health}});
        for(const auto& [c,s]:stations)ss.push_back({{"p",{c.x,c.y,c.z}},{"type",s.type},{"inventory",s.inventory.json()},{"progress",s.progress},{"energy",s.energy}});
        for(const auto& [c,s]:crops)cs.push_back({{"p",{c.x,c.y,c.z}},{"age",s.age}});
        for(const auto& [c,target]:portals)ps.push_back({{"p",{c.x,c.y,c.z}},{"target",{target.x,target.y,target.z}}});
        Statement rt(db_,"INSERT INTO runtime VALUES(?,?)");for(const auto& [k,v]:std::map<std::string,Json>{{"entities",es},{"stations",ss},{"crops",cs},{"portals",ps}}){rt.bind(1,k);rt.bind(2,v.dump());rt.write();}
        sql(db_,"COMMIT");
    }catch(...){sql(db_,"ROLLBACK");throw;}
}
void World::exportBackup(const std::filesystem::path& path) {
    if(!db_)throw std::runtime_error("No world is open");
    std::error_code comparisonError;
    bool same=std::filesystem::equivalent(path,pathFromUtf8(savePath),comparisonError);
    if(same||std::filesystem::absolute(path).lexically_normal()==std::filesystem::absolute(pathFromUtf8(savePath)).lexically_normal())throw std::runtime_error("Backup must use a different file");
    sqlite3* dest=nullptr;if(sqlite3_open(utf8Path(path).c_str(),&dest)!=SQLITE_OK){if(dest)sqlite3_close(dest);throw std::runtime_error("Cannot open backup");}
    auto* b=sqlite3_backup_init(dest,"main",db_,"main");if(!b){sqlite3_close(dest);throw std::runtime_error("Cannot begin backup");}
    int code=sqlite3_backup_step(b,-1);int end=sqlite3_backup_finish(b);sqlite3_close(dest);if(code!=SQLITE_DONE||end!=SQLITE_OK)throw std::runtime_error("Backup incomplete");
}
}
