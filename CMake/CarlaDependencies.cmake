#[[

  Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
  de Barcelona (UAB).
  
  This work is licensed under the terms of the MIT license.
  For a copy, see <https://opensource.org/licenses/MIT>.

]]

include (FetchContent)

set (CARLA_DEPENDENCIES_PENDING)

macro (carla_dependency_add NAME TAG ARCHIVE_URL GIT_URL)
  if (PREFER_CLONE)
    carla_message ("Cloning ${NAME}...")
    FetchContent_Declare(
      ${NAME}
      GIT_REPOSITORY ${GIT_URL}
      GIT_TAG ${TAG}
      GIT_SUBMODULES_RECURSE ON
      GIT_SHALLOW ON
      GIT_PROGRESS ON
      OVERRIDE_FIND_PACKAGE
      ${ARGN}
    )
    list (APPEND CARLA_DEPENDENCIES_PENDING ${NAME})
  else ()
    carla_message ("Downloading ${NAME}...")
    FetchContent_Declare(
      ${NAME}
      URL ${ARCHIVE_URL}
      OVERRIDE_FIND_PACKAGE
      ${ARGN}
    )
    list (APPEND CARLA_DEPENDENCIES_PENDING ${NAME})
  endif ()
endmacro ()

macro (carla_dependencies_make_available)
  FetchContent_MakeAvailable (
    ${CARLA_DEPENDENCIES_PENDING})
  set (CARLA_DEPENDENCIES_PENDING)
endmacro ()

macro (carla_fetchcontent_option NAME VALUE)
  set (${NAME} ${VALUE} CACHE INTERNAL "")
endmacro ()



# ==== SQLITE3 ====

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)

string (REPLACE "." "" CARLA_SQLITE_TAG ${CARLA_SQLITE_VERSION})

carla_dependency_add (
  sqlite3
  ${CARLA_SQLITE_TAG}
  https://www.sqlite.org/2024/sqlite-amalgamation-${CARLA_SQLITE_TAG}.zip
  ""
)
carla_dependencies_make_available ()

add_library (
  libsqlite3 STATIC
  ${sqlite3_SOURCE_DIR}/sqlite3.h
  ${sqlite3_SOURCE_DIR}/sqlite3.c
)

add_executable (
  sqlite3
  ${sqlite3_SOURCE_DIR}/shell.c
)

if (LINUX)
  target_link_libraries (libsqlite3 PRIVATE ${CMAKE_DL_LIBS})
  target_link_libraries (libsqlite3 PRIVATE Threads::Threads)
endif ()

target_link_libraries (
  sqlite3 PRIVATE
  libsqlite3
)

# ==== ZLIB ====

carla_fetchcontent_option (ZLIB_BUILD_EXAMPLES OFF)
carla_dependency_add (
  zlib
  ${CARLA_ZLIB_TAG}
  https://github.com/madler/zlib/archive/refs/tags/${CARLA_ZLIB_TAG}.zip
  https://github.com/madler/zlib.git
)
carla_dependencies_make_available ()
include_directories (${zlib_SOURCE_DIR} ${zlib_BINARY_DIR}) # @TODO HACK

# ==== LIBPNG ====

carla_fetchcontent_option (PNG_TESTS OFF)
carla_fetchcontent_option (PNG_SHARED OFF)
carla_fetchcontent_option (PNG_TOOLS OFF)
carla_fetchcontent_option (PNG_BUILD_ZLIB ON)
if (WIN32)
  carla_fetchcontent_option (ZLIB_LIBRARY ${zlib_BINARY_DIR}/zlibstatic${CARLA_DEBUG_AFFIX}.lib)
else ()
  carla_fetchcontent_option (ZLIB_LIBRARY ${zlib_BINARY_DIR}/libz.a)
