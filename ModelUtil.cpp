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
  std::filesystem::create_directories("./Data/PatchedMesh");
  make_bad_patches(patch_ids, ".\\Data\\PatchedMesh\\temp_BadPatches.txt");

  std::unordered_set<int> all_patches;
  for (MeshLib::CTMesh::MeshFaceIterator fi(mesh.get()); !fi.end(); fi++)
  {
      all_patches.insert(fi.value()->get_g());
  }
  for (int patch : all_patches)
  {
      mesh->Patch_Write(patch, ".\\Data\\PatchedMesh\\temp");
  }

  for (auto patch_id : patch_ids) {
    std::string patch_cmd =
        ".\\Bin\\TestCCGL.exe -yamabe_flow_poly_annulus_single "
        ".\\Data\\PatchedMesh\\temp_" +
        std::to_string(patch_id) + ".m .\\Data\\PatchedMesh\\temp_" +
        std::to_string(patch_id) + ".uv.m";
    cmdPopen(patch_cmd);

    std::string remesh_cmd =
        ".\\Bin\\CDT_QT_Normal_Remesh.exe -MetricRemesh .\\Data\\PatchedMesh\\temp_" +
        std::to_string(patch_id) + ".uv.m .\\Data\\PatchedMesh\\temp_" +
        std::to_string(patch_id) + ".m 240 60 25 1 3 1 1 0";
    cmdPopen(remesh_cmd);

    std::string stitch_cmd = ".\\Bin\\MeshStitching.exe .\\Data\\PatchedMesh\\ "
                             ".\\Data\\PatchedMesh\\temp_BadPatches.txt "
                             ".\\Data\\temp.m";
    cmdPopen(stitch_cmd);
  }

  std::unique_ptr<MeshLib::CTMesh> patched_mesh =
      std::make_unique<MeshLib::CTMesh>();
  patched_mesh->read_m("./Data/temp.m");
  return patched_mesh;
}

//! @brief 写obj文件，除了网格的连接信息意外，还要包括给定的分组信息
//! @param mesh 待写的网格
//! @param target_dir 目标路径
//! @param gid 分组id函数，传入patch_id返回group_id
void ModelUtil::write_group_obj(MeshLib::CTMesh* mesh, const std::filesystem::path& target_dir, std::function<int(int)> gid)
{
    int i = 1;
    for (MeshLib::CTMesh::MeshVertexIterator vi(mesh); !vi.end(); vi++) {
        MeshLib::CTMesh::CVertex* v = *vi;
        v->id() = i++;
    }
	std::ofstream obj(target_dir);
	for (MeshLib::CTMesh::MeshVertexIterator vi(mesh); !vi.end(); vi++) {
		MeshLib::CTMesh::CVertex* v = *vi;
		obj << "v " << v->point()[0] << " " << v->point()[1] << " " << v->point()[2] << std::endl;
	}
    std::unordered_map<int, std::vector<std::vector<int>>> groupMap;
	for (MeshLib::CTMesh::MeshFaceIterator fi(mesh); !fi.end(); fi++) {
		MeshLib::CTMesh::CFace* f = *fi;
		int group = gid(f->get_g());
		std::vector<int> face;
		for (MeshLib::CTMesh::FaceVertexIterator fvi(f); !fvi.end(); fvi++) {
			MeshLib::CTMesh::CVertex* v = *fvi;
			face.push_back(v->id());
		}
		groupMap[group].push_back(face);
	}

    for (auto& group : groupMap) {
        obj << "g " << group.first << std::endl;
        for (auto& face : group.second) {
            obj << "f";
            for (auto& v : face) {
                obj << " " << v;
            }
            obj << std::endl;
        }
    }
	
	obj.close();
}

