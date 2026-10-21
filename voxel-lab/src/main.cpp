#include "world.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <chrono>
#include <cctype>
#include <future>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>
using namespace vl;
namespace {
Vector3 rv(Vec3 p){return {p.x,p.y,p.z};}
Vec3 vv(Vector3 p){return {p.x,p.y,p.z};}
Color rgb(std::array<unsigned char,3> c){return {c[0],c[1],c[2],255};}
const Color ink{228,233,226,255},muted{147,163,160,255},panel{18,30,33,247},accent{222,181,105,255},teal{109,188,164,255};
void label(const std::string& s,int x,int y,int size=18,Color c=ink){DrawText(s.c_str(),x,y,size,c);}
bool button(Rectangle box,const std::string& s,bool active=false){
    bool over=CheckCollisionPointRec(GetMousePosition(),box);DrawRectangleRounded(box,.14f,5,active?Color{63,94,88,255}:over?Color{50,67,66,255}:Color{30,46,48,255});
    DrawRectangleRoundedLinesEx(box,.14f,5,1,active?accent:Color{59,77,77,255});
    label(s,int(box.x)+14,int(box.y)+(int(box.height)-18)/2,18,active?accent:ink);return over&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
void bar(int x,int y,int width,float value,float maximum,Color color,const std::string& caption){
    DrawRectangle(x,y,width,5,{45,58,59,255});DrawRectangle(x,y,int(width*std::clamp(value/maximum,0.f,1.f)),5,color);label(caption,x,y-20,14,muted);
}
struct ChunkGpu {
    std::vector<Model> models;
    int step=0;std::array<int,4> neighbors{};std::uint64_t revision=~std::uint64_t(0);
    std::size_t triangles=0,transitions=0;
};
void release(ChunkGpu& gpu){for(auto& m:gpu.models)UnloadModel(m);gpu.models.clear();}
void upload(ChunkGpu& gpu,const Geometry& geo,Shader shader){
    release(gpu);gpu.triangles=geo.vertices.size()/3;gpu.transitions=geo.transitionTriangles;
    constexpr size_t limit=60000; // Stay below 16-bit mesh index limits on all backends.
    for(size_t offset=0;offset<geo.vertices.size();offset+=limit){
        size_t n=std::min(limit,geo.vertices.size()-offset);Mesh mesh{};mesh.vertexCount=int(n);mesh.triangleCount=int(n/3);
        mesh.vertices=static_cast<float*>(MemAlloc(unsigned(n*3*sizeof(float))));mesh.normals=static_cast<float*>(MemAlloc(unsigned(n*3*sizeof(float))));mesh.colors=static_cast<unsigned char*>(MemAlloc(unsigned(n*4)));
        mesh.texcoords=static_cast<float*>(MemAlloc(unsigned(n*2*sizeof(float))));
        for(size_t j=0;j<n;++j){const auto& v=geo.vertices[offset+j];mesh.vertices[j*3]=v.position.x;mesh.vertices[j*3+1]=v.position.y;mesh.vertices[j*3+2]=v.position.z;
            mesh.normals[j*3]=v.normal.x;mesh.normals[j*3+1]=v.normal.y;mesh.normals[j*3+2]=v.normal.z;mesh.texcoords[j*2]=v.position.x;mesh.texcoords[j*2+1]=v.position.z;
            for(int k=0;k<4;++k)mesh.colors[j*4+k]=v.color[k];}
        UploadMesh(&mesh,false);auto model=LoadModelFromMesh(mesh);model.materials[0].shader=shader;gpu.models.push_back(model);
    }
}
struct MeshResult {int x=0,z=0,step=1;std::array<int,4> neighbors{};std::uint64_t revision=0;Geometry geo;};
class Client {
public:
    Game game;
    std::filesystem::path root,saveDir;
    bool playing=false,exit=false,grounded=false,debug=false,wire=false,brush=false,mutedAudio=false,doubleJumped=false;
    int radius=5,panelPage=0,recipeScroll=0,catalogScroll=0;
    std::string screen="menu",worldName="Meadow",seedText="12345",console;
    bool newCreative=true;int editing=0,selectedSlot=-1;
    float actionCooldown=0,mineProgress=0,autosave=0,messageAge=0;
    Camera3D camera{};
    std::map<std::pair<int,int>,ChunkGpu> chunks;
    std::future<MeshResult> pending;
    Sound impact{};bool hasSound=false;
    Model camp{},barrel{};bool hasCamp=false,hasBarrel=false;
    Shader terrainShader{};
    Vec3 campPos{};
    explicit Client(std::filesystem::path path,std::filesystem::path saves):root(std::move(path)),saveDir(std::move(saves)){
        game.loadContent(root);camera.up={0,1,0};camera.fovy=75;camera.projection=CAMERA_PERSPECTIVE;
    }
    ~Client(){if(pending.valid())pending.wait();for(auto& [key,c]:chunks)release(c);if(hasCamp)UnloadModel(camp);if(hasBarrel)UnloadModel(barrel);if(hasSound)UnloadSound(impact);if(terrainShader.id)UnloadShader(terrainShader);}
    void assets(){
        terrainShader=LoadShader((root/"content/shaders/terrain.vs").string().c_str(),(root/"content/shaders/terrain.fs").string().c_str());
        auto modelPath=root/"assets/kenney_survival-kit/Models/GLB format";
        auto cp=modelPath/"campfire-pit.glb",bp=modelPath/"barrel.glb";
        if(std::filesystem::exists(cp)){camp=LoadModel(cp.string().c_str());hasCamp=camp.meshCount>0;}
        if(std::filesystem::exists(bp)){barrel=LoadModel(bp.string().c_str());hasBarrel=barrel.meshCount>0;}
        if(IsAudioDeviceReady())for(const auto& e:std::filesystem::recursive_directory_iterator(root/"assets/kenney_impact-sounds/Audio")){
            if(e.path().extension()==".ogg"&&e.path().filename().string().find("wood")!=std::string::npos){impact=LoadSound(e.path().string().c_str());hasSound=IsSoundValid(impact);break;}
        }
    }
    void start(){
        if(pending.valid())pending.wait();if(pending.valid())pending.get();
        for(auto& [k,c]:chunks)release(c);chunks.clear();
        int seed=12345;try{seed=std::stoi(seedText);}catch(...){}
        game.start(saveDir/(worldName+".sqlite"),seed,newCreative);playing=true;screen="play";DisableCursor();
        campPos={9,game.world.height(9,7),7};autosave=0;grounded=false;game.player.velocity={};
    }
    Vec3 direction()const {float y=game.player.yaw,p=game.player.pitch;return {std::sin(y)*std::cos(p),std::sin(p),-std::cos(y)*std::cos(p)};}
    void syncCamera(){auto p=game.player.position;camera.position=rv(p+Vec3{0,1.62f,0});camera.target=rv(p+Vec3{0,1.62f,0}+direction());}
    void changeScreen(const std::string& s){screen=s;mineProgress=0;selectedSlot=-1;if(s=="play")DisableCursor();else EnableCursor();}
    bool collides(Vec3 feet)const {
        for(float y:{0.15f,.9f,1.68f})for(float x:{-.26f,.26f})for(float z:{-.26f,.26f})if(game.world.solid(feet+Vec3{x,y,z},game.registry))return true;
        return false;
    }
    void movement(float dt){
        auto& p=game.player;auto mouse=GetMouseDelta();p.yaw+=mouse.x*.0022f;p.pitch=std::clamp(p.pitch-mouse.y*.0022f,-1.52f,1.52f);
        if(IsKeyPressed(KEY_F)&&p.creative){p.flying=!p.flying;p.velocity={};}
        Vec3 f{std::sin(p.yaw),0,-std::cos(p.yaw)},right{std::cos(p.yaw),0,std::sin(p.yaw)},move;
        if(IsKeyDown(KEY_W))move=move+f;if(IsKeyDown(KEY_S))move=move-f;if(IsKeyDown(KEY_D))move=move+right;if(IsKeyDown(KEY_A))move=move-right;
        if(move.length()>0)move=normalized(move);
        bool sprint=IsKeyDown(KEY_LEFT_SHIFT)&&p.stamina>0;float speed=p.flying?(sprint?23.f:12.f):(sprint?7.8f:4.5f);
        if(!p.creative){p.stamina=std::clamp(p.stamina+(sprint&&move.length()>0?-19.f:12.f)*dt,0.f,100.f);if(p.hunger<4)speed*=.6f;}
        if(p.flying){if(IsKeyDown(KEY_SPACE))move.y=1;if(IsKeyDown(KEY_LEFT_CONTROL))move.y=-1;p.position=p.position+move*(dt*speed);return;}
        bool swimming=game.world.water(p.position+Vec3{0,1,0});
        if(swimming){speed*=.65f;if(IsKeyDown(KEY_SPACE))p.velocity.y=4;}
        if(IsKeyPressed(KEY_SPACE)&&grounded){p.velocity.y=8;grounded=false;}
        else if(IsKeyPressed(KEY_SPACE)&&!doubleJumped&&p.skills.contains("double_jump")&&p.stamina>=12){p.velocity.y=8;doubleJumped=true;p.stamina-=12;}
        if(IsKeyPressed(KEY_K)&&p.skills.contains("dash")&&p.stamina>=20){for(int i=0;i<10;++i){auto target=p.position+move*.2f;if(collides(target))break;p.position=target;}p.stamina-=20;p.defenseWindow=.15f;}
        if(IsKeyPressed(KEY_H)&&p.skills.contains("parry")&&p.stamina>=15){p.defenseWindow=.6f;p.stamina-=15;}
        if(IsKeyPressed(KEY_J)&&!grounded&&p.skills.contains("slam")&&p.stamina>=18){p.velocity.y=-20;p.stamina-=18;for(auto& e:game.world.entities)if((e.position-p.position).length()<4)e.health-=7;}
        int substeps=4;float tick=dt/substeps;
        for(int i=0;i<substeps;++i){
            auto axisMove=[&](Vec3 delta){Vec3 dest=p.position+delta;if(!collides(dest)){p.position=dest;return;}if(grounded&&!collides(dest+Vec3{0,.6f,0}))p.position=dest+Vec3{0,.6f,0};};
            axisMove({move.x*speed*tick,0,0});axisMove({0,0,move.z*speed*tick});
            p.velocity.y-=(swimming?6.f:24.f)*tick;float fall=p.velocity.y;Vec3 dest=p.position+Vec3{0,p.velocity.y*tick,0};
            if(collides(dest)){if(p.velocity.y<0){if(!grounded&&fall<-13&&!p.creative&&!swimming)p.health-=(-fall-13)*.65f;grounded=true;doubleJumped=false;}
                p.velocity.y=0;
            }else{p.position=dest;grounded=false;}
        }
        if(p.position.y<-35)game.respawn();
    }
    int lod(int x,int z,int cx,int cz)const {int d=std::max(std::abs(x-cx),std::abs(z-cz));return d<=2?1:d<=4?2:4;}
    void streaming(){
        int cx=floorDiv(int(std::floor(game.player.position.x)),16),cz=floorDiv(int(std::floor(game.player.position.z)),16);
        if(pending.valid()&&pending.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=pending.get();
            if(result.revision==game.world.revision&&std::abs(result.x-cx)<=radius&&std::abs(result.z-cz)<=radius){auto& chunk=chunks[{result.x,result.z}];upload(chunk,result.geo,terrainShader);chunk.step=result.step;chunk.neighbors=result.neighbors;chunk.revision=result.revision;}}
        for(auto it=chunks.begin();it!=chunks.end();)if(std::abs(it->first.first-cx)>radius+1||std::abs(it->first.second-cz)>radius+1){release(it->second);it=chunks.erase(it);}else ++it;
        if(pending.valid())return;
        for(int d=0;d<=radius;++d)for(int z=cz-d;z<=cz+d;++z)for(int x=cx-d;x<=cx+d;++x){if(std::max(std::abs(x-cx),std::abs(z-cz))!=d)continue;
            int step=lod(x,z,cx,cz);std::array<int,4> n{lod(x-1,z,cx,cz),lod(x+1,z,cx,cz),lod(x,z-1,cx,cz),lod(x,z+1,cx,cz)};
            auto it=chunks.find({x,z});if(it!=chunks.end()&&it->second.revision==game.world.revision&&it->second.step==step&&it->second.neighbors==n)continue;
            auto snapshot=std::make_shared<World>(game.world);const Registry* registry=&game.registry;
            pending=std::async(std::launch::async,[snapshot,registry,x,z,step,n](){MeshResult result{x,z,step,n,snapshot->revision,{}};result.geo=meshColumn(*snapshot,x,z,step,n,*registry);return result;});return;
        }
    }
    int entityTarget(float reach)const {
        Ray ray{camera.position,rv(direction())};int target=-1;float best=reach;
        for(size_t i=0;i<game.world.entities.size();++i){const auto& e=game.world.entities[i];if(e.health<=0)continue;auto pos=rv(e.position);auto hit=GetRayCollisionBox(ray,{{pos.x-.5f,pos.y,pos.z-.5f},{pos.x+.5f,pos.y+1.8f,pos.z+.5f}});if(hit.hit&&hit.distance<best){target=int(i);best=hit.distance;}}
        return target;
    }
    void actions(float dt){
        auto& p=game.player;actionCooldown=std::max(0.f,actionCooldown-dt);
        for(int k=0;k<10;++k)if(IsKeyPressed(k==9?KEY_ZERO:KEY_ONE+k))p.selected=k;
        float wheel=GetMouseWheelMove();if(wheel!=0)p.selected=(p.selected+(wheel<0?1:9))%10;
        if(IsKeyPressed(KEY_F6)){brush=!brush;game.message=brush?"Wide terrain brush enabled":"Precise digging enabled";}
        if(IsKeyPressed(KEY_F7)){if(IsKeyDown(KEY_LEFT_SHIFT))game.buildSize=game.buildSize==7?3:game.buildSize+2;else game.buildShape=(game.buildShape+1)%5;const char* names[]={"single","line","wall","cube","sphere"};game.message="Build brush: "+std::string(names[game.buildShape])+" / size "+std::to_string(game.buildSize);}
        if(IsKeyPressed(KEY_F3))debug=!debug;if(IsKeyPressed(KEY_F4))wire=!wire;
        if(IsKeyPressed(KEY_F5))save();
        auto hit=game.world.raycast(vv(camera.position),direction(),p.creative?12.f:6.f,game.registry);
        auto& held=p.inventory.slots[p.selected];const auto& tool=game.registry.item(held.id);
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
            bool bow=held.id=="survival:bow";int ent=entityTarget(bow?32.f:4.f);
            if(ent>=0&&actionCooldown<=0){
                auto& e=game.world.entities[size_t(ent)];float distance=(e.position+Vec3{0,.8f,0}-vv(camera.position)).length();auto obstruction=game.world.raycast(vv(camera.position),direction(),distance,game.registry);
                if(!obstruction.hit){if(!bow||p.creative||p.inventory.remove("survival:arrow",1)){e.health-=tool.damage;actionCooldown=.45f;
                    if(!p.creative&&tool.durability>0&&++held.wear>=tool.durability)held={};
                    if(e.health<=0){p.xp+=5;p.inventory.add(game.registry,"survival:meat",2);game.message="Creature defeated; +5 XP";}else game.message="Hit "+e.type;
                }else game.message="No arrows";}
            }else if(hit.hit&&actionCooldown<=0){float duration=p.creative?.12f:std::max(.15f,game.registry.item(hit.id).hardness/tool.speed);mineProgress+=dt/duration;
                if(mineProgress>=1){if(game.mine(hit,brush)&&hasSound&&!mutedAudio)PlaySound(impact);mineProgress=0;actionCooldown=.16f;}
            }
        }else mineProgress=0;
        if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)&&actionCooldown<=0){
            bool used=false;
            if(hit.hit&&!IsKeyDown(KEY_LEFT_SHIFT))used=game.world.useStation(hit.cell,game.registry,p,game.message);
            if(!used)game.place(hit,brush);actionCooldown=.2f;
        }
        if(IsKeyPressed(KEY_Q)&&!held.empty()){held.count--;if(held.count<=0)held={};game.message="Discarded one item";}
    }
    void save(){game.world.save(game.player);game.message="World saved to SQLite";autosave=0;}
    void update(float dt){
        if(!playing)return;
        if(screen=="play"){
            if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_TAB))changeScreen("guide");
            else if(IsKeyPressed(KEY_E))changeScreen("inventory");
            else if(IsKeyPressed(KEY_SLASH)||IsKeyPressed(KEY_T)){console="/";changeScreen("console");}
            else {movement(dt);syncCamera();actions(dt);game.tick(dt);}
        }else if(IsKeyPressed(KEY_ESCAPE)||(screen=="inventory"&&IsKeyPressed(KEY_E)))changeScreen("play");
        streaming();autosave+=dt;if(autosave>30)save();syncCamera();
    }
    void drawMob(const Entity& e){
        if(e.health<=0)return;auto p=rv(e.position);bool monster=e.type=="zombie"||e.type=="skeleton"||e.type=="creeper";
        Color body=monster?(e.type=="skeleton"?Color{209,210,193,255}:Color{91,129,89,255}):e.type=="sheep"?Color{215,218,202,255}:Color{204,147,135,255};
        if(monster){DrawCube({p.x,p.y+1.05f,p.z},.65f,.85f,.35f,e.type=="skeleton"?body:Color{68,104,120,255});DrawCube({p.x,p.y+1.7f,p.z},.62f,.62f,.6f,body);
            for(float x:{-.19f,.19f})DrawCube({p.x+x,p.y+.36f,p.z},.24f,.72f,.3f,e.type=="skeleton"?body:Color{64,67,93,255});
            DrawCube({p.x-.18f,p.y+1.72f,p.z-.31f},.1f,.08f,.02f,{32,38,35,255});DrawCube({p.x+.18f,p.y+1.72f,p.z-.31f},.1f,.08f,.02f,{32,38,35,255});
        }else{DrawCube({p.x,p.y+.65f,p.z},1.05f,.7f,.62f,body);DrawCube({p.x+.55f,p.y+.85f,p.z},.45f,.5f,.5f,body);
            for(float x:{-.34f,.34f})for(float z:{-.2f,.2f}){float stride=.045f*std::sin(e.phase*5+x*8+z*8);DrawCube({p.x+x,p.y+.2f+stride,p.z+z},.18f,.4f,.18f,ColorBrightness(body,-.2f));}}
    }
    void scene(){
        float daylight=float(std::clamp(std::sin(game.world.time*6.2831853-1.5708)*.6+.55,.12,1.));
        Color sky=ColorLerp({13,24,42,255},{139,190,206,255},daylight);ClearBackground(sky);
        float fog[3]={sky.r/255.f,sky.g/255.f,sky.b/255.f},cam[3]={camera.position.x,camera.position.y,camera.position.z},start=float(radius*16)*.56f,end=float(radius*16)*.98f;
        SetShaderValue(terrainShader,GetShaderLocation(terrainShader,"cameraPosition"),cam,SHADER_UNIFORM_VEC3);
        SetShaderValue(terrainShader,GetShaderLocation(terrainShader,"fogColor"),fog,SHADER_UNIFORM_VEC3);
        SetShaderValue(terrainShader,GetShaderLocation(terrainShader,"fogStart"),&start,SHADER_UNIFORM_FLOAT);
        SetShaderValue(terrainShader,GetShaderLocation(terrainShader,"fogEnd"),&end,SHADER_UNIFORM_FLOAT);
        BeginMode3D(camera);rlDisableBackfaceCulling();
        for(auto& [key,c]:chunks){Color tint=ColorBrightness(WHITE,(daylight-1)*.55f);for(auto& m:c.models){if(wire)DrawModelWires(m,{0,0,0},1,tint);else DrawModel(m,{0,0,0},1,tint);}}
        // Local water surface. Density provides the shore and the renderer draws
        // a flat plane; this is a lake, not a simulated flowing liquid system.
        float wx=std::floor(game.player.position.x/16)*16,wz=std::floor(game.player.position.z/16)*16;
        DrawPlane({wx,6.f,wz},{float((radius+1)*32),float((radius+1)*32)},{71,135,159,165});
        for(const auto& e:game.world.entities)drawMob(e);
        for(const auto& [c,crop]:game.world.crops){auto p=rv(center(c));float h=.3f+.65f*crop.age/60;DrawCube({p.x,p.y+h/2,p.z},.6f,h,.08f,crop.age>=60?accent:teal);DrawCube({p.x,p.y+h/2,p.z},.08f,h,.6f,crop.age>=60?accent:teal);}
        if(hasCamp)DrawModel(camp,rv(campPos),1,WHITE);
        if(hasBarrel)DrawModel(barrel,rv(campPos+Vec3{1.8f,0,.5f}),1,WHITE);
        auto hit=game.world.raycast(vv(camera.position),direction(),game.player.creative?12.f:6.f,game.registry);
        if(hit.hit&&screen=="play"){
            if(hit.terrain){DrawSphereWires(rv(hit.point),brush?2.8f:.7f,8,12,{238,218,154,220});}
            else DrawCubeWires(rv(center(hit.cell)),1.015f,1.015f,1.015f,accent);
        }
        rlEnableBackfaceCulling();EndMode3D();
        if(game.world.water(game.player.position+Vec3{0,1.6f,0}))DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),{38,96,141,100});
    }
    void itemIcon(const Stack& s,int x,int y,int size){
        if(s.empty())return;const auto& d=game.registry.item(s.id);Color c=rgb(d.color);
        if(d.kind=="tool"){DrawLineEx({float(x+size/4),float(y+size*3/4)},{float(x+size*3/4),float(y+size/4)},5,{140,105,67,255});DrawRectangle(x+size/3,y+size/6,size*2/3,size/5,c);}
        else {DrawRectangle(x+3,y+3,size-6,size-6,ColorBrightness(c,-.2f));DrawRectangle(x+3,y+3,size-6,6,c);DrawRectangle(x+3,y+3,6,size-6,ColorBrightness(c,.12f));}
        if(s.count>1)label(std::to_string(s.count),x+size-MeasureText(std::to_string(s.count).c_str(),14),y+size-14,14);
        if(d.durability>0){DrawRectangle(x+2,y+size-3,size-4,3,{62,68,68,255});DrawRectangle(x+2,y+size-3,int((size-4)*std::clamp(1-float(s.wear)/d.durability,0.f,1.f)),3,teal);}
    }
    void hud(){
        int w=GetScreenWidth(),h=GetScreenHeight();auto& p=game.player;
        DrawRectangle(24,24,210,59,{18,30,33,215});label("VOXEL / LAB",38,34,23,accent);label(p.creative?"CREATIVE EXPLORATION":"SURVIVAL EXPEDITION",38,62,12,muted);
        label("TRANSVOXEL TERRAIN",w-221,29,14,ink);label("F3 diagnostics    Tab field guide",w-250,49,13,muted);
        int sx=w/2-300;
        for(int i=0;i<10;++i){Rectangle box{float(sx+i*60),float(h-82),54,54};DrawRectangleRounded(box,.12f,4,i==p.selected?Color{66,80,68,245}:Color{22,35,38,230});DrawRectangleRoundedLinesEx(box,.12f,4,i==p.selected?2.f:1.f,i==p.selected?accent:Color{67,83,80,255});itemIcon(p.inventory.slots[i],sx+i*60+9,h-77,35);label(i==9?"0":std::to_string(i+1),sx+i*60+4,h-78,10,muted);}
        if(!p.inventory.slots[p.selected].empty()){auto name=game.registry.item(p.inventory.slots[p.selected].id).name;label(name,w/2-MeasureText(name.c_str(),18)/2,h-113,18,ink);}
        if(!p.creative){bar(sx,h-140,145,p.health,20,{211,108,94,255},"HEALTH");bar(sx+165,h-140,145,p.hunger,20,accent,"HUNGER");bar(sx+330,h-140,145,p.stamina,100,teal,"STAMINA");bar(sx+495,h-140,105,p.oxygen,20,{110,171,207,255},"OXYGEN");}
        if(screen=="play"){int x=w/2,y=h/2;DrawLine(x-7,y,x-2,y,ink);DrawLine(x+2,y,x+7,y,ink);DrawLine(x,y-7,x,y-2,ink);DrawLine(x,y+2,x,y+7,ink);
            if(mineProgress>0){DrawRectangle(x-45,y+25,90,4,{30,45,47,200});DrawRectangle(x-45,y+25,int(90*mineProgress),4,accent);}
        }
        DrawRectangle(24,h-53,420,28,{18,30,33,190});label("WASD move   Space jump   LMB dig   RMB use/place",35,h-45,13,muted);
        if(!game.message.empty()){int size=15;std::string msg=game.message;if(msg.size()>115)msg=msg.substr(0,112)+"...";int tw=MeasureText(msg.c_str(),size);DrawRectangle(w/2-tw/2-12,96,tw+24,30,{18,30,33,220});label(msg,w/2-tw/2,104,size,ink);}
        if(debug){size_t tris=0,seams=0;for(const auto& [key,c]:chunks){tris+=c.triangles;seams+=c.transitions;}std::ostringstream s;s<<"C++20 / SQLite / Transvoxel\n"<<GetFPS()<<" FPS   "<<chunks.size()<<" columns   "<<tris<<" triangles\n"<<seams<<" transition triangles   "<<(pending.valid()?"meshing":"idle")<<"\nXYZ "<<int(p.position.x)<<" "<<int(p.position.y)<<" "<<int(p.position.z)<<"   seed "<<game.world.seed()<<"\n"<<game.world.blocks.size()<<" block edits   "<<game.world.edits.size()<<" density edits\nXP "<<p.xp<<"   "<<(brush?"wide brush":"precision brush");DrawRectangle(24,103,470,144,{18,30,33,220});label(s.str(),36,114,16,ink);}
    }
    void menu(){
        int w=GetScreenWidth(),h=GetScreenHeight();ClearBackground({15,26,30,255});
        // A restrained terrain contour motif keeps the menu independent of GPU assets.
        for(int j=0;j<20;++j){float base=h*.36f+j*22;for(int x=0;x<w;x+=9){float a=base+std::sin(x*.006f+j*.16f)*48+std::sin(x*.014f)*14,b=base+std::sin((x+9)*.006f+j*.16f)*48+std::sin((x+9)*.014f)*14;DrawLineEx({float(x),a},{float(x+9),b},1,{53,83,80,static_cast<unsigned char>(30+j*4)});}}
        label("VOXEL",w/2-260,85,70,ink);label("LAB",w/2-8,85,70,accent);label("A world you can reshape.",w/2-122,170,21,muted);
        int x=w/2-230,y=225;DrawRectangleRounded({float(x-25),float(y-25),510,375},.04f,6,panel);
        label("YOUR EXPEDITION",x,y,13,teal);label("WORLD NAME",x,y+37,12,muted);
        if(button({float(x),float(y+59),460,39},worldName,editing==1))editing=1;
        label("WORLD SEED",x,y+111,12,muted);if(button({float(x),float(y+132),460,39},seedText,editing==2))editing=2;
        if(button({float(x),float(y+190),222,42},"Creative",newCreative))newCreative=true;
        if(button({float(x+238),float(y+190),222,42},"Survival",!newCreative))newCreative=false;
        if(button({float(x),float(y+250),460,48},"Enter / Continue world",true)){if(worldName.empty())worldName="Meadow";try{start();editing=0;}catch(const std::exception& e){game.message=e.what();screen="menu";EnableCursor();}}
        if(editing){int c;auto& target=editing==1?worldName:seedText;while((c=GetCharPressed())>0){bool allowed=editing==1?std::isalnum(static_cast<unsigned char>(c))||c=='-'||c=='_':std::isdigit(static_cast<unsigned char>(c))||(c=='-'&&target.empty());if(c<128&&allowed&&target.size()<24)target.push_back(char(c));}if(IsKeyPressed(KEY_BACKSPACE)&&!target.empty())target.pop_back();}
        label("Native C++    /    Transvoxel LOD    /    SQLite worlds",w/2-240,h-78,17,muted);label("Same world name resumes its saved seed, inventory and game mode.",w/2-257,h-48,14,muted);
        if(!game.message.empty()&&game.message!="Welcome to Voxel Lab")label(game.message.substr(0,110),24,h-24,14,accent);
    }
    void inventoryPanel(int x,int y,int width,int height){
        if(button({float(x+28),float(y+80),145,34},"Inventory",panelPage==0))panelPage=0;
        if(button({float(x+183),float(y+80),145,34},"Crafting",panelPage==1))panelPage=1;
        if(game.player.creative&&button({float(x+338),float(y+80),145,34},"Creative",panelPage==2))panelPage=2;
        if(panelPage==0){label("Click two slots to swap / merge. Right-click a stack to split.",x+28,y+132,15,muted);
            for(int i=0;i<40;++i){int xx=x+28+(i%10)*66,yy=y+170+(i/10)*66;Rectangle b{float(xx),float(yy),58,58};DrawRectangleRounded(b,.1f,4,selectedSlot==i?Color{72,85,67,255}:Color{30,47,49,255});if(i==game.player.selected)DrawRectangleRoundedLinesEx(b,.1f,4,2,accent);itemIcon(game.player.inventory.slots[i],xx+8,yy+7,42);
                if(CheckCollisionPointRec(GetMousePosition(),b)){
                    const auto& s=game.player.inventory.slots[i];if(!s.empty())label(game.registry.item(s.id).name+"  /  "+s.id,x+28,y+height-65,15,ink);
                    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){if(selectedSlot<0)selectedSlot=i;else {auto& a=game.player.inventory.slots[size_t(selectedSlot)];auto& dest=game.player.inventory.slots[i];if(i!=selectedSlot&&a.id==dest.id&&a.wear==dest.wear&&!a.empty()){int n=std::min(a.count,game.registry.item(a.id).maxStack-dest.count);a.count-=n;dest.count+=n;if(a.count==0)a={};}else if(i!=selectedSlot)std::swap(a,dest);selectedSlot=-1;}}
                    if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)&&!s.empty()){for(auto& target:game.player.inventory.slots)if(target.empty()){int n=(s.count+1)/2;if(n==s.count)break;auto source=s;target={source.id,n,source.wear};game.player.inventory.slots[i].count-=n;break;}}
                }
            }
            label("Storage: RMB a chest with a selected stack to deposit.",x+28,y+height-40,14,muted);label("Select an empty hotbar slot, then RMB to withdraw.",x+28,y+height-22,14,muted);
        }else if(panelPage==1){
            int visible=std::max(1,(height-160)/64);recipeScroll=std::clamp(recipeScroll-int(GetMouseWheelMove()*2),0,std::max(0,int(game.registry.recipes.size())-visible));
            for(int i=0;i<visible&&i+recipeScroll<int(game.registry.recipes.size());++i){int idx=i+recipeScroll;auto& r=game.registry.recipes[size_t(idx)];int yy=y+132+i*64;
                bool can=true;std::string needs;for(const auto& a:r.inputs){if(!needs.empty())needs+=" + ";needs+=std::to_string(a.count)+" "+game.registry.item(a.id).name;if(game.player.inventory.count(a.id)<a.count)can=false;}
                label(r.name+"  x"+std::to_string(r.output.count),x+28,yy,18,can?ink:muted);label(needs.substr(0,72),x+28,yy+24,13,muted);
                if(button({float(x+width-139),float(yy),110,39},"Craft",can))game.craft(size_t(idx));
            }
        }else {
            std::vector<std::string> ids;for(const auto& [id,d]:game.registry.items)if(id!="core:air"&&id!="survival:crop")ids.push_back(id);
            int visible=std::max(1,(height-160)/48);catalogScroll=std::clamp(catalogScroll-int(GetMouseWheelMove()*2),0,std::max(0,int(ids.size())-visible));
            for(int i=0;i<visible&&i+catalogScroll<int(ids.size());++i){auto id=ids[size_t(i+catalogScroll)];const auto& d=game.registry.item(id);int yy=y+132+i*48;itemIcon({id,1,0},x+28,yy,30);label(d.name,x+72,yy+3,18);label(id,x+310,yy+7,12,muted);if(button({float(x+width-120),float(yy),90,35},"Add"))game.player.inventory.add(game.registry,id,d.maxStack);}
        }
    }
    void overlays(){
        if(screen=="play")return;
        int w=GetScreenWidth(),h=GetScreenHeight();DrawRectangle(0,0,w,h,{6,15,18,155});
        if(screen=="console"){
            DrawRectangle(24,h-190,w-48,155,panel);label("COMMAND CONSOLE",42,h-173,14,teal);label("/help  /give  /gamemode  /time  /tp  /summon  /save  /export",42,h-145,16,muted);
            int c;while((c=GetCharPressed())>0)if(c>=32&&c<127&&console.size()<200)console.push_back(char(c));if(IsKeyPressed(KEY_BACKSPACE)&&!console.empty())console.pop_back();label(console+"_",42,h-99,22,ink);
            if(IsKeyPressed(KEY_ENTER)){game.command(console);changeScreen("play");}return;
        }
        int width=std::min(910,w-72),height=std::min(600,h-80),x=(w-width)/2,y=(h-height)/2;
        DrawRectangleRounded({float(x),float(y),float(width),float(height)},.025f,6,panel);label(screen=="inventory"?"PACK & WORKSHOP":"FIELD GUIDE",x+28,y+25,27,accent);
        if(button({float(x+width-125),float(y+20),95,35},"Resume"))changeScreen("play");
        if(screen=="inventory"){inventoryPanel(x,y,width,height);return;}
        label("Shape a landscape. Build a home. Keep exploring.",x+28,y+75,19,ink);
        const char* rows[]={"W A S D     Move                 SPACE       Jump / fly up","SHIFT       Sprint               CTRL        Fly down","LMB         Mine / attack         RMB         Use / place / eat","1-0 / wheel Hotbar               E           Pack & crafting","F           Creative flight       F6          Wide terrain brush","F3          Diagnostics           F4          Wireframe / LOD seams","F5          Save world            /           Command console"};
        for(int i=0;i<7;++i)label(rows[i],x+28,y+118+i*28,17,muted);
        label("F7 shape brush / Shift F7 size. /dungeon creates a sanctum.",x+28,y+335,16,ink);
        label("/learn double_jump | dash (K) | parry (H) | slam (J): 20 XP.",x+28,y+360,16,ink);
        label("Furnace: ore + coal. Empty hand withdraws. Seeds grow wheat.",x+28,y+385,16,ink);
        if(button({float(x+28),float(y+430),195,39},"Open pack"))changeScreen("inventory");
        if(button({float(x+235),float(y+430),195,39},"Save world"))save();
        if(button({float(x+442),float(y+430),195,39},mutedAudio?"Sound: off":"Sound: on"))mutedAudio=!mutedAudio;
        if(button({float(x+28),float(y+485),195,39},"View distance -"))radius=std::max(3,radius-1);
        if(button({float(x+235),float(y+485),195,39},"View distance +"))radius=std::min(9,radius+1);
        if(button({float(x+442),float(y+485),195,39},"Save & main menu")){save();playing=false;screen="menu";EnableCursor();}
        label("C++ port in development. See docs/PORT_STATUS.md for feature coverage.",x+28,y+height-33,13,muted);
    }
};
}
int main(int argc,char** argv){
    bool smoke=false;int frames=900;std::string screenshot,scenario="play";std::filesystem::path saveDir,dataRoot;
    for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--smoke-test")smoke=true;else if(a=="--frames"&&i+1<argc)frames=std::stoi(argv[++i]);else if(a=="--screenshot"&&i+1<argc)screenshot=argv[++i];else if(a=="--save-dir"&&i+1<argc)saveDir=argv[++i];else if(a=="--data-root"&&i+1<argc)dataRoot=argv[++i];else if(a=="--scenario"&&i+1<argc)scenario=argv[++i];}
    if(dataRoot.empty()){
        auto candidate=std::filesystem::absolute(argv[0]).parent_path();
        for(int i=0;i<5;++i){if(std::filesystem::exists(candidate/"content/base.json")){dataRoot=candidate;break;}candidate=candidate.parent_path();}
        if(dataRoot.empty())dataRoot=VOXEL_SOURCE_DIR;
    }
    if(saveDir.empty())saveDir=dataRoot/"Worlds/Native";
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);SetTraceLogLevel(LOG_WARNING);InitWindow(1280,800,"Voxel Lab | C++ Transvoxel Sandbox");SetWindowMinSize(1000,720);SetExitKey(KEY_NULL);SetTargetFPS(90);
    InitAudioDevice();
    int exitCode=0;
    try {
        Client client(dataRoot,saveDir);client.assets();
        if(smoke){client.worldName="Smoke";client.newCreative=true;client.start();client.game.player.flying=true;client.game.player.position={28,33,38};client.game.player.yaw=-.62f;client.game.player.pitch=-.38f;client.debug=scenario=="play";
            if(scenario=="inventory"){client.changeScreen("inventory");client.panelPage=1;}
            if(scenario=="guide")client.changeScreen("guide");
            if(scenario=="dungeon"){client.game.dungeon();client.game.player.position=client.game.world.portals.begin()->second;client.game.player.yaw=3.14159f;client.game.player.pitch=-.04f;}
        }
        int frame=0;
        while(!WindowShouldClose()&&!client.exit){
            float dt=std::min(GetFrameTime(),.05f);
            try{if(!smoke)client.update(dt);else{if(scenario!="dungeon")client.game.tick(dt);client.syncCamera();client.streaming();}}
            catch(const std::exception& e){client.game.message=e.what();client.changeScreen(client.playing?"guide":"menu");}
            BeginDrawing();if(client.playing){client.scene();client.hud();client.overlays();}else client.menu();EndDrawing();
            if(smoke&&++frame>=frames){
                if(!screenshot.empty())TakeScreenshot(screenshot.c_str());client.save();
                size_t triangles=0,seams=0;for(const auto& [key,c]:client.chunks){triangles+=c.triangles;seams+=c.transitions;}
                Json result={{"chunks",client.chunks.size()},{"triangles",triangles},{"transition_triangles",seams},{"items",client.game.registry.items.size()},{"recipes",client.game.registry.recipes.size()},{"camp_model",client.hasCamp},{"barrel_model",client.hasBarrel},{"audio_loaded",client.hasSound},{"shader_loaded",client.terrainShader.id>0},{"scenario",scenario},{"sqlite_saved",true}};
                std::filesystem::create_directories(saveDir);std::ofstream(saveDir/"smoke-result.json")<<result.dump(2);if(triangles==0||seams==0)exitCode=2;break;
            }
        }
        if(client.playing)client.save();
    }catch(const std::exception& e){std::cerr<<"Voxel Lab: "<<e.what()<<"\n";exitCode=1;}
    CloseAudioDevice();CloseWindow();return exitCode;
}
