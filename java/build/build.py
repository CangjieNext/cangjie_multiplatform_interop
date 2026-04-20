#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

"""Cangjie Java interoperability build script"""
import argparse
import datetime
import glob
import logging
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import zipfile
from enum import Enum
from logging.handlers import TimedRotatingFileHandler
from subprocess import PIPE

if 'JAVA_HOME' not in os.environ:
    print("JAVA_HOME environment variable must be set to JDK 17 or greater", file=sys.stderr)
    exit(1)

if 'CANGJIE_HOME' not in os.environ:
    print("CANGJIE_HOME environment variable must be set (are you in Cangjie SDK environment?)", file=sys.stderr)
    exit(1)

# The date used as modification time for all JAR contents
JAR_CONTENTS_EPOCH = (2023, 1, 2, 3, 4, 5)

IS_DARWIN = platform.system() == "Darwin"
IS_WINDOWS = platform.system() == "Windows"
JAVA_HOME = os.environ['JAVA_HOME']
JAVA_INCLUDE = os.path.join(JAVA_HOME, 'include')
JAVA_INCLUDE_ARCH = os.path.join(JAVA_INCLUDE, 'darwin') if IS_DARWIN else os.path.join(JAVA_INCLUDE, 'win32') if IS_WINDOWS else os.path.join(JAVA_INCLUDE, 'linux')

CJ_HOME = os.environ['CANGJIE_HOME']
CJ_RUNTIME_LIB = os.path.join(CJ_HOME, 'runtime', 'lib')

BUILD_DIR = os.path.dirname(os.path.abspath(__file__))
HOME_DIR = os.path.dirname(BUILD_DIR)
REPACK_DIR = os.path.join(BUILD_DIR, 'jar-repack')

MIRROR_GEN_DIR = os.path.join(HOME_DIR, 'src', 'java-mirror-gen')
MIRROR_GEN_JAR = os.path.join(MIRROR_GEN_DIR, 'java-mirror-gen.jar')
DIST_DIR = os.path.join(HOME_DIR, 'dist')
DEFAULT_INSTALL_DIR = os.path.join(DIST_DIR, 'install')

INTEROPLIB_DIR = os.path.join(HOME_DIR, 'src', 'interoplib')
LIBRARY_LOADER_JAR = "library-loader.jar"
JAVA_INTEROP_THIRD_PARTY = os.path.join(MIRROR_GEN_DIR, 'third_party')

OUT_INTEROPLIB_CJO = os.path.join(DIST_DIR, "interoplib.interop.cjo")
OUT_JAVA_LANG_CJO = os.path.join(DIST_DIR, "java.lang.cjo")

LOG_DIR = os.path.join(BUILD_DIR, 'logs')
LOG_FILE = os.path.join(LOG_DIR, 'JavaInterop.log')

CJC_BASE_ARGS = ["-Woff", "unused", "-Woff", "parser", "--strip-all", "-O2", "--output-dir=" + DIST_DIR, "--int-overflow=wrapping", "--disable-reflection"]
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
    env["SOURCE_DATE_EPOCH"] = "0"
    env["ZERO_AR_DATE"] = "1"
    return env

def check_clang(args):
    # If user didn't specify --target-toolchain, we search for an available compiler in $PATH.
    # If user did specify --target-toolchain, we search in user given path ONLY. By doing so
    # user could see a proper 'compiler not found' error if the given path is incorrect.
    toolchain_path = args.target_toolchain if args.target_toolchain else None
    if toolchain_path and (not os.path.exists(toolchain_path)):
        LOG.error(f"The given toolchain path does not exist: {toolchain_path}")

    c_compiler = shutil.which("clang", path=toolchain_path)

    if c_compiler is None:
        if toolchain_path:
            LOG.error(f"Cannot find clang in the given toolchain path: {toolchain_path}")
        else:
            LOG.error("Cannot find clang in $PATH")
        fatal("clang is required to build interop libraries")

    return c_compiler

