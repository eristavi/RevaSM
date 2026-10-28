#include <revasm/machine.hpp>
#include "internal.hpp"

#include <limits>

namespace revasm {
using namespace detail;

std::uint64_t int_mask(IntWidth width) {
    const auto n = width_bits(width);
    return n == 64 ? std::numeric_limits<std::uint64_t>::max() : ((1ull << n) - 1ull);
}

std::uint64_t canonicalize(IntWidth width, std::uint64_t bits) {
    return bits & int_mask(width);
}

std::string to_string(const Type& type) {
    switch (type.kind) {
        case TypeKind::Int: return "i" + std::to_string(width_bits(type.width));
        case TypeKind::Pred: return "pred";
        case TypeKind::Addr: return "addr";
    }
    return "<?>";
}

std::string to_string(TrapReason reason) {
    switch (reason) {
        case TrapReason::GuardFailed: return "GuardFailed";
        case TrapReason::ExplicitTrap: return "ExplicitTrap";
        case TrapReason::OutOfBounds: return "OutOfBounds";
        case TrapReason::PermissionDenied: return "PermissionDenied";
        case TrapReason::StaleAddress: return "StaleAddress";
        case TrapReason::UninitializedRead: return "UninitializedRead";
        case TrapReason::CheckedOverflow: return "CheckedOverflow";
        case TrapReason::AuthorityViolation: return "AuthorityViolation";
        case TrapReason::ResourceExhausted: return "ResourceExhausted";
        case TrapReason::InvalidExecution: return "InvalidExecution";
    }
    return "UnknownTrap";
}

} // namespace revasm
