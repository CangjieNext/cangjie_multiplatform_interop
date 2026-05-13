#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

"""Cangjie ObjectiveC interoperability build script"""
import argparse
import fileinput
import logging
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
from enum import Enum
from logging.handlers import TimedRotatingFileHandler
from subprocess import PIPE

IS_DARWIN = platform.system() == "Darwin"

BUILD_DIR = os.path.dirname(os.path.abspath(__file__))
HOME_DIR = os.path.dirname(BUILD_DIR)

OBJC_INTEROP_GEN_DIR = os.path.join(HOME_DIR, 'src', 'ObjCInteropGen')
CMAKE_BUILD_DIR = os.path.join(OBJC_INTEROP_GEN_DIR, 'build', 'build')
DEFAULT_INSTALL_DIR = os.path.join(HOME_DIR, 'dist')

INTEROPLIB_DIR = os.path.join(HOME_DIR, 'src', 'interoplib')
OUTPUT_DIR = os.path.join(INTEROPLIB_DIR, 'output')
DYLIB_EXT = "dylib" if IS_DARWIN else "so"

OUT_OBJC_INTERNAL_DYLIB = os.path.join(OUTPUT_DIR, f"libobjc.internal.{DYLIB_EXT}")
OUT_OBJC_INTERNAL_A     = os.path.join(OUTPUT_DIR, "libobjc.internal.a")
OUT_OBJC_INTERNAL_CJO   = os.path.join(OUTPUT_DIR, "objc.internal.cjo")

OUT_OBJC_LANG_DYLIB = os.path.join(OUTPUT_DIR, f"libobjc.lang.{DYLIB_EXT}")
OUT_OBJC_LANG_A     = os.path.join(OUTPUT_DIR, "libobjc.lang.a")
OUT_OBJC_LANG_CJO   = os.path.join(OUTPUT_DIR, "objc.lang.cjo")

LOG_DIR = os.path.join(BUILD_DIR, 'logs')
LOG_FILE = os.path.join(LOG_DIR, 'ObjCInteropGen.log')

CJC_BASE_ARGS = ["-Woff", "unused", "-Woff", "parser", "-O2", f"--output-dir={OUTPUT_DIR}", "--int-overflow=wrapping", "--disable-reflection"]
if not IS_DARWIN:
    CJC_BASE_ARGS += ["--link-options", "-z relro", "--link-options", "-z now"]

def log_output(output):
    """log command output"""
    while True:
        line = output.stdout.readline()
        if not line:
            output.communicate()
            returncode = output.returncode
            if returncode != 0:
                LOG.critical('build error: %d!\n', returncode)
                sys.exit(1)
            break
        try:
            LOG.info(line.decode('ascii', 'ignore').rstrip())
        except UnicodeEncodeError:
            LOG.info(line.decode('utf-8', 'ignore').rstrip())

def init_log(name):
    """init log config"""
    prepare_dir(LOG_DIR)

    log = logging.getLogger(name)
    log.setLevel(logging.DEBUG)
    formatter = logging.Formatter(
        '[%(asctime)s:%(module)s:%(lineno)s:%(levelname)s] %(message)s')
    streamhandler = logging.StreamHandler(sys.stdout)
    streamhandler.setLevel(logging.DEBUG)
    streamhandler.setFormatter(formatter)
    log.addHandler(streamhandler)
    filehandler = TimedRotatingFileHandler(LOG_FILE,
                                           when='W6',
                                           interval=1,
                                           backupCount=60)
    filehandler.setLevel(logging.DEBUG)
    filehandler.setFormatter(formatter)
    log.addHandler(filehandler)
    return log

def fatal(message):
    """Log the message as CRITICAL and raise Exception with the same message"""
    LOG.critical(message)
    raise Exception(message)

def fixedEnv(env=None):
    if env is None:
        env = os.environ.copy()
    env["ZERO_AR_DATE"] = "1"
    return env

def check_in_toolchain(args, tool):
    # If user didn't specify --target-toolchain, we search for an available tool in $PATH.
    # If user did specify --target-toolchain, we search in user given path ONLY. By doing so
    # user could see a proper '... not found' error if the given path is incorrect.
    toolchain_path = args.target_toolchain if args.target_toolchain else None
    if toolchain_path and (not os.path.exists(toolchain_path)):
        LOG.error(f"The given toolchain path does not exist: {toolchain_path}")

    c_tool = shutil.which(tool, path=toolchain_path)

    if c_tool is None:
        if toolchain_path:
            LOG.error(f"Cannot find {tool} in the given toolchain path: {toolchain_path}")
        else:
            LOG.error(f"Cannot find {tool} in $PATH")
        fatal(f"{tool} is required to build interop libraries")

    return c_tool

