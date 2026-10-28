#include <revasm/machine.hpp>
#include "internal.hpp"

#include <limits>
#include <stdexcept>
#include <type_traits>

namespace revasm {
using namespace detail;

Interpreter::Interpreter(const Module& module) : module_(module) {}

const Function* Interpreter::find_function(const std::string& name) const {
    for (const auto& fn : module_.functions) if (fn.name == name) return &fn;
    return nullptr;
}

const Block* Interpreter::find_block(const Function& function, BlockId id) const {
    for (const auto& block : function.blocks) if (block.id == id) return &block;
    return nullptr;
}

ObjectId Interpreter::create_object(std::uint64_t size, std::vector<ObjectId>& frame_objects) {
    const ObjectId id = next_object_id_++;
    Object object;
    object.id = id;
    object.bytes.assign(static_cast<std::size_t>(size), 0);
    object.initialized.assign(static_cast<std::size_t>(size), module_.profile.init_policy == InitPolicy::Zeroed);
    objects_.emplace(id, std::move(object));
    frame_objects.push_back(id);
    return id;
}

void Interpreter::end_frame(std::vector<ObjectId>& frame_objects) {
    for (ObjectId id : frame_objects) {
        auto it = objects_.find(id);
        if (it != objects_.end()) it->second.alive = false;
    }
}

ExecutionResult Interpreter::run(const std::string& function, const std::vector<Value>& args) {
    const Function* fn = find_function(function);
    if (!fn) return {std::nullopt, ExecutionTrap{TrapReason::InvalidExecution, "function not found: " + function}};
    return execute_function(*fn, args);
}

ExecutionResult Interpreter::execute_function(const Function& function, const std::vector<Value>& args) {
    if (args.size() != function.params.size()) {
        return {std::nullopt, ExecutionTrap{TrapReason::InvalidExecution, "argument count mismatch"}};
    }
    std::unordered_map<ValueId, Value> function_values;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (!(type_of_value(args[i]) == function.params[i].type)) {
            return {std::nullopt, ExecutionTrap{TrapReason::InvalidExecution, "argument type mismatch"}};
        }
        function_values[function.params[i].id] = args[i];
    }

    std::vector<ObjectId> frame_objects;
    BlockId current = function.entry;
    std::vector<Value> incoming;

    auto finish = [&](ExecutionResult result) {
        end_frame(frame_objects);
        return result;
    };

