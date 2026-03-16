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
import logging
import multiprocessing
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
INTEROPLIB_OUT = os.path.join(INTEROPLIB_DIR, 'output')
DYLIB_EXT = "dylib" if IS_DARWIN else "so"

INTEROPLIB_OBJCLIB_DIR = os.path.join(INTEROPLIB_DIR, 'src', 'objclib')

RELEASE = "release"
INTEROPLIB_NAME_IN_TOML = "interoplib"
OBJC_NAME_IN_TOML = "objc"
INTEROPLIB_OUT_PREFIX = os.path.join(INTEROPLIB_OUT, RELEASE, INTEROPLIB_NAME_IN_TOML)
OBJC_OUT_PREFIX = os.path.join(INTEROPLIB_OUT, RELEASE, OBJC_NAME_IN_TOML)

OUT_INTEROPLIB_COMMON_DYLIB = os.path.join(INTEROPLIB_OUT_PREFIX, f"libinteroplib.common.{DYLIB_EXT}")
OUT_INTEROPLIB_COMMON_CJO   = os.path.join(INTEROPLIB_OUT_PREFIX, "interoplib.common.cjo")

OUT_INTEROPLIB_OBJC_DYLIB = os.path.join(INTEROPLIB_OUT_PREFIX, f"libinteroplib.objc.{DYLIB_EXT}")
OUT_INTEROPLIB_OBJC_CJO   = os.path.join(INTEROPLIB_OUT_PREFIX, "interoplib.objc.cjo")

OUT_OBJC_LANG_DYLIB = os.path.join(OBJC_OUT_PREFIX, f"libobjc.lang.{DYLIB_EXT}")
OUT_OBJC_LANG_CJO   = os.path.join(OBJC_OUT_PREFIX, "objc.lang.cjo")

OUT_INTEROPLIB_OBJCLIB_DYLIB = os.path.join(INTEROPLIB_OUT_PREFIX, f"libinteroplib.objclib.{DYLIB_EXT}")

LOG_DIR = os.path.join(BUILD_DIR, 'logs')
LOG_FILE = os.path.join(LOG_DIR, 'ObjCInteropGen.log')

def log_output(output):
    """log command output"""
    while True:
        line = output.stdout.readline()
        if not line:
            output.communicate()
            returncode = output.returncode
            if returncode != 0:
                LOG.error('build error: %d!\n', returncode)
                sys.exit(1)
            break
        try:
            LOG.info(line.decode('ascii', 'ignore').rstrip())
        except UnicodeEncodeError:
            LOG.info(line.decode('utf-8', 'ignore').rstrip())

def init_log(name):
    """init log config"""
    if not os.path.exists(LOG_DIR):
        os.makedirs(LOG_DIR)

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

def command(*args, cwd=None, env=None):
    """Execute a child program via 'subprocess.Popen' and log the output"""
    output = subprocess.Popen(args, stdout=PIPE, cwd=cwd, env=fixedEnv(env))
    log_output(output)
    if output.returncode:
        fatal('"' + ' '.join(args) + '" returned ' + output.returncode)

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

