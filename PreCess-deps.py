#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import os
import platform
import shlex
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

@dataclass(frozen=True)
class Platform:
    """构建平台：system 与 arch 是两个独立维度的组合。"""

    system: str
    arch: str

    @property
    def name(self) -> str:
        return f"{self.system}_{self.arch}"

    @property
    def is_wasm(self) -> bool:
        return self.system == "wasm"

# 各系统支持的架构：原生平台为处理器架构，wasm 为线程模型（单线程/多线程）。
SUPPORTED_PLATFORMS = {
    "windows": ("x64",),
    "linux": ("x64", "arm64"),
    "macos": ("x64", "arm64"),
    "wasm": ("single", "multiple"),
}
PLATFORM_CHOICES = [
    f"{system}_{arch}"
    for system, architectures in SUPPORTED_PLATFORMS.items()
    for arch in architectures
]

def parse_platform(text: str) -> Platform:
    """把 system_arch 形式的平台名解析为 Platform，组合非法时报错。"""

    system, separator, arch = text.partition("_")
    if separator and arch in SUPPORTED_PLATFORMS.get(system, ()):
        return Platform(system, arch)
    raise argparse.ArgumentTypeError(
        f"未知平台 {text!r}，可选：{'、'.join(PLATFORM_CHOICES)}"
    )

def detect_host_platform() -> Platform:
    """探测当前宿主平台；架构取自运行 Python 的机器，与实际编译工具链一致。"""

    if sys.platform == "win32":
        system = "windows"
    elif sys.platform == "darwin":
        system = "macos"
    else:
        system = "linux"
    arch = "arm64" if platform.machine().lower() in ("arm64", "aarch64") else "x64"
    return Platform(system, arch)

# Gmsh 链接的 OpenCASCADE 工具箱清单（按静态链接依赖顺序排列，TKernel 最后）。
OCC_TOOLKIT_LIBRARIES = [
    "TKDESTEP",
    "TKDEIGES",
    "TKXSBase",
    "TKOffset",
    "TKFeat",
    "TKFillet",
    "TKBool",
    "TKMesh",
    "TKHLR",
    "TKBO",
    "TKPrim",
    "TKShHealing",
    "TKTopAlgo",
    "TKGeomAlgo",
    "TKBRep",
    "TKGeomBase",
    "TKG3d",
    "TKG2d",
    "TKMath",
    "TKernel",
]

# CGAL 头文件依赖的 Boost 库清单（原生与 wasm 共用）。
BOOST_INCLUDE_LIBRARIES = ";".join(
    [
        "algorithm",
        "any",
        "bimap",
        "bind",
        "callable_traits",
        "concept_check",
        "config",
        "container",
        "container_hash",
        "core",
        "dynamic_bitset",
        "foreach",
        "format",
        "function",
        "functional",
        "graph",
        "heap",
        "intrusive",
        "iterator",
        "lexical_cast",
        "logic",
        "math",
        "mpl",
        "multi_array",
        "multi_index",
        "multiprecision",
        "optional",
        "predef",
        "preprocessor",
        "program_options",
        "property_map",
        "ptr_container",
        "random",
        "range",
        "smart_ptr",
        "static_assert",
        "stl_interfaces",
        "tuple",
        "type_traits",
        "unordered",
        "utility",
        "variant",
    ]
)

# 思源黑体（SIL OFL 1.1，允许随程序自由嵌入与再分发）：wasm 构建嵌入的 CJK 界面字体。
SOURCE_HAN_SANS_REF = "2.005R"
WASM_FONT_DOWNLOADS = (
    (
        "https://raw.githubusercontent.com/adobe-fonts/source-han-sans/"
        f"{SOURCE_HAN_SANS_REF}/SubsetOTF/CN/SourceHanSansCN-Regular.otf",
        "SourceHanSansCN-Regular.otf",
    ),
    (
        "https://raw.githubusercontent.com/adobe-fonts/source-han-sans/"
        f"{SOURCE_HAN_SANS_REF}/LICENSE.txt",
        "SourceHanSans-LICENSE.txt",
    ),
)

@dataclass(frozen=True)
class GitRepository:
    """需要克隆的第三方源码仓库。"""

    name: str
    url: str
    ref: str
    destination: str = ""
    detached_commit: str = ""

@dataclass(frozen=True)
class DependenciesSettings:
    """一次依赖引导任务的配置。"""

    dependency_dir: Path
    build_release: bool
    build_relwithdebinfo: bool
    qt_path: Path | None
    platform: Platform
    stage: str
    toolchain_path: Path | None
    host: Platform
    emsdk_path: Path | None

    @property
    def source_dir(self) -> Path:
        return self.dependency_dir / "_source"

    @property
    def install_dir(self) -> Path:
        return self.dependency_dir / self.platform.name

    @property
    def is_cross(self) -> bool:
        return self.platform != self.host

    @property
    def prefix_paths(self) -> str:
        prefixes = [str(self.install_dir)]
        # 交叉编译时 --qt 指向宿主平台 Qt，不能混入目标平台的查找路径。
        if self.qt_path is not None and not self.is_cross:
            prefixes.append(str(self.qt_path))
        return os.pathsep.join(prefixes)

    def build_directory(self, source: Path) -> Path:
        """源码树内的构建目录；交叉平台与宿主构建互不污染。"""

        if self.is_cross:
            return source / f"build-{self.platform.name}"
        return source / "build"

    @property
    def cross_toolchain_path(self) -> Path | None:
        if self.toolchain_path is not None:
            return self.toolchain_path
        if self.platform.is_wasm:
            return (
                self.emsdk_path
                / "upstream"
                / "emscripten"
                / "cmake"
                / "Modules"
                / "Platform"
                / "Emscripten.cmake"
            )
        return None

    @property
    def install_configs(self) -> list[str]:
        configs = []
        if self.build_relwithdebinfo:
            configs.append("RelWithDebInfo")
        configs.append("Debug")
        if self.build_release:
            configs.append("Release")
        return configs


GIT_REPOSITORIES = [
    GitRepository(
        "KDDockWidgets",
        "https://github.com/KDAB/KDDockWidgets.git",
        "v2.4.0",
    ),
    GitRepository(
        "vtk",
        "https://gitlab.kitware.com/vtk/vtk.git",
        "v9.6.2",
    ),
    GitRepository(
        "freetype",
        "https://gitlab.freedesktop.org/freetype/freetype.git",
        "VER-2-14-1",
    ),
    GitRepository(
        "OCCT",
        "https://github.com/Open-Cascade-SAS/OCCT.git",
        "V8_0_0",
    ),
    GitRepository(
        "spdlog",
        "https://github.com/gabime/spdlog.git",
        "v1.16.0",
    ),
    GitRepository(
        "Catch2",
        "https://github.com/catchorg/Catch2.git",
        "v3.11.0",
    ),
    GitRepository(
        "libMeshb",
        "https://github.com/LoicMarechal/libMeshb.git",
        "v7.80",
    ),
    GitRepository(
        "TetGen",
        "https://github.com/TetGen/TetGen.git",
        "v1.6.0",
        "tetgen",
    ),
    GitRepository(
        "gmsh",
        "https://gitlab.onelab.info/gmsh/gmsh.git",
        "master",
        "gmsh-occ8",
        "86596d7902a1b00e23641ac5c904b7c1f880ce9f",
    ),
]

class DependencyError(Exception):
    """依赖引导过程中的可预期错误。"""

def format_command(command: Sequence[object]) -> str:
    return shlex.join(str(item) for item in command)

def run(
    command: Sequence[object],
    cwd: Path | None = None,
    environment: dict[str, str] | None = None,
) -> None:
    command_list = [str(item) for item in command]
    print(f"[命令] {format_command(command_list)}")
    if cwd is not None:
        print(f"[目录] {cwd}")
    subprocess.run(command_list, cwd=cwd, check=True, env=environment)

