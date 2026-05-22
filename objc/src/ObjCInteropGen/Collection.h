// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef COLLECTION_H
#define COLLECTION_H

#include <utility>

namespace objcgen {

template <class Container, class ConstIterator = typename Container::const_iterator,
    class Iterator = typename Container::iterator>
class Collection final {
public:
    explicit Collection(Container& container) noexcept : container_(container)
    {
    }

    [[nodiscard]] auto size() const noexcept(noexcept(std::size(std::declval<Container>())))
    {
        return std::size(container_);
    }

    [[nodiscard]] auto empty() const noexcept(noexcept(std::empty(std::declval<Container>())))
    {
        return std::empty(container_);
    }

    [[nodiscard]] auto begin() const noexcept(noexcept(std::begin(std::declval<const Container>())))
    {
        return ConstIterator(std::begin(container_));
    }

    [[nodiscard]] auto begin() noexcept(noexcept(std::begin(std::declval<Container>())))
    {
        return Iterator(std::begin(container_));
    }

    [[nodiscard]] auto end() const noexcept(noexcept(std::end(std::declval<const Container>())))
    {
        return ConstIterator(std::end(container_));
    }

    [[nodiscard]] auto end() noexcept(noexcept(std::end(std::declval<Container>())))
    {
        return Iterator(std::end(container_));
    }

private:
    Container& container_;
};

template <class Container, class ConstIterator = typename Container::const_iterator>
[[nodiscard]] auto ConstCollection(const Container& container) noexcept
{
    return Collection<const Container, ConstIterator, ConstIterator>(container);
}

template <class Iterator> class PointerIterator final {
public:
    using difference_type = typename Iterator::difference_type;
    using value_type = typename Iterator::value_type;
    using pointer = typename Iterator::pointer;
    using reference = typename Iterator::reference;
    using iterator_category = typename Iterator::iterator_category;

    explicit PointerIterator(Iterator it) noexcept(noexcept(Iterator(it))) : it_(it)
    {
    }

    [[nodiscard]] auto& operator*() const noexcept(noexcept(**std::declval<Iterator>()))
    {
        return **it_;
    }

    auto& operator++() noexcept(noexcept(++std::declval<Iterator>()))
    {
        ++it_;
        return *this;
    }

    [[nodiscard]] friend auto operator==(const PointerIterator& it1, const PointerIterator& it2) noexcept(
        noexcept(std::declval<Iterator>() == std::declval<Iterator>()))
    {
        return it1.it_ == it2.it_;
    }

    [[nodiscard]] friend auto operator!=(const PointerIterator& it1, const PointerIterator& it2) noexcept(
        noexcept(std::declval<Iterator>() != std::declval<Iterator>()))
    {
        return it1.it_ != it2.it_;
    }

    [[nodiscard]] friend auto operator-(const PointerIterator& it1, const PointerIterator& it2) noexcept(
        noexcept(std::declval<Iterator>() - std::declval<Iterator>()))
    {
        return it1.it_ - it2.it_;
    }

private:
    Iterator it_;
};

template <class Container> [[nodiscard]] auto ConstPointerCollection(const Container& container) noexcept
{
    return ConstCollection<const Container, PointerIterator<typename Container::const_iterator>>(container);
}

template <class Container> [[nodiscard]] auto PointerCollection(Container& container) noexcept
{
    return Collection<Container, PointerIterator<typename Container::const_iterator>,
        PointerIterator<typename Container::iterator>>(container);
}

} // namespace objcgen

#endif // COLLECTION_H
