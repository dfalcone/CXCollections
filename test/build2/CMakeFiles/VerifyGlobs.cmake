# CMAKE generated file: DO NOT EDIT!
# Generated by CMake Version 3.19
cmake_policy(SET CMP0009 NEW)

# incFiles at CMakeLists.txt:14 (file)
file(GLOB_RECURSE NEW_GLOB LIST_DIRECTORIES false "C:/Coding/Projects/CXCollections/test/../include/cxcollections/*.hpp")
set(OLD_GLOB
  "C:/Coding/Projects/CXCollections/test/../include/cxcollections/Allocator.hpp"
  "C:/Coding/Projects/CXCollections/test/../include/cxcollections/Atomic.hpp"
  "C:/Coding/Projects/CXCollections/test/../include/cxcollections/EntityComponentSystem.hpp"
  )
if(NOT "${NEW_GLOB}" STREQUAL "${OLD_GLOB}")
  message("-- GLOB mismatch!")
  file(TOUCH_NOCREATE "C:/Coding/Projects/CXCollections/test/build2/CMakeFiles/cmake.verify_globs")
endif()

# srcFiles at CMakeLists.txt:15 (file)
file(GLOB_RECURSE NEW_GLOB LIST_DIRECTORIES false "C:/Coding/Projects/CXCollections/test/../source/*.cpp")
set(OLD_GLOB
  )
if(NOT "${NEW_GLOB}" STREQUAL "${OLD_GLOB}")
  message("-- GLOB mismatch!")
  file(TOUCH_NOCREATE "C:/Coding/Projects/CXCollections/test/build2/CMakeFiles/cmake.verify_globs")
endif()
