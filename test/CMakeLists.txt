cmake_minimum_required (VERSION 3.10)

if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    set(CMAKE_SYSTEM_VERSION "10.0.17763.0")
endif()

project ( CXTest )

set ( CX_OBJ_DIR "obj" )
if (CMAKE_VS_PLATFORM_NAME)
    set ( CX_OBJ_DIR ${CMAKE_VS_PLATFORM_NAME} )
endif()

file(GLOB_RECURSE incFiles CONFIGURE_DEPENDS "../include/cxcollections/*.hpp" )
file(GLOB_RECURSE srcFiles CONFIGURE_DEPENDS "../source/*.cpp" )

include_directories("../include/cxcollections")
add_executable ( CXTest ${incFiles} ${srcFiles} "Test_Allocator.cpp" )

mark_as_advanced( FORCE CMAKE_INSTALL_PREFIX ) # not supporting cmake install
set_target_properties( CXTest PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties( CXTest PROPERTIES LINKER_LANGUAGE CXX)

if(MSVC)
    set_property( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CXTest )
    set_target_properties( CXTest PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    target_compile_options(CXTest PRIVATE "$<$<CONFIG:Debug>:/MTd>"         )
    target_compile_options(CXTest PRIVATE "$<$<CONFIG:Release>:/MT>"        "$<$<CONFIG:Release>:/O2>"        "$<$<CONFIG:Release>:/Oi>"       )
    target_compile_options(CXTest PRIVATE "$<$<CONFIG:MinSizeRel>:/MT>"     "$<$<CONFIG:MinSizeRel>:/O1>"     "$<$<CONFIG:MinSizeRel>:/Oi>"    )
    target_compile_options(CXTest PRIVATE "$<$<CONFIG:RelWithDebInfo>:/MT>" "$<$<CONFIG:RelWithDebInfo>:/O2>" "$<$<CONFIG:RelWithDebInfo>:/Oi>")
    target_compile_options(CXTest PRIVATE "/Zc:__cplusplus")
    target_compile_options(CXTest PRIVATE "/fp:fast")

    #if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
else()
endif()

set_property(TARGET CXTest PROPERTY CXX_STANDARD 17)
set_property(TARGET CXTest PROPERTY CXX_STANDARD_REQUIRED ON)

message ( STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message ( STATUS "PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
message ( STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message ( STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message ( STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")