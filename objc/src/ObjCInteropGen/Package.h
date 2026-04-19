// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef SCOPE_H
#define SCOPE_H

#include <cassert>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Symbol.h"

namespace objcgen {

class Package;

class PackageFilter : NonCopyable {
protected:
    explicit PackageFilter(const Package& package) noexcept : package_(package)
    {
    }

public:
    virtual ~PackageFilter() = default;

    [[nodiscard]] virtual bool apply(std::string_view entity_name) const = 0;

    [[nodiscard]] const Package& package() const noexcept
    {
        return package_;
    }

    [[nodiscard]] const std::string& package_name() const noexcept;

private:
    const Package& package_;
};

class PackageFile final {
    std::string file_name_;
    std::filesystem::path output_path_;
    Package* const package_;
    std::vector<FileLevelSymbol*> symbols_;

public:
    PackageFile(std::string file_name, Package& package);

    [[nodiscard]] std::string_view file_name() const
    {
        return file_name_;
    }

    [[nodiscard]] const std::filesystem::path& output_path() const noexcept
    {
        return output_path_;
    }

    [[nodiscard]] Package& package() const noexcept
    {
        return *package_;
    }

    void add_symbol(FileLevelSymbol& symbol)
    {
        assert(symbol.is_file_level());
        symbols_.push_back(&symbol);
    }

    [[nodiscard]] auto begin() const noexcept
    {
        return symbols_.begin();
    }

    [[nodiscard]] auto end() const noexcept
    {
        return symbols_.end();
    }
};

using package_file_map_t = std::unordered_map<std::string, PackageFile*>;

class PackageFilesIterator final {
    package_file_map_t::const_iterator it_;

public:
    explicit PackageFilesIterator(package_file_map_t::const_iterator it) noexcept : it_(it)
    {
    }

    [[nodiscard]] PackageFile& operator*() const noexcept
    {
        return *it_->second;
    }

    PackageFilesIterator& operator++()
    {
        ++it_;
        return *this;
    }

    friend bool operator!=(const PackageFilesIterator& lhs, const PackageFilesIterator& rhs) noexcept
    {
        return lhs.it_ != rhs.it_;
    }
};

class Package final : NonCopyable {
    const std::string cangjie_name_;
    const std::string output_path_;
    const PackageFilter* filters_ = nullptr;
    package_file_map_t files_;
    std::unordered_set<Package*> depends_on_;

public:
    Package(std::string cangjie_name, std::string output_path)
        : cangjie_name_(std::move(cangjie_name)), output_path_(std::move(output_path))
    {
    }

    ~Package() = default;

    [[nodiscard]] const std::string& cangjie_name() const noexcept
    {
        return cangjie_name_;
    }

    [[nodiscard]] const std::string& output_path() const noexcept
    {
        return output_path_;
    }

    [[nodiscard]] const PackageFilter* filters() const noexcept
    {
        return filters_;
    }

    [[nodiscard]] const std::unordered_set<Package*>& depends_on() const noexcept
    {
        return depends_on_;
    }

    void set_filters(const PackageFilter& filters) noexcept
    {
        assert(!filters_);
        filters_ = &filters;
    }

    void add_file(PackageFile& file)
    {
        [[maybe_unused]] auto [_, inserted] = files_.try_emplace(std::string(file.file_name()), &file);
        assert(inserted);
    }

    void add_dependency_edge(Package& package)
    {
        assert(&package != this);
        depends_on_.emplace(&package);
    }

    [[nodiscard]] PackageFile* operator[](const std::string& name) const
    {
        const auto it = files_.find(name);
        return it == files_.end() ? nullptr : it->second;
    }

    [[nodiscard]] PackageFile* operator[](const std::string_view name) const
    {
        return (*this)[std::string(name)];
    }

    [[nodiscard]] PackageFilesIterator begin() const noexcept
    {
        return PackageFilesIterator{files_.begin()};
    }

    [[nodiscard]] PackageFilesIterator end() const noexcept
    {
        return PackageFilesIterator{files_.end()};
    }
};

using package_map_t = std::unordered_map<std::string, Package*>;

class PackagesIterator final {
    package_map_t::const_iterator it_;

public:
    explicit PackagesIterator(package_map_t::const_iterator it) noexcept : it_(it)
    {
    }

    [[nodiscard]] Package& operator*() const noexcept
    {
        return *it_->second;
    }

    PackagesIterator& operator++() noexcept
    {
        ++it_;
        return *this;
    }

    [[nodiscard]] friend bool operator!=(const PackagesIterator& lhs, const PackagesIterator& rhs) noexcept
    {
        return lhs.it_ != rhs.it_;
    }
};

class Packages final {
public:
    void insert(Package& package)
    {
        const auto& cangjie_name = package.cangjie_name();
        assert(by_cangjie_name_.find(cangjie_name) == by_cangjie_name_.end());
        by_cangjie_name_.try_emplace(cangjie_name, &package);
    }

    [[nodiscard]] Package* by_cangjie_name(const std::string_view name) const
    {
        return by_cangjie_name(std::string(name));
    }

    [[nodiscard]] Package* by_cangjie_name(const std::string& name) const
    {
        const auto it = by_cangjie_name_.find(name);
        return it == by_cangjie_name_.end() ? nullptr : it->second;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return by_cangjie_name_.size();
    }

    [[nodiscard]] PackagesIterator begin() const noexcept
    {
        return PackagesIterator{by_cangjie_name_.begin()};
    }

    [[nodiscard]] PackagesIterator end() const noexcept
    {
        return PackagesIterator{by_cangjie_name_.end()};
    }

private:
    package_map_t by_cangjie_name_;
};

extern Packages packages;

void create_packages();

} // namespace objcgen

#endif // SCOPE_H
