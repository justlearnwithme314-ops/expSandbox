#include "world.hpp"
#include <stdexcept>
// Official Eric Lengyel tables, MIT. See references/Transvoxel/LICENSE.
namespace vl::tables {
#include "../references/Transvoxel/Transvoxel.cpp"
}
namespace vl {
static Vertex vertex(Vec3 p,Vec3 n){return {p,n,{160,175,135,255}};}
static Vec3 gradient(const Field& field,Vec3 p,float e=0.5f) {
    return normalized({field(p+Vec3{e,0,0})-field(p-Vec3{e,0,0}),field(p+Vec3{0,e,0})-field(p-Vec3{0,e,0}),field(p+Vec3{0,0,e})-field(p-Vec3{0,0,e})});
}
static void triangle(Geometry& mesh,Vertex a,Vertex b,Vertex c,bool transition=false) {
    Vec3 n=cross(b.position-a.position,c.position-a.position);
    if(n.length()<0.0000001f)return;
    if(dot(n,a.normal+b.normal+c.normal)<0)std::swap(b,c);
    mesh.vertices.push_back(a);mesh.vertices.push_back(b);mesh.vertices.push_back(c);
    if(transition)++mesh.transitionTriangles;else ++mesh.regularTriangles;
}
Geometry meshRegular(const Field& field,Vec3 origin,Vec3 size,int step) {
    if(step<=0)throw std::invalid_argument("Meshing step must be positive");
    int nx=int(size.x)/step,ny=int(size.y)/step,nz=int(size.z)/step;
    if(nx<1||ny<1||nz<1||nx>256||ny>256||nz>256)throw std::invalid_argument("Invalid mesh dimensions");
    int sx=nx+3,sy=ny+3,sz=nz+3;
    std::vector<float> samples(size_t(sx)*sy*sz);
    auto index=[=](int x,int y,int z){return size_t((z+1)*sy+(y+1))*sx+(x+1);};
    for(int z=-1;z<=nz+1;++z)for(int y=-1;y<=ny+1;++y)for(int x=-1;x<=nx+1;++x)samples[index(x,y,z)]=field(origin+Vec3{float(x*step),float(y*step),float(z*step)});
    auto normal=[&](int x,int y,int z){return normalized({samples[index(x+1,y,z)]-samples[index(x-1,y,z)],samples[index(x,y+1,z)]-samples[index(x,y-1,z)],samples[index(x,y,z+1)]-samples[index(x,y,z-1)]});};
    Geometry mesh;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){
        std::array<Vec3,8> p;std::array<float,8> d;int code=0;
        for(int i=0;i<8;++i){int dx=i&1,dy=(i>>1)&1,dz=(i>>2)&1;p[i]=origin+Vec3{float((x+dx)*step),float((y+dy)*step),float((z+dz)*step)};d[i]=samples[index(x+dx,y+dy,z+dz)];if(d[i]<0)code|=1<<i;}
        if(code==0||code==255)continue;
        const auto& data=tables::regularCellData[tables::regularCellClass[code]];
        std::array<Vertex,12> vv;
        for(int i=0;i<data.GetVertexCount();++i){auto edge=tables::regularVertexData[code][i];int a=(edge>>4)&15,b=edge&15;
            float t=std::clamp(d[a]/(d[a]-d[b]),0.f,1.f);
            Vec3 na=normal(x+(a&1),y+((a>>1)&1),z+((a>>2)&1)),nb=normal(x+(b&1),y+((b>>1)&1),z+((b>>2)&1));
            vv[i]=vertex(p[a]+(p[b]-p[a])*t,normalized(na*(1-t)+nb*t));
        }
        for(int i=0;i<data.GetTriangleCount();++i)triangle(mesh,vv[data.vertexIndex[i*3]],vv[data.vertexIndex[i*3+1]],vv[data.vertexIndex[i*3+2]]);
    }return mesh;
}
Geometry meshTransition(const Field& field,Vec3 origin,Vec3 u,Vec3 v,int uCells,int vCells,int step) {
    if(step<1||uCells%2||vCells%2)throw std::invalid_argument("Transition faces need paired fine cells");
    Geometry mesh;
    constexpr int order[9]={0,1,2,5,8,7,6,3,4};
    for(int y=0;y<vCells;y+=2)for(int x=0;x<uCells;x+=2){
        std::array<Vec3,13> p;std::array<float,13> d;
        for(int i=0;i<9;++i){p[i]=origin+u*float((x+i%3)*step)+v*float((y+i/3)*step);d[i]=field(p[i]);}
        int code=0;for(int i=0;i<9;++i)if(d[order[i]]<0)code|=1<<i;
        if(code==0||code==511)continue;
        constexpr int coarse[4]={0,2,6,8};for(int i=0;i<4;++i){p[9+i]=p[coarse[i]];d[9+i]=d[coarse[i]];}
        auto cls=tables::transitionCellClass[code];const auto& data=tables::transitionCellData[cls&127];
        std::array<Vertex,12> vv;
        for(int i=0;i<data.GetVertexCount();++i){auto edge=tables::transitionVertexData[code][i];int a=(edge>>4)&15,b=edge&15;
            float t=std::clamp(d[a]/(d[a]-d[b]),0.f,1.f);Vec3 pos=p[a]+(p[b]-p[a])*t;vv[i]=vertex(pos,gradient(field,pos));}
        for(int i=0;i<data.GetTriangleCount();++i){int a=data.vertexIndex[i*3],b=data.vertexIndex[i*3+1],c=data.vertexIndex[i*3+2];if(cls&128)std::swap(b,c);triangle(mesh,vv[a],vv[b],vv[c],true);}
    }return mesh;
}
static void append(Geometry& a,Geometry b){a.regularTriangles+=b.regularTriangles;a.transitionTriangles+=b.transitionTriangles;a.vertices.insert(a.vertices.end(),b.vertices.begin(),b.vertices.end());}
Geometry meshColumn(const World& w,int cx,int cz,int step,const std::array<int,4>& neighbor,const Registry& r){
    Field f=[&](Vec3 p){return w.density(p);};Vec3 origin{float(cx*16),-32,float(cz*16)};
    Geometry out=meshRegular(f,origin,{16,96,16},step);
    // Zero-width transition patches lie on the shared plane. No chunk shrinking
    // is required: fine/coarse contour differences are filled on that plane.
    // This column layout has no Y-resolution boundary; all four lateral faces
    // use the same generic transition routine (which also supports Y faces).
    if(neighbor[0]==step*2)append(out,meshTransition(f,origin,{0,0,1},{0,1,0},16/step,96/step,step));
    if(neighbor[1]==step*2)append(out,meshTransition(f,origin+Vec3{16,0,0},{0,0,1},{0,1,0},16/step,96/step,step));
    if(neighbor[2]==step*2)append(out,meshTransition(f,origin,{1,0,0},{0,1,0},16/step,96/step,step));
    if(neighbor[3]==step*2)append(out,meshTransition(f,origin+Vec3{0,0,16},{1,0,0},{0,1,0},16/step,96/step,step));
    for(auto& v:out.vertices){auto color=r.item(w.terrainMaterial(v.position)).color;
        if(v.normal.y<0.55f)color=r.item("core:stone").color;
        float variation=0.94f+0.06f*std::sin(v.position.x*1.7f+v.position.z*0.8f);
        float lighting=0.50f+0.50f*std::max(0.f,dot(v.normal,normalized({-0.5f,0.9f,0.3f})));
        for(int i=0;i<3;++i)v.color[i]=static_cast<unsigned char>(color[i]*variation*lighting);v.color[3]=255;
    }
    constexpr Cell dirs[6]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
    constexpr int corners[6][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}};
    auto cube=[&](Cell c,const std::string& id){const auto& def=r.item(id);auto col=def.color;
        for(int face=0;face<6;++face){auto dir=dirs[face];auto next=w.block({c.x+dir.x,c.y+dir.y,c.z+dir.z});if(next!="core:air"&&r.item(next).solid)continue;
            Vertex vv[4];for(int i=0;i<4;++i){int k=corners[face][i];Vec3 p{float(c.x+(k&1)),float(c.y+((k>>1)&1)),float(c.z+((k>>2)&1))};float l=face==3?1.f:face==2?0.52f:face==0||face==1?0.78f:0.68f;vv[i]={p,{float(dir.x),float(dir.y),float(dir.z)},{static_cast<unsigned char>(col[0]*l),static_cast<unsigned char>(col[1]*l),static_cast<unsigned char>(col[2]*l),255}};}
            triangle(out,vv[0],vv[1],vv[2]);triangle(out,vv[0],vv[2],vv[3]);
        }
    };
    // Generated tree blocks; explicit edits may also place blocks outside this Y range.
    for(int z=cz*16;z<cz*16+16;++z)for(int x=cx*16;x<cx*16+16;++x)for(int y=5;y<43;++y){Cell c{x,y,z};auto id=w.block(c);if(id!="core:air"&&w.density(center(c))> -0.5f)cube(c,id);}
    for(const auto& [c,id]:w.blocks)if(c.x>=cx*16&&c.x<cx*16+16&&c.z>=cz*16&&c.z<cz*16+16&&(c.y<5||c.y>=43)&&id!="core:air")cube(c,id);
    return out;
}
}
