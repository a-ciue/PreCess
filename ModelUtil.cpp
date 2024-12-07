#include "ModelUtil.h"
#include "ToolMesh.h"
#include "FaceSplitter.h"

#include <array>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <filesystem>

vtkNew<vtkMinimalStandardRandomSequence> ModelUtil::randomSequence;
vtkNew<vtkNamedColors> ModelUtil::colors;

std::unique_ptr<MeshLib::CTMesh>
ModelUtil::mesh_from_spline(std::filesystem::path spline_dir) {

  //std::string mkdir_cmd = "mkdir ./Data/PatchedMesh";
  //cmdPopen(mkdir_cmd);
    std::filesystem::remove_all("./Data/PatchedMesh");
    std::filesystem::create_directories("./Data/PatchedMesh");
    std::filesystem::copy_file(spline_dir, ".\\Data\\PatchedMesh\\temp.stp");

  // cmd = Spline2Tri_BaseGen_Command.bat spline_dir output 60
  std::string cmd =
      "Spline2Tri_BaseGen_Command.bat " ".\\Data\\PatchedMesh\\temp.stp" " .\\Data\\PatchedMesh\\temp 60";
  cmdPopen(cmd);

  std::string stitch_cmd = ".\\Bin\\MeshStitching.exe .\\Data\\PatchedMesh\\ "
                           ".\\Data\\PatchedMesh\\temp_BadPatches.txt "
                           ".\\Data\\temp.m";
  cmdPopen(stitch_cmd);

  std::filesystem::path output_dir = "./Data/temp.m";

  std::unique_ptr<MeshLib::CTMesh> mesh = std::make_unique<MeshLib::CTMesh>();
  mesh->read_m(output_dir.string().c_str());
  return mesh;
}

