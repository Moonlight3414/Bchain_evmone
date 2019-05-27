// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// Licensed under the Apache License, Version 2.0.

#pragma once

#include <evmone/evmone.h>

#include <gtest/gtest.h>
#include <test/utils/host_mock.hpp>

#define EXPECT_STATUS(STATUS_CODE)                                           \
    EXPECT_EQ(result.status_code, STATUS_CODE);                              \
    if constexpr (STATUS_CODE != EVMC_SUCCESS && STATUS_CODE != EVMC_REVERT) \
    {                                                                        \
        EXPECT_EQ(result.gas_left, 0);                                       \
    }                                                                        \
    (void)0

#define EXPECT_GAS_USED(STATUS_CODE, GAS_USED)  \
    EXPECT_EQ(result.status_code, STATUS_CODE); \
    EXPECT_EQ(gas_used, GAS_USED)

#define EXPECT_OUTPUT_INT(X)                                                                       \
    {                                                                                              \
        ASSERT_EQ(result.output_size, sizeof(intx::uint256));                                      \
        evmc_bytes32 x;                                                                            \
        intx::be::store(x.bytes, intx::uint256{X});                                                \
        EXPECT_EQ(                                                                                 \
            to_hex({result.output_data, result.output_size}), to_hex({x.bytes, sizeof(x.bytes)})); \
    }                                                                                              \
    (void)0

class execution : public testing::Test, public MockedHost
{
protected:
    evmc_instance* vm = nullptr;
    evmc_revision rev = EVMC_BYZANTIUM;  // Use Byzantium by default.
    evmc_message msg = {};
    evmc_result result = {};
    int64_t gas_used = 0;

    execution() noexcept : vm{evmc_create_evmone()} {}

    ~execution() noexcept override
    {
        // Release the attached EVM execution result.
        if (result.release)
            result.release(&result);
    }

    /// Wrapper for evmone::execute. The result will be in the .result field.
    void execute(int64_t gas, bytes_view code, std::string_view input_hex = {}) noexcept
    {
        auto input = from_hex(input_hex);
        msg.gas = gas;
        msg.input_data = input.data();
        msg.input_size = input.size();
        execute(msg, code);
    }

    /// Wrapper for evmone::execute. The result will be in the .result field.
    void execute(int64_t gas, std::string_view code_hex, std::string_view input_hex = {}) noexcept
    {
        execute(gas, from_hex(code_hex), input_hex);
    }

    void execute(bytes_view code, std::string_view input_hex = {}) noexcept
    {
        execute(std::numeric_limits<int64_t>::max(), code, input_hex);
    }

    void execute(std::string_view code_hex, std::string_view input_hex = {}) noexcept
    {
        execute(std::numeric_limits<int64_t>::max(), code_hex, input_hex);
    }

    /// Wrapper for evmone::execute. The result will be in the .result field.
    void execute(const evmc_message& m, bytes_view code) noexcept
    {
        // Release previous result.
        if (result.release)
            result.release(&result);

        result = vm->execute(vm, this, rev, &m, &code[0], code.size());
        gas_used = m.gas - result.gas_left;
    }
};
