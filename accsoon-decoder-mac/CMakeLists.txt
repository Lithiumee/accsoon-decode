cmake_minimum_required( VERSION 3.0 )

PROJECT(demo)
set( CMAKE_BUILD_TYPE "Debug" )
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Find Boost
find_package(Boost REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif()

# Find SDL2
find_package(SDL2 REQUIRED)
if(SDL2_FOUND)
    include_directories(${SDL2_INCLUDE_DIRS})
    link_directories(${SDL2_LIBRARY_DIRS})
endif()

# Find FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavutil libavcodec libswscale libavdevice)
if(FFMPEG_FOUND)
    include_directories(${FFMPEG_INCLUDE_DIRS})
    link_directories(${FFMPEG_LIBRARY_DIRS})
endif()

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCES)
add_executable( main ${SOURCES} )

target_link_libraries(main ${FFMPEG_LIBRARIES} ${SDL2_LIBRARIES} ${Boost_LIBRARIES})