def write_text_file(path: Path, content: str, newline: str = "\n") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline=newline) as output:
        output.write(content)

def clone_repositories(settings: DependenciesSettings) -> None:
    repositories = list(GIT_REPOSITORIES)
    if settings.qt_path is None:
        repositories.insert(
            0,
            GitRepository(
                "qt5",
                "https://code.qt.io/qt/qt5.git",
                "v6.8.3",
            ),
        )

    for repository in repositories:
        destination_name = repository.destination or repository.name
        destination = settings.source_dir / destination_name
        if destination.exists():
            if not (destination / ".git").exists():
                raise DependencyError(
                    f"源码目录已存在但不是 git 仓库：{destination}；请先手动处理该目录"
                )
            print(f"[跳过] 源码已存在：{destination}")
            continue

        command = [
            "git",
            "clone",
            "--single-branch",
            "--depth",
            "1",
            "--branch",
            repository.ref,
            repository.url,
            destination,
        ]
        # Gmsh 需要先克隆 master 才能取到当前使用的固定开发提交。
        if repository.detached_commit:
            command.remove("--depth")
            command.remove("1")
        run(command)
        if repository.detached_commit:
            run(["git", "checkout", "--detach", repository.detached_commit], destination)

def download_file(url: str, destination: Path) -> None:
    if destination.exists() and destination.stat().st_size > 0:
        print(f"[跳过] 压缩包已存在：{destination}")
        return

    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".part")
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "PreCess-dependency-bootstrap"},
    )
    print(f"[下载] {url}")
    try:
        with urllib.request.urlopen(request, timeout=30) as response, temporary.open("wb") as output:
            total_size = response.getheader("Content-Length")
            total = int(total_size) if total_size and total_size.isdigit() else 0
            downloaded = 0
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                output.write(chunk)
                downloaded += len(chunk)
                if total:
                    percent = downloaded * 100 // total
                    print(f"\r[进度] {percent:3d}% ({downloaded} / {total} 字节)", end="")
                else:
                    print(f"\r[进度] {downloaded} 字节", end="")
        print()
        temporary.replace(destination)
    except Exception:
        print()
        if temporary.exists():
            temporary.unlink()
        raise

def download_archives(settings: DependenciesSettings) -> None:
    downloads = [
        (
            "https://github.com/Open-Cascade-SAS/OCCT/releases/download/V8_0_0/3rdparty-vc14-64.zip",
            settings.source_dir / "OCCT" / "3rdparty-vc14-64-temp.zip",
        ),
        (
            "https://github.com/CGAL/cgal/releases/download/v6.2/CGAL-6.2.zip",
            settings.source_dir / "CGAL-6.2.zip",
        ),
        (
            "https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.tar.xz",
            settings.source_dir / "boost-1.91.0-1-cmake.tar.xz",
        ),
    ]
    for url, destination in downloads:
        download_file(url, destination)

def download_wasm_font(settings: DependenciesSettings) -> None:
    """下载 wasm 嵌入用思源黑体到 <wasm 依赖根>/fonts/。

    app/CMakeLists.txt 默认按该路径查找嵌入字体，并同时分发随附的 OFL 许可证。
    """

    font_dir = settings.install_dir / "fonts"
    for url, file_name in WASM_FONT_DOWNLOADS:
        download_file(url, font_dir / file_name)

def remove_path(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.exists():
        shutil.rmtree(path)

def extract_zip(archive: Path, destination: Path) -> None:
    print(f"[解压] {archive} -> {destination}")
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as zip_file:
        zip_file.extractall(destination)

def extract_boost_archive(archive: Path, destination: Path) -> None:
    """解压 Boost 归档并去掉一层归档目录。"""

    print(f"[解压] {archive} -> {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    excluded_marker = "/libs/decimal/doc/modules/ROOT/examples"
    with tarfile.open(archive, "r:xz") as archive_file:
        members = archive_file.getmembers()
        stripped_members = []
        for member in members:
            parts = Path(member.name).parts
            if len(parts) <= 1:
                continue
            if excluded_marker in "/" + member.name:
                continue
            member.name = str(Path(*parts[1:]))
            if member.linkname:
                link_parts = Path(member.linkname).parts
                if len(link_parts) > 1:
                    member.linkname = str(Path(*link_parts[1:]))
            stripped_members.append(member)
        try:
            archive_file.extractall(destination, members=stripped_members, filter="data")
        except TypeError:
            # Python 3.11 及更早版本没有 data 过滤器。
            archive_file.extractall(destination, members=stripped_members)

def prepare_archives(settings: DependenciesSettings) -> None:
    occt_source = settings.source_dir / "OCCT"
    third_party_dir = occt_source / "3rdparty-vc14-64"
    outer_archive = occt_source / "3rdparty-vc14-64-temp.zip"
    inner_archive = occt_source / "3rdparty-vc14-64.zip"
    if not third_party_dir.exists():
        if not inner_archive.exists():
            extract_zip(outer_archive, occt_source)
        if not inner_archive.exists():
            raise DependencyError("OCCT 第三方压缩包中缺少 3rdparty-vc14-64.zip")
        extract_zip(inner_archive, occt_source)
    else:
        print(f"[跳过] 已解压：{third_party_dir}")

    # CGAL 为纯头文件库，按平台各解压一份，保证 CMAKE_PREFIX_PATH 指向平台目录即可发现。
    cgal_destination = settings.install_dir / "CGAL-6.2"
    cgal_marker = cgal_destination / "CMakeLists.txt"
    if not cgal_marker.exists():
        extract_zip(settings.source_dir / "CGAL-6.2.zip", settings.install_dir)
        for directory_name in ("data", "demo", "examples", "doc_html"):
            remove_path(cgal_destination / directory_name)
    else:
        print(f"[跳过] 已解压：{cgal_destination}")

    boost_source = settings.source_dir / "boost-src"
    if not boost_source.exists():
        extract_boost_archive(
            settings.source_dir / "boost-1.91.0-1-cmake.tar.xz",
            boost_source,
        )
    else:
        print(f"[跳过] 已解压：{boost_source}")

def fetch_sources(settings: DependenciesSettings) -> None:
    settings.dependency_dir.mkdir(parents=True, exist_ok=True)
    settings.source_dir.mkdir(exist_ok=True)
    print(f"[阶段] 拉取依赖源码 -> {settings.source_dir}")
    clone_repositories(settings)
    download_archives(settings)
    if settings.platform.is_wasm:
        download_wasm_font(settings)
    prepare_archives(settings)

def toolchain_arguments(settings: DependenciesSettings) -> list[str]:
    toolchain_path = settings.cross_toolchain_path
    if toolchain_path is None:
        return []
    return [f"-DCMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain_path}"]

def configure_cmake_project(
    source: Path,
    build_directory: Path,
    install_prefix: Path,
    definitions: Iterable[tuple[str, object]],
    settings: DependenciesSettings,
    *,
    cmake_command: Sequence[object] | None = None,
    use_toolchain: bool = True,
    environment: dict[str, str] | None = None,
) -> None:
    # -D 定义必须合并为单个 VAR:TYPE=VALUE 参数；cmake 不接受名称与值分列两个 argv。
    definition_arguments = [f"{name}={value}" for name, value in definitions]

    command = list(cmake_command) if cmake_command is not None else ["cmake"]
    command.extend(
        [
            "-S",
            source,
            "-B",
            build_directory,
            "-G",
            "Ninja Multi-Config",
            *(
                toolchain_arguments(settings) if use_toolchain else []
            ),
            f"-DCMAKE_INSTALL_PREFIX:PATH={install_prefix}",
            "-DCMAKE_INSTALL_MESSAGE=LAZY",
            *definition_arguments,
        ]
    )
    run(command, settings.source_dir, environment=environment)

def install_configs(
    configs: Sequence[str],
    build_directory: Path,
    environment: dict[str, str] | None = None,
) -> None:
    for config in configs:
        run(
            ["cmake", "--build", build_directory, "--target", "install", "--config", config],
            environment=environment,
        )

def qt_submodules_initialized(source: Path) -> bool:
    """qtbase 是否已初始化。

    已初始化的源码树不传 -init-submodules：该选项会触发 init-repository 的
    脏工作区检查，而 Qt 构建过程本身会改写子模块内的生成文件（如 tiffconf.h），
    导致后续任何平台的 configure 都被误判为 dirty 而拒绝执行。
    """

    return (source / "qtbase" / "CMakeLists.txt").exists()

def require_qt_configured(build_directory: Path) -> None:
    """configure.bat 出错时仍返回 0，需以生成产物判断配置是否成功。"""

    if not (build_directory / "CMakeCache.txt").exists():
        raise DependencyError("Qt 配置失败：构建目录未生成 CMakeCache.txt，请检查上方 configure 输出")

def require_qt_out_of_source(source: Path) -> None:
    """qt5 源码根目录若残留 in-source 构建产物，configure 传入源码路径时会被
    CMake 判定为“已存在的构建树”，从而在源码根目录复用陈旧缓存（in-source 配置），
    导致交叉构建使用过期依赖路径。"""

    stale_cache = source / "CMakeCache.txt"
    if stale_cache.exists():
        raise DependencyError(
            f"qt5 源码根目录存在 in-source 构建产物：{stale_cache}；"
            "顶层 configure 会复用其中陈旧的缓存变量。请先清理该目录下的 "
            "CMakeCache.txt、CMakeFiles、build.ninja、config.* 等构建产物后重试"
        )

def build_qt(settings: DependenciesSettings) -> None:
    if settings.qt_path is not None:
        print(f"[跳过] 使用外部 Qt：{settings.qt_path}")
        return

    source = settings.source_dir / "qt5"
    build_directory = settings.build_directory(source)
    build_directory.mkdir(exist_ok=True)
    require_qt_out_of_source(source)
    qt_config = "-debug-and-release"
    if not settings.build_release and not settings.build_relwithdebinfo:
        qt_config = "-debug"
    configure_script_name = (
        "configure.bat" if settings.host.system == "windows" else "configure"
    )
    configure_script = source / configure_script_name
    qt_arguments = [
        "-submodules",
        "qtdeclarative",
        qt_config,
        *toolchain_arguments(settings),
        "-prefix",
        settings.install_dir / "Qt6.8.3",
        "CMAKE_INSTALL_MESSAGE=LAZY",
    ]
    if not qt_submodules_initialized(source):
        qt_arguments.insert(0, "-init-submodules")
    run([configure_script, *qt_arguments], build_directory)
    require_qt_configured(build_directory)
    run(["cmake", "--build", ".", "--parallel", "--target", "install"], build_directory)

def build_spdlog(settings: DependenciesSettings) -> None:
    source = settings.source_dir / "spdlog"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "spdlog1.16.0",
        [
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i"),
            ("-DSPDLOG_BUILD_SHARED", "1"),
        ],
        settings,
    )
    install_configs(settings.install_configs, build_directory)

def build_kddockwidgets(settings: DependenciesSettings) -> None:
    source = settings.source_dir / "KDDockWidgets"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "KDDockWidgets-qt6-2.4.0",
        [
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i"),
            ("-DCMAKE_PREFIX_PATH:PATH", settings.prefix_paths),
        ],
        settings,
    )
    install_configs(settings.install_configs, build_directory)