def command(*args, cwd=None, env=None):
    """Execute a child program via 'subprocess.Popen' and log the output"""
    output = subprocess.Popen(args, stdout=PIPE, cwd=cwd, env=fixedEnv(env))
    log_output(output)
    if output.returncode:
        fatal('"' + ' '.join(args) + '" returned ' + output.returncode)

def adjust_target(target):
    match target:
        case "ios-aarch64": return "arm64-apple-ios11"
        case "ios-simulator-aarch64": return "arm64-apple-ios11-simulator"
        case "ios-simulator-x86_64": return "x86_64-apple-ios11-simulator"
        case _: return target

def runtime_name(target):
    return target+"_cjnative"

def download_and_patch_tinytoml():
    """Set up the tinytoml third-party library"""
    TINYTOML_DIR = os.path.join(HOME_DIR, "third_party", "tinytoml")
    PATCH_FILE = os.path.join(TINYTOML_DIR, "fixFloatEqualError.patch")
    TAR_PATH = os.path.join(TINYTOML_DIR, "tinytoml-0.4.tar.gz")
    TINYTOMLTAR_PATH = os.path.join(TINYTOML_DIR, "tinytoml-0.4")

    LOG.info("Setting up tinytoml...")

    # Create Parent Directory
    os.makedirs(os.path.dirname(TINYTOML_DIR), exist_ok=True)

    try:

        if not os.path.exists(TAR_PATH):
            # Fetch tinytoml from the remote repostory
            subprocess.run(
                ["git", "clone", "https://gitcode.com/src-openeuler/tinytoml.git",
                 TINYTOML_DIR],
                 check=True
            )
            # Switch to the specified tag
            subprocess.run(
                ["git", "checkout", "openEuler-24.03-LTS-SP1"],
                cwd=TINYTOML_DIR,
                check=True
            )

        # Extract the package
        LOG.info("Extracting tinytoml package...")
        with tarfile.open(TAR_PATH) as tar:
            tar.extractall(path=TINYTOML_DIR)

        # Apply Patch
        patch_file_exists = os.path.exists(PATCH_FILE)
        target_dir_exists = os.path.exists(TINYTOMLTAR_PATH) and os.path.isdir(TINYTOMLTAR_PATH)
        if patch_file_exists and target_dir_exists:
            LOG.info("Applying tinytoml patch...")
            subprocess.run(
                f"patch -p0 -l -f < {PATCH_FILE}",
                cwd=TINYTOMLTAR_PATH,
                shell=True,
                check=True
            )
        else:
            LOG.warning(f"Patch file not found: {PATCH_FILE}")

        TOML_H_SRC = os.path.join(TINYTOMLTAR_PATH, "include", "toml", "toml.h")
        TOML_H_DST = os.path.join(TINYTOML_DIR, "toml.h")

        if os.path.exists(TOML_H_SRC):
            LOG.info(f"Copying toml.h from {TOML_H_SRC} to {TOML_H_DST}")
            shutil.copy2(TOML_H_SRC, TOML_H_DST)

    except subprocess.CalledProcessError as e:
        LOG.error(f"Failed to setup tinytoml: {str(e)}")
        # Clean Files
        if os.path.exists(TINYTOML_DIR):
            shutil.rmtree(TINYTOML_DIR)
        raise

def replace_in_file(text_to_search, replacement_text, filename):
    with fileinput.FileInput(filename, inplace=True) as file:
        for line in file:
            print(line.replace(text_to_search, replacement_text), end='')

