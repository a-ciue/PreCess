<!-- Improved compatibility of back to top link: See: https://github.com/othneildrew/Best-README-Template/pull/73 -->
<a id="readme-top"></a>
<!--
*** Thanks for checking out the Best-README-Template. If you have a suggestion
*** that would make this better, please fork the repo and create a pull request
*** or simply open an issue with the tag "enhancement".
*** Don't forget to give the project a star!
*** Thanks again! Now go create something AMAZING! :D
-->



<!-- PROJECT SHIELDS -->
<!--
*** I'm using markdown "reference style" links for readability.
*** Reference links are enclosed in brackets [ ] instead of parentheses ( ).
*** See the bottom of this document for the declaration of the reference variables
*** for contributors-url, forks-url, etc. This is an optional, concise syntax you may use.
*** https://www.markdownguide.org/basic-syntax/#reference-style-links
-->
[![Forks][forks-shield]][forks-url]
[![Gitee Stargazers][stars-shield]][stars-url]
[![Github Stargazers][github-stars-shield]][github-stars-url]
[![Issues][issues-shield]][issues-url]
[![Contributors][contributors-shield]][contributors-url]
[![project_license][license-shield]][license-url]
<!-- [![LinkedIn][linkedin-shield]][linkedin-url] -->

<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://gitee.com/precess/PreCess">
    <img src="https://foruda.gitee.com/images/1754736381119453114/37538937_9363227.png" alt="Logo" width="80" height="80">
  </a>

  <h3 align="center">前蔚处理 PreCess</h3>

  <p align="center">
    专注网格处理的CAE前处理软件，面向网格算法开发者与工业界实际网格处理业务需求。
    <br />
    <a href="https://gitee.com/precess/PreCess/wikis/Home"><strong>项目文档 »</strong></a>
    <br />
    <br />
    <a href="https://gitee.com/precess/PreCess/releases">发行版</a>
    &middot;
    <a href="https://gitee.com/precess/PreCess/issues/new?template=1-bug%E6%8A%A5%E5%91%8A.yml">Bug 报告</a>
    &middot;
    <a href="https://gitee.com/precess/PreCess/issues/new?template=3-%E5%8A%9F%E8%83%BD%E5%BB%BA%E8%AE%AE.yml">功能反馈</a>
  </p>
</div>



<!-- TABLE OF CONTENTS -->
<details>
  <summary>展开目录</summary>
  <ol>
    <li>
      <a href="#关于项目">关于项目</a>
      <ul>
        <li><a href="#项目技术">项目技术</a></li>
      </ul>
    </li>
    <li>
      <a href="#构建项目">构建项目</a>
      <ul>
        <li><a href="#准备">准备</a></li>
        <li>
            <a href="#构建">构建</a>
            <ul>
                <li><a href="#Windows用户">Windows用户</a></li>
            </ul>
        </li>
      </ul>
    </li>
    <li><a href="#功能用法">功能用法</a></li>
    <li><a href="#路线图">路线图</a></li>
    <li><a href="#参与项目">参与项目</a></li>
    <li><a href="#许可证">许可证</a></li>
    <li><a href="#联系方式">联系方式</a></li>
    <li><a href="#鸣谢">鸣谢</a></li>
  </ol>
</details>

<!-- ABOUT THE PROJECT -->
## 🔭关于项目