std::unique_ptr<MeshLib::CTMesh>
ModelUtil::remesh_patches(std::unique_ptr<MeshLib::CTMesh> mesh,
               const std::vector<int> &patch_ids) {

  std::filesystem::remove_all("./Data/PatchedMesh/");
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

MeshLib::CToolVertex* ModelUtil::split_face(MeshLib::CToolFace* face, MeshLib::CTMesh* mesh)
{
    using namespace MeshLib;
    using CFace = CTMesh::CFace;
    using tFace = CFace*;
    using CVertex = CTMesh::CVertex;
    using tVertex = CVertex*;
    using CHalfEdge = CTMesh::CHalfEdge;
    using tHalfEdge = CHalfEdge*;
    using CEdge = CTMesh::CEdge;
    int m_vertex_id = 0;

    for (std::list<tVertex>::iterator viter = mesh->vertices().begin(); viter != mesh->vertices().end(); viter++) {
        tVertex pV = *viter;
        m_vertex_id = (m_vertex_id > pV->id()) ? m_vertex_id : pV->id();
    }

    int m_face_id = 0;
    for (std::list<tFace>::iterator fiter = mesh->faces().begin(); fiter != mesh->faces().end(); fiter++) {
        tFace pF = *fiter;
        m_face_id = (m_face_id > pF->id()) ? m_face_id : pF->id();
    }

    CVertex* pV = mesh->createVertex(++m_vertex_id);

    CVertex* v[3];
    CHalfEdge* h[3];
    CHalfEdge* hs[3];

    CEdge* eg[3];

    h[0] = mesh->faceHalfedge(face);
    h[1] = mesh->faceNextCcwHalfEdge(h[0]);
    h[2] = mesh->faceNextCcwHalfEdge(h[1]);

    for (int i = 0; i < 3; i++) {
        v[i] = mesh->halfedgeTarget(h[i]);
        eg[i] = mesh->halfedgeEdge(h[i]);
        hs[i] = mesh->halfedgeSym(h[i]);
    }

    CFace* f = new CFace();
    assert(f != NULL);
    f->id() = ++m_face_id;
    f->get_g() = face->get_g();
    mesh->faces().push_back(f);

    // create halfedges
    tHalfEdge hes[3];
    for (int i = 0; i < 3; i++) {
        hes[i] = new CHalfEdge;
        assert(hes[i]);
    }

    // linking to each other
    for (int i = 0; i < 3; i++) {
        hes[i]->he_next() = hes[(i + 1) % 3];
        hes[i]->he_prev() = hes[(i + 2) % 3];
    }

    // linking to face
    for (int i = 0; i < 3; i++) {
        hes[i]->face() = f;
        f->halfedge() = hes[i];
    }

    f = new CFace();
    assert(f != NULL);
    f->id() = ++m_face_id;
    f->get_g() = face->get_g();
    mesh->faces().push_back(f);

    // create halfedges
    tHalfEdge hes2[3];

    for (int i = 0; i < 3; i++) {
        hes2[i] = new CHalfEdge;
        assert(hes2[i]);
    }

    // linking to each other
    for (int i = 0; i < 3; i++) {
        hes2[i]->he_next() = hes2[(i + 1) % 3];
        hes2[i]->he_prev() = hes2[(i + 2) % 3];
    }

    // linking to face
    for (int i = 0; i < 3; i++) {
        hes2[i]->face() = f;
        f->halfedge() = hes2[i];
    }

    CEdge* e[3];
    for (int i = 0; i < 3; i++) {
        e[i] = new CEdge();
        assert(e[i]);
        mesh->edges().push_back(e[i]);
    }

    _attach_halfedge_to_edge(h[1], hes[0], e[0]);
    _attach_halfedge_to_edge(hes[2], hes2[1], e[1]);
    _attach_halfedge_to_edge(h[2], hes2[0], e[2]);
    _attach_halfedge_to_edge(h[0], hs[0], eg[0]);
    _attach_halfedge_to_edge(hes[1], hs[1], eg[1]);
    _attach_halfedge_to_edge(hes2[2], hs[2], eg[2]);

    pV->halfedge() = h[1];

    h[1]->vertex() = pV;
    h[2]->vertex() = v[2];

    hes[0]->vertex() = v[0];
    hes[1]->vertex() = v[1];
    hes[2]->vertex() = pV;

    hes2[0]->vertex() = pV;
    hes2[1]->vertex() = v[1];
    hes2[2]->vertex() = v[2];

    v[0]->halfedge() = h[0];
    v[1]->halfedge() = hes[1];
    v[2]->halfedge() = hes2[2];
    /*
            for( int i = 0; i < 3; i ++ )
            {
                    v[i]->halfedge() = hs[i]->he_sym();
            }
    */
    return pV;
}

//MeshLib::CToolVertex* ModelUtil::split_edge(MeshLib::CToolEdge* edge, MeshLib::CTMesh* mesh)
//{
//    using namespace MeshLib;
//    using CFace = CTMesh::CFace;
//    using tFace = CFace*;
//    using CVertex = CTMesh::CVertex;
//    using tVertex = CVertex*;
//    using CHalfEdge = CTMesh::CHalfEdge;
//    using tHalfEdge = CHalfEdge*;
//    using CEdge = CTMesh::CEdge;
//    using tEdge = CEdge*;
//
//    int m_vertex_id = 0;
//
//    for (std::list<tVertex>::iterator viter = mesh->vertices().begin(); viter != mesh->vertices().end(); viter++) {
//        tVertex pV = *viter;
//        m_vertex_id = (m_vertex_id > pV->id()) ? m_vertex_id : pV->id();
//    }
//
//    int m_face_id = 0;
//    for (std::list<tFace>::iterator fiter = mesh->faces().begin(); fiter != mesh->faces().end(); fiter++) {
//        tFace pF = *fiter;
//        m_face_id = (m_face_id > pF->id()) ? m_face_id : pF->id();
//    }
//
//    CVertex* pV = mesh->createVertex(++m_vertex_id);
//
//    CHalfEdge* h[12];
//    CHalfEdge* s[6];
//    CVertex* v[6];
//    CEdge* eg[6];
//
//    h[0] = mesh->edgeHalfedge(edge, 0);
//    h[1] = mesh->faceNextCcwHalfEdge(h[0]);
//    h[2] = mesh->faceNextCcwHalfEdge(h[1]);
//
//    h[3] = mesh->edgeHalfedge(edge, 1);
//    h[4] = mesh->faceNextCcwHalfEdge(h[3]);
//    h[5] = mesh->faceNextCcwHalfEdge(h[4]);
//
//    CFace* f[4];
//    f[0] = mesh->halfedgeFace(h[0]);
//    f[1] = mesh->halfedgeFace(h[3]);
//
//    for (int i = 0; i < 6; i++) {
//        eg[i] = mesh->halfedgeEdge(h[i]);
//        v[i] = mesh->halfedgeVertex(h[i]);
//        s[i] = mesh->halfedgeSym(h[i]);
//    }
//
//    f[2] = new CFace();
//    assert(f[2] != NULL);
//    f[2]->id() = ++m_face_id;
//    mesh->faces().push_back(f[2]);
//
//    // create halfedges
//    for (int i = 6; i < 9; i++) {
//        h[i] = new CHalfEdge;
//        assert(h[i]);
//    }
//
//    // linking to each other
//    for (int i = 0; i < 3; i++) {
//        h[i + 6]->he_next() = h[6 + (i + 1) % 3];
//        h[i + 6]->he_prev() = h[6 + (i + 2) % 3];
//    }
//
//    // linking to face
//    for (int i = 6; i < 9; i++) {
//        h[i]->face() = f[2];
//        f[2]->halfedge() = h[i];
//    }
//
//    f[3] = new CFace();
//    assert(f[3] != NULL);
//    f[3]->id() = ++m_face_id;
//    mesh->faces().push_back(f[3]);
//
//    // create halfedges
//    for (int i = 9; i < 12; i++) {
//        h[i] = new CHalfEdge;
//        assert(h[i]);
//    }
//
//    // linking to each other
//    for (int i = 0; i < 3; i++) {
//        h[i + 9]->he_next() = h[9 + (i + 1) % 3];
//        h[i + 9]->he_prev() = h[9 + (i + 2) % 3];
//    }
//
//    // linking to face
//    for (int i = 9; i < 12; i++) {
//        h[i]->face() = f[3];
//        f[3]->halfedge() = h[i];
//    }
//
//    CEdge* e[3];
//
//    for (int i = 0; i < 3; i++) {
//        e[i] = new CEdge();
//        mesh->edges().push_back(e[i]);
//        assert(e[i]);
//    }
//
//    _attach_halfedge_to_edge(h[2], h[6], e[0]);
//    _attach_halfedge_to_edge(h[8], h[9], e[1]);
//    _attach_halfedge_to_edge(h[4], h[11], e[2]);
//
//    _attach_halfedge_to_edge(h[0], h[3], eg[0]);
//    _attach_halfedge_to_edge(h[1], s[1], eg[1]);
//    _attach_halfedge_to_edge(h[5], s[5], eg[5]);
//
//    _attach_halfedge_to_edge(h[7], s[2], eg[2]);
//    _attach_halfedge_to_edge(h[10], s[4], eg[4]);
//
//    h[0]->vertex() = v[0];
//    h[1]->vertex() = v[1];
//    h[2]->vertex() = pV;
//    h[3]->vertex() = pV;
//    h[4]->vertex() = v[4];
//    h[5]->vertex() = v[5];
//    h[6]->vertex() = v[1];
//    h[7]->vertex() = v[2];
//    h[8]->vertex() = pV;
//    h[9]->vertex() = v[2];
//    h[10]->vertex() = v[4];
//    h[11]->vertex() = pV;
//
//    v[0]->halfedge() = h[0];
//    v[1]->halfedge() = h[1];
//    v[2]->halfedge() = h[7];
//    v[4]->halfedge() = h[4];
//    pV->halfedge() = h[3];
//
//    for (int k = 0; k < 4; k++) {
//        CHalfEdge* pH = mesh->faceHalfedge(f[k]);
//        for (int i = 0; i < 3; i++) {
//            assert(pH->vertex() == pH->he_sym()->he_prev()->vertex());
//            pH = mesh->faceNextCcwHalfEdge(pH);
//        }
//    }
//    return pV;
//}
MeshLib::CToolVertex* ModelUtil::split_edge(MeshLib::CToolEdge* edge, MeshLib::CTMesh* mesh) {
    using namespace MeshLib;
    using V = MeshLib::CTMesh::CVertex;
    using H = MeshLib::CTMesh::CHalfEdge;

    bool notBoundary = edge->halfedge(1);
    MeshLib::CFaceSplitter<MeshLib::CTMesh> splitter(mesh);
    V* midv = splitter.split_edge(mesh->edgeHalfedge(edge, 0));

	std::array<H*, 2> vins {};
    vins[0] = mesh->vertexHalfedge(midv);
    vins[1] = mesh->halfedgeNext(mesh->halfedgeNext(vins[0]));
    std::pair p = splitter.create_edge(midv, vins[0], mesh->halfedgeTarget(vins[1]), vins[1]);
    p.second->get_g() = mesh->halfedgeFace(vins[0])->get_g();

    if (notBoundary)
    {
        vins[0] = mesh->vertexNextCcwInHalfEdge(vins[0]);
        vins[1] = mesh->halfedgeNext(mesh->halfedgeNext(vins[0]));
        p = splitter.create_edge(midv, vins[0], mesh->halfedgeTarget(vins[1]), vins[1]);
        p.second->get_g() = mesh->halfedgeFace(vins[0])->get_g();
    }
    return midv;
}

std::string ModelUtil::cmdPopen(const std::string& cmdLine)
{
    char buffer[1024] = { '\0' };
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
