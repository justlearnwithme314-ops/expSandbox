#include "c_api.h"
#include "world.hpp"
#include <cstring>
struct vl_game {vl::Game value;};
namespace {
thread_local std::string error;
template<class F> int32_t guard(vl_game* g,F operation){error.clear();if(!g){error="Null game handle";return 0;}try{operation(g->value);return 1;}catch(const std::exception& e){error=e.what();return 0;}catch(...){error="Unknown native error";return 0;}}
}
extern "C" {
uint32_t vl_api_version(){return 1;}
vl_game* vl_open(const char* root,const char* path,int32_t seed,int32_t creative){
    error.clear();if(!root||!path){error="Missing root or SQLite path";return nullptr;}
    try{auto g=std::make_unique<vl_game>();g->value.loadContent(vl::pathFromUtf8(root));g->value.start(vl::pathFromUtf8(path),seed,creative!=0);return g.release();}catch(const std::exception& e){error=e.what();return nullptr;}catch(...){error="Unknown native error";return nullptr;}
}
void vl_close(vl_game* g){delete g;}
const char* vl_last_error(){return error.c_str();}
int32_t vl_save(vl_game* g){return guard(g,[](vl::Game& game){game.world.save(game.player);});}
int32_t vl_command(vl_game* g,const char* command){return guard(g,[&](vl::Game& game){if(!command)throw std::invalid_argument("Null command");game.command(command);});}
int32_t vl_density(vl_game* g,float x,float y,float z,float* value){return guard(g,[&](vl::Game& game){if(!value||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||std::abs(x)>100000||std::abs(y)>1000||std::abs(z)>100000)throw std::invalid_argument("Invalid density arguments");*value=game.world.density({x,y,z});});}
int32_t vl_sculpt(vl_game* g,float x,float y,float z,float radius,int32_t add){return guard(g,[&](vl::Game& game){if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(radius)||radius<.25f||radius>8||std::abs(x)>100000||std::abs(y)>1000||std::abs(z)>100000)throw std::invalid_argument("Invalid sculpt coordinates/radius");game.world.sculpt({x,y,z},radius,add!=0);});}
int32_t vl_set_block(vl_game* g,int32_t x,int32_t y,int32_t z,const char* id){return guard(g,[&](vl::Game& game){if(!id||!game.registry.contains(id))throw std::invalid_argument("Unknown namespaced block ID");if(std::string(id)!="core:air"&&!game.registry.item(id).placeable)throw std::invalid_argument("Item is not placeable");game.world.setBlock({x,y,z},id);});}
size_t vl_profile_json(vl_game* g,char* buffer,size_t capacity){size_t required=0;guard(g,[&](vl::Game& game){auto text=game.player.json().dump();required=text.size()+1;if(buffer&&capacity>=required)std::memcpy(buffer,text.c_str(),required);});return required;}
}
