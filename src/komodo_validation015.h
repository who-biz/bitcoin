/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include <node/context.h>
#include <rpc/blockchain.h>
#include <validation.h>
#include <wallet/wallet.h>

// in init.cpp at top of AppInitParameterInteraction()
// int32_t komodo_init();
// komodo_init();

// in rpc/blockchain.cpp
// #include komodo_rpcblockchain.h
// ...
// { "blockchain",         "calc_MoM",               &calc_MoM,             {"height", "MoMdepth"}  },
// { "blockchain",         "height_MoM",             &height_MoM,             {"height"}  },

// in validation.cpp
// at end of ConnectBlock: komodo_connectblock(pindex,*(CBlock *)&block);
// at beginning DisconnectBlock: komodo_disconnect((CBlockIndex *)pindex,(CBlock *)&block);
/* add to ContextualCheckBlockHeader
    uint256 hash = block.GetHash();
    int32_t notarized_height;
    ....
    else if ( komodo_checkpoint(&notarized_height,(int32_t)nHeight,hash) < 0 )
    {
        CBlockIndex *heightblock = chainActive[nHeight];
        if ( heightblock != 0 && heightblock->GetBlockHash() == hash )
        {
            //fprintf(stderr,"got a pre notarization block that matches height.%d\n",(int32_t)nHeight);
            return true;
        } else return state.DoS(100, error("%s: forked chain %d older than last notarized (height %d) vs %d", __func__,nHeight, notarized_height));
    }
*/
/* add to getinfo
{
    int32_t komodo_prevMoMheight();
    extern uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID,NOTARIZED_MOM;
    extern int32_t NOTARIZED_HEIGHT,NOTARIZED_MOMDEPTH;
    obj.pushKV("notarizedhash",         NOTARIZED_HASH.GetHex());
    obj.pushKV("notarizedtxid",         NOTARIZED_DESTTXID.GetHex());
    obj.pushKV("notarized",                (int)NOTARIZED_HEIGHT);
    obj.pushKV("prevMoMheight",                (int)komodo_prevMoMheight());
    obj.pushKV("notarized_MoMdepth",                (int)NOTARIZED_MOMDEPTH);
    obj.pushKV("notarized_MoM",         NOTARIZED_MOM.GetHex());
}*/

#include <wallet/wallet.h>
#include <key_io.h>
#include <base58.h>

#define SATOSHIDEN ((uint64_t)100000000L)
#define dstr(x) ((double)(x) / SATOSHIDEN)
#define portable_mutex_t pthread_mutex_t
#define portable_mutex_init(ptr) pthread_mutex_init(ptr,NULL)
#define portable_mutex_lock pthread_mutex_lock
#define portable_mutex_unlock pthread_mutex_unlock

#define CRYPTO777_PUBSECPSTR "020e46e79a2a8d12b9b5d12c7a91adb4e454edfae43c0a0cb805427d2ac7613fd9"
#define KOMODO_MINRATIFY 2
#define KOMODO_ELECTION_GAP 2000
#define KOMODO_ASSETCHAIN_MAXLEN 65

// KMD Notary Seasons 
// 1: ENDS: May 1st 2018 1530921600
// 2: ENDS: July 15th 2019 1563148800 -> estimated height 4,173,578
// 3: 3rd season ending isnt known, so use very far times in future.
// to add 4th season, change NUM_KMD_SEASONS to 4, add height of activation for third season ending one spot before 999999999.
#define NUM_KMD_SEASONS 1
#define NUM_KMD_NOTARIES 64

//for CHIPS testnet
#define NUM_TESTNET_SEASONS   1
#define NUM_TESTNET_NOTARIES  2
#define TESTNET_MINRATIFY     2
static const int32_t TESTNET_SEASON_HEIGHTS[NUM_TESTNET_SEASONS] = { 999999999 };

static const char *notaries_elected_testnet[NUM_TESTNET_SEASONS][NUM_TESTNET_NOTARIES][2] =
{
  {
        //TODO: change these to correct pubkeys for testnet notas
        { "biz_testnet",      "030000000000000000000000000000000000000000000000000000000000000000" },
        { "SHossain_testnet", "030000000000000000000000000000000000000000000000000000000000000000" }
  }
};

static const bool testnet_enabled = (gArgs.GetBoolArg("-testnet",1) == 0);

// first season had no third party coins, so it ends at block 0. 
// second season ends at approx block 4,173,578, please check this!!!!! it should be as close as possible to July 15th 0:00 UTC. 
// third season ending height is unknown so it set to very very far in future. 
// Fourth season is ending at the timestamp 1623682800, and the approximate CHIPS block height of that time is of 8207456.
static const int32_t KMD_SEASON_HEIGHTS[NUM_KMD_SEASONS] = {999999999};

