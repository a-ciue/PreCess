# Copied from CMake, but 为了兼容find_package的config搜索模式:
# * 修改了import target名称为 freetype（小写）
# * 修改了模块名为 Findfreetype.cmake（小写）
# * 添加了对dll文件的寻找逻辑
# * 添加了对动态库SHARED/静态库STATIC的区分逻辑
#[=======================================================================[.rst:
Findfreetype
------------

Finds the FreeType font renderer library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following :ref:`Imported Targets`:

``freetype``
  .. versionadded:: 3.10

  Target encapsulating the freetype library usage requirements, available if
  freetype is found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``freetype_FOUND``
  Boolean indicating whether the (requested version of) freetype is found.  For
  backward compatibility, the ``FREETYPE_FOUND`` variable is also set to the
  same value.

``FREETYPE_INCLUDE_DIRS``
  Include directories containing headers needed to use freetype.  This is the
  concatenation of ``FREETYPE_INCLUDE_DIR_ft2build`` and
  ``FREETYPE_INCLUDE_DIR_freetype2`` variables.

``FREETYPE_LIBRARIES``
  Libraries needed to link against for using freetype.

``FREETYPE_VERSION_STRING``
  The version of freetype found.

.. versionadded:: 3.7
  Debug and Release library variants are found separately.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``FREETYPE_INCLUDE_DIR_ft2build``
  The directory containing the main freetype API configuration header.

``FREETYPE_INCLUDE_DIR_freetype2``
  The directory containing freetype public headers.

Hints
^^^^^

This module accepts the following variables:

``FREETYPE_DIR``
  The user may set this cache variable to the root directory of a freetype
  installation to find freetype in non-standard locations.

Examples
^^^^^^^^

Finding freetype and linking it to a project target:

.. code-block:: cmake

  find_package(freetype)
  target_link_libraries(project_target PRIVATE freetype)
#]=======================================================================]

cmake_policy(PUSH)
cmake_policy(SET CMP0159 NEW) # file(STRINGS) with REGEX updates CMAKE_MATCH_<n>

# Created by Eric Wing.
# Modifications by Alexander Neundorf.
# This file has been renamed to "Findfreetype.cmake" instead of the correct
# "FindFreeType.cmake" in order to be compatible with the one from KDE4, Alex.

# Ugh, FreeType seems to use some #include trickery which
# makes this harder than it should be. It looks like they
# put ft2build.h in a common/easier-to-find location which
# then contains a #include to a more specific header in a
# more specific location (#include <freetype/config/ftheader.h>).
# Then from there, they need to set a bunch of #define's
# so you can do something like:
# #include FT_FREETYPE_H
# Unfortunately, using CMake's mechanisms like include_directories()
# wants explicit full paths and this trickery doesn't work too well.
# I'm going to attempt to cut out the middleman and hope
# everything still works.

set(FREETYPE_FIND_ARGS
  HINTS
    ${FREETYPE_DIR}
  PATHS
    ENV GTKMM_BASEPATH
    [HKEY_CURRENT_USER\\SOFTWARE\\gtkmm\\2.4;Path]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\gtkmm\\2.4;Path]
)

find_path(
  FREETYPE_INCLUDE_DIR_ft2build
  ft2build.h
  ${FREETYPE_FIND_ARGS}
  PATH_SUFFIXES
    include/freetype2
    include
    freetype2
)

find_path(
  FREETYPE_INCLUDE_DIR_freetype2
  NAMES
    freetype/config/ftheader.h
    config/ftheader.h
  ${FREETYPE_FIND_ARGS}
  PATH_SUFFIXES
    include/freetype2
    include
    freetype2
)

if(NOT FREETYPE_LIBRARY)
  find_library(FREETYPE_LIBRARY_RELEASE
    NAMES
      freetype
      libfreetype
      freetype219
    ${FREETYPE_FIND_ARGS}
    PATH_SUFFIXES
      lib
  )
  if(FREETYPE_LIBRARY_RELEASE)
    get_filename_component(FREETYPE_LIBRARY_RELEASE_DIR ${FREETYPE_LIBRARY_RELEASE} DIRECTORY)
    find_file(FREETYPE_LIBRARY_RELEASE_DLL
      NAMES freetype.dll libfreetype.dll
      HINTS ${FREETYPE_LIBRARY_RELEASE_DIR}/..
      PATH_SUFFIXES
        bin
    )
  endif()
  find_library(FREETYPE_LIBRARY_DEBUG
    NAMES
      freetyped
      libfreetyped
      freetype219d
    ${FREETYPE_FIND_ARGS}
    PATH_SUFFIXES
      lib
  )  
  if(FREETYPE_LIBRARY_DEBUG)
    get_filename_component(FREETYPE_LIBRARY_DEBUG_DIR ${FREETYPE_LIBRARY_DEBUG} DIRECTORY)
    find_file(FREETYPE_LIBRARY_DEBUG_DLL
      NAMES freetyped.dll libfreetyped.dll
      HINTS ${FREETYPE_LIBRARY_DEBUG_DIR}/..
      PATH_SUFFIXES
        bin
    )
  endif()
  include(SelectLibraryConfigurations)
  select_library_configurations(FREETYPE)
