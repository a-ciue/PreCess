# 许可证说明（中文）

> 本文件为中文译本，仅供参考。若有歧义，以英文版 `LICENSE.md` 及许可证原文为准。

## 本仓库自有代码 —— LGPLv3

**本仓库自有代码统一采用 LGPLv3，第三方组件遵循各自许可证。**

本仓库自有代码与材料采用 **GNU Lesser General Public License 第 3 版
（LGPLv3）**，SPDX 标识为 `LGPL-3.0-only`。

适用范围包括 `core/`、`model/`、`app/`、`plugins/`、`python/`、`examples/`、
`sdk/`、`cmake/`、`resource/`、测试、文档及根目录构建与配置文件；下述第三方材料除外。

LGPLv3 原文见 `LGPLv3-LICENSE.txt`。LGPLv3 引用了 GPLv3 条款并附加额外许可，
因此同时附带 `GPLv3-LICENSE.txt`；附带 GPLv3 原文不表示本仓库改用 GPLv3。

LGPLv3 允许应用程序在满足其条件时，以其它条款（包括闭源条款）使用和链接库。
分发时须保留必要声明，按要求提供受许可覆盖的源码，并满足适用的库替换、重新链接
及安装信息要求。对 LGPL 代码的修改在分发时仍受 LGPLv3 约束。
此处为摘要，不能替代许可证原文。

## 第三方组件 —— 遵循各自许可证

第三方源码、库、资源及其它材料保留原版权声明和许可证。本仓库的 LGPLv3 声明
不会重新许可这些组件，也不会覆盖其原条款。例如，`model/ops/tiny_obj_loader.h`
保留文件内的 MIT 许可。

Qt、OpenCASCADE、VTK、libMeshb 等依赖，以实际使用版本附带的许可证为准。
分发者须保留其要求的声明和许可原文，并履行各自的源码提供等义务。

ZenithGridAddons 等外部插件工程不因本声明而改变许可证。分发主程序与外部插件
的组合时，须评估并遵守该组合适用的许可；本声明不豁免 GPL 或 AGPL 义务。

## 贡献与依赖规则

新增自有代码贡献须采用 LGPLv3，贡献者须具备相应授权权利。引入第三方代码须保留
原声明，新增依赖须审查，确保保持本仓库的 LGPLv3 许可模式。不得引入会要求
本仓库本体按 GPL 或 AGPL 分发的代码或依赖。

本声明不覆盖第三方权利，也不追溯改变此前发行版本的许可。

完整条款：

- [GNU Lesser General Public License v3.0](https://www.gnu.org/licenses/lgpl-3.0.html)
- [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html)
