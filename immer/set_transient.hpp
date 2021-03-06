//
// immer: immutable data structures for C++
// Copyright (C) 2016, 2017, 2018 Juan Pedro Bolivar Puente
//
// This software is distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://boost.org/LICENSE_1_0.txt
//

#pragma once

#include "memory_policy.hpp"
#include "detail/hamts/champ.hpp"

#include <functional>

namespace immer {

/*!
 * **WORK IN PROGRESS**
 */
template <typename T,
          typename Hash          = std::hash<T>,
          typename Equal         = std::equal_to<T>,
          typename MemoryPolicy  = default_memory_policy,
          detail::hamts::bits_t B = default_bits>
class set_transient;

} // namespace immer
