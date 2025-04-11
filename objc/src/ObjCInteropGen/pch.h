#include <deque>
#include <filesystem>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_set>

#pragma warning(push)
#pragma warning(disable : 4805) // '|': unsafe mix of type 'unsigned int' and type 'bool' in operation
#include <clang-c/Index.h>
#include <clang/AST/DeclObjC.h>
#pragma warning(pop)

#include "toml.hpp"
