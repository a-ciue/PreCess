#pragma once
#include <any>
#include <map>
#include <string>

class AttributeOperator;
/**
 * @brief 属性渲染策略接口
 * 
 * 定义属性渲染策略的接口，具体的渲染策略需要继承该接口并实现Render方法
 * 该接口用于不同类型属性的渲染策略，如RGB颜色、标量值、向量等
 * @author yh
 */
class IAttributeRenderStrategy {
public:
    virtual ~IAttributeRenderStrategy() = default;
    /**
     * @brief 属性渲染
     * @param op 属性操作器
     * @param attr_name 属性名称
     * @param args 渲染参数
     */
    virtual void render(
        AttributeOperator op,
        const std::string& attr_name,
        std::map<std::string, std::any> args)
        = 0;

    void cancelActiveAttribute(AttributeOperator op);
};