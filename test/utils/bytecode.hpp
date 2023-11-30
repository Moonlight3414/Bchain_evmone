// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019-2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <evmone/eof.hpp>
#include <evmone/instructions_traits.hpp>
#include <intx/intx.hpp>
#include <test/utils/utils.hpp>
#include <algorithm>
#include <ostream>
#include <stdexcept>

struct bytecode;

inline bytecode push(uint64_t n);
inline bytecode push(evmc::address addr);
inline bytecode push(evmc::bytes32 bs);

using enum evmone::Opcode;
using evmone::Opcode;

// TODO: Pull bytecode in evmone namespace
struct bytecode : bytes
{
    bytecode() noexcept = default;

    bytecode(bytes b) : bytes{std::move(b)} {}

    bytecode(const uint8_t* data, size_t size) : bytes{data, size} {}

    bytecode(Opcode opcode) : bytes{uint8_t(opcode)} {}

    template <typename T,
        typename = typename std::enable_if_t<std::is_convertible_v<T, std::string_view>>>
    bytecode(T hex) : bytes{from_spaced_hex(hex).value()}
    {}

    bytecode(uint64_t n) : bytes{push(n)} {}

    bytecode(evmc::address addr) : bytes{push(addr)} {}

    bytecode(evmc::bytes32 bs) : bytes{push(bs)} {}

    operator bytes_view() const noexcept { return {data(), size()}; }
};

inline bytecode operator+(bytecode a, bytecode b)
{
    return static_cast<bytes&>(a) + static_cast<bytes&>(b);
}

inline bytecode& operator+=(bytecode& a, bytecode b)
{
    return a = a + b;
}

inline bytecode& operator+=(bytecode& a, bytes b)
{
    return a = a + bytecode{b};
}

inline bool operator==(const bytecode& a, const bytecode& b) noexcept
{
    return static_cast<const bytes&>(a) == static_cast<const bytes&>(b);
}

inline std::ostream& operator<<(std::ostream& os, const bytecode& c)
{
    return os << hex(c);
}

inline bytecode operator*(int n, bytecode c)
{
    auto out = bytecode{};
    while (n-- > 0)
        out += c;
    return out;
}

inline bytecode operator*(int n, Opcode op)
{
    return n * bytecode{op};
}

