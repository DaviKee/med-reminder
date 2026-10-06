#
# The MIT License (MIT)
# Copyright (C) 2026 Huawei Device Co., Ltd and iSoftStone Information Technology(Group)Co.,Ltd.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
set(version "1.0.0")
project(getcapacitor VERSION ${version})

#all header
file(GLOB sdk_header
    ${sdk_plugin_header}
)

#common source
file(GLOB sdk_plugin_cpp
    "*.cpp"
    "*.c"
    "*.cc"
    "src/*.cpp"
    "src/*.c"
    "src/*.cc"
)

#all source
file(GLOB sdk_src
    ${sdk_plugin_cpp}
)

#static lib
add_library(${PROJECT_NAME}${STATIC_LIB_SUFFIX} STATIC
    ${sdk_src}
)
    
set_target_properties(${PROJECT_NAME}${STATIC_LIB_SUFFIX}
    PROPERTIES
    LINKER_LANGUAGE CXX
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    OUTPUT_NAME ${TARGET_OUTPUT_NAME_PREFIX}${PROJECT_NAME}${STATIC_LIB_SUFFIX}
)

target_include_directories(${PROJECT_NAME}${STATIC_LIB_SUFFIX}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
