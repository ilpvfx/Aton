#==========
#
# Copyright (c) 2017, Dan Bethell, Johannes Saam, Vahan Sosoyan.
# All rights reserved.
#
# For license information regarding redistribution and
# use, please refer to the COPYING file.
#
#==========

cmake_minimum_required( VERSION 2.6 )
project( aton )

#=====
# General
set( CMAKE_BUILD_TYPE Release )
set( Boost_USE_STATIC_LIBS ON )
set( CMAKE_CXX_COMPILER g++ )
set( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98 -include cstddef" )

find_package( Boost 1.54.0 COMPONENTS regex filesystem system REQUIRED )
find_package( Nuke REQUIRED )

include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${Boost_INCLUDE_DIRS}
  ${Nuke_INCLUDE_DIR}
  ) 

#=====
# Build the Nuke plugin
add_library( nuke_plugin 
  SHARED
  ${CMAKE_SOURCE_DIR}/src/aton_node.cpp 
  ${CMAKE_SOURCE_DIR}/src/aton_framebuffer.cpp
  ${CMAKE_SOURCE_DIR}/src/aton_server.cpp
  ${CMAKE_SOURCE_DIR}/src/aton_client.cpp
  )

set_target_properties( nuke_plugin
  PROPERTIES
  PREFIX ""
  OUTPUT_NAME "aton"
  COMPILE_FLAGS "-DUSE_GLEW ${Nuke_COMPILE_FLAGS}"
  LINK_FLAGS "${Nuke_LINK_FLAGS}"
  )

target_link_libraries( nuke_plugin 
  ${Boost_LIBRARIES}
  ${Nuke_LIBRARIES}
  )

#=====
# Build the Arnold plugin
find_package( Arnold )

if( ARNOLD_FOUND )
    include_directories( ${Arnold_INCLUDE_DIR} )
   
    # To compile against Arnold 5
    add_definitions(-DARNOLD_5)
    
    # Aton Driver
    add_library( arnold_plugin
      SHARED
      ${CMAKE_SOURCE_DIR}/src/aton_driver_arnold.cpp
      ${CMAKE_SOURCE_DIR}/src/aton_client.cpp
      )
    
    set_target_properties( arnold_plugin
      PROPERTIES
      PREFIX ""
      OUTPUT_NAME "driver_aton"
      COMPILE_FLAGS "${Arnold_COMPILE_FLAGS}"
    )

    target_link_libraries( arnold_plugin
      ${Boost_LIBRARIES}
      ${Arnold_ai_LIBRARY}
      )

    # # Aton Operator
    # add_library( arnold_operator
    #   SHARED
    #   ${CMAKE_SOURCE_DIR}/src/aton_operator_arnold.cpp
    #   )

    # set_target_properties( arnold_operator
    #   PROPERTIES
    #   PREFIX ""
    #   OUTPUT_NAME "operator_aton"
    #   COMPILE_FLAGS "${Arnold_COMPILE_FLAGS}"
    # )

    # target_link_libraries( arnold_operator
    #   ${Arnold_ai_LIBRARY}
    #   )
endif( ARNOLD_FOUND )
