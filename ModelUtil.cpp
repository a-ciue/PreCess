#include "ModelUtil.h"

std::unique_ptr<MeshLib::CTMesh> ModelUtil::mesh_from_spline(std::filesystem::path spline_dir) {

   // cmd = Spline2Tri_BaseGen_Command.bat spline_dir output 60 
    std::string cmd = "Spline2Tri_BaseGen_Command.bat " + spline_dir.string() + " output 60";
    cmdPopen(cmd);

    std::filesystem::path output_dir = "output.m";


    std::unique_ptr<MeshLib::CTMesh> mesh = std::make_unique<MeshLib::CTMesh>();
    mesh->read_mesh(output_dir.string());
}


static std::unique_ptr<MeshLib::CTMesh> remesh_patches(std::unique_ptr<MeshLib::CTMesh> mesh, const std::vector<int>& patch_ids){
    // eg: ./Bin/TestCCGL.exe -yamabe_flow_poly_annulus_single ./Data/PatchedMesh/airplane_115.m ./Data/PatchedMesh/airplane_115.uv.m
    //call ./Bin/CDT_QT_Normal_Remesh.exe -MetricRemesh ./Data/PatchedMesh/airplane_115.uv.m ./Data/PatchedMesh/airplane_115.m 240 60 25 1 3 1 1 0
    // airplane_115 is the name of the mesh
    // patch_ids is [240 60 25 1 3 1 1 0]

    mesh->write_m("temp.m");

    std::string cmd1 = "./Bin/TestCCGL.exe -yamabe_flow_poly_annulus_single temp.m temp.uv.m";

    std::string cmd = "./Bin/CDT_QT_Normal_Remesh.exe -MetricRemesh temp.uv.m temp.m ";

    for (int i = 0; i < patch_ids.size(); i++) {
        cmd += std::to_string(patch_ids[i]) + " ";
    }

    cmdPopen(cmd1);
    cmdPopen(cmd);


}


std::string cmdPopen(const std::string& cmdLine) {
    char buffer[1024] = { '\0' };
    FILE* pf = NULL;
    pf = _popen(cmdLine.c_str(), "r");
    if (NULL == pf) {
        printf("open pipe failed\n");
        return std::string("");
    }
    std::string ret;
    while (fgets(buffer, sizeof(buffer), pf)) {
        ret += buffer;
    }
    _pclose(pf);
    return ret;
}