// Era array of pubkeys. Add extra seasons to bottom as requried, after adding appropriate info above. 
static const char *notaries_elected[NUM_KMD_SEASONS][NUM_KMD_NOTARIES][2] =
{
    {
        //TODO: change first two entries to proper pubkeys for biz & SHossain
        { "biz_testnet",      "030000000000000000000000000000000000000000000000000000000000000000" },
        { "SHossain_testnet", "030000000000000000000000000000000000000000000000000000000000000000" },
        { "0_kolo_testA", "0287aa4b73988ba26cf6565d815786caf0d2c4af704d7883d163ee89cd9977edec" },
        { "artik_AR", "029acf1dcd9f5ff9c455f8bb717d4ae0c703e089d16cf8424619c491dff5994c90" },
        { "artik_EU", "03f54b2c24f82632e3cdebe4568ba0acf487a80f8a89779173cdb78f74514847ce" },
        { "artik_NA", "0224e31f93eff0cc30eaf0b2389fbc591085c0e122c4d11862c1729d090106c842" },
        { "artik_SH", "02bdd8840a34486f38305f311c0e2ae73e84046f6e9c3dd3571e32e58339d20937" },
        { "badass_EU", "0209d48554768dd8dada988b98aca23405057ac4b5b46838a9378b95c3e79b9b9e" },
        { "badass_NA", "02afa1a9f948e1634a29dc718d218e9d150c531cfa852843a1643a02184a63c1a7" },
        { "badass_SH", "026b49dd3923b78a592c1b475f208e23698d3f085c4c3b4906a59faf659fd9530b" },
        { "crackers_EU", "03bc819982d3c6feb801ec3b720425b017d9b6ee9a40746b84422cbbf929dc73c3" }, // 10
        { "crackers_NA", "03205049103113d48c7c7af811b4c8f194dafc43a50d5313e61a22900fc1805b45" },
        { "crackers_SH", "02be28310e6312d1dd44651fd96f6a44ccc269a321f907502aae81d246fabdb03e" },
        { "durerus_EU", "02bcbd287670bdca2c31e5d50130adb5dea1b53198f18abeec7211825f47485d57" },
        { "etszombi_AR", "031c79168d15edabf17d9ec99531ea9baa20039d0cdc14d9525863b83341b210e9" },
        { "etszombi_EU", "0281b1ad28d238a2b217e0af123ce020b79e91b9b10ad65a7917216eda6fe64bf7" }, // 15
        { "etszombi_SH", "025d7a193c0757f7437fad3431f027e7b5ed6c925b77daba52a8755d24bf682dde" },
        { "farl4web_EU", "0300ecf9121cccf14cf9423e2adb5d98ce0c4e251721fa345dec2e03abeffbab3f" },
        { "farl4web_SH", "0396bb5ed3c57aa1221d7775ae0ff751e4c7dc9be220d0917fa8bbdf670586c030" },
        { "fullmoon_AR", "0254b1d64840ce9ff6bec9dd10e33beb92af5f7cee628f999cb6bc0fea833347cc" },
        { "fullmoon_NA", "031fb362323b06e165231c887836a8faadb96eda88a79ca434e28b3520b47d235b" }, // 20
        { "fullmoon_SH", "030e12b42ec33a80e12e570b6c8274ce664565b5c3da106859e96a7208b93afd0d" },
        { "grewal_NA", "03adc0834c203d172bce814df7c7a5e13dc603105e6b0adabc942d0421aefd2132" },
        { "grewal_SH", "03212a73f5d38a675ee3cdc6e82542a96c38c3d1c79d25a1ed2e42fcf6a8be4e68" },
        { "indenodes_AR", "02ec0fa5a40f47fd4a38ea5c89e375ad0b6ddf4807c99733c9c3dc15fb978ee147" },
        { "indenodes_EU", "0221387ff95c44cb52b86552e3ec118a3c311ca65b75bf807c6c07eaeb1be8303c" },
        { "indenodes_NA", "02698c6f1c9e43b66e82dbb163e8df0e5a2f62f3a7a882ca387d82f86e0b3fa988" },
        { "indenodes_SH", "0334e6e1ec8285c4b85bd6dae67e17d67d1f20e7328efad17ce6fd24ae97cdd65e" },
        { "jeezy_EU", "023cb3e593fb85c5659688528e9a4f1c4c7f19206edc7e517d20f794ba686fd6d6" },
        { "jsgalt_NA", "027b3fb6fede798cd17c30dbfb7baf9332b3f8b1c7c513f443070874c410232446" },
        { "karasugoi_NA", "02a348b03b9c1a8eac1b56f85c402b041c9bce918833f2ea16d13452309052a982" }, // 30
        { "kashifali_EU", "033777c52a0190f261c6f66bd0e2bb299d30f012dcb8bfff384103211edb8bb207" },
        { "kolo_AR", "03016d19344c45341e023b72f9fb6e6152fdcfe105f3b4f50b82a4790ff54e9dc6" },
        { "kolo_SH", "02aa24064500756d9b0959b44d5325f2391d8e95c6127e109184937152c384e185" },
        { "metaphilibert_AR", "02adad675fae12b25fdd0f57250b0caf7f795c43f346153a31fe3e72e7db1d6ac6" },
        { "movecrypto_AR", "022783d94518e4dc77cbdf1a97915b29f427d7bc15ea867900a76665d3112be6f3" },
        { "movecrypto_EU", "021ab53bc6cf2c46b8a5456759f9d608966eff87384c2b52c0ac4cc8dd51e9cc42" },
        { "movecrypto_NA", "02efb12f4d78f44b0542d1c60146738e4d5506d27ec98a469142c5c84b29de0a80" },
        { "movecrypto_SH", "031f9739a3ebd6037a967ce1582cde66e79ea9a0551c54731c59c6b80f635bc859" },
        { "muros_AR", "022d77402fd7179335da39479c829be73428b0ef33fb360a4de6890f37c2aa005e" },
        { "noashh_AR", "029d93ef78197dc93892d2a30e5a54865f41e0ca3ab7eb8e3dcbc59c8756b6e355" }, // 40
        { "noashh_EU", "02061c6278b91fd4ac5cab4401100ffa3b2d5a277e8f71db23401cc071b3665546" },
        { "noashh_NA", "033c073366152b6b01535e15dd966a3a8039169584d06e27d92a69889b720d44e1" },
        { "nxtswe_EU", "032fb104e5eaa704a38a52c126af8f67e870d70f82977e5b2f093d5c1c21ae5899" },
        { "polycryptoblog_NA", "02708dcda7c45fb54b78469673c2587bfdd126e381654819c4c23df0e00b679622" },
        { "pondsea_AR", "032e1c213787312099158f2d74a89e8240a991d162d4ce8017d8504d1d7004f735" },
        { "pondsea_EU", "0225aa6f6f19e543180b31153d9e6d55d41bc7ec2ba191fd29f19a2f973544e29d" },
        { "pondsea_NA", "031bcfdbb62268e2ff8dfffeb9ddff7fe95fca46778c77eebff9c3829dfa1bb411" },
        { "pondsea_SH", "02209073bc0943451498de57f802650311b1f12aa6deffcd893da198a544c04f36" },
        { "popcornbag_AR", "02761f106fb34fbfc5ddcc0c0aa831ed98e462a908550b280a1f7bd32c060c6fa3" },
        { "popcornbag_NA", "03c6085c7fdfff70988fda9b197371f1caf8397f1729a844790e421ee07b3a93e8" }, // 50
        { "ptytrader_NA", "0328c61467148b207400b23875234f8a825cce65b9c4c9b664f47410b8b8e3c222" },
        { "ptytrader_SH", "0250c93c492d8d5a6b565b90c22bee07c2d8701d6118c6267e99a4efd3c7748fa4" },
        { "rnr_AR", "029bdb08f931c0e98c2c4ba4ef45c8e33a34168cb2e6bf953cef335c359d77bfcd" },
        { "rnr_EU", "03f5c08dadffa0ffcafb8dd7ffc38c22887bd02702a6c9ac3440deddcf2837692b" },
        { "rnr_NA", "02e17c5f8c3c80f584ed343b8dcfa6d710dfef0889ec1e7728ce45ce559347c58c" },
        { "rnr_SH", "037536fb9bdfed10251f71543fb42679e7c52308bcd12146b2568b9a818d8b8377" },
        { "titomane_AR", "03cda6ca5c2d02db201488a54a548dbfc10533bdc275d5ea11928e8d6ab33c2185" },
        { "titomane_EU", "02e41feded94f0cc59f55f82f3c2c005d41da024e9a805b41105207ef89aa4bfbd" },
        { "titomane_SH", "035f49d7a308dd9a209e894321f010d21b7793461b0c89d6d9231a3fe5f68d9960" },
        { "vanbreuk_EU", "024f3cad7601d2399c131fd070e797d9cd8533868685ddbe515daa53c2e26004c3" }, // 60
        { "xrobesx_NA", "03f0cc6d142d14a40937f12dbd99dbd9021328f45759e26f1877f2a838876709e1" },
        { "xxspot1_XX", "02ef445a392fcaf3ad4176a5da7f43580e8056594e003eba6559a713711a27f955" },
        { "xxspot2_XX", "03d85b221ea72ebcd25373e7961f4983d12add66a92f899deaf07bab1d8b6f5573" }
   }
};

union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
typedef union _bits256 bits256;

struct sha256_vstate { uint64_t length; uint32_t state[8],curlen; uint8_t buf[64]; };
struct rmd160_vstate { uint64_t length; uint8_t buf[64]; uint32_t curlen, state[5]; };
int32_t KOMODO_TXINDEX = 1;
void ImportAddress(CWallet*, const CTxDestination& dest, const std::string& strLabel);

