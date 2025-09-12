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
#include "QRenderWindowStyle.h"
#include "Core.h"
#include <vtkAppendPolyData.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkUnstructuredGrid.h>
#include <vtkTriangle.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkUnsignedCharArray.h>  
#include <vtkCellData.h>           
#include <vtkDoubleArray.h>
#include <cstdlib>                 

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
    this->model_data_ = std::make_unique<MeshDataVtk>(model_data);

    // point data
    auto points_data = vtkSmartPointer<vtkPoints>::New();
    {
        auto& vtk_points = this->model_data_->vtk_points_;
        auto points_data_array = vtkSmartPointer<vtkDoubleArray>::New();

        points_data_array->SetNumberOfComponents(3);
        points_data_array->SetArray(const_cast<double*>(vtk_points.data()->data()), 3 * vtk_points.size(), 1);
        points_data->SetData(points_data_array);
    }

    // cell (triangle) data
    auto triangles_data = vtkSmartPointer<vtkCellArray>::New();
    {
        auto triangles_data_array = vtkSmartPointer<vtkAOSDataArrayTemplate<Index>>::New();
        auto& vtk_triangles = this->model_data_->vtk_triangles_;
        triangles_data_array->SetArray(const_cast<Index*>(vtk_triangles.data()->data()), 3 * vtk_triangles.size(), 1);
        triangles_data->SetData(3, triangles_data_array);
    }

    // poly data
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points_data);
    polyData->SetPolys(triangles_data);

    this->mapper_->SetInputDataObject(polyData);
    createBlockMapper(*this->model_data_);
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
    this->visibility_ = visibility;
	this->actor_->SetVisibility(visibility);
}

void MeshActor::setRenderEdge(bool is_render)
{
    this->edge_render_ = is_render;
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

Index MeshActor::get_model_block_id(vtkIdType block_id) const
{
    return this->model_data_->model_block_id(block_id);
}

void MeshActor::createBlockMapper(const MeshDataVtk& model_data)
{
    auto multiblock = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    const auto& blocks = model_data.model_blocks_->block_datas;

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
