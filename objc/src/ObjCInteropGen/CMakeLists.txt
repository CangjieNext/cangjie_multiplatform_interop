cmake_minimum_required(VERSION 3.16.5)
project(ObjCInteropGen)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(APPLE AND NOT Clang_DIR)
    set(Clang_DIR /opt/homebrew/opt/llvm@15/lib/cmake/clang)
endif()

find_package(Clang REQUIRED CONFIG)
if(CLANG_INSTALL_PREFIX)
    message(STATUS "Found libclang ${LLVM_PACKAGE_VERSION} in ${CLANG_INSTALL_PREFIX}")
else()
    message(STATUS "Found libclang ${LLVM_PACKAGE_VERSION} in ${Clang_DIR}")
endif()

add_executable(ObjCInteropGen
        CangjieWriter.cpp
        CangjieWriter.h
        ClangSession.h
        Config.cpp
        Config.h
        Diagnostics.cpp
        Diagnostics.h
        InputFile.cpp
        InputFile.h
        Logging.cpp
        Logging.h
        Mappings.cpp
        Mappings.h
        MappingsConfig.cpp
        MarkPackage.cpp
        MarkPackage.h
        Mode.cpp
        Mode.h
        Package.cpp
        Package.h
        PackageConfig.h
        PackageFilters.cpp
        PackageOutputs.cpp
        SingleDeclarationSymbolVisitor.cpp
        SingleDeclarationSymbolVisitor.h
        SourceScanner.cpp
        SourceScanner.h
        SourceScannerConfig.cpp
        SourceScannerConfig.h
        Strings.h
        Symbol.cpp
        Symbol.h
        Transform.cpp
        Transform.h
        Universe.cpp
        Universe.h
        main.cpp
)

# Aggressive C++ standard library assertions for libstdc++
#target_compile_options(ObjCInteropGen PRIVATE $<$<CONFIG:Debug>:-ggdb3>)
#target_compile_definitions(ObjCInteropGen PRIVATE $<$<CONFIG:Debug>:_GLIBCXX_DEBUG;_GLIBCXX_DEBUG_BACKTRACE;_GLIBCXX_DEBUG_PEDANTIC>)
#target_link_libraries(ObjCInteropGen PRIVATE $<$<CONFIG:Debug>:stdc++exp>)

if(MSVC)
    target_compile_options(ObjCInteropGen PRIVATE /W3)
else()
    target_compile_options(ObjCInteropGen PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(UNIX AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ObjCInteropGen PRIVATE -fstack-protector-all -fstack-protector-strong)
    target_compile_definitions(ObjCInteropGen PRIVATE _FORTIFY_SOURCE=2)
    if(NOT APPLE)
        target_link_options(ObjCInteropGen PRIVATE -s -Wl,-z,relro,-z,now,-z,noexecstack)
    endif()
endif()

# Link ObjCInteropGen to LLVM & Clang
target_include_directories(ObjCInteropGen SYSTEM PRIVATE ${CLANG_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS} ../../third_party/tomlplusplus)
target_link_libraries(ObjCInteropGen PRIVATE libclang clangAST)

install(TARGETS ObjCInteropGen DESTINATION tools/bin)
