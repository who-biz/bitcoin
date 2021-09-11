// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"

#include "crypto/ethash/ethashWraper/EthashAux.h"
#include <inttypes.h>

const int64_t zerohexa_limit = 268435456;

uint256 CBlockHeader::GetHash() const
{
    // RETURN TO ORIGINAL STATE
    /*printf("Prevoiushash: %s\n", hashPrevBlock.GetHex().c_str());
    printf("nNonce: %"PRIu64"\n", nNonce);
    printf("GetHash: %s\n", SerializeHash(*this).ToString().c_str());*/
    return SerializeHash(*this);
}

uint256 CBlockHeader::GetSeed() const
{
    return EthashAux::seedHash(nHeight);
}

uint256 CBlockHeader::GetPoWHash() const
{
    uint256 seed = EthashAux::seedHash(nHeight); 
    EthashProofOfWork::Result result = EthashAux::eval(seed, hashPrevBlock,  nNonce);
    //check mixhash
    if(result.mixHash.IsNull() && result.value.IsNull()) {
        //LogPrintf("Error in ethash algo: %s\n", result.msg);
    }
    if (result.mixHash.Compare(mixhash)){
        //LogPrintf("Error in comparing mixhash\n");
        return uint256();
    }
    return result.value;
}

uint256 CBlockHeader::MakePoWHash()
{
    uint256 thash;
    //perform ethash
    EnsurePrecomputed(nHeight);
    ethash_return_value r = EthashAux::performEthash(nHeight, hashPrevBlock, nNonce);
    thash = uint256(r.result.getChars(true));
    mixhash = uint256(r.mix_hash.getChars(true));
    nHeight = nHeight;
    return thash;
}

void CBlockHeader::EnsurePrecomputed(unsigned _number) const
{
	if (_number % ETHASH_EPOCH_LENGTH > ETHASH_EPOCH_LENGTH * 9 / 10)
    {
        // 90% of the way to the new epoch
		EthashAux::computeFull(EthashAux::seedHash(_number + ETHASH_EPOCH_LENGTH), true);
    }	
}

std::string CBlock::ToString() const
{
    //TODO include mixhash and height
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%ulld, mixhash=%s, nHeight=%u, vtx=%u)\n",
    GetHash().ToString(),
    nVersion,
    hashPrevBlock.ToString(),
    hashMerkleRoot.ToString(),
    nTime, nBits, nNonce, mixhash.ToString(), nHeight,
    vtx.size());
    
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        s << "  " << vtx[i].ToString() << "\n";
    }
    return s.str();
}

int64_t GetBlockWeight(const CBlock& block)
{
    // This implements the weight = (stripped_size * 4) + witness_size formula,
    // using only serialization with and without witness data. As witness_size
    // is equal to total_size - stripped_size, this formula is identical to:
    // weight = (stripped_size * 3) + total_size.
    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
}
