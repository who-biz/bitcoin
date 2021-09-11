#include "crypto/ethash/ethashWraper/EthashAux.h"
#include "crypto/ethash/ethashlib/internal.h"
#include "crypto/ethash/ethashExtension/SHA3.h"
//#include "crypto/ethash/ethashExtension/Common.h"

#define DEV_IF_THROWS(X) try{X;}catch(...)

EthashAux* EthashAux::s_this = nullptr;

EthashAux::~EthashAux()
{
}

EthashAux* EthashAux::get()
{
	static boost::once_flag flag;
	boost::call_once(flag, []{s_this = new EthashAux();});
	return s_this;
}

uint256 EthashAux::seedHash(unsigned _number)
{
	unsigned epoch = _number / ETHASH_EPOCH_LENGTH;
	Guard l(get()->x_epochs);
	if (epoch >= get()->m_seedHashes.size())
	{
		uint256 ret;
		unsigned n = 0;
		if (!get()->m_seedHashes.empty())
		{
			ret = get()->m_seedHashes.back();
			n = get()->m_seedHashes.size() - 1;
		}
		get()->m_seedHashes.resize(epoch + 1);
		//printf("Searching for seedHash of epoch %u\n", epoch);
		for (; n <= epoch; ++n, ret = sha3(ret))
		{
			get()->m_seedHashes[n] = ret;
			//printf("Epoch: %u is %s\n", n, ret.GetHex().c_str());
		}
		//printf("Testcase sha3 is %s\n", ret.GetHex().c_str());
	}
	return get()->m_seedHashes[epoch];
}

uint64_t EthashAux::number(uint256 const& _seedHash)
{
	Guard l(get()->x_epochs);

	unsigned epoch = 0;
	auto epochIter = get()->m_epochs.find(_seedHash);
	if (epochIter == get()->m_epochs.end())
	{
		//		cdebug << "Searching for seedHash " << _seedHash;
		// different sha3 algo, need to add SHA3, RPL... vectorref.h
		for (uint256 h; h != _seedHash && epoch < 2048; ++epoch, h = sha3(h), get()->m_epochs[h] = epoch) {
		}
		if (epoch == 2048)
		{
			std::ostringstream error;
			//error << "apparent block number for " << _seedHash.GetHex() << " is too high; max is " << (ETHASH_EPOCH_LENGTH * 2048);
			throw std::invalid_argument(error.str());
		}
	}
	else
		epoch = epochIter->second;
	return epoch * ETHASH_EPOCH_LENGTH;
}

EthashAux::LightType EthashAux::light(uint256 const& _seedHash)
{
	UpgradableGuard l(get()->x_lights);

	if (get()->m_lights.count(_seedHash))
		return get()->m_lights.at(_seedHash);
	UpgradeGuard l2(l);

	return (get()->m_lights[_seedHash] = std::make_shared<LightAllocation>(_seedHash));
}

EthashAux::LightAllocation::LightAllocation(uint256 const& _seedHash)
{
	
	uint64_t blockNumber = EthashAux::number(_seedHash);
	light = ethash_light_new(blockNumber);
	if (!light)
		BOOST_THROW_EXCEPTION(std::runtime_error("ethash_light_new()"));
	size = ethash_get_cachesize(blockNumber);
}

EthashAux::LightAllocation::~LightAllocation()
{
	ethash_light_delete(light);
}

EthashAux::FullAllocation::FullAllocation(ethash_light_t _light, ethash_callback_t _cb)
{
//	cdebug << "About to call ethash_full_new...";
	full = ethash_full_new(_light, _cb);
//	cdebug << "Called OK.";
	if (!full)
	{
		throw std::runtime_error("ethash_full_new");
	}
}

EthashAux::FullAllocation::~FullAllocation()
{
	ethash_full_delete(full);
} 

static std::function<int(unsigned)> s_dagCallback;

static int dagCallbackShim(unsigned _p)
{
	//clog(DAGChannel) << "Generating DAG file. Progress: " << toString(_p) << "%";
	//LogPrintf("Generating DAG file. Progress: %u\n", _p);
	return s_dagCallback ? s_dagCallback(_p) : 0;
}

EthashAux::FullType EthashAux::full(uint256 const& _seedHash, bool _createIfMissing, std::function<int(unsigned)> const& _f)
{
	FullType ret;
	auto l = light(_seedHash);

	DEV_GUARDED(get()->x_fulls)
		if ((ret = get()->m_fulls[_seedHash].lock()))
		{
			get()->m_lastUsedFull = ret;
			return ret;
		}
	
	if (_createIfMissing || computeFull(_seedHash, false) == 100)
	{
		s_dagCallback = _f;
//		cnote << "Loading from libethash...";
		ret = std::make_shared<FullAllocation>(l->light, dagCallbackShim);
//		cnote << "Done loading.";

		DEV_GUARDED(get()->x_fulls)
			get()->m_fulls[_seedHash] = get()->m_lastUsedFull = ret;
	}

	return ret;
}

