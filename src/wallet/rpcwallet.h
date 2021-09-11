// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_RPCWALLET_H
#define BITCOIN_WALLET_RPCWALLET_H

class UniValue;
class CRPCTable;

void RegisterWalletRPCCommands(CRPCTable &tableRPC);

void GetReceivedWalletAddresses(const CTransaction& tx, UniValue& result);

#endif //BITCOIN_WALLET_RPCWALLET_H
