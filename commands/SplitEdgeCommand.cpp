//
// Created by 张家僮 on 5/14/25.
//
// command/SplitEdgeCommand.cpp
#include "SplitEdgeCommand.h"
#include <QVariantList>

SplitEdgeCommand::SplitEdgeCommand(ModelOperator model_op, QSelection* selection)
        : model_op_(model_op), selection_(selection) { }

void SplitEdgeCommand::execute() {
    auto* md = asMeshData();
    auto sel = selection->move();
    // 假定 sel->ids[0] 为 patch_id，sel->ids[1] 为 edge_v_id1，sel->ids[2] 为 edge_v_id2
    //int patch_id = sel->ids[0];
    int edge_v_id1 = sel->ids[0];
    int edge_v_id2 = sel->ids[1];
    //std::vector<int>& vids = patches.at(patch_id)->vertexIDs_;
    //std::array<int, 2> edge_v_gid { vids[edge_v_ids[0]], vids[edge_v_ids[1]] };
    std::array<int, 2> edge_v_gid { edge_v_id1, edge_v_id2 };
    MeshLib::CToolVertex *v1 = md->mesh_->idVertex(edge_v_gid[0]),
                         *v2 = md->mesh_->idVertex(edge_v_gid[1]);

    MeshLib::CToolEdge* edge {};
    for (MeshLib::CTMesh::VertexEdgeIterator ei(v1); !ei.end(); ei++)
    {
        MeshLib::CToolEdge* ce = *ei;
	    if (md->mesh_->edgeVertex1(ce) == v2 || md->mesh_->edgeVertex2(ce) == v2)
	    {
            edge = ce;
		    break;
	    }
    }

    if (!edge)
    {
        throw std::runtime_error("invalid vertex id pair");
    }

    // 记录父节点信息
    //int father_id = patches.at(patch_id)->father_id;

    ModelUtil::split_edge(edge, md->mesh_.get());

    std::vector<Index> patch_ids {};
    MeshLib::CToolHalfEdge* he1 = md->mesh_->edgeHalfedge(edge, 0);
    patch_ids.push_back(md->mesh_->halfedgeFace(he1)->get_g());
    if (MeshLib::CToolHalfEdge* he2 = md->mesh_->edgeHalfedge(edge, 1);
        he2 && md->mesh_->halfedgeFace(he2)->get_g() != patch_ids.front())
    // 被切分edge两侧不同属一个patch
    {
        patch_ids.push_back(md->mesh_->halfedgeFace(he2)->get_g());
    }

    // 更新涉及的patch的father_id
    //for (int pid : patch_ids) {
    //    update_father_id(pid, father_id);
    //}

    md->update_patches(patch_ids, false);

    model_op_.notifyChanged();
}

void SplitEdgeCommand::undo() {
    // TODO: 实现撤销逻辑（例如删除新顶点、恢复原面结构）
}

void SplitEdgeCommand::redo()
{
}

QList<ArgTypeObject*> SplitEdgeCommand::getArgsModel()
{
    QList<ArgTypeObject*> model;
    // 添加一个面选择器
    model.append(new ArgTypeObject(3, "选择边", "无"));

    return model;
}

unique_ptr<SplitEdgeCommand> SplitEdgeCommand::create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list)
{
    // 根据传入的参数创建 SplitFaceCommand 对象
    return std::make_unique<SplitEdgeCommand>(model_op, list.at(0).value<QSelection*>());
}
