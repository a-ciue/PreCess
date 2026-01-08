#ifndef ATTRIBUTE_OPERATOR_H
#define ATTRIBUTE_OPERATOR_H
#include "AttributeCommon.h"
#include "MeshActor.h"
#include <array>
#include <optional>
#include <string>
#include <vtkDataSet.h>
class MeshActor;

class AttributeOperator {
public:
    AttributeOperator(MeshActor* actor);
    /**
     * @brief 设置属性渲染方式
     * @param mode 渲染方式 0:RGB 1:SCALAR 2:UV 3:VECTOR
     * @param type 属性类型 0:VERTEX 1:EDGE 2:FACE 3:SOLID
     * @param attr_name 属性名称
     * @param texturePath 贴图路径，仅在mode为UV时有效，传空表示不设置贴图
     * @param glyphScale 箭头缩放比例，仅在mode为VECTOR时有效
     * @param scalarRange 标量范围，仅在mode为SCALAR时有效，传空表示不设置
     */
    void setAttriMode(
        const std::string& attr_name,
        Mode mode,
        ElementType type,
        const std::string& texture_path = "",
        double glyph_scale = -1,
        std::optional<std::pair<double, double>> scalar_range = std::nullopt);

    /**
     * @brief 取消属性渲染
     */
    void cancelActiveAttribute();

private:
    MeshActor* actor_;
    void setScalarRange(double min, double max);
    void setGlyph3DScaleFactor(double scale);
    void setActiveScalarAttribute(std::string attr_name, ElementType type);
    void setActiveVectorAttribute(std::string attr_name, ElementType type);
    void setActiveRGBAttribute(std::string attr_name, ElementType type);
    void createGlyph3D(vtkDataSet* input, const std::array<double, 3>& color, double scale = 0.3);
    void setTextureImage(std::string attr_name, std::string texturePath);
    void cancelTextureImage();
};

#endif // ATTRIBUTE_OPERATOR_H