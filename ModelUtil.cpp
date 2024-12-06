#include "ModelUtil.h"
#include "ToolMesh.h"

std::unique_ptr<MeshLib::CTMesh>
ModelUtil::mesh_from_spline(std::filesystem::path spline_dir) {

  std::string mkdir_cmd = "mkdir ./Data/PatchedMesh";
  cmdPopen(mkdir_cmd);

  // cmd = Spline2Tri_BaseGen_Command.bat spline_dir output 60
  std::string cmd =
      "Spline2Tri_BaseGen_Command.bat " + spline_dir.string() + " temp 60";
  cmdPopen(cmd);

  std::string stitch_cmd = "./Bin/MeshStitching.exe ./Data/PatchedMesh/ "
                           "./Data/PatchedMesh/temp_BadPatches.txt"
                           "./Data/temp.m";
  cmdPopen(stitch_cmd);

  std::filesystem::path output_dir = "./Data/temp.m";

  std::unique_ptr<MeshLib::CTMesh> mesh = std::make_unique<MeshLib::CTMesh>();
  mesh->read_m(output_dir.string().c_str());
  return mesh;
}

std::unique_ptr<MeshLib::CTMesh>
ModelUtil::remesh_patches(std::unique_ptr<MeshLib::CTMesh> mesh,
               const std::vector<int> &patch_ids) {

  std::filesystem::remove("./Data/PatchedMesh/");
  std::string mkdir_cmd = "mkdir ./Data/PatchedMesh";
  cmdPopen(mkdir_cmd);
  mesh->write_m("./Data/temp.m");

  for (auto patch_id : patch_ids) {
    std::string patch_cmd =
        "./Bin/TestCCGL.exe -yamabe_flow_poly_annulus_single "
        "./Data/PatchedMesh/temp_" +
        std::to_string(patch_id) + ".m ./Data/PatchedMesh/temp_" +
        std::to_string(patch_id) + ".uv.m";
    cmdPopen(patch_cmd);

    std::string remesh_cmd =
        "./Bin/TestCCGL.exe -MetricRemesh ./Data/PatchedMesh/temp_" +
        std::to_string(patch_id) + ".uv.m ./Data/PatchedMesh/temp_" +
        std::to_string(patch_id) + ".m 240 60 25 1 3 1 1 0";
    cmdPopen(remesh_cmd);

    std::string stitch_cmd = "./Bin/MeshStitching.exe ./Data/PatchedMesh/ "
                             "./Data/PatchedMesh/temp_BadPatches.txt"
                             "./Data/temp.m";
    cmdPopen(stitch_cmd);
  }

  std::unique_ptr<MeshLib::CTMesh> patched_mesh =
      std::make_unique<MeshLib::CTMesh>();
  patched_mesh->read_m("./Data/PatchedMesh/temp.m");
  return patched_mesh;
}

std::string cmdPopen(const std::string &cmdLine) {
  char buffer[1024] = {'\0'};
  FILE *pf = NULL;
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

void ModelUtil::_attach_halfedge_to_edge(MeshLib::CToolHalfEdge* he0, MeshLib::CToolHalfEdge* he1,
	MeshLib::CToolEdge* e)
{
    if (he0 == NULL) {
        e->halfedge(0) = he1;
        e->halfedge(1) = NULL;
    } else if (he1 == NULL) {
        e->halfedge(0) = he0;
        e->halfedge(1) = NULL;
    } else {
        e->halfedge(0) = he0;
        e->halfedge(1) = he1;
    }

    if (he0 != NULL)
        he0->edge() = e;
    if (he1 != NULL)
        he1->edge() = e;
}
