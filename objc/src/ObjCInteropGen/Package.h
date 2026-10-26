// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef SCOPE_H
#define SCOPE_H

#include <algorithm>
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

class RootFilter {
    const std::string pattern_;
    Package* package_ = nullptr;

public:
    explicit RootFilter(std::string pattern) : pattern_(std::move(pattern))
    {
    }

    [[nodiscard]] const std::string& pattern() const noexcept
    {
        return pattern_;
    }

    [[nodiscard]] Package* package() const noexcept
    {
        return package_;
    }

    bool set_package(Package* package);

    [[nodiscard]] const PackageFilter& as_filter() const noexcept;
};

class PackageFile final {
    Package* const package_;
    std::vector<FileLevelSymbol*> symbols_;

public:
    explicit PackageFile(Package& package) noexcept : package_(&package)
    {
    }

    [[nodiscard]] Package& package() const noexcept
    {
        return *package_;
    }

    [[nodiscard]] bool is_empty() const noexcept
    {
        return symbols_.empty();
    }

    void add_symbol(FileLevelSymbol& symbol)
    {
        assert(symbol.defining_file());
        symbols_.push_back(&symbol);
    }

    template <class Pred> void erase_symbols_if(Pred pred)
    {
        symbols_.erase(std::remove_if(symbols_.begin(), symbols_.end(), pred), symbols_.end());
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

using package_file_map_t = std::map<std::filesystem::path, PackageFile>;

// `filters` defines which entities may belong to a package. When `root-filters` is set,
// only `root-filters` matches become closure roots; filter-only members join the package
// only when reached via reference closure.
class Package final : NonCopyable {
    const std::string cangjie_name_;
    const std::filesystem::path output_path_;
    const PackageFilter* filters_ = nullptr;
    const PackageFilter* root_filters_ = nullptr;
    package_file_map_t files_;
    std::unordered_set<Package*> depends_on_;

public:
    Package(std::string cangjie_name, std::filesystem::path output_path) noexcept;

    ~Package() = default;

    [[nodiscard]] const std::string& cangjie_name() const noexcept
    {
        return cangjie_name_;
    }

    [[nodiscard]] const std::filesystem::path& output_path() const noexcept
    {
        return output_path_;
    }

    [[nodiscard]] std::filesystem::path output_path(const std::filesystem::path& output_file_name) const;

    [[nodiscard]] const PackageFilter* filters() const noexcept
    {
        return filters_;
    }

    [[nodiscard]] const PackageFilter* root_filters() const noexcept
    {
        return root_filters_;
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

    void set_root_filters(const PackageFilter& root_filters) noexcept
    {
        assert(!root_filters_);
        root_filters_ = &root_filters;
    }

    [[nodiscard]] PackageFile& get_file(std::filesystem::path file_name);

    void add_dependency_edge(Package& package)
    {
        assert(&package != this);
        depends_on_.emplace(&package);
    }

    [[nodiscard]] auto begin() const noexcept
    {
        return files_.begin();
    }

    [[nodiscard]] auto begin() noexcept
    {
        return files_.begin();
    }

    [[nodiscard]] auto end() const noexcept
    {
        return files_.end();
    }

    [[nodiscard]] auto end() noexcept
    {
        return files_.end();
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
    [[nodiscard]] static const Packages& get() noexcept;

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

void create_packages();

class Roots final {
public:
    [[nodiscard]] static const Roots& get() noexcept;

    void register_from_value(std::string value)
    {
        roots.emplace_back(std::move(value));
    }

    ~Roots() = default;

    [[nodiscard]] std::size_t size() const noexcept
    {
        return roots.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return roots.empty();
    }

    [[nodiscard]] auto begin() noexcept
    {
        return roots.begin();
    }
    [[nodiscard]] auto begin() const noexcept
    {
        return roots.begin();
    }
    [[nodiscard]] auto end() noexcept
    {
        return roots.end();
    }
    [[nodiscard]] auto end() const noexcept
    {
        return roots.end();
    }

private:
    std::vector<RootFilter> roots;
};

} // namespace objcgen

#endif // SCOPE_H