int32_t gettxout_scriptPubKey(int32_t height,uint8_t *scriptPubKey,int32_t maxsize,uint256 txid,int32_t n)
{
    static uint256 zero; int32_t i,m; uint8_t *ptr; CTransactionRef tx=0; uint256 hashBlock;
    LOCK(cs_main);
    if ( KOMODO_TXINDEX != 0 )
    {
        CBlockIndex *pindex = g_rpc_node->chainman->ActiveChain().Tip();

        if (tx = GetTransaction(pindex, nullptr, txid, Params().GetConsensus(), hashBlock))
        {
            //fprintf(stderr,"ht.%d couldnt get txid.%s\n",height,txid.GetHex().c_str());
            return(-1);
        }
    }
    else
    {
        CWallet * const pwallet = GetMainWallet().get();
        if ( pwallet != 0 )
        {
            auto it = pwallet->mapWallet.find(txid);
            if ( it != pwallet->mapWallet.end() )
            {
                const CWalletTx& wtx = it->second;
                tx = wtx.tx;
                //fprintf(stderr,"found tx in wallet\n");
            }
        }
    }
    if ( tx != 0 && n >= 0 && n <= (int32_t)tx->vout.size() ) // vout.size() seems off by 1
    {
        ptr = (uint8_t *)tx->vout[n].scriptPubKey.data();
        m = tx->vout[n].scriptPubKey.size();
        for (i=0; i<maxsize&&i<m; i++)
            scriptPubKey[i] = ptr[i];
        //fprintf(stderr,"got scriptPubKey[%d] via rawtransaction ht.%d %s\n",m,height,txid.GetHex().c_str());
        return(i);
    }
    else if ( tx != 0 )
        fprintf(stderr,"gettxout_scriptPubKey ht.%d n.%d > voutsize.%d\n",height,n,(int32_t)tx->vout.size());
    return(-1);
}

int32_t komodo_importaddress(std::string addr)
{
    CTxDestination address; CWallet * const pwallet = GetMainWallet().get();
    if ( pwallet != 0 )
    {
        LOCK2(cs_main, pwallet->cs_wallet);
        address = DecodeDestination(addr);
        if ( IsValidDestination(address) != 0 )
        {
            isminetype mine = pwallet->IsMine(address);
            if ( (mine & ISMINE_SPENDABLE) != 0 || (mine & ISMINE_WATCH_ONLY) != 0 )
            {
                //printf("komodo_importaddress %s already there\n",EncodeDestination(address).c_str());
                return(0);
            }
            else
            {
                printf("komodo_importaddress %s\n",EncodeDestination(address).c_str());
                ImportAddress(pwallet, address, addr);
                return(1);
            }
        }
        printf("%s -> komodo_importaddress.(%s) failed valid.%d\n",addr.c_str(),EncodeDestination(address).c_str(),IsValidDestination(address));
    }
    return(-1);
}

// following is ported from libtom
/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtom.org
 */

#define STORE32L(x, y)                                                                     \
{ (y)[3] = (uint8_t)(((x)>>24)&255); (y)[2] = (uint8_t)(((x)>>16)&255);   \
(y)[1] = (uint8_t)(((x)>>8)&255); (y)[0] = (uint8_t)((x)&255); }

#define LOAD32L(x, y)                            \
{ x = (uint32_t)(((uint64_t)((y)[3] & 255)<<24) | \
((uint32_t)((y)[2] & 255)<<16) | \
((uint32_t)((y)[1] & 255)<<8)  | \
((uint32_t)((y)[0] & 255))); }

#define STORE64L(x, y)                                                                     \
{ (y)[7] = (uint8_t)(((x)>>56)&255); (y)[6] = (uint8_t)(((x)>>48)&255);   \
(y)[5] = (uint8_t)(((x)>>40)&255); (y)[4] = (uint8_t)(((x)>>32)&255);   \
(y)[3] = (uint8_t)(((x)>>24)&255); (y)[2] = (uint8_t)(((x)>>16)&255);   \
(y)[1] = (uint8_t)(((x)>>8)&255); (y)[0] = (uint8_t)((x)&255); }

#define LOAD64L(x, y)                                                       \
{ x = (((uint64_t)((y)[7] & 255))<<56)|(((uint64_t)((y)[6] & 255))<<48)| \
(((uint64_t)((y)[5] & 255))<<40)|(((uint64_t)((y)[4] & 255))<<32)| \
(((uint64_t)((y)[3] & 255))<<24)|(((uint64_t)((y)[2] & 255))<<16)| \
(((uint64_t)((y)[1] & 255))<<8)|(((uint64_t)((y)[0] & 255))); }

#define STORE32H(x, y)                                                                     \
{ (y)[0] = (uint8_t)(((x)>>24)&255); (y)[1] = (uint8_t)(((x)>>16)&255);   \
(y)[2] = (uint8_t)(((x)>>8)&255); (y)[3] = (uint8_t)((x)&255); }

#define LOAD32H(x, y)                            \
{ x = (uint32_t)(((uint64_t)((y)[0] & 255)<<24) | \
((uint32_t)((y)[1] & 255)<<16) | \
((uint32_t)((y)[2] & 255)<<8)  | \
((uint32_t)((y)[3] & 255))); }

#define STORE64H(x, y)                                                                     \
{ (y)[0] = (uint8_t)(((x)>>56)&255); (y)[1] = (uint8_t)(((x)>>48)&255);     \
(y)[2] = (uint8_t)(((x)>>40)&255); (y)[3] = (uint8_t)(((x)>>32)&255);     \
(y)[4] = (uint8_t)(((x)>>24)&255); (y)[5] = (uint8_t)(((x)>>16)&255);     \
(y)[6] = (uint8_t)(((x)>>8)&255); (y)[7] = (uint8_t)((x)&255); }

#define LOAD64H(x, y)                                                      \
{ x = (((uint64_t)((y)[0] & 255))<<56)|(((uint64_t)((y)[1] & 255))<<48) | \
(((uint64_t)((y)[2] & 255))<<40)|(((uint64_t)((y)[3] & 255))<<32) | \
(((uint64_t)((y)[4] & 255))<<24)|(((uint64_t)((y)[5] & 255))<<16) | \
(((uint64_t)((y)[6] & 255))<<8)|(((uint64_t)((y)[7] & 255))); }

// Various logical functions
#define RORc(x, y) ( ((((uint32_t)(x)&0xFFFFFFFFUL)>>(uint32_t)((y)&31)) | ((uint32_t)(x)<<(uint32_t)(32-((y)&31)))) & 0xFFFFFFFFUL)
#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y))
#define S(x, n)         RORc((x),(n))
#define R(x, n)         (((x)&0xFFFFFFFFUL)>>(n))
#define Sigma0(x)       (S(x, 2) ^ S(x, 13) ^ S(x, 22))
#define Sigma1(x)       (S(x, 6) ^ S(x, 11) ^ S(x, 25))
#define Gamma0(x)       (S(x, 7) ^ S(x, 18) ^ R(x, 3))
#define Gamma1(x)       (S(x, 17) ^ S(x, 19) ^ R(x, 10))
#define MIN(x, y) ( ((x)<(y))?(x):(y) )