void ModelUtil::write_group_inp(MeshLib::CTMesh* mesh, const std::filesystem::path& target_dir,
	std::function<int(int)> gid)
{
    std::unordered_map<int, std::vector<MeshLib::CTMesh::CFace*>> groupMap;
	for (MeshLib::CTMesh::MeshFaceIterator fi(mesh); !fi.end(); fi++) {
		MeshLib::CTMesh::CFace* f = *fi;
		int group = gid(f->get_g());
		groupMap[group].push_back(f);
	}

    // 输出
	std::ofstream inp(target_dir);

    for (auto&& [group_id, _] : groupMap) {
        inp << "*PART, Name=Part_" << group_id << '\n';
    }
    inp << "*NODE\n";
	for (MeshLib::CTMesh::MeshVertexIterator vi(mesh); !vi.end(); vi++) {
		MeshLib::CTMesh::CVertex* v = *vi;
        inp << v->id() << ',' << v->point()[0] << ',' << v->point()[1] << ',' << v->point()[2] << '\n';
	}
    for (auto&& [group_id, group_faces] : groupMap) {
        inp << "*ELEMENT,TYPE=S3,ELSET=Part_" << group_id << '\n';
        for (auto&& face : group_faces) {
            inp << face->id();
		    for (MeshLib::CTMesh::FaceVertexIterator fvi(face); !fvi.end(); fvi++) {
			    MeshLib::CTMesh::CVertex* v = *fvi;
                inp << ',' << v->id();
		    }
            inp << '\n';
        }
    }
	
	inp.close();
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
    mesh->map_vert()[pV->id()] = pV;

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
    mesh->map_face()[f->id()] = f;

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
    mesh->map_face()[f->id()] = f;

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
void ModelUtil::split_edge(MeshLib::CToolEdge* edge, MeshLib::CTMesh* mesh) {
    using namespace MeshLib;
    using V = MeshLib::CTMesh::CVertex;
    using H = MeshLib::CTMesh::CHalfEdge;

    bool notBoundary = edge->halfedge(1);
    CPoint mid = (mesh->edgeVertex1(edge)->point() + mesh->edgeVertex2(edge)->point()) / 2;

    MeshLib::CFaceSplitter<MeshLib::CTMesh> splitter(mesh);
    V* midv = splitter.split_edge(mesh->edgeHalfedge(edge, 0));
    midv->point() = mid;

	std::array<H*, 2> vins {};
    vins[0] = mesh->vertexHalfedge(midv);
    vins[1] = mesh->halfedgeNext(mesh->halfedgeNext(vins[0]));
    int g = mesh->halfedgeFace(vins[0])->get_g();
    std::pair p = splitter.create_edge(midv, vins[0], mesh->halfedgeTarget(vins[1]), vins[1]);
    p.second->get_g() = g;

    if (notBoundary)
    {
        vins[0] = mesh->vertexNextCcwInHalfEdge(vins[0]);
        vins[1] = mesh->halfedgeNext(mesh->halfedgeNext(vins[0]));
        g = mesh->halfedgeFace(vins[0])->get_g();
        p = splitter.create_edge(midv, vins[0], mesh->halfedgeTarget(vins[1]), vins[1]);
        p.second->get_g() = g;
    }
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

void ModelUtil::make_bad_patches(const std::vector<int>& patch_ids, std::string filename)
{
    std::ofstream file(filename);

    for (int patch_id : patch_ids)
    {
        file << "BadPatch temp_" << patch_id << ".m reverse\n";
    }
    file.close();
}

std::unique_ptr<MeshLib::CTMesh> ModelUtil::read_obj_with_groups(const std::filesystem::path& obj_file) {
    if (!std::filesystem::exists(obj_file)) {
        throw std::runtime_error("OBJ file does not exist: " + obj_file.string());
    }

    // 第一次读取：加载网格几何信息
    auto mesh = std::make_unique<MeshLib::CTMesh>();
    // 调用 CTMesh 的 read_obj 成员函数读取网格
    mesh->read_obj(obj_file.string().c_str());
    // 第二次读取：解析分组信息并更新面片的 m_g 属性
    std::ifstream obj_stream(obj_file);
    if (!obj_stream.is_open()) {
        throw std::runtime_error("Failed to open OBJ file for reading groups: " + obj_file.string());
    }

    if (!mesh) {
        throw std::runtime_error("Failed to load mesh geometry from OBJ file.");
    }

    std::string line;
    int current_group_id = -1; // 当前分组的 ID
    // 获取面片集合并进行迭代
    auto& face_list = mesh->faces();
    auto face_iter = face_list.begin();

    while (std::getline(obj_stream, line)) {
        std::istringstream line_stream(line);
        std::string prefix;
        line_stream >> prefix;

        // 处理 g 标签
        if (prefix == "g") {
            std::string group_name;
            line_stream >> group_name;

            // 为每个分组分配唯一的 ID
            current_group_id++;
            std::cout << "Group: " << group_name << " -> ID: " << current_group_id << std::endl;
        }
            // 处理面信息
        else if (prefix == "f" && face_iter != mesh->faces().end()) {
            // 为当前面设置分组 ID
            auto face = *face_iter;
            //face->set_g(current_group_id);  // 假设 `set_g` 更新 m_g 属性
            face->get_g() = current_group_id;
            ++face_iter;
        }
    }

    // 检查是否所有面都被分组
    if (face_iter != mesh->faces().end()) {
        throw std::runtime_error("Mismatch between OBJ face count and mesh faces.");
    }

    std::cout << "Finished reading OBJ file with groups. Total groups: " << (current_group_id + 1) << std::endl;
    return mesh;
}
//std::unique_ptr<MeshLib::CTMesh> ModelUtil::read_obj(const std::filesystem::path& obj_file){
//    auto mesh = std::make_unique<MeshLib::CTMesh>();
//
//    // 简单示例：加载几何点和面信息（具体实现取决于您的数据结构）
//    std::ifstream input(obj_file);
//    if (!input) {
//        throw std::runtime_error("Failed to open OBJ file.");
//    }
//
//    std::string line;
//    while (std::getline(input, line)) {
//        std::istringstream ss(line);
//        std::string prefix;
//        ss >> prefix;
//
//        if (prefix == "v") {
//            double x, y, z;
//            ss >> x >> y >> z;
//            // 添加顶点到网格中
//        } else if (prefix == "f") {
//            int v1, v2, v3;
//            ss >> v1 >> v2 >> v3;
//            // 添加面到网格中
//        }
//    }
//
//    return mesh;
//}