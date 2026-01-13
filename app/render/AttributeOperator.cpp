#include "AttributeOperator.h"

AttributeOperator::AttributeOperator(MeshActor* mesh_actor)
    : mesh_actor_(mesh_actor) {};

vtkPolyDataMapper* AttributeOperator::getVertexMapper() { 
    return mesh_actor_->vertex_mapper_; 
}

vtkPolyDataMapper* AttributeOperator::getEdgeMapper() { 
    return mesh_actor_->edge_mapper_; 
}

vtkPolyDataMapper* AttributeOperator::getFaceMapper() { 
    return mesh_actor_->face_mapper_; 
}

vtkPolyDataMapper* AttributeOperator::getSolidMapper() { 
    return mesh_actor_->solid_mapper_; 
}

vtkPolyDataMapper* AttributeOperator::getGlyph3DMapper() { 
    return mesh_actor_->glyph3D_mapper_; 
}

vtkActor* AttributeOperator::getSolidActor() { 
    return mesh_actor_->solid_actor_; 
}

vtkActor* AttributeOperator::getFaceActor() { 
    return mesh_actor_->face_actor_; 
}

vtkActor* AttributeOperator::getEdgeActor() { 
    return mesh_actor_->edge_actor_; 
}

vtkActor* AttributeOperator::getVertexActor() { 
    return mesh_actor_->vertex_actor_; 
}

vtkActor* AttributeOperator::getGlyph3DActor() { 
    return mesh_actor_->glyph3D_actor_; 
}

vtkUnstructuredGrid* AttributeOperator::getSolidData() { 
    return mesh_actor_->solid_data_; 
}

vtkPolyData* AttributeOperator::getFaceData() { 
    return mesh_actor_->face_data_; 
}

vtkPolyData* AttributeOperator::getEdgeData() { 
    return mesh_actor_->edge_data_; 
}

vtkPolyData* AttributeOperator::getVertexData() { 
    return mesh_actor_->vertex_data_; 
}