def build_vtk(settings: DependenciesSettings) -> None:
    source = settings.source_dir / "vtk"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "VTK9.6.2",
        [
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i"),
            ("-DCMAKE_PREFIX_PATH:PATH", settings.prefix_paths),
            ("-DVTK_SMP_IMPLEMENTATION_TYPE:STRING", "STDThread"),
            ("-DVTK_GROUP_ENABLE_Qt:STRING", "WANT"),
        ],
        settings,
    )
    install_configs(settings.install_configs, build_directory)

def build_freetype(settings: DependenciesSettings) -> None:
    source = settings.source_dir / "freetype"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "freetype2.14.1",
        [("-DCMAKE_PREFIX_PATH:PATH", settings.prefix_paths)],
        settings,
    )
    install_configs(("Release", "Debug"), build_directory)

def build_occt(settings: DependenciesSettings) -> None:
    source = settings.source_dir / "OCCT"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "OpenCASCADE8.0.0",
        [
            ("-DINSTALL_DIR:PATH", settings.install_dir / "OpenCASCADE8.0.0"),
            (
                "-D3RDPARTY_DIR:PATH",
                source / "3rdparty-vc14-64",
            ),
            (
                "-D3RDPARTY_FREETYPE_DIR:PATH",
                settings.install_dir / "freetype2.14.1",
            ),
            ("-DUSE_VTK:BOOL", "1"),
            (
                "-D3RDPARTY_VTK_DIR:PATH",
                settings.install_dir / "VTK9.6.2",
            ),
        ],
        settings,
    )
    install_configs(settings.install_configs, build_directory)

def build_catch2(settings: DependenciesSettings) -> None:
    source = settings.source_dir / "Catch2"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "Catch2-3.11.0",
        [("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i")],
        settings,
    )
    install_configs(settings.install_configs, build_directory)

def build_libmeshb(settings: DependenciesSettings) -> None:
    source = settings.source_dir / "libMeshb"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "libMeshb7.80",
        [("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i")],
        settings,
    )
    install_configs(settings.install_configs, build_directory)

def prepare_tetgen_project(source: Path) -> None:
    cmake_content = "\n".join(
        [
            "cmake_minimum_required(VERSION 3.5)",
            "project(tetgen CXX)",
            "add_library(tet STATIC tetgen.cxx predicates.cxx)",
            "target_compile_definitions(tet PUBLIC TETLIBRARY)",
            "target_include_directories(tet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})",
            "install(TARGETS tet ARCHIVE DESTINATION lib)",
            "install(FILES tetgen.h DESTINATION include)",
            "",
        ]
    )
    write_text_file(source / "CMakeLists.txt", cmake_content)

def build_tetgen(settings: DependenciesSettings) -> None:
    source = settings.source_dir / "tetgen"
    build_directory = settings.build_directory(source)
    prepare_tetgen_project(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "tetgen1.6.0",
        [
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i"),
            ("-DCMAKE_DEBUG_POSTFIX", "d"),
        ],
        settings,
    )
    install_configs(settings.install_configs, build_directory)

def occ_import_libraries(settings: DependenciesSettings) -> str:
    """OCCT 各配置的导入库同名且指向同一个 DLL（TKxxx.dll 无配置后缀），可互换；
    统一指向 RelWithDebInfo 的 libi，避免生成器表达式在 Ninja Multi-Config
    下传给 Gmsh 后不被求值、ninja 把带 `$<` 的字面串当路径找的问题。"""
    cas_root = (settings.install_dir / "OpenCASCADE8.0.0").as_posix()
    return ";".join(
        f"{cas_root}/win64/vc14/libi/{library}.lib"
        for library in OCC_TOOLKIT_LIBRARIES
    )

