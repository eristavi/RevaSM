#include <revasm/machine.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

using namespace revasm;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

Module memory_roundtrip_module(InitPolicy init = InitPolicy::RuntimeTracked) {
    Module m;
    m.profile.init_policy = init;
    Function main;
    main.name = "main";
    main.return_type = Type::int_type(IntWidth::I64);
    main.entry = 0;
    Block b;
    b.id = 0;
    b.ops = {
        FrameObject{1, 16, static_cast<Permissions>(PermRead | PermWrite)},
        ConstInt{2, IntWidth::I64, 0x1122334455667788ull},
        StoreInt{1, 2, IntWidth::I64},
        LoadInt{3, 1, IntWidth::I64},
    };
    b.terminator = Return{3};
    main.blocks.push_back(std::move(b));
    m.functions.push_back(std::move(main));
    return m;
}

Module stale_escape_module() {
    Module m;

    Function make;
    make.name = "make_local";
    make.return_type = Type::addr();
    make.entry = 0;
    Block mb;
    mb.id = 0;
    mb.ops = {
        FrameObject{1, 8, static_cast<Permissions>(PermRead | PermWrite)},
        ConstInt{2, IntWidth::I64, 42},
        StoreInt{1, 2, IntWidth::I64},
    };
    mb.terminator = Return{1};
    make.blocks.push_back(std::move(mb));

    Function main;
    main.name = "main";
    main.return_type = Type::int_type(IntWidth::I64);
    main.entry = 0;
    Block b;
    b.id = 0;
    b.ops = {
        Call{3, "make_local", {}},
        LoadInt{4, 3, IntWidth::I64},
    };
    b.terminator = Return{4};
    main.blocks.push_back(std::move(b));

    m.functions = {std::move(make), std::move(main)};
    return m;
}

Module uninitialized_module(InitPolicy policy) {
    Module m;
    m.profile.init_policy = policy;
    Function main;
    main.name = "main";
    main.return_type = Type::int_type(IntWidth::I64);
    main.entry = 0;
    Block b;
    b.id = 0;
    b.ops = {
        FrameObject{1, 8, static_cast<Permissions>(PermRead | PermWrite)},
        LoadInt{2, 1, IntWidth::I64},
    };
    b.terminator = Return{2};
    main.blocks.push_back(std::move(b));
    m.functions.push_back(std::move(main));
    return m;
}

Module narrowing_module(bool illegal) {
    Module m;
    Function main;
    main.name = "main";
    main.return_type = Type::int_type(IntWidth::I64);
    main.entry = 0;
    Block b;
    b.id = 0;
    b.ops.push_back(FrameObject{1, 16, static_cast<Permissions>(PermRead | PermWrite)});
    b.ops.push_back(AddrNarrow{2, 1, 0, 8, static_cast<Permissions>(illegal ? (PermRead | PermWrite | 0x4) : PermRead)});
    b.ops.push_back(ConstInt{3, IntWidth::I64, 7});
    if (illegal) {
        b.ops.push_back(StoreInt{2, 3, IntWidth::I64});
        b.terminator = Return{3};
    } else {
        b.ops.push_back(LoadInt{4, 2, IntWidth::I64});
        b.terminator = Return{4};
    }
    main.blocks.push_back(std::move(b));
    m.functions.push_back(std::move(main));
    return m;
}

Module guard_module(bool truth) {
    Module m;
    Function main;
    main.name = "main";
    main.return_type = Type::int_type(IntWidth::I64);
    main.entry = 0;
    Block b;
    b.id = 0;
    b.ops = {
        ConstInt{1, IntWidth::I64, truth ? 9u : 9u},
        ConstInt{2, IntWidth::I64, truth ? 9u : 10u},
        Equal{3, 1, 2},
        Guard{3},
    };
    b.terminator = Return{1};
    main.blocks.push_back(std::move(b));
    m.functions.push_back(std::move(main));
    return m;
}

