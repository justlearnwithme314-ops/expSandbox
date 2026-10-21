#include "world.hpp"
#include <fstream>
#include <sstream>
namespace vl {
void Game::loadContent(const std::filesystem::path& root){
    registry.load(root/"content/base.json");
    auto mods=root/"content/mods";
    if(std::filesystem::exists(mods)){std::vector<std::filesystem::path> paths;for(const auto& e:std::filesystem::directory_iterator(mods))if(e.path().extension()==".json")paths.push_back(e.path());std::sort(paths.begin(),paths.end());for(const auto& p:paths)registry.load(p);}
}
void Game::start(const std::filesystem::path& path,int seed,bool creative){
    world=World(seed);player=Player{};player.creative=creative;
    player.position={4,world.height(4,4)+3,4};
    for(const auto& [id,n]:std::vector<std::pair<std::string,int>>{{"frontier:wood_pickaxe",1},{"core:log",16},{"building:planks",32},{"core:stone",32},{"survival:apple",8},{"survival:wheat_seeds",8},{"survival:chest",2},{"crafting:furnace",2},{"building:torch",16},{"survival:bow",1},{"survival:arrow",24}})player.inventory.add(registry,id,n);
    world.open(path,player);
    // Seed natural populations only for a new world, not after every reload.
    if(world.elapsed==0&&world.entities.empty())for(int i=0;i<14;++i){float x=float((i*17)%91-40),z=float((i*31)%87-40);world.entities.push_back({i+1,i%5==0?"zombie":i%3==0?"sheep":"pig",{x,world.height(x,z)+1,z},i%5==0?16.f:10.f,float(i),0});}
    events.emit({"world_ready",{{"seed",world.seed()}}});message="World ready. E inventory  |  Tab field guide";
}
void Game::respawn(){player.position={4,world.height(4,4)+4,4};player.velocity={};player.health=20;player.hunger=20;player.oxygen=20;message="Respawned at the meadow. Inventory retained.";}
void Game::tick(float dt){
    dt=std::clamp(dt,0.f,0.1f);player.defenseWindow=std::max(0.f,player.defenseWindow-dt);world.tick(dt,player,registry);survivalClock_+=dt;
    if(!player.creative&&survivalClock_>=1){survivalClock_=0;player.hunger=std::max(0.f,player.hunger-0.008f);
        bool under=world.water(player.position+Vec3{0,1.5f,0});
        player.oxygen=std::clamp(player.oxygen+(under?-1.3f:3.f),0.f,20.f);
        if(player.oxygen<=0||player.hunger<=0)player.health-=1;
        if(player.hunger>16&&player.health<20)player.health=std::min(20.f,player.health+0.12f);
    }
    if(player.health<=0)respawn();
}
static bool wearTool(Player& p,const Registry& r){auto& s=p.inventory.slots[p.selected];if(s.empty())return false;const auto& d=r.item(s.id);if(d.durability>0){++s.wear;if(s.wear>=d.durability)s={};}return true;}
bool Game::mine(const RayHit& hit,bool brush){
    if(!hit.hit)return false;
    std::string drop=registry.item(hit.id).drop;
    if(!player.creative&&registry.item(hit.id).hardness>=1000){message="Bedrock cannot be broken";return false;}
    auto oldInventory=player.inventory;
    if(!player.creative&&player.inventory.add(registry,drop,1)>0){message="Inventory full";return false;}
    if(hit.terrain){world.sculpt(hit.point,brush?2.8f:1.35f,false);}
    else {
        if(auto crop=world.crops.find(hit.cell);crop!=world.crops.end()&&crop->second.age>=60){
            if(player.inventory.add(registry,"survival:wheat",2)>0){player.inventory=oldInventory;message="Make room for the wheat harvest";return false;}
        }
        if(auto it=world.stations.find(hit.cell);it!=world.stations.end()){
            for(const auto& s:it->second.inventory.slots)if(!s.empty()&&player.inventory.add(registry,s.id,s.count,s.wear)>0){player.inventory=oldInventory;message="Make inventory room before breaking storage";return false;}
            world.stations.erase(it);
        }
        if(auto it=world.crops.find(hit.cell);it!=world.crops.end()){
            world.crops.erase(it);
        }
        world.setBlock(hit.cell,"core:air");
    }
    if(!player.creative){wearTool(player,registry);++player.xp;player.hunger=std::max(0.f,player.hunger-0.03f);}
    message="Collected "+registry.item(drop).name;events.emit({"block_broken",{{"id",hit.id}}});return true;
}
bool Game::place(const RayHit& hit,bool brush){
    auto& stack=player.inventory.slots[player.selected];if(stack.empty())return false;
    const auto& def=registry.item(stack.id);
    if(def.food>0){if(player.hunger>=20&&!player.creative){message="Already full";return false;}player.hunger=std::min(20.f,player.hunger+float(def.food));if(!player.creative&&--stack.count==0)stack={};message="Ate "+def.name;return true;}
    if(!hit.hit)return false;
    Cell pos=cellAt(hit.point+hit.normal*0.55f);if(!hit.terrain)pos={hit.cell.x+int(std::round(hit.normal.x)),hit.cell.y+int(std::round(hit.normal.y)),hit.cell.z+int(std::round(hit.normal.z))};
    if(stack.id=="survival:wheat_seeds"){
        if(hit.normal.y<0.5f){message="Plant seeds on soil";return false;}
        if(world.block(pos)!="core:air"){message="Planting space occupied";return false;}
        world.crops[pos]={0};world.setBlock(pos,"survival:crop");if(!player.creative&&--stack.count==0)stack={};message="Wheat planted; grows in one minute";return true;
    }
    if(!def.placeable){message="This item cannot be placed";return false;}
    if(buildShape>0&&!brush)return build(hit);
    Vec3 pc=center(pos);if(std::abs(pc.x-player.position.x)<0.8f&&std::abs(pc.z-player.position.z)<0.8f&&pc.y>player.position.y-0.5f&&pc.y<player.position.y+1.9f){message="Placement would intersect you";return false;}
    if(world.block(pos)!="core:air"){message="Space occupied";return false;}
    if(brush&&player.creative){world.sculpt(hit.point,2.2f,true);message="Added terrain";return true;}
    std::string id=stack.id;world.setBlock(pos,id);
    if(id=="survival:chest"||id=="crafting:furnace"||id=="industry:generator"||id=="industry:battery"||id=="industry:crusher")world.stations[pos]={id,{},0,0};
    if(!player.creative&&--stack.count==0)stack={};message="Placed "+def.name;events.emit({"block_placed",{{"id",id}}});return true;
}
bool Game::craft(std::size_t index){
    if(index>=registry.recipes.size())return false;const auto& recipe=registry.recipes[index];
    if(recipe.station!="hand"&&!player.creative){bool found=false;for(int z=-4;z<=4&&!found;++z)for(int y=-3;y<=3&&!found;++y)for(int x=-4;x<=4;++x){auto p=cellAt(player.position);auto id=world.block({p.x+x,p.y+y,p.z+z});if(id==recipe.station){found=true;break;}}
        if(!found){message="Requires nearby "+registry.item(recipe.station).name;return false;}}
    if(!player.inventory.craft(registry,recipe)){message="Missing ingredients or inventory space";return false;}player.xp+=2;message="Crafted "+registry.item(recipe.output.id).name;events.emit({"item_crafted",{{"recipe",recipe.id}}});return true;
}
bool World::useStation(Cell pos,const Registry& r,Player& p,std::string& msg){
    auto id=block(pos);
    if(auto portal=portals.find(pos);portal!=portals.end()){p.position=portal->second;p.velocity={};msg="Travelled through the sanctum gate";return true;}
    if(id=="building:bed"){time=0.28;p.health=20;msg="Rested until morning";return true;}
    if(id=="building:tnt"){
        sculpt(center(pos),4.f,false);
        for(int z=-3;z<=3;++z)for(int y=-3;y<=3;++y)for(int x=-3;x<=3;++x)if(x*x+y*y+z*z<12){Cell c{pos.x+x,pos.y+y,pos.z+z};if(!stations.contains(c))setBlock(c,"core:air");}
        msg="TNT detonated";return true;
    }
    auto it=stations.find(pos);if(it==stations.end())return false;
    auto& st=it->second;auto& selected=p.inventory.slots[p.selected];
    if(!selected.empty()){
        Inventory target=st.inventory;int left=target.add(r,selected.id,selected.count,selected.wear);
        int transferred=selected.count-left;if(transferred==0){msg="Station full";return true;}
        st.inventory=target;selected.count=left;if(left==0)selected={};msg="Deposited "+std::to_string(transferred)+" items";
    }else{
        for(auto& s:st.inventory.slots)if(!s.empty()){int n=s.count,left=p.inventory.add(r,s.id,n,s.wear);s.count=left;msg="Withdrew "+std::to_string(n-left)+" "+r.item(s.id).name;if(left==0)s={};return true;}
        msg="Station empty. Select an item to deposit.";
    }return true;
}
void World::tick(float dt,Player& p,const Registry& r){
    elapsed+=dt;time=std::fmod(time+dt/1200.,1.);tickAccumulator_+=dt;
    for(auto& e:entities){if(e.health<=0)continue;e.phase+=dt;e.cooldown=std::max(0.f,e.cooldown-dt);
        bool hostile=e.type=="zombie"||e.type=="skeleton"||e.type=="creeper";Vec3 dir=p.position-e.position;float dist=dir.length();
        auto walk=[&](Vec3 target){
            if(solid(target+Vec3{0,.7f,0},r))return;
            for(float y=e.position.y+1.f;y>=e.position.y-2.5f;y-=.1f)if(solid({target.x,y,target.z},r)){
                if(y+0.12f>e.position.y+.65f)return;
                e.position={target.x,y+.12f,target.z};return;
            }
        };
        if(hostile&&dist<23&&dist>1.3f){dir.y=0;dir=normalized(dir);walk(e.position+dir*(dt*1.9f));}
        else if(!hostile)walk(e.position+Vec3{std::sin(e.phase*.23f+e.id),0,std::cos(e.phase*.31f+e.id)}*(dt*.5f));
        if(hostile&&dist<1.8f&&e.cooldown<=0&&!p.creative){if(p.defenseWindow>0)e.health-=2;else p.health-=2;e.cooldown=1.2f;}
    }
    if(tickAccumulator_<0.5f)return;float delta=tickAccumulator_;tickAccumulator_=0;
    for(auto& [c,crop]:crops)if(block(c)=="survival:crop")crop.age=std::min(60.f,crop.age+delta);
    // Bounded loaded simulation. Adjacent generators feed batteries/processors.
    constexpr Cell directions[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for(auto& [c,s]:stations){
        if(block(c)!=s.type)continue;
        if(s.type=="industry:generator"){
            if(s.energy<100&&s.inventory.remove("survival:coal",1))s.energy+=80;
            for(auto d:directions){auto i=stations.find({c.x+d.x,c.y+d.y,c.z+d.z});if(i!=stations.end()&&i->second.type!="industry:generator"){float transfer=std::min({s.energy,delta*10.f,100.f-i->second.energy});s.energy-=transfer;i->second.energy+=transfer;}}
        }
        if(s.type=="crafting:furnace"||s.type=="industry:crusher"){
            if(s.type=="crafting:furnace"&&s.energy<1&&s.inventory.remove("survival:coal",1))s.energy+=40;
            std::string input,output;
            if(s.inventory.count("core:iron_ore")){input="core:iron_ore";output=s.type=="industry:crusher"?"industry:iron_dust":"frontier:iron_ingot";}
            else if(s.inventory.count("industry:iron_dust")){input="industry:iron_dust";output="frontier:iron_ingot";}
            else if(s.inventory.count("core:sand")){input="core:sand";output="building:glass";}
            if(!input.empty()&&s.energy>0){s.progress+=delta;s.energy=std::max(0.f,s.energy-delta);
                if(s.progress>=4){Inventory candidate=s.inventory;candidate.remove(input,1);if(candidate.add(r,output,s.type=="industry:crusher"?2:1)==0){s.inventory=candidate;s.progress=0;}}
            }else s.progress=0;
        }
    }
    // Sand/gravel falls only in the active neighborhood; container blocks never move.
    std::vector<std::pair<Cell,std::string>> falling;
    for(const auto& [c,id]:blocks)if((id=="core:sand"||id=="core:gravel")&&(center(c)-p.position).length()<40&&block({c.x,c.y-1,c.z})=="core:air"&&density(center(c)+Vec3{0,-1,0})>0)falling.push_back({c,id});
    for(const auto& [c,id]:falling){setBlock(c,"core:air");setBlock({c.x,c.y-1,c.z},id);}
}
void Game::command(const std::string& raw){
    std::istringstream in(raw);std::string cmd;in>>cmd;if(!cmd.empty()&&cmd[0]=='/')cmd.erase(0,1);
    if(cmd=="help"){message="/give ID N | /gamemode creative|survival | /time day|night | /tp X Y Z | /summon | /dungeon | /learn | /save | /export | /heal";return;}
    if(cmd=="dungeon"){dungeon();return;}
    if(cmd=="learn"){std::string skill;in>>skill;if(skill!="double_jump"&&skill!="dash"&&skill!="parry"&&skill!="slam"){message="Skills: double_jump, dash, parry, slam. Each costs 20 XP.";return;}if(player.skills.contains(skill)){message="Technique already learned";return;}if(!player.creative&&player.xp<20){message="Need 20 XP";return;}if(!player.creative)player.xp-=20;player.skills.insert(skill);message="Learned "+skill;return;}
    if(cmd=="give"){std::string id;int count=1;in>>id;if(!(in>>count))count=1;if(!registry.contains(id)){message="Unknown namespaced item ID";return;}int left=player.inventory.add(registry,id,std::clamp(count,1,999));message=left?"Inventory full":"Item added";return;}
    if(cmd=="gamemode"){std::string m;in>>m;if(m!="creative"&&m!="survival"){message="Use creative or survival";return;}player.creative=m=="creative";if(!player.creative)player.flying=false;message="Mode: "+m;return;}
    if(cmd=="time"){std::string t;in>>t;world.time=t=="night"?0.8:0.28;message="Time set";return;}
    if(cmd=="tp"){Vec3 p;if(in>>p.x>>p.y>>p.z&&std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<100000&&std::abs(p.z)<100000&&p.y>-30&&p.y<1000){player.position=p;player.velocity={};message="Teleported";}else message="Use /tp X Y Z (Y: -30..1000)";return;}
    if(cmd=="summon"){std::string type;in>>type;if(type!="pig"&&type!="sheep"&&type!="zombie"&&type!="skeleton"&&type!="creeper"){message="Available: pig sheep zombie skeleton creeper";return;}int id=1;for(const auto& e:world.entities)id=std::max(id,e.id+1);Vec3 p=player.position+Vec3{4,0,4};p.y=world.height(p.x,p.z);world.entities.push_back({id,type,p,12,0,0});message="Summoned "+type;return;}
    if(cmd=="heal"){player.health=20;player.hunger=20;message="Vitals restored";return;}
    if(cmd=="save"){world.save(player);message="World saved to SQLite";return;}
    if(cmd=="export"){world.save(player);auto path=pathFromUtf8(world.savePath).parent_path()/"export.sqlite";world.exportBackup(path);message="Consistent snapshot exported to "+path.string();return;}
    message="Unknown command. /help lists commands";
}
bool Game::build(const RayHit& hit){
    auto& stack=player.inventory.slots[player.selected];if(stack.empty())return false;
    Cell start=cellAt(hit.point+hit.normal*.6f);int half=buildSize/2;
    std::vector<Cell> cells;
    for(int z=-half;z<=half;++z)for(int y=0;y<buildSize;++y)for(int x=-half;x<=half;++x){
        bool include=buildShape==1?(y==0&&z==0):buildShape==2?(z==0):buildShape==3?true:(x*x+(y-half)*(y-half)+z*z<=half*half);
        if(!include)continue;Cell c{start.x+x,start.y+y,start.z+z};if(world.block(c)!="core:air")continue;
        Vec3 pc=center(c);if(std::abs(pc.x-player.position.x)<.8f&&std::abs(pc.z-player.position.z)<.8f&&pc.y>player.position.y-.5f&&pc.y<player.position.y+1.9f){message="Brush intersects player";return false;}
        if(world.density(pc)<0)continue;cells.push_back(c);
    }
    if(cells.empty()){message="No empty cells in brush";return false;}
    // Station metadata/containers require individual placement.
    if(stack.id=="survival:chest"||stack.id=="crafting:furnace"||stack.id.starts_with("industry:")){message="Place machines and containers individually";return false;}
    if(!player.creative&&stack.count<int(cells.size())){message="Need "+std::to_string(cells.size())+" blocks in selected stack";return false;}
    std::string id=stack.id;for(auto c:cells)world.setBlock(c,id);
    if(!player.creative){stack.count-=int(cells.size());if(stack.count==0)stack={};}message="Built "+std::to_string(cells.size())+" blocks";return true;
}
void Game::dungeon(){
    // A fresh, bounded nine-room sanctum. Work away from the player; do not
    // regenerate an existing container-bearing site on repeated commands.
    int ox=int(std::floor(player.position.x))+38,oz=int(std::floor(player.position.z));
    int oy=std::clamp(int(world.height(float(ox+12),float(oz+12)))-18,-24,12);
    for(const auto& [c,s]:world.stations)if(c.x>=ox-1&&c.x<=ox+26&&c.z>=oz-1&&c.z<=oz+26){message="A station occupies this site. Move before creating another sanctum.";return;}
    for(int z=0;z<25;++z)for(int x=0;x<25;++x)for(int y=0;y<=6;++y){
        Cell c{ox+x,oy+y,oz+z};Vec3 p{float(c.x),float(c.y),float(c.z)};
        bool wall=x==0||z==0||x==24||z==24||y==0||y==6;
        if((x==8||x==16)&&z%8!=4)wall=true;
        if((z==8||z==16)&&x%8!=4)wall=true;
        if(y>=1&&y<=4&&((x==8||x==16)&&z%8==4))wall=false;
        if(y>=1&&y<=4&&((z==8||z==16)&&x%8==4))wall=false;
        world.edits[c]=std::max(0.f,2.f-world.baseDensity(p));
        world.blocks[c]=wall?"building:bricks":"core:air";
    }
    Cell loot{ox+20,oy+1,oz+20};world.setBlock(loot,"survival:chest");auto& chest=world.stations[loot];chest.type="survival:chest";chest.inventory.add(registry,"frontier:diamond",5);chest.inventory.add(registry,"frontier:iron_ingot",12);chest.inventory.add(registry,"survival:apple",8);
    int id=1;for(const auto& e:world.entities)id=std::max(id,e.id+1);
    for(int i=0;i<5;++i)world.entities.push_back({id+i,"skeleton",{float(ox+4+(i%3)*8),float(oy+1),float(oz+12+(i/3)*8)},18,0,0});
    Cell gate=cellAt(player.position+Vec3{2,0,0});gate.y=int(std::ceil(world.height(float(gate.x),float(gate.z))));
    Cell back{ox+3,oy+1,oz+3};Vec3 arrival{float(ox+4)+.5f,float(oy+1)+.1f,float(oz+4)+.5f};
    world.setBlock(gate,"adventure:gate");world.portals[gate]=arrival;
    world.setBlock(back,"adventure:gate");world.portals[back]=center(gate)+Vec3{1.5f,1,0};
    ++world.revision;message="Nine-room sanctum created. RMB the violet gate beside you; chest awaits in the far room.";
}
}