def build_gmsh(settings: DependenciesSettings) -> None:
    source = settings.source_dir / "gmsh-occ8"
    build_directory = settings.build_directory(source)
    # Gmsh 只认 CASROOT 环境变量探测 OpenCASCADE（find_path HINTS ENV CASROOT），
    # 不消费任何 OCC_INCLUDE_DIR 定义；探测失败时静默关闭 HAVE_OCC，只编译桩实现。
    environment = {**os.environ, "CASROOT": str(settings.install_dir / "OpenCASCADE8.0.0")}
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "gmsh-occ8",
        [
            ("-DCMAKE_CONFIGURATION_TYPES:STRING", "Debug;Release;RelWithDebInfo"),
            ("-DCMAKE_DEBUG_POSTFIX:STRING", "d"),
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX:STRING", "i"),
            ("-DENABLE_OCC:BOOL", "ON"),
            ("-DOCC_LIBS:STRING", occ_import_libraries(settings)),
            ("-DENABLE_OPENMP:BOOL", "OFF"),
            ("-DBUILD_TESTING:BOOL", "OFF"),
            ("-DENABLE_BUILD_DYNAMIC:BOOL", "OFF"),
            ("-DENABLE_BUILD_LIB:BOOL", "OFF"),
            ("-DENABLE_BUILD_SHARED:BOOL", "ON"),
        ],
        settings,
        environment=environment,
    )
    configs = ["Debug"]
    if settings.build_relwithdebinfo:
        configs.append("RelWithDebInfo")
    if settings.build_release:
        configs.append("Release")
    install_configs(configs, build_directory)
    fix_gmsh_occ_paths(settings)

def fix_gmsh_occ_paths(settings: DependenciesSettings) -> None:
    target_file = settings.install_dir / "gmsh-occ8" / "share" / "gmsh" / "gmshTargets.cmake"
    if not target_file.exists():
        raise DependencyError(f"未找到 Gmsh 生成的 CMake 目标文件：{target_file}")

    dependency_prefix = settings.install_dir.as_posix()
    windows_prefix = str(settings.install_dir)
    relative_prefix = "${_IMPORT_PREFIX}/../OpenCASCADE8.0.0"
    content = target_file.read_text(encoding="utf-8")
    updated = content.replace(
        f"{dependency_prefix}/OpenCASCADE8.0.0", relative_prefix
    ).replace(f"{windows_prefix}\\OpenCASCADE8.0.0", relative_prefix)
    if updated == content:
        if relative_prefix in content:
            print("[跳过] OpenCASCADE 路径已经是相对路径")
            return
        # 桌面 gmsh 以 shared 库导出（gmsh::shared）：OCCT 是其私有链接依赖，
        # 不进导出文件；消费方 GmshPlugin 已显式链接所需工具箱，无路径可改写属正常形态。
        print("[跳过] gmsh 导出未携带 OpenCASCADE 绝对路径，无需改写")
        return
    write_text_file(target_file, updated)

def build_boost(settings: DependenciesSettings) -> None:
    source = settings.source_dir / "boost-src"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "boost-1.91.0",
        [
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i"),
            ("-DCMAKE_DEBUG_POSTFIX", "d"),
            ("-DBUILD_SHARED_LIBS", "ON"),
            ("-DBOOST_INSTALL_LAYOUT", "system"),
            ("-DBOOST_INCLUDE_LIBRARIES", BOOST_INCLUDE_LIBRARIES),
        ],
        settings,
    )
    install_configs(settings.install_configs, build_directory)

def build_native(settings: DependenciesSettings) -> None:
    """构建原生（非 wasm）平台的依赖。"""

    if settings.platform.system != "windows":
        raise DependencyError(
            f"{settings.platform.name} 原生依赖构建配方尚未适配，"
            "当前支持 windows 原生构建与 wasm 交叉构建"
        )
    if settings.is_cross and settings.toolchain_path is None:
        raise DependencyError(
            f"在 {settings.host.name} 宿主上构建 {settings.platform.name} "
            "需要 --toolchain 指定交叉编译工具链文件"
        )
    if shutil.which("cl.exe") is None:
        raise DependencyError("未找到 cl.exe，请在 Visual Studio x64 构建环境中运行")
    if shutil.which("cmake") is None:
        raise DependencyError("未找到 cmake，请先将其加入 PATH")
    if shutil.which("ninja") is None:
        raise DependencyError("未找到 ninja，请先将其加入 PATH")

    print(f"[阶段] 构建原生依赖 -> {settings.install_dir}")
    build_qt(settings)
    build_spdlog(settings)
    build_kddockwidgets(settings)
    build_vtk(settings)
    build_freetype(settings)
    build_occt(settings)
    build_catch2(settings)
    build_libmeshb(settings)
    build_tetgen(settings)
    build_gmsh(settings)
    build_boost(settings)

REQUIRED_EMSDK_VERSION = "3.1.56"

# VTK 9.6 的 WebAssembly 构建强制 wasm 原生异常并经 vtkplatform 注入最终链接；
# 链接中所有使用 C++ 异常的静态库必须以同一模式编译，否则旧式 EH 对象引用的
# __cxa_find_matching_catch_* 在链接期无解。Qt 模块以 -fno-exceptions 构建不受影响。
WASM_EXCEPTION_FLAGS = "-fwasm-exceptions"

# Emscripten 的 -pthread 会切换 musl libc 的线程版数据布局（如 FILE 内的锁），
# 全链路静态库必须以同一模式编译，混链单线程对象会内存错乱。
WASM_THREAD_FLAGS = "-pthread"

def wasm_compile_flags(settings: DependenciesSettings, *extra: str) -> str:
    """组装依赖包的公共编译旗标；多线程构建追加 -pthread，单线程构建保持原样。"""

    flags = [WASM_THREAD_FLAGS] if settings.platform.arch == "multiple" else []
    return " ".join(flags + list(extra))

# Emscripten 兼容层缺失的 GL 枚举宏；带平台守卫，不影响宿主平台的 VTK 构建。
VTK_WASM_GL_COMPAT_PATCH = (
    "#ifdef __EMSCRIPTEN__\n"
    "#  define glDrawBuffer(arg)\n"
    "#  define GL_BACK_LEFT 0\n"
    "#  define GL_BACK_RIGHT 0\n"
    "#  define GL_FRONT_LEFT 0\n"
    "#  define GL_FRONT_RIGHT 0\n"
    "#endif\n\n"
)

def prepare_wasm_environment(settings: DependenciesSettings) -> dict[str, str]:
    """校验 emsdk 并组装子进程使用的环境变量副本。"""

    emsdk = settings.emsdk_path
    emscripten_dir = emsdk / "upstream" / "emscripten"
    version_file = emscripten_dir / "emscripten-version.txt"
    if not version_file.exists():
        raise DependencyError(f"未找到 Emscripten 版本文件：{version_file}")
    version = version_file.read_text(encoding="utf-8").strip().strip('"')
    if version != REQUIRED_EMSDK_VERSION:
        raise DependencyError(
            f"Emscripten 版本不符：Qt 6.8.3 的 wasm 构建要求 {REQUIRED_EMSDK_VERSION}，"
            f"实际为 {version}；请执行 emsdk install {REQUIRED_EMSDK_VERSION} 与 "
            f"emsdk activate {REQUIRED_EMSDK_VERSION}"
        )

    environment = os.environ.copy()
    path_additions = [str(emsdk), str(emscripten_dir)]
    node_dir = emsdk / "node"
    if node_dir.exists():
        node_executable_name = "node.exe" if settings.host.system == "windows" else "node"
        node_executables = sorted(node_dir.glob(f"*/bin/{node_executable_name}"))
        if node_executables:
            environment["EMSDK_NODE"] = str(node_executables[0])
            path_additions.append(str(node_executables[0].parent))
    environment["EMSDK"] = str(emsdk)
    environment["PATH"] = os.pathsep.join(path_additions + [environment.get("PATH", "")])
    return environment