def command(*args, cwd=None, env=None):
    """Execute a child program via 'subprocess.Popen' and log the output"""
    output = subprocess.Popen(args, stdout=PIPE, cwd=cwd, env=fixedEnv(env))
    log_output(output)
    if output.returncode:
        fatal('"' + ' '.join(args) + '" returned ' + output.returncode)

def dylib_ext(target):
    if "darwin" in target or "ios" in target:
        return "dylib"
    elif "windows" in target:
        return "dll"
    else:
        return "so"

def repack_jar_stable(jar):
    tmp_dir = os.path.join(REPACK_DIR, f"{pathlib.Path(jar).stem}-unpacked")
    tmp_jar = os.path.join(REPACK_DIR, pathlib.Path(jar).name)
    if os.path.isdir(tmp_dir):
        shutil.rmtree(tmp_dir, ignore_errors=True)
    shutil.unpack_archive(jar, tmp_dir, "zip")

    # (atime, mtime)
    timestamp = datetime.datetime(*JAR_CONTENTS_EPOCH).timestamp()
    timestamps = (timestamp, timestamp)
    for root, dirs, files in os.walk(tmp_dir):
        for d in dirs:
            os.utime(os.path.join(root, d), timestamps)
        for f in files:
            os.utime(os.path.join(root, f), timestamps)

    # Create a new deterministic zip
    with zipfile.ZipFile(tmp_jar, 'w', compression=zipfile.ZIP_DEFLATED) as zip:
        for root, dirs, files in os.walk(tmp_dir):
            # Sort dirs and files to ensure deterministic order
            dirs.sort()
            files.sort()

            for file in files:
                file_path = os.path.join(root, file)
                arcname = os.path.relpath(file_path, tmp_dir)

                # Create ZipInfo with fixed timestamp
                info = zipfile.ZipInfo(arcname)
                info.date_time = JAR_CONTENTS_EPOCH
                info.compress_type = zipfile.ZIP_DEFLATED

                with zip.open(info, 'w') as dest, open(file_path, 'rb') as src:
                    shutil.copyfileobj(src, dest)

    os.replace(tmp_jar, jar)
    shutil.rmtree(tmp_dir, ignore_errors=True)

def copy_with_exclusions(src, dst, exclusions):
    """Copy the folder while excluding specified files and folders"""
    os.makedirs(dst, exist_ok=True)

    # Traverse the source directory tree
    for root, dirs, files in os.walk(src):
        # Calculate the relative path
        rel_root = os.path.relpath(root, src)

        # Check if the current directory is in the exclusion list.
        skip_dir = False
        for exclusion in exclusions:
            if rel_root == exclusion or rel_root.startswith(exclusion + os.sep):
                skip_dir = True
                break

        if skip_dir:
            # Remove subdirectories from the traversal
            dirs[:] = []
            continue

        # Creating a target directory
        dest_dir = os.path.join(dst, rel_root)
        os.makedirs(dest_dir, exist_ok=True)

        # Copy files
        for file in files:
            src_file = os.path.join(root, file)
            dest_file = os.path.join(dest_dir, file)

            # Check if the file is in the exclusion list
            rel_file = os.path.join(rel_root, file)
            skip_file = False
            for exclusion in exclusions:
                if rel_file == exclusion or rel_file.startswith(exclusion + os.sep):
                    skip_file = True
                    break

            if not skip_file:
                shutil.copy2(src_file, dest_file)

