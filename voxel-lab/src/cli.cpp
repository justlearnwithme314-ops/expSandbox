#include "world.hpp"
#include <iostream>
int main(int argc,char** argv){
    if(argc<3){std::cout<<"Usage: voxel-cli DATA_ROOT WORLD.sqlite [\"/command\" ...]\nWithout commands, reads commands from standard input. /quit saves and exits.\n";return 0;}
    try{
        vl::Game game;game.loadContent(vl::pathFromUtf8(argv[1]));game.start(vl::pathFromUtf8(argv[2]),12345,true);
        if(argc>3){for(int i=3;i<argc;++i){game.command(argv[i]);std::cout<<game.message<<"\n";}}
        else {std::string line;while(std::getline(std::cin,line)){if(line=="/quit")break;game.command(line);std::cout<<game.message<<"\n";}}
        game.world.save(game.player);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
