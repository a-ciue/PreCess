#include "MeshActor.h"
#include "MeshActor.h"
#include "MeshActor.h"
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>
#include "ModelUtil.h"
#include "Style.h"
#include "Core.h"
#include <vtkAppendPolyData.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkUnstructuredGrid.h>
#include <vtkTriangle.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkUnsignedCharArray.h>  
#include <vtkCellData.h>           
#include <cstdlib>                 
using Index = int;

vtkNew<vtkMinimalStandardRandomSequence> MeshActor::randomSequence;
vtkNew<vtkNamedColors> MeshActor::colors;



MeshActor::MeshActor(vtkRenderer* renderer, bool is_edge_render, ModelRenderMode render_mode)
{
    this->renderer_ = renderer;
    this->setRenderEdge(is_edge_render);
    this->render_mode_=render_mode;

}

void MeshActor::loadModelData(const MeshDataVtk& model_data)
{
	this->model_data_ = model_data;
	vtkIdType point_id=0;
    vtkSmartPointer<vtkPoints> points_data = vtkSmartPointer<vtkPoints>::New();
    for (const auto&point : this->model_data_.vtk_points_) {
        
        points_data->InsertNextPoint(point[0], point[1], point[2]);
        this->model_data_.model_point_id_.push_back(point_id++);
        /*cout << point_id <<"          " ;
        cout << this->model_data_.model_point_id_[point_id]<<endl;*/
    }

    vtkSmartPointer<vtkCellArray> triangles_data = vtkSmartPointer<vtkCellArray>::New();
    for (const auto&triangle : this->model_data_.vtk_triangles_) {
        Index triangle_idx=0;
        vtkIdType triangle_idxs[3]{ triangle[0], triangle[1], triangle[2] };
        triangles_data->InsertNextCell(3, triangle_idxs);
        this->model_data_.model_face_id_.push_back(triangle_idx++);
    }
    auto polyData = vtkSmartPointer<vtkPolyData>::New();

    polyData->SetPoints(points_data);
    polyData->SetPolys(triangles_data);

    this->mapper_->SetInputDataObject(polyData);
    createBlockMapper(this->model_data_);


}

void MeshActor::deleteMeshActor()
{
    if (this->renderer_)
    {
        renderer_->RemoveActor(this->actor_);
    }
}

void MeshActor::setVisibility(bool visibility)
{
	this->actor_->SetVisibility(visibility);
}

void MeshActor::setRenderEdge(bool is_render)
{
    this->actor_->GetProperty()->SetEdgeVisibility(is_render);
}

void MeshActor::setRenderMode(ModelRenderMode render_mode)
{
    this->render_mode_ = render_mode;
    if (render_mode_ == ModelRenderMode::Face) {
        this->actor_->SetMapper(this->mapper_);
        this->renderer_->AddActor(this->actor_);
    }
    else if (render_mode_ == ModelRenderMode::Block) {
        this->actor_->SetMapper(this->block_mapper_);
        this->renderer_->AddActor(this->actor_);
    }
    else {
        std::cerr << "invalid renderMode in QRenderWindow::changeRenderer" << std::endl;
        return;
    }
}

bool MeshActor::getIsEdgeRender()
{
    return this->edge_render_;
}

ModelRenderMode MeshActor::getMeshRenderMode()
{
    return this->render_mode_;
}

void MeshActor::addPickList(vtkPropCollection* pick_list) const
{
    pick_list->AddItem(this->actor_);
}

Index MeshActor::get_model_face_id(vtkIdType face_id) const
{
    return this->model_data_.model_face_id(face_id);
}

Index MeshActor::get_model_point_id(vtkIdType point_id) const
{
    return this->model_data_.model_point_id(point_id);
}

Index MeshActor::get_model_block_id(vtkIdType block_id) const
{
    return this->model_data_.model_block_id(block_id);
}

void MeshActor::createBlockMapper(const MeshDataVtk& model_data)
{
    auto multiblock = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    const auto& blocks = model_data.model_blocks_.block_datas;

    for (size_t block_index = 0; block_index < blocks.size(); ++block_index) {
        const auto& block = blocks[block_index];

        std::unordered_map<vtkIdType, vtkIdType> global_to_local;
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto cells = vtkSmartPointer<vtkCellArray>::New();
        auto grid = vtkSmartPointer<vtkPolyData>::New();

        auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
        colors->SetNumberOfComponents(3);
        colors->SetName("BlockColors");

        // 为该 block 随机生成颜色
        const std::array<unsigned char, 3> rgb = {
            static_cast<unsigned char>(rand() % 256),
            static_cast<unsigned char>(rand() % 256),
            static_cast<unsigned char>(rand() % 256)
        };

        vtkIdType local_id = 0;
        points->Allocate(static_cast<vtkIdType>(block.faces_.size() * 3));  // 预分配，粗略估计
        cells->AllocateEstimate(block.faces_.size(), 3);                     // 预估每个 cell 是三角形

        for (vtkIdType face_id : block.faces_) {
            const auto& tri = model_data.vtk_triangles_[face_id];
            vtkIdType tri_pts[3];

            for (int i = 0; i < 3; ++i) {
                vtkIdType global_id = tri[i];
                auto iter = global_to_local.find(global_id);
                if (iter == global_to_local.end()) {
                    const auto& pt = model_data.vtk_points_[global_id];
                    points->InsertPoint(local_id, pt.data());
                    global_to_local[global_id] = local_id;
                    tri_pts[i] = local_id++;
                }
                else {
                    tri_pts[i] = iter->second;
                }
            }

            cells->InsertNextCell(3, tri_pts);
            colors->InsertNextTypedTuple(rgb.data());
        }

        grid->SetPoints(points);
        grid->SetPolys(cells);
        grid->GetCellData()->SetScalars(colors);

        multiblock->SetBlock(static_cast<unsigned int>(block_index), grid);
    }

    this->block_mapper_->SetInputDataObject(multiblock);
    this->block_mapper_->SetScalarModeToUseCellData();
    this->block_mapper_->ScalarVisibilityOn();
}