Module block_parameter_module() {
    Module m;
    Function main;
    main.name = "main";
    main.return_type = Type::int_type(IntWidth::I64);
    main.entry = 0;

    Block entry;
    entry.id = 0;
    entry.ops = {
        ConstInt{1, IntWidth::I64, 40},
        ConstInt{2, IntWidth::I64, 2},
        Add{3, 1, 2, ArithmeticPolicy::Checked, Signedness::Unsigned},
    };
    entry.terminator = Go{1, {3}};

    Block exit;
    exit.id = 1;
    exit.params = {Param{10, Type::int_type(IntWidth::I64)}};
    exit.terminator = Return{10};

    main.blocks = {std::move(entry), std::move(exit)};
    m.functions.push_back(std::move(main));
    return m;
}

Module invalid_cross_block_ssa_module() {
    Module m;
    Function main;
    main.name = "main";
    main.return_type = Type::int_type(IntWidth::I64);
    main.entry = 0;
    Block a;
    a.id = 0;
    a.ops = {ConstInt{1, IntWidth::I64, 5}};
    a.terminator = Go{1, {}};
    Block b;
    b.id = 1;
    b.terminator = Return{1}; // illegal: must be passed as a block parameter
    main.blocks = {std::move(a), std::move(b)};
    m.functions.push_back(std::move(main));
    return m;
}

Module checked_overflow_module(bool wrapping) {
    Module m;
    Function main;
    main.name = "main";
    main.return_type = Type::int_type(IntWidth::I8);
    main.entry = 0;
    Block b;
    b.id = 0;
    b.ops = {
        ConstInt{1, IntWidth::I8, 255},
        ConstInt{2, IntWidth::I8, 1},
        Add{3, 1, 2, wrapping ? ArithmeticPolicy::Wrap : ArithmeticPolicy::Checked, Signedness::Unsigned},
    };
    b.terminator = Return{3};
    main.blocks.push_back(std::move(b));
    m.functions.push_back(std::move(main));
    return m;
}

Module permission_module() {
    Module m;
    Function main;
    main.name = "main";
    main.return_type = Type::int_type(IntWidth::I64);
    main.entry = 0;
    Block b;
    b.id = 0;
    b.ops = {
        FrameObject{1, 8, static_cast<Permissions>(PermRead | PermWrite)},
        AddrNarrow{2, 1, 0, 8, static_cast<Permissions>(PermRead)},
        ConstInt{3, IntWidth::I64, 99},
        StoreInt{2, 3, IntWidth::I64},
    };
    b.terminator = Return{3};
    main.blocks.push_back(std::move(b));
    m.functions.push_back(std::move(main));
    return m;
}

Module one_past_access_module() {
    Module m;
    m.profile.address_derivation = AddressDerivationPolicy::StrictInBounds;
    m.profile.allow_one_past = true;
    Function main;
    main.name = "main";
    main.return_type = Type::int_type(IntWidth::I8);
    main.entry = 0;
    Block b;
    b.id = 0;
    b.ops = {
        FrameObject{1, 8, static_cast<Permissions>(PermRead | PermWrite)},
        AddrOffset{2, 1, 8},
        LoadInt{3, 2, IntWidth::I8},
    };
    b.terminator = Return{3};
    main.blocks.push_back(std::move(b));
    m.functions.push_back(std::move(main));
    return m;
}

Module address_equality_module() {
    Module m;
    Function main;
    main.name = "main";
    main.return_type = Type::pred();
    main.entry = 0;
    Block b;
    b.id = 0;
    b.ops = {
        FrameObject{1, 8, static_cast<Permissions>(PermRead | PermWrite)},
        Equal{2, 1, 1},
    };
    b.terminator = Return{2};
    main.blocks.push_back(std::move(b));
    m.functions.push_back(std::move(main));
    return m;
}

} // namespace

