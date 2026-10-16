# Cangjie-Java 互操作开发指南

## 开源项目介绍

Cangjie-Java 互操作 SDK (软件开发工具包) 包括：

1. cjc：指代仓颉编译器，支持 Cangjie 源码的编译和胶水代码的生成。
2. `java-mirror-generator.jar`：java工具包，用于根据 Java 的 .class 文件自动生成仓颉格式的 Mirror Type。
3. 互操作库: 胶水代码依赖的互操作库, 定义了 java 基本类型的镜像类型定义，包括 `JObject`, `JString`, `JArray<T>`。

![Cangjie-Java互操作架构图](../figures/Cangjie-Java.png)

本文主要介绍java-mirror-generator.jar工具和互操作库的开发指南。

## 目录

Cangjie-Java 互操作 SDK 源码目录如下图所示，其主要功能如注释中所描述。

```
|-- build   # 构建脚本
|-- doc     # 介绍文档
|-- src     # 源码文件
    |-- interoplib # 互操作库
    |-- java-mirror-gen # 仓颉镜像文件生成工具
|-- test # 测试用例
```

## 安装和使用指导

### 构建准备

以下是一个 Ubuntu22 环境下的构建指导。

1. 下载并解压最新仓颉包，配置仓颉环境：

Cangjie-Java 互操作 SDK 构建依赖以下工具：

- `Cangjie SDK`: 构建参考 [cangjie SDK](https://gitcode.com/Cangjie/cangjie_build)
- `java-mirror-generator.jar` 构建依赖
    - 请使用 `JDK 17` 或更新的版本
    - 请使用 `Apache Ant 1.10.12` 或更新的版本
- 使用 `python` 构建脚本方式编译，请安装 `python3`。


### 构建步骤

1. 克隆本项目：

    ```shell
    git clone https://gitcode.com/Cangjie/cangjie_multiplatform_interop.git
    ```

2. 安装依赖：

    假设 `Cangjie SDK` 构建好的目录为 `/home/user/sdks/cangjie`，JDK 安装目录为 `/home/user/sdks/java/JDK`，Ant 安装目录为 `/home/user/sdks/java/Ant`


3. 通过 `java/build` 目录下的脚本构建 Cangjie-Java 互操作 SDK：

    ```shell
    # 配置环境变量
    source /home/user/sdks/cangjie/envsetup.sh
    export PATH=$PATH:/home/user/sdks/java/JDK/bin:/home/user/sdks/java/Ant/bin
    cd cangjie_multiplatform_interop/java
    
    # 构建 java-mirror-gen.jar
    python3 build/build.py build -t release
    
    # 构建互操作库
    python3 build/build.py build -t release --target-lib linux_x86_64_cjnative --target x86_64-linux-gnu
    ```

    当前支持 `debug`、`release` 两种编译类型，开发者需要通过 `-t` 或者 `--build-type` 指定。

4. 安装到指定目录

    ```shell
    # 安装 java-mirror-gen.jar
    python3 build/build.py install
    # 安装互操作库
    python3 build/build.py install --target linux_x86_64
    ```

    默认安装到 `java/dist` 目录下，支持开发者通过 `install` 命令的参数 `--prefix` 指定安装目录：

    ```shell
    # 安装 java-mirror-gen.jar
    python3 build/build.py install --prefix ./output
    # 安装互操作库
    python3 build/build.py install --target linux_x86_64 --prefix ./output
    ```

    编译产物目录结构为:

    ```
    output/
    ├── lib
    │   └── library-loader.jar
    │   └── linux_x86_64_cjnative
    │       ├── libjava.internal.a
    │       └── libjava.lang.a
    ├── modules
    │   └── linux_x86_64_cjnative
    │       ├── java.internal.cjo
    │       └── java.lang.cjo
    ├── runtime
    │   └── lib
    │       └── linux_x86_64_cjnative
    │           ├── libjava.internal.so
    │           └── libjava.lang.so
    └── tools
        └── bin
            └── java-mirror-gen.jar
    ```

5. 清理编译中间产物：

   ```shell
   python3 build/build.py clean
   ```

### 更多构建选项

`python3 build/build.py build` 命令也支持其他参数设置，具体可通过如下命令来查询：

```bash
python3 build/build.py build -h
```

## 相关仓

- [cangjie 仓](https://gitcode.com/Cangjie/cangjie_compiler)
- [cangjie SDK](https://gitcode.com/Cangjie/cangjie_build)