def resolve_wasm_host_qt(settings: DependenciesSettings) -> Path:
    """解析 -qt-host-path 使用的宿主平台 Qt（交叉 Qt 构建必需）。"""

    if settings.qt_path is not None:
        return settings.qt_path
    host_qt = settings.dependency_dir / settings.host.name / "Qt6.8.3"
    if not host_qt.exists():
        raise DependencyError(
            f"未找到宿主平台 Qt：{host_qt}；请先完成原生依赖构建，"
            "或用 --qt 指定宿主 Qt 安装路径"
        )
    return host_qt

def require_path(path: Path, description: str) -> Path:
    if not path.exists():
        raise DependencyError(f"{description}不存在：{path}")
    return path

def wasm_qt_cmake_command(settings: DependenciesSettings, wasm_qt: Path) -> list[str]:
    script_name = "qt-cmake.bat" if settings.host.system == "windows" else "qt-cmake"
    script = require_path(wasm_qt / "bin" / script_name, "交叉 Qt 的 qt-cmake")
    return [str(script)]

def build_qt_wasm(
    settings: DependenciesSettings,
    host_qt: Path,
    environment: dict[str, str],
) -> None:
    """按 wasm.md 的流程交叉编译 Qt（Release 单配置，产物供全链路静态链接）。"""

    source = settings.source_dir / "qt5"
    build_directory = settings.build_directory(source)
    build_directory.mkdir(exist_ok=True)
    require_qt_out_of_source(source)
    configure_script_name = (
        "configure.bat" if settings.host.system == "windows" else "configure"
    )
    configure_script = require_path(source / configure_script_name, "Qt 配置脚本")
    qt_arguments = [
        "-submodules",
        "qtdeclarative,qtimageformats",
        "-release",
        "-nomake",
        "examples",
        "-nomake",
        "tests",
        "-qt-host-path",
        host_qt,
        "-no-warnings-are-errors",
        # 开启 Qt 的 WebAssembly 原生异常：QtCore 保留 C++ 异常（其余模块
        # -fno-exceptions 不受影响），QtWasmHelpers 据此为消费者注入
        # -fwasm-exceptions，与 VTK 9.6 强制的原生异常保持同一模式。
        "-feature-wasm-exceptions",
        "-platform",
        "wasm-emscripten",
        "-prefix",
        settings.install_dir / "Qt6.8.3",
        "CMAKE_INSTALL_MESSAGE=LAZY",
    ]
    if settings.platform.arch == "multiple":
        # wasm 多线程：开启 Qt 线程支持（configure 摘要的 Thread support），
        # Qt6::Platform 会随之向消费者传播 -pthread；运行期还需浏览器跨源隔离
        # （COOP/COEP）提供 SharedArrayBuffer。
        qt_arguments += ["-feature-thread"]
    if not qt_submodules_initialized(source):
        qt_arguments.insert(0, "-init-submodules")
    run([configure_script, *qt_arguments], build_directory, environment=environment)
    require_qt_configured(build_directory)
    run(
        ["cmake", "--build", ".", "--parallel", "--target", "install"],
        build_directory,
        environment=environment,
    )

def patch_vtk_wasm_gl_compat(settings: DependenciesSettings) -> None:
    """为 VTK 的 Qt 窗口源文件补充 Emscripten 缺失的 GL 兼容宏（幂等）。"""

    target_file = settings.source_dir / "vtk" / "GUISupport" / "Qt" / "QVTKOpenGLWindow.cxx"
    content = target_file.read_bytes()
    if b"__EMSCRIPTEN__" in content:
        print(f"[跳过] VTK wasm GL 兼容补丁已存在：{target_file}")
        return
    target_file.write_bytes(VTK_WASM_GL_COMPAT_PATCH.encode("utf-8") + content)
    print(f"[补丁] 已写入 VTK wasm GL 兼容宏：{target_file}")

def patch_occt_wasm_toolkit_gl2ps(settings: DependenciesSettings) -> None:
    """OCCT 对 VTK9 无条件追加 vtkRenderingGL2PSOpenGL2 链接；该模块在 GLES
    （wasm）VTK 构建中被条件排除，且 IVtk 系源码不直接引用 gl2ps，改为按目标
    存在性追加（原生下目标存在，行为不变）。"""

    target_file = settings.source_dir / "OCCT" / "adm" / "cmake" / "occt_toolkit.cmake"
    content = target_file.read_bytes()
    if b"TARGET vtkRenderingGL2PSOpenGL2" in content:
        print(f"[跳过] OCCT toolkit gl2ps 条件补丁已存在：{target_file}")
        return
    old = (
        b"        if(VTK_MAJOR_VERSION GREATER 6)\n"
        b"          list (APPEND USED_TOOLKITS_BY_CURRENT_PROJECT vtkRenderingGL2PSOpenGL2)"
    )
    new = (
        b"        if(VTK_MAJOR_VERSION GREATER 6 AND TARGET vtkRenderingGL2PSOpenGL2)\n"
        b"          list (APPEND USED_TOOLKITS_BY_CURRENT_PROJECT vtkRenderingGL2PSOpenGL2)"
    )
    # 该文件为 CRLF 行尾，按实际行尾匹配。
    if b"\r\n" in content:
        old = old.replace(b"\n", b"\r\n")
        new = new.replace(b"\n", b"\r\n")
    if old not in content:
        raise DependencyError(f"未在 {target_file} 中找到 gl2ps 追加代码块")
    target_file.write_bytes(content.replace(old, new))
    print(f"[补丁] 已为 OCCT toolkit 追加 gl2ps 目标存在性条件：{target_file}")

def patch_occt_wasm_convert_signals(settings: DependenciesSettings) -> None:
    """EMSCRIPTEN 下跳过 OCC_CONVERT_SIGNALS（幂等）。

    该宏让 OCCT 经 sigsetjmp 把硬件信号转成 C++ 异常；wasm 上本无可捕获的硬件
    信号，机制无意义。更关键的是 setjmp 与 -fwasm-exceptions 的 invoke 共存会
    触发 LLVM 19 WebAssembly Instruction Selection 的编译器崩溃（ICE
    0xC0000005，见 TKBRep/BinTools 序列化源文件）。桌面 MSVC 构建不进入该分支，
    不受影响。
    """

    target_file = settings.source_dir / "OCCT" / "adm" / "cmake" / "occt_defs_flags.cmake"
    content = target_file.read_bytes()
    if b"NOT EMSCRIPTEN" in content:
        print(f"[跳过] OCCT wasm 信号转换补丁已存在：{target_file}")
        return
    old = "  add_definitions(-DOCC_CONVERT_SIGNALS)"
    new = (
        "  if (NOT EMSCRIPTEN)\n"
        "    # wasm 原生异常与 Emscripten SjLj 共存触发 LLVM ICE，信号转换在 wasm 上亦无意义\n"
        "    add_definitions(-DOCC_CONVERT_SIGNALS)\n"
        "  endif()"
    )
    newline = b"\r\n" if b"\r\n" in content else b"\n"
    old_bytes = old.replace("\n", "\r\n" if newline == b"\r\n" else "\n").encode("utf-8")
    new_bytes = new.replace("\n", "\r\n" if newline == b"\r\n" else "\n").encode("utf-8")
    if old_bytes not in content:
        raise DependencyError(f"未在 {target_file} 中找到 OCC_CONVERT_SIGNALS 定义行")
    target_file.write_bytes(content.replace(old_bytes, new_bytes, 1))
    print(f"[补丁] 已按 EMSCRIPTEN 条件跳过 OCC_CONVERT_SIGNALS：{target_file}")

