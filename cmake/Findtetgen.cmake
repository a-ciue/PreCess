#[=======================================================================[.rst:
Findtetgen
----------

Finds the TetGen tetrahedral mesh generator library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following :ref:`Imported Targets`:

``tetgen::tetgen``
  Target encapsulating the TetGen library usage requirements, available if
  tetgen is found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``tetgen_FOUND``
  Boolean indicating whether tetgen is found.

``TETGEN_INCLUDE_DIR``
  Include directory containing the tetgen.h header.

``TETGEN_LIBRARY``
  The tetgen library.

Hints
^^^^^

``tetgen_DIR``
  The user may set this cache variable to the root directory of a tetgen
  installation to find tetgen in non-standard locations.

Examples
^^^^^^^^

Finding tetgen and linking it to a project target:

.. code-block:: cmake

  find_package(tetgen REQUIRED)
  target_link_libraries(project_target PRIVATE tetgen::tetgen)
#]=======================================================================]

cmake_policy(PUSH)
cmake_policy(SET CMP0159 NEW)

set(TETGEN_FIND_ARGS
  HINTS
    ${tetgen_DIR} ${TETGEN_DIR}
  PATH_SUFFIXES
    tetgen1.6.0/include
    include
)

find_path(TETGEN_INCLUDE_DIR
  tetgen.h
  ${TETGEN_FIND_ARGS}
)

find_library(TETGEN_LIBRARY_RELEASE
  NAMES
    tet
    tetgen
    libtet
  HINTS
    ${tetgen_DIR} ${TETGEN_DIR}
  PATH_SUFFIXES
    tetgen1.6.0/lib
    lib
)

find_library(TETGEN_LIBRARY_DEBUG
  NAMES
    tetd
    tetgend
    libtetd
  HINTS
    ${tetgen_DIR} ${TETGEN_DIR}
  PATH_SUFFIXES
    tetgen1.6.0/lib
    lib
)

find_library(TETGEN_LIBRARY_RELWITHDEBINFO
  NAMES
    teti
    tetgeni
    libteti
  HINTS
    ${tetgen_DIR} ${TETGEN_DIR}
  PATH_SUFFIXES
    tetgen1.6.0/lib
    lib
)

include(SelectLibraryConfigurations)
select_library_configurations(TETGEN)

unset(TETGEN_FIND_ARGS)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  tetgen
  REQUIRED_VARS
    TETGEN_LIBRARY
    TETGEN_INCLUDE_DIR
)

mark_as_advanced(TETGEN_INCLUDE_DIR)

if(tetgen_FOUND)
  if(NOT TARGET tetgen::tetgen)
    add_library(tetgen::tetgen STATIC IMPORTED)
    set_target_properties(tetgen::tetgen PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${TETGEN_INCLUDE_DIR}"
      INTERFACE_COMPILE_DEFINITIONS "TETLIBRARY")

    if(TETGEN_LIBRARY_RELEASE)
      set_property(TARGET tetgen::tetgen APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(tetgen::tetgen PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
        IMPORTED_LOCATION_RELEASE "${TETGEN_LIBRARY_RELEASE}")
    endif()

    if(TETGEN_LIBRARY_DEBUG)
      set_property(TARGET tetgen::tetgen APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(tetgen::tetgen PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
        IMPORTED_LOCATION_DEBUG "${TETGEN_LIBRARY_DEBUG}")
    endif()

    if(TETGEN_LIBRARY_RELWITHDEBINFO)
      set_property(TARGET tetgen::tetgen APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
      set_target_properties(tetgen::tetgen PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "CXX"
        IMPORTED_LOCATION_RELWITHDEBINFO "${TETGEN_LIBRARY_RELWITHDEBINFO}")
    elseif(TETGEN_LIBRARY_RELEASE)
      # 未安装 RelWithDebInfo 变体时，下游 RelWithDebInfo 构建回退到 Release 库
      set_target_properties(tetgen::tetgen PROPERTIES
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release")
    endif()

    if(NOT TETGEN_LIBRARY_RELEASE AND NOT TETGEN_LIBRARY_DEBUG)
      set_target_properties(tetgen::tetgen PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
        IMPORTED_LOCATION "${TETGEN_LIBRARY}")
    endif()
  endif()
endif()

cmake_policy(POP)
