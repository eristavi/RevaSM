#include <revasm/machine.hpp>
#include "internal.hpp"

#include <type_traits>
#include <unordered_set>

namespace revasm {
using namespace detail;

std::vector<VerificationError> Verifier::verify(const Module& module) const {
    std::vector<VerificationError> errors;
    std::unordered_map<std::string, const Function*> functions;

    for (const auto& fn : module.functions) {
        if (!functions.emplace(fn.name, &fn).second) {
            errors.push_back({fn.name, "duplicate function name"});
        }
    }

    auto error = [&](const std::string& where, const std::string& message) {
        errors.push_back({where, message});
    };

    for (const auto& fn : module.functions) {
        const std::string fwhere = "function " + fn.name;
        std::unordered_map<BlockId, const Block*> blocks;
        for (const auto& block : fn.blocks) {
            if (!blocks.emplace(block.id, &block).second) error(fwhere, "duplicate block id");
        }
        if (!blocks.contains(fn.entry)) error(fwhere, "entry block does not exist");

        std::unordered_set<ValueId> all_ids;
        TypeEnv function_params;
        for (const auto& p : fn.params) {
            if (!all_ids.insert(p.id).second) error(fwhere, "duplicate SSA value id " + std::to_string(p.id));
            function_params.types[p.id] = p.type;
        }
        for (const auto& block : fn.blocks) {
            for (const auto& p : block.params) {
                if (!all_ids.insert(p.id).second) error(fwhere, "duplicate SSA value id " + std::to_string(p.id));
            }
            for (const auto& op : block.ops) {
                std::optional<ValueId> out;
                std::visit([&](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, ConstInt> || std::is_same_v<T, Add> ||
                                  std::is_same_v<T, Equal> || std::is_same_v<T, FrameObject> ||
                                  std::is_same_v<T, AddrOffset> || std::is_same_v<T, AddrNarrow> ||
                                  std::is_same_v<T, LoadInt>) out = item.out;
                    else if constexpr (std::is_same_v<T, Call>) out = item.out;
                }, op);
                if (out && !all_ids.insert(*out).second) error(fwhere, "duplicate SSA value id " + std::to_string(*out));
            }
        }

        for (const auto& block : fn.blocks) {
            const std::string where = fwhere + " block " + std::to_string(block.id);
            TypeEnv env = function_params;
            for (const auto& p : block.params) env.types[p.id] = p.type;

            auto require = [&](ValueId id, const Type& expected, const std::string& role) -> bool {
                const Type* actual = lookup(env, id);
                if (!actual) {
                    error(where, role + " uses unavailable SSA value " + std::to_string(id));
                    return false;
                }
                if (!(*actual == expected)) {
                    error(where, role + " expected " + to_string(expected) + " but got " + to_string(*actual));
                    return false;
                }
                return true;
            };

            auto require_same_int = [&](ValueId a, ValueId b, const std::string& role) -> std::optional<IntWidth> {
                const Type* ta = lookup(env, a);
                const Type* tb = lookup(env, b);
                if (!ta || !tb) {
                    error(where, role + " uses unavailable SSA value");
                    return std::nullopt;
                }
                if (ta->kind != TypeKind::Int || tb->kind != TypeKind::Int || ta->width != tb->width) {
                    error(where, role + " requires equal-width integer operands");
                    return std::nullopt;
                }
                return ta->width;
            };

            for (const auto& op : block.ops) {
                std::visit([&](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, ConstInt>) {
                        env.types[item.out] = Type::int_type(item.width);
                    } else if constexpr (std::is_same_v<T, Add>) {
                        if (auto width = require_same_int(item.lhs, item.rhs, "add")) env.types[item.out] = Type::int_type(*width);
                    } else if constexpr (std::is_same_v<T, Equal>) {
                        const Type* a = lookup(env, item.lhs);
                        const Type* b = lookup(env, item.rhs);
                        if (!a || !b || !(*a == *b)) {
                            error(where, "equal requires operands of the same available type");
                        } else if (a->kind == TypeKind::Addr) {
                            error(where, "address equality is intentionally unresolved in this prototype");
                        } else {
                            env.types[item.out] = Type::pred();
                        }
                    } else if constexpr (std::is_same_v<T, Guard>) {
                        require(item.pred, Type::pred(), "guard");
                    } else if constexpr (std::is_same_v<T, FrameObject>) {
                        if (item.permissions & ~(PermRead | PermWrite)) error(where, "frame.object contains unknown permissions");
                        env.types[item.out] = Type::addr();
                    } else if constexpr (std::is_same_v<T, AddrOffset>) {
                        if (require(item.base, Type::addr(), "addr.offset")) env.types[item.out] = Type::addr();
                    } else if constexpr (std::is_same_v<T, AddrNarrow>) {
                        if (require(item.base, Type::addr(), "addr.narrow")) env.types[item.out] = Type::addr();
                    } else if constexpr (std::is_same_v<T, StoreInt>) {
                        require(item.address, Type::addr(), "store address");
                        require(item.value, Type::int_type(item.width), "store value");
                    } else if constexpr (std::is_same_v<T, LoadInt>) {
                        if (require(item.address, Type::addr(), "load address")) env.types[item.out] = Type::int_type(item.width);
                    } else if constexpr (std::is_same_v<T, Call>) {
                        const auto fit = functions.find(item.function);
                        if (fit == functions.end()) {
                            error(where, "call target does not exist: " + item.function);
                            return;
                        }
                        const Function& callee = *fit->second;
                        if (item.args.size() != callee.params.size()) {
                            error(where, "call argument count mismatch for " + item.function);
                        } else {
                            for (std::size_t i = 0; i < item.args.size(); ++i) require(item.args[i], callee.params[i].type, "call argument");
                        }
                        if (callee.return_type.has_value() != item.out.has_value()) {
                            error(where, "call result presence does not match callee signature");
                        } else if (item.out && callee.return_type) {
                            env.types[*item.out] = *callee.return_type;
                        }
                    }
                }, op);
            }

            auto verify_edge = [&](BlockId target, const std::vector<ValueId>& args, const std::string& role) {
                const auto bit = blocks.find(target);
                if (bit == blocks.end()) {
                    error(where, role + " target block does not exist");
                    return;
                }
                const auto& params = bit->second->params;
                if (args.size() != params.size()) {
                    error(where, role + " argument count does not match target block parameters");
                    return;
                }
                for (std::size_t i = 0; i < args.size(); ++i) require(args[i], params[i].type, role + " argument");
            };

            std::visit([&](const auto& term) {
                using T = std::decay_t<decltype(term)>;
                if constexpr (std::is_same_v<T, Go>) {
                    verify_edge(term.target, term.args, "go");
                } else if constexpr (std::is_same_v<T, Branch>) {
                    require(term.pred, Type::pred(), "branch predicate");
                    verify_edge(term.true_target, term.true_args, "branch true");
                    verify_edge(term.false_target, term.false_args, "branch false");
                } else if constexpr (std::is_same_v<T, Return>) {
                    if (fn.return_type.has_value() != term.value.has_value()) {
                        error(where, "return value presence does not match function signature");
                    } else if (fn.return_type && term.value) {
                        require(*term.value, *fn.return_type, "return");
                    }
                } else if constexpr (std::is_same_v<T, Trap>) {
                    // Always valid: terminal explicit trap.
                }
            }, block.terminator);
        }
    }

    return errors;
}

} // namespace revasm
