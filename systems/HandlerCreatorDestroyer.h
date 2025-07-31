#ifndef SYSTEMS_HANDLER_CREATOR_DESTROYER
#define SYSTEMS_HANDLER_CREATOR_DESTROYER
#include <any>
#include <functional>

namespace systems {
/**
 * @brief 处理器的创建与删除函数，用于构造与析构插件来的处理器
 *
 * 因为需要保证“哪里new出来的哪里delete”，
 */
struct HandlerCreatorDestroyer {
	/**
	 * @brief 从插件构造处理器对象并返回基类指针。
	 *
	 * 此函数使用std::any返回新构造的处理器对象指针，包装类型是系统处理器接口指针SystemHandler*
	 */
	std::function<std::any()> creator;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
	/**
	 * @brief 析构从creator获取的处理器对象。
	 *
	 * 传入的std::any包装类型和creator的返回值相同
	 * @sa HandlerCreatorDestroyer::creator
	 */
	std::function<void(std::any)> destroyer;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};
}

#endif // SYSTEMS_HANDLER_CREATOR_DESTROYER