static inline int32_t sha256_vcompress(struct sha256_vstate * md,uint8_t *buf)
{
    uint32_t S[8],W[64],t0,t1,i;
    for (i=0; i<8; i++) // copy state into S
        S[i] = md->state[i];
    for (i=0; i<16; i++) // copy the state into 512-bits into W[0..15]
        LOAD32H(W[i],buf + (4*i));
    for (i=16; i<64; i++) // fill W[16..63]
        W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
    
#define RND(a,b,c,d,e,f,g,h,i,ki)                    \
t0 = h + Sigma1(e) + Ch(e, f, g) + ki + W[i];   \
t1 = Sigma0(a) + Maj(a, b, c);                  \
d += t0;                                        \
h  = t0 + t1;
    
    RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],0,0x428a2f98);
    RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],1,0x71374491);
    RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],2,0xb5c0fbcf);
    RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],3,0xe9b5dba5);
    RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],4,0x3956c25b);
    RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],5,0x59f111f1);
    RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],6,0x923f82a4);
    RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],7,0xab1c5ed5);
    RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],8,0xd807aa98);
    RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],9,0x12835b01);
    RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],10,0x243185be);
    RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],11,0x550c7dc3);
    RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],12,0x72be5d74);
    RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],13,0x80deb1fe);
    RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],14,0x9bdc06a7);
    RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],15,0xc19bf174);
    RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],16,0xe49b69c1);
    RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],17,0xefbe4786);
    RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],18,0x0fc19dc6);
    RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],19,0x240ca1cc);
    RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],20,0x2de92c6f);
    RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],21,0x4a7484aa);
    RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],22,0x5cb0a9dc);
    RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],23,0x76f988da);
    RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],24,0x983e5152);
    RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],25,0xa831c66d);
    RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],26,0xb00327c8);
    RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],27,0xbf597fc7);
    RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],28,0xc6e00bf3);
    RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],29,0xd5a79147);
    RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],30,0x06ca6351);
    RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],31,0x14292967);
    RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],32,0x27b70a85);
    RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],33,0x2e1b2138);
    RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],34,0x4d2c6dfc);
    RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],35,0x53380d13);
    RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],36,0x650a7354);
    RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],37,0x766a0abb);
    RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],38,0x81c2c92e);
    RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],39,0x92722c85);
    RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],40,0xa2bfe8a1);
    RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],41,0xa81a664b);
    RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],42,0xc24b8b70);
    RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],43,0xc76c51a3);
    RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],44,0xd192e819);
    RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],45,0xd6990624);
    RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],46,0xf40e3585);
    RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],47,0x106aa070);
    RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],48,0x19a4c116);
    RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],49,0x1e376c08);
    RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],50,0x2748774c);
    RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],51,0x34b0bcb5);
    RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],52,0x391c0cb3);
    RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],53,0x4ed8aa4a);
    RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],54,0x5b9cca4f);
    RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],55,0x682e6ff3);
    RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],56,0x748f82ee);
    RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],57,0x78a5636f);
    RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],58,0x84c87814);
    RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],59,0x8cc70208);
    RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],60,0x90befffa);
    RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],61,0xa4506ceb);
    RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],62,0xbef9a3f7);
    RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],63,0xc67178f2);
#undef RND
    for (i=0; i<8; i++) // feedback
        md->state[i] = md->state[i] + S[i];
    return(0);
}

#undef RORc
#undef Ch
#undef Maj
#undef S
#undef R
#undef Sigma0
#undef Sigma1
#undef Gamma0
#undef Gamma1

static inline void sha256_vinit(struct sha256_vstate * md)
{
    md->curlen = 0;
    md->length = 0;
    md->state[0] = 0x6A09E667UL;
    md->state[1] = 0xBB67AE85UL;
    md->state[2] = 0x3C6EF372UL;
    md->state[3] = 0xA54FF53AUL;
    md->state[4] = 0x510E527FUL;
    md->state[5] = 0x9B05688CUL;
    md->state[6] = 0x1F83D9ABUL;
    md->state[7] = 0x5BE0CD19UL;
}

static inline int32_t sha256_vprocess(struct sha256_vstate *md,const uint8_t *in,uint64_t inlen)
{
    uint64_t n; int32_t err;
    if ( md->curlen > sizeof(md->buf) )
        return(-1);
    while ( inlen > 0 )
    {
        if ( md->curlen == 0 && inlen >= 64 )
        {
            if ( (err= sha256_vcompress(md,(uint8_t *)in)) != 0 )
                return(err);
            md->length += 64 * 8, in += 64, inlen -= 64;
        }
        else
        {
            n = MIN(inlen,64 - md->curlen);
            memcpy(md->buf + md->curlen,in,(size_t)n);
            md->curlen += n, in += n, inlen -= n;
            if ( md->curlen == 64 )
            {
                if ( (err= sha256_vcompress(md,md->buf)) != 0 )
                    return(err);
                md->length += 8*64;
                md->curlen = 0;
            }
        }
    }
    return(0);
}

static inline int32_t sha256_vdone(struct sha256_vstate *md,uint8_t *out)
{
    int32_t i;
    if ( md->curlen >= sizeof(md->buf) )
        return(-1);
    md->length += md->curlen * 8; // increase the length of the message
    md->buf[md->curlen++] = (uint8_t)0x80; // append the '1' bit
    // if len > 56 bytes we append zeros then compress.  Then we can fall back to padding zeros and length encoding like normal.
    if ( md->curlen > 56 )
    {
        while ( md->curlen < 64 )
            md->buf[md->curlen++] = (uint8_t)0;
        sha256_vcompress(md,md->buf);
        md->curlen = 0;
    }
    while ( md->curlen < 56 ) // pad upto 56 bytes of zeroes
        md->buf[md->curlen++] = (uint8_t)0;
    STORE64H(md->length,md->buf+56); // store length
    sha256_vcompress(md,md->buf);
    for (i=0; i<8; i++) // copy output
        STORE32H(md->state[i],out+(4*i));
    return(0);
}
// end libtom

void vcalc_sha256(char deprecated[(256 >> 3) * 2 + 1],uint8_t hash[256 >> 3],uint8_t *src,int32_t len)
{
    struct sha256_vstate md;
    sha256_vinit(&md);
    sha256_vprocess(&md,src,len);
    sha256_vdone(&md,hash);
}

bits256 bits256_doublesha256(char *deprecated,uint8_t *data,int32_t datalen)
{
    bits256 hash,hash2; int32_t i;
    vcalc_sha256(0,hash.bytes,data,datalen);
    vcalc_sha256(0,hash2.bytes,hash.bytes,sizeof(hash));
    for (i=0; i<(int32_t)sizeof(hash); i++)
        hash.bytes[i] = hash2.bytes[sizeof(hash) - 1 - i];
    return(hash);
}

int32_t iguana_rwnum(int32_t rwflag,uint8_t *serialized,int32_t len,void *endianedp)
{
    int32_t i; uint64_t x;
    if ( rwflag == 0 )
    {
        x = 0;
        for (i=len-1; i>=0; i--)
        {
            x <<= 8;
            x |= serialized[i];
        }
        switch ( len )
        {
            case 1: *(uint8_t *)endianedp = (uint8_t)x; break;
            case 2: *(uint16_t *)endianedp = (uint16_t)x; break;
            case 4: *(uint32_t *)endianedp = (uint32_t)x; break;
            case 8: *(uint64_t *)endianedp = (uint64_t)x; break;
        }
    }
    else
    {
        x = 0;
        switch ( len )
        {
            case 1: x = *(uint8_t *)endianedp; break;
            case 2: x = *(uint16_t *)endianedp; break;
            case 4: x = *(uint32_t *)endianedp; break;
            case 8: x = *(uint64_t *)endianedp; break;
        }
        for (i=0; i<len; i++,x >>= 8)
            serialized[i] = (uint8_t)(x & 0xff);
    }
    return(len);
}

int32_t iguana_rwbignum(int32_t rwflag,uint8_t *serialized,int32_t len,uint8_t *endianedp)
{
    int32_t i;
    if ( rwflag == 0 )
    {
        for (i=0; i<len; i++)
            endianedp[i] = serialized[i];
    }
    else
    {
        for (i=0; i<len; i++)
            serialized[i] = endianedp[i];
    }
    return(len);
}

int32_t bitweight(uint64_t x)
{
    int i,wt = 0;
    for (i=0; i<64; i++)
        if ( (1LL << i) & x )
            wt++;
    return(wt);
}

int32_t _unhex(char c)
{
    if ( c >= '0' && c <= '9' )
        return(c - '0');
    else if ( c >= 'a' && c <= 'f' )
        return(c - 'a' + 10);
    else if ( c >= 'A' && c <= 'F' )
        return(c - 'A' + 10);
    return(-1);
}

