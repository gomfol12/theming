cmake_minimum_required(VERSION 3.20)

# project name
project(theming VERSION 1.0)

# c standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

# build options
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

set(CMAKE_C_FLAGS "-Wall -Wextra -pedantic -Wno-unused-parameter -Wno-unused-value -Wshadow -Wdouble-promotion -Wformat=2 -Wformat-overflow -Wformat-truncation=2 -Wundef -fno-common -Wconversion")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

# find source files
file(GLOB C_SOURCE "${PROJECT_SOURCE_DIR}/src/*.c")

# executable
add_executable(${CMAKE_PROJECT_NAME} ${C_SOURCE})

# include dirs
target_include_directories(
  ${CMAKE_PROJECT_NAME} PRIVATE "${PROJECT_BINARY_DIR}"
                                "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Find the json-c library
find_package(json-c REQUIRED)

# Link json-c statically
set(JSON_C_STATIC_LIBRARY json-c::json-c)
if (NOT TARGET ${JSON_C_STATIC_LIBRARY})
  message(FATAL_ERROR "Could not find the json-c static library")
endif()

# link libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${JSON_C_STATIC_LIBRARY} m)

# install location
set(CMAKE_INSTALL_PREFIX "/usr/local")

# resource path
option(USE_INSTALL_RESOURCE_PATH "Set resource path to install location" OFF)
if(NOT USE_INSTALL_RESOURCE_PATH)
  set(RESOURCE_PATH "${PROJECT_SOURCE_DIR}/content/")
else()
  set(RESOURCE_PATH
      "${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/content/")
endif()

# generate config.h
configure_file(include/project_vars.h.in project_vars.h)

# executable RPATH
set_target_properties(${CMAKE_PROJECT_NAME}
                      PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# install location
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION bin)

# content install location
if(EXISTS "${PROJECT_SOURCE_DIR}/content")
  file(GLOB CONTENT_FILES "${PROJECT_SOURCE_DIR}/content/*")
  if(CONTENT_FILES)
    install(DIRECTORY content
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}")
  endif()
endif()