int main() {
    Verifier verifier;

    {
        auto m = memory_roundtrip_module();
        auto errors = verifier.verify(m);
        check(errors.empty(), "memory roundtrip module verifies");
        Interpreter vm(m);
        auto r = vm.run("main");
        check(r.succeeded(), "memory roundtrip executes");
        check(r.value && std::get<IntValue>(*r.value).bits == 0x1122334455667788ull, "little-endian roundtrip preserves value");
    }

    {
        auto m = stale_escape_module();
        check(verifier.verify(m).empty(), "escaped-local module is structurally valid");
        Interpreter vm(m);
        auto r = vm.run("main");
        check(!r.succeeded() && r.trap->reason == TrapReason::StaleAddress, "escaped frame address traps after invocation ends");
    }

    {
        auto m = uninitialized_module(InitPolicy::RuntimeTracked);
        check(verifier.verify(m).empty(), "runtime-tracked init module verifies");
        Interpreter vm(m);
        auto r = vm.run("main");
        check(!r.succeeded() && r.trap->reason == TrapReason::UninitializedRead, "runtime-tracked policy traps uninitialized read");
    }

    {
        auto m = uninitialized_module(InitPolicy::Zeroed);
        Interpreter vm(m);
        auto r = vm.run("main");
        check(r.succeeded(), "zeroed prototype policy permits first read");
        check(r.value && std::get<IntValue>(*r.value).bits == 0, "zeroed prototype policy yields zero");
    }

    {
        auto m = guard_module(false);
        Interpreter vm(m);
        auto r = vm.run("main");
        check(!r.succeeded() && r.trap->reason == TrapReason::GuardFailed, "false guard is a defined terminal trap");
    }

    {
        auto m = block_parameter_module();
        check(verifier.verify(m).empty(), "block-parameter SSA module verifies");
        Interpreter vm(m);
        auto r = vm.run("main");
        check(r.succeeded() && r.value && std::get<IntValue>(*r.value).bits == 42, "block parameter carries SSA value across control edge");
    }

    {
        auto m = invalid_cross_block_ssa_module();
        auto errors = verifier.verify(m);
        check(!errors.empty(), "verifier rejects cross-block SSA value not passed as block parameter");
    }

    {
        auto m = narrowing_module(true);
        Interpreter vm(m);
        auto r = vm.run("main");
        check(!r.succeeded() && r.trap->reason == TrapReason::AuthorityViolation, "addr.narrow cannot create new permission bits");
    }

    {
        auto m = checked_overflow_module(false);
        check(verifier.verify(m).empty(), "checked-overflow module verifies");
        Interpreter vm(m);
        auto r = vm.run("main");
        check(!r.succeeded() && r.trap->reason == TrapReason::CheckedOverflow, "checked i8 add traps on overflow");
    }

    {
        auto m = checked_overflow_module(true);
        Interpreter vm(m);
        auto r = vm.run("main");
        check(r.succeeded() && r.value && std::get<IntValue>(*r.value).bits == 0, "wrapping i8 add wraps explicitly to zero");
    }

    {
        auto m = permission_module();
        check(verifier.verify(m).empty(), "read-only narrowed capability is structurally valid");
        Interpreter vm(m);
        auto r = vm.run("main");
        check(!r.succeeded() && r.trap->reason == TrapReason::PermissionDenied, "store through read-only narrowed address traps");
    }

    {
        auto m = one_past_access_module();
        check(verifier.verify(m).empty(), "one-past prototype module verifies under declared profile");
        Interpreter vm(m);
        auto r = vm.run("main");
        check(!r.succeeded() && r.trap->reason == TrapReason::OutOfBounds, "one-past address may be derived by profile but not dereferenced");
    }

    {
        auto m = address_equality_module();
        auto errors = verifier.verify(m);
        check(!errors.empty(), "verifier keeps address equality unresolved instead of inventing semantics");
    }

    if (failures == 0) {
        std::cout << "All semantic-machine prototype tests passed.\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
