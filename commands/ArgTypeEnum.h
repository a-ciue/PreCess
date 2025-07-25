/**
 * @file ArgTypeEnum.h
 * @brief 参数类型枚举类定义，只被ArgType.h和QArgType.h.in使用。
 *
 * 因为需要被嵌入到其他头文件中，所以不在本文件中使用include guard。
 * @author 张家僮(htxz_6a6@163.com)
 */
enum class ArgTypeEnum {
     None = 0, // 无类型
     Int, // 整型
     Float, // 浮点型
     Text, // 字符串
     Bool, // 布尔型
     Path, // 文件路径
     Combo, // 下拉框选择类型
     // OBJECT, // 对象类型
     // ARRAY, // 数组类型
     // MAP, // 映射类型
     Selector // 选择器类型
 };