def fetch_jdk(target_dir):
    """Download only required folders from bishengjdk-21 repository"""
    repo_url = "https://gitcode.com/openeuler/bishengjdk-21"
    tag_name = "jdk-21.0.8-ga-b011"
    required_folders_src = ["jdk.compiler", "java.compiler"]
    required_folders_tools = ["anttasks", "compileproperties", "propertiesparser"]

    # Define files and folders to exclude
    exclusions_src = {
        "java.compiler": [
            "share/classes/module-info.java"
        ],
        "jdk.compiler": [
            "share/classes/com/sun/tools/javac/launcher",
            "share/classes/jdk",
            "share/classes/module-info.java",
            "share/classes/sun",
            "share/data",
            "share/man"
        ]
    }

    exclusions_tools = {
        "anttasks": [
            "DumpClassesTask.java",
            "GenStubsTask.java",
            "SelectToolTask.java"
        ]
    }

    # Creating a target directory
    jdk_src_dir = os.path.join(target_dir, "jdk", "src")
    jdk_tools_dir = os.path.join(target_dir, "jdk", "make", "langtools", "tools")
    clone_dir = os.path.join(target_dir, "bishengjdk-21")

    if not os.path.exists(clone_dir) and not os.path.exists(jdk_src_dir):
        # Download the BishengJDK
        LOG.info(f'Cloning bishengjdk-21 repository (tag: {tag_name})...\n')
        subprocess.run(
            ["git", "clone", "--depth=1", "-b", tag_name, repo_url, clone_dir],
            stdout=PIPE,
        )

    if not os.path.exists(jdk_src_dir):
        # Source directory exists
        if not os.path.exists(clone_dir):
            LOG.error(f"Source directory does not exist: {clone_dir}")

        # Target durectory exists
        os.makedirs(jdk_src_dir, exist_ok=True)
        LOG.info(f"The target directory has been created: {jdk_src_dir}")
        # Traverse and copy the required folders
        for folder in required_folders_src:
            src_path = os.path.join(clone_dir, "src", folder)
            dest_path_src = os.path.join(jdk_src_dir, folder)
            dest_path_tools = os.path.join(jdk_tools_dir, folder)
            if not os.path.exists(src_path):
                LOG.error(f"The required folder does not exist: {src_path}")
                continue

            # If the target folder already exists, delete it first
            if os.path.exists(dest_path_src):
                LOG.info(f"The target folder already exists and is being deleted: {dest_path_src}")
                shutil.rmtree(dest_path_src)

            # Copy folder
            LOG.info(f"Copying: {src_path} -> {dest_path_src}")
            copy_with_exclusions(src_path, dest_path_src, exclusions_src.get(folder, []))
            LOG.info(f"Copy Successfully: {folder}")

        for folder in required_folders_tools:
            src_path = os.path.join(clone_dir, "make", "langtools", "tools", folder)
            dest_path_tools = os.path.join(jdk_tools_dir, folder)
            if not os.path.exists(src_path):
                LOG.error(f"The required folder does not exist: {src_path}")
                continue

            # If the target folder already exists, delete it first
            if os.path.exists(dest_path_tools):
                LOG.info(f"The target folder already exists and is being deleted: {dest_path_tools}")
                shutil.rmtree(dest_path_tools)

            # Copy folder
            LOG.info(f"Copying: {src_path} -> {dest_path_tools}")
            copy_with_exclusions(src_path, dest_path_tools, exclusions_tools.get(folder, []))
            LOG.info(f"Copy Successfully: {folder}")

        LOG.info(f"Delete temporary clone directory: {clone_dir}")
        shutil.rmtree(clone_dir)
        # Apply Jdk_interop specific modification
        """Apply a patch file to the target directory"""
        patch_path = os.path.join(JAVA_INTEROP_THIRD_PARTY, "jdk_interop.patch")
        if os.path.exists(patch_path):
            patch_cmd = f"patch -p1 -l -f < {patch_path}"
            LOG.info("CMDPATH: %s", HOME_DIR)
            subprocess.run(patch_cmd, shell=True, cwd=HOME_DIR, check=True)
            LOG.info('Patch applied successfully')
        else:
            LOG.info("Warning: jdk_interop.patch not found at %s", HOME_DIR)
    else:
        LOG.info('jdk directory already exists, skipping fetch\n')

def is_android_64bit(target):
    if "android" in target:
        return ("aarch64" in target) or ("x86_64" in target)
    return False

