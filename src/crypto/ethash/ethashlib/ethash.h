#pragma once

#include <stdint.h>
#include <string.h>
#include <vector>
#include <algorithm>

#define ETHASH_REVISION 23
#define ETHASH_EPOCH_LENGTH 30000U //Dag
#define ETHASH_MIX_BYTES 128       //Dag
#define ETHASH_DATASET_PARENTS 256 //Dag
#define ETHASH_CACHE_ROUNDS 3      //Dag
#define ETHASH_ACCESSES 64         //Hashimoto
#define ETHASH_DAG_MAGIC_NUM_SIZE 8//Dag
#define ETHASH_DAG_MAGIC_NUM 0xFEE1DEADBADDCAFE //Dag

#ifdef __cplusplus
extern "C" {
#endif

/// Type of a seedhash/blockhash e.t.c.
typedef struct ethash_h256 { 

uint8_t b[32];
std::vector<unsigned char> getChars(bool reverse){
    std::vector<unsigned char> vec(b, b + 32);
	if(reverse)
		std::reverse(vec.begin(), vec.end());
    return vec;
};

char* getCharPointer(){
    return (char*)b;
};
    
} ethash_h256_t;

struct ethash_light;
typedef struct ethash_light* ethash_light_t;
struct ethash_full;
typedef struct ethash_full* ethash_full_t;
typedef int(*ethash_callback_t)(unsigned);

typedef struct ethash_return_value {
	ethash_h256_t result;
	ethash_h256_t mix_hash;
	bool success;
} ethash_return_value_t;

/**
 * Allocate and initialize a new ethash_light handler
 *
 * @param block_number   The block number for which to create the handler
 * @return               Newly allocated ethash_light handler or NULL in case of
 *                       ERRNOMEM or invalid parameters used for @ref ethash_compute_cache_nodes()
 */
ethash_light_t ethash_light_new(uint64_t block_number);
/**
 * Frees a previously allocated ethash_light handler
 * @param light        The light handler to free
 */
void ethash_light_delete(ethash_light_t light);

/**
 * Calculate the light client data
 *
 * @param light          The light client handler
 * @param header_hash    The header hash to pack into the mix
 * @param nonce          The nonce to pack into the mix
 * @return               an object of ethash_return_value_t holding the return values
 */
ethash_return_value_t ethash_light_compute(
	ethash_light_t light,
	ethash_h256_t const header_hash,
	uint64_t nonce
);

/**
 * Allocate and initialize a new ethash_full handler
 *
 * @param light         The light handler containing the cache.
 * @param callback      A callback function with signature of @ref ethash_callback_t
 *                      It accepts an unsigned with which a progress of DAG calculation
 *                      can be displayed. If all goes well the callback should return 0.
 *                      If a non-zero value is returned then DAG generation will stop.
 *                      Be advised. A progress value of 100 means that DAG creation is
 *                      almost complete and that this function will soon return succesfully.
 *                      It does not mean that the function has already had a succesfull return.
 * @return              Newly allocated ethash_full handler or NULL in case of
 *                      ERRNOMEM or invalid parameters used for @ref ethash_compute_full_data()
 */
ethash_full_t ethash_full_new(ethash_light_t light, ethash_callback_t callback);

/**
 * Frees a previously allocated ethash_full handler
 * @param full    The light handler to free
 */
void ethash_full_delete(ethash_full_t full);

/**
 * Calculate the full client data
 *
 * @param full           The full client handler
 * @param header_hash    The header hash to pack into the mix
 * @param nonce          The nonce to pack into the mix
 * @return               An object of ethash_return_value to hold the return value
 */
ethash_return_value_t ethash_full_compute(
	ethash_full_t full,
	ethash_h256_t const header_hash,
	uint64_t nonce
);

/**
 * Calculate the seedhash for a given block number
 */
ethash_h256_t ethash_get_seedhash(uint64_t block_number);

#ifdef __cplusplus
}
#endif