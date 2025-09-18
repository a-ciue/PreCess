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
[![Stargazers][stars-shield]][stars-url]
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
    <a href=><strong>Explore the docs »</strong></a>
    <br />
    <br />
    <a href=>View Demo</a>
    &middot;
    <a href=>Report Bug</a>
    &middot;
    <a href=>Request Feature</a>
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
## 关于项目

[![Product Name Screen Shot][product-screenshot]](https://geohubdut.netlify.app/products/item-a765/)

本项目使用**前沿版本的开发技术**，力争成为工业软件CAE前处理领域前沿的开源软件项目，成为工业软件CAE前处理领域**最好的网格处理开源项目**。同开源社区学习力争成为现代C++的**前沿工程实践**。

**"PreCess"** 取自 **Pre-Process** 前处理的英文名。**“前蔚处理”** 取自 **前处理**。

本项目使用CMake构建，力争做到**跨平台**开发。软件目标兼容Windows/Linux/MacOS系统。

本项目面向**网格算法开发者**与**工业界**对前处理的处理需求。

本项目使用**插件化架构**，将功能都封装在插件中。为了方便基于软件做**二次开发**，项目中的插件依赖使用LGPLv3许可，允许**不修改依赖源码前提下闭源**。

**在开源项目早期参与项目，是技术历练与学习实践，是为世界开源社区贡献，也是一场以身入局的人生投资。**

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### 我们能做什么？

长久以来网格算法开发存在**痛点问题**：

每个网格算法需要用户给予**输入**时，要么想办法找到要操作的位置的id并作为输入参数，要么自己写一个图形交互界面并建立点击反馈

* 一方面以id作为算法输入，需要使用其他工具辅助判断元素的具体id
* 另一方面，自行编写的图形界面，输入越复杂则需要在开发交互程序的投入就越高

网格算法需要可视化**输出**结果时，需要打包若干个可视化工具用于做结果的后处理。这些后处理程序可以被集成到同一个软件中。

调试网格算法的**执行过程**，通常选择**每次迭代**弹出可视化窗口，或者是输出**中间结果文件**事后可视化。我们也许可以提供一个算法可视化框架去管理中间结果，中间过程能在软件中调试，让调试过程更友好。

对于**工业界**处理网格的业务需求，我们可以通过提供集成网格算法的平台，背靠大工工业软件实验室的优秀网格算法基础，做定制化网格软件开发。

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### 项目技术

* [![C++][C++]][C++-url]
* [![CMake][CMake]][CMake-url]
* [![Qt][Qt]][Qt-url]
* [![VTK][VTK]][VTK-url]
* [![OCC][OCC]][OCC-url]

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## 构建项目

### 准备

* 下载[CMake安装包](https://cmake.org/download/)并安装，勾选把CMake添加到PATH环境变量
* **Windows**用户需要安装[Visual Studio并选择安装C++桌面开发组件](https://visualstudio.microsoft.com/zh-hans/vs/features/cplusplus/)

### 构建

#### Windows用户 （使用vcpkg的导出依赖包）

1. 在本项目[发行版页面](https://gitee.com/precess/PreCess/releases)，下载带预编译依赖包vcpkg-export与项目源码PreCess，确保文件目录结构为两文件夹并列：
```
- vcpkg-export
    - installed
    - scripts
    - .vcpkg.root
    - vcpkg.exe
- PreCess（项目源码目录）
```
2. `Win`+`Q`搜索并打开`x64 Native Tools Command Prompt for VS 2022`
3. 假设该目录为`<path>`，执行命令：
```pwsh
cd /d <path>/PreCess
cmake . --preset vcpkg-export
cd out/build/vcpkg-export
cmake --build .
cmake --install .
cd ../../install/vcpkg-export/bin
PreCess.exe
```

#### Windows用户（不使用vcpkg）

**前提**：需要提前准备所需依赖库：qt、vtk、occ，暂且略过

1. 在本项目[发行版页面](https://gitee.com/precess/PreCess/releases)，下载带预编译依赖库的软件源码并解压
2. `Win`+`Q`搜索并打开`x64 Native Tools Command Prompt for VS 2022`
3. 假设解压目录为`<path>`，
```pwsh
cd <path>/PreCess
cmake -S . -B build -DCMAKE_PREFIX_PATH="<dependent_package_directories>"
cd build
cmake --build .
cmake --install . 
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- USAGE EXAMPLES -->
## 功能用法

- 导入并展示模型
- 交互式算法调用：选择面、边、块
- 插件开发与集成：功能皆插件。IO插件、算法插件

_For more examples, please refer to the [Documentation]()_

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ROADMAP -->
## 路线图

需要完成的**特性、功能**，即**迭代计划**：

> 未加粗的目标仍处于计划预览状态。

* [ ] **CI支持**：
  * [ ] 完成项目的测试框架：对每个功能类都要编写单元测试
  * [ ] 配置远程托管平台的自动测试与检查
* [ ] **网格类型支持**：修改数据层网格核心数据结构与渲染窗口代码，做到能导入并可视化。目前软件只能表示三角形网格，要能表示其他面网格如
  * [ ] 四边形网格、更广义的多面体网格
  * [ ] 添加对体网格的支持
* [ ] **渲染窗口的基础交互支持**：主要是支持各种网格元素的拾取
  * [ ] 面网格的点线面
  * [ ] 体网格的点线面体
  * [ ] 体网格的切面支持，效果参考[HexaLab](https://www.hexalab.net/)暂时只需要实现Filters的Slice中的类切平面效果
* [ ] **辅助数据结构系统**：一些网格算法需要依赖某种特定的数据结构如CTMesh等，需要一个系统用于注册插件给定的数据结构类型、拿模型数据构造并存储于内存中，以免每次执行算法对数据结构反复构造
  * [ ] **数据层存储附加数据支持**：可以存储**模型核心数据**外的附加数据，如顶点id、uv纹理坐标等，也可用于存储**辅助数据结构**。**模型核心数据**指模型点的坐标与面构成等模型的核心数据
* [ ] **现代化UI开发**：
  * [ ] 可停靠的窗口：引入KDDockWidgets
  * [ ] 优化焦点管理，及时失焦。
    * [ ] 窗口焦点管理：能够选中某个窗口
  * [ ] 右键菜单管理
  * [ ] 键盘快捷键管理
  * [ ] 软件选项设置管理
  * [ ] Python解释器对软件的控制：参考HyperMesh等，激进点参考Blender
  * [ ] 菜单栏视图或窗口header按钮或窗口header右键菜单可以管理窗口状态：开关、选项页等。参考HyperMesh、Ansys，激进点的参考Visual Studio与Blender
  * [ ] 类Blender的自定义窗口布局预设
* [ ] 任务执行  
  * [ ] 任务的失败恢复与撤销重做
  * [ ] 任务暂停或断点：算法执行过程中的调试。一般都是每次执行输出一个结果到文件再打开模型查看情况，也许可以做到更精细的算法控制
  * [ ] UI的更新机制：数据更新还挺困难。比如数据更新可能会导致当前操作不合法，需要考虑避免
* [ ] 从软件中剥离业务逻辑：这服务于以下几点
  * [ ] C++类库：模型层抽象成库是最基础的形态
  * [ ] 对模型层做Python Wrapper接口层，可以像Blender将模型层打包成wheel与UI解释器调用接口
  * [ ] 无头软件：软件不是必须得要一个UI吧！命令行也是一种调用接口方法
  * [ ] C/S架构：模型层打包成服务器上的服务，类ParaView
* [ ] 插件管理
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
## 参与项目

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

由衷感谢任何你尝试对本项目做出的贡献！无论是贡献代码还是为项目反馈意见建议，甚至只是反馈修复拼写错误等小问题都**十分感谢**！更欢迎您参与项目例会与项目系列课程活动、尝试调试代码解决问题。

在此从简单开始列举可以为项目的**贡献形式**：

1. 给项目一个**Star**⭐！
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

理论上**每个人都可以**修改项目代码并实现功能，如果缺少指导与项目结构理解可以[参与交流](#联系方式)。**贡献代码**难度由简单排序：

1. 修改完善项目**文档**、根据[注释要求](https://gitee.com/precess/PreCess/wikis/%E4%BB%A3%E7%A0%81%E6%8F%90%E4%BA%A4%E8%A7%84%E8%8C%83#%E6%B3%A8%E9%87%8A)完善程序**注释**
2. 为项目补充缺失的单元**测试**等测试代码
3. 发现并调试修复程序中的**bug**
4. 基于现有架构做**二次开发**，开发软件拓展插件，集成网格算法等功能
5. 开发软件的**UI框架与渲染窗口**：UI框架、渲染窗口交互
6. 开发**软件核心部分**代码：按需开发或完善系统、修改调整模型核心数据结构

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- LICENSE -->
## 许可证

本项目对**不同部分代码**分别使用了`GPLv3`与`LGPLv3`许可证。

为了允许插件作者开发**插件闭源**与**从中获利**，插件开发所依赖的代码文件使用`LGPLv3`许可。剩余代码使用`GPLv3`许可，禁止将本软件集成到其他**闭源软件**中。如果想商用GPL部分代码可以选择咨询使用**商用协议**。

许可证详情参见`LICENSE.md`。

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTACT -->
## 联系方式

<div align="center">
    <img src="https://foruda.gitee.com/images/1754749758923219333/0f73d9d8_9363227.png" alt="Logo" width="300">
</div>

仓库链接: [https://gitee.com/precess/PreCess](https://gitee.com/precess/PreCess)

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ACKNOWLEDGMENTS -->
## 鸣谢

* [GeoHub-DUT](https://geohubdut.netlify.app/)
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
[issues-shield]: https://svg.hamm.cn/gitee.svg?user=precess&project=PreCess&type=issue
[issues-url]: https://gitee.com/precess/PreCess/issues
[license-shield]: https://svg.hamm.cn/gitee.svg?user=precess&project=PreCess&type=license
[license-url]: https://gitee.com/precess/PreCess/blob/master/LICENSE.txt
[linkedin-shield]: https://img.shields.io/badge/-111-black.svg?colorB=555
[linkedin-url]: https://linkedin.com/in/linkedin_username
[product-screenshot]: resource/PreCess_letter.png
[C++]: https://img.shields.io/badge/C++%2017%2F20-000000?style=for-the-badge&logo=cplusplus&logoColor=white
[C++-url]: https://cppreference.com/
[CMake]: https://img.shields.io/badge/CMake-000000?style=for-the-badge&logo=cmake&logoColor=white
[CMake-url]: https://cmake.org/
[Qt]: https://img.shields.io/badge/Qt%20Quick%206-000000?style=for-the-badge&logo=qt&logoColor=white
[Qt-url]: https://qt.io/
[VTK]: https://img.shields.io/badge/VTK-000000?style=for-the-badge&logo=vtk&logoColor=white
[VTK-url]: https://vtk.org/
[OCC]: https://img.shields.io/badge/OpenCASCADE-000000?style=for-the-badge&logo=occ&logoColor=white
[OCC-url]: https://dev.opencascade.org/