int32_t is_hexstr(char *str,int32_t n)
{
    int32_t i;
    if ( str == 0 || str[0] == 0 )
        return(0);
    for (i=0; str[i]!=0; i++)
    {
        if ( n > 0 && i >= n )
            break;
        if ( _unhex(str[i]) < 0 )
            break;
    }
    if ( n == 0 )
        return(i);
    return(i == n);
}

int32_t unhex(char c)
{
    int32_t hex;
    if ( (hex= _unhex(c)) < 0 )
    {
        //printf("unhex: illegal hexchar.(%c)\n",c);
    }
    return(hex);
}

unsigned char _decode_hex(char *hex) { return((unhex(hex[0])<<4) | unhex(hex[1])); }

int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex)
{
    int32_t adjust,i = 0;
    //printf("decode.(%s)\n",hex);
    if ( is_hexstr(hex,n) <= 0 )
    {
        memset(bytes,0,n);
        return(n);
    }
    if ( hex[n-1] == '\n' || hex[n-1] == '\r' )
        hex[--n] = 0;
    if ( n == 0 || (hex[n*2+1] == 0 && hex[n*2] != 0) )
    {
        if ( n > 0 )
        {
            bytes[0] = unhex(hex[0]);
            printf("decode_hex n.%d hex[0] (%c) -> %d hex.(%s) [n*2+1: %d] [n*2: %d %c] len.%ld\n",n,hex[0],bytes[0],hex,hex[n*2+1],hex[n*2],hex[n*2],(long)strlen(hex));
        }
        bytes++;
        hex++;
        adjust = 1;
    } else adjust = 0;
    if ( n > 0 )
    {
        for (i=0; i<n; i++)
            bytes[i] = _decode_hex(&hex[i*2]);
    }
    //bytes[i] = 0;
    return(n + adjust);
}

char hexbyte(int32_t c)
{
    c &= 0xf;
    if ( c < 10 )
        return('0'+c);
    else if ( c < 16 )
        return('a'+c-10);
    else return(0);
}

int32_t init_hexbytes_noT(char *hexbytes,unsigned char *message,long len)
{
    int32_t i;
    if ( len <= 0 )
    {
        hexbytes[0] = 0;
        return(1);
    }
    for (i=0; i<len; i++)
    {
        hexbytes[i*2] = hexbyte((message[i]>>4) & 0xf);
        hexbytes[i*2 + 1] = hexbyte(message[i] & 0xf);
        //printf("i.%d (%02x) [%c%c]\n",i,message[i],hexbytes[i*2],hexbytes[i*2+1]);
    }
    hexbytes[len*2] = 0;
    //printf("len.%ld\n",len*2+1);
    return((int32_t)len*2+1);
}

uint32_t komodo_chainactive_timestamp()
{
    if ( g_rpc_node->chainman->ActiveChain().Tip() != 0 )
        return((uint32_t)g_rpc_node->chainman->ActiveChain().Tip()->GetBlockTime());
    else return(0);
}

CBlockIndex *komodo_chainactive(int32_t height)
{
    CBlockIndex *tipindex;
    if ( (tipindex= g_rpc_node->chainman->ActiveChain().Tip()) != 0 )
    {
        if ( height <= tipindex->nHeight )
            return(g_rpc_node->chainman->ActiveChain()[height]);
        // else fprintf(stderr,"komodo_chainactive height %d > active.%d\n",height,g_rpc_node->chainman->ActiveChain().Tip()->nHeight);
    }
    //fprintf(stderr,"komodo_chainactive null g_rpc_node->chainman->ActiveChain().Tip() height %d\n",height);
    return(0);
}

uint32_t komodo_heightstamp(int32_t height)
{
    CBlockIndex *ptr;
    if ( height > 0 && (ptr= komodo_chainactive(height)) != 0 )
        return(ptr->nTime);
    //else fprintf(stderr,"komodo_heightstamp null ptr for block.%d\n",height);
    return(0);
}

bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey)
{
    CTxDestination address;
    if ( ExtractDestination(scriptPubKey,address) != 0 )
    {
        strcpy(destaddr,(char *)EncodeDestination(address).c_str());
        return(true);
    }
    //fprintf(stderr,"ExtractDestination failed\n");
    return(false);
}

bool pubkey2addr(char *destaddr,uint8_t *pubkey33)
{
    std::vector<uint8_t>pk; int32_t i;
    for (i=0; i<33; i++)
        pk.push_back(pubkey33[i]);
    return(Getscriptaddress(destaddr,CScript() << pk << OP_CHECKSIG));
}

struct notarized_checkpoint
{
    uint256 notarized_hash,notarized_desttxid,MoM,MoMoM;
    int32_t nHeight,notarized_height,MoMdepth,MoMoMdepth,MoMoMoffset,kmdstarti,kmdendi;
} *NPOINTS;
std::string NOTARY_PUBKEY;
uint8_t NOTARY_PUBKEY33[33];
uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID,NOTARIZED_MOM;
int32_t NUM_NPOINTS,last_NPOINTSi,NOTARIZED_HEIGHT,NOTARIZED_MOMDEPTH,KOMODO_NEEDPUBKEYS;
portable_mutex_t komodo_mutex;

void komodo_importpubkeys()
{
    int32_t i,n,val,dispflag = 0; std::string addr;
    if (testnet_enabled == true)
    {
        for (n = 0; n < NUM_TESTNET_SEASONS; n++)
        {
            for (i=0; i<NUM_TESTNET_NOTARIES; i++) 
            {
                 uint8_t pubkey[33]; char address[64];
                 decode_hex(pubkey,33,(char *)notaries_elected[n][i][1]);
                 pubkey2addr((char *)address,(uint8_t *)pubkey);
                 addr.clear();
                 addr.append(address);
                 if ( (val= komodo_importaddress(addr)) < 0 )
                      fprintf(stderr,"error importing (%s)\n",addr.c_str());
                 else if ( val != 0 )
                     dispflag++;
            }
        }
    }
    else
    {
        for (n = 0; n < NUM_KMD_SEASONS; n++)
        {
            for (i=0; i<NUM_KMD_NOTARIES; i++) 
            {
                 uint8_t pubkey[33]; char address[64];
                 decode_hex(pubkey,33,(char *)notaries_elected[n][i][1]);
                 pubkey2addr((char *)address,(uint8_t *)pubkey);
                 addr.clear();
                 addr.append(address);
                 if ( (val= komodo_importaddress(addr)) < 0 )
                      fprintf(stderr,"error importing (%s)\n",addr.c_str());
                 else if ( val != 0 )
                     dispflag++;
            }
        }
    }
    if ( dispflag != 0 )
        fprintf(stderr,"%d Notary pubkeys imported\n",dispflag);
} 

int32_t komodo_init()
{
    NOTARY_PUBKEY = gArgs.GetArg("-pubkey", "");
    decode_hex(NOTARY_PUBKEY33,33,(char *)NOTARY_PUBKEY.c_str());
    if ( gArgs.GetBoolArg("-txindex", DEFAULT_TXINDEX) == 0 )
    {
        fprintf(stderr,"txindex is off, import notary pubkeys\n");
        KOMODO_NEEDPUBKEYS = 1;
        KOMODO_TXINDEX = 0;
    }
    return(0);
}

bits256 iguana_merkle(bits256 *tree,int32_t txn_count)
{
    int32_t i,n=0,prev; uint8_t serialized[sizeof(bits256) * 2];
    if ( txn_count == 1 )
        return(tree[0]);
    prev = 0;
    while ( txn_count > 1 )
    {
        if ( (txn_count & 1) != 0 )
            tree[prev + txn_count] = tree[prev + txn_count-1], txn_count++;
        n += txn_count;
        for (i=0; i<txn_count; i+=2)
        {
            iguana_rwbignum(1,serialized,sizeof(*tree),tree[prev + i].bytes);
            iguana_rwbignum(1,&serialized[sizeof(*tree)],sizeof(*tree),tree[prev + i + 1].bytes);
            tree[n + (i >> 1)] = bits256_doublesha256(0,serialized,sizeof(serialized));
        }
        prev = n;
        txn_count >>= 1;
    }
    return(tree[n]);
}

