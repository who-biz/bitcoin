#include <komodo_defs.h>
#include <node/context.h>
#include <rpc/blockchain.h>
#include <validation.h>
#include <wallet/wallet.h>
#include <key_io.h>
#include <base58.h>

#define SATOSHIDEN ((uint64_t)100000000L)
#define dstr(x) ((double)(x) / SATOSHIDEN)
#define portable_mutex_t pthread_mutex_t
#define portable_mutex_init(ptr) pthread_mutex_init(ptr,NULL)
#define portable_mutex_lock pthread_mutex_lock
#define portable_mutex_unlock pthread_mutex_unlock

extern uint8_t NOTARY_PUBKEY33[33];

int32_t gettxout_scriptPubKey(int32_t height,uint8_t *scriptPubKey,int32_t maxsize,uint256 txid,int32_t n);
int32_t komodo_importaddress(std::string addr);
void vcalc_sha256(char deprecated[(256 >> 3) * 2 + 1],uint8_t hash[256 >> 3],uint8_t *src,int32_t len);
bits256 bits256_doublesha256(char *deprecated,uint8_t *data,int32_t datalen);
int32_t iguana_rwnum(int32_t rwflag,uint8_t *serialized,int32_t len,void *endianedp);
int32_t iguana_rwbignum(int32_t rwflag,uint8_t *serialized,int32_t len,uint8_t *endianedp);
int32_t bitweight(uint64_t x);
int32_t _unhex(char c);
int32_t is_hexstr(char *str,int32_t n);
int32_t unhex(char c);
unsigned char _decode_hex(char *hex);
int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex);
char hexbyte(int32_t c);
int32_t init_hexbytes_noT(char *hexbytes,unsigned char *message,long len);
int32_t komodo_blockload(CBlock& block,CBlockIndex *pindex);
int32_t komodo_blockheight(uint256 hash);
uint32_t komodo_chainactive_timestamp();
CBlockIndex *komodo_chainactive(int32_t height);
uint32_t komodo_heightstamp(int32_t height);
bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey);
bool pubkey2addr(char *destaddr,uint8_t *pubkey33);
void komodo_importpubkeys();
int32_t komodo_init();
bits256 iguana_merkle(bits256 *tree,int32_t txn_count);
uint256 komodo_calcMoM(int32_t height,int32_t MoMdepth);
int32_t getkmdseason(int32_t height);
int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);
void komodo_clearstate();
void komodo_disconnect(CBlockIndex *pindex,CBlock *block);
struct notarized_checkpoint *komodo_npptr(int32_t height);
int32_t komodo_prevMoMheight();
int32_t komodo_notarized_height(int32_t *prevMoMheightp,uint256 *hashp,uint256 *txidp);
int32_t komodo_MoMdata(int32_t *notarized_htp,uint256 *MoMp,uint256 *kmdtxidp,int32_t height,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip);
int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp);
void komodo_notarized_update(int32_t nHeight,int32_t notarized_height,uint256 notarized_hash,uint256 notarized_desttxid,uint256 MoM,int32_t MoMdepth);
int32_t komodo_checkpoint(int32_t *notarized_heightp,int32_t nHeight,uint256 hash);
void komodo_voutupdate(int32_t txi,int32_t vout,uint8_t *scriptbuf,int32_t scriptlen,int32_t height,int32_t *specialtxp,int32_t *notarizedheightp,uint64_t value,int32_t notarized,uint64_t signedmask);
void komodo_connectblock(CBlockIndex *pindex,CBlock& block);
CPubKey buf2pk(uint8_t *buf33);