else()
  # on Windows, ensure paths are in canonical format (forward slahes):
  file(TO_CMAKE_PATH "${FREETYPE_LIBRARY}" FREETYPE_LIBRARY)
endif()

unset(FREETYPE_FIND_ARGS)

# set the user variables
if(FREETYPE_INCLUDE_DIR_ft2build AND FREETYPE_INCLUDE_DIR_freetype2)
  set(FREETYPE_INCLUDE_DIRS "${FREETYPE_INCLUDE_DIR_ft2build};${FREETYPE_INCLUDE_DIR_freetype2}")
  list(REMOVE_DUPLICATES FREETYPE_INCLUDE_DIRS)
endif()
set(FREETYPE_LIBRARIES "${FREETYPE_LIBRARY}")

if(EXISTS "${FREETYPE_INCLUDE_DIR_freetype2}/freetype/freetype.h")
  set(FREETYPE_H "${FREETYPE_INCLUDE_DIR_freetype2}/freetype/freetype.h")
elseif(EXISTS "${FREETYPE_INCLUDE_DIR_freetype2}/freetype.h")
  set(FREETYPE_H "${FREETYPE_INCLUDE_DIR_freetype2}/freetype.h")
endif()

if(FREETYPE_INCLUDE_DIR_freetype2 AND FREETYPE_H)
  file(STRINGS "${FREETYPE_H}" freetype_version_str
       REGEX "^#[\t ]*define[\t ]+FREETYPE_(MAJOR|MINOR|PATCH)[\t ]+[0-9]+$")

  unset(FREETYPE_VERSION_STRING)
  foreach(VPART MAJOR MINOR PATCH)
    foreach(VLINE ${freetype_version_str})
      if(VLINE MATCHES "^#[\t ]*define[\t ]+FREETYPE_${VPART}[\t ]+([0-9]+)$")
        set(FREETYPE_VERSION_PART "${CMAKE_MATCH_1}")
        if(FREETYPE_VERSION_STRING)
          string(APPEND FREETYPE_VERSION_STRING ".${FREETYPE_VERSION_PART}")
        else()
          set(FREETYPE_VERSION_STRING "${FREETYPE_VERSION_PART}")
        endif()
        unset(FREETYPE_VERSION_PART)
      endif()
    endforeach()
  endforeach()
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  freetype
  REQUIRED_VARS
    FREETYPE_LIBRARY
    FREETYPE_INCLUDE_DIRS
  VERSION_VAR
    FREETYPE_VERSION_STRING
)

mark_as_advanced(
  FREETYPE_INCLUDE_DIR_freetype2
  FREETYPE_INCLUDE_DIR_ft2build
)

if(freetype_FOUND)
  if(NOT TARGET freetype)
    if(FREETYPE_LIBRARY_RELEASE_DLL OR FREETYPE_LIBRARY_DEBUG_DLL)
        add_library(freetype SHARED IMPORTED)
    else()
        add_library(freetype STATIC IMPORTED)
    endif()
    set_target_properties(freetype PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}")

    if(FREETYPE_LIBRARY_RELEASE)
      set_property(TARGET freetype APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
      if(FREETYPE_LIBRARY_RELEASE_DLL)
        set_target_properties(freetype PROPERTIES
          IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
          IMPORTED_LOCATION_RELEASE "${FREETYPE_LIBRARY_RELEASE_DLL}"
          IMPORTED_IMPLIB_RELEASE "${FREETYPE_LIBRARY_RELEASE}")
      else()
        set_target_properties(freetype PROPERTIES
          IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
          IMPORTED_LOCATION_RELEASE "${FREETYPE_LIBRARY_RELEASE}"
          IMPORTED_IMPLIB_RELEASE "${FREETYPE_LIBRARY_RELEASE}")
      endif()
    endif()

    if(FREETYPE_LIBRARY_DEBUG)
      set_property(TARGET freetype APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
        if(FREETYPE_LIBRARY_DEBUG_DLL)
          set_target_properties(freetype PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
            IMPORTED_LOCATION_DEBUG "${FREETYPE_LIBRARY_DEBUG_DLL}"
            IMPORTED_IMPLIB_DEBUG "${FREETYPE_LIBRARY_DEBUG}")
        else()
          set_target_properties(freetype PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
            IMPORTED_LOCATION_DEBUG "${FREETYPE_LIBRARY_DEBUG}"
            IMPORTED_IMPLIB_DEBUG "${FREETYPE_LIBRARY_DEBUG}")
        endif()
    endif()

    if(NOT FREETYPE_LIBRARY_RELEASE AND NOT FREETYPE_LIBRARY_DEBUG)
      set_target_properties(freetype PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${FREETYPE_LIBRARY}")
    endif()
  endif()
endif()

cmake_policy(POP)