def build(args):
    """interoplib or objc-interop-gen build"""
    if args.target:
        runtime = runtime_name(args.target)
        LOG.info('begin build interoplib for ' + runtime + '\n')

        OBJCLIB_O = os.path.join(prepare_dir(OUTPUT_DIR), "objclib.o")
        INTEROPLIB_SRC = os.path.join(INTEROPLIB_DIR, "objc", "src")
        SRC_INTERNAL = os.path.join(INTEROPLIB_SRC, "internal")
        SRC_LANG = os.path.join(INTEROPLIB_SRC, "lang")

        clang = check_in_toolchain(args, "clang")
        ld = check_in_toolchain(args, "ld")
        ar = check_in_toolchain(args, "ar")
        ranlib = check_in_toolchain(args, "ranlib")

        clang_opts = ["-fmodules", "-c", "-fPIC"]
        ld_opts = []
        if args.target_lib:
            clang_target = adjust_target(args.target_lib)
            clang_opts += [f"--target={clang_target}"]
        if IS_DARWIN and (("darwin" in args.target) or ("macos" in args.target)):
            clang_opts += ["-mmacosx-version-min=12.0"]
            ld_opts += ["-macos_version_min", "12.0"]
        if args.target_sysroot:
            clang_opts += [f"-isysroot{args.target_sysroot}"]

        if IS_DARWIN:
            # -fno-objc-msgsend-selector-stubs must be specified when the used clang
            # version is the Apple Clang, in order to prevent objc_msgSend stub optimization.
            # The Cangjie LLVM is [currently] a fork of open-source LLVM 15,
            # which cannot link object files generated with this optimization.
            clang_version = subprocess.run([clang, "--version"], capture_output=True, check=True).stdout.decode().splitlines()[0]
            if "Apple clang" in clang_version:
                clang_opts += ["-fno-objc-msgsend-selector-stubs"]
        else:
            clang_opts += subprocess.run(['gnustep-config', '--objc-flags'], capture_output=True).stdout.decode().split()

        # objclib.o
        command(
            clang, *clang_opts, "-I.", "cjinterop.m", f"-o{OBJCLIB_O}",
            cwd=os.path.join(INTEROPLIB_DIR, 'src', 'objclib')
        )

        # cjc to compile cj-code of interoplib
        cjc_args = ["cjc", "--no-sub-pkg", f"--import-path={OUTPUT_DIR}"] + CJC_BASE_ARGS
        if args.target_lib:
            cjc_args += ["--target=" + adjust_target(args.target_lib)]
        if args.target_sysroot:
            cjc_args += ["--sysroot", args.target_sysroot]
        if args.target_toolchain:
            cjc_args += ["-B", args.target_toolchain]

        if IS_DARWIN:
            cjc_args += ["-lobjc"]

        cjc_A = cjc_args + ["--output-type=staticlib"]
        cjc_SO = cjc_args + ["--output-type=dylib"]

        # objc.internal (+ objclib.o)
        command(
            *(cjc_A + ["-p", SRC_INTERNAL]),
            cwd=INTEROPLIB_DIR
        )
        command(
            ar, "-x", "libobjc.internal.a",
            cwd=OUTPUT_DIR,
        )
        os.rename(os.path.join(OUTPUT_DIR, "objc.internal.o"), os.path.join(OUTPUT_DIR, "orig.objc.internal.o"))
        command(
            ld, "-r", *ld_opts, "-o", "objc.internal.o", "orig.objc.internal.o", OBJCLIB_O,
            cwd=OUTPUT_DIR,
        )
        os.remove(os.path.join(OUTPUT_DIR, "orig.objc.internal.o"))
        os.remove(os.path.join(OUTPUT_DIR, "libobjc.internal.a"))
        command(
            ar, "-cr", "libobjc.internal.a", "objc.internal.o",
            cwd=OUTPUT_DIR,
        )
        command(
            ranlib, "-D", "libobjc.internal.a",
            cwd=OUTPUT_DIR,
        )

        command(
            *(cjc_SO + ["-p", SRC_INTERNAL, "--link-options", OBJCLIB_O]),
            cwd=INTEROPLIB_DIR
        )

        # objc.lang
        command(
            *(cjc_A + ["-p", SRC_LANG]),
            cwd=INTEROPLIB_DIR
        )
        command(
            *(cjc_SO + ["-p", SRC_LANG, f"-L{OUTPUT_DIR}", "-lobjc.internal"]),
            cwd=INTEROPLIB_DIR
        )

        LOG.info('end build interoplib for ' + runtime + '\n')
    else:
        LOG.info('begin build objc-interop-gen...\n')

        # Add the tinytoml third-party library for acquisition and customize code application.
        if args.toml_dir:
            TOML_DIR = args.toml_dir
        else:
            TOML_DIR = os.path.join(HOME_DIR, "third_party", "tinytoml");
            tinytoml_target = os.path.join(TOML_DIR, "toml.h")
            if not os.path.exists(tinytoml_target):
                download_and_patch_tinytoml()

        cmake_args = ["-B", CMAKE_BUILD_DIR, "-DCMAKE_BUILD_TYPE=" + args.build_type.value, "-DTOML_DIR=" + TOML_DIR]
        if args.llvm_src_dir or args.llvm_build_dir:
            if not args.llvm_src_dir or not args.llvm_build_dir:
                LOG.critical("Both directories must be specified: --llvm-src-dir and --llvm-build-dir")
                sys.exit(1)
            cmake_args.append("-DLLVM_SRC_DIR=" + args.llvm_src_dir)
            cmake_args.append("-DLLVM_BUILD_DIR=" + args.llvm_build_dir)

        command("cmake", *cmake_args, cwd=OBJC_INTEROP_GEN_DIR)

        command("cmake", "--build", CMAKE_BUILD_DIR, cwd=OBJC_INTEROP_GEN_DIR)

        LOG.info('end build objc-interop-gen\n')