def build_vtk_wasm(
    settings: DependenciesSettings,
    wasm_qt: Path,
    environment: dict[str, str],
) -> None:
    """按 wasm.md 的配方交叉编译 VTK。

    wasm 只支持静态链接：freetype 与 tiff 统一复用 Qt 内置版本，
    避免与最终链接的 libQt6BundledFreetype.a / libqtiff.a 产生重复符号。
    """

    source = settings.source_dir / "vtk"
    build_directory = settings.build_directory(source)
    freetype_include = require_path(
        wasm_qt / "include" / "QtFreetype", "Qt 内置 freetype 头文件目录"
    )
    freetype_library = require_path(
        wasm_qt / "lib" / "libQt6BundledFreetype.a", "Qt 内置 freetype 静态库"
    )
    tiff_library = require_path(
        wasm_qt / "plugins" / "imageformats" / "libqtiff.a", "Qt 内置 tiff 静态库"
    )
    tiff_include = require_path(
        settings.source_dir / "qt5" / "qtimageformats" / "src" / "3rdparty" / "libtiff" / "libtiff",
        "libtiff 头文件目录",
    )
    definitions = [
        ("-DCMAKE_C_FLAGS", wasm_compile_flags(settings)),
        ("-DCMAKE_CXX_FLAGS", wasm_compile_flags(settings)),
        ("-DCMAKE_PREFIX_PATH:PATH", wasm_qt),
        ("-DBUILD_SHARED_LIBS:BOOL", "OFF"),
        ("-DVTK_GROUP_ENABLE_Qt:STRING", "WANT"),
        # wasm 下不用 std::thread，SMP 改为单线程实现。
        ("-DVTK_SMP_IMPLEMENTATION_TYPE:STRING", "Sequential"),
        ("-DVTK_MODULE_USE_EXTERNAL_VTK_freetype:BOOL", "ON"),
        ("-DFREETYPE_INCLUDE_DIR_ft2build:PATH", freetype_include),
        ("-DFREETYPE_INCLUDE_DIR_freetype2:PATH", freetype_include),
        ("-DFREETYPE_LIBRARY:FILEPATH", freetype_library),
        ("-DVTK_MODULE_USE_EXTERNAL_VTK_tiff:BOOL", "ON"),
        ("-DTIFF_LIBRARY:FILEPATH", tiff_library),
        ("-DTIFF_INCLUDE_DIR:PATH", tiff_include),
    ]
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "VTK9.6.2",
        definitions,
        settings,
        cmake_command=wasm_qt_cmake_command(settings, wasm_qt),
        use_toolchain=False,
        environment=environment,
    )
    install_configs(("Release",), build_directory, environment)

def build_occt_wasm(
    settings: DependenciesSettings,
    wasm_qt: Path,
    environment: dict[str, str],
) -> Path:
    """交叉编译 OpenCASCADE 静态库；freetype 同样复用 Qt 内置版本。"""

    source = settings.source_dir / "OCCT"
    build_directory = settings.build_directory(source)
    install_prefix = settings.install_dir / "OpenCASCADE8.0.0"
    freetype_include = require_path(
        wasm_qt / "include" / "QtFreetype", "Qt 内置 freetype 头文件目录"
    )
    freetype_library = require_path(
        wasm_qt / "lib" / "libQt6BundledFreetype.a", "Qt 内置 freetype 静态库"
    )
    # TKOpenGl 在 Emscripten 下走 GLES2/EGL（USE_GLES2 默认开启），头文件由 emsdk sysroot 提供。
    gl_include_dir = require_path(
        settings.emsdk_path / "upstream" / "emscripten" / "cache" / "sysroot" / "include",
        "emsdk sysroot 头文件目录",
    )
    # OCCT 的 freetype.cmake 收尾时会连同缓存清掉 FREETYPE_INCLUDE_DIR_*，
    # 其后 VTK 导出配置的 find_dependency(Freetype) 需借助 FREETYPE_DIR 环境变量
    # 重新命中 Qt 内置 freetype，才能重建 Freetype::Freetype 目标。
    environment = dict(environment)
    environment["FREETYPE_DIR"] = str(wasm_qt / "include" / "QtFreetype")
    definitions = [
        ("-DCMAKE_C_FLAGS", wasm_compile_flags(settings, WASM_EXCEPTION_FLAGS)),
        ("-DCMAKE_CXX_FLAGS", wasm_compile_flags(settings, WASM_EXCEPTION_FLAGS)),
        ("-DINSTALL_DIR:PATH", install_prefix),
        ("-DBUILD_LIBRARY_TYPE:STRING", "Static"),
        # app/render 使用 TKIVtk（OCCT→VTK 桥接），与原生一致开启 VTK 支持；
        # TKIVtkDraw 随 DRAW 关闭自动排除。
        ("-DUSE_VTK:BOOL", "ON"),
        ("-D3RDPARTY_VTK_DIR:PATH", settings.install_dir / "VTK9.6.2"),
        ("-DUSE_TCL:BOOL", "OFF"),
        ("-DBUILD_MODULE_Draw:BOOL", "OFF"),
        ("-D3RDPARTY_FREETYPE_DIR:PATH", wasm_qt),
        ("-D3RDPARTY_FREETYPE_INCLUDE_DIR_ft2build:PATH", freetype_include),
        ("-D3RDPARTY_FREETYPE_INCLUDE_DIR_freetype2:PATH", freetype_include),
        ("-D3RDPARTY_FREETYPE_LIBRARY:FILEPATH", freetype_library),
        ("-D3RDPARTY_EGL_DIR:PATH", settings.emsdk_path / "upstream" / "emscripten" / "cache" / "sysroot"),
        ("-D3RDPARTY_EGL_INCLUDE_DIR:PATH", gl_include_dir),
        ("-D3RDPARTY_GLES2_DIR:PATH", settings.emsdk_path / "upstream" / "emscripten" / "cache" / "sysroot"),
        ("-D3RDPARTY_GLES2_INCLUDE_DIR:PATH", gl_include_dir),
        # OCCT 的 find_package(VTK) 会经 VTK 导出配置 find_dependency(Qt6/OpenGL/TIFF/Freetype 等)，
        # 与 app 配置同理：prefix 指向 wasm_single，并预置 Qt 内置 freetype/tiff 的查找变量；
        # find 模式需放开重根限制，否则 VTK_DIR/本地路径会被重根进 sysroot 而找不到。
        ("-DCMAKE_PREFIX_PATH:PATH", settings.install_dir),
        ("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE:STRING", "BOTH"),
        ("-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE:STRING", "BOTH"),
        ("-DTIFF_LIBRARY:FILEPATH", wasm_qt / "plugins" / "imageformats" / "libqtiff.a"),
        (
            "-DTIFF_INCLUDE_DIR:PATH",
            settings.source_dir
            / "qt5"
            / "qtimageformats"
            / "src"
            / "3rdparty"
            / "libtiff"
            / "libtiff",
        ),
        ("-DFREETYPE_INCLUDE_DIR_ft2build:PATH", freetype_include),
        ("-DFREETYPE_INCLUDE_DIR_freetype2:PATH", freetype_include),
        ("-DFREETYPE_LIBRARY:FILEPATH", freetype_library),
    ]
    configure_cmake_project(
        source,
        build_directory,
        install_prefix,
        definitions,
        settings,
        environment=environment,
    )
    install_configs(("Release",), build_directory, environment)
    return install_prefix

def locate_occt_wasm_layout(install_prefix: Path) -> tuple[Path, Path]:
    """探测 OCCT 交叉安装布局（Unix 布局头文件在 include、库在 lib；
    Windows 布局头文件在 inc）。"""

    include_dir = install_prefix / "include"
    if not include_dir.exists():
        include_dir = require_path(install_prefix / "inc", "OpenCASCADE 头文件目录")
    candidates = sorted(install_prefix.glob("**/libTKernel*.a")) or sorted(
        install_prefix.glob("**/TKernel*.a")
    )
    if not candidates:
        raise DependencyError(f"未在 {install_prefix} 中找到 OpenCASCADE 静态库 TKernel")
    return include_dir, candidates[0].parent

