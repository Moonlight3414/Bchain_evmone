// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2018-2019 The evmone Authors.
// Licensed under the Apache License, Version 2.0.

#include <evmc/instructions.h>
#include <evmone/analysis.hpp>
#include <gtest/gtest.h>
#include <test/utils/bytecode.hpp>
#include <test/utils/utils.hpp>

using namespace evmone;

constexpr auto rev = EVMC_BYZANTIUM;

const auto fake_fn_table = []() noexcept
{
    evmone::exec_fn_table fns;
    for (size_t i = 0; i < fns.size(); ++i)
        fns[i] = (evmone::exec_fn)i;
    return fns;
}
();


TEST(analysis, example1)
{
    const auto code = push(0x2a) + push(0x1e) + OP_MSTORE8 + OP_MSIZE + push(0) + OP_SSTORE;
    const auto analysis = analyze(fake_fn_table, rev, &code[0], code.size());

    ASSERT_EQ(analysis.instrs.size(), 12);

    EXPECT_EQ(analysis.instrs[0].fn, fake_fn_table[OPX_BEGINBLOCK]);
    EXPECT_EQ(analysis.instrs[2].fn, fake_fn_table[OP_PUSH1]);
    EXPECT_EQ(analysis.instrs[3].small_push_value, 0x2a);
    EXPECT_EQ(analysis.instrs[4].fn, fake_fn_table[OP_PUSH1]);
    EXPECT_EQ(analysis.instrs[5].small_push_value, 0x1e);
    EXPECT_EQ(analysis.instrs[6].fn, fake_fn_table[OP_MSTORE8]);
    EXPECT_EQ(analysis.instrs[7].fn, fake_fn_table[OP_MSIZE]);
    EXPECT_EQ(analysis.instrs[8].fn, fake_fn_table[OP_PUSH1]);
    EXPECT_EQ(analysis.instrs[10].fn, fake_fn_table[OP_SSTORE]);
    EXPECT_EQ(analysis.instrs[11].fn, fake_fn_table[OP_STOP]);

    EXPECT_EQ(analysis.instrs[1].block.gas_cost, 14);
    EXPECT_EQ(analysis.instrs[1].block.stack_req, 0);
    EXPECT_EQ(analysis.instrs[1].block.stack_max_growth, 2);
}

TEST(analysis, stack_up_and_down)
{
    const auto code = OP_DUP2 + 6 * OP_DUP1 + 10 * OP_POP + push(0);
    const auto analysis = analyze(fake_fn_table, rev, &code[0], code.size());

    ASSERT_EQ(analysis.instrs.size(), 22);
    EXPECT_EQ(analysis.instrs[0].fn, fake_fn_table[OPX_BEGINBLOCK]);
    EXPECT_EQ(analysis.instrs[2].fn, fake_fn_table[OP_DUP2]);
    EXPECT_EQ(analysis.instrs[3].fn, fake_fn_table[OP_DUP1]);
    EXPECT_EQ(analysis.instrs[9].fn, fake_fn_table[OP_POP]);
    EXPECT_EQ(analysis.instrs[19].fn, fake_fn_table[OP_PUSH1]);
    EXPECT_EQ(analysis.instrs[20].small_push_value, 0);

    EXPECT_EQ(analysis.instrs[1].block.gas_cost, 7 * 3 + 10 * 2 + 3);
    EXPECT_EQ(analysis.instrs[1].block.stack_req, 3);
    EXPECT_EQ(analysis.instrs[1].block.stack_max_growth, 7);
}

TEST(analysis, push)
{
    constexpr auto push_value = 0x8807060504030201;
    const auto code = push(push_value) + "7f00ee";
    const auto analysis = analyze(fake_fn_table, rev, &code[0], code.size());

    ASSERT_EQ(analysis.instrs.size(), 7);
    ASSERT_EQ(analysis.push_values.size(), 1);
    EXPECT_EQ(analysis.instrs[0].fn, fake_fn_table[OPX_BEGINBLOCK]);
    EXPECT_EQ(analysis.instrs[1].block.gas_cost, 6);
    EXPECT_EQ(analysis.instrs[2].fn, fake_fn_table[OP_PUSH8]);
    EXPECT_EQ(analysis.instrs[3].small_push_value, push_value);
    EXPECT_EQ(analysis.instrs[4].fn, fake_fn_table[OP_PUSH32]);
    EXPECT_EQ(analysis.instrs[5].push_value, &analysis.push_values[0]);
    EXPECT_EQ(analysis.push_values[0], intx::uint256{0xee} << 240);
}

TEST(analysis, jumpdest_skip)
{
    // If the JUMPDEST is the first instruction in a basic block it should be just omitted
    // and no new block should be created in this place.

    const auto code = bytecode{} + OP_STOP + OP_JUMPDEST;
    auto analysis = evmone::analyze(fake_fn_table, rev, &code[0], code.size());

    ASSERT_EQ(analysis.instrs.size(), 6);
    EXPECT_EQ(analysis.instrs[0].fn, fake_fn_table[OPX_BEGINBLOCK]);
    EXPECT_EQ(analysis.instrs[1].block.gas_cost, 0);
    EXPECT_EQ(analysis.instrs[2].fn, fake_fn_table[OP_STOP]);
    EXPECT_EQ(analysis.instrs[3].fn, fake_fn_table[OP_JUMPDEST]);
    EXPECT_EQ(analysis.instrs[4].block.gas_cost, 1);
    EXPECT_EQ(analysis.instrs[5].fn, fake_fn_table[OP_STOP]);
}

