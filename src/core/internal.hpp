#pragma once

#include <revasm/machine.hpp>

#include <limits>
#include <unordered_map>

namespace revasm::detail {

inline std::uint64_t width_bits(IntWidth width) {
    return static_cast<std::uint8_t>(width);
}

inline std::uint64_t width_bytes(IntWidth width) {
    return width_bits(width) / 8;
}

inline Type type_of_value(const Value& value) {
    if (const auto* i = std::get_if<IntValue>(&value)) return Type::int_type(i->width);
    if (std::holds_alternative<bool>(value)) return Type::pred();
    return Type::addr();
}

inline std::int64_t sign_extend(IntWidth width, std::uint64_t bits) {
    const auto n = width_bits(width);
    bits &= int_mask(width);
    if (n == 64) return static_cast<std::int64_t>(bits);
    const std::uint64_t sign = 1ull << (n - 1);
    if ((bits & sign) == 0) return static_cast<std::int64_t>(bits);
    return static_cast<std::int64_t>(bits | ~int_mask(width));
}

inline bool has_permission(Permissions value, Permission required) {
    return (value & static_cast<Permissions>(required)) != 0;
}

struct TypeEnv {
    std::unordered_map<ValueId, Type> types;
};

inline const Type* lookup(const TypeEnv& env, ValueId id) {
    const auto it = env.types.find(id);
    return it == env.types.end() ? nullptr : &it->second;
}

} // namespace revasm::detail