def occ_wasm_libraries(lib_dir: Path) -> str:
    library_paths = []
    for library in OCC_TOOLKIT_LIBRARIES:
        for name in (f"lib{library}.a", f"{library}.a"):
            candidate = lib_dir / name
            if candidate.exists():
                library_paths.append(candidate.as_posix())
                break
        else:
            raise DependencyError(f"未找到 OpenCASCADE 静态库：{lib_dir / f'lib{library}.a'}")
    return ";".join(library_paths)

def build_kddockwidgets_wasm(
    settings: DependenciesSettings,
    wasm_qt: Path,
    environment: dict[str, str],
) -> None:
    source = settings.source_dir / "KDDockWidgets"
    build_directory = settings.build_directory(source)
    definitions = [
        ("-DCMAKE_C_FLAGS", wasm_compile_flags(settings, WASM_EXCEPTION_FLAGS)),
        ("-DCMAKE_CXX_FLAGS", wasm_compile_flags(settings, WASM_EXCEPTION_FLAGS)),
        ("-DCMAKE_PREFIX_PATH:PATH", wasm_qt),
        # KDDW 有自己的静态开关，BUILD_SHARED_LIBS 对它无效。
        ("-DKDDockWidgets_STATIC:BOOL", "ON"),
        ("-DKDDockWidgets_EXAMPLES:BOOL", "OFF"),
        ("-DKDDockWidgets_TESTS:BOOL", "OFF"),
    ]
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "KDDockWidgets-qt6-2.4.0",
        definitions,
        settings,
        cmake_command=wasm_qt_cmake_command(settings, wasm_qt),
        use_toolchain=False,
        environment=environment,
    )
    install_configs(("Release",), build_directory, environment)

def build_spdlog_wasm(settings: DependenciesSettings, environment: dict[str, str]) -> None:
    source = settings.source_dir / "spdlog"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "spdlog1.16.0",
        [
            ("-DCMAKE_C_FLAGS", wasm_compile_flags(settings, WASM_EXCEPTION_FLAGS)),
            ("-DCMAKE_CXX_FLAGS", wasm_compile_flags(settings, WASM_EXCEPTION_FLAGS)),
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i"),
        ],
        settings,
        environment=environment,
    )
    install_configs(("Release",), build_directory, environment)

def build_catch2_wasm(settings: DependenciesSettings, environment: dict[str, str]) -> None:
    source = settings.source_dir / "Catch2"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "Catch2-3.11.0",
        [
            ("-DCMAKE_C_FLAGS", wasm_compile_flags(settings, WASM_EXCEPTION_FLAGS)),
            ("-DCMAKE_CXX_FLAGS", wasm_compile_flags(settings, WASM_EXCEPTION_FLAGS)),
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i"),
        ],
        settings,
        environment=environment,
    )
    install_configs(("Release",), build_directory, environment)

def build_libmeshb_wasm(settings: DependenciesSettings, environment: dict[str, str]) -> None:
    source = settings.source_dir / "libMeshb"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "libMeshb7.80",
        [
            ("-DCMAKE_C_FLAGS", wasm_compile_flags(settings, "-sSUPPORT_LONGJMP=wasm")),
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i"),
            # 交叉构建时禁用其自动探测到的宿主 gfortran（Fortran 封装为可选组件）。
            ("-DCMAKE_Fortran_COMPILER:STRING", "OFF"),
        ],
        settings,
        environment=environment,
    )
    install_configs(("Release",), build_directory, environment)

def build_tetgen_wasm(settings: DependenciesSettings, environment: dict[str, str]) -> None:
    source = settings.source_dir / "tetgen"
    build_directory = settings.build_directory(source)
    prepare_tetgen_project(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "tetgen1.6.0",
        [
            ("-DCMAKE_C_FLAGS", wasm_compile_flags(settings)),
            ("-DCMAKE_CXX_FLAGS", wasm_compile_flags(settings)),
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i"),
            ("-DCMAKE_DEBUG_POSTFIX", "d"),
        ],
        settings,
        environment=environment,
    )
    install_configs(("Release",), build_directory, environment)

def inject_gmsh_occ_static_libraries(
    settings: DependenciesSettings,
    occ_lib_dir: Path,
) -> None:
    """静态 gmsh 的导出目标不携带链接依赖（gmsh 源码只对 shared 目标挂库），
    把 OCCT 静态库以相对路径注入 gmsh::lib 的接口链接属性。"""

    target_file = settings.install_dir / "gmsh-occ8" / "share" / "gmsh" / "gmshTargets.cmake"
    if not target_file.exists():
        raise DependencyError(f"未找到 Gmsh 生成的 CMake 目标文件：{target_file}")

    content = target_file.read_text(encoding="utf-8")
    if "INTERFACE_LINK_LIBRARIES" in content:
        print(f"[跳过] Gmsh 目标已携带链接依赖：{target_file}")
        return

    occ_root = settings.install_dir / "OpenCASCADE8.0.0"
    lib_sub_dir = occ_lib_dir.relative_to(occ_root).as_posix()
    libraries = ";".join(
        f"${{_IMPORT_PREFIX}}/../OpenCASCADE8.0.0/{lib_sub_dir}/lib{library}.a"
        for library in OCC_TOOLKIT_LIBRARIES
    )
    include_anchor = 'INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"'
    if include_anchor not in content:
        raise DependencyError(f"未在 {target_file} 中找到 gmsh::lib 的接口属性注入点")
    updated = content.replace(
        include_anchor,
        include_anchor + f'\n  INTERFACE_LINK_LIBRARIES "{libraries}"',
    )
    write_text_file(target_file, updated)
    print(f"[注入] 已为 gmsh::lib 写入 OpenCASCADE 静态库链接依赖")

def build_gmsh_wasm(
    settings: DependenciesSettings,
    occ_prefix: Path,
    environment: dict[str, str],
) -> None:
    source = settings.source_dir / "gmsh-occ8"
    build_directory = settings.build_directory(source)
    _, occ_lib_dir = locate_occt_wasm_layout(occ_prefix)
    # Gmsh 只认 CASROOT 环境变量探测 OpenCASCADE；探测失败时静默关闭 HAVE_OCC，只编译桩实现。
    # Emscripten 工具链默认 FIND_ROOT_PATH_MODE_INCLUDE=ONLY，会把 CASROOT 提示路径重根化，
    # 须放开为 BOTH 才能让 find_path 命中依赖目录。
    environment = {**environment, "CASROOT": str(occ_prefix)}
    definitions = [
        ("-DCMAKE_C_FLAGS", wasm_compile_flags(settings, WASM_EXCEPTION_FLAGS)),
        ("-DCMAKE_CXX_FLAGS", wasm_compile_flags(settings, WASM_EXCEPTION_FLAGS)),
        # Emscripten.cmake 默认 MODE_INCLUDE=ONLY 会重根化 find_path 的 CASROOT 提示，须放开。
        ("-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE", "BOTH"),
        ("-DENABLE_OCC:BOOL", "ON"),
        ("-DOCC_LIBS:STRING", occ_wasm_libraries(occ_lib_dir)),
        ("-DENABLE_OPENMP:BOOL", "OFF"),
        # gmsh 的 std::thread 并行先保持关闭，待多线程 ABI 验证通过后再单独开启。
        ("-DENABLE_MULTITHREADED:BOOL", "OFF"),
        # HXT 内置一份无命名空间的 tetgen 拷贝（hxt_boundary_recovery），与 TetGenLibPlugin
        # 链接的 libtet.a 在 wasm 全静态链接下重复符号冲突；HXT 是多线程 3D 算法，单线程
        # wasm 上无收益，且体网格剖分由 TetGenLibPlugin 提供，直接关闭。
        ("-DENABLE_HXT:BOOL", "OFF"),
        ("-DBUILD_TESTING:BOOL", "OFF"),
        ("-DENABLE_BUILD_DYNAMIC:BOOL", "OFF"),
        ("-DENABLE_BUILD_LIB:BOOL", "ON"),
        ("-DENABLE_BUILD_SHARED:BOOL", "OFF"),
    ]
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "gmsh-occ8",
        definitions,
        settings,
        environment=environment,
    )
    install_configs(("Release",), build_directory, environment)
    inject_gmsh_occ_static_libraries(settings, occ_lib_dir)

