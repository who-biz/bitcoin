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
// at end of ConnectTip: komodo_connectblock(pindexNew,*(CBlock *)&blockconnecting);
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
#define KOMODO_MINRATIFY 11
#define KOMODO_ELECTION_GAP 2000
#define KOMODO_ASSETCHAIN_MAXLEN 65

// KMD Notary Seasons 
// 1: ENDS: May 1st 2018 1530921600
// 2: ENDS: July 15th 2019 1563148800 -> estimated height 4,173,578
// 3: 3rd season ending isnt known, so use very far times in future.
// to add 4th season, change NUM_KMD_SEASONS to 4, add height of activation for third season ending one spot before 999999999.
#define NUM_KMD_SEASONS 5
#define NUM_KMD_NOTARIES 64
// first season had no third party coins, so it ends at block 0. 
// second season ends at approx block 4,173,578, please check this!!!!! it should be as close as possible to July 15th 0:00 UTC. 
// third season ending height is unknown so it set to very very far in future. 
// Fourth season is ending at the timestamp 1623682800, and the approximate CHIPS block height of that time is of 8207456.
static const int32_t KMD_SEASON_HEIGHTS[NUM_KMD_SEASONS] = {0, 4173578, 6415000, 8207456, 999999999};

// Era array of pubkeys. Add extra seasons to bottom as requried, after adding appropriate info above. 
static const char *notaries_elected[NUM_KMD_SEASONS][NUM_KMD_NOTARIES][2] =
{
    {
        { "0_jl777_testA", "03b7621b44118017a16043f19b30cc8a4cfe068ac4e42417bae16ba460c80f3828" },
        { "0_jl777_testB", "02ebfc784a4ba768aad88d44d1045d240d47b26e248cafaf1c5169a42d7a61d344" },
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
    },
    {
        {"0dev1_jl777", "03b7621b44118017a16043f19b30cc8a4cfe068ac4e42417bae16ba460c80f3828" },
        {"0dev2_kolo", "030f34af4b908fb8eb2099accb56b8d157d49f6cfb691baa80fdd34f385efed961" },
        {"0dev3_kolo", "025af9d2b2a05338478159e9ac84543968fd18c45fd9307866b56f33898653b014" },
        {"0dev4_decker", "028eea44a09674dda00d88ffd199a09c9b75ba9782382cc8f1e97c0fd565fe5707" },
        {"a-team_SH", "03b59ad322b17cb94080dc8e6dc10a0a865de6d47c16fb5b1a0b5f77f9507f3cce" },
        {"artik_AR", "029acf1dcd9f5ff9c455f8bb717d4ae0c703e089d16cf8424619c491dff5994c90" },
        {"artik_EU", "03f54b2c24f82632e3cdebe4568ba0acf487a80f8a89779173cdb78f74514847ce" },
        {"artik_NA", "0224e31f93eff0cc30eaf0b2389fbc591085c0e122c4d11862c1729d090106c842" },
        {"artik_SH", "02bdd8840a34486f38305f311c0e2ae73e84046f6e9c3dd3571e32e58339d20937" },
        {"badass_EU", "0209d48554768dd8dada988b98aca23405057ac4b5b46838a9378b95c3e79b9b9e" },
        {"badass_NA", "02afa1a9f948e1634a29dc718d218e9d150c531cfa852843a1643a02184a63c1a7" }, // 10
        {"batman_AR", "033ecb640ec5852f42be24c3bf33ca123fb32ced134bed6aa2ba249cf31b0f2563" },
        {"batman_SH", "02ca5898931181d0b8aafc75ef56fce9c43656c0b6c9f64306e7c8542f6207018c" },
        {"ca333_EU", "03fc87b8c804f12a6bd18efd43b0ba2828e4e38834f6b44c0bfee19f966a12ba99" },
        {"chainmakers_EU", "02f3b08938a7f8d2609d567aebc4989eeded6e2e880c058fdf092c5da82c3bc5ee" },
        {"chainmakers_NA", "0276c6d1c65abc64c8559710b8aff4b9e33787072d3dda4ec9a47b30da0725f57a" },
        {"chainstrike_SH", "0370bcf10575d8fb0291afad7bf3a76929734f888228bc49e35c5c49b336002153" },
        {"cipi_AR", "02c4f89a5b382750836cb787880d30e23502265054e1c327a5bfce67116d757ce8" },
        {"cipi_NA", "02858904a2a1a0b44df4c937b65ee1f5b66186ab87a751858cf270dee1d5031f18" },
        {"crackers_EU", "03bc819982d3c6feb801ec3b720425b017d9b6ee9a40746b84422cbbf929dc73c3" },
        {"crackers_NA", "03205049103113d48c7c7af811b4c8f194dafc43a50d5313e61a22900fc1805b45" }, // 20
        {"dwy_EU", "0259c646288580221fdf0e92dbeecaee214504fdc8bbdf4a3019d6ec18b7540424" },
        {"emmanux_SH", "033f316114d950497fc1d9348f03770cd420f14f662ab2db6172df44c389a2667a" },
        {"etszombi_EU", "0281b1ad28d238a2b217e0af123ce020b79e91b9b10ad65a7917216eda6fe64bf7" },
        {"fullmoon_AR", "03380314c4f42fa854df8c471618751879f9e8f0ff5dbabda2bd77d0f96cb35676" },
        {"fullmoon_NA", "030216211d8e2a48bae9e5d7eb3a42ca2b7aae8770979a791f883869aea2fa6eef" },
        {"fullmoon_SH", "03f34282fa57ecc7aba8afaf66c30099b5601e98dcbfd0d8a58c86c20d8b692c64" },
        {"goldenman_EU", "02d6f13a8f745921cdb811e32237bb98950af1a5952be7b3d429abd9152f8e388d" },
        {"indenodes_AR", "02ec0fa5a40f47fd4a38ea5c89e375ad0b6ddf4807c99733c9c3dc15fb978ee147" },
        {"indenodes_EU", "0221387ff95c44cb52b86552e3ec118a3c311ca65b75bf807c6c07eaeb1be8303c" },
        {"indenodes_NA", "02698c6f1c9e43b66e82dbb163e8df0e5a2f62f3a7a882ca387d82f86e0b3fa988" }, // 30
        {"indenodes_SH", "0334e6e1ec8285c4b85bd6dae67e17d67d1f20e7328efad17ce6fd24ae97cdd65e" },
        {"jackson_AR", "038ff7cfe34cb13b524e0941d5cf710beca2ffb7e05ddf15ced7d4f14fbb0a6f69" },
        {"jeezy_EU", "023cb3e593fb85c5659688528e9a4f1c4c7f19206edc7e517d20f794ba686fd6d6" },
        {"karasugoi_NA", "02a348b03b9c1a8eac1b56f85c402b041c9bce918833f2ea16d13452309052a982" },
        {"komodoninja_EU", "038e567b99806b200b267b27bbca2abf6a3e8576406df5f872e3b38d30843cd5ba" },
        {"komodoninja_SH", "033178586896915e8456ebf407b1915351a617f46984001790f0cce3d6f3ada5c2" },
        {"komodopioneers_SH", "033ace50aedf8df70035b962a805431363a61cc4e69d99d90726a2d48fb195f68c" },
        {"libscott_SH", "03301a8248d41bc5dc926088a8cf31b65e2daf49eed7eb26af4fb03aae19682b95" },
        {"lukechilds_AR", "031aa66313ee024bbee8c17915cf7d105656d0ace5b4a43a3ab5eae1e14ec02696" },
        {"madmax_AR", "03891555b4a4393d655bf76f0ad0fb74e5159a615b6925907678edc2aac5e06a75" }, // 40
        {"meshbits_AR", "02957fd48ae6cb361b8a28cdb1b8ccf5067ff68eb1f90cba7df5f7934ed8eb4b2c" },
        {"meshbits_SH", "025c6e94877515dfd7b05682b9cc2fe4a49e076efe291e54fcec3add78183c1edb" },
        {"metaphilibert_AR", "02adad675fae12b25fdd0f57250b0caf7f795c43f346153a31fe3e72e7db1d6ac6" },
        {"metaphilibert_SH", "0284af1a5ef01503e6316a2ca4abf8423a794e9fc17ac6846f042b6f4adedc3309" },
        {"patchkez_SH", "0296270f394140640f8fa15684fc11255371abb6b9f253416ea2734e34607799c4" },
        {"pbca26_NA", "0276aca53a058556c485bbb60bdc54b600efe402a8b97f0341a7c04803ce204cb5" },
        {"peer2cloud_AR", "034e5563cb885999ae1530bd66fab728e580016629e8377579493b386bf6cebb15" },
        {"peer2cloud_SH", "03396ac453b3f23e20f30d4793c5b8ab6ded6993242df4f09fd91eb9a4f8aede84" },
        {"polycryptoblog_NA", "02708dcda7c45fb54b78469673c2587bfdd126e381654819c4c23df0e00b679622" },
        {"hyper_AR", "020f2f984d522051bd5247b61b080b4374a7ab389d959408313e8062acad3266b4" }, // 50
        {"hyper_EU", "03d00cf9ceace209c59fb013e112a786ad583d7de5ca45b1e0df3b4023bb14bf51" },
        {"hyper_SH", "0383d0b37f59f4ee5e3e98a47e461c861d49d0d90c80e9e16f7e63686a2dc071f3" },
        {"hyper_NA", "03d91c43230336c0d4b769c9c940145a8c53168bf62e34d1bccd7f6cfc7e5592de" },
        {"popcornbag_AR", "02761f106fb34fbfc5ddcc0c0aa831ed98e462a908550b280a1f7bd32c060c6fa3" },
        {"popcornbag_NA", "03c6085c7fdfff70988fda9b197371f1caf8397f1729a844790e421ee07b3a93e8" },
        {"alien_AR", "0348d9b1fc6acf81290405580f525ee49b4749ed4637b51a28b18caa26543b20f0" },
        {"alien_EU", "020aab8308d4df375a846a9e3b1c7e99597b90497efa021d50bcf1bbba23246527" },
        {"thegaltmines_NA", "031bea28bec98b6380958a493a703ddc3353d7b05eb452109a773eefd15a32e421" },
        {"titomane_AR", "029d19215440d8cb9cc6c6b7a4744ae7fb9fb18d986e371b06aeb34b64845f9325" },
        {"titomane_EU", "0360b4805d885ff596f94312eed3e4e17cb56aa8077c6dd78d905f8de89da9499f" }, // 60
        {"titomane_SH", "03573713c5b20c1e682a2e8c0f8437625b3530f278e705af9b6614de29277a435b" },
        {"webworker01_NA", "03bb7d005e052779b1586f071834c5facbb83470094cff5112f0072b64989f97d7" },
        {"xrobesx_NA", "03f0cc6d142d14a40937f12dbd99dbd9021328f45759e26f1877f2a838876709e1" },
    },
    {
        {"madmax_NA", "02ef81a360411adf71184ff04d0c5793fc41fd1d7155a28dd909f21f35f4883ac1" },
        {"alright_AR", "036a6bca1c2a8166f79fa8a979662892742346cc972b432f8e61950a358d705517" },
        {"strob_NA", "02049202f3872877e81035549f6f3a0f868d0ad1c9b0e0d2b48b1f30324255d26d" },
        {"dwy_EU", "037b29e58166f7a2122a9ebfff273b40805b6d710adc032f1f8cf077bdbe7c0d5c" },
        {"phm87_SH", "03889a10f9df2caef57220628515693cf25316fe1b0693b0241419e75d0d0e66ed" },
        {"chainmakers_NA", "030e4822bddba10eb50d52d7da13106486651e4436962078ee8d681bc13f4993e9" },
        {"indenodes_EU", "03a416533cace0814455a1bb1cd7861ce825a543c6f6284a432c4c8d8875b7ace9" },
        {"blackjok3r_SH", "03d23bb5aad3c20414078472220cc5c26bc5aeb41e43d72c99158d450f714d743a" },
        {"chainmakers_EU", "034f8c0a504856fb3d80a94c3aa78828c1152daf8ccc45a17c450f32a1e242bb0c" },
        {"titomane_AR", "0358cd6d7460654a0ddd5125dd6fa0402d0719999444c6cc3888689a0b4446136a" },
        {"fullmoon_SH", "0275031fa79846c5d667b1f7c4219c487d439cd367dd294f73b5ecd55b4e673254" },
        {"indenodes_NA", "02b3908eda4078f0e9b6704451cdc24d418e899c0f515fab338d7494da6f0a647b" },
        {"chmex_EU", "03e5b7ab96b7271ecd585d6f22807fa87da374210a843ec3a90134cbf4a62c3db1" },
        {"metaphilibert_SH", "03b21ff042bf1730b28bde43f44c064578b41996117ac7634b567c3773089e3be3" },
        {"ca333_DEV", "029c0342ce2a4f9146c7d1ee012b26f5c2df78b507fb4461bf48df71b4e3031b56" },
        {"cipi_NA", "034406ac4cf94e84561c5d3f25384dd59145e92fefc5972a037dc6a44bfa286688" },
        {"pungocloud_SH", "0203064e291045187927cc35ed350e046bba604e324bb0e3b214ea83c74c4713b1" },
        {"voskcoin_EU", "037bfd946f1dd3736ddd2cb1a0731f8b83de51be5d1be417496fbc419e203bc1fe" },
        {"decker_DEV", "02fca8ee50e49f480de275745618db7b0b3680b0bdcce7dcae7d2e0fd5c3345744" },
        {"cryptoeconomy_EU", "037d04b7d16de61a44a3fc766bea4b7791023a36675d6cee862fe53defd04dd8f2" },
        {"etszombi_EU", "02f65da26061d1b9f1756a274918a37e83086dbfe9a43d2f0b35b9d2f593b31907" },
        {"karasugoi_NA", "024ba10f7f5325fd6ec6cab50c5242d142d00fab3537c0002097c0e98f72014177" },
        {"pirate_AR", "0353e2747f89968741c24f254caec24f9f49a894a0039ee9ba09234fcbad75c77d" },
        {"metaphilibert_AR", "0239e34ad22957bbf4c8df824401f237b2afe8d40f7a645ecd43e8f27dde1ab0da" },
        {"zatjum_SH", "03643c3b0a07a34f6ae7b048832e0216b935408dfc48b0c7d3d4faceb38841f3f3" },
        {"madmax_AR", "038735b4f6881925e5a9b14275af80fa2b712c8bd57eef26b97e5b153218890e38" },
        {"lukechilds_NA", "024607d85ea648511fa50b13f29d16beb2c3a248c586b449707d7be50a6060cf50" },
        {"cipi_AR", "025b7655826f5fd3a807cbb4918ef9f02fe64661153ca170db981e9b0861f8c5ad" },
        {"tonyl_AR", "03a8db38075c80348889871b4318b0a79a1fd7e9e21daefb4ca6e4f05e5963569c" },
        {"infotech_DEV", "0399ff59b0244103486a94acf1e4a928235cb002b20e26a6f3163b4a0d5e62db91" },
        {"fullmoon_NA", "02adf6e3cb8a3c94d769102aec9faf2cb073b7f2979ce64efb1161a596a8d16312" },
        {"etszombi_AR", "03c786702b81e0122157739c8e2377cf945998d36c0d187ec5c5ff95855848dfdd" },
        {"node-9_EU", "024f2402daddee0c8169ccd643e5536c2cf95b9690391c370a65c9dd0169fc3dc6" },
        {"phba2061_EU", "02dc98f064e3bf26a251a269893b280323c83f1a4d4e6ccd5e84986cc3244cb7c9" },
        {"indenodes_AR", "0242778789986d614f75bcf629081651b851a12ab1cc10c73995b27b90febb75a2" },
        {"and1-89_EU", "029f5a4c6046de880cc95eb448d20c80918339daff7d71b73dd3921895559d7ca3" },
        {"komodopioneers_SH", "02ae196a1e93444b9fcac2b0ccee428a4d9232a00b3a508484b5bccaedc9bac55e" },
        {"komodopioneers_EU", "03c7fef345ca6b5326de9d5a38498638801eee81bfea4ca8ffc3dacac43c27b14d" },
        {"d0ct0r_NA", "0235b211469d7c1881d30ab647e0d6ddb4daf9466f60e85e6a33a92e39dedde3a7" },
        {"kolo_DEV", "03dc7c71a5ef7104f81e62928446c4216d6e9f68d944c893a21d7a0eba74b1cb7c" },
        {"peer2cloud_AR", "0351c784d966dbb79e1bab4fad7c096c1637c98304854dcdb7d5b5aeceb94919b4" },
        {"webworker01_SH", "0221365d89a6f6373b15daa4a50d56d34ad1b4b8a48a7fd090163b6b5a5ecd7a0a" },
        {"webworker01_NA", "03bfc36a60194b495c075b89995f307bec68c1bcbe381b6b29db47af23494430f9" },
        {"pbca26_NA", "038319dcf74916486dbd506ac866d184c17c3202105df68c8335a1a1079ef0dfcc" },
        {"indenodes_SH", "031d1584cf0eb4a2d314465e49e2677226b1615c3718013b8d6b4854c15676a58c" },
        {"pirate_NA", "034899e6e884b3cb1f491d296673ab22a6590d9f62020bea83d721f5c68b9d7aa7" },
        {"lukechilds_AR", "031ee242e67a8166e489c0c4ed1e5f7fa32dff19b4c1749de35f8da18befa20811" },
        {"dragonhound_NA", "022405dbc2ea320131e9f0c4115442c797bf0f2677860d46679ac4522300ce8c0a" },
        {"fullmoon_AR", "03cd152ae20adcc389e77acad25953fc2371961631b35dc92cf5c96c7729c2e8d9" },
        {"chainzilla_SH", "03fe36ff13cb224217898682ce8b87ba6e3cdd4a98941bb7060c04508b57a6b014" },
        {"titomane_EU", "03d691cd0914a711f651082e2b7b27bee778c1309a38840e40a6cf650682d17bb5" },
        {"jeezy_EU", "022bca828b572cb2b3daff713ed2eb0bbc7378df20f799191eebecf3ef319509cd" },
        {"titomane_SH", "038c2a64f7647633c0e74eec93f9a668d4bf80214a43ed7cd08e4e30d3f2f7acfb" },
        {"alien_AR", "024f20c096b085308e21893383f44b4faf1cdedea9ad53cc7d7e7fbfa0c30c1e71" },
        {"pirate_EU", "0371f348b4ac848cdfb732758f59b9fdd64285e7adf769198771e8e203638db7e6" },
        {"thegaltmines_NA", "03e1d4cec2be4c11e368ff0c11e80cd1b09da8026db971b643daee100056b110fa" },
        {"computergenie_NA", "02f945d87b7cd6e9f2173a110399d36b369edb1f10bdf5a4ba6fd4923e2986e137" },
        {"nutellalicka_SH", "035ec5b9e88734e5bd0f3bd6533e52f917d51a0e31f83b2297aabb75f9798d01ef" },
        {"chainstrike_SH", "0221f9dee04b7da1f3833c6ea7f7325652c951b1c239052b0dadb57209084ca6a8" },
        {"dwy_SH", "02c593f32643f1d9af5c03eddf3e67d085b9173d9bc746443afe0abff9e5dd72f4" },
        {"alien_EU", "022b85908191788f409506ebcf96a892f3274f352864c3ed566c5a16de63953236" },
        {"gt_AR", "0307c1cf89bd8ed4db1b09a0a98cf5f746fc77df3803ecc8611cf9455ec0ce6960" },
        {"patchkez_SH", "03d7c187689bf829ca076a30bbf36d2e67bb74e16a3290d8a55df21d6cb15c80c1" },
        {"decker_AR", "02a85540db8d41c7e60bf0d33d1364b4151cad883dd032878ea4c037f67b769635" },
    },
  {
   {"alien_AR", "024f20c096b085308e21893383f44b4faf1cdedea9ad53cc7d7e7fbfa0c30c1e71" },
        {"alien_EU", "022b85908191788f409506ebcf96a892f3274f352864c3ed566c5a16de63953236" },
        {"strob_NA", "02285bf2f9e96068ecac14bc6f770e394927b4da9f5ba833eaa9468b5d47f203a3" },
        {"titomane_SH", "02abf206bafc8048dbdc042b8eb6b1e356ea5dbe149eae3532b4811d4905e5cf01" },
        {"fullmoon_AR", "03639bc56d3fecf856f17759a441c5893668e7c2d460f3d216798a413cd6766bb2" },
        {"phba2061_EU", "03369187ce134bd7793ee34af7756fe1ab27202e09306491cdd5d8ad2c71697937" },
        {"fullmoon_NA", "03e388bcc579ac2675f8fadfa921eec186dcea8d2b43de1eed6caba23d5a962b74" },
        {"fullmoon_SH", "03a5cfda2b097c808834ccdd805828c811b519611feabdfe6b3644312e53f6748f" },
        {"madmax_AR", "027afddbcf690230dd8d435ec16a7bfb0083e6b77030f763437f291dfc40a579d0" },
        {"titomane_EU", "02276090e483db1a01a802456b10831b3b6e0a6ad3ece9b2a01f4aad0e480c8edc" },
        {"cipi_NA", "03f4e69edcb4fa3b2095cb8cb1ca010f4ec4972eac5d8822397e5c8d87aa21a739" },
        {"indenodes_SH", "031d1584cf0eb4a2d314465e49e2677226b1615c3718013b8d6b4854c15676a58c" },
        {"decker_AR", "02a85540db8d41c7e60bf0d33d1364b4151cad883dd032878ea4c037f67b769635" },
        {"indenodes_EU", "03a416533cace0814455a1bb1cd7861ce825a543c6f6284a432c4c8d8875b7ace9" },
        {"madmax_NA", "036d3afebe1eab09f4c38c3ee6a4659ad390f3df92787c11437a58c59a29e408e6" },
        {"chainzilla_SH", "0311dde03c2dd654ce78323b718ed3ad73a464d1bde97820f3395f54788b5420dd" },
        {"peer2cloud_AR", "0243958faf9ae4d43b598b859ddc595c170c4cf50f8e4517d660ae5bc72aeb821b" },
        {"pirate_EU", "0240011b95cde819f298fe0f507b2260c9fecdab784924076d4d1e54c522103cb1" },
        {"webworker01_NA", "02de90c720c007229374772505a43917a84ed129d5fbcfa4949cc2e9b563351124" },
        {"zatjum_SH", "0241c5660ca540780be66603b1791127a1261d56abbcb7562c297eec8e4fc078fb" },
        {"titomane_AR", "03958bd8d13fe6946b8d0d0fbbc3861c72542560d0276e80a4c6b5fe55bc758b81" },
        {"chmex_EU", "030bf7bd7ad0515c33b5d5d9a91e0729baf801b9002f80495ae535ea1cebb352cb" },
        {"indenodes_NA", "02b3908eda4078f0e9b6704451cdc24d418e899c0f515fab338d7494da6f0a647b" },
        {"patchkez_SH", "028c08db6e7242681f50db6c234fe3d6e12fb1a915350311be26373bac0d457d49" },
        {"metaphilibert_AR", "0239e34ad22957bbf4c8df824401f237b2afe8d40f7a645ecd43e8f27dde1ab0da" },
        {"etszombi_EU", "03a5c083c78ba397970f20b544a01c13e7ed36ca8a5ae26d5fe7bd38b92b6a0c94" },
        {"pirate_NA", "02ad7ef25d2dd461e361120cd3efe7cbce5e9512c361e9185aac33dd303d758613" },
        {"metaphilibert_SH", "03b21ff042bf1730b28bde43f44c064578b41996117ac7634b567c3773089e3be3" },
        {"indenodes_AR", "0242778789986d614f75bcf629081651b851a12ab1cc10c73995b27b90febb75a2" },
        {"chainmakers_NA", "028803e07bcc521fde264b7191a944f9b3612e8ee4e24a99bcd903f6976240839a" },
        {"mihailo_EU", "036494e7c9467c8c7ff3bf29e841907fb0fa24241866569944ea422479ec0e6252" },
        {"tonyl_AR", "0229e499e3f2e065ced402ceb8aaf3d5ab8bd3793aa074305e9fa30772ce604908" },
        {"alien_NA", "022f62b56ddfd07c9860921c701285ac39bb3ac8f6f083d1b59c8f4943be3de162" },
        {"pungocloud_SH", "02641c36ae6747b88150a463a1fe65cf7a9d1c00a64387c73f296f0b64e77c7d3f" },
        {"node9_EU", "0392e4c9400e69f28c6b9e89d586da69d5a6af7702f1045eaa6ebc1996f0496e1f" },
        {"smdmitry_AR", "0397b7584cb29717b721c0c587d4462477efc1f36a56921f133c9d17b0cd7f278a" },
        {"nodeone_NA", "0310a249c6c2dcc29f2135715138a9ddb8e01c0eab701cbd0b96d9cec660dbdc58" },
        {"gcharang_SH", "02a654037d12cdd609f4fad48e15ec54538e03f61fdae1acb855f16ebacac6bd73" },
        {"cipi_EU", "026f4f66385daaf8313ef30ffe4988e7db497132682dca185a70763d93e1417d9d" },
        {"etszombi_AR", "03bfcbca83f11e622fa4eed9a1fa25dba377981ea3b22e3d0a4015f9a932af9272" },
        {"pbca26_NA", "03c18431bb6bc95672f640f19998a196becd2851d5dcba4795fe8d85b7d77eab81" },
        {"mylo_SH", "026d5f29d09ff3f33e14db4811606249b2438c6bcf964876714f81d1f2d952acde" },
        {"swisscertifiers_EU", "02e7722ebba9f8b5ebfb4e87d4fa58cc75aef677535b9cfc060c7d9471aacd9c9e" },
        {"marmarachain_AR", "028690ca1e3afdf8a38b421f6a41f5ff407afc96d5a7a6a488330aae26c8b086bb" },
        {"karasugoi_NA", "02f803e6f159824a181cc5d709f3d1e7ff65f19e1899920724aeb4e3d2d869f911" },
        {"phm87_SH", "03889a10f9df2caef57220628515693cf25316fe1b0693b0241419e75d0d0e66ed" },
        {"oszy_EU", "03c53bd421de4a29ce68c8cc83f802e1181e77c08f8f16684490d61452ea8d023a" },
        {"chmex_AR", "030cd487e10fbf142e0e8d582e702ecb775f378569c3cb5acd0ff97b6b12803588" },
        {"dragonhound_NA", "029912212d370ee0fb4d38eefd8bfcd8ab04e2c3b0354020789c29ddf2a35c72d6" },
        {"strob_SH", "0213751a1c59d3489ca85b3d62a3d606dcef7f0428aa021c1978ea16fb38a2fad6" },
        {"madmax_EU", "0397ec3a4ad84b3009566d260c89f1c4404e86e5d044964747c9371277e38f5995" },
        {"dudezmobi_AR", "033c121d3f8d450174674a73f3b7f140b2717a7d51ea19ee597e2e8e8f9d5ed87f" },
        {"daemonfox_NA", "023c7584b1006d4a62a4b4c9c1ede390a3789316547897d5ed49ff9385a3acb411" },
        {"nutellalicka_SH", "0284c4d3cb97dd8a32d10fb32b1855ae18cf845dad542e3b8937ca0e998fb54ecc" },
        {"starfleet_EU", "03c6e047218f34644ccba67e317b9da5d28e68bbbb6b9973aef1281d2bafa46496" },
        {"mrlynch_AR", "03e67440141f53a08684c329ebc852b018e41f905da88e52aa4a6dc5aa4b12447a" },
        {"greer_NA", "0262da6aaa0b295b8e2f120035924758a4a630f899316dc63ee15ef03e9b7b2b23" },
        {"mcrypt_SH", "027a4ca7b11d3456ff558c08bb04483a89c7f383448461fd0b6b3b07424aabe9a4" },
        {"decker_EU", "027777775b89ff548c3be54fb0c9455437d87f38bfce83bdef113899881b219c9e" },
        {"dappvader_SH", "025199bc04bcb8a17976d9fe8bc87763a6150c2727321aa59bf34a2b49f2f3a0ce" },
        {"alright_DEV", "03b6f9493658bdd102503585a08ae642b49d6a68fb69ac3626f9737cd7581abdfa" },
        {"artemii235_DEV", "037a20916d2e9ea575300ac9d729507c23a606b9a200c8e913d7c9832f912a1fa7" },
        {"tonyl_DEV", "0258b77d7dcfc6c2628b0b6b438951a6e74201fb2cd180a795e4c37fcf8e78a678" },
        {"decker_DEV", "02fca8ee50e49f480de275745618db7b0b3680b0bdcce7dcae7d2e0fd5c3345744" }
   },
   {    
        // Season 5
	{"alrighttt_DEV", "02a876c6c35060041f6beadb201f4dfc567e80eedd3a4206ff10d99878087bd440"}, // 0
	{"alien_AR", "024f20c096b085308e21893383f44b4faf1cdedea9ad53cc7d7e7fbfa0c30c1e71"},
	{"artempikulin_AR", "03a45c4ad7f279cbc50acb48d81fc0eb63c4c5f556e3a4393fb3d6414df09c6e4c"},
	{"chmex_AR", "030cd487e10fbf142e0e8d582e702ecb775f378569c3cb5acd0ff97b6b12803588"},
	{"cipi_AR", "02336758998f474659020e6887ece61ac7b8567f9b2d38724ebf77ae800c1fb2b7"},
	{"shadowbit_AR", "03949b06c2773b4573aeb0b52e70ccc2d98dc5794a47e24eeb902c9d28e0e8d28b"},
	{"goldenman_AR", "03d745bc6921104b73734e6d9615671bc70b9e11e26c9b0c9abf0d2f9babd01a4d"},
	{"kolo_AR", "027579d0722b2f75b3d11a73829449e4251b4471716b6cb743c7667379750c8fb0"},
	{"madmax_AR", "02ddb23f18e61ea792ae0f28be5a52859e7963bf7f1d2c4f19eec18ac6497cfa2a"},
	{"mcrypt_AR", "02845d016c68c3e5ce924b164abc271511f3092ae359677a515e8f81a9533472f4"},
	{"mrlynch_AR", "03e67440141f53a08684c329ebc852b018e41f905da88e52aa4a6dc5aa4b12447a"}, // 10
	{"ocean_AR", "02d216e72d37a38449d661413cbc6e1f008b21cffdb06865f7be636e2cbc1e5346"},
	{"smdmitry_AR", "0397b7584cb29717b721c0c587d4462477efc1f36a56921f133c9d17b0cd7f278a"},
	{"tokel_AR", "02e4e07060fcd3640a3fd6d54cc15924f2bf63f8172b96a9f1d538ca7a0e490dc5"},
	{"tonyl_AR", "02e2d9ecdc9f462a4767f7dfe8ed243c98fcccc1511989a60e3f859dc6fda42d16"},
	{"tonyl_DEV", "0399c4f8c5b604cda64c1ccb8fdbd7a89730131519f87491a79b0811e619102d8f"},
	{"artem_DEV", "025ee88d1c12f546c1c8942d7a3e0678f10bc27cc566e27bf4a2d2178e018d18c6"},
	{"alien_EU", "022b85908191788f409506ebcf96a892f3274f352864c3ed566c5a16de63953236"},
	{"alienx_EU", "025de0911bab55616c307f02ea8a5915a2e0c8e479aa97968e7f00d1025cbe6c6d"},
	{"ca333_EU", "03a582cfae3760bb1cb38311d686cfeede8f8c4ce263aa1c082fc836c367859122"},
	{"chmex_EU", "030bf7bd7ad0515c33b5d5d9a91e0729baf801b9002f80495ae535ea1cebb352cb"}, // 20
	{"cipi_EU", "033a812d6cccdc4208378728f3a0e15db5b12074def9ab686ddc3752715ff1a194"},
	{"cipi2_EU", "0302ca28a041ed00544de737651bdec9bafe3b7f1c0bf2c6092f2368d59fec75c2"},
	{"shadowbit_EU", "025f8de3a6181270ceb5c31654e6a6e95d0339bc14b46b5e3050e8a69861c91baa"},
	{"komodopioneers_EU", "02fb31b130babe79ac780a6118702555a8c66875835f35c2232a6cb8b1438fe71d"},
	{"madmax_EU", "02e7e5306f159df252ecfded9bab6297050d12640b908b456ea553f90872f8a160"},
	{"marmarachain_EU", "027029380f49b0c3cc1b814976f1a83f0c25d84020ad0a27454e55ebdb2ccc83d7"},
	{"node-9_EU", "029401e427cffa29bb2bd7664110e160d525fac6f1518ac7b59343b16de301e0ac"},
	{"slyris_EU", "02a0705ec221a94a6a5b3ea2e763ba0350f8213c73e8dad49a708fb1e87acdc5f8"},
	{"smdmitry_EU", "0338f30ca34d0aca0d79b69abde447036aaaa75f482b6c75801fd382e984337d01"},
	{"van_EU", "0370305b9e91d46331da202ae733d6050d01038ef6eceb2036ada394a48fae84b9"}, // 30
	{"shadowbit_DEV", "03e2de3418c88be0cfe2fa0dcfdaea001b5a36ad86e6833ad284d79021ae7e2b94"},
	{"gcharang_DEV", "0321868e0eb39271330fa2c3a9f4e542275d9719f8b87773c5432448ab10d6943d"},
	{"alien_NA", "022f62b56ddfd07c9860921c701285ac39bb3ac8f6f083d1b59c8f4943be3de162"},
	{"alienx_NA", "025d5e11725233ab161f4f63d697c5f9f0c6b9d3aa2b9c68299638f8cc63faa9c2"},
	{"cipi_NA", "0335352862da521bd90b99d394db1ee3ecde379db9cf7ba2f28b16fa76153e289f"},
	{"computergenie_NA", "02f945d87b7cd6e9f2173a110399d36b369edb1f10bdf5a4ba6fd4923e2986e137"},
	{"dragonhound_NA", "0366a87a476a09e05560c5aae0e44d2ab9ba56e69701cee24307871ddd37c86258"},
	{"hyper_NA", "0303503ea8f5ec8bcab474962dfadd3561b44732b6ad308acd8d04276dd2f1baf3"},
	{"madmax_NA", "0378e47061572e4a406bbad1522c03c3331d0a6c820fde1248ccf2cbc72fec47c2"},
	{"node-9_NA", "03fac1468a949244dd4c563062459d46e966479fe23748382fc2e3e8d05218023e"}, // 40
	{"nodeone_NA", "0310a249c6c2dcc29f2135715138a9ddb8e01c0eab701cbd0b96d9cec660dbdc58"},
	{"pbca26_NA", "03e8485883eba6d4f2902338ab6aac87654a4b98d3bc01f89638aaf9c37db66ccf"},
	{"ptyx_NA", "028267c92db3c48a99dfb8d88e9cdab60d8a1525913ab3978b1b629667b12b1ee2"},
	{"strob_NA", "02285bf2f9e96068ecac14bc6f770e394927b4da9f5ba833eaa9468b5d47f203a3"},
	{"karasugoi_NA", "02f803e6f159824a181cc5d709f3d1e7ff65f19e1899920724aeb4e3d2d869f911"},
	{"webworker01_NA", "03d6c76aabe24fde7ce7cc37cff0899d50a20d4147ac0b2db812e2a1edcf0d5232"},
	{"yurii_DEV", "0243977da0533c7c1a37f0f6e30175225c9012d9f3f426180ff6e5710f5a50e32b"},
	{"ca333_DEV", "035f3413d71856ac0859f564ced42fe1ce5c5058df888f4592b8a11d34a5ba3a45"},
	{"chmex_SH", "03e09c8ee6ae20cde64857d116c4bb5d50db6de2887ac39ea3ccf6434b1abf8698"},
	{"collider_SH", "033a1b62de10c3802f359da7767b033eac3837b58530722f3ddd2f359a2cd0a8f9"}, // 50
	{"dappvader_SH", "02684e2e7425ffa36d331f7a2f9c4542b61e88370dc6b4313a5025643f82ee17fa"},
	{"drkush_SH", "0210320b03f00f10f16313eb6e8929b5be7e66a034a4e9b7d11f2d87aa92708c6c"},
	{"majora31_SH", "03bc75c112ac7c6a99d6eb3fe5582feef4fd1b43f11c08ad887e21c4c3bc4e9104"},
	{"mcrypt_SH", "027a4ca7b11d3456ff558c08bb04483a89c7f383448461fd0b6b3b07424aabe9a4"},
	{"metaphilibert_SH", "03b21ff042bf1730b28bde43f44c064578b41996117ac7634b567c3773089e3be3"},
	{"mylo_SH", "026a52dba25ca4deb225a5ef7fca117d59e20ef2319b00e1bb6750a5d61e5ed601"},
	{"nutellaLicka_SH", "03ca46ea9a32de632823419948188088069f5820023920d810da6076624adb9901"},
	{"pbca26_SH", "021b39173b2b966ab277799a1f148a1d9e6cf26020f5f7eb9708f020ee0461e9c0"},
	{"phit_SH", "021b893b7978284e3d73701a623f23104fcce27e70fb49427c215f9a7481f652da"},
	{"sheeba_SH", "030dd2c3c02cbc5b3c25e3c54ed02c1541951a6f5ecf8adbd353e8d9052d08b8fc"}, // 60
	{"strob_SH", "0213751a1c59d3489ca85b3d62a3d606dcef7f0428aa021c1978ea16fb38a2fad6"},
	{"strobnidan_SH", "033e33ef18effb979437cd202bb87dc32385e16ebd52d6f762d8a3b308d6f89c52"},
	{"dragonhound_DEV", "02b3c168ed4acd96594288cee3114c77de51b6afe1ab6a866887a13a96ee80f33c"}
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
        if ( GetTransaction(txid,tx,Params().GetConsensus(),hashBlock,false) == 0 )
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
    {
        fprintf(stderr,"gettxout_scriptPubKey ht.%d n.%d > voutsize.%d\n",height,n,(int32_t)tx->vout.size());
<<<<<<< HEAD:src/komodo_validation015.h
    }
=======

>>>>>>> 3fdbec2ff2a6377fa069bf3b74b6414c0813ad0b:src/komodo_validation022.h
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
    if ( height <= KMD_SEASON_HEIGHTS[0] )
        return(1);
    for (int32_t i = 1; i < NUM_KMD_SEASONS; i++)
    {
        if ( height <= KMD_SEASON_HEIGHTS[i] && height >= KMD_SEASON_HEIGHTS[i-1] )
            return(i+1);
    }
    return(0);
}

int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp)
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
