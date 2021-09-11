#pragma once

#include "uint256.h"
#include <stdbool.h>
#include <iostream>
#include <memory>
#include "crypto/ethash/ethashlib/ethash.h"
#include "crypto/ethash/ethashExtension/Guards.h"
#include "crypto/ethash/ethashExtension/Exceptions.h"
#include "crypto/ethash/ethashWraper/EthashProofOfWork.h"
#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>

#include <boost/chrono/chrono.hpp>

using uint256s = std::vector<uint256>;

class EthashAux
{
public:
    ~EthashAux();

    static EthashAux* get();

    struct LightAllocation{
        LightAllocation(uint256 const& _seedHash);
		~LightAllocation();
        EthashProofOfWork::Result compute(uint256 const& _headerHash, uint64_t const& _nonce) const;
        ethash_light_t light;
        uint64_t size;
    };

    struct FullAllocation{
        FullAllocation(ethash_light_t light, ethash_callback_t _cb);
        ~FullAllocation();
        EthashProofOfWork::Result compute(uint256 const& _headerHash, uint64_t const& _nonce) const;
        ethash_full_t full;
    };

    using LightType = std::shared_ptr<LightAllocation>;
    using FullType = std::shared_ptr<FullAllocation>;

    static uint256 seedHash(unsigned _number);
    static uint64_t number(uint256 const& _seedHash);
    static LightType light(uint256 const& _seedHash);

    static const uint64_t NotGenerating = (uint64_t)-1;
    /// Kicks off generation of DAG for @a _seedHash and @returns false or @returns true if ready.
	static unsigned computeFull(uint256 const& _seedHash, bool _createIfMissing = true);
    /// Kicks off generation of DAG for @a _blocknumber and blocks until ready; @returns result or empty pointer if not existing and _createIfMissing is false.
	static FullType full(uint256 const& _seedHash, bool _createIfMissing = false, std::function<int(unsigned)> const& _f = std::function<int(unsigned)>());

    static EthashProofOfWork::Result eval(uint256 const& _seedHash, uint256 const& _headerHash, uint64_t const& _nonce);

    static uint256 getSeedHash(uint64_t block_number);

    static void reverseUint256(uint256 const& hash, uint8_t* _array);

    static ethash_return_value performEthash(int blockNumber, uint256 prevhash, uint64_t nonce);

private:
    EthashAux() {};

    static EthashAux* s_this;

    SharedMutex x_lights;
    typedef boost::unordered_map<uint256, std::shared_ptr<LightAllocation>> Lights; 
    Lights m_lights;

    Mutex x_fulls;
    typedef boost::unordered_map<uint256, std::weak_ptr<FullAllocation>> Fulls; 
    Fulls m_fulls;
    FullType m_lastUsedFull;
    std::unique_ptr<boost::thread> m_fullGenerator;
    uint64_t m_generatingFullNumber = NotGenerating;
    unsigned m_fullProgress;

    Mutex x_epochs;
    typedef boost::unordered_map<uint256, unsigned> Epochs; 
    Epochs m_epochs;
    uint256s m_seedHashes;
};

inline std::size_t hash_value(const uint256 &F)
{
	return boost::hash_range(F.begin(),F.end());
}