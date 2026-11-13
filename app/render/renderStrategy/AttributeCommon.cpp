/** @file AttributeCommon.cpp
 * @brief 共用基线的同名标量属性优先级。
 */
#include "AttributeCommon.h"
#include <array>
#include <vtkFieldData.h>

ScalarAttribute findScalarAttribute(const std::string& attribute, vtkFieldData* points,
    vtkFieldData* edges, vtkFieldData* faces, vtkFieldData* solids)
{
    const std::array<vtkFieldData*, 4> domains { points, edges, faces, solids };
    // 只决定首次命中，分量数与筛选能力由调用者随后判断。
    for (size_t domain = 0; domain < domains.size(); ++domain) {
        if (domains[domain]) {
            if (auto* values = domains[domain]->GetArray(attribute.c_str()))
                return { values, static_cast<ElementType>(domain) };
        }
    }
    return { };
}
