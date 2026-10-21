#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
struct sqlite3;
namespace vl {
using Json = nlohmann::json;
inline std::filesystem::path pathFromUtf8(const std::string& s){return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()),s.size()));}
struct Vec3 {
    float x=0,y=0,z=0;
    Vec3 operator+(Vec3 b) const {return {x+b.x,y+b.y,z+b.z};}
    Vec3 operator-(Vec3 b) const {return {x-b.x,y-b.y,z-b.z};}
    Vec3 operator*(float k) const {return {x*k,y*k,z*k};}
    float length() const {return std::sqrt(x*x+y*y+z*z);}
};
inline float dot(Vec3 a,Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Vec3 normalized(Vec3 a) {float n=a.length();return n>0.00001f?a*(1/n):Vec3{0,1,0};}
struct Cell {
    int x=0,y=0,z=0;
    auto operator<=>(const Cell&) const = default;
};
struct CellHash {size_t operator()(Cell p) const noexcept;};
inline Cell cellAt(Vec3 p) {return {int(std::floor(p.x)),int(std::floor(p.y)),int(std::floor(p.z))};}
inline Vec3 center(Cell c) {return {c.x+0.5f,c.y+0.5f,c.z+0.5f};}
inline int floorDiv(int x,int n) {return x/n-((x%n)<0?1:0);}
struct ItemDef {
    std::string id,name,kind,drop;
    std::array<unsigned char,3> color{180,180,180};
    bool solid=true,placeable=false;
    float hardness=1,damage=1,speed=1;
    int maxStack=64,durability=0,food=0;
};
struct Ingredient {std::string id;int count=1;};
struct Recipe {std::string id,name,station;std::vector<Ingredient> inputs;Ingredient output;};
class Registry {
public:
    void load(const std::filesystem::path& file);
    const ItemDef& item(const std::string& id) const;
    bool contains(const std::string& id) const {return items.contains(id);}
    std::map<std::string,ItemDef> items;
    std::vector<Recipe> recipes;
};
struct Stack {
    std::string id;
    int count=0,wear=0;
    bool empty() const {return count<=0||id.empty();}
};
class Inventory {
public:
    std::array<Stack,40> slots{};
    int count(const std::string& id) const;
    int add(const Registry&,std::string id,int count,int wear=0);
    bool remove(const std::string& id,int count);
    bool craft(const Registry&,const Recipe& recipe);
    Json json() const;
    void restore(const Json&);
};
struct Player {
    Vec3 position{4,25,4},velocity{};
    float yaw=0,pitch=0,health=20,hunger=20,stamina=100,oxygen=20;
    bool creative=true,flying=false;
    int selected=0,xp=0;
    std::set<std::string> skills;
    float defenseWindow=0;
    Inventory inventory;
    Json json() const;
    void restore(const Json&);
};
struct Entity {
    int id=0;
    std::string type="pig";
    Vec3 position{};
    float health=10,phase=0,cooldown=0;
};
struct Station {
    std::string type;
    Inventory inventory;
    float progress=0,energy=0;
};
struct Crop {float age=0;};
struct RayHit {bool hit=false,terrain=false;Vec3 point{},normal{};Cell cell{};std::string id;float distance=0;};
struct Noise;
class World {
public:
    explicit World(int seed=12345);
    World(const World&);
    World& operator=(const World&);
    ~World();
    int seed() const {return seed_;}
    float height(float x,float z) const;
    float baseDensity(Vec3 p) const;
    float density(Vec3 p) const;
    Vec3 normal(Vec3 p) const;
    std::string terrainMaterial(Vec3 p) const;
    std::string generatedBlock(Cell p) const;
    std::string block(Cell p) const;
    bool solid(Vec3 p,const Registry&) const;
    bool water(Vec3 p) const {return p.y<6.f&&p.y>height(p.x,p.z)&&density(p)>0;}
    void setBlock(Cell p,const std::string& id);
    void sculpt(Vec3 p,float radius,bool add);
    RayHit raycast(Vec3 origin,Vec3 dir,float reach,const Registry&) const;
    void open(const std::filesystem::path& path,Player&);
    void save(const Player&);
    void exportBackup(const std::filesystem::path& path);
    void tick(float dt,Player&,const Registry&);
    bool useStation(Cell,const Registry&,Player&,std::string& message);
    std::uint64_t revision=0;
    double time=0.28,elapsed=0;
    std::unordered_map<Cell,std::string,CellHash> blocks;
    std::unordered_map<Cell,float,CellHash> edits;
    std::map<Cell,Station> stations;
    std::map<Cell,Crop> crops;
    std::map<Cell,Vec3> portals;
    std::vector<Entity> entities;
    std::string savePath;
private:
    int seed_;
    std::shared_ptr<Noise> noise_;
    sqlite3* db_=nullptr;
    float tickAccumulator_=0;
    void loadState(Player&);
};
struct Vertex {Vec3 position,normal;std::array<unsigned char,4> color;};
struct Geometry {std::vector<Vertex> vertices;std::size_t regularTriangles=0,transitionTriangles=0;};
using Field=std::function<float(Vec3)>;
Geometry meshRegular(const Field&,Vec3 origin,Vec3 size,int step);
// Each 2x2 fine face patch is stitched to a 1x1 coarse face with the
// canonical 512-case transition tables. Face axes are arbitrary in 3D.
Geometry meshTransition(const Field&,Vec3 origin,Vec3 u,Vec3 v,int uCells,int vCells,int step);
Geometry meshColumn(const World&,int cx,int cz,int step,const std::array<int,4>& neighbors,const Registry&);
struct Event {std::string type;Json payload;};
class EventBus {
public:
    using Listener=std::function<void(const Event&)>;
    void on(std::string type,Listener callback) {listeners_[type].push_back(std::move(callback));}
    void emit(const Event& event) const {auto it=listeners_.find(event.type);if(it!=listeners_.end())for(const auto& f:it->second)f(event);}
private:std::map<std::string,std::vector<Listener>> listeners_;
};
class Game {
public:
    Registry registry;
    World world;
    Player player;
    EventBus events;
    std::string message="Welcome to Voxel Lab";
    int buildShape=0,buildSize=3;
    void loadContent(const std::filesystem::path& root);
    void start(const std::filesystem::path&,int seed,bool creative);
    void tick(float dt);
    bool mine(const RayHit& hit,bool brush=false);
    bool place(const RayHit& hit,bool brush=false);
    bool craft(std::size_t recipeIndex);
    void command(const std::string& text);
    void respawn();
    bool build(const RayHit& hit);
    void dungeon();
private:float survivalClock_=0;
};
}
