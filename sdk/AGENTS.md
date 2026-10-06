# SDK 独立插件开发引导

本目录供已安装 SDK 下的插件开发使用。先读取 ../skills/zenithgrid-external-plugin-development/SKILL.md，再按任务读取构建或 API reference。

- 项目网址与源码主仓库：[PreCess](https://gitee.com/precess/PreCess)。仅有二进制分发包、需要查看实现或完整示例时，可到这里查阅源码；优先使用发行版对应的 tag/commit，接口以本次 SDK 头文件为准。
- 使用 C++20、兼容工具链和配套依赖；find_package(PreCess REQUIRED) 不限定版本，按 ABI 提示核对，Debug/非 Debug 不混用。
- 保留整个 examples 布局，不手动设置 PRECESS_PLUGIN_IN_TREE；产物在构建根目录的 plugins/。
- 安装前核对目标主程序与插件扫描目录；构建、测试通过不等于完成主程序加载验证。
- 模型写入、任务、预览和线程契约按 SDK 头文件与 skill 执行；正式功能优先用 Selector 选择目标。
- 示例使用 AGPLv3，OwnerQueue 保留 LGPLv3；GPL/AGPL 代码不回灌 LGPL SDK。
- 中文沟通；UTF-8 无 BOM、CRLF；未经要求不提交或推送。
