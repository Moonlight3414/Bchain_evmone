// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state.hpp"
#include "../utils/stdx/utility.hpp"
#include "host.hpp"
#include "state_view.hpp"
#include <evmone/constants.hpp>
#include <evmone/eof.hpp>
#include <algorithm>

namespace evmone::state
{
namespace
{
constexpr int64_t num_words(size_t size_in_bytes) noexcept
{
    return static_cast<int64_t>((size_in_bytes + 31) / 32);
}

size_t compute_tx_data_tokens(evmc_revision rev, bytes_view data) noexcept
{
    const auto num_zero_bytes = static_cast<size_t>(std::ranges::count(data, 0));
    const auto num_nonzero_bytes = data.size() - num_zero_bytes;

    const size_t nonzero_byte_multiplier = rev >= EVMC_ISTANBUL ? 4 : 17;
    return (nonzero_byte_multiplier * num_nonzero_bytes) + num_zero_bytes;
}

int64_t compute_access_list_cost(const AccessList& access_list) noexcept
{
    static constexpr auto ADDRESS_COST = 2400;
    static constexpr auto STORAGE_KEY_COST = 1900;

    int64_t cost = 0;
    for (const auto& [_, keys] : access_list)
        cost += ADDRESS_COST + static_cast<int64_t>(keys.size()) * STORAGE_KEY_COST;
    return cost;
}

int64_t compute_tx_intrinsic_cost(evmc_revision rev, const Transaction& tx) noexcept
{
    static constexpr auto TX_BASE_COST = 21000;
    static constexpr auto TX_CREATE_COST = 32000;
    static constexpr auto DATA_TOKEN_COST = 4;
    static constexpr auto INITCODE_WORD_COST = 2;

    const auto is_create = !tx.to.has_value();  // Covers also EOF creation transactions.

    const auto create_cost = (is_create && rev >= EVMC_HOMESTEAD) ? TX_CREATE_COST : 0;

    const auto num_data_tokens = static_cast<int64_t>(compute_tx_data_tokens(rev, tx.data));
    const auto data_cost = num_data_tokens * DATA_TOKEN_COST;

    const auto access_list_cost = compute_access_list_cost(tx.access_list);

    const auto initcode_cost =
        (is_create && rev >= EVMC_SHANGHAI) ? INITCODE_WORD_COST * num_words(tx.data.size()) : 0;

    const auto intrinsic_cost =
        TX_BASE_COST + create_cost + data_cost + access_list_cost + initcode_cost;
    return intrinsic_cost;
}

evmc_message build_message(
    const Transaction& tx, int64_t execution_gas_limit, evmc_revision rev) noexcept
{
    const auto recipient = tx.to.has_value() ? *tx.to : evmc::address{};

    const auto is_legacy_eof_create =
        rev >= EVMC_OSAKA && !tx.to.has_value() && is_eof_container(tx.data);

    return {.kind = is_legacy_eof_create ? EVMC_EOFCREATE :
                    tx.to.has_value()    ? EVMC_CALL :
                                           EVMC_CREATE,
        .flags = 0,
        .depth = 0,
        .gas = execution_gas_limit,
        .recipient = recipient,
        .sender = tx.sender,
        .input_data = tx.data.data(),
        .input_size = tx.data.size(),
        .value = intx::be::store<evmc::uint256be>(tx.value),
        .create2_salt = {},
        .code_address = recipient,
        .code = nullptr,
        .code_size = 0};
}
}  // namespace

StateDiff State::build_diff(evmc_revision rev) const
{
    StateDiff diff;
    for (const auto& [addr, m] : m_modified)
    {
        if (m.destructed)
        {
            // TODO: This must be done even for just_created
            //   because destructed may pre-date just_created. Add test to evmone (EEST has it).
            diff.deleted_accounts.emplace_back(addr);
            continue;
        }
        if (m.erase_if_empty && rev >= EVMC_SPURIOUS_DRAGON && m.is_empty())
        {
            if (!m.just_created)  // Don't report just created accounts
                diff.deleted_accounts.emplace_back(addr);
            continue;
        }

        // Unconditionally report nonce and balance as modified.
        // TODO: We don't have information if the balance/nonce has actually changed.
        //   One option is to just keep the original values. This may be handy for RPC.
        // TODO(clang): In old Clang emplace_back without Account doesn't compile.
        //   NOLINTNEXTLINE(modernize-use-emplace)
        auto& a = diff.modified_accounts.emplace_back(StateDiff::Entry{addr, m.nonce, m.balance});

        // Output only the new code.
        // TODO: Output also the code hash. It will be needed for DB update and MPT hash.
        if (m.just_created && !m.code.empty())
            a.code = m.code;

        for (const auto& [k, v] : m.storage)
        {
            if (v.current != v.original)
                a.modified_storage.emplace_back(k, v.current);
        }
    }
    return diff;
}

Account& State::insert(const address& addr, Account account)
{
    const auto r = m_modified.insert({addr, std::move(account)});
    assert(r.second);
    return r.first->second;
}

Account* State::find(const address& addr) noexcept
{
    // TODO: Avoid double lookup (find+insert) and not cached initial state lookup for non-existent
    //   accounts. If we want to cache non-existent account we need a proper flag for it.
    if (const auto it = m_modified.find(addr); it != m_modified.end())
        return &it->second;
    if (const auto cacc = m_initial.get_account(addr); cacc)
        return &insert(addr, {.nonce = cacc->nonce,
                                 .balance = cacc->balance,
                                 .code_hash = cacc->code_hash,
                                 .has_initial_storage = cacc->has_storage});
    return nullptr;
}

Account& State::get(const address& addr) noexcept
{
    auto acc = find(addr);
    assert(acc != nullptr);
    return *acc;
}

Account& State::get_or_insert(const address& addr, Account account)
{
    if (const auto acc = find(addr); acc != nullptr)
        return *acc;
    return insert(addr, std::move(account));
}

bytes_view State::get_code(const address& addr)
{
    auto* a = find(addr);
    if (a == nullptr)
        return {};
    if (a->code_hash == Account::EMPTY_CODE_HASH)
        return {};
    if (a->code.empty())
        a->code = m_initial.get_account_code(addr);
    return a->code;
}

Account& State::touch(const address& addr)
{
    auto& acc = get_or_insert(addr, {.erase_if_empty = true});
    if (!acc.erase_if_empty && acc.is_empty())
    {
        acc.erase_if_empty = true;
        m_journal.emplace_back(JournalTouched{addr});
    }
    return acc;
}

StorageValue& State::get_storage(const address& addr, const bytes32& key)
{
    // TODO: Avoid account lookup by giving the reference to the account's storage to Host.
    auto& acc = get(addr);
    const auto [it, missing] = acc.storage.try_emplace(key);
    if (missing)
    {
        const auto initial_value = m_initial.get_storage(addr, key);
        it->second = {initial_value, initial_value};
    }
    return it->second;
}

void State::journal_balance_change(const address& addr, const intx::uint256& prev_balance)
{
    m_journal.emplace_back(JournalBalanceChange{{addr}, prev_balance});
}

void State::journal_storage_change(
    const address& addr, const bytes32& key, const StorageValue& value)
{
    m_journal.emplace_back(JournalStorageChange{{addr}, key, value.current, value.access_status});
}

void State::journal_transient_storage_change(
    const address& addr, const bytes32& key, const bytes32& value)
{
    m_journal.emplace_back(JournalTransientStorageChange{{addr}, key, value});
}

void State::journal_bump_nonce(const address& addr)
{
    m_journal.emplace_back(JournalNonceBump{addr});
}

void State::journal_create(const address& addr, bool existed)
{
    m_journal.emplace_back(JournalCreate{{addr}, existed});
}

void State::journal_destruct(const address& addr)
{
    m_journal.emplace_back(JournalDestruct{addr});
}

void State::journal_access_account(const address& addr)
{
    m_journal.emplace_back(JournalAccessAccount{addr});
}

void State::rollback(size_t checkpoint)
{
    while (m_journal.size() != checkpoint)
    {
        std::visit(
            [this](const auto& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, JournalNonceBump>)
                {
                    get(e.addr).nonce -= 1;
                }
                else if constexpr (std::is_same_v<T, JournalTouched>)
                {
                    get(e.addr).erase_if_empty = false;
                }
                else if constexpr (std::is_same_v<T, JournalDestruct>)
                {
                    get(e.addr).destructed = false;
                }
                else if constexpr (std::is_same_v<T, JournalAccessAccount>)
                {
                    get(e.addr).access_status = EVMC_ACCESS_COLD;
                }
                else if constexpr (std::is_same_v<T, JournalCreate>)
                {
                    if (e.existed)
                    {
                        // This account is not always "touched". TODO: Why?
                        auto& a = get(e.addr);
                        a.nonce = 0;
                        a.code_hash = Account::EMPTY_CODE_HASH;
                        a.code.clear();
                    }
                    else
                    {
                        // TODO: Before Spurious Dragon we don't clear empty accounts ("erasable")
                        //       so we need to delete them here explicitly.
                        //       This should be changed by tuning "erasable" flag
                        //       and clear in all revisions.
                        m_modified.erase(e.addr);
                    }
                }
                else if constexpr (std::is_same_v<T, JournalStorageChange>)
                {
                    auto& s = get(e.addr).storage.find(e.key)->second;
                    s.current = e.prev_value;
                    s.access_status = e.prev_access_status;
                }
                else if constexpr (std::is_same_v<T, JournalTransientStorageChange>)
                {
                    auto& s = get(e.addr).transient_storage.find(e.key)->second;
                    s = e.prev_value;
                }
                else if constexpr (std::is_same_v<T, JournalBalanceChange>)
                {
                    get(e.addr).balance = e.prev_balance;
                }
                else
                {
                    // TODO(C++23): Change condition to `false` once CWG2518 is in.
                    static_assert(std::is_void_v<T>, "unhandled journal entry type");
                }
            },
            m_journal.back());
        m_journal.pop_back();
    }
}

