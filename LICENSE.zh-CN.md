# License Information（中文）

> 本文件为中文翻译版本，仅供参考。
> **正式许可条款以英文版 `LICENSE.md` 及仓库中的 `LGPLv3-LICENSE.txt` /
> `AGPLv3-LICENSE.txt` 原文为准。**
> 若中英文表述存在歧义，以英文版本为准。

本仓库采用**双许可证**结构，请按目录判断适用许可证。

## Section 1: LGPLv3 —— 核心库与其构建辅助脚本

以下目录及其子目录使用 **GNU Lesser General Public License, version 3
(LGPLv3)**：

- `core/`
- `model/`
- `cmake/`（辅助定位依赖的 CMake 脚本，同时服务于 LGPL 与 AGPL 层）

许可证全文见本仓库的 `LGPLv3-LICENSE.txt`。

LGPLv3 允许第三方在**不修改依赖源码**的前提下，把这些目录的产物作为库链接
到闭源软件中；`cmake/` 中的 Find 模块也可被下游 LGPL 使用者直接复用。
请勿把非 LGPL 兼容代码引入这些目录。

## Section 2: AGPLv3 —— 应用、插件与其它

除 Section 1 覆盖的目录以外的所有代码使用 **GNU Affero General Public
License, version 3 (AGPLv3)**，包括但不限于：

- `app/`
- `plugins/`
- `test/`
- `resource/`（应用品牌资源与 Windows 资源脚本，随 `app/` 一同分发）
- 仓库根目录下的构建脚本、配置与文档
- `...`

许可证全文见本仓库的 `AGPLv3-LICENSE.txt`。

之所以整体应用/插件层选用 AGPLv3，是因为插件依赖链中可能包含以 AGPLv3
许可发布的第三方库。

## 许可证边界规则

- **`core/`、`model/`、`cmake/`（LGPLv3）**
  - `core/` / `model/` 单独作为库链接使用时按 LGPLv3 处理，可被闭源软件动态链接。
  - `cmake/` 仅在构建期使用，不会被链接进最终二进制；采用 LGPLv3 便于下游
    LGPL 使用者直接复用其中的 Find 模块。
  - 允许被 AGPLv3 的 `app/` / `plugins/` 代码调用；LGPLv3 与 AGPLv3 兼容。
  - 禁止在这些目录中引入 GPL / AGPL / 或与 LGPLv3 不兼容的代码，
    否则会破坏"闭源二次开发"能力。
- **`app/`、`plugins/`、`resource/` 及其余目录（AGPLv3）**
  - 修改或分发这些代码必须遵守 AGPLv3，包括第 13 条"通过网络提供服务时
    必须提供对应源代码"。
  - 若发行版打包了任何 AGPLv3 组件，
    则该发行版整体按 AGPLv3 分发义务处理。
- **单独发布仅使用 `core/` + `model/` + `cmake/` 的衍生作品**
  - 只要没有链接 AGPLv3 组件，可以按 LGPLv3 单独发布，不受 AGPL 传染。
- **新增依赖**
  - 加入 `core/` / `model/` / `cmake/`：许可证必须与 **LGPLv3** 兼容。
  - 加入 `app/` / `plugins/` / `resource/` 等 AGPL 区域：许可证必须与
    **AGPLv3** 兼容。

关于许可证的详细信息，请访问：
- [GNU Lesser General Public License v3.0](https://www.gnu.org/licenses/lgpl-3.0.html)
- [GNU Affero General Public License v3.0](https://www.gnu.org/licenses/agpl-3.0.html)