TEST(analysis, jump1)
{
    const auto code = jump(add(4, 2)) + OP_JUMPDEST + mstore(0, 3) + ret(0, 0x20) + jump(6);
    const auto analysis = analyze(fake_fn_table, rev, &code[0], code.size());

    ASSERT_EQ(analysis.jumpdest_offsets.size(), 1);
    ASSERT_EQ(analysis.jumpdest_targets.size(), 1);
    EXPECT_EQ(analysis.jumpdest_offsets[0], 6);
    EXPECT_EQ(analysis.jumpdest_targets[0], 8);
    EXPECT_EQ(find_jumpdest(analysis, 6), 8);
    EXPECT_EQ(find_jumpdest(analysis, 0), -1);
    EXPECT_EQ(find_jumpdest(analysis, 7), -1);
}

TEST(analysis, empty)
{
    bytes code;
    auto analysis = evmone::analyze(fake_fn_table, rev, &code[0], code.size());

    EXPECT_EQ(analysis.instrs.size(), 3);
    EXPECT_EQ(analysis.instrs[0].fn, fake_fn_table[OPX_BEGINBLOCK]);
    EXPECT_EQ(analysis.instrs[1].block.gas_cost, 0);
    EXPECT_EQ(analysis.instrs[2].fn, fake_fn_table[OP_STOP]);
}

TEST(analysis, only_jumpdest)
{
    const auto code = bytecode{OP_JUMPDEST};
    auto analysis = evmone::analyze(fake_fn_table, rev, &code[0], code.size());

    ASSERT_EQ(analysis.jumpdest_offsets.size(), 1);
    ASSERT_EQ(analysis.jumpdest_targets.size(), 1);
    EXPECT_EQ(analysis.jumpdest_offsets[0], 0);
    EXPECT_EQ(analysis.jumpdest_targets[0], 0);
}

TEST(analysis, jumpi_at_the_end)
{
    const auto code = bytecode{OP_JUMPI};
    auto analysis = evmone::analyze(fake_fn_table, rev, &code[0], code.size());

    ASSERT_EQ(analysis.instrs.size(), 6);
    EXPECT_EQ(analysis.instrs[0].fn, fake_fn_table[OPX_BEGINBLOCK]);
    EXPECT_EQ(analysis.instrs[2].fn, fake_fn_table[OP_JUMPI]);
    EXPECT_EQ(analysis.instrs[3].fn, fake_fn_table[OPX_BEGINBLOCK]);
    EXPECT_EQ(analysis.instrs[5].fn, fake_fn_table[OP_STOP]);
}

TEST(analysis, terminated_last_block)
{
    // TODO: Even if the last basic block is properly terminated an additional artificial block
    // is going to be created with only STOP instruction.
    const auto code = ret(0, 0);
    auto analysis = evmone::analyze(fake_fn_table, rev, &code[0], code.size());

    ASSERT_EQ(analysis.instrs.size(), 10);
    EXPECT_EQ(analysis.instrs[0].fn, fake_fn_table[OPX_BEGINBLOCK]);
    EXPECT_EQ(analysis.instrs[6].fn, fake_fn_table[OP_RETURN]);
    EXPECT_EQ(analysis.instrs[7].fn, fake_fn_table[OPX_BEGINBLOCK]);
    EXPECT_EQ(analysis.instrs[9].fn, fake_fn_table[OP_STOP]);
}

TEST(analysis, jumpdests_groups)
{
    const auto code = 3 * OP_JUMPDEST + push(1) + 3 * OP_JUMPDEST + push(2) + OP_JUMPI;
    auto analysis = evmone::analyze(fake_fn_table, rev, &code[0], code.size());

    ASSERT_EQ(analysis.instrs.size(), 20);
    EXPECT_EQ(analysis.instrs[0].fn, fake_fn_table[OP_JUMPDEST]);
    EXPECT_EQ(analysis.instrs[2].fn, fake_fn_table[OP_JUMPDEST]);
    EXPECT_EQ(analysis.instrs[4].fn, fake_fn_table[OP_JUMPDEST]);
    EXPECT_EQ(analysis.instrs[6].fn, fake_fn_table[OP_PUSH1]);
    EXPECT_EQ(analysis.instrs[8].fn, fake_fn_table[OP_JUMPDEST]);
    EXPECT_EQ(analysis.instrs[10].fn, fake_fn_table[OP_JUMPDEST]);
    EXPECT_EQ(analysis.instrs[12].fn, fake_fn_table[OP_JUMPDEST]);
    EXPECT_EQ(analysis.instrs[14].fn, fake_fn_table[OP_PUSH1]);
    EXPECT_EQ(analysis.instrs[16].fn, fake_fn_table[OP_JUMPI]);
    EXPECT_EQ(analysis.instrs[17].fn, fake_fn_table[OPX_BEGINBLOCK]);
    EXPECT_EQ(analysis.instrs[19].fn, fake_fn_table[OP_STOP]);


    ASSERT_EQ(analysis.jumpdest_offsets.size(), 6);
    ASSERT_EQ(analysis.jumpdest_targets.size(), 6);
    EXPECT_EQ(analysis.jumpdest_offsets[0], 0);
    EXPECT_EQ(analysis.jumpdest_targets[0], 0);
    EXPECT_EQ(analysis.jumpdest_offsets[1], 1);
    EXPECT_EQ(analysis.jumpdest_targets[1], 2);
    EXPECT_EQ(analysis.jumpdest_offsets[2], 2);
    EXPECT_EQ(analysis.jumpdest_targets[2], 4);
    EXPECT_EQ(analysis.jumpdest_offsets[3], 5);
    EXPECT_EQ(analysis.jumpdest_targets[3], 8);
    EXPECT_EQ(analysis.jumpdest_offsets[4], 6);
    EXPECT_EQ(analysis.jumpdest_targets[4], 10);
    EXPECT_EQ(analysis.jumpdest_offsets[5], 7);
    EXPECT_EQ(analysis.jumpdest_targets[5], 12);
}