endif ()
carla_fetchcontent_option (ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
carla_fetchcontent_option (ZLIB_LIBRARIES ${ZLIB_LIBRARY})
carla_dependency_add (
  libpng
  ${CARLA_LIBPNG_TAG}
  https://github.com/pnggroup/libpng/archive/refs/tags/${CARLA_LIBPNG_TAG}.zip
  https://github.com/glennrp/libpng.git
)
carla_dependencies_make_available ()
include_directories (${libpng_SOURCE_DIR} ${libpng_BINARY_DIR}) # @TODO HACK



# ==== BOOST ====

carla_fetchcontent_option (BOOST_ENABLE_PYTHON ${BUILD_PYTHON_API})
carla_fetchcontent_option (BOOST_ENABLE_MPI OFF)
carla_fetchcontent_option (BOOST_LOCALE_WITH_ICU OFF)
carla_fetchcontent_option (BOOST_LOCALE_WITH_ICONV OFF)
set (
  BOOST_EXCLUDED_PROJECTS
  iostreams
  locale
  redis
  timer
  wave
  url
  mysql
  nowide
  stacktrace
  contract
  fiber
  log
  program_options
)
carla_fetchcontent_option (
  BOOST_EXCLUDE_LIBRARIES
  "${BOOST_EXCLUDED_PROJECTS}")
carla_dependency_add(
  boost
  ${CARLA_BOOST_TAG}
  https://github.com/boostorg/boost/releases/download/${CARLA_BOOST_TAG}/${CARLA_BOOST_TAG}.zip
  https://github.com/boostorg/boost.git
)

# ==== EIGEN ====

carla_fetchcontent_option (EIGEN_BUILD_PKGCONFIG OFF)
carla_fetchcontent_option (BUILD_TESTING OFF)
carla_fetchcontent_option (EIGEN_BUILD_DOC OFF)
carla_dependency_add (
  eigen
  ${CARLA_EIGEN_TAG}
  https://gitlab.com/libeigen/eigen/-/archive/${CARLA_EIGEN_TAG}/eigen-${CARLA_EIGEN_TAG}.tar.gz
  https://gitlab.com/libeigen/eigen.git
)

# ==== RPCLIB ====

carla_dependency_add (
  rpclib
  ${CARLA_RPCLIB_TAG}
  https://github.com/carla-simulator/rpclib/archive/refs/heads/${CARLA_RPCLIB_TAG}.zip
  https://github.com/carla-simulator/rpclib.git
)

# ==== RECAST ====

carla_fetchcontent_option (RECASTNAVIGATION_BUILDER OFF)
carla_dependency_add (
  recastnavigation
  ${CARLA_RECAST_TAG}
  https://github.com/carla-simulator/recastnavigation/archive/refs/heads/${CARLA_RECAST_TAG}.zip
  https://github.com/carla-simulator/recastnavigation.git
)

# ==== PROJ ====

if (ENABLE_OSM2ODR)
  carla_fetchcontent_option (BUILD_TESTING OFF)
  carla_fetchcontent_option (ENABLE_TIFF OFF)
  carla_fetchcontent_option (ENABLE_CURL OFF)
  carla_dependency_add (
    proj
    https://github.com/OSGeo/PROJ/archive/refs/tags/${CARLA_PROJ_TAG}.zip
    https://github.com/OSGeo/PROJ.git
    ${CARLA_PROJ_TAG}
  )
endif ()

# ==== XERCESC ====

if (ENABLE_OSM2ODR)
  carla_dependency_add (
    xercesc
    https://github.com/apache/xerces-c/archive/refs/tags/${CARLA_XERCESC_TAG}.zip
    https://github.com/apache/xerces-c.git
    ${CARLA_XERCESC_TAG}
  )
endif ()

# ==== LUNASVG ====

if (BUILD_OSM_WORLD_RENDERER)
  carla_dependency_add (
    lunasvg
    https://github.com/sammycage/lunasvg/archive/refs/tags/${CARLA_LUNASVG_TAG}.zip
    https://github.com/sammycage/lunasvg.git
    ${CARLA_LUNASVG_TAG}
  )
endif ()

# ==== LIBOSMSCOUT ====

if (BUILD_OSM_WORLD_RENDERER)
  carla_dependency_add (
    libosmscout
    https://github.com/Framstag/libosmscout/archive/refs/tags/${CARLA_LIBOSMSCOUT_TAG}.zip
    https://github.com/Framstag/libosmscout.git
    ${CARLA_LIBOSMSCOUT_TAG}
  )
endif ()

# ==== STREETMAP ====

if (BUILD_CARLA_UNREAL)
  carla_dependency_add (
    StreetMap
    https://github.com/carla-simulator/StreetMap.git
    https://github.com/carla-simulator/StreetMap/archive/refs/heads/${CARLA_STREETMAP_TAG}.zip
    ${CARLA_STREETMAP_TAG}
    SOURCE_DIR ${CARLA_WORKSPACE_PATH}/Unreal/CarlaUnreal/Plugins/StreetMap
  )
endif ()

carla_dependencies_make_available ()
