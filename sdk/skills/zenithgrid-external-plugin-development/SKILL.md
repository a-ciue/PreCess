---
name: zenithgrid-external-plugin-development
description: 使用已安装的 ZenithGrid / PreCess SDK 开发、构建、测试和安装源外 C++ 插件。用于示例或独立仓库的 SDK 构建，不用于随主程序构建项目内插件或示例，也不用于 SDK 打包。
---

# ZenithGrid 源外插件开发（SDK 独立构建）

项目网址与源码主仓库：[PreCess](https://gitee.com/precess/PreCess)。仅有二进制分发包、需要查看实现或完整示例时，可到这里查阅源码；优先使用发行版对应的 tag/commit，接口以本次 SDK 头文件为准。

SDK 包与 API 仍叫 PreCess，插件使用 C++20。新增算法一律使用 FeatureHandler；AlgorithmHandler / AlgorithmSystem 已废弃并冻结，不得新增旧算法插件或扩展旧接口。按项目预设、PreCess_DIR 或 PRECESS_SDK_PATH 定位用户指定的 SDK；本 skill 随 SDK 安装时，其上两级目录为 SDK 前缀。

- 构建、测试、加载、安装、打包：读取 [references/build.md](references/build.md)。
- Handler、选择器、事件、模型写入、任务、预览、交互：读取 [references/development.md](references/development.md)，以已安装头文件确认签名。
- 最小骨架参考 examples/ExternalPlugin；事件、任务、预览、旧算法进度（仅兼容示例，不用于新增插件）分别参考 FeatureDemoPlugin、TaskDemoPlugin、ScalePreviewPlugin、ProgressDemoPlugin。

不固定 SDK 或编译器版本；按 SDK 的依赖/ABI 提示核对兼容性，Debug/非 Debug 不混用，不常规关闭校验。示例的活动组件查询是教学简化，正式功能用 Selector。GPL/AGPL 代码及依赖留在插件工程，不引入 LGPL SDK。

交付说明实际构建、测试与加载结果；未启动主程序时不声称运行验证通过。未经用户要求不提交、推送或发布。