unsigned EthashAux::computeFull(uint256 const& _seedHash, bool _createIfMissing)
{
	Guard l(get()->x_fulls);
	//boost::lock_guard<boost::mutex> lock(get()->x_fulls);
	
	uint64_t blockNumber;

	try {
		blockNumber = EthashAux::number(_seedHash);
	}
	catch(const std::exception& e) {
		return 0;
	}
    
	if (FullType ret = get()->m_fulls[_seedHash].lock())
	{
		get()->m_lastUsedFull = ret;
		return 100;
	}

	if(!get()->m_fullGenerator){
		//LogPrintf("Thread is null\n");
	}
	else if(get()->m_generatingFullNumber == NotGenerating && get()->m_fullGenerator->joinable()) {
		get()->m_fullGenerator->detach();
	}

	if (_createIfMissing && (!get()->m_fullGenerator || !get()->m_fullGenerator->joinable()))
	{
		get()->m_fullProgress = 0;
		get()->m_generatingFullNumber = blockNumber / ETHASH_EPOCH_LENGTH * ETHASH_EPOCH_LENGTH;
		get()->m_fullGenerator = std::unique_ptr<boost::thread>(new boost::thread([=](){
			get()->full(_seedHash, true, [](unsigned p){ get()->m_fullProgress = p; return 0; });
			//LogPrintf("Full DAG loaded");
			get()->m_fullProgress = 0;
			get()->m_generatingFullNumber = NotGenerating;
		}));
	}

	return (get()->m_generatingFullNumber == blockNumber) ? get()->m_fullProgress : 0;
}

void EthashAux::reverseUint256(uint256 const& hash, uint8_t* _array)
{
	memset(_array, 0, 32);
	for(int i=32-1; i>=0; i--)
		_array[32-i-1] = hash.begin()[i];
}

EthashProofOfWork::Result EthashAux::FullAllocation::compute(uint256 const& _headerHash, uint64_t const& _nonce) const
{
	//Parse _headerHash
	ethash_h256_t reverse_hash;
	reverseUint256(_headerHash, reverse_hash.b);
	ethash_return_value_t r = ethash_full_compute(full, reverse_hash, _nonce);
	/*if (!r.success)
		BOOST_THROW_EXCEPTION(DAGCreationFailure(""));*/
	return EthashProofOfWork::Result{uint256(r.result.getChars(true)), uint256(r.mix_hash.getChars(true)), "" };
}

EthashProofOfWork::Result EthashAux::LightAllocation::compute(uint256 const& _headerHash, uint64_t const& _nonce) const
{
	//Parse _headerHash
	ethash_h256_t reverse_hash;
	reverseUint256(_headerHash, reverse_hash.b);
	ethash_return_value r = ethash_light_compute(light, reverse_hash, _nonce);
	if (!r.success)
		BOOST_THROW_EXCEPTION(std::runtime_error("Ethash wasn't success!"));
	return EthashProofOfWork::Result{uint256(r.result.getChars(true)), uint256(r.mix_hash.getChars(true)), "" };
}

EthashProofOfWork::Result EthashAux::eval(uint256 const& _seedHash, uint256 const& _headerHash, uint64_t const& _nonce)
{
	DEV_GUARDED(get()->x_fulls)
		if (FullType dag = get()->m_fulls[_seedHash].lock())
			return dag->compute(_headerHash, _nonce);
	try
		{
			return EthashAux::get()->light(_seedHash)->compute(_headerHash, _nonce);
		}
		catch(std::exception e)
		{
			return EthashProofOfWork::Result{ uint256(), uint256(), e.what() };
		}
}

ethash_return_value EthashAux::performEthash(int blockNumber, uint256 prevhash, uint64_t nonce)
{
	EthashAux::FullType dag;
    uint256 seed = seedHash(blockNumber); //block number  - replace

	while (/*!shouldStop() && */!dag)
        {
            while (/*!shouldStop() &&*/ computeFull(seed, true) != 100) {
                #if defined(HAVE_WORKING_BOOST_SLEEP_FOR)
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
                #elif defined(HAVE_WORKING_BOOST_SLEEP)
                    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
                #else
                    //should never get here
                    #error missing boost sleep implementation
                #endif
            }
            dag = full(seed, false);
        }
	
	ethash_h256_t reverse_hash;
	reverseUint256(prevhash, reverse_hash.b);
	return ethash_full_compute(dag->full, reverse_hash, nonce);
}

uint256 EthashAux::getSeedHash(uint64_t block_number)
{
    uint256 ret;
    ethash_h256_t some = ethash_get_seedhash(block_number);
    ret = uint256(some.getChars(true));
    return ret;
}