uint256 komodo_calcMoM(int32_t height,int32_t MoMdepth)
{
    static uint256 zero; bits256 MoM,*tree; CBlockIndex *pindex; int32_t i;
    if ( MoMdepth >= height )
        return(zero);
    tree = (bits256 *)calloc(MoMdepth * 3,sizeof(*tree));
    for (i=0; i<MoMdepth; i++)
    {
        if ( (pindex= komodo_chainactive(height - i)) != 0 )
            memcpy(&tree[i],&pindex->hashMerkleRoot,sizeof(bits256));
        else
        {
            free(tree);
            return(zero);
        }
    }
    MoM = iguana_merkle(tree,MoMdepth);
    free(tree);
    return(*(uint256 *)&MoM);
}

int32_t getkmdseason(int32_t height)
{
    if (testnet_enabled == true)
    {
        if ( height <= TESTNET_SEASON_HEIGHTS[0] )
            return(1);
        for (int32_t i = 1; i < NUM_TESTNET_SEASONS; i++)
        {
            if ( height <= TESTNET_SEASON_HEIGHTS[i] && height >= TESTNET_SEASON_HEIGHTS[i-1] )
                return(i+1);
        }
    }
    else
    {
        if ( height <= KMD_SEASON_HEIGHTS[0] )
            return(1);
        for (int32_t i = 1; i < NUM_KMD_SEASONS; i++)
        {
            if ( height <= KMD_SEASON_HEIGHTS[i] && height >= KMD_SEASON_HEIGHTS[i-1] )
                return(i+1);
        }
    }
    return(0);
}

int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp)
{
    if (testnet_enabled == true)
    {
        static uint8_t kmd_pubkeys[NUM_TESTNET_SEASONS][64][33],didinit[NUM_TESTNET_SEASONS];
        int32_t kmd_season,i;
        if ( (kmd_season= getkmdseason(height)) != 0 )
        {
            if ( didinit[kmd_season-1] == 0 )
            {
                for (i=0; i<NUM_TESTNET_NOTARIES; i++) 
                    decode_hex(kmd_pubkeys[kmd_season-1][i],33,(char *)notaries_elected[kmd_season-1][i][1]);
                didinit[kmd_season-1] = 1;
            }
            memcpy(pubkeys,kmd_pubkeys[kmd_season-1],NUM_TESTNET_NOTARIES * 33);
            return(NUM_TESTNET_NOTARIES);
        }
    }
    else
    {
        static uint8_t kmd_pubkeys[NUM_KMD_SEASONS][64][33],didinit[NUM_KMD_SEASONS];
        int32_t kmd_season,i;
        if ( (kmd_season= getkmdseason(height)) != 0 )
        {
            if ( didinit[kmd_season-1] == 0 )
            {
                for (i=0; i<NUM_KMD_NOTARIES; i++) 
                    decode_hex(kmd_pubkeys[kmd_season-1][i],33,(char *)notaries_elected[kmd_season-1][i][1]);
                didinit[kmd_season-1] = 1;
            }
            memcpy(pubkeys,kmd_pubkeys[kmd_season-1],NUM_KMD_NOTARIES * 33);
            return(NUM_KMD_NOTARIES);
        }
    }
    return(-1);
}

void komodo_clearstate()
{
    portable_mutex_lock(&komodo_mutex);
    memset(&NOTARIZED_HEIGHT,0,sizeof(NOTARIZED_HEIGHT));
    memset(&NOTARIZED_HASH,0,sizeof(NOTARIZED_HASH));
    memset(&NOTARIZED_DESTTXID,0,sizeof(NOTARIZED_DESTTXID));
    memset(&NOTARIZED_MOM,0,sizeof(NOTARIZED_MOM));
    memset(&NOTARIZED_MOMDEPTH,0,sizeof(NOTARIZED_MOMDEPTH));
    memset(&last_NPOINTSi,0,sizeof(last_NPOINTSi));
    portable_mutex_unlock(&komodo_mutex);
}

void komodo_disconnect(CBlockIndex *pindex,CBlock *block)
{
    if ( (int32_t)pindex->nHeight <= NOTARIZED_HEIGHT )
    {
        fprintf(stderr,"komodo_disconnect unexpected reorg pindex->nHeight.%d vs %d\n",(int32_t)pindex->nHeight,NOTARIZED_HEIGHT);
        komodo_clearstate(); // bruteforce shortcut. on any reorg, no active notarization until next one is seen
    }
}

struct notarized_checkpoint *komodo_npptr(int32_t height)
{
    int32_t i; struct notarized_checkpoint *np = 0;
    for (i=NUM_NPOINTS-1; i>=0; i--)
    {
        np = &NPOINTS[i];
        if ( np->MoMdepth > 0 && height > np->notarized_height-np->MoMdepth && height <= np->notarized_height )
            return(np);
    }
    return(0);
}

int32_t komodo_prevMoMheight()
{
    static uint256 zero;
    int32_t i; struct notarized_checkpoint *np = 0;
    for (i=NUM_NPOINTS-1; i>=0; i--)
    {
        np = &NPOINTS[i];
        if ( np->MoM != zero )
            return(np->notarized_height);
    }
    return(0);
}

//struct komodo_state *komodo_stateptr(char *symbol,char *dest);
int32_t komodo_notarized_height(int32_t *prevMoMheightp,uint256 *hashp,uint256 *txidp)
{
    *hashp = NOTARIZED_HASH;
    *txidp = NOTARIZED_DESTTXID;
    *prevMoMheightp = komodo_prevMoMheight();
    return(NOTARIZED_HEIGHT);
}

int32_t komodo_MoMdata(int32_t *notarized_htp,uint256 *MoMp,uint256 *kmdtxidp,int32_t height,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip)
{
    struct notarized_checkpoint *np = 0;
    if ( (np= komodo_npptr(height)) != 0 )
    {
        *notarized_htp = np->notarized_height;
        *MoMp = np->MoM;
        *kmdtxidp = np->notarized_desttxid;
        *MoMoMp = np->MoMoM;
        *MoMoMoffsetp = np->MoMoMoffset;
        *MoMoMdepthp = np->MoMoMdepth;
        *kmdstartip = np->kmdstarti;
        *kmdendip = np->kmdendi;
        return(np->MoMdepth);
    }
    *notarized_htp = *MoMoMoffsetp = *MoMoMdepthp = *kmdstartip = *kmdendip = 0;
    memset(MoMp,0,sizeof(*MoMp));
    memset(MoMoMp,0,sizeof(*MoMoMp));
    memset(kmdtxidp,0,sizeof(*kmdtxidp));
    return(0);
}

