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

class PackageFilter {
    const Package* package_;

public:
    explicit PackageFilter(const Package* package) : package_(package)
    {
        assert(package);
    }

    virtual ~PackageFilter() = default;

    [[nodiscard]] virtual bool apply(std::string_view entity_name) const = 0;

    PackageFilter(const PackageFilter& other) = delete;

    PackageFilter(PackageFilter&& other) noexcept = delete;

    PackageFilter& operator=(const PackageFilter& other) = delete;

    PackageFilter& operator=(PackageFilter&& other) noexcept = delete;

    [[nodiscard]] const Package* package() const
    {
        return package_;
    }

    [[nodiscard]] std::string_view package_name() const;
};

class PackageFile final {
    std::string file_name_;
    std::filesystem::path output_path_;
    Package* package_;
    std::vector<FileLevelSymbol*> symbols_;

public:
    PackageFile(std::string file_name, Package* package);

    [[nodiscard]] std::string_view file_name() const
    {
        return file_name_;
    }

    [[nodiscard]] std::filesystem::path output_path() const
    {
        return output_path_;
    }

    [[nodiscard]] Package* package() const
    {
        return package_;
    }

    void add_symbol(FileLevelSymbol* symbol)
    {
        assert(symbol);
        assert(symbol->is_file_level());
        symbols_.push_back(symbol);
    }

    [[nodiscard]] auto cbegin() const
    {
        return symbols_.cbegin();
    }

    [[nodiscard]] auto cend() const
    {
        return symbols_.cend();
    }

    [[nodiscard]] auto begin() const
    {
        return cbegin();
    }

    [[nodiscard]] auto end() const
    {
        return cend();
    }
};

using package_file_map_t = std::unordered_map<std::string, PackageFile*>;

class PackageFilesIterator final {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = PackageFile;
    using pointer = value_type*;   // or also value_type*
    using reference = value_type&; // or also value_type&

    package_file_map_t::const_iterator it_;

    [[nodiscard]] reference get() const;

public:
    explicit PackageFilesIterator(package_file_map_t::const_iterator it) : it_(std::move(it))
    {
    }

    reference operator*() const
    {
        return get();
    }
    pointer operator->() const
    {
        return &get();
    }

    // Prefix increment
    PackageFilesIterator& operator++()
    {
        ++it_;
        return *this;
    }

    // Postfix increment
    PackageFilesIterator operator++(int)
    {
        PackageFilesIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool operator==(const PackageFilesIterator& lhs, const PackageFilesIterator& rhs)
    {
        return lhs.it_ == rhs.it_;
    }

    friend bool operator!=(const PackageFilesIterator& lhs, const PackageFilesIterator& rhs)
    {
        return !(lhs == rhs);
    }
};

class Package final {
    std::string cangjie_name_;
    std::string output_path_;
    const PackageFilter* filters_ = nullptr;
    package_file_map_t files_;
    std::unordered_set<Package*> depends_on_;

public:
    Package(std::string cangjie_name, std::string output_path)
        : cangjie_name_(std::move(cangjie_name)), output_path_(std::move(output_path))
    {
    }

    ~Package() = default;

    Package(const Package& other) = delete;

    Package(Package&& other) noexcept = delete;

    Package& operator=(const Package& other) = delete;

    Package& operator=(Package&& other) noexcept = delete;

    [[nodiscard]] const auto& cangjie_name() const noexcept
    {
        return cangjie_name_;
    }

    [[nodiscard]] std::string_view output_path() const
    {
        return output_path_;
    }

    [[nodiscard]] const PackageFilter* filters() const
    {
        return filters_;
    }

    [[nodiscard]] const std::unordered_set<Package*>& depends_on() const
    {
        return depends_on_;
    }

    void set_filters(const PackageFilter* filters)
    {
        assert(!filters_);
        assert(filters);
        filters_ = filters;
    }

    void add_file(PackageFile* file)
    {
        assert(file);
        [[maybe_unused]] auto [_, inserted] = files_.emplace(file->file_name(), file);
        assert(inserted);
    }

    void add_dependency_edge(Package* package)
    {
        assert(package);
        assert(package != this);
        depends_on_.emplace(package);
    }

    PackageFile* operator[](const std::string& name) const
    {
        const auto it = files_.find(name);
        return it == files_.end() ? nullptr : it->second;
    }

    PackageFile* operator[](const std::string_view name) const
    {
        return (*this)[std::string(name)];
    }

    auto cbegin() const
    {
        return PackageFilesIterator{files_.cbegin()};
    }

    auto cend() const
    {
        return PackageFilesIterator{files_.cend()};
    }

    auto begin() const
    {
        return cbegin();
    }

    auto end() const
    {
        return cend();
    }
};

using package_map_t = std::unordered_map<std::string, Package*>;

class PackagesIterator final {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Package;
    using pointer = value_type*;   // or also value_type*
    using reference = value_type&; // or also value_type&

    package_map_t::const_iterator it_;

    [[nodiscard]] reference get() const;

public:
    explicit PackagesIterator(package_map_t::const_iterator it) : it_(std::move(it))
    {
    }

    reference operator*() const
    {
        return get();
    }
    pointer operator->() const
    {
        return &get();
    }

    // Prefix increment
    PackagesIterator& operator++()
    {
        ++it_;
        return *this;
    }

    // Postfix increment
    PackagesIterator operator++(int)
    {
        PackagesIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool operator==(const PackagesIterator& lhs, const PackagesIterator& rhs)
    {
        return lhs.it_ == rhs.it_;
    }

    friend bool operator!=(const PackagesIterator& lhs, const PackagesIterator& rhs)
    {
        return !(lhs == rhs);
    }
};

class Packages final {
    package_map_t by_cangjie_name_;

public:
    Packages() = default;

    void insert(Package* package)
    {
        auto cangjie_name = std::string(package->cangjie_name());
        assert(by_cangjie_name_.find(cangjie_name) == by_cangjie_name_.end());
        by_cangjie_name_.emplace(cangjie_name, package);
    }

    Package* by_cangjie_name(const std::string_view name) const
    {
        return by_cangjie_name(std::string(name));
    }

    Package* by_cangjie_name(const std::string& name) const
    {
        const auto it = by_cangjie_name_.find(name);
        return it == by_cangjie_name_.end() ? nullptr : it->second;
    }

    [[nodiscard]] std::size_t size() const
    {
        assert(by_cangjie_name_.size() == by_cangjie_name_.size());
        return by_cangjie_name_.size();
    }

    [[nodiscard]] auto cbegin() const
    {
        return PackagesIterator{by_cangjie_name_.cbegin()};
    }

    [[nodiscard]] auto cend() const
    {
        return PackagesIterator{by_cangjie_name_.cend()};
    }

    [[nodiscard]] auto begin() const
    {
        return cbegin();
    }

    [[nodiscard]] auto end() const
    {
        return cend();
    }
};

extern Packages packages;

void create_packages();

} // namespace objcgen

#endif // SCOPE_H
