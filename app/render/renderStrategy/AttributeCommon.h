/** @file AttributeCommon.h
 * @brief 属性渲染模式和统一的标量属性查找规则。
 */
#pragma once

#include <string>

class vtkDataArray;
class vtkFieldData;

enum class Mode {
    RGB,
    SCALAR,
    UV,
    VECTOR
};

enum class ElementType {
    VERTEX,
    EDGE,
    FACE,
    SOLID
};

/** @brief 按名称命中的临时标量数组及其实体域，不持有数组生命周期。 */
struct ScalarAttribute {
    vtkDataArray* values { };
    ElementType type { ElementType::VERTEX };
};

/** @brief 按点、边、面、体顺序返回第一个同名数组，不因分量数或筛选能力跳过命中项。 */
ScalarAttribute findScalarAttribute(const std::string& attribute, vtkFieldData* points,
    vtkFieldData* edges, vtkFieldData* faces, vtkFieldData* solids);