/// Validates transaction and computes its execution gas limit (the amount of gas provided to EVM).
/// @return  Execution gas limit or transaction validation error.
std::variant<TransactionProperties, std::error_code> validate_transaction(
    const StateView& state_view, const BlockInfo& block, const Transaction& tx, evmc_revision rev,
    int64_t block_gas_left, int64_t blob_gas_left) noexcept
{
    switch (tx.type)
    {
    case Transaction::Type::blob:
        if (rev < EVMC_CANCUN)
            return make_error_code(TX_TYPE_NOT_SUPPORTED);
        if (!tx.to.has_value())
            return make_error_code(CREATE_BLOB_TX);
        if (tx.blob_hashes.empty())
            return make_error_code(EMPTY_BLOB_HASHES_LIST);

        assert(block.blob_base_fee.has_value());
        if (tx.max_blob_gas_price < *block.blob_base_fee)
            return make_error_code(FEE_CAP_LESS_THEN_BLOCKS);

        if (std::ranges::any_of(tx.blob_hashes, [](const auto& h) { return h.bytes[0] != 0x01; }))
            return make_error_code(INVALID_BLOB_HASH_VERSION);
        if (std::cmp_greater(tx.blob_gas_used(), blob_gas_left))
            return make_error_code(BLOB_GAS_LIMIT_EXCEEDED);
        break;

    default:;
    }

    switch (tx.type)
    {
    case Transaction::Type::set_code:
        if (rev < EVMC_PRAGUE)
            return make_error_code(TX_TYPE_NOT_SUPPORTED);
        [[fallthrough]];

    case Transaction::Type::blob:
    case Transaction::Type::eip1559:
        if (rev < EVMC_LONDON)
            return make_error_code(TX_TYPE_NOT_SUPPORTED);

        if (tx.max_priority_gas_price > tx.max_gas_price)
            return make_error_code(TIP_GT_FEE_CAP);  // Priority gas price is too high.
        [[fallthrough]];

    case Transaction::Type::access_list:
        if (rev < EVMC_BERLIN)
            return make_error_code(TX_TYPE_NOT_SUPPORTED);
        [[fallthrough]];

    case Transaction::Type::legacy:;
    }

    assert(tx.max_priority_gas_price <= tx.max_gas_price);

    if (tx.gas_limit > block_gas_left)
        return make_error_code(GAS_LIMIT_REACHED);

    if (tx.max_gas_price < block.base_fee)
        return make_error_code(FEE_CAP_LESS_THEN_BLOCKS);

    // We need some information about the sender so lookup the account in the state.
    // TODO: During transaction execution this account will be also needed, so we may pass it along.
    const auto sender_acc = state_view.get_account(tx.sender).value_or(
        StateView::Account{.code_hash = Account::EMPTY_CODE_HASH});

    if (sender_acc.code_hash != Account::EMPTY_CODE_HASH)
        return make_error_code(SENDER_NOT_EOA);  // Origin must not be a contract (EIP-3607).

    if (sender_acc.nonce == Account::NonceMax)  // Nonce value limit (EIP-2681).
        return make_error_code(NONCE_HAS_MAX_VALUE);

    if (sender_acc.nonce < tx.nonce)
        return make_error_code(NONCE_TOO_HIGH);

    if (sender_acc.nonce > tx.nonce)
        return make_error_code(NONCE_TOO_LOW);

    // initcode size is limited by EIP-3860.
    if (rev >= EVMC_SHANGHAI && !tx.to.has_value() && tx.data.size() > MAX_INITCODE_SIZE)
        return make_error_code(INIT_CODE_SIZE_LIMIT_EXCEEDED);

    // Compute and check if sender has enough balance for the theoretical maximum transaction cost.
    // Note this is different from tx_max_cost computed with effective gas price later.
    // The computation cannot overflow if done with 512-bit precision.
    auto max_total_fee = umul(uint256{tx.gas_limit}, tx.max_gas_price);
    max_total_fee += tx.value;

    if (tx.type == Transaction::Type::blob)
    {
        const auto total_blob_gas = tx.blob_gas_used();
        // FIXME: Can overflow uint256.
        max_total_fee += total_blob_gas * tx.max_blob_gas_price;
    }
    if (sender_acc.balance < max_total_fee)
        return make_error_code(INSUFFICIENT_FUNDS);

    const auto intrinsic_cost = compute_tx_intrinsic_cost(rev, tx);
    if (tx.gas_limit < intrinsic_cost)
        return make_error_code(INTRINSIC_GAS_TOO_LOW);

    const auto execution_gas_limit = tx.gas_limit - intrinsic_cost;
    return TransactionProperties{execution_gas_limit};
}

