# Cangjie-ObjC 互操作开发指南

## 开源项目介绍

Cangjie-ObjC 互操作 SDK (软件开发工具包) 包括：

1. cjc：指代仓颉编译器，支持 Cangjie 源码的编译和胶水代码的生成。详情参看[cangjie](https://gitcode.com/Cangjie/cangjie_compiler)
2. `ObjCInteropGen`：镜像类型生成器，用于根据 Objective-C 的 .h 文件自动生成仓颉格式的 Mirror Type。
3. 互操作库: 胶水代码依赖的互操作库,定义了注册、运行时的一些接口。

![Cangjie-ObjC互操作架构图](../figures/Cangjie-ObjC.png)

本文主要介绍ObjCInteropGen和互操作库的开发指南。

## 目录

`ObjCInteropGen`镜像类型生成工具源码目录如下所示，其主要功能如注释中所描述。

```text
objc
|-- build  # 构建脚本
|-- doc    # 介绍文档
|-- src    #源码文件
    |-- interoplib    # 互操作库
    |-- ObjCInteropGen    # 仓颉镜像生成器源码
|-- test    # 测试用例
|-- third_party    # toml 文件读写功能代码
```

## 安装和使用指导

### 构建前准备

`ObjCInteropGen`和互操作库构建前需要准备以下工具：

- `Cangjie SDK`: 构建参考 [cangjie SDK](https://gitcode.com/Cangjie/cangjie_build)
    - 请确保先`source`仓颉SDK下envsetup.sh脚本
- `cmake`
    - 系统里需要有`cmake`工具
- `libclang`
    - 在macOS和Linux上，必须安装libclang开发环境
- 使用 `python` 构建脚本方式编译，请安装 `python3`

### 构建步骤

开发者可以使用 `objc/build` 目录下的构建脚本 `build.py` 进行 `ObjCInteropGen` 和 `interoplib`构建。

#### 构建ObjCInteropGen

1. 执行 `objc/build` 目录下的构建脚本 `build.py` 构建 `objc`：

    ```shell
    cd ${WORKDIR}/cangjie_multiplatform_interop/objc/build     
    python3 build.py build -t release        #构建生成器
    python3 build.py install            #安装生成器
    python3 build.py clean           #清理构建产物  
    ```

    开发者在build时需要通过 `-t, --build-type` 指定构建产物版本，可选值为 `debug/release`。

    安装成功后，`ObjCInteropGen` 可执行文件会生成在 `objc/dist/tools/bin` 目录中。

2. 修改环境变量：

    ```shell
    export PATH=${WORKDIR}/cangjie_multiplatform_interop/objc/dist/tools/bin:$PATH
    ```

3. 验证 `ObjCInteropGen` 是否安装成功：

    ```shell
    ObjCInteropGen
    ```

    如果输出 `ObjCInteropGen` 的帮助信息，则表示安装成功。

#### 构建互操作库

1. 执行 `objc/build` 目录下的构建脚本 `build.py` 构建`interoplib`：

    ```shell
    python3 build.py build --target=linux_x86_64        #构建互操作库
    python3 build.py install --target=linux_x86_64        #安装互操作库
    python3 build.py clean           #清理构建产物
    ```
    在build和install时需要通过 `--target` 指定当前系统架构。

2. 构建产物所在目录

    ```
    objc
    |-- dist
        |-- lib/linux_x86_64_cjnative
            |-- libobjc.internal.a
            |-- libobjc.lang.a
        |-- modules/linux_x86_64_cjnative
            |-- objc.internal.cjo
            |-- objc.lang.cjo
        |-- runtime/lib/linux_x86_64_cjnative
            |-- libobjc.internal.so
            |-- libobjc.lang.so
    ```
    lib产物将会在cangjie-compiler中使用。

#### 更多构建选项

`build.py` 的 `build` 功能提供如下额外选项：
- `-t, --build-type BUILD_TYPE`: 指定构建产物版本，可选值为 `debug/release`；
- `--target-toolchain`: 在构建interoplib时，使用指定路径下的工具进行交叉编译（应指向bin目录）；
- `--target-sysroot`: 在构建interoplib时，使用`--sysroot`将此参数传递给仓颉编译器；
- `-h, --help`: 打印 `build` 的帮助信息。

`build.py` 还提供如下额外功能：

- `insatall [--prefix PREFIX]`: 将构建产物安装到指定路径，不指定路径时默认为 `objc/dist/` 目录；`install` 前需要先正确执行 `build`；
- `install [-h, --help]`: 打印 `install` 的帮助信息；
- `-h, --help`: 打印 `build.py` 的帮助信息。

### ObjCInteropGen使用指导

`ObjCInteropGen` 提供以下主要命令和 `toml` 配置文件，用于配置Objective-C项目的一些信息。

#### 使用前准备

- 使用前请确认IOS框架和库的头文件路径，这些框架和库中定义了你将要生成镜像的类的所有依赖项。包括IOS标准库头文件的位置，通常Objective-C编译器在构建项目时会查找头文件的所有目录。

#### 命令介绍

执行 `ObjCInteropGen` 后，会打印 `ObjCInteropGen` 的命令列表，如下所示：

```text
Usage: ObjCInteropGen [-v] config-file.toml
    -v
        increase logging verbosity level (can be applied multiple times)
```

常用命令介绍如下：

1. `-v` 命令：

    生成详细输出。

2. `config-file.toml` 文件：

    配置文件的路径名。

#### 配置介绍

配置文件 `config-file.toml` 用于配置一些需要生成镜像文件输入、输出、包等信息，`ObjCInteropGen` 主要通过这个文件进行解析执行。

##### 配置示例

```toml
[output-roots.default]
path = "generated"

[sources.all]
paths = ["test.h"]

[sources-mixins.default]
sources = [".*"]
arguments-append = [
    "-F", "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks",
    "-isystem", "/Library/Developer/CommandLineTools/usr/lib/clang/17/include",
    "-isystem", "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include"
]

[[packages]]
filters = { include = ["NS.+"] }
package-name = "objc.foundation"
```

##### 配置字段说明

1. 输出目录根路径

    - **`output-roots`** 字段：

       `[output-roots]` 表定义了本地文件系统中某个目录路径的名称，镜像生成器将使用该路径作为一个或多个特定包输出目录的公共根路径，这些输出目录通过 `[packages]` 中的 `output-root` 属性来设置。

       示例

        ```toml
        [output-roots.lib]
        path = "./lib/src"
        
        [output-roots.app]
        path = "./main/src"

        [[packages]]
        package-name = "com.vendor1.lib1"
        output-root = "lib"  # Output to "./lib/src/com/vendor1/lib1"
        filters = ...

        [[packages]]
        package-name = "com.mycompany.app"
        output-root = "app"  # Output to "./main/src/com/mycompany/app"
        filters = ...
        ```
        
2. 源文件

    - **`sources`** 字段：

        `[sources]`表中指定了一组单独的头文件，这些头文件是镜像生成器的输入。
        
        **Properties:**

        - **`paths`** (必选)
        
        是单个头文件路径名的字符串数组，镜像生成器必须读取这些文件。

        - **`arguments`** (可选))

        定义clang选项的字符串数组，镜像生成器在处理`paths`中列出的源文件时会传递这些选项。

        示例

        ```toml
        [sources.all]
        paths = ["original-objc/M.h"]
        ```        
    - `source-mixins`字段
    
        `[source-mixins]`表中`sources`属性会定义一个正则表达式，用来匹配 `[sources]` 表，在处理匹配上的表中头文件时必须把额外的参数传递给clang。

        **Properties:**

        - **`sources`** (必选)
        
        包含正则表达式的字符串。

        - **`arguments-prepend`**  
          **`arguments-append`** (可选)

        镜像生成器在处理源文件时，会将该字段值作为选项传递给clang。`arguments-prepend`和`arguments-append`的区别是，这两个属性列出的选项将分别放在 `[sources]` 表`arguments`属性指定选项的之前或之后传递。

        示例

        ```toml
        [sources.UIWidgets]
        paths = ["objc/UIWidgets.h"]
        arguments = [ "-I", "/usr/local/include/share/Widgets" ]

        [sources.UIPanels]
        paths = ["objc/UIPanels.h"]
        arguments = [ "-I", "/usr/local/include/share/Panels" ]

        [sources-mixins.UI]
        # Add these Clang arguments for both UIWidgets and UIPanels
        sources = ["UI.+"]
        arguments-append = [
            "-I", "/usr/local/include/Frameworks/AcmeUI"
        ]
      ```

3. 包

   - **`packages`** 字段：

        `[[packages]]` 数组中的每个条目指定了一个目标仓颉包名称，定义哪些Objective-C实体将会被镜像到这个包中，以及指定在该包中的输出目录（可选）。
        
        **Properties:**
        
        - **`package-name`** (必选)
        
        目标仓颉包名称。

        - **`output-path`** (可选)
        
        指定输出文件在包中的输出目录路径。如果该路径名中有不存在的目录，镜像生成器将尝试创建它们。

        - **`output-root`** (可选)
        
        设置输出文件所在的根目录。

        如果没有设置`output-root`，并且只有一个元素在 `[output-roots]` 中，将使用该条目中的路径名。否则，将显示错误。

        示例
        
        ```toml
        [output-roots.main]
        path="./cj-mirrors"

        [[packages]]
        package-name = "objc.foundation"
        output-root = "main"

        # 输出文件会被放在：./cj-mirrors/objc/foundation.
        ```
        - **`filters`** (必选)
        
        定义了一组名称过滤器，这些过滤器定义了源文件中哪些Objective-C实体声明必须镜像到给定仓颉包中。详情参看[名称过滤器](#####名称过滤器)
            
        示例
        
        ```toml
        # The Foundation framework
        [[packages]]
        package-name = "objc.foundation"
        filters = { include = "NS.+" }
        ```
            
4. 类型替换

    - **`mappings`** 字段：

        `[[mappings]]`是一个表数组，可以把Objective-C类型替换为另一种类型。几乎所有类型（除了`int`之类的C基元类型）都可以被替换为的另一种类型。

        示例
        
        ```toml
        # Replace the type id, the root of the Objective-C type hierarchy,
        # with NSObjectProtocol everywhere.
        [[mappings]]
        id = "NSObjectProtocol"
        ```

5. 配置文件的引入

    示例

    ```toml
    import = "../common.toml"
    ```
#### 生成效果示例

 **示例**

Objective-C源码：

```c
@interface A
- (void)foo;
- (void)foo:(int)i;
- (void)foo:(int)i bar:(int) j;
- (void)foo:(int)i bar:(int) j baz:(int) k;
@end
```

生成的Cangjie镜像类：

```
@ObjCMirror
public open class A <: id {
    public open func foo(): Unit
    @ForeignName["foo:"] public open func foo(i: Int32): Unit
    @ForeignName["foo:bar:"] public open func fooBar(i: Int32, j: Int32): Unit
    @ForeignName["foo:bar:baz:"] public open func fooBarBaz(i: Int32, j: Int32, k: Int32): Unit
}
```

#### 其他

##### 名称过滤器

每个名称过滤器是一个包含以下内容的 TOML 表：
  - 包含以下属性之一：`include`、`exclude`、`union`、`intersect` 和 `not`，
  - 可选地包含 `filter` 属性或 `filter-not` 属性。
下面是这些属性的详细介绍。

**`include`**

该值可以是一个正则表达式或者一个数组。 
如果值是一个单一的正则表达式，则只有匹配该正则表达式的名称才能通过过滤器。如果值是一个数组，则名称只需匹配数组中的任意一个正则表达式即可成功通过。

示例

```toml
# Only include entities that names of which start with "NS":
filters = { include = "NS.+" }

# Only include entities with names that either start with "Foo"
# or end with "Bar":
filters = { include = ["Foo.*", ".*Bar"] }
```

**`exclude`**

与 `include` 相反（见上文）。一个名称在给定值下通过 `include` 过滤器时，不会通过具有相同值的 `exclude` 过滤器，反之亦然。

示例

```toml
# Include everything but entities the names of which start with "INTERNAL_":
filters = { exclude = "INTERNAL_.+" }
```

**`union`**

一个将两个或多个过滤器组合在一起的运算符。其值必须是一个过滤器数组，只要通过其中任何一个过滤器的名称，就通过了整个 `union` 过滤器。

示例

```toml
# An equivalent of the second `include` example above:
filters = { union = [ { include = "Foo.*" }, { include = ".*Bar" } ] }
```

**`intersect`**

一个将两个或多个过滤器组合的操作符。其值必须是一个过滤器数组。只有当名称通过所有这些过滤器时，才能通过整个 `intersect` 过滤器。

示例

```toml
// Adding a negative filter:
filters = { intersect = [ { include = "NS.+" },
                          { exclude = "NSAccidentalClash" } ] }
```

**`not`**

一个用于反转过滤器含义的操作符。该值必须是一个单一的过滤器。

示例

```toml
// Another way to add a negative filter:
filters = { intersect = [ { include = "NS.+" },
                          { not = { include = "NSAccidentalClash" } } ] }
```

**`filter`**  
**`filter-not`** (可选)

这些属性必须与其他属性混合使用，即它们不能是 `filters` 表中唯一的属性。它们的值可以是一个正则表达式或数组，就像 `include` 和 `exclude` 属性的值一样。
`filter` 将通过主过滤器筛选的集合缩减为仅包含给定单个正则表达式或数组中任意正则表达式匹配的名称。
`filter-not` 将通过主过滤器筛选的集合缩减为仅包含给定单个正则表达式或数组中任何正则表达式不匹配的名称。
`filter`和`filter-not`实际上是`intersect`操作的简写。

示例

```toml
# Without filter-not:
filters = { intersect = [ { include = "NS.+" },
                          { exclude = "NSAccidentalClash" } ] }
# With filter-not:
filters = { include = "NS.+", filter-not = "NSAccidentalClash" }

# 'filter' and 'filter-not' can be used together:
filters = { include    = ".*Fizz.+",
            filter     = ".+Buzz.*",
            filter-not = ".*FizzBuzz.*" }
```

## 相关仓

- [cangjie](https://gitcode.com/Cangjie/cangjie_compiler)
- [cangjie SDK](https://gitcode.com/Cangjie/cangjie_build)