template <typename T>
inline typename std::enable_if_t<std::is_same_v<T, uint16_t> || std::is_same_v<T, int16_t>, bytes>
big_endian(T value)
{
    return {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
}

struct eof_bytecode
{
private:
    std::vector<bytecode> m_codes;
    std::vector<evmone::EOFCodeType> m_types;
    bytecode m_data;

    /// Constructs EOF header bytes
    bytecode header() const
    {
        assert(!m_codes.empty());
        assert(m_codes[0].size() <= std::numeric_limits<uint16_t>::max());
        assert(m_data.size() <= std::numeric_limits<uint16_t>::max());
        assert(m_codes.size() == m_types.size());

        constexpr uint8_t version = 0x01;

        bytecode out{bytes{0xEF, 0x00, version}};

        // type header
        const auto types_size = static_cast<uint16_t>(m_types.size() * 4);
        out += "01" + big_endian(types_size);

        // codes header
        const auto code_count = static_cast<uint16_t>(m_codes.size());
        out += "02"_hex + big_endian(code_count);
        for (const auto& code : m_codes)
        {
            const auto code_size = static_cast<uint16_t>(code.size());
            out += big_endian(code_size);
        }

        // data header
        const auto data_size = static_cast<uint16_t>(m_data.size());
        out += "04" + big_endian(data_size);
        out += "00";  // terminator

        // types section
        for (const auto& type : m_types)
            out += bytes{type.inputs, type.outputs} + big_endian(type.max_stack_height);
        return out;
    }

public:
    explicit eof_bytecode(bytecode code, uint16_t max_stack_height = 0)
      : m_codes{std::move(code)}, m_types{{0, 0x80, max_stack_height}}
    {}

    auto& code(bytecode c, uint8_t inputs, uint8_t outputs, uint16_t max_stack_height)
    {
        m_codes.emplace_back(std::move(c));
        m_types.emplace_back(inputs, outputs, max_stack_height);
        return *this;
    }

    auto& data(bytecode d)
    {
        m_data = std::move(d);
        return *this;
    }

    operator bytecode() const
    {
        bytecode out{header()};
        for (const auto& code : m_codes)
            out += code;
        out += m_data;
        return out;
    }
};

inline bytecode push0()
{
    return OP_PUSH0;
}

inline bytecode push(bytes_view data)
{
    if (data.empty())
        throw std::invalid_argument{"push data empty"};
    if (data.size() > (OP_PUSH32 - OP_PUSH1 + 1))
        throw std::invalid_argument{"push data too long"};
    return Opcode(data.size() + OP_PUSH1 - 1) + bytes{data};
}

inline bytecode push(std::string_view hex_data)
{
    return push(from_spaced_hex(hex_data).value());
}

inline bytecode push(const intx::uint256& value)
{
    uint8_t data[sizeof(value)]{};
    intx::be::store(data, value);
    return push({data, std::size(data)});
}

bytecode push(Opcode opcode) = delete;

inline bytecode push(Opcode opcode, const bytecode& data)
{
    if (opcode < OP_PUSH1 || opcode > OP_PUSH32)
        throw std::invalid_argument{"invalid push opcode " + std::to_string(opcode)};

    const auto num_instr_bytes = static_cast<size_t>(opcode) - OP_PUSH1 + 1;
    if (data.size() > num_instr_bytes)
        throw std::invalid_argument{"push data too long"};

    const auto instr_bytes = bytes(num_instr_bytes - data.size(), 0) + bytes{data};
    return opcode + instr_bytes;
}

inline bytecode push(uint64_t n)
{
    auto data = bytes{};
    for (; n != 0; n >>= 8)
        data.push_back(uint8_t(n));
    std::reverse(data.begin(), data.end());
    if (data.empty())
        data.push_back(0);
    return push(data);
}

inline bytecode push(evmc::bytes32 bs)
{
    bytes_view data{bs.bytes, sizeof(bs.bytes)};
    return push(data.substr(std::min(data.find_first_not_of(uint8_t{0}), size_t{31})));
}

inline bytecode push(evmc::address addr)
{
    return push({std::data(addr.bytes), std::size(addr.bytes)});
}

inline bytecode dup1(bytecode c)
{
    return c + OP_DUP1;
}

inline bytecode add(bytecode a, bytecode b)
{
    return b + a + OP_ADD;
}

inline bytecode add(bytecode a)
{
    return a + OP_ADD;
}

inline bytecode mul(bytecode a, bytecode b)
{
    return b + a + OP_MUL;
}

inline bytecode not_(bytecode a)
{
    return a + OP_NOT;
}

inline bytecode iszero(bytecode a)
{
    return a + OP_ISZERO;
}

inline bytecode eq(bytecode a, bytecode b)
{
    return b + a + OP_EQ;
}

inline bytecode byte(bytecode a, bytecode n)
{
    return a + n + OP_BYTE;
}

inline bytecode mload(bytecode index)
{
    return index + OP_MLOAD;
}

inline bytecode mstore(bytecode index)
{
    return index + OP_MSTORE;
}

inline bytecode mstore(bytecode index, bytecode value)
{
    return value + index + OP_MSTORE;
}

inline bytecode mstore8(bytecode index)
{
    return index + OP_MSTORE8;
}

inline bytecode mstore8(bytecode index, bytecode value)
{
    return value + index + OP_MSTORE8;
}

inline bytecode mcopy(bytecode dst, bytecode src, bytecode length)
{
    return length + src + dst + OP_MCOPY;
}

inline bytecode jump(bytecode target)
{
    return target + OP_JUMP;
}

inline bytecode jumpi(bytecode target, bytecode condition)
{
    return condition + target + OP_JUMPI;
}

inline bytecode rjump(int16_t offset)
{
    return OP_RJUMP + big_endian(offset);
}

inline bytecode rjumpi(int16_t offset, bytecode condition)
{
    return condition + (OP_RJUMPI + big_endian(offset));
}

inline bytecode rjumpv(const std::initializer_list<int16_t> offsets, bytecode condition)
{
    assert(offsets.size() > 0);
    bytecode ret = condition + OP_RJUMPV + static_cast<Opcode>(offsets.size() - 1);
    for (const auto offset : offsets)
        ret += bytecode{big_endian(offset)};
    return ret;
}

inline bytecode ret(bytecode index, bytecode size)
{
    return size + index + OP_RETURN;
}

inline bytecode ret_top()
{
    return mstore(0) + ret(0, 0x20);
}

inline bytecode ret(bytecode c)
{
    return c + ret_top();
}

inline bytecode revert(bytecode index, bytecode size)
{
    return size + index + OP_REVERT;
}

inline bytecode selfdestruct(bytecode beneficiary)
{
    return std::move(beneficiary) + OP_SELFDESTRUCT;
}

inline bytecode keccak256(bytecode index, bytecode size)
{
    return size + index + OP_KECCAK256;
}

inline bytecode calldataload(bytecode index)
{
    return index + OP_CALLDATALOAD;
}

inline bytecode calldatasize()
{
    return OP_CALLDATASIZE;
}

inline bytecode calldatacopy(bytecode dst, bytecode src, bytecode size)
{
    return std::move(size) + std::move(src) + std::move(dst) + OP_CALLDATACOPY;
}

inline bytecode returndatasize()
{
    return OP_RETURNDATASIZE;
}

inline bytecode returndatacopy(bytecode dst, bytecode src, bytecode size)
{
    return std::move(size) + std::move(src) + std::move(dst) + OP_RETURNDATACOPY;
}

inline bytecode sstore(bytecode index, bytecode value)
{
    return value + index + OP_SSTORE;
}

inline bytecode sload(bytecode index)
{
    return index + OP_SLOAD;
}

inline bytecode tstore(bytecode index, bytecode value)
{
    return value + index + OP_TSTORE;
}

inline bytecode tload(bytecode index)
{
    return index + OP_TLOAD;
}

inline bytecode blockhash(bytecode number)
{
    return number + OP_BLOCKHASH;
}

inline bytecode blobhash(bytecode index)
{
    return index + OP_BLOBHASH;
}

inline bytecode setupx(
    bytecode num_val_slots, bytecode mod_size, bytecode mod_offset, bytecode mod_id)
{
    return num_val_slots + mod_size + mod_offset + mod_id + OP_SETUPX;
}

inline bytecode storex(bytecode num_vals, bytecode val_offset, bytecode val_slot)
{
    return num_vals + val_offset + val_slot + OP_STOREX;
}

inline bytecode loadx(bytecode num_vals, bytecode val_idx, bytecode mem_offset)
{
    return num_vals + val_idx + mem_offset + OP_LOADX;
}

inline bytecode addmodx(uint8_t result_slot_idx, uint8_t x_slot_idx, uint8_t y_slot_idx)
{
    return hex(OP_ADDMODX) + hex(result_slot_idx) + hex(x_slot_idx) + hex(y_slot_idx);
}

inline bytecode submodx(uint8_t result_slot_idx, uint8_t x_slot_idx, uint8_t y_slot_idx)
{
    return hex(OP_SUBMODX) + hex(result_slot_idx) + hex(x_slot_idx) + hex(y_slot_idx);
}

inline bytecode mulmodx(uint8_t result_slot_idx, uint8_t x_slot_idx, uint8_t y_slot_idx)
{
    return hex(OP_MULMODX) + hex(result_slot_idx) + hex(x_slot_idx) + hex(y_slot_idx);
}

template <Opcode kind>
struct call_instruction
{
private:
    bytecode m_address = 0;
    bytecode m_gas = 0;
    bytecode m_value = 0;
    bytecode m_input = 0;
    bytecode m_input_size = 0;
    bytecode m_output = 0;
    bytecode m_output_size = 0;

public:
    explicit call_instruction(bytecode address) : m_address{std::move(address)} {}

    auto& gas(bytecode g)
    {
        m_gas = std::move(g);
        return *this;
    }


    template <Opcode k = kind>
    typename std::enable_if<k == OP_CALL || k == OP_CALLCODE, call_instruction&>::type value(
        bytecode v)
    {
        m_value = std::move(v);
        return *this;
    }

    auto& input(bytecode index, bytecode size)
    {
        m_input = std::move(index);
        m_input_size = std::move(size);
        return *this;
    }

    auto& output(bytecode index, bytecode size)
    {
        m_output = std::move(index);
        m_output_size = std::move(size);
        return *this;
    }

    operator bytecode() const
    {
        auto code = m_output_size + m_output + m_input_size + m_input;
        if constexpr (kind == OP_CALL || kind == OP_CALLCODE)
            code += m_value;
        code += m_address + m_gas + kind;
        return code;
    }
};

inline call_instruction<OP_DELEGATECALL> delegatecall(bytecode address)
{
    return call_instruction<OP_DELEGATECALL>{std::move(address)};
}

inline call_instruction<OP_STATICCALL> staticcall(bytecode address)
{
    return call_instruction<OP_STATICCALL>{std::move(address)};
}

inline call_instruction<OP_CALL> call(bytecode address)
{
    return call_instruction<OP_CALL>{std::move(address)};
}

inline call_instruction<OP_CALLCODE> callcode(bytecode address)
{
    return call_instruction<OP_CALLCODE>{std::move(address)};
}


template <Opcode kind>
struct create_instruction
{
private:
    bytecode m_value = 0;
    bytecode m_input = 0;
    bytecode m_input_size = 0;
    bytecode m_salt = 0;

public:
    auto& value(bytecode v)
    {
        m_value = std::move(v);
        return *this;
    }

    auto& input(bytecode index, bytecode size)
    {
        m_input = std::move(index);
        m_input_size = std::move(size);
        return *this;
    }

    template <Opcode k = kind>
    typename std::enable_if<k == OP_CREATE2, create_instruction&>::type salt(bytecode salt)
    {
        m_salt = std::move(salt);
        return *this;
    }

    operator bytecode() const
    {
        bytecode code;
        if constexpr (kind == OP_CREATE2)
            code += m_salt;
        code += m_input_size + m_input + m_value + kind;
        return code;
    }
};

inline auto create()
{
    return create_instruction<OP_CREATE>{};
}

inline auto create2()
{
    return create_instruction<OP_CREATE2>{};
}


inline std::string hex(Opcode opcode) noexcept
{
    return hex(static_cast<uint8_t>(opcode));
}

inline std::string decode(bytes_view bytecode)
{
    auto s = std::string{"bytecode{}"};
    for (auto it = bytecode.begin(); it != bytecode.end(); ++it)
    {
        const auto opcode = *it;
        if (const auto name = evmone::instr::traits[opcode].name; name)
        {
            s += std::string{" + OP_"} + name;

            if (size_t imm_size = evmone::instr::traits[opcode].immediate_size; imm_size > 0)
            {
                const auto imm_start = it + 1;
                imm_size = std::min(imm_size, static_cast<size_t>(bytecode.end() - imm_start));

                if (imm_size != 0)
                {
                    s += " + \"" + hex({&*imm_start, imm_size}) + '"';
                    it += imm_size;
                }
            }
        }
        else
            s += " + \"" + hex(opcode) + '"';
    }

    return s;
}