    while (true) {
        const Block* block = find_block(function, current);
        if (!block) return finish({std::nullopt, ExecutionTrap{TrapReason::InvalidExecution, "missing block"}});
        if (incoming.size() != block->params.size()) {
            return finish({std::nullopt, ExecutionTrap{TrapReason::InvalidExecution, "block argument mismatch"}});
        }

        std::unordered_map<ValueId, Value> values = function_values;
        for (std::size_t i = 0; i < incoming.size(); ++i) values[block->params[i].id] = incoming[i];

        auto get = [&](ValueId id) -> Value* {
            auto it = values.find(id);
            return it == values.end() ? nullptr : &it->second;
        };

        auto trap = [&](TrapReason reason, std::string detail) -> ExecutionResult {
            return {std::nullopt, ExecutionTrap{reason, std::move(detail)}};
        };

        for (const auto& op : block->ops) {
            std::optional<ExecutionResult> failure;
            std::visit([&](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if (failure) return;
                if constexpr (std::is_same_v<T, ConstInt>) {
                    values[item.out] = IntValue{item.width, canonicalize(item.width, item.bits)};
                } else if constexpr (std::is_same_v<T, Add>) {
                    auto* lv = get(item.lhs); auto* rv = get(item.rhs);
                    if (!lv || !rv || !std::holds_alternative<IntValue>(*lv) || !std::holds_alternative<IntValue>(*rv)) {
                        failure = trap(TrapReason::InvalidExecution, "add operands unavailable"); return;
                    }
                    const auto l = std::get<IntValue>(*lv); const auto r = std::get<IntValue>(*rv);
                    if (l.width != r.width) { failure = trap(TrapReason::InvalidExecution, "add width mismatch"); return; }
                    const auto mask = int_mask(l.width);
                    const auto raw = (l.bits & mask) + (r.bits & mask);
                    if (item.policy == ArithmeticPolicy::Checked) {
                        bool overflow = false;
                        if (item.signedness == Signedness::Unsigned) {
                            overflow = raw > mask || (l.width == IntWidth::I64 && raw < (l.bits & mask));
                        } else {
                            const auto a = sign_extend(l.width, l.bits);
                            const auto b = sign_extend(l.width, r.bits);
                            if (l.width == IntWidth::I64) {
                                overflow = (b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
                                           (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b);
                            } else {
                                const auto n = width_bits(l.width);
                                const std::int64_t minv = -(1ll << (n - 1));
                                const std::int64_t maxv = (1ll << (n - 1)) - 1;
                                const std::int64_t sum = a + b;
                                overflow = sum < minv || sum > maxv;
                            }
                        }
                        if (overflow) { failure = trap(TrapReason::CheckedOverflow, "checked add overflow"); return; }
                    }
                    values[item.out] = IntValue{l.width, canonicalize(l.width, raw)};
                } else if constexpr (std::is_same_v<T, Equal>) {
                    auto* a = get(item.lhs); auto* b = get(item.rhs);
                    if (!a || !b) { failure = trap(TrapReason::InvalidExecution, "equal operand unavailable"); return; }
                    if (a->index() != b->index()) { failure = trap(TrapReason::InvalidExecution, "equal type mismatch"); return; }
                    bool eq = false;
                    if (std::holds_alternative<IntValue>(*a)) {
                        const auto av = std::get<IntValue>(*a), bv = std::get<IntValue>(*b);
                        eq = av.width == bv.width && av.bits == bv.bits;
                    } else if (std::holds_alternative<bool>(*a)) {
                        eq = std::get<bool>(*a) == std::get<bool>(*b);
                    } else {
                        failure = trap(TrapReason::InvalidExecution, "address equality is not defined by this prototype");
                        return;
                    }
                    values[item.out] = eq;
                } else if constexpr (std::is_same_v<T, Guard>) {
                    auto* p = get(item.pred);
                    if (!p || !std::holds_alternative<bool>(*p)) { failure = trap(TrapReason::InvalidExecution, "guard predicate unavailable"); return; }
                    if (!std::get<bool>(*p)) failure = trap(TrapReason::GuardFailed, "guard predicate is false");
                } else if constexpr (std::is_same_v<T, FrameObject>) {
                    try {
                        ObjectId id = create_object(item.size, frame_objects);
                        values[item.out] = Address{id, 0, 0, item.size, item.permissions};
                    } catch (const std::bad_alloc&) {
                        failure = trap(TrapReason::ResourceExhausted, "frame.object allocation failed");
                    } catch (const std::length_error&) {
                        failure = trap(TrapReason::ResourceExhausted, "frame.object size is not representable by this implementation");
                    }
                } else if constexpr (std::is_same_v<T, AddrOffset>) {
                    auto* b = get(item.base);
                    if (!b || !std::holds_alternative<Address>(*b)) { failure = trap(TrapReason::InvalidExecution, "addr.offset base unavailable"); return; }
                    Address addr = std::get<Address>(*b);
                    std::uint64_t next = 0;
                    if (item.delta >= 0) {
                        const auto d = static_cast<std::uint64_t>(item.delta);
                        if (addr.offset > std::numeric_limits<std::uint64_t>::max() - d) { failure = trap(TrapReason::OutOfBounds, "address arithmetic overflow"); return; }
                        next = addr.offset + d;
                    } else {
                        const auto d = static_cast<std::uint64_t>(-(item.delta + 1)) + 1;
                        if (addr.offset < d) { failure = trap(TrapReason::OutOfBounds, "address arithmetic underflow"); return; }
                        next = addr.offset - d;
                    }
                    if (module_.profile.address_derivation == AddressDerivationPolicy::StrictInBounds) {
                        const bool in = next >= addr.lower && next < addr.upper;
                        const bool one_past = module_.profile.allow_one_past && next == addr.upper;
                        if (!in && !one_past) { failure = trap(TrapReason::OutOfBounds, "derived address outside authority bounds"); return; }
                    }
                    addr.offset = next;
                    values[item.out] = addr;
                } else if constexpr (std::is_same_v<T, AddrNarrow>) {
                    auto* b = get(item.base);
                    if (!b || !std::holds_alternative<Address>(*b)) { failure = trap(TrapReason::InvalidExecution, "addr.narrow base unavailable"); return; }
                    Address addr = std::get<Address>(*b);
                    const bool bounds_ok = item.lower >= addr.lower && item.upper <= addr.upper && item.lower <= item.upper;
                    const bool perms_ok = (item.permissions & ~addr.permissions) == 0;
                    if (!bounds_ok || !perms_ok) { failure = trap(TrapReason::AuthorityViolation, "addr.narrow would amplify authority"); return; }
                    addr.lower = item.lower; addr.upper = item.upper; addr.permissions = item.permissions;
                    if (addr.offset < addr.lower || addr.offset > addr.upper) {
                        failure = trap(TrapReason::AuthorityViolation, "current location excluded by narrowed bounds"); return;
                    }
                    values[item.out] = addr;
                } else if constexpr (std::is_same_v<T, StoreInt>) {
                    auto* av = get(item.address); auto* vv = get(item.value);
                    if (!av || !vv || !std::holds_alternative<Address>(*av) || !std::holds_alternative<IntValue>(*vv)) {
                        failure = trap(TrapReason::InvalidExecution, "store operands unavailable"); return;
                    }
                    const Address addr = std::get<Address>(*av); const IntValue iv = std::get<IntValue>(*vv);
                    auto oit = objects_.find(addr.object);
                    if (oit == objects_.end() || !oit->second.alive) { failure = trap(TrapReason::StaleAddress, "store through stale address"); return; }
                    if (!has_permission(addr.permissions, PermWrite)) { failure = trap(TrapReason::PermissionDenied, "store requires write authority"); return; }
                    const auto bytes = width_bytes(item.width);
                    if (iv.width != item.width || addr.offset < addr.lower || addr.offset > addr.upper || bytes > addr.upper - addr.offset || addr.offset + bytes > oit->second.bytes.size()) {
                        failure = trap(TrapReason::OutOfBounds, "store outside object bounds"); return;
                    }
                    const auto bits = canonicalize(item.width, iv.bits);
                    for (std::uint64_t i = 0; i < bytes; ++i) {
                        oit->second.bytes[static_cast<std::size_t>(addr.offset + i)] = static_cast<std::uint8_t>((bits >> (8 * i)) & 0xffu);
                        oit->second.initialized[static_cast<std::size_t>(addr.offset + i)] = true;
                    }
                } else if constexpr (std::is_same_v<T, LoadInt>) {
                    auto* av = get(item.address);
                    if (!av || !std::holds_alternative<Address>(*av)) { failure = trap(TrapReason::InvalidExecution, "load address unavailable"); return; }
                    const Address addr = std::get<Address>(*av);
                    auto oit = objects_.find(addr.object);
                    if (oit == objects_.end() || !oit->second.alive) { failure = trap(TrapReason::StaleAddress, "load through stale address"); return; }
                    if (!has_permission(addr.permissions, PermRead)) { failure = trap(TrapReason::PermissionDenied, "load requires read authority"); return; }
                    const auto bytes = width_bytes(item.width);
                    if (addr.offset < addr.lower || addr.offset > addr.upper || bytes > addr.upper - addr.offset || addr.offset + bytes > oit->second.bytes.size()) {
                        failure = trap(TrapReason::OutOfBounds, "load outside object bounds"); return;
                    }
                    if (module_.profile.init_policy == InitPolicy::RuntimeTracked) {
                        for (std::uint64_t i = 0; i < bytes; ++i) {
                            if (!oit->second.initialized[static_cast<std::size_t>(addr.offset + i)]) {
                                failure = trap(TrapReason::UninitializedRead, "load observes uninitialized byte"); return;
                            }
                        }
                    }
                    std::uint64_t bits = 0;
                    for (std::uint64_t i = 0; i < bytes; ++i) bits |= static_cast<std::uint64_t>(oit->second.bytes[static_cast<std::size_t>(addr.offset + i)]) << (8 * i);
                    values[item.out] = IntValue{item.width, canonicalize(item.width, bits)};
                } else if constexpr (std::is_same_v<T, Call>) {
                    const Function* callee = find_function(item.function);
                    if (!callee) { failure = trap(TrapReason::InvalidExecution, "missing call target"); return; }
                    std::vector<Value> call_args;
                    for (ValueId id : item.args) {
                        auto* v = get(id); if (!v) { failure = trap(TrapReason::InvalidExecution, "call argument unavailable"); return; }
                        call_args.push_back(*v);
                    }
                    auto result = execute_function(*callee, call_args);
                    if (!result.succeeded()) { failure = result; return; }
                    if (item.out) {
                        if (!result.value) { failure = trap(TrapReason::InvalidExecution, "callee returned no value"); return; }
                        values[*item.out] = *result.value;
                    }
                }
            }, op);
            if (failure) return finish(*failure);
        }

        bool transferred = false;
        ExecutionResult terminal;
        std::visit([&](const auto& term) {
            using T = std::decay_t<decltype(term)>;
            if constexpr (std::is_same_v<T, Go>) {
                const Block* target = find_block(function, term.target);
                if (!target || term.args.size() != target->params.size()) { terminal = trap(TrapReason::InvalidExecution, "invalid go edge"); return; }
                incoming.clear();
                for (ValueId id : term.args) { auto* v = get(id); if (!v) { terminal = trap(TrapReason::InvalidExecution, "go arg unavailable"); return; } incoming.push_back(*v); }
                current = term.target; transferred = true;
            } else if constexpr (std::is_same_v<T, Branch>) {
                auto* p = get(term.pred);
                if (!p || !std::holds_alternative<bool>(*p)) { terminal = trap(TrapReason::InvalidExecution, "branch predicate unavailable"); return; }
                const bool take_true = std::get<bool>(*p);
                const BlockId target_id = take_true ? term.true_target : term.false_target;
                const auto& ids = take_true ? term.true_args : term.false_args;
                const Block* target = find_block(function, target_id);
                if (!target || ids.size() != target->params.size()) { terminal = trap(TrapReason::InvalidExecution, "invalid branch edge"); return; }
                incoming.clear();
                for (ValueId id : ids) { auto* v = get(id); if (!v) { terminal = trap(TrapReason::InvalidExecution, "branch arg unavailable"); return; } incoming.push_back(*v); }
                current = target_id; transferred = true;
            } else if constexpr (std::is_same_v<T, Return>) {
                if (term.value) {
                    auto* v = get(*term.value);
                    if (!v) terminal = trap(TrapReason::InvalidExecution, "return value unavailable");
                    else terminal = ExecutionResult{*v, std::nullopt};
                } else terminal = ExecutionResult{std::nullopt, std::nullopt};
            } else if constexpr (std::is_same_v<T, Trap>) {
                terminal = trap(TrapReason::ExplicitTrap, term.reason);
            }
        }, block->terminator);

        if (transferred) continue;
        return finish(terminal);
    }
}

std::size_t Interpreter::object_count() const { return objects_.size(); }

bool Interpreter::object_alive(ObjectId id) const {
    const auto it = objects_.find(id);
    return it != objects_.end() && it->second.alive;
}

} // namespace revasm