int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp)
{
    struct notarized_checkpoint *np = 0; int32_t i=0,flag = 0;
    if ( NUM_NPOINTS > 0 )
    {
        flag = 0;
        if ( last_NPOINTSi < NUM_NPOINTS && last_NPOINTSi > 0 )
        {
            np = &NPOINTS[last_NPOINTSi-1];
            if ( np->nHeight < nHeight )
            {
                for (i=last_NPOINTSi; i<NUM_NPOINTS; i++)
                {
                    if ( NPOINTS[i].nHeight >= nHeight )
                    {
                        //printf("flag.1 i.%d np->ht %d [%d].ht %d >= nHeight.%d, last.%d num.%d\n",i,np->nHeight,i,NPOINTS[i].nHeight,nHeight,last_NPOINTSi,NUM_NPOINTS);
                        flag = 1;
                        break;
                    }
                    np = &NPOINTS[i];
                    last_NPOINTSi = i;
                }
            }
        }
        if ( flag == 0 )
        {
            np = 0;
            for (i=0; i<NUM_NPOINTS; i++)
            {
                if ( NPOINTS[i].nHeight >= nHeight )
                {
                    //printf("i.%d np->ht %d [%d].ht %d >= nHeight.%d\n",i,np->nHeight,i,NPOINTS[i].nHeight,nHeight);
                    break;
                }
                np = &NPOINTS[i];
                last_NPOINTSi = i;
            }
        }
    }
    if ( np != 0 )
    {
        if ( np->nHeight >= nHeight || (i < NUM_NPOINTS && np[1].nHeight < nHeight) )
            fprintf(stderr,"warning: flag.%d i.%d np->ht %d [1].ht %d >= nHeight.%d\n",flag,i,np->nHeight,np[1].nHeight,nHeight);
        *notarized_hashp = np->notarized_hash;
        *notarized_desttxidp = np->notarized_desttxid;
        return(np->notarized_height);
    }
    memset(notarized_hashp,0,sizeof(*notarized_hashp));
    memset(notarized_desttxidp,0,sizeof(*notarized_desttxidp));
    return(0);
}

void komodo_notarized_update(int32_t nHeight,int32_t notarized_height,uint256 notarized_hash,uint256 notarized_desttxid,uint256 MoM,int32_t MoMdepth)
{
    static int didinit; static uint256 zero; static FILE *fp; CBlockIndex *pindex; struct notarized_checkpoint *np,N; long fpos;
    if ( didinit == 0 )
    {
        char fname[512];int32_t latestht = 0;
        //decode_hex(NOTARY_PUBKEY33,33,(char *)NOTARY_PUBKEY.c_str());
        pthread_mutex_init(&komodo_mutex,NULL);
#ifdef _WIN32
        sprintf(fname,"%s\\notarizations",gArgs.GetDataDirBase().string().c_str());
#else
        sprintf(fname,"%s/notarizations",gArgs.GetDataDirBase().string().c_str());
#endif
        printf("fname.(%s)\n",fname);
        if ( (fp= fopen(fname,"rb+")) == 0 )
            fp = fopen(fname,"wb+");
        else
        {
            fpos = 0;
            while ( fread(&N,1,sizeof(N),fp) == sizeof(N) )
            {
                //pindex = komodo_chainactive(N.notarized_height);
                //if ( pindex != 0 && pindex->GetBlockHash() == N.notarized_hash && N.notarized_height > latestht )
                if ( N.notarized_height > latestht )
                {
                    NPOINTS = (struct notarized_checkpoint *)realloc(NPOINTS,(NUM_NPOINTS+1) * sizeof(*NPOINTS));
                    np = &NPOINTS[NUM_NPOINTS++];
                    *np = N;
                    latestht = np->notarized_height;
                    NOTARIZED_HEIGHT = np->notarized_height;
                    NOTARIZED_HASH = np->notarized_hash;
                    NOTARIZED_DESTTXID = np->notarized_desttxid;
                    NOTARIZED_MOM = np->MoM;
                    NOTARIZED_MOMDEPTH = np->MoMdepth;
                    fprintf(stderr,"%d ",np->notarized_height);
                    fpos = ftell(fp);
                } //else fprintf(stderr,"%s error with notarization ht.%d %s\n",ASSETCHAINS_SYMBOL,N.notarized_height,pindex->GetBlockHash().ToString().c_str());
            }
            if ( ftell(fp) !=  fpos )
                fseek(fp,fpos,SEEK_SET);
        }
        fprintf(stderr,"finished loading %s [%s]\n",fname,NOTARY_PUBKEY.c_str());
        didinit = 1;
    }
    if ( notarized_height == 0 )
        return;
    if ( notarized_height >= nHeight )
    {
        fprintf(stderr,"komodo_notarized_update REJECT notarized_height %d > %d nHeight\n",notarized_height,nHeight);
        return;
    }
    pindex = komodo_chainactive(notarized_height);
    if ( pindex == 0 || pindex->GetBlockHash() != notarized_hash || notarized_height != pindex->nHeight )
    {
        fprintf(stderr,"komodo_notarized_update reject nHeight.%d notarized_height.%d:%d\n",nHeight,notarized_height,(int32_t)pindex->nHeight);
        return;
    }
    //fprintf(stderr,"komodo_notarized_update nHeight.%d notarized_height.%d prev.%d\n",nHeight,notarized_height,NPOINTS!=0?NPOINTS[NUM_NPOINTS-1].notarized_height:-1);
    portable_mutex_lock(&komodo_mutex);
    NPOINTS = (struct notarized_checkpoint *)realloc(NPOINTS,(NUM_NPOINTS+1) * sizeof(*NPOINTS));
    np = &NPOINTS[NUM_NPOINTS++];
    memset(np,0,sizeof(*np));
    np->nHeight = nHeight;
    NOTARIZED_HEIGHT = np->notarized_height = notarized_height;
    NOTARIZED_HASH = np->notarized_hash = notarized_hash;
    NOTARIZED_DESTTXID = np->notarized_desttxid = notarized_desttxid;
    if ( MoM != zero && MoMdepth > 0 )
    {
        NOTARIZED_MOM = np->MoM = MoM;
        NOTARIZED_MOMDEPTH = np->MoMdepth = MoMdepth;
    }
    if ( fp != 0 )
    {
        if ( fwrite(np,1,sizeof(*np),fp) == sizeof(*np) )
            fflush(fp);
        else printf("error writing notarization to %d\n",(int32_t)ftell(fp));
    }
    // add to stored notarizations
    portable_mutex_unlock(&komodo_mutex);
}

int32_t komodo_checkpoint(int32_t *notarized_heightp,int32_t nHeight,uint256 hash)
{
    int32_t notarized_height; uint256 zero,notarized_hash,notarized_desttxid; CBlockIndex *notary; CBlockIndex *pindex;
    memset(&zero,0,sizeof(zero));
    //komodo_notarized_update(0,0,zero,zero,zero,0);
    if ( (pindex= g_rpc_node->chainman->ActiveChain().Tip()) == 0 )
        return(-1);
    notarized_height = komodo_notarizeddata(pindex->nHeight,&notarized_hash,&notarized_desttxid);
    *notarized_heightp = notarized_height;
    if ( notarized_height >= 0 && notarized_height <= pindex->nHeight && (notary= g_rpc_node->chainman->m_blockman.m_block_index[notarized_hash]) != 0 )
    {
        //printf("nHeight.%d -> (%d %s)\n",pindex->nHeight,notarized_height,notarized_hash.ToString().c_str());
        if ( notary->nHeight == notarized_height ) // if notarized_hash not in chain, reorg
        {
            if ( nHeight < notarized_height )
            {
                fprintf(stderr,"nHeight.%d < NOTARIZED_HEIGHT.%d\n",nHeight,notarized_height);
                return(-1);
            }
            else if ( nHeight == notarized_height && memcmp(&hash,&notarized_hash,sizeof(hash)) != 0 )
            {
                fprintf(stderr,"nHeight.%d == NOTARIZED_HEIGHT.%d, diff hash\n",nHeight,notarized_height);
                return(-1);
            }
        } else fprintf(stderr,"unexpected error notary_hash %s ht.%d at ht.%d\n",notarized_hash.ToString().c_str(),notarized_height,notary->nHeight);
    } else if ( notarized_height > 0 )
        fprintf(stderr,"%s couldnt find notarized.(%s %d) ht.%d\n",ASSETCHAINS_SYMBOL,notarized_hash.ToString().c_str(),notarized_height,pindex->nHeight);
    return(0);
}

