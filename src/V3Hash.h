// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Hash calculation
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// Copyright 2003-2025 by Wilson Snyder. This program is free software; you
// can redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************

#ifndef VERILATOR_V3HASH_H_
#define VERILATOR_V3HASH_H_

#include "verilatedos.h"

#include <cstdint>
#include <string>

//######################################################################
// V3Hash -- Generic hashing

class V3Hash final {
    uint32_t m_value;  // The 32-bit hash value.

    static uint32_t combine(uint32_t a, uint32_t b) {
        return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
    }

public:
    // CONSTRUCTORS
    V3Hash()
        : m_value{0} {}
    explicit V3Hash(uint32_t val)
        : m_value{val} {}
    explicit V3Hash(int32_t val)
        : m_value{static_cast<uint32_t>(val)} {}
    explicit V3Hash(uint64_t val)
        : m_value{combine(static_cast<uint32_t>(val), static_cast<uint32_t>(val >> 32))} {}
    explicit V3Hash(int64_t val)
        : m_value{combine(static_cast<uint32_t>(val), static_cast<uint32_t>(val >> 32))} {}
    explicit V3Hash(const std::string& val);

    // METHODS
    uint32_t value() const VL_MT_SAFE { return m_value; }
    std::string toString() const VL_MT_SAFE;

    // OPERATORS
    // Comparisons
    bool operator==(const V3Hash& rh) const { return m_value == rh.m_value; }
    bool operator!=(const V3Hash& rh) const { return m_value != rh.m_value; }
    bool operator<(const V3Hash& rh) const { return m_value < rh.m_value; }

    // '+' combines hashes
    template <typename T>
    V3Hash operator+(T that) const {
        return V3Hash{combine(m_value, V3Hash{that}.m_value)};
    }

    // '+=' combines in place
    template <typename T>
    V3Hash& operator+=(T that) {
        return *this = *this + that;
    }
};

std::ostream& operator<<(std::ostream& os, const V3Hash& rhs) VL_MT_SAFE;

template <>
struct std::hash<V3Hash> {
    std::size_t operator()(const V3Hash& h) const noexcept { return h.value(); }
};

#endif  // Guard
