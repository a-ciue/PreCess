/**
 * @file QModelQuery.cpp
 * @brief 模型层查询的 QML 适配器实现（逻辑委托给 SessionQuery）
 */
#include "QModelQuery.h"
#include "SessionQuery.h"

#include <QString>
#include <QVariantList>
#include <spdlog/spdlog.h>

#include <string>
#include <vector>

QModelQuery::QModelQuery(session::SessionQuery* query, QObject* parent)
    : QObject(parent)
    , m_query(query)
{
}

std::optional<MeshDataVtk> QModelQuery::getMeshData(Index model_id)
{
    return m_query->meshData(model_id);
}

std::optional<MeshDataVtk> QModelQuery::getMeshDataByComponent(Index component_id)
{
    return m_query->meshDataByComponent(component_id);
}

std::optional<Index> QModelQuery::findEdgeByEndpoints(Index component_id, Index p0, Index p1)
{
    return m_query->findEdgeByEndpoints(component_id, p0, p1);
}

Index QModelQuery::pointGlobalId(Index component_id, Index local_point_id) const
{
    return m_query->pointGlobalId(component_id, local_point_id);
}

std::vector<GeometryDataVtk> QModelQuery::getGeometryVtkData(Index model_id)
{
    return m_query->geometryData(model_id);
}

std::optional<GeometryDataVtk> QModelQuery::getGeometryVtkDataByComponent(Index component_id)
{
    return m_query->geometryDataByComponent(component_id);
}

std::vector<Index> QModelQuery::getComponentIds(Index model_id) const
{
    return m_query->componentIds(model_id);
}

int QModelQuery::findModelIdByComponent(Index component_id) const
{
    return m_query->findModelIdByComponent(component_id);
}

bool QModelQuery::hasModel(Index model_id) const
{
    return m_query->hasModel(model_id);
}

bool QModelQuery::hasComponent(Index component_id) const
{
    return m_query->hasComponent(component_id);
}

QVariantList QModelQuery::getGeometryEdgeMappedPointIds(Index component_id, int localGeometryEdgeId)
{
    QVariantList out;
    for (Index pid : m_query->geometryEdgeMappedPointIds(component_id, localGeometryEdgeId))
        out.push_back(pid);
    return out;
}

QString QModelQuery::getModelName(Index model_id) const
{
    const auto name = m_query->modelName(model_id);
    if (!name) {
        spdlog::error("模型不存在，无法获取名称,id:{}", model_id);
        return QString();
    }
    return QString::fromStdString(*name);
}

QString QModelQuery::getComponentName(Index component_id) const
{
    const auto name = m_query->componentName(component_id);
    return name ? QString::fromStdString(*name) : QString();
}

QStringList QModelQuery::getModelAttriName(Index model_id) const
{
    if (!m_query->hasModel(model_id)) {
        spdlog::error("模型不存在，无法获取属性名，id:{}", model_id);
        return { };
    }
    QStringList attri_list;
    for (const auto& attr : m_query->modelAttributes(model_id))
        attri_list.append(QString::fromStdString(attr.name));
    return attri_list;
}

QList<Element::Type> QModelQuery::getModelAttriType(Index model_id) const
{
    if (!m_query->hasModel(model_id)) {
        spdlog::error("模型不存在，无法获取属性类型，id:{}", model_id);
        return { };
    }
    QList<Element::Type> type_list;
    for (const auto& attr : m_query->modelAttributes(model_id))
        type_list.append(attr.type);
    return type_list;
}

QVariantList QModelQuery::getComponentAttriInfo(Index component_id) const
{
    QVariantList out;
    for (const auto& info : m_query->componentAttributeInfos(component_id)) {
        QVariantMap item;
        item["name"] = QString::fromStdString(info.name);
        item["displayName"] = QString::fromStdString(info.display_name);
        item["type"] = static_cast<int>(info.type);
        item["typeName"] = QString::fromStdString(info.type_name);
        item["attrType"] = info.attr_type;
        item["componentCount"] = info.component_count;
        out.append(item);
    }
    return out;
}

QVariantList QModelQuery::listModels() const
{
    QVariantList out;
    for (const auto& summary : m_query->listModels()) {
        QVariantMap m;
        m["model_id"] = summary.model_id;
        m["name"] = QString::fromStdString(summary.name);
        m["component_count"] = summary.component_count;
        out.push_back(m);
    }
    return out;
}

QVariantList QModelQuery::getComponentsSummary(Index model_id) const
{
    QVariantList out;
    for (const auto& summary : m_query->componentSummaries(model_id)) {
        QVariantMap m;
        m["component_id"] = summary.component_id;
        m["name"] = QString::fromStdString(summary.name);
        m["has_mesh"] = summary.has_mesh;
        m["has_geometry"] = summary.has_geometry;
        m["material_id"] = summary.material_id;
        out.push_back(m);
    }
    return out;
}

QVariantMap QModelQuery::getMeshSummary(Index component_id) const
{
    QVariantMap m;
    const auto summary = m_query->meshSummary(component_id);
    m["has_mesh"] = summary.has_mesh;
    if (!summary.has_mesh)
        return m;

    m["vertex_count"] = summary.vertex_count;
    m["edge_count"] = static_cast<int>(summary.edge_count);
    m["face_count"] = static_cast<int>(summary.face_count);
    m["solid_count"] = static_cast<int>(summary.solid_count);
    return m;
}

QVariantMap QModelQuery::getGeometrySummary(Index component_id) const
{
    QVariantMap m;
    const auto summary = m_query->geometrySummary(component_id);
    m["has_geometry"] = summary.has_geometry;
    if (!summary.has_geometry)
        return m;

    m["vertex_count"] = summary.vertex_count;
    m["edge_count"] = summary.edge_count;
    m["face_count"] = summary.face_count;
    m["solid_count"] = summary.solid_count;
    return m;
}

std::optional<GeomFaceId> QModelQuery::resolveGeometryFaceLocalId(Index component_id, int localFaceId)
{
    return m_query->resolveGeometryFaceLocalId(component_id, localFaceId);
}

std::optional<GeomEdgeId> QModelQuery::resolveGeometryEdgeLocalId(Index component_id, int localEdgeId)
{
    return m_query->resolveGeometryEdgeLocalId(component_id, localEdgeId);
}

std::optional<GeomVertexId> QModelQuery::resolveGeometryVertexLocalId(Index component_id, int localVertexId)
{
    return m_query->resolveGeometryVertexLocalId(component_id, localVertexId);
}

std::optional<GeomSolidId> QModelQuery::resolveGeometrySolidLocalId(Index component_id, int localSolidId)
{
    return m_query->resolveGeometrySolidLocalId(component_id, localSolidId);
}
