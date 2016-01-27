cmake_minimum_required(VERSION 2.8)
project(3dsmsplus)
enable_language(ASM)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${DKA_SUGGESTED_C_FLAGS} -DLSB_FIRST -DUSE_ZLIB -O2 -mtp=soft") # Use the devkitArm suggested flags. This is not mandatory.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DKA_SUGGESTED_CXX_FLAGS} -DLSB_FIRST -DUSE_ZLIB -O2 -mtp=soft")

# Note that you must copy the cmake folder and the DevkitArm3DS.cmake file in this directory
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake) # Add the cmake folder to the modules paths, so that we can use the tools and find_package for ctrulib
include(Tools3DS) # Include all the macros and tools needed for 3ds development.

find_package(CTRULIB REQUIRED) # Look for ctrulib
#find_package(LIBKAIRY REQUIRED) # Look for libkairy

file(GLOB_RECURSE SOURCE_FILES
        source/*
)
add_executable(3dsmsplus ${SOURCE_FILES}) # Create the elf file
target_include_directories(3dsmsplus PUBLIC source) # Add the ctrulib include folder

#target_link_libraries(3dsmsplus ${LIBKAIRY_LIBRARIES}) # Link libkairy

target_link_libraries(3dsmsplus ${LIBCTRU_LIBRARIES}) # Link ctrulib
target_include_directories(3dsmsplus PUBLIC ${LIBCTRU_INCLUDE_DIRS}) # Add the ctrulib include folder

target_include_directories(3dsmsplus PUBLIC ${DEVKITPRO}/portlibs/armv6k/include)
target_link_libraries(3dsmsplus ${DEVKITPRO}/portlibs/armv6k/lib/libz.a)

target_link_libraries(3dsmsplus libm.a)

add_3dsx_target(3dsmsplus) # Generate the .3dsx and .smdh from the CtrBootManager target

set(NETLOAD_IP "172.20.146.32")
add_netload_target(3dsmsplus_3dslink 3dsmsplus)

add_citra_target(3dsmsplus_Citra 3dsmsplus)
set_target_properties(3dsmsplus PROPERTIES COMPILE_FLAGS "-DCITRA")
