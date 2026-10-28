#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace revasm {

using ValueId = std::uint32_t;
using BlockId = std::uint32_t;
using ObjectId = std::uint64_t;

enum class IntWidth : std::uint8_t { I8 = 8, I16 = 16, I32 = 32, I64 = 64 };
enum class TypeKind : std::uint8_t { Int, Pred, Addr };

struct Type {
    TypeKind kind{TypeKind::Int};
    IntWidth width{IntWidth::I64};

    static Type int_type(IntWidth width) { return Type{TypeKind::Int, width}; }
    static Type pred() { return Type{TypeKind::Pred, IntWidth::I8}; }
    static Type addr() { return Type{TypeKind::Addr, IntWidth::I64}; }

    friend bool operator==(const Type&, const Type&) = default;
};

enum Permission : std::uint8_t {
    PermRead = 1u << 0,
    PermWrite = 1u << 1,
};
using Permissions = std::uint8_t;

struct IntValue {
    IntWidth width{IntWidth::I64};
    std::uint64_t bits{0};
};

struct Address {
    ObjectId object{0};
    std::uint64_t offset{0};
    std::uint64_t lower{0};
    std::uint64_t upper{0}; // exclusive
    Permissions permissions{0};
};

using Value = std::variant<IntValue, bool, Address>;

struct Param {
    ValueId id{};
    Type type{};
};

enum class ArithmeticPolicy : std::uint8_t { Wrap, Checked };
enum class Signedness : std::uint8_t { Unsigned, Signed };
enum class InitPolicy : std::uint8_t { RuntimeTracked, Zeroed };
enum class AddressDerivationPolicy : std::uint8_t { StrictInBounds, PermitOutOfBoundsUntilAccess };

struct PrototypeProfile {
    InitPolicy init_policy{InitPolicy::RuntimeTracked};
    AddressDerivationPolicy address_derivation{AddressDerivationPolicy::StrictInBounds};
    bool allow_one_past{true};
};

struct ConstInt { ValueId out; IntWidth width; std::uint64_t bits; };
struct Add { ValueId out; ValueId lhs; ValueId rhs; ArithmeticPolicy policy; Signedness signedness; };
struct Equal { ValueId out; ValueId lhs; ValueId rhs; };
struct Guard { ValueId pred; };
struct FrameObject { ValueId out; std::uint64_t size; Permissions permissions; };
struct AddrOffset { ValueId out; ValueId base; std::int64_t delta; };
struct AddrNarrow { ValueId out; ValueId base; std::uint64_t lower; std::uint64_t upper; Permissions permissions; };
struct StoreInt { ValueId address; ValueId value; IntWidth width; };
struct LoadInt { ValueId out; ValueId address; IntWidth width; };
struct Call { std::optional<ValueId> out; std::string function; std::vector<ValueId> args; };

using Op = std::variant<ConstInt, Add, Equal, Guard, FrameObject, AddrOffset, AddrNarrow, StoreInt, LoadInt, Call>;

struct Go { BlockId target; std::vector<ValueId> args; };
struct Branch {
    ValueId pred;
    BlockId true_target;
    std::vector<ValueId> true_args;
    BlockId false_target;
    std::vector<ValueId> false_args;
};
struct Return { std::optional<ValueId> value; };
struct Trap { std::string reason; };

using Terminator = std::variant<Go, Branch, Return, Trap>;

struct Block {
    BlockId id{};
    std::vector<Param> params;
    std::vector<Op> ops;
    Terminator terminator{Return{std::nullopt}};
};

struct Function {
    std::string name;
    std::vector<Param> params;
    std::optional<Type> return_type;
    BlockId entry{};
    std::vector<Block> blocks;
};

struct Module {
    PrototypeProfile profile{};
    std::vector<Function> functions;
};

enum class TrapReason : std::uint8_t {
    GuardFailed,
    ExplicitTrap,
    OutOfBounds,
    PermissionDenied,
    StaleAddress,
    UninitializedRead,
    CheckedOverflow,
    AuthorityViolation,
    ResourceExhausted,
    InvalidExecution,
};

struct ExecutionTrap {
    TrapReason reason{TrapReason::InvalidExecution};
    std::string detail;
};

struct ExecutionResult {
    std::optional<Value> value;
    std::optional<ExecutionTrap> trap;

    bool succeeded() const { return !trap.has_value(); }
};

struct VerificationError {
    std::string where;
    std::string message;
};

class Verifier {
public:
    std::vector<VerificationError> verify(const Module& module) const;
};

class Interpreter {
public:
    explicit Interpreter(const Module& module);
    ExecutionResult run(const std::string& function, const std::vector<Value>& args = {});

    std::size_t object_count() const;
    bool object_alive(ObjectId id) const;

private:
    struct Object {
        ObjectId id{};
        std::vector<std::uint8_t> bytes;
        std::vector<bool> initialized;
        bool alive{true};
    };

    const Module& module_;
    std::unordered_map<ObjectId, Object> objects_;
    ObjectId next_object_id_{1};

    const Function* find_function(const std::string& name) const;
    const Block* find_block(const Function& function, BlockId id) const;
    ExecutionResult execute_function(const Function& function, const std::vector<Value>& args);

    ObjectId create_object(std::uint64_t size, std::vector<ObjectId>& frame_objects);
    void end_frame(std::vector<ObjectId>& frame_objects);
};

std::string to_string(const Type& type);
std::string to_string(TrapReason reason);
std::uint64_t int_mask(IntWidth width);
std::uint64_t canonicalize(IntWidth width, std::uint64_t bits);

} // namespace revasm
