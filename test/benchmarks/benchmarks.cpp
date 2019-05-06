// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// Licensed under the Apache License, Version 2.0.

#include <evmone/evmone.h>

#include <benchmark/benchmark.h>
#include <test/utils/utils.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace benchmark;

namespace
{
// FIXME: Allow running with empty code.
// clang-format off
const auto empty_code = from_hex("00");
const auto sha1_divs_code = from_hex("608060405234801561001057600080fd5b5060043610610047577c010000000000000000000000000000000000000000000000000000000060003504631605782b811461004c575b600080fd5b6100f26004803603602081101561006257600080fd5b81019060208101813564010000000081111561007d57600080fd5b82018360208201111561008f57600080fd5b803590602001918460018302840111640100000000831117156100b157600080fd5b91908080601f016020809104026020016040519081016040528093929190818152602001838380828437600092019190915250929550610127945050505050565b604080517fffffffffffffffffffffffffffffffffffffffff0000000000000000000000009092168252519081900360200190f35b60006040518251602084019350604067ffffffffffffffc0600183011601600982820310600181146101585761015f565b6040820191505b50776745230100efcdab890098badcfe001032547600c3d2e1f06101d0565b6000838310156101c9575080820151928290039260208410156101c9577fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff60208590036101000a0119165b9392505050565b60005b82811015610686576101e684828961017e565b85526101f684602083018961017e565b60208601526040818503106001811461020e57610217565b60808286038701535b506040830381146001811461022b57610239565b602086018051600887021790525b5060405b6080811015610339578581017fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc08101517fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc88201517fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe08301517ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff48401516002911891909218189081027ffffffffefffffffefffffffefffffffefffffffefffffffefffffffefffffffe1663800000009091047c010000000100000001000000010000000100000001000000010000000116179052600c0161023d565b5060805b61014081101561043a578581017fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff808101517fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff908201517fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc08301517fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe88401516004911891909218189081027ffffffffcfffffffcfffffffcfffffffcfffffffcfffffffcfffffffcfffffffc1663400000009091047c03000000030000000300000003000000030000000300000003000000031617905260180161033d565b508160008060005b605081101561065c5760148104801561047257600181146104ae57600281146104e857600381146105275761055d565b6501000000000085046a0100000000000000000000860481186f01000000000000000000000000000000870416189350635a827999925061055d565b6501000000000085046f0100000000000000000000000000000086046a0100000000000000000000870418189350636ed9eba1925061055d565b6a010000000000000000000085046f010000000000000000000000000000008604818117650100000000008804169116179350638f1bbcdc925061055d565b6501000000000085046f0100000000000000000000000000000086046a010000000000000000000087041818935063ca62c1d692505b50601f770800000000000000000000000000000000000000000000008504168063ffffffe073080000000000000000000000000000000000000087041617905080840190508063ffffffff86160190508083019050807c0100000000000000000000000000000000000000000000000000000000600484028c0151040190507401000000000000000000000000000000000000000081026501000000000086041794506a0100000000000000000000633fffffff6a040000000000000000000087041663c00000006604000000000000880416170277ffffffff00ffffffff000000000000ffffffff00ffffffff861617945050600181019050610442565b5050509190910177ffffffff00ffffffff00ffffffff00ffffffff00ffffffff16906040016101d3565b506c0100000000000000000000000063ffffffff821667ffffffff000000006101008404166bffffffff0000000000000000620100008504166fffffffff000000000000000000000000630100000086041673ffffffff00000000000000000000000000000000640100000000870416171717170294505050505091905056fea165627a7a7230582083396642a98f6018c81ca24dc0c2af8e842bd33a6b8d7f08632dc1bc372e466a0029");
const auto sha1_shifts_code = from_hex("608060405234801561001057600080fd5b506004361061002b5760003560e01c80631605782b14610030575b600080fd5b6100d66004803603602081101561004657600080fd5b81019060208101813564010000000081111561006157600080fd5b82018360208201111561007357600080fd5b8035906020019184600183028401116401000000008311171561009557600080fd5b91908080601f0160208091040260200160405190810160405280939291908181526020018383808284376000920191909152509295506100f8945050505050565b604080516bffffffffffffffffffffffff199092168252519081900360200190f35b60006040518251602084019350604067ffffffffffffffc06001830116016009828203106001811461012957610130565b6040820191505b50776745230100efcdab890098badcfe001032547600c3d2e1f0610183565b60008383101561017c5750808201519282900392602084101561017c5760001960208590036101000a0119165b9392505050565b60005b8281101561045c5761019984828961014f565b85526101a984602083018961014f565b6020860152604081850310600181146101c1576101ca565b60808286038701535b50604083038114600181146101de576101ee565b8460031b60208701511760208701525b5060405b608081101561027157858101603f19810151603719820151601f19830151600b1984015118911818600181901b7ffffffffefffffffefffffffefffffffefffffffefffffffefffffffefffffffe16601f9190911c7c010000000100000001000000010000000100000001000000010000000116179052600c016101f2565b5060805b6101408110156102f557858101607f19810151606f19820151603f1983015160171984015118911818600281901b7ffffffffcfffffffcfffffffcfffffffcfffffffcfffffffcfffffffcfffffffc16601e9190911c7c030000000300000003000000030000000300000003000000030000000316179052601801610275565b508160008060005b60508110156104325760148104801561032d576001811461034e576002811461036d5760038114610391576103ac565b602885901c605086901c8118607887901c16189350635a82799992506103ac565b8460501c8560781c189350838560281c189350636ed9eba192506103ac565b605085901c607886901c818117602888901c169116179350638f1bbcdc92506103ac565b8460501c8560781c189350838560281c18935063ca62c1d692505b50601f8460bb1c168063ffffffe086609b1c1617905080840190508063ffffffff86160190508083019050808260021b8b015160e01c0190508060a01b8560281c179450633fffffff8560521c1663c00000008660321c161760501b77ffffffff00ffffffff000000000000ffffffff00ffffffff8616179450506001810190506102fd565b5050509190910177ffffffff00ffffffff00ffffffff00ffffffff00ffffffff1690604001610186565b5063ffffffff811667ffffffff000000008260081c166bffffffff00000000000000008360101c166fffffffff0000000000000000000000008460181c1673ffffffff000000000000000000000000000000008560201c161717171760601b94505050505091905056fea165627a7a72305820227af8b272b9b0e3d345f580ebcde55f50e3e8b7ecafabffcadb92e55e4de68e0029");
const auto blake2b_shifts_code = from_hex("608060405234801561001057600080fd5b50600436106100365760003560e01c80631e0924231461003b578063d299dac0146102bb575b600080fd5b610282600480360360a081101561005157600080fd5b81019060208101813564010000000081111561006c57600080fd5b82018360208201111561007e57600080fd5b803590602001918460018302840111640100000000831117156100a057600080fd5b91908080601f01602080910402602001604051908101604052809392919081815260200183838082843760009201919091525092959493602081019350359150506401000000008111156100f357600080fd5b82018360208201111561010557600080fd5b8035906020019184600183028401116401000000008311171561012757600080fd5b91908080601f016020809104026020016040519081016040528093929190818152602001838380828437600092019190915250929594936020810193503591505064010000000081111561017a57600080fd5b82018360208201111561018c57600080fd5b803590602001918460018302840111640100000000831117156101ae57600080fd5b91908080601f016020809104026020016040519081016040528093929190818152602001838380828437600092019190915250929594936020810193503591505064010000000081111561020157600080fd5b82018360208201111561021357600080fd5b8035906020019184600183028401116401000000008311171561023557600080fd5b91908080601f0160208091040260200160405190810160405280939291908181526020018383808284376000920191909152509295505050903567ffffffffffffffff1691506103f49050565b604051808261010080838360005b838110156102a8578181015183820152602001610290565b5050505090500191505060405180910390f35b610282600480360360608110156102d157600080fd5b8101906020810181356401000000008111156102ec57600080fd5b8201836020820111156102fe57600080fd5b8035906020019184600183028401116401000000008311171561032057600080fd5b91908080601f016020809104026020016040519081016040528093929190818152602001838380828437600092019190915250929594936020810193503591505064010000000081111561037357600080fd5b82018360208201111561038557600080fd5b803590602001918460018302840111640100000000831117156103a757600080fd5b91908080601f0160208091040260200160405190810160405280939291908181526020018383808284376000920191909152509295505050903567ffffffffffffffff1691506104489050565b6103fc61156d565b61040461158d565b61040c61156d565b61042982858961041b8a610485565b6104248a610485565b610538565b61043382896106dc565b61043d828261079b565b979650505050505050565b61045061156d565b61047d848460206040519081016040528060008152506020604051908101604052806000815250866103f4565b949350505050565b61048d6115ca565b60005b82518110156104fb5760088184018101519067ffffffffffffffff82166007841660010182026040031b9084908404600281106104c957fe5b6020020151188360088404600281106104de57fe5b67ffffffffffffffff909216602092909202015250600101610490565b5061050d8160005b60200201516108cd565b67ffffffffffffffff168152610524816001610503565b67ffffffffffffffff166020820152919050565b67ffffffffffffffff84161580610559575060408467ffffffffffffffff16115b80610565575060408351115b1561056f57600080fd5b61057761156d565b506040805161010081018252676a09e667f3bcc908815267bb67ae8584caa73b6020820152673c6ef372fe94f82b9181019190915267a54ff53a5f1d36f1606082015267510e527fade682d16080820152679b05688c2b3e6c1f60a0820152671f83d9abfb41bd6b60c0820152675be0cd19137e217960e082015260005b600881101561063d5781816008811061060a57fe5b602002015187602001518260088110151561062157fe5b67ffffffffffffffff90921660209290920201526001016105f5565b50835160208781018051805167ffffffffffffffff94851660081b188918630101000018841690528551815160809081018051909218851690915286830151825160a0018051909118851690528551825160c00180519091188516905291850151905160e00180519091188316905286821690880152845190600090821611156106d3576106cb87866106dc565b608060608801525b50505050505050565b60005b815181101561079657826060015167ffffffffffffffff166080141561074057606083015160408401805167ffffffffffffffff9092169091016fffffffffffffffffffffffffffffffff16905261073883600061094f565b600060608401525b60608301805167ffffffffffffffff600182018116909252166107616115e5565b508351835160009085908590811061077557fe5b90602001015160f81c60f81b60f81c905080838301535050506001016106df565b505050565b60608201805160408401805167ffffffffffffffff8084169182016fffffffffffffffffffffffffffffffff1690925260019092011690915260006107de6115e5565b508351825b60808110156107f95782818301536001016107e3565b5061080585600161094f565b60005b60808601516008900481101561085457602086015161082c90826008811061050357fe5b85826008811061083857fe5b67ffffffffffffffff9092166020929092020152600101610808565b506040856080015110156108c65760808501516020860151600860078316810260400392610889929190046008811061050357fe5b67ffffffffffffffff16901c84600887608001518115156108a657fe5b04600881106108b157fe5b67ffffffffffffffff90921660209290920201525b5050505050565b600067010000000000000060ff8316026501000000000061ff00841602630100000062ff000085160261010063ff000000861681029064ff00000000871604630100000065ff00000000008816046501000000000066ff00000000000089160467010000000000000067ff000000000000008a16041818181818181892915050565b610957611604565b61095f611604565b61096761156d565b506040805161010081018252676a09e667f3bcc908815267bb67ae8584caa73b6020820152673c6ef372fe94f82b9181019190915267a54ff53a5f1d36f1606082015267510e527fade682d16080820152679b05688c2b3e6c1f60a0820152671f83d9abfb41bd6b60c0820152675be0cd19137e217960e082015260005b6008811015610a5f57602086015181600881106109fe57fe5b6020020151848260108110610a0f57fe5b67ffffffffffffffff9092166020929092020152818160088110610a2f57fe5b6020020151846008830160108110610a4357fe5b67ffffffffffffffff90921660209290920201526001016109e5565b506040850180516101808501805167ffffffffffffffff928316188216905290516101a085018051680100000000000000006fffffffffffffffffffffffffffffffff9093169290920490911890911690528315610acc576101c0830180511967ffffffffffffffff1690525b600080805b60108160ff161015610b56576000925060038116801515610b0c578851600460ff84160460ff16600481101515610b0457fe5b602002015192505b67ffffffffffffffff83826003036040021c169350610b2a846108cd565b8660ff841660108110610b3957fe5b67ffffffffffffffff909216602092909202015250600101610ad1565b50610b7985600060046008600c89845b60200201518a60015b60200201516113e8565b610b9685600160056009600d8960025b60200201518a6003610b6f565b610bb38560026006600a600e8960045b60200201518a6005610b6f565b610bd08560036007600b600f8960065b60200201518a6007610b6f565b610bed8560006005600a600f8960085b60200201518a6009610b6f565b610c0a8560016006600b600c89600a5b60200201518a600b610b6f565b610c2785600260076008600d89600c5b60200201518a600d610b6f565b610c4385600360046009600e89815b60200201518a600f610b6f565b610c6085600060046008600c89600e5b60200201518a600a610b6f565b610c7d85600160056009600d8960045b60200201518a6008610b6f565b610c918560026006600a600e896009610c36565b610cae8560036007600b600f89600d5b60200201518a6006610b6f565b610ccb8560006005600a600f8960015b60200201518a600c610b6f565b610ce88560016006600b600c8960005b60200201518a6002610b6f565b610cfc85600260076008600d89600b610bc3565b610d1085600360046009600e896005610b89565b610d2485600060046008600c89600b610c70565b610d4185600160056009600d89600c5b60200201518a6000610b6f565b610d558560026006600a600e896005610cdb565b610d688560036007600b600f8981610c1a565b610d848560006005600a600f89825b60200201518a600e610b6f565b610d988560016006600b600c896003610ca1565b610dab85600260076008600d8983610b66565b610dc785600360046009600e89825b60200201518a6004610b6f565b610ddb85600060046008600c896007610be0565b610def85600160056009600d896003610b66565b610e038560026006600a600e89600d610cbe565b610e168560036007600b600f8982610d77565b610e2a8560006005600a600f896002610ca1565b610e3e8560016006600b600c896005610c53565b610e5285600260076008600d896004610d34565b610e6685600360046009600e89600f610c70565b610e7a85600060046008600c896009610d34565b610e8d85600160056009600d8983610bc3565b610ea08560026006600a600e8984610dba565b610eb48560036007600b600f89600a610c36565b610ec88560006005600a600f89600e610b66565b610edb8560016006600b600c8982610cbe565b610eef85600260076008600d896006610c70565b610f0285600360046009600e8984610c1a565b610f1685600060046008600c896002610cbe565b610f2a85600160056009600d896006610c53565b610f3e8560026006600a600e896000610bfd565b610f528560036007600b600f896008610b89565b610f668560006005600a600f896004610c1a565b610f7a8560016006600b600c896007610ba6565b610f8e85600260076008600d89600f610d77565b610fa285600360046009600e896001610be0565b610fb585600060046008600c8981610ba6565b610fc885600160056009600d8984610c36565b610fdb8560026006600a600e8981610c1a565b610fef8560036007600b600f896004610c53565b6110028560006005600a600f8984610bc3565b6110158560016006600b600c8983610b89565b61102985600260076008600d896009610cdb565b61103d85600360046009600e896008610bfd565b61105185600060046008600c89600d610bfd565b61106585600160056009600d896007610d77565b6110798560026006600a600e89600c610b66565b61108c8560036007600b600f8984610be0565b61109f8560006005600a600f8983610d34565b6110b38560016006600b600c89600f610dba565b6110c685600260076008600d8982610ca1565b6110da85600360046009600e896002610c53565b6110ee85600060046008600c896006610c36565b61110285600160056009600d89600e610be0565b6111168560026006600a600e89600b610b89565b61112a8560036007600b600f896000610c70565b61113e8560006005600a600f89600c610cdb565b6111528560016006600b600c89600d610bc3565b61116685600260076008600d896001610dba565b61117a85600360046009600e89600a610ba6565b61118e85600060046008600c89600a610cdb565b6111a285600160056009600d896008610dba565b6111b68560026006600a600e896007610ca1565b6111ca8560036007600b600f896001610ba6565b6111dd8560006005600a600f8981610bfd565b6111f18560016006600b600c896009610d77565b61120585600260076008600d896003610cbe565b61121985600360046009600e89600d610d34565b61122c85600060046008600c8984610b66565b61124085600160056009600d896002610b89565b6112548560026006600a600e896004610ba6565b6112688560036007600b600f896006610bc3565b61127c8560006005600a600f896008610be0565b6112908560016006600b600c89600a610bfd565b6112a485600260076008600d89600c610c1a565b6112b785600360046009600e8981610c36565b6112cb85600060046008600c89600e610c53565b6112df85600160056009600d896004610c70565b6112f38560026006600a600e896009610c36565b6113078560036007600b600f89600d610ca1565b61131b8560006005600a600f896001610cbe565b61132f8560016006600b600c896000610cdb565b61134385600260076008600d89600b610bc3565b61135785600360046009600e896005610b89565b60005b60088160ff1610156113de578560ff60088301166010811061137857fe5b60200201518660ff83166010811061138c57fe5b602002015189602001518360ff166008811015156113a657fe5b6020020151181888602001518260ff166008811015156113c257fe5b67ffffffffffffffff909216602092909202015260010161135a565b5050505050505050565b60008787601081106113f657fe5b60200201519050600088876010811061140b57fe5b60200201519050600089876010811061142057fe5b6020020151905060008a876010811061143557fe5b6020020151905068010000000000000000868486010893508318602081811c91901b67ffffffffffffffff161868010000000000000000818308915067ffffffffffffffff82841860281b1682841860181c18925068010000000000000000858486010893508318601081901c60309190911b67ffffffffffffffff161868010000000000000000818308928318603f81901c60019190911b67ffffffffffffffff1618929150838b8b601081106114e957fe5b67ffffffffffffffff9092166020929092020152828b8a6010811061150a57fe5b67ffffffffffffffff9092166020929092020152818b896010811061152b57fe5b67ffffffffffffffff9092166020929092020152808b886010811061154c57fe5b67ffffffffffffffff90921660209290920201525050505050505050505050565b610100604051908101604052806008906020820280388339509192915050565b6101e0604051908101604052806115a26115e5565b81526020016115af61156d565b81526000602082018190526040820181905260609091015290565b60408051808201825290600290829080388339509192915050565b6080604051908101604052806004906020820280388339509192915050565b61020060405190810160405280601090602082028038833950919291505056fea165627a7a72305820a59dc9d098d29bacdd88cb50c25c96ed4ba3047fd46a5c6ecf57e447a3c699100029");
// clang-format on

const auto vm = evmc_create_evmone();

int64_t execute(bytes_view code, bytes_view input) noexcept
{
    constexpr auto gas = std::numeric_limits<int64_t>::max();
    auto msg = evmc_message{};
    msg.gas = gas;
    msg.input_data = input.data();
    msg.input_size = input.size();
    auto r = vm->execute(vm, nullptr, EVMC_CONSTANTINOPLE, &msg, code.data(), code.size());

    // FIXME: Fix evmc_release_result() helper.
    if (r.release)
        r.release(&r);

    return gas - r.gas_left;
}

void empty(State& state) noexcept
{
    for (auto _ : state)
        execute(empty_code, {});
}
BENCHMARK(empty);

void sha1_divs(State& state) noexcept
{
    const auto input_size = static_cast<size_t>(state.range(0));

    auto abi_input =
        from_hex("1605782b0000000000000000000000000000000000000000000000000000000000000020");

    auto oss = std::ostringstream{};
    oss << std::hex << std::setfill('0') << std::setw(64) << input_size;
    abi_input += from_hex(oss.str());

    abi_input.resize(abi_input.size() + input_size, 0);

    auto total_gas_used = int64_t{0};
    auto iteration_gas_used = int64_t{0};
    for (auto _ : state)
        total_gas_used += iteration_gas_used = execute(sha1_divs_code, abi_input);

    state.counters["gas_used"] = Counter(iteration_gas_used);
    state.counters["gas_rate"] = Counter(total_gas_used, Counter::kIsRate);
}
BENCHMARK(sha1_divs)->Arg(0)->Arg(1351)->Arg(2737)->Arg(5311)->Arg(64 * 1024)->Unit(kMicrosecond);


void sha1_shifts(State& state) noexcept
{
    const auto input_size = static_cast<size_t>(state.range(0));

    auto abi_input =
        from_hex("1605782b0000000000000000000000000000000000000000000000000000000000000020");

    auto oss = std::ostringstream{};
    oss << std::hex << std::setfill('0') << std::setw(64) << input_size;
    abi_input += from_hex(oss.str());

    abi_input.resize(abi_input.size() + input_size, 0);

    auto total_gas_used = int64_t{0};
    auto iteration_gas_used = int64_t{0};
    for (auto _ : state)
        total_gas_used += iteration_gas_used = execute(sha1_shifts_code, abi_input);

    state.counters["gas_used"] = Counter(iteration_gas_used);
    state.counters["gas_rate"] = Counter(total_gas_used, Counter::kIsRate);
}
BENCHMARK(sha1_shifts)->Arg(0)->Arg(1351)->Arg(2737)->Arg(5311)->Arg(64 * 1024)->Unit(kMicrosecond);


void blake2b_shifts(State& state) noexcept
{
    const auto input_size = static_cast<size_t>(state.range(0));

    auto abi_input = from_hex(
        "d299dac0"
        "0000000000000000000000000000000000000000000000000000000000000060"
        "0000000000000000000000000000000000000000000000000000000000000080"
        "0000000000000000000000000000000000000000000000000000000000000040");

    auto oss = std::ostringstream{};
    oss << std::hex << std::setfill('0') << std::setw(64) << input_size;
    abi_input += from_hex(oss.str());

    abi_input.resize(abi_input.size() + input_size, 0);

    auto total_gas_used = int64_t{0};
    auto iteration_gas_used = int64_t{0};
    for (auto _ : state)
        total_gas_used += iteration_gas_used = execute(blake2b_shifts_code, abi_input);

    state.counters["gas_used"] = Counter(iteration_gas_used);
    state.counters["gas_rate"] = Counter(total_gas_used, Counter::kIsRate);
}
BENCHMARK(blake2b_shifts)
    ->Arg(0)
    ->Arg(2805)
    ->Arg(5610)
    ->Arg(8415)
    ->Arg(64 * 1024)
    ->Unit(kMicrosecond);


}  // namespace

BENCHMARK_MAIN();