StateDiff finalize(const StateView& state_view, evmc_revision rev, const address& coinbase,
    std::optional<uint64_t> block_reward, std::span<const Ommer> ommers,
    std::span<const Withdrawal> withdrawals)
{
    State state{state_view};
    // TODO: The block reward can be represented as a withdrawal.
    if (block_reward.has_value())
    {
        const auto reward = *block_reward;
        assert(reward % 32 == 0);  // Assume block reward is divisible by 32.
        const auto reward_by_32 = reward / 32;
        const auto reward_by_8 = reward / 8;

        state.touch(coinbase).balance += reward + reward_by_32 * ommers.size();
        for (const auto& ommer : ommers)
        {
            assert(ommer.delta > 0 && ommer.delta < 8);
            state.touch(ommer.beneficiary).balance += reward_by_8 * (8 - ommer.delta);
        }
    }

    for (const auto& withdrawal : withdrawals)
        state.touch(withdrawal.recipient).balance += withdrawal.get_amount();

    return state.build_diff(rev);
}

TransactionReceipt transition(const StateView& state_view, const BlockInfo& block,
    const BlockHashes& block_hashes, const Transaction& tx, evmc_revision rev, evmc::VM& vm,
    const TransactionProperties& tx_props)
{
    State state{state_view};

    auto& sender_acc = state.get_or_insert(tx.sender);
    assert(sender_acc.nonce < Account::NonceMax);  // Required for valid tx.
    ++sender_acc.nonce;                            // Bump sender nonce.

    const auto base_fee = (rev >= EVMC_LONDON) ? block.base_fee : 0;
    assert(tx.max_gas_price >= base_fee);                   // Required for valid tx.
    assert(tx.max_gas_price >= tx.max_priority_gas_price);  // Required for valid tx.
    const auto priority_gas_price =
        std::min(tx.max_priority_gas_price, tx.max_gas_price - base_fee);
    const auto effective_gas_price = base_fee + priority_gas_price;

    assert(effective_gas_price <= tx.max_gas_price);  // Required for valid tx.
    const auto tx_max_cost = tx.gas_limit * effective_gas_price;

    sender_acc.balance -= tx_max_cost;  // Modify sender balance after all checks.

    if (tx.type == Transaction::Type::blob)
    {
        // This uint64 * uint256 cannot overflow, because tx.blob_gas_used has limits enforced
        // before this stage.
        assert(block.blob_base_fee.has_value());
        const auto blob_fee = intx::umul(intx::uint256(tx.blob_gas_used()), *block.blob_base_fee);
        assert(blob_fee <= std::numeric_limits<intx::uint256>::max());
        assert(sender_acc.balance >= blob_fee);  // Required for valid tx.
        sender_acc.balance -= intx::uint256(blob_fee);
    }

    Host host{rev, vm, state, block, block_hashes, tx};

    sender_acc.access_status = EVMC_ACCESS_WARM;  // Tx sender is always warm.
    if (tx.to.has_value())
        host.access_account(*tx.to);
    for (const auto& [a, storage_keys] : tx.access_list)
    {
        host.access_account(a);
        for (const auto& key : storage_keys)
            state.get_storage(a, key).access_status = EVMC_ACCESS_WARM;
    }
    // EIP-3651: Warm COINBASE.
    // This may create an empty coinbase account. The account cannot be created unconditionally
    // because this breaks old revisions.
    if (rev >= EVMC_SHANGHAI)
        host.access_account(block.coinbase);

    const auto result = host.call(build_message(tx, tx_props.execution_gas_limit, rev));

    auto gas_used = tx.gas_limit - result.gas_left;

    const auto max_refund_quotient = rev >= EVMC_LONDON ? 5 : 2;
    const auto refund_limit = gas_used / max_refund_quotient;
    const auto refund = std::min(result.gas_refund, refund_limit);
    gas_used -= refund;
    assert(gas_used > 0);

    sender_acc.balance += tx_max_cost - gas_used * effective_gas_price;
    state.touch(block.coinbase).balance += gas_used * priority_gas_price;

    // Cumulative gas used is unknown in this scope.
    TransactionReceipt receipt{
        tx.type, result.status_code, gas_used, {}, host.take_logs(), {}, state.build_diff(rev)};

    // Cannot put it into constructor call because logs are std::moved from host instance.
    receipt.logs_bloom_filter = compute_bloom_filter(receipt.logs);

    return receipt;
}
}  // namespace evmone::state
