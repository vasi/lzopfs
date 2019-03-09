cmake_minimum_required( VERSION 3.1.0 )
# 3.1.0 for FindThreads with namespace Threads::Threads

project( lzopfs )

set( CMAKE_CXX_STANDARD 11 )

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake" )

include( TestBigEndian )
TEST_BIG_ENDIAN( IS_BIG_ENDIAN )
if ( IS_BIG_ENDIAN )
    add_definitions( -DUSE_BIG_ENDIAN=1 )
else()
    add_definitions( -DUSE_BIG_ENDIAN=0 )
endif()

find_package( Threads REQUIRED )
find_package( LibLZMA REQUIRED )
find_package( ZLIB REQUIRED )
find_package( BZip2 REQUIRED )
find_package( Lzo REQUIRED )
find_package( fuse REQUIRED )

add_definitions( ${FUSE_DEFINITIONS} )

file( GLOB SOURCE_FILES src/*.cpp )

include_directories( ${LIBLZMA_INCLUDE_DIR} ${BZIP2_INCLUDE_DIR} ${LZO_INCLUDE_DIR} ${FUSE_INCLUDE_DIRS} )

    message(STATUS "Lzo Library ${LZO_LIB}")
    message(STATUS "Lzo Include Found in ${LZO_INCLUDE_DIR}")

add_executable( lzopfs ${SOURCE_FILES} )
target_link_libraries( lzopfs Threads::Threads ZLIB::ZLIB ${LIBLZMA_LIBRARIES} ${BZIP2_LIBRARIES} ${LZO_LIB} ${FUSE_LIBRARIES} )
