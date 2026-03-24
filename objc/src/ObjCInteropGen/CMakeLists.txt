cmake_minimum_required(VERSION 3.16.5)
project(ObjCInteropGen)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(ObjCInteropGen
        CangjieWriter.cpp
        CangjieWriter.h
        ClangSession.h
        Config.cpp
        Config.h
        Diagnostics.cpp
        Diagnostics.h
        FatalException.h
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
        SourceScannerConfig.cpp
        SourceScannerConfig.h
        Strings.h
        Symbol.cpp
        Symbol.h
        TomlParseError.h
        Transform.cpp
        Transform.h
        Universe.cpp
        Universe.h
        main.cpp
)

if(LLVM_SRC_DIR AND LLVM_BUILD_DIR)
    target_link_directories(ObjCInteropGen PRIVATE "${LLVM_BUILD_DIR}/lib")
    target_include_directories(ObjCInteropGen SYSTEM PRIVATE
      "${LLVM_SRC_DIR}/clang/include"
      "${LLVM_SRC_DIR}/llvm/include"
      "${LLVM_BUILD_DIR}/include"
      "${LLVM_BUILD_DIR}/tools/clang/include"
    )
    target_link_libraries(ObjCInteropGen PRIVATE
      LLVMBinaryFormat
      LLVMBitstreamReader
      LLVMCore
      LLVMFrontendOpenMP
      LLVMRemarks
      LLVMSupport
      clang
      clangBasic
      clangLex
    )
    if(APPLE)
        target_link_libraries(ObjCInteropGen PRIVATE ncurses)
    endif()
else()
    find_package(Clang REQUIRED CONFIG)
    if(CLANG_INSTALL_PREFIX)
        message(STATUS "Found libclang ${LLVM_PACKAGE_VERSION} in ${CLANG_INSTALL_PREFIX}")
    else()
        message(STATUS "Found libclang ${LLVM_PACKAGE_VERSION} in ${Clang_DIR}")
    endif()
    target_include_directories(ObjCInteropGen SYSTEM PRIVATE "${CLANG_INCLUDE_DIRS}" "${LLVM_INCLUDE_DIRS}")
    target_link_libraries(ObjCInteropGen PRIVATE libclang)
endif()

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

if(NOT TOML_DIR)
    set(TOML_DIR, ../../third_party/tinytoml)
endif()

target_include_directories(ObjCInteropGen SYSTEM PRIVATE "${TOML_DIR}")
target_link_libraries(ObjCInteropGen PRIVATE clangAST)

install(TARGETS ObjCInteropGen DESTINATION tools/bin)
