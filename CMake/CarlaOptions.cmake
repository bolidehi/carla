#[[

  Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
  de Barcelona (UAB).
  
  This work is licensed under the terms of the MIT license.
  For a copy, see <https://opensource.org/licenses/MIT>.

]]

macro (carla_string_option NAME DESCRIPTION VALUE)
  set (${NAME} ${VALUE} CACHE STRING ${DESCRIPTION})
endmacro ()

include (${CARLA_WORKSPACE_PATH}/CMake/Options/Common.cmake)
include (${CARLA_WORKSPACE_PATH}/CMake/Options/Unreal.cmake)
include (${CARLA_WORKSPACE_PATH}/CMake/Options/Dependencies.cmake)