[![Product Name Screen Shot][product-screenshot]](https://geohubdut.netlify.app/products/item-a765/)

**"PreCess"** 取自 **Pre-Process** 前处理的英文名。**“前蔚处理”** 取自 **前处理**。

PreCess 是专注网格处理的 CAE 前处理开源软件，面向网格算法开发者与工业界需求。提供可视化交互框架，用户可在渲染窗口中直接拾取点、线、面、体等网格元素作为算法输入，降低验证调试成本。应用于网格算法研发、工业 CAE 前处理及游戏资产处理，致力成为 CAE 前处理开源生态底座。

本项目使用**插件化架构**，将功能都封装在插件中。插件按 IO、算法、编辑、功能（`FeatureSystem` 事件驱动，可声明参数、菜单、按键绑定与视口交互能力）四类系统注册，由主程序运行时按需加载。**核心库** `core/` 与 `model/` 采用 **LGPLv3** 许可，允许在不修改依赖源码前提下被闭源软件链接使用；**应用与插件层**（`app/`、`plugins/` 等）采用 **AGPLv3** 许可。

本项目使用CMake构建，力争做到**跨平台**开发，目标兼容Windows/Linux/MacOS系统。使用前沿开发技术，力争成为工业软件CAE前处理领域前沿的开源软件项目，成为工业软件CAE前处理领域**最好的网格处理开源项目**。

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### 🤔我们能做什么？

长久以来网格算法开发存在**痛点问题**：
1. 对算法开发者：网格算法需要可视化**输出**结果时，需要打包若干个可视化工具用于做结果的后处理。这些交互输入和后处理逻辑可以被集成到同一个软件中。
2. 对工业界：网格算法开发离工业界实际应用需求较远，我们可以提供一个**集成网格算法的平台**，接触工业界的网格处理业务需求来迭代算法。

我们是大连理工大学软件学院网格组的学生实践团队，致力于**网格算法研发与工业应用**。我们可以为**网格算法开发者**提供一个可视化交互框架，降低验证调试成本；

对于**工业界**处理网格的业务需求，我们可以提供集成网格算法的平台。一方面凭借许可证的灵活性可以集成各种开源网格算法处理技术；另一方面背靠大工软院网格团队的优秀网格算法基础，做商业定制化网格软件开发。

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### 🛠️项目技术

* [![C++][C++]][C++-url]
* [![CMake][CMake]][CMake-url]
* [![Qt][Qt]][Qt-url]
* [![VTK][VTK]][VTK-url]
* [![OCC][OCC]][OCC-url]
* [![spdlog][spdlog]][spdlog-url]
* [![Catch2][Catch2]][Catch2-url]
* [![KDDockWidgets][KDDockWidgets]][KDDockWidgets-url]

**插件层功能依赖**（由 `PreCess-deps.bat` 获取，随对应插件按需构建）：

* [![CGAL][CGAL]][CGAL-url]：插件共用网格转换层 `plugins/shared/cgal_support` 与网格修复插件（CGAL PMP）
* [![Gmsh][Gmsh]][Gmsh-url]：Gmsh 渐进式网格划分插件（`gmsh::shared`）
* [![TetGen][TetGen]][TetGen-url]：TetGen 体网格剖分插件（`tetgen::tetgen`，旧版调用外部 `tetgen` 可执行文件）

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## 🚀快速构建项目

#### Windows用户

1. 在本项目[发行版页面](https://gitee.com/precess/PreCess/releases)，下载带预编译依赖包`PreCess-deps`与项目源码`PreCess`，并解压放置在相同目录`<path>`
2. `Win`+`Q`搜索并打开`x64 Native Tools Command Prompt for VS 2022`
3. 执行命令：
```bash
cd /d "<path>/PreCess"
cmake -S . -B ./build -GNinja "-DCMAKE_PREFIX_PATH=<path>/PreCess-deps" -DCMAKE_INSTALL_PREFIX=./install-rel -DCMAKE_BUILD_TYPE=Release
cmake --build ./build --target install
"./install-rel/bin/PreCess.exe"
```

> 开发提示：仓库自带 `CMakeUserPresets.json.example` 预设模板用来便捷配置Windows VS开发环境，根据需要复制并重命名为 `CMakeUserPresets.json`，然后修改其中的路径和环境变量。

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- USAGE EXAMPLES -->
## ⚙️功能用法

- **模型导入与展示**：支持 IGES、STEP、OBJ、PLY、`.m`、Medit `.mesh`、Abaqus `.inp`、VTK `.vtk` 等格式导入与展示，支持多文件同时导入；几何与网格分层组织、网格/几何渲染风格切换（如「网格·面·带网格线」「网格·线·带内部线」等）、体网格切面裁剪。
- **交互式选择与算法调用**：组件/点/边/面/体/几何点线面体选择器，网格面支持按角度扩散多选；对选择结果调用网格算法插件（Gmsh 渐进式划分、TetGen/TetGenLib 体网格剖分、CCGMeshToNURBS、执行 cmd 命令等）。
- **几何处理**：几何基本形状创建、几何删除。
- **网格编辑**：创建面、删除面等编辑插件。
- **测量与尺寸标注**：视口交互测量与参数化尺寸标注（距离/角度/半径/长度/面积/体积/包围盒/重心）。
- **网格质量分析**：计算所选组件的网格质量并生成标量属性，支持属性渲染与颜色表。
- **网格修复**：基于 CGAL PMP 的孔洞三角化填补、自相交面检测、退化面清理。
- **插件开发与集成**：功能皆插件，按 IO/算法/编辑/功能（FeatureSystem 事件驱动）四类系统注册；功能可声明参数、菜单、按键绑定与视口交互能力，示例见 `plugins/` 目录。

_For more examples, please refer to the [Documentation](https://gitee.com/precess/PreCess/wikis/Home)_

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ROADMAP -->
## 🗺️路线图

需要完成的**特性、功能**，即**迭代计划**：

> 未加粗的目标仍处于计划预览状态。

* [ ] **CI支持**：
  * [x] 完成项目的测试框架：对每个功能类都要编写单元测试
  * [ ] 配置远程托管平台的自动测试与检查
    * [x] Doxygen 文档自动生成、自动构建与单元测试 CI
    * [ ] 代码静态检查 CI
* [x] **网格类型支持**：做到能导入并可视化三角形网格、四边形网格、更广义的多边形网格、体网格
* [x] **渲染窗口的基础交互支持**：支持各种网格元素的拾取：点、边、面、体及几何元素，网格面支持按角度扩散多选
  * [x] 体网格的切面支持，实现切平面裁剪网格的可视化效果
  * [ ] 更便捷的切面可视化与可视化，效果参考[HexaLab](https://www.hexalab.net/)
* [ ] **辅助数据结构系统**：一些网格算法需要依赖某种特定的数据结构如CTMesh等，需要一个系统用于注册插件给定的数据结构类型、拿模型数据构造并存储于内存中，以免每次执行算法对数据结构反复构造
  * [ ] **数据层存储附加数据支持**：可以存储**模型核心数据**外的附加数据，如顶点id、uv纹理坐标等，也可用于存储**辅助数据结构**。**模型核心数据**指模型点的坐标与面构成等模型的核心数据
* [ ] **现代化UI开发**：
  * [x] 可停靠的窗口：引入KDDockWidgets
  * [x] 拖拽导入模型文件：支持从文件管理器一次拖入一个或多个文件导入
  * [ ] 优化焦点管理，及时失焦。
    * [ ] 窗口焦点管理：能够选中某个窗口
  * [ ] 右键菜单管理
    * [x] 渲染窗口、对象树右键菜单（显隐等操作）
    * [ ] 通用右键菜单注册管理
  * [ ] 键盘快捷键管理
    * [x] 功能插件按键绑定（FeatureSystem KeyEvent）
    * [ ] 全局快捷键管理
  * [ ] 软件选项设置管理
    * [x] 插件注册/注销管理
    * [ ] 偏好设置窗口：主题设置
  * [ ] Python解释器对软件的控制：参考HyperMesh等，激进点参考Blender
  * [ ] 菜单栏视图或窗口header按钮或窗口header右键菜单可以管理窗口状态：开关、选项页等。参考HyperMesh、Ansys，激进点的参考Visual Studio与Blender
  * [ ] 类Blender的自定义窗口布局预设
* [ ] 任务执行  
  * [ ] 任务的失败恢复与撤销重做
    * [x] 模型层快照/恢复原语（组件/模型级 takeSnapshot/restoreSnapshot）
    * [ ] 撤销重做
  * [ ] 任务暂停或断点：算法执行过程中的调试。一般都是每次执行输出一个结果到文件再打开模型查看情况，也许可以做到更精细的算法控制
  * [ ] UI的更新机制：数据更新还挺困难。比如数据更新可能会导致当前操作不合法，需要考虑避免
* [ ] 从软件中剥离业务逻辑：这服务于以下几点
  * [x] C++类库：模型层已抽象为 `core/`、`model/` 静态库
  * [ ] 对模型层做Python Wrapper接口层，可以像Blender将模型层打包成wheel与UI解释器调用接口
  * [ ] 无头软件：软件不是必须得要一个UI吧！命令行也是一种调用接口方法
  * [ ] C/S架构：模型层打包成服务器上的服务，类ParaView
* [ ] 插件管理
  * [x] 插件管理界面：偏好设置中提供插件注册/注销管理
  * [ ] 完善动态插件机制：ABI稳定过于抽象，在用户的插件使用过程中逐渐完善
  * [ ] 插件化的还不够彻底，目前只是实现了业务逻辑的插件化，没有考虑渲染或者UI的插件化
  * [ ] 静态插件机制设计：使用constexpr静态注册，元数据在插件编译时嵌入。尽量兼容动态插件接口
* [ ] 多任务管理
  * [ ] 算法不阻塞UI，区分工作线程与界面线程
  * [ ] 任务资源占用识别与任务的多线程调度：模型A正在运行某个拓扑操作，此时不能同时对A进行另一个拓扑操作。并行的资源占用、任务暂停。考虑使用std::execution实现UE5的任务图的可行性。
* [ ] 算法图：可以以蓝图的形式连接各个算法的输入输出，并予以执行。参考UE5的蓝图系统与Blender的几何节点
* [ ] 动画系统：一些算法的执行过程就是完美的动画
* [ ] 操作结果预览：Blender在进行某些网格操作如挤出收口等操作时，有预览效果可以看得到。但这要UI与算法功能强相关，功能又在dll插件里，跨层操作难度大。参考Blender

可以查看开启的或进行中的 [Issues](https://gitee.com/precess/PreCess/issues) 或 [里程碑](https://gitee.com/precess/PreCess/milestones) 来了解每个任务目前的具体进度。也可以**开启新的Issue**描述遇到的bug、建议或新的项目需求。

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTRIBUTING -->
## 🤝参与项目

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

由衷感谢任何你尝试对本项目做出的贡献！无论是贡献代码还是为项目反馈意见建议，甚至只是反馈修复拼写错误等小问题都**十分感谢**！更欢迎您参与项目例会与项目系列课程活动、尝试调试代码解决问题。

> **接口稳定性提示**：项目处于**预览阶段**（当前 0.x 版本），`core/`、`model/` 库与插件接口**随时可能发生二进制级或源代码级的不兼容变更**，不提供稳定性承诺；不同**编译器/编译器版本或构建配置**也可能导致二进制不兼容。插件源码**随主仓库同步构建**是规避该问题最稳妥的方式；独立构建时只要与主程序使用**兼容的编译器、构建配置与依赖库版本**，同样可以保持二进制兼容。无论哪种方式，请勿依赖旧 ABI。

在此从简单开始列举可以为项目的**贡献形式**：

1. 给项目一个**Star**🌟！
2. **新建[Issue](https://gitee.com/precess/PreCess/issues)**：反馈项目bug、功能改进建议、新功能建议。写Issue时按照给定模板进行填写，并标注对应的tag标签。如，
   * 项目Bug：反馈程序运行过程中遇到的bug，填写Issue时附带程序的输出记录。最好能做到稳定复现Bug，并附带Bug的复现操作流程。标注**bug标签**
   * 改进建议：标注enhancement标签
   * 新功能：标注feature标签
3. **发起Pull Request**：尝试修改完善项目文档、或解决实现Issue。
   1. Fork本仓库
   2. 创建你的Feature分支 (`git checkout -b feature/AmazingFeature`)
   3. Commit你做的更改 (`git commit -m 'Add some AmazingFeature'`)
   4. 推送到个人的Fork仓库 (`git push origin feature/AmazingFeature`)
   5. 开启一个Pull Request请求。如果解决了某个Issue填写对应的Issue

在AI时代理论上**每个人都可以**修改项目代码并实现功能，如果缺少指导与项目结构理解可以[参与交流](#联系方式)。**贡献代码**难度由简单排序：

1. 修改完善项目**[Wiki文档](https://gitee.com/precess/PreCess/wikis)**、根据[注释要求](https://gitee.com/precess/PreCess/wikis/%E4%BB%A3%E7%A0%81%E6%8F%90%E4%BA%A4%E8%A7%84%E8%8C%83#%E6%B3%A8%E9%87%8A)完善程序**注释**
2. 为项目补充缺失的单元**测试**等测试代码
3. 发现并调试修复程序中的**bug**
4. 基于现有架构做**二次开发**，开发软件拓展插件，集成网格算法等功能
5. 开发软件的**UI框架与渲染窗口**：UI框架、渲染窗口交互
6. 开发**软件核心部分**代码：按需开发或完善系统、修改调整模型核心数据结构

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- LICENSE -->
## ⚖️许可证

本项目对**不同目录代码**分别采用 `LGPLv3` 与 `AGPLv3` 双许可证：

- **`core/`、`model/`、`cmake/`（LGPLv3）**：核心库层与其构建辅助脚本。
  允许在**不修改依赖源码**的前提下被闭源软件链接使用，便于二次开发；
  `cmake/` 中的 Find 模块可被下游 LGPL 使用者直接复用。
- **`app/`、`plugins/`、`resource/` 及其余目录（AGPLv3）**：应用、插件与
  应用品牌资源。由于插件依赖链中可能包含以 AGPLv3 发布的第三方库，
  此部分统一采用 AGPLv3；分发或通过网络对外提供服务时须遵守 AGPLv3
  全部条款（含第 13 条网络分发义务）。

LGPLv3 与 AGPLv3 兼容，因此 `app/` / `plugins/` 可以正常调用 `core/` /
`model/`；只是任何发行版一旦打包了 AGPLv3 组件，整体分发义务按 AGPLv3
处理。

许可证边界与义务详情参见 `LICENSE.md`（正式版本，英文）、
`LGPLv3-LICENSE.txt` 与 `AGPLv3-LICENSE.txt`。
中文翻译版本见 `LICENSE.zh-CN.md`（仅供参考，以英文版本为准）。

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTACT -->
## 📞联系方式

<div align="center">
    <img src="https://foruda.gitee.com/images/1754749758923219333/0f73d9d8_9363227.png" alt="Logo" width="300">
</div>

仓库链接: [https://gitee.com/precess/PreCess](https://gitee.com/precess/PreCess)

github镜像：[https://github.com/a-ciue/PreCess](https://github.com/a-ciue/PreCess)

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ACKNOWLEDGMENTS -->
## 🙏鸣谢

* [大连理工大学软件学院先进工业软件与软件工程研究所](https://ss.dlut.edu.cn/index.htm)
* 大连理工大学特色化示范性软件学院大型工业软件特色班

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/endpoint?url=https%3A%2F%2Fgitee-badge.vercel.app%2Fjson%2Fcontributors%2Fprecess%2FPreCess
[contributors-url]: https://gitee.com/precess/PreCess/graphs/contributors
[forks-shield]: https://gitee.com/precess/PreCess/badge/fork.svg?theme=dark
[forks-url]: https://gitee.com/precess/PreCess/network/members
[stars-shield]: https://gitee.com/precess/PreCess/badge/star.svg?theme=dark
[stars-url]: https://gitee.com/precess/PreCess/stargazers
[github-stars-shield]: https://img.shields.io/github/stars/a-ciue/PreCess?style=social
[github-stars-url]: https://github.com/a-ciue/PreCess/stargazers
[issues-shield]: https://svg.hamm.cn/gitee.svg?user=precess&project=PreCess&type=issue
[issues-url]: https://gitee.com/precess/PreCess/issues
[license-shield]: https://img.shields.io/badge/license-LGPLv3%2FAGPLv3-blue.svg
[license-url]: https://gitee.com/precess/PreCess/blob/master/LICENSE.md
[linkedin-shield]: https://img.shields.io/badge/-111-black.svg?colorB=555
[linkedin-url]: https://linkedin.com/in/linkedin_username
[product-screenshot]: resource/PreCess_letter.png
[C++]: https://img.shields.io/badge/C++%2017-000000?style=for-the-badge&logo=cplusplus&logoColor=white
[C++-url]: https://cppreference.com/
[CMake]: https://img.shields.io/badge/CMake-000000?style=for-the-badge&logo=cmake&logoColor=white
[CMake-url]: https://cmake.org/
[Qt]: https://img.shields.io/badge/Qt%20Quick%206-000000?style=for-the-badge&logo=qt&logoColor=white
[Qt-url]: https://qt.io/
[VTK]: https://img.shields.io/badge/VTK-000000?style=for-the-badge&logo=vtk&logoColor=white
[VTK-url]: https://vtk.org/
[OCC]: https://img.shields.io/badge/OpenCASCADE-000000?style=for-the-badge&logo=occ&logoColor=white
[OCC-url]: https://dev.opencascade.org/
[spdlog]: https://img.shields.io/badge/spdlog-000000?style=for-the-badge&logo=spdlog&logoColor=white
[spdlog-url]: https://github.com/gabime/spdlog
[Catch2]: https://img.shields.io/badge/Catch2-000000?style=for-the-badge&logo=catch2&logoColor=white
[Catch2-url]: https://github.com/catchorg/Catch2
[KDDockWidgets]: https://img.shields.io/badge/KDDockWidgets-000000?style=for-the-badge&logo=kddockwidgets&logoColor=white
[KDDockWidgets-url]: https://github.com/KDAB/KDDockWidgets
[CGAL]: https://img.shields.io/badge/CGAL-000000?style=for-the-badge&logo=cgal&logoColor=white
[CGAL-url]: https://www.cgal.org/
[Gmsh]: https://img.shields.io/badge/Gmsh-000000?style=for-the-badge&logo=gmsh&logoColor=white
[Gmsh-url]: https://gmsh.info/
[TetGen]: https://img.shields.io/badge/TetGen-000000?style=for-the-badge&logo=tetgen&logoColor=white
[TetGen-url]: https://wias-berlin.de/software/tetgen/