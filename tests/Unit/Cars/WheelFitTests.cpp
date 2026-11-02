#include "Framework/Test.h"
#include "Game/Cars/WheelFit.h"
#include <array>
#include <cstring>
#include <limits>

namespace {
struct Vertex {float x,y,z,nx,ny,nz,u,v;};
oj::Mesh fixture(bool padding) {
    oj::Mesh mesh;
    const Vertex vertices[]={{-2,-4,0,0,1,0,.125f,.625f},{2,4,0,0,1,0,.25f,.5f},
                             {-2,0,-4,0,1,0,.375f,.75f},{2,0,4,0,1,0,.5f,.875f}};
    for(size_t i=0;i<2;++i) {
        oj::MeshPart part;part.name="tyre";part.stride=sizeof(Vertex);part.vertexCount=2;
        part.declaration={{0,0,2,0,0,0},{0,12,2,0,3,0},{0,24,1,0,5,0}};
        part.vertices.resize(2*sizeof(Vertex));
        std::memcpy(part.vertices.data(),vertices+i*2,part.vertices.size());
        part.bounds.min={-2,-4,-4};part.bounds.max={2,4,4};mesh.parts.push_back(part);
    }
    mesh.bounds.min=padding?std::array<float,3>{-3,-5,-5}:std::array<float,3>{-2,-4,-4};
    mesh.bounds.max=padding?std::array<float,3>{3,5,5}:std::array<float,3>{2,4,4};
    return mesh;
}
oj::MeshBounds target() {
    oj::MeshBounds b;b.min={4,12,22};b.max={16,28,38};b.centre={10,20,30};return b;
}
Vertex vertex(const oj::Mesh& mesh,size_t part,size_t index) {
    Vertex out;std::memcpy(&out,mesh.parts[part].vertices.data()+index*sizeof(Vertex),sizeof(out));return out;
}
}

OJ_UNIT_TEST(WheelFit, VisibleGeometryIgnoresCullingPadding) {
    using oj::test::check;
    auto padded=fixture(true),tight=fixture(false);
    check(oj::scene::fitWheelMesh(padded,target())&&oj::scene::fitWheelMesh(tight,target()),
          "both valid tyre meshes fit");
    bool same=true;for(size_t i=0;i<padded.parts.size();++i)same&=padded.parts[i].vertices==tight.parts[i].vertices;
    check(same,"culling padding does not change the fitted visible tyre");
    const auto a=vertex(padded,0,0),b=vertex(padded,0,1),c=vertex(padded,1,0),d=vertex(padded,1,1);
    check(a.x==4&&a.y==12&&a.z==30&&b.x==16&&b.y==28&&b.z==30&&c.z==22&&d.z==38,
          "actual vertices reach factory width and diameter at the requested centre");
    check(a.u==.125f&&a.v==.625f&&a.nx==0&&a.ny==1&&a.nz==0,
          "wheel UVs and normalized axis normal survive nonuniform fitting");
    check(padded.bounds.min==target().min&&padded.bounds.max==target().max,
          "whole-mesh bounds enclose the fitted visible tyre");
}

OJ_UNIT_TEST(WheelFit, MalformedGeometryLeavesInputUntouched) {
    using oj::test::check;
    for(int invalid=0;invalid<3;++invalid) {
        auto mesh=fixture(true);
        if(invalid==0)mesh.parts[1].declaration[0].offset=30;
        if(invalid==1)mesh.parts[1].vertices.pop_back();
        if(invalid==2) {
            const float nan=std::numeric_limits<float>::quiet_NaN();
            std::memcpy(mesh.parts[1].vertices.data(),&nan,sizeof(nan));
        }
        const auto before=mesh;
        check(!oj::scene::fitWheelMesh(mesh,target()),"invalid wheel position data is rejected");
        check(mesh.bounds.min==before.bounds.min&&mesh.bounds.max==before.bounds.max&&
                  mesh.parts[0].vertices==before.parts[0].vertices&&mesh.parts[1].vertices==before.parts[1].vertices,
              "rejected wheel is not partially resized");
    }
}