def build(args):
    """interoplib or objc-interop-gen build"""
    if args.target:
        runtime = runtime_name(args.target)
        LOG.info('begin build interoplib for ' + runtime + '\n')

        # cj-code of interoplib is built by cjpm
        CJPM_CONFIG = "--cfg_darwin_objc" if IS_DARWIN else "--cfg_linux_objc"

        cjpm_env = os.environ.copy()
        # interoplib/cjpm.toml passes these _OPTION to cjc via CJPM_CONFIG
        if args.target_lib:
            cjpm_target = args.target_lib
            if cjpm_target == "ios-aarch64":
                cjpm_target = "arm64-apple-ios11"
            if cjpm_target == "ios-simulator-aarch64":
                cjpm_target = "arm64-apple-ios11-simulator"
            cjpm_env["TARGET_OPTION"] = "--target=" + cjpm_target
        if args.target_sysroot:
            cjpm_env["SYSROOT_OPTION"] = "--sysroot=" + args.target_sysroot
        # target_toolchain is not used for cjpm

        command("cjpm", "build", "--target-dir=" + INTEROPLIB_OUT, CJPM_CONFIG, cwd=INTEROPLIB_DIR, env=cjpm_env)

        # objc-code of interoplib is built by clang
        CANGJIE_HOME=os.environ['CANGJIE_HOME']
        CANGJIE_RUNTIME_LIB_PATH=f"{CANGJIE_HOME}/runtime/lib/{runtime}"

        clang_command = ["clang", "-shared"]
        if args.target_lib:
            clang_target = args.target_lib
            if clang_target == "ios-aarch64":
                clang_target = "arm64-apple-ios11"
            if clang_target == "ios-simulator-aarch64":
                clang_target = "arm64-apple-ios11-simulator"
            clang_command += [f"--target={clang_target}"]
        if args.target_sysroot:
            clang_command += [f"-isysroot{args.target_sysroot}"]
        if IS_DARWIN:
            clang_command += ["-lobjc"]
        else:
            clang_command += subprocess.run(['gnustep-config', '--objc-flags'], capture_output=True).stdout.decode().split()
            clang_command += subprocess.run(['gnustep-config', '--objc-libs'], capture_output=True).stdout.decode().split()

        clang_command += [f"-L{CANGJIE_RUNTIME_LIB_PATH}", "-lcangjie-runtime"]
        clang_command += ["-I.", "cjinterop.m", f"-o{OUT_INTEROPLIB_OBJCLIB_DYLIB}"]

        clang_env = os.environ.copy()
        if args.target_toolchain:
            clang_env['PATH'] = f"{args.target_toolchain}:{clang_env['PATH']}"

        output = subprocess.Popen(
            clang_command.copy(),
            env=clang_env,
            cwd=INTEROPLIB_OBJCLIB_DIR,
            stdout=PIPE,
        )
        log_output(output)

        LOG.info('end build interoplib for ' + runtime + '\n')
    else:
        LOG.info('begin build objc-interop-gen...\n')

        # Add the tinytoml third-party library for acquisition and customize code application.
        tinytoml_target = os.path.join(HOME_DIR, "third_party", "tinytoml", "toml.h")
        if not os.path.exists(tinytoml_target):
            download_and_patch_tinytoml()

        command("cmake", "-B", CMAKE_BUILD_DIR, '-DCMAKE_BUILD_TYPE=' + args.build_type.value, cwd=OBJC_INTEROP_GEN_DIR)

        command("cmake", "--build", CMAKE_BUILD_DIR, cwd=OBJC_INTEROP_GEN_DIR)

        LOG.info('end build objc-interop-gen\n')


def clean(args):
    """clean build outputs and logs"""
    LOG.info("begin clean objc-interop-gen...\n")
    if os.path.isdir(CMAKE_BUILD_DIR):
        shutil.rmtree(CMAKE_BUILD_DIR, ignore_errors=True)
    LOG.info("end clean objc-interop-gen\n")

    LOG.info("begin clean interoplib...\n")
    command("cjpm", "clean", "--target-dir=" + INTEROPLIB_OUT, cwd=INTEROPLIB_DIR)
    LOG.info("end clean interoplib\n")

def prepare_dir(base_path, *relative_path):
    """
    Insure that the directory specified by the arguments exists (create it if it
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

        installation_dir = prepare_dir(install_path, "runtime", "lib", runtime)
        install_files(installation_dir,
                      OUT_INTEROPLIB_COMMON_DYLIB,
                      OUT_INTEROPLIB_OBJC_DYLIB,
                      OUT_OBJC_LANG_DYLIB,
                      OUT_INTEROPLIB_OBJCLIB_DYLIB)
        if IS_DARWIN:
            change_install_names(os.path.join(installation_dir, "libinteroplib.common.dylib"), [])
            change_install_names(
                os.path.join(installation_dir, "libinteroplib.objc.dylib"),
                ["libinteroplib.common.dylib"]
            )
            change_install_names(
                os.path.join(installation_dir, "libobjc.lang.dylib"),
                ["libinteroplib.common.dylib", "libinteroplib.objc.dylib"]
            )
            change_install_names(os.path.join(installation_dir, "libinteroplib.objclib.dylib"), [])

        install_files(
            prepare_dir(install_path, "modules", runtime),
            OUT_INTEROPLIB_COMMON_CJO,
            OUT_INTEROPLIB_OBJC_CJO,
            OUT_OBJC_LANG_CJO
        )

        LOG.info("end install interoplib for " + runtime + "\n")
    else:
        LOG.info("begin install objc-interop-gen...")

        install_file(prepare_dir(install_path, "tools", "bin"), os.path.join(CMAKE_BUILD_DIR, "ObjCInteropGen"))

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