def build_boost_wasm(settings: DependenciesSettings, environment: dict[str, str]) -> None:
    source = settings.source_dir / "boost-src"
    build_directory = settings.build_directory(source)
    configure_cmake_project(
        source,
        build_directory,
        settings.install_dir / "boost-1.91.0",
        [
            ("-DCMAKE_RELWITHDEBINFO_POSTFIX", "i"),
            ("-DCMAKE_DEBUG_POSTFIX", "d"),
            ("-DBUILD_SHARED_LIBS", "OFF"),
            ("-DBOOST_INSTALL_LAYOUT", "system"),
            ("-DBOOST_INCLUDE_LIBRARIES", BOOST_INCLUDE_LIBRARIES),
            # Emscripten 平台不被 Boost.Config 识别为 posix，显式启用 pthread 路径
            # （单线程构建下为桩实现，避免 container 等库落入 Windows API 分支）。
            ("-DCMAKE_C_FLAGS", wasm_compile_flags(settings, "-DBOOST_HAS_PTHREADS", WASM_EXCEPTION_FLAGS)),
            ("-DCMAKE_CXX_FLAGS", wasm_compile_flags(settings, "-DBOOST_HAS_PTHREADS", WASM_EXCEPTION_FLAGS)),
        ],
        settings,
        environment=environment,
    )
    install_configs(("Release",), build_directory, environment)

def build_wasm(settings: DependenciesSettings) -> None:
    """把依赖交叉编译到 WebAssembly（仅 Release；multiple 架构为 -pthread 多线程配方）。"""

    if shutil.which("cmake") is None:
        raise DependencyError("未找到 cmake，请先将其加入 PATH")
    if shutil.which("ninja") is None:
        raise DependencyError("未找到 ninja，请先将其加入 PATH")

    environment = prepare_wasm_environment(settings)
    host_qt = resolve_wasm_host_qt(settings)
    print(f"[阶段] WebAssembly 交叉编译（{settings.platform.arch}） -> {settings.install_dir}")
    print(f"[环境] emsdk：{settings.emsdk_path}")
    print(f"[环境] 宿主 Qt：{host_qt}")

    # Qt 是后续所有依赖（VTK 的 qt-cmake、freetype/tiff 单一来源）的前提。
    build_qt_wasm(settings, host_qt, environment)
    wasm_qt = require_path(settings.install_dir / "Qt6.8.3", "交叉 Qt 安装目录")

    patch_vtk_wasm_gl_compat(settings)
    build_vtk_wasm(settings, wasm_qt, environment)
    patch_occt_wasm_toolkit_gl2ps(settings)
    patch_occt_wasm_convert_signals(settings)
    occ_prefix = build_occt_wasm(settings, wasm_qt, environment)
    build_kddockwidgets_wasm(settings, wasm_qt, environment)
    build_spdlog_wasm(settings, environment)
    build_catch2_wasm(settings, environment)
    build_libmeshb_wasm(settings, environment)
    build_tetgen_wasm(settings, environment)
    build_gmsh_wasm(settings, occ_prefix, environment)
    build_boost_wasm(settings, environment)

def build_dependencies(settings: DependenciesSettings) -> None:
    if settings.platform.is_wasm:
        build_wasm(settings)
        return
    build_native(settings)

def parse_arguments(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="拉取并编译 PreCess 项目依赖",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "示例：\n"
            "  PreCess-deps.bat D:\\PreCess\\deps\n"
            "  sh ./PreCess-deps.sh ./deps --stage fetch\n"
            "  sh ./PreCess-deps.sh ./deps --platform wasm_single --stage build\n"
        ),
    )
    parser.add_argument("dependency_dir", type=Path, help="依赖根目录")
    parser.add_argument(
        "--release",
        action="store_true",
        help="同时构建 Release 配置（默认仅构建 RelWithDebInfo + Debug）",
    )
    parser.add_argument(
        "--no-relinfo",
        action="store_true",
        help="跳过 RelWithDebInfo 配置的构建",
    )
    parser.add_argument(
        "--qt",
        type=Path,
        metavar="QT_PATH",
        help=(
            "原生平台：使用现有 Qt 安装并跳过 Qt 源码编译；"
            "wasm 平台：指定 -qt-host-path 使用的宿主 Qt"
            "（默认取依赖目录下宿主平台的 Qt6.8.3）"
        ),
    )
    parser.add_argument(
        "--platform",
        type=parse_platform,
        metavar="{%s}" % ",".join(PLATFORM_CHOICES),
        default=None,
        help="依赖构建平台（system_arch），默认随宿主自动探测",
    )
    parser.add_argument(
        "--stage",
        choices=("all", "fetch", "build"),
        default="all",
        help="执行阶段：all=拉取并构建，fetch=仅拉取，build=仅构建",
    )
    parser.add_argument(
        "--toolchain",
        type=Path,
        help="传给 CMake 的交叉编译工具链文件",
    )
    parser.add_argument(
        "--emsdk",
        type=Path,
        help="emsdk 安装路径（wasm 平台必需，默认读取 EMSDK 环境变量）",
    )
    return parser.parse_args(argv)

def resolve_input_path(path: Path | None, option_name: str) -> Path | None:
    if path is None:
        return None
    resolved = path.expanduser().resolve()
    if not resolved.exists():
        raise DependencyError(f"{option_name} 路径不存在：{resolved}")
    return resolved

def resolve_emsdk_path(arguments: argparse.Namespace, target: Platform) -> Path | None:
    emsdk_path = resolve_input_path(arguments.emsdk, "--emsdk")
    if not target.is_wasm or emsdk_path is not None:
        return emsdk_path
    env_emsdk = os.environ.get("EMSDK")
    if env_emsdk:
        return Path(env_emsdk).expanduser().resolve()
    raise DependencyError("wasm 平台需要 emsdk：请用 --emsdk 指定路径或设置 EMSDK 环境变量")

def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    dependency_dir = arguments.dependency_dir.expanduser().resolve()
    host = detect_host_platform()
    target = arguments.platform if arguments.platform is not None else host
    settings = DependenciesSettings(
        dependency_dir=dependency_dir,
        build_release=arguments.release,
        build_relwithdebinfo=not arguments.no_relinfo,
        qt_path=resolve_input_path(arguments.qt, "--qt"),
        platform=target,
        stage=arguments.stage,
        toolchain_path=resolve_input_path(arguments.toolchain, "--toolchain"),
        host=host,
        emsdk_path=resolve_emsdk_path(arguments, target),
    )

    print(f"待配置依赖路径：{dependency_dir}")
    print(f"宿主平台：{host.name}")
    print(f"构建平台：{target.name}")
    if settings.stage in ("all", "fetch"):
        fetch_sources(settings)
    if settings.stage in ("all", "build"):
        build_dependencies(settings)
    print("处理完成！")
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except DependencyError as error:
        print(f"错误：{error}")
        sys.exit(1)
    except subprocess.CalledProcessError as error:
        command_text = format_command(error.cmd) if error.cmd else "<未知命令>"
        print(f"错误：命令执行失败（退出码 {error.returncode}）：{command_text}")
        sys.exit(error.returncode if isinstance(error.returncode, int) and error.returncode else 1)
    except KeyboardInterrupt:
        print("\n用户中断操作")
        sys.exit(130)