def clean(args):
    """clean build outputs and logs"""
    LOG.info("begin clean objc-interop-gen...\n")
    if os.path.isdir(CMAKE_BUILD_DIR):
        shutil.rmtree(CMAKE_BUILD_DIR, ignore_errors=True)
    LOG.info("end clean objc-interop-gen\n")

    LOG.info("begin clean interoplib...\n")
    if os.path.isdir(OUTPUT_DIR):
        shutil.rmtree(OUTPUT_DIR, ignore_errors=True)
    LOG.info("end clean interoplib\n")

def prepare_dir(base_path, *relative_path):
    """
    Ensure that the directory specified by the arguments exists (create it if it
    does not) and return its path.
    """
    path = os.path.join(base_path, *relative_path)
    if not os.path.exists(path):
        os.makedirs(path)
    return path

def install_file(install_dir, file):
    if os.path.isfile(file):
        shutil.copy2(file, install_dir)
    else:
        fatal("Cannot find \"" + file + "\" for installing to \"" + install_dir + "\"")

def install_files(install_dir, *files):
    for file in files:
        install_file(install_dir, file)

def find_match(lines, regexp):
    """Searching the list of lines, find the first substring that matches the capture in the specified pattern"""
    pattern = re.compile(regexp)
    for line in lines:
        matched = re.search(pattern, line)
        if matched:
            return matched.group(1)
    return None

def change_dependency_install_name(binary, otool_output, library):
    """
    Change the dependent library install name to the @rpath-based one.  If the
    binary does not contain an install name for this library, raise an exception.

    Arguments:
    binary - the name of the binary file where to change the dependency
    otool_output - the output of the `otool -l <binary>` command, splitted into
                   lines
    library - the file name (without a path) of the library
    """
    path = find_match(otool_output, r"name (.*/" + library + r") \(offset \d*\)")
    if path:
        command("install_name_tool", "-change", path, "@rpath/" + library, binary)
    else:
        fatal("Expected dependency on \"" + library + "\" in \"" + binary + "\" not found")

def change_install_names(dylib, dependencies):
    """
    Set @rpath in the specified dynamic library and change the install names of its
    dependencies to @rpath-based ones.  If the binary does not contain any of the
    specified dependencies, raise an exception.

    Arguments:
    dylib - the name of the dynamic library where to change the dependency
    dependencies - array of dependency file names (without paths)
    """
    otool_output = subprocess.run(["otool", "-l", dylib], capture_output=True, env=fixedEnv()).stdout.decode().splitlines()
    rpath = find_match(otool_output, r"path (.*) \(offset \d*\)")
    command("install_name_tool", "-id", "@rpath/" + os.path.basename(dylib), dylib)
    if rpath:
        command("install_name_tool", "-rpath", rpath, "@loader_path", dylib)
    else:
        command("install_name_tool", "-add_rpath", "@loader_path", dylib)
    for dependency in dependencies:
        change_dependency_install_name(dylib, otool_output, dependency)

