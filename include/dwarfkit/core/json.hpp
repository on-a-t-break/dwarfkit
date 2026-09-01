#pragma once

#include <charconv>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace dwarfkit {

// Plays the role of plain JS objects everywhere Wharfkit accepts or returns
// one. ordered_json keeps insertion order for stringify parity.
using json = nlohmann::ordered_json;

// Accessors for json that came off a network: they never throw, where
// nlohmann's value()/at()/get<T> throw on a wrong type or a missing key and
// operator[] on a const object is undefined past the end. The library's public
// API is exception-free, so parsing a remote response must go through these.
inline std::string jsonStr(const json& value, std::string_view key,
                           std::string fallback = {}) {
    if (!value.is_object()) {
        return fallback;
    }
    const auto it = value.find(key);
    return it != value.end() && it->is_string() ? it->get<std::string>() : fallback;
}

inline double jsonNum(const json& value, std::string_view key, double fallback = 0) {
    if (!value.is_object()) {
        return fallback;
    }
    const auto it = value.find(key);
    return it != value.end() && it->is_number() ? it->get<double>() : fallback;
}

// A uint64 written either as a json number or as a decimal string (nodeos and
// the action stream use both). Returns the fallback for anything else.
inline uint64_t jsonUInt64(const json& value, std::string_view key, uint64_t fallback = 0) {
    if (!value.is_object()) {
        return fallback;
    }
    const auto it = value.find(key);
    if (it == value.end()) {
        return fallback;
    }
    if (it->is_number_unsigned()) {
        return it->get<uint64_t>();
    }
    if (it->is_number_integer() && it->get<int64_t>() >= 0) {
        return static_cast<uint64_t>(it->get<int64_t>());
    }
    if (it->is_string()) {
        const std::string text = it->get<std::string>();
        uint64_t parsed = 0;
        const auto* first = text.data();
        const auto* last = first + text.size();
        const auto [ptr, ec] = std::from_chars(first, last, parsed);
        if (ec == std::errc{} && ptr == last) {
            return parsed;
        }
    }
    return fallback;
}

// The json value at key, or a null json if it is missing or the receiver is
// not an object. Safe to chain.
inline const json& jsonAt(const json& value, std::string_view key) {
    static const json nullValue;
    if (!value.is_object()) {
        return nullValue;
    }
    const auto it = value.find(key);
    return it != value.end() ? *it : nullValue;
}

}  // namespace dwarfkit
