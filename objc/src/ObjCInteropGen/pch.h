#include <deque>
#include <filesystem>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_set>

#ifdef OBJCGEN_NO_WARNINGS
#pragma warning(push)
#pragma warning(disable : 4100) // unreferenced parameter
#pragma warning(disable : 4146) // unary minus operator applied to unsigned type, result still unsigned
#pragma warning(disable : 4244) // conversion from 'clang::DiagnosticsEngine::ArgumentKind' to 'unsigned char', possible
                                // loss of data
#pragma warning(disable : 4245) // conversion from 'int' to 'size_t', signed/unsigned mismatch
#pragma warning(disable : 4267) // conversion from 'size_t' to 'const int', possible loss of data
#pragma warning(disable : 4291) // no matching operator delete found
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#pragma warning(disable : 4389) // signed/unsigned mismatch
#pragma warning(disable : 4458) // declaration of 'EC' hides class member
#pragma warning(disable : 4805) // unsafe mix of type 'unsigned int' and type 'bool' in operation
#endif
#include <clang-c/Index.h>
#include <clang/AST/DeclObjC.h>
#ifdef OBJCGEN_NO_WARNINGS
#pragma warning(pop)
#endif

#if defined OBJCGEN_NO_WARNINGS && defined __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#include "toml.hpp"
#if defined OBJCGEN_NO_WARNINGS && defined __clang__
#pragma clang diagnostic pop
#endif