def install(args):
    """install objc-interop-gen or interoplib"""
    install_path = os.path.abspath(args.install_prefix) if args.install_prefix else DEFAULT_INSTALL_DIR

    if args.target:
        # while cjpm does not support install command for libs, just copy to install them
        runtime = runtime_name(args.target)
        LOG.info("begin install interoplib for " + runtime + "\n")

        installation_dir_static = prepare_dir(install_path, "lib", runtime)
        install_files(
            installation_dir_static,
            OUT_OBJC_INTERNAL_A,
            OUT_OBJC_LANG_A,
        )

        installation_dir_dynamic = prepare_dir(install_path, "runtime", "lib", runtime)
        install_files(
            installation_dir_dynamic,
            OUT_OBJC_INTERNAL_DYLIB,
            OUT_OBJC_LANG_DYLIB,
        )

        if IS_DARWIN:
            change_install_names(
                os.path.join(installation_dir_dynamic, "libobjc.internal.dylib"),
                []
            )
            change_install_names(
                os.path.join(installation_dir_dynamic, "libobjc.lang.dylib"),
                ["libobjc.internal.dylib"]
            )

        install_files(
            prepare_dir(install_path, "modules", runtime),
            OUT_OBJC_INTERNAL_CJO,
            OUT_OBJC_LANG_CJO
        )

        LOG.info("end install interoplib for " + runtime + "\n")
    else:
        LOG.info("begin install objc-interop-gen...")

        tools_bin_dir = prepare_dir(install_path, "tools", "bin")
        install_file(tools_bin_dir, os.path.join(CMAKE_BUILD_DIR, "ObjCInteropGen"))
        if IS_DARWIN:
            # Change the @rpath of the ObjCInteropGen executable to the directory inside the
            # same Cangjie SDK where the dependent library, libclang.dylib, is located.
            INSTALLED_OBJC_INTEROP_GEN = os.path.join(tools_bin_dir, "ObjCInteropGen")
            rpath = find_match(
                subprocess.run(
                    ["otool", "-l", INSTALLED_OBJC_INTEROP_GEN],
                    capture_output=True
                ).stdout.decode().splitlines(),
                r"path (.*) \(offset \d*\)"
            )
            if rpath:
                command(
                    "install_name_tool",
                    "-rpath",
                    rpath,
                    "@loader_path/../../third_party/llvm/lib",
                    INSTALLED_OBJC_INTEROP_GEN
                )
            else:
                command(
                    "install_name_tool",
                    "-add_rpath",
                    "@loader_path/../../third_party/llvm/lib",
                    INSTALLED_OBJC_INTEROP_GEN
                )

        LOG.info("end install objc-interop-gen")

class BuildType(Enum):
    """CMAKE_BUILD_TYPE options"""
    debug = 'Debug'
    release = 'Release'

    def __str__(self):
        return self.name

    def __repr__(self):
        return str(self)

    @staticmethod
    def argparse(s):
        try:
            return BuildType[s]
        except KeyError:
            return s

def main():
    """build entry"""
    clang_path = shutil.which('clang')
    clang_pp_path = shutil.which('clang++')
    if not clang_path:
      LOG.error('clang is required to build cangjie compiler')
    if not clang_pp_path:
      LOG.error('clang++ is required to build cangjie compiler')

    os.environ['CC'] = clang_path
    os.environ['CXX'] = clang_pp_path

    parser = argparse.ArgumentParser(description='build Objective-C binding generator or interoplib')
    subparsers = parser.add_subparsers(help='sub command help')
    parser_build = subparsers.add_parser('build', help='build Objective-C binding generator or interoplib')
    parser_build.add_argument('-t',
                              '--build-type',
                              type=BuildType.argparse,
                              dest='build_type',
                              default=BuildType.release,
                              choices=list(BuildType),
                              help='select target build type')
    parser_build.add_argument("--toml-dir", help="location of the toml.h C++ header file")
    parser_build.add_argument("--llvm-src-dir", help="LLVM source directory")
    parser_build.add_argument("--llvm-build-dir",
        help="LLVM build directory that contains generated headers and compiled libraries"
    )
    parser_build.add_argument(
        "--target", dest="target", type=str,
        help="build interoplib for the specified target"
    )
    parser_build.add_argument(
        "--target-lib", dest="target_lib", type=str,
        help="when build interoplib, use the specified target triple"
    )
    parser_build.add_argument(
        "--target-toolchain", dest="target_toolchain", type=str,
        help="when build interoplib, use the tools under the given path to cross-compile (should point to bin directory)"
    )
    parser_build.add_argument(
        "--target-sysroot", dest="target_sysroot", type=str,
        help="when build interoplib, pass this argument to the compilers as --sysroot"
    )
    parser_build.set_defaults(func=build)

    parser_install = subparsers.add_parser("install", help="install Objective-C binding generator or interoplib")
    parser_install.add_argument(
        "--target", dest="target", type=str,
        help="install interoplib for the specified target"
    )
    parser_install.add_argument('--prefix',
                            dest='install_prefix',
                            help='target install prefix')
    parser_install.set_defaults(func=install)

    parser_clean = subparsers.add_parser("clean", help="clean for both Objective-C binding generator and interoplib")
    parser_clean.set_defaults(func=clean)

    args = parser.parse_args()
    if not hasattr(args, 'func'):
        args = parser.parse_args(['build'] + sys.argv[1:])

    args.func(args)

if __name__ == '__main__':
    LOG = init_log('root')
    os.environ['LANG'] = "C.UTF-8"
    main()