def build(args):
    """Java binding generator or interoplib build"""
    """ target-lib is a marker that interoplib should be built """
    if args.target_lib:
        LOG.info('begin build interoplib for ' + args.target_lib + '\n')

        DYLIB_EXT = dylib_ext(args.target_lib)
        OUT_CINTEROPLIB_O = os.path.join(DIST_DIR, "cinteroplib.o")

        if not os.path.exists(DIST_DIR):
            os.makedirs(DIST_DIR)

        #clang c_core.c
        clang_args = [check_clang(args)]
        if IS_DARWIN:
            clang_args += ["-D_XOPEN_SOURCE=600"]
        if args.target:
            clang_args += ["--target=" + args.target]
            if is_android_64bit(args.target):
                clang_args += ["-Wl,-z,max-page-size=16384"]
        if args.target_sysroot:
            clang_args += ["-isysroot", args.target_sysroot]

        clang_O = clang_args.copy() + ["-c", "-fstack-protector-strong", "-fPIC"]
        clang_O += ["-o", OUT_CINTEROPLIB_O]
        clang_O += ["-I" + JAVA_INCLUDE, "-I" + JAVA_INCLUDE_ARCH]
        clang_O += ["c_core.c"]
        command(*clang_O, cwd=INTEROPLIB_DIR)

        #cjc jni.cj registry.cj
        cjc_args = ["cjc"] + list(CJC_BASE_ARGS)
        if args.target:
            cjc_args += ["--target=" + args.target]
        if args.target_sysroot:
            cjc_args += ["--sysroot", args.target_sysroot]
        if args.target_toolchain:
            cjc_args += ["-B", args.target_toolchain]

        cjc_A = cjc_args.copy() + ["--output-type=staticlib"]
        cjc_SO = cjc_args.copy() + ["--output-type=dylib"]

        command(*(cjc_A.copy() + ["jni.cj", "registry.cj"]), cwd=INTEROPLIB_DIR)
        command(*(cjc_SO.copy() + ["jni.cj", "registry.cj", OUT_CINTEROPLIB_O]), cwd=INTEROPLIB_DIR)

        javalib_args = [f"--import-path={DIST_DIR}"] + list(glob.glob(os.path.join(INTEROPLIB_DIR, "javalib") + "/*.cj", recursive=False))
        command(*(cjc_A.copy() + javalib_args.copy()), cwd=INTEROPLIB_DIR)
        command(*(cjc_SO.copy() + javalib_args.copy() + ["-L" + DIST_DIR, "-linteroplib.interop"]), cwd=INTEROPLIB_DIR)

        command(
            "javac", "-d", DIST_DIR, "-source", "8", "-target", "8", "LibraryLoader.java", "$$NativeConstructorMarker.java", "ClassAnalyser.java", "MethodDef.java",
            cwd=INTEROPLIB_DIR,
        )

        command(
            "jar", "cf",  LIBRARY_LOADER_JAR, "cangjie/lang/LibraryLoader.class", "cangjie/lang/internal/$$NativeConstructorMarker.class", "cangjie/interop/java/ClassAnalyser.class", "cangjie/interop/java/MethodDef.class",
            cwd=DIST_DIR,
        )

        repack_jar_stable(os.path.join(DIST_DIR, LIBRARY_LOADER_JAR))

        LOG.info('end build interoplib for ' + args.target_lib + '\n')
    else:
        LOG.info('begin build java-binding-gen...\n')

        # Fetch jdk before building
        fetch_jdk(JAVA_INTEROP_THIRD_PARTY)

        command("ant", "clean", "build", cwd=MIRROR_GEN_DIR)

        repack_jar_stable(MIRROR_GEN_JAR)

        LOG.info('end build java-binding-gen\n')


def clean(args):
    """clean build outputs and logs"""
    if os.path.isdir(REPACK_DIR):
        shutil.rmtree(REPACK_DIR, ignore_errors=True)
    LOG.info("begin clean java-binding-gen...\n")
    output = subprocess.Popen(
        ["ant", "clean"],
        cwd=MIRROR_GEN_DIR,
        stdout=PIPE,
        env=fixedEnv(),
    )
    log_output(output)
    jdk_dir = os.path.join(JAVA_INTEROP_THIRD_PARTY, "jdk")
    if os.path.isdir(jdk_dir):
        shutil.rmtree(jdk_dir, ignore_errors=True)
    LOG.info("end clean java-binding-gen\n")
    LOG.info("begin clean interoplib...\n")
    if os.path.isdir(DIST_DIR):
        shutil.rmtree(DIST_DIR, ignore_errors=True)
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