void komodo_voutupdate(int32_t txi,int32_t vout,uint8_t *scriptbuf,int32_t scriptlen,int32_t height,int32_t *specialtxp,int32_t *notarizedheightp,uint64_t value,int32_t notarized,uint64_t signedmask)
{
    static uint256 zero; static uint8_t crypto777[33];
    int32_t MoMdepth,opretlen,len = 0; uint256 hash,desttxid,MoM;
    if ( scriptlen == 35 && scriptbuf[0] == 33 && scriptbuf[34] == 0xac )
    {
        if ( crypto777[0] != 0x02 )// && crypto777[0] != 0x03 )
            decode_hex(crypto777,33,(char *)CRYPTO777_PUBSECPSTR);
        if ( memcmp(crypto777,scriptbuf+1,33) == 0 )
            *specialtxp = 1;
    }
    if ( scriptbuf[len++] == 0x6a )
    {
        if ( (opretlen= scriptbuf[len++]) == 0x4c )
            opretlen = scriptbuf[len++]; // len is 3 here
        else if ( opretlen == 0x4d )
        {
            opretlen = scriptbuf[len++];
            opretlen += (scriptbuf[len++] << 8);
        }
        //printf("opretlen.%d vout.%d [%s].(%s)\n",opretlen,vout,(char *)&scriptbuf[len+32*2+4],ASSETCHAINS_SYMBOL);
        if ( vout == 1 && opretlen-3 >= 32*2+4 && strcmp(ASSETCHAINS_SYMBOL,(char *)&scriptbuf[len+32*2+4]) == 0 )
        {
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&hash);
            len += iguana_rwnum(0,&scriptbuf[len],sizeof(*notarizedheightp),(uint8_t *)notarizedheightp);
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&desttxid);
            if ( notarized != 0 && *notarizedheightp > NOTARIZED_HEIGHT && *notarizedheightp < height )
            {
                int32_t nameoffset = (int32_t)strlen(ASSETCHAINS_SYMBOL) + 1;
                //NOTARIZED_HEIGHT = *notarizedheightp;
                //NOTARIZED_HASH = hash;
                //NOTARIZED_DESTTXID = desttxid;
                memset(&MoM,0,sizeof(MoM));
                MoMdepth = 0;
                len += nameoffset;
                if ( len+36-3 <= opretlen )
                {
                    len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&MoM);
                    len += iguana_rwnum(0,&scriptbuf[len],sizeof(MoMdepth),(uint8_t *)&MoMdepth);
                    if ( MoM == zero || MoMdepth > *notarizedheightp || MoMdepth < 0 )
                    {
                        memset(&MoM,0,sizeof(MoM));
                        MoMdepth = 0;
                    }
                    else
                    {
                        //fprintf(stderr,"VALID %s MoM.%s [%d]\n",ASSETCHAINS_SYMBOL,MoM.ToString().c_str(),MoMdepth);
                    }
                }
                komodo_notarized_update(height,*notarizedheightp,hash,desttxid,MoM,MoMdepth);
                fprintf(stderr,"%s ht.%d NOTARIZED.%d %s %sTXID.%s lens.(%d %d)\n",ASSETCHAINS_SYMBOL,height,*notarizedheightp,hash.ToString().c_str(),"KMD",desttxid.ToString().c_str(),opretlen,len);
            } //else fprintf(stderr,"notarized.%d ht %d vs prev %d vs height.%d\n",notarized,*notarizedheightp,NOTARIZED_HEIGHT,height);
        }
    }
}

void komodo_connectblock(CBlockIndex *pindex,CBlock& block)
{
    static int32_t hwmheight;
    uint64_t signedmask; uint8_t scriptbuf[4096],pubkeys[64][33],scriptPubKey[35]; uint256 zero; int32_t i,j,k,numnotaries,notarized,scriptlen,numvalid,specialtx,notarizedheight,len,numvouts,numvins,height,txn_count;
    if ( KOMODO_NEEDPUBKEYS != 0 )
    {
        komodo_importpubkeys();
        KOMODO_NEEDPUBKEYS = 0;
    }
    memset(&zero,0,sizeof(zero));
    komodo_notarized_update(0,0,zero,zero,zero,0);
    numnotaries = komodo_notaries(pubkeys,pindex->nHeight,pindex->GetBlockTime());
    if ( pindex->nHeight > hwmheight )
        hwmheight = pindex->nHeight;
    else
    {
        if ( pindex->nHeight != hwmheight )
            printf("%s hwmheight.%d vs pindex->nHeight.%d t.%u reorg.%d\n",ASSETCHAINS_SYMBOL,hwmheight,pindex->nHeight,(uint32_t)pindex->nTime,hwmheight-pindex->nHeight);
    }
    if ( pindex != 0 )
    {
        height = pindex->nHeight;
        txn_count = block.vtx.size();
        for (i=0; i<txn_count; i++)
        {
            //txhash = block.vtx[i]->GetHash();
            numvouts = block.vtx[i]->vout.size();
            specialtx = notarizedheight = notarized = 0;
            signedmask = 0;
            numvins = block.vtx[i]->vin.size();
            for (j=0; j<numvins; j++)
            {
                if ( i == 0 && j == 0 )
                    continue;
                if ( block.vtx[i]->vin[j].prevout.hash != zero && (scriptlen= gettxout_scriptPubKey(height,scriptPubKey,sizeof(scriptPubKey),block.vtx[i]->vin[j].prevout.hash,block.vtx[i]->vin[j].prevout.n)) == 35 )
                {
                    for (k=0; k<numnotaries; k++)
                        if ( memcmp(&scriptPubKey[1],pubkeys[k],33) == 0 )
                        {
                            signedmask |= (1LL << k);
                            break;
                        }
                } // else if ( block.vtx[i]->vin[j].prevout.hash != zero ) printf("%s cant get scriptPubKey for ht.%d txi.%d vin.%d\n",ASSETCHAINS_SYMBOL,height,i,j);
            }
            numvalid = bitweight(signedmask);
            if ( numvalid >= KOMODO_MINRATIFY )
                notarized = 1;
            //if ( NOTARY_PUBKEY33[0] != 0 )
            //    printf("(tx.%d: ",i);
            for (j=0; j<numvouts; j++)
            {
                //if ( NOTARY_PUBKEY33[0] != 0 )
                //    printf("%.8f ",dstr(block.vtx[i]->vout[j].nValue));
                len = block.vtx[i]->vout[j].scriptPubKey.size();
                if ( len >= (int32_t)sizeof(uint32_t) && len <= (int32_t)sizeof(scriptbuf) )
                {
                    memcpy(scriptbuf,block.vtx[i]->vout[j].scriptPubKey.data(),len);
                    komodo_voutupdate(i,j,scriptbuf,len,height,&specialtx,&notarizedheight,(uint64_t)block.vtx[i]->vout[j].nValue,notarized,signedmask);
                }
            }
            //if ( NOTARY_PUBKEY33[0] != 0 )
            //    printf(") ");
            //if ( NOTARY_PUBKEY33[0] != 0 )
            //    printf("%s ht.%d\n",ASSETCHAINS_SYMBOL,height);
            //printf("[%s] ht.%d txi.%d signedmask.%llx numvins.%d numvouts.%d notarized.%d special.%d\n",ASSETCHAINS_SYMBOL,height,i,(long long)signedmask,numvins,numvouts,notarized,specialtx);
        }
    } else fprintf(stderr,"komodo_connectblock: unexpected null pindex\n");
}
