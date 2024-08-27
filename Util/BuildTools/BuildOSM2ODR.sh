#! /bin/bash
DOC_STRING="Build OSM2ODR."

USAGE_STRING=$(cat <<- END
Usage: $0 [-h|--help]

commands

    [--clean]    Clean intermediate files.
    [--rebuild]  Clean and rebuild both configurations.
END
)

REMOVE_INTERMEDIATE=false
BUILD_OSM2ODR=false
GIT_PULL=true
CURRENT_OSM2ODR_COMMIT=2a490962dc54da711ab09265393a4dc2f6d31813
OSM2ODR_BRANCH=aaron/defaultsidewalkwidth
OSM2ODR_REPO=https://github.com/carla-simulator/sumo.git

OPTS=`getopt -o h --long help,rebuild,build,clean,carsim,no-pull -n 'parse-options' -- "$@"`

eval set -- "$OPTS"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --rebuild )
      REMOVE_INTERMEDIATE=true;
      BUILD_OSM2ODR=true;
      shift ;;
    --build )
      BUILD_OSM2ODR=true;
      shift ;;
    --no-pull )
      GIT_PULL=false
      shift ;;
    --clean )
      REMOVE_INTERMEDIATE=true;
      shift ;;
    -h | --help )
      echo "$DOC_STRING"
      echo "$USAGE_STRING"
      exit 1
      ;;
    * )
      shift ;;
  esac
done

source $(dirname "$0")/Environment.sh

function get_source_code_checksum {
  local EXCLUDE='*__pycache__*'
  find "${OSM2ODR_BASENAME}-source"/* \! -path "${EXCLUDE}" -print0 | sha1sum | awk '{print $1}'
}

if ! { ${REMOVE_INTERMEDIATE} || ${BUILD_OSM2ODR}; }; then
  fatal_error "Nothing selected to be done."
fi

OSM2ODR_BASENAME=${CARLA_BUILD_FOLDER}/osm2odr

# ==============================================================================
# -- Clean intermediate files --------------------------------------------------
# ==============================================================================

if ${REMOVE_INTERMEDIATE} ; then

  log "Cleaning intermediate files and folders."

  rm -Rf ${OSM2ODR_BASENAME}-client-build* ${OSM2ODR_BASENAME}-server-build*
  rm -Rf ${OSM2ODR_BASENAME}-server-install* ${OSM2ODR_BASENAME}-client-install*

fi

# ==============================================================================
# -- Build library -------------------------------------------------------------
# ==============================================================================

if ${BUILD_OSM2ODR} ; then

  if [[ -d ${OSM2ODR_BASENAME}-client-install && -d ${OSM2ODR_BASENAME}-server-install ]] ; then
    log "OSM2ODR already installed."
  else
    rm -Rf \
        ${OSM2ODR_BASENAME}-source \
        ${OSM2ODR_BASENAME}-server-build ${OSM2ODR_BASENAME}-client-build \
        ${OSM2ODR_BASENAME}-server-install ${OSM2ODR_BASENAME}-client-install

    log "Building OSM2ODR."
    if [ ! -d ${OSM2ODR_BASENAME}-source ] ; then
      cd ${CARLA_BUILD_FOLDER}
      curl --retry 5 --retry-max-time 120 -L -o OSM2ODR.zip https://github.com/carla-simulator/sumo/archive/${CURRENT_OSM2ODR_COMMIT}.zip
      unzip -qq OSM2ODR.zip
      rm -f OSM2ODR.zip
      mv sumo-${CURRENT_OSM2ODR_COMMIT} ${OSM2ODR_BASENAME}-source
    fi

    mkdir -p ${OSM2ODR_BASENAME}-client-build
    pushd ${OSM2ODR_BASENAME}-client-build >/dev/null

    cmake ${OSM2ODR_BASENAME}-source \
        -G "Eclipse CDT4 - Ninja" \
        -DCMAKE_INSTALL_PREFIX=${OSM2ODR_BASENAME}-client-install \
        -DCMAKE_TOOLCHAIN_FILE="${CARLA_CLIENT_TOOLCHAIN_FILE}" \
        -DOSM2ODR_INCLUDE_DIR=${CARLA_BUILD_FOLDER}/proj-client-install/include \
        -DOSM2ODR_LIBRARY=${CARLA_BUILD_FOLDER}/proj-client-install/lib/libproj.so \
        -DXercesC_INCLUDE_DIR=${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-client-install/include \
        -DXercesC_LIBRARY=${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-client-install/lib/libxerces-c.so

    ninja osm2odr
    ninja install
    popd >/dev/null

    mkdir -p ${OSM2ODR_BASENAME}-server-build
    pushd ${OSM2ODR_BASENAME}-server-build >/dev/null

    cmake ${OSM2ODR_BASENAME}-source \
        -G "Eclipse CDT4 - Ninja" \
        -DCMAKE_INSTALL_PREFIX=${OSM2ODR_BASENAME}-server-install \
        -DCMAKE_TOOLCHAIN_FILE="${CARLA_SERVER_TOOLCHAIN_FILE}" \
        -DOSM2ODR_INCLUDE_DIR=${CARLA_BUILD_FOLDER}/proj-server-install/include \
        -DOSM2ODR_LIBRARY=${CARLA_BUILD_FOLDER}/proj-server-install/lib/libproj.a \
        -DXercesC_INCLUDE_DIR=${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-server-install/include \
        -DXercesC_LIBRARY=${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-server-install/lib/libxerces-c.a

    ninja osm2odr
    ninja install
    popd >/dev/null

    rm -Rf ${OSM2ODR_BASENAME}-server-build ${OSM2ODR_BASENAME}-client-build
  fi

  cp -p -r ${OSM2ODR_BASENAME}-server-install/include/* ${LIBCARLA_INSTALL_SERVER_FOLDER}/include/
  cp -p ${OSM2ODR_BASENAME}-server-install/lib/*.a ${LIBCARLA_INSTALL_SERVER_FOLDER}/lib

  cp -p -r ${OSM2ODR_BASENAME}-client-install/include/* ${LIBCARLA_INSTALL_CLIENT_FOLDER}/include/
  cp -p ${OSM2ODR_BASENAME}-client-install/lib/*.a ${LIBCARLA_INSTALL_CLIENT_FOLDER}/lib

fi

log " OSM2ODR Success!"