def runtime_name(target):
    return target if target.endswith("_cjnative") else target + "_cjnative"

def install(args):
    """install java-binding-gen or interoplib"""
    install_path = os.path.abspath(args.install_prefix) if args.install_prefix else DEFAULT_INSTALL_DIR

    if args.target:
        LOG.info("begin install interoplib for " + args.target + "\n")
        runtime = runtime_name(args.target)

        DYLIB_EXT = dylib_ext(args.target)
        OUT_CINTEROPLIB_O = os.path.join(DIST_DIR, "cinteroplib.o")
        OUT_INTEROPLIB_A = os.path.join(DIST_DIR, "libinteroplib.interop.a")
        OUT_INTEROPLIB_SO = os.path.join(DIST_DIR, f"libinteroplib.interop.{DYLIB_EXT}")
        OUT_JAVA_LANG_A = os.path.join(DIST_DIR, "libjava.lang.a")
        OUT_JAVA_LANG_SO = os.path.join(DIST_DIR, f"libjava.lang.{DYLIB_EXT}")

        DEST_LIB = prepare_dir(install_path, "lib", runtime)
        install_files(
            DEST_LIB,
            OUT_CINTEROPLIB_O,
            OUT_INTEROPLIB_A,
            OUT_JAVA_LANG_A
        )

        DEST_DYLIB = prepare_dir(install_path, "runtime", "lib", runtime)
        install_files(
            DEST_DYLIB,
            OUT_INTEROPLIB_SO,
            OUT_JAVA_LANG_SO
        )

        DEST_CJO = prepare_dir(install_path, "modules", runtime)
        install_files(DEST_CJO, OUT_INTEROPLIB_CJO, OUT_JAVA_LANG_CJO)

        lib_loader_dst = prepare_dir(install_path, 'lib')
        lib_loader_jar = os.path.join(DIST_DIR, LIBRARY_LOADER_JAR)
        install_file(lib_loader_dst, lib_loader_jar)

        LOG.info("end install interoplib for " + args.target + "\n")
    else:
        LOG.info("begin install java-binding-gen...")

        mirror_gen_dst = prepare_dir(install_path, 'tools', 'bin')
        install_file(mirror_gen_dst, MIRROR_GEN_JAR)

        LOG.info("end install java-binding-gen")

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
    parser = argparse.ArgumentParser(description='build Java binding generator or interoplib')
    subparsers = parser.add_subparsers(help='sub command help')
    parser_build = subparsers.add_parser('build', help='build Java binding generator or interoplib')
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

    parser_install = subparsers.add_parser("install", help="install Java binding generator or interoplib")
    parser_install.add_argument(
        "--target", dest="target", type=str,
        help="install interoplib for the specified target"
    )
    parser_install.add_argument('--prefix',
                            dest='install_prefix',
                            help='target install prefix')
    parser_install.set_defaults(func=install)

    parser_clean = subparsers.add_parser("clean", help="clean for both Java binding generator and interoplib")
    parser_clean.set_defaults(func=clean)

    args = parser.parse_args()


    if hasattr(args, 'target'):
        if args.target == "android-aarch64":
            args.target = "aarch64-linux-android31"
        if args.target == "ios-aarch64":
            args.target = "arm64-apple-ios11"
        if args.target == "ios-simulator-aarch64":
            args.target = "arm64-apple-ios11-simulator"

    if not hasattr(args, 'func'):
        args = parser.parse_args(['build'] + sys.argv[1:])

    args.func(args)

if __name__ == '__main__':
    LOG = init_log('root')
    os.environ['LANG'] = "C.UTF-8"
    main()

