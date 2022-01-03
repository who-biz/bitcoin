/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
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

#ifndef KOMODO_NSPVFULLNODE_H
#define KOMODO_NSPVFULLNODE_H

// NSPV_get... functions need to return the exact serialized length, which is the size of the structure minus size of pointers, plus size of allocated data

#include "addressindex.h"
//#include "notarisationdb.h"
#include <boost/foreach.hpp>
#include "rpc/server.h"
#include "komodo_defs.h"

extern NodeContext* g_rpc_node;

uint256 Parseuint256(const char *hexstr)
{
    uint256 txid; int32_t i; std::vector<unsigned char> txidbytes(ParseHex(hexstr));
    memset(&txid,0,sizeof(txid));
    if ( strlen(hexstr) == 64 )
    {
        for (i=31; i>=0; i--)
            ((uint8_t *)&txid)[31-i] = ((uint8_t *)txidbytes.data())[i];
    }
    return(txid);
}

CPubKey pubkey2pk(std::vector<uint8_t> vpubkey)
{
    CPubKey pk;
    pk.Set(vpubkey.begin(), vpubkey.end());
    return(pk);
}

static std::map<std::string,bool> nspv_remote_commands =  {{"channelsopen", true},{"channelspayment", true},{"channelsclose", true},{"channelsrefund", true},
{"channelslist", true},{"channelsinfo", true},{"oraclescreate", true},{"oraclesfund", true},{"oraclesregister", true},{"oraclessubscribe", true}, 
{"oraclesdata", true},{"oraclesinfo", false},{"oracleslist", false},{"gatewaysbind", true},{"gatewaysdeposit", true},{"gatewaysclaim", true},{"gatewayswithdraw", true},
{"gatewayspartialsign", true},{"gatewayscompletesigning", true},{"gatewaysmarkdone", true},{"gatewayspendingdeposits", true},{"gatewayspendingwithdraws", true},
{"gatewaysprocessed", true},{"gatewaysinfo", false},{"gatewayslist", false},{"faucetfund", true},{"faucetget", true}};

struct NSPV_ntzargs
{
    uint256 txid,desttxid,blockhash;
    int32_t txidht,ntzheight;
};

uint256 NSPV_opretextract(int32_t *heightp,uint256 *blockhashp,char *symbol,std::vector<uint8_t> opret,uint256 txid)
{
    LogPrintf(">>> (%s) called <<<\n",__func__);
    uint256 desttxid; int32_t i;
    iguana_rwnum(0,&opret[32],sizeof(*heightp),heightp);
    for (i=0; i<32; i++)
        ((uint8_t *)blockhashp)[i] = opret[i];
    for (i=0; i<32; i++)
        ((uint8_t *)&desttxid)[i] = opret[4 + 32 + i];
    if ( 0 && *heightp != 2690 )
        fprintf(stderr," ntzht.%d %s <- txid.%s size.%d\n",*heightp,(*blockhashp).GetHex().c_str(),(txid).GetHex().c_str(),(int32_t)opret.size());
    return(desttxid);
}

int32_t NSPV_notarization_find(struct NSPV_ntzargs *args,int32_t height,int32_t dir)
{
    LogPrintf(">>> (%s) called <<<\n",__func__);
    uint256* ignore_ntzhash;
    int32_t ntzheight = 0; uint256 hashBlock; CTransactionRef tx; char *symbol; std::vector<uint8_t> opret; uint256 nota_txid;
    symbol = (ASSETCHAINS_SYMBOL[0] == 0) ? (char *)"KMD" : ASSETCHAINS_SYMBOL;
    memset(args,0,sizeof(*args));
    if ( dir > 0 )
        height += 10;
    if ( (args->txidht= komodo_notarizeddata(height,ignore_ntzhash,&nota_txid)) == 0 )
        return(-1);
    args->txid = nota_txid;
    if ( !GetTransaction(args->txid,tx,Params().GetConsensus(),hashBlock,false) || tx->vout.size() < 2 )
        return(-2);
    GetOpReturnData(tx->vout[1].scriptPubKey,opret);
    if ( opret.size() >= 32*2+4 )
        args->desttxid = NSPV_opretextract(&args->ntzheight,&args->blockhash,symbol,opret,args->txid);
    return(args->ntzheight);
}

int32_t NSPV_notarized_bracket(struct NSPV_ntzargs *prev,struct NSPV_ntzargs *next,int32_t height)
{
    LogPrintf(">>> (%s) called <<<\n",__func__);
    uint256 bhash; int32_t txidht,ntzht,nextht,i=0;
    memset(prev,0,sizeof(*prev));
    memset(next,0,sizeof(*next));
    if ( (ntzht= NSPV_notarization_find(prev,height,-1)) < 0 || ntzht > height || ntzht == 0 )
        return(-1);
    txidht = height+1;
    while ( (ntzht=  NSPV_notarization_find(next,txidht,1)) < height )
    {
        nextht = next->txidht + 10*i;
//fprintf(stderr,"found forward ntz, but ntzht.%d vs height.%d, txidht.%d -> nextht.%d\n",next->ntzheight,height,txidht,nextht);
        memset(next,0,sizeof(*next));
        txidht = nextht;
        if ( ntzht <= 0 )
            break;
        if ( i++ > 10 )
            break;
    }
    return(0);
}

int32_t NSPV_ntzextract(struct NSPV_ntz *ptr,uint256 ntztxid,int32_t txidht,uint256 desttxid,int32_t ntzheight)
{
    LogPrintf(">>> (%s) called <<<\n",__func__);
    CBlockIndex *pindex;
    ptr->blockhash = *g_rpc_node->chainman->ActiveChain()[ntzheight]->phashBlock;
    ptr->height = ntzheight;
    ptr->txidheight = txidht;
    ptr->othertxid = desttxid;
    ptr->txid = ntztxid;
    if ( (pindex= komodo_chainactive(ptr->txidheight)) != 0 )
        ptr->timestamp = pindex->nTime;
    return(0);
}

int32_t NSPV_getntzsresp(struct NSPV_ntzsresp *ptr,int32_t origreqheight)
{
    LogPrintf(">>> (%s) called <<<\n",__func__);
    struct NSPV_ntzargs prev,next; int32_t reqheight = origreqheight;
    if ( reqheight < g_rpc_node->chainman->ActiveChain().Tip()->nHeight )
        reqheight++;
    if ( NSPV_notarized_bracket(&prev,&next,reqheight) == 0 )
    {
        if ( prev.ntzheight != 0 )
        {
            ptr->reqheight = origreqheight;
            if ( NSPV_ntzextract(&ptr->prevntz,prev.txid,prev.txidht,prev.desttxid,prev.ntzheight) < 0 )
                return(-1);
        }
        if ( next.ntzheight != 0 )
        {
            if ( NSPV_ntzextract(&ptr->nextntz,next.txid,next.txidht,next.desttxid,next.ntzheight) < 0 )
                return(-1);
        }
    }
    return(sizeof(*ptr));
}

int32_t NSPV_setequihdr(struct NSPV_equihdr *hdr,int32_t height)
{
    LogPrintf(">>> (%s) called <<<\n",__func__);
    CBlockIndex *pindex;
    if ( (pindex= komodo_chainactive(height)) != 0 )
    {
        hdr->nVersion = pindex->nVersion;
        if ( pindex->pprev == 0 )
            return(-1);
        hdr->hashPrevBlock = pindex->pprev->GetBlockHash();
        hdr->hashMerkleRoot = pindex->hashMerkleRoot;
        hdr->nTime = pindex->nTime;
        hdr->nBits = pindex->nBits;
        hdr->nNonce = pindex->nNonce;
        return(sizeof(*hdr));
    }
    return(-1);
}

int32_t NSPV_getinfo(struct NSPV_inforesp *ptr,int32_t reqheight)
{
    LogPrintf(">>> (%s) called <<<\n",__func__);
    int32_t prevMoMheight,len = 0; CBlockIndex *pindex, *pindex2; struct NSPV_ntzsresp pair;
    if ( (pindex= g_rpc_node->chainman->ActiveChain().Tip()) != 0 )
    {
        ptr->height = pindex->nHeight;
        ptr->blockhash = pindex->GetBlockHash();
        memset(&pair,0,sizeof(pair));
        //TODO: figure out solution for NotarisationDB headache
        //if ( NSPV_getntzsresp(&pair,ptr->height-1) < 0 )
        //    return(-1);
        //ptr->notarization = pair.prevntz;
        if ( (pindex2= komodo_chainactive(ptr->notarization.txidheight)) != 0 )
            ptr->notarization.timestamp = pindex->nTime;
        //fprintf(stderr, "timestamp.%i\n", ptr->notarization.timestamp );
        if ( reqheight == 0 )
            reqheight = ptr->height;
        ptr->hdrheight = reqheight;
        ptr->version = NSPV_PROTOCOL_VERSION;
        if ( NSPV_setequihdr(&ptr->H,reqheight) < 0 )
            return(-1);
        return(sizeof(*ptr));
    } else return(-1);
}

int32_t NSPV_getaddressutxos(struct NSPV_utxosresp *ptr,char *coinaddr,int32_t skipcount,uint32_t filter)
{
    int64_t total = 0,interest=0; uint32_t locktime; int32_t ind=0,tipheight,maxlen,txheight,n = 0,len = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    maxlen = DEFAULT_BLOCK_MAX_SIZE - 512;
    maxlen /= sizeof(*ptr->utxos);
    strncpy(ptr->coinaddr,coinaddr,sizeof(ptr->coinaddr)-1);
    ptr->filter = filter;
    if ( skipcount < 0 )
        skipcount = 0;
    if ( (ptr->numutxos= (int32_t)unspentOutputs.size()) >= 0 && ptr->numutxos < maxlen )
    {
        tipheight = g_rpc_node->chainman->ActiveChain().Tip()->nHeight;
        ptr->nodeheight = tipheight;
        if ( skipcount >= ptr->numutxos )
            skipcount = ptr->numutxos-1;
        ptr->skipcount = skipcount;
        if ( ptr->numutxos-skipcount > 0 )
        {
            ptr->utxos = (struct NSPV_utxoresp *)calloc(ptr->numutxos-skipcount,sizeof(*ptr->utxos));
            for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
            {
                // if gettxout is != null to handle mempool
                {
                    if ( n >= skipcount && myIsutxo_spentinmempool(ignoretxid,ignorevin,it->first.txhash,(int32_t)it->first.index) == 0  )
                    {
                        ptr->utxos[ind].txid = it->first.txhash;
                        ptr->utxos[ind].vout = (int32_t)it->first.index;
                        ptr->utxos[ind].satoshis = it->second.satoshis;
                        ptr->utxos[ind].height = it->second.blockHeight;
//                        if ( ASSETCHAINS_SYMBOL[0] == 0 && it->second.satoshis >= 10*COIN )
//                        {
//                            ptr->utxos[n].extradata = komodo_accrued_interest(&txheight,&locktime,ptr->utxos[ind].txid,ptr->utxos[ind].vout,ptr->utxos[ind].height,ptr->utxos[ind].satoshis,tipheight);
//                            interest += ptr->utxos[ind].extradata;
//                        }
                        ind++;
                        total += it->second.satoshis;
                    }
                    n++;
                }
            }
        }
        ptr->numutxos = ind;
        if ( len < maxlen )
        {
            len = (int32_t)(sizeof(*ptr) + sizeof(*ptr->utxos)*ptr->numutxos - sizeof(ptr->utxos));
            //fprintf(stderr,"getaddressutxos for %s -> n.%d:%d total %.8f interest %.8f len.%d\n",coinaddr,n,ptr->numutxos,dstr(total),dstr(interest),len);
            ptr->total = total;
            ptr->interest = interest;
            return(len);
        }
    }
    if ( ptr->utxos != 0 )
        free(ptr->utxos);
    memset(ptr,0,sizeof(*ptr));
    return(0);
}

int32_t NSPV_getaddresstxids(struct NSPV_txidsresp *ptr,char *coinaddr,int32_t skipcount,uint32_t filter)
{
    int32_t maxlen,txheight,ind=0,n = 0,len = 0; CTransactionRef tx; uint256 hashBlock;
    std::vector<std::pair<CAddressIndexKey, CAmount> > txids;
    ptr->nodeheight = g_rpc_node->chainman->ActiveChain().Tip()->nHeight;
    maxlen = DEFAULT_BLOCK_MAX_SIZE - 512;
    maxlen /= sizeof(*ptr->txids);
    strncpy(ptr->coinaddr,coinaddr,sizeof(ptr->coinaddr)-1);
    ptr->filter = filter;
    if ( skipcount < 0 )
        skipcount = 0;

    if ( txids.size() <= std::numeric_limits<uint16_t>::max() &&
         (ptr->numtxids = txids.size()) >= 0 && ptr->numtxids < maxlen )
    {
        if ( skipcount >= ptr->numtxids )
            skipcount = ptr->numtxids-1;
        ptr->skipcount = skipcount;
        if ( ptr->numtxids-skipcount > 0 )
        {
            ptr->txids = (struct NSPV_txidresp *)calloc(ptr->numtxids-skipcount,sizeof(*ptr->txids));
            // TODO: make this loop optimal (!), probably with reverse iterator, to get only last needed N transactions
            for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=txids.begin(); it!=txids.end(); it++)
            {
                if ( n >= skipcount )
                {
                    ptr->txids[ind].txid = it->first.txhash;
                    ptr->txids[ind].vout = (int32_t)it->first.index;
                    ptr->txids[ind].satoshis = (int64_t)it->second;
                    ptr->txids[ind].height = (int64_t)it->first.blockHeight;
                    ind++;
                }
                n++;
            }
        }
        ptr->numtxids = ind;
        len = (int32_t)(sizeof(*ptr) + sizeof(*ptr->txids)*ptr->numtxids - sizeof(ptr->txids));
        return(len);
    }
    if ( ptr->txids != 0 )
        free(ptr->txids);
    memset(ptr,0,sizeof(*ptr));
    return(0);
}

/*int32_t NSPV_mempoolfuncs(bits256 *satoshisp,int32_t *vindexp,std::vector<uint256> &txids,char *coinaddr,uint8_t funcid,uint256 txid,int32_t vout)
{
    int32_t num = 0,vini = 0,vouti = 0; uint8_t evalcode=0,func=0;  std::vector<uint8_t> vopret; char destaddr[64];
    *vindexp = -1;
    memset(satoshisp,0,sizeof(*satoshisp));
    mempool = EnsureAnyMempool(
    if ( mempool->size() == 0 )
        return(0);
    LOCK(mempool->cs);
    BOOST_FOREACH(const CTxMemPoolEntry &e,mempool->mapTx)
    {
        const CTransaction tx = e.GetTx();
        const uint256 &hash = tx.GetHash();
        if ( funcid == NSPV_MEMPOOL_ALL )
        {
            txids.push_back(hash);
            num++;
            continue;
        }
        else if ( funcid == NSPV_MEMPOOL_INMEMPOOL )
        {
            if ( hash == txid )
            {
                txids.push_back(hash);
                return(++num);
            }
            continue;
        }
        if ( funcid == NSPV_MEMPOOL_ISSPENT )
        {
            BOOST_FOREACH(const CTxIn &txin,tx.vin)
            {
                //fprintf(stderr,"%s/v%d ",uint256_str(str,txin.prevout.hash),txin.prevout.n);
                if ( txin.prevout.n == vout && txin.prevout.hash == txid )
                {
                    txids.push_back(hash);
                    *vindexp = vini;
                    return(++num);
                }
                vini++;
            }
        }
        else if ( funcid == NSPV_MEMPOOL_ADDRESS )
        {
            BOOST_FOREACH(const CTxOut &txout,tx.vout)
            {
                vouti++;
            }
        }
        //fprintf(stderr,"are vins for %s\n",uint256_str(str,hash));
    }
    return(num);
}

int32_t NSPV_mempooltxids(struct NSPV_mempoolresp *ptr,char *coinaddr,uint8_t funcid,uint256 txid,int32_t vout)
{
    std::vector<uint256> txids; bits256 satoshis; uint256 tmp,tmpdest; int32_t i,len = 0;
    ptr->nodeheight = g_rpc_node->chainman->ActiveChain().Tip()->nHeight;
    strncpy(ptr->coinaddr,coinaddr,sizeof(ptr->coinaddr)-1);
    ptr->txid = txid;
    ptr->vout = vout;
    ptr->funcid = funcid;
    if ( NSPV_mempoolfuncs(&satoshis,&ptr->vindex,txids,coinaddr,funcid,txid,vout) >= 0 )
    {
        if ( (ptr->numtxids= (int32_t)txids.size()) >= 0 )
        {
            if ( ptr->numtxids > 0 )
            {
                ptr->txids = (uint256 *)calloc(ptr->numtxids,sizeof(*ptr->txids));
                for (i=0; i<ptr->numtxids; i++)
                {
                    tmp = txids[i];
                    iguana_rwbignum(0,(uint8_t *)&tmp,sizeof(*ptr->txids),(uint8_t *)&ptr->txids[i]);
                }
            }
            if ( funcid == NSPV_MEMPOOL_ADDRESS )
            {
                memcpy(&tmp,&satoshis,sizeof(tmp));
                iguana_rwbignum(0,(uint8_t *)&tmp,sizeof(ptr->txid),(uint8_t *)&ptr->txid);
            }
            len = (int32_t)(sizeof(*ptr) + sizeof(*ptr->txids)*ptr->numtxids - sizeof(ptr->txids));
            return(len);
        }
    }
    if ( ptr->txids != 0 )
        free(ptr->txids);
    memset(ptr,0,sizeof(*ptr));
    return(0);
}
*/
int32_t NSPV_remoterpc(struct NSPV_remoterpcresp *ptr,char *json,int n)
{
    LogPrintf(">>> (%s) called <<<\n",__func__);
    std::vector<uint256> txids; int32_t i,len = 0; UniValue result; std::string response;
    UniValue request(UniValue::VOBJ),rpc_result(UniValue::VOBJ); JSONRPCRequest jreq; CPubKey mypk;

    try
    {
        request.read(json,n);
        jreq.parse(request);
        strcpy(ptr->method,jreq.strMethod.c_str());
        len+=sizeof(ptr->method);
        std::map<std::string, bool>::iterator it = nspv_remote_commands.find(jreq.strMethod);
        if (it==nspv_remote_commands.end())
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not supported!");
        const CRPCCommand *cmd=tableRPC[jreq.strMethod];
        if (!cmd)
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");
        if (it->second)
        {
            if (!request.exists("mypk"))
                throw JSONRPCError(RPC_PARSE_ERROR, "No pubkey supplied in remote rpc request, necessary for this type of rpc");
            std::string str=request["mypk"].get_str();
            mypk=pubkey2pk(ParseHex(str));
            if (!mypk.IsValid())
                throw JSONRPCError(RPC_PARSE_ERROR, "Not valid pubkey passed in remote rpc call");
        }
        bool success = false;
        //TODO: ensure we are calling cmd->actor correctly
        cmd->actor(jreq,result,success);
        if (success && (result.isObject() || result.isArray()))
        {
            rpc_result = JSONRPCReplyObj(result, NullUniValue, jreq.id);
            response=rpc_result.write();
            memcpy(ptr->json,response.c_str(),response.size());
            len+=response.size();
            return (len);
        }
        else throw JSONRPCError(RPC_MISC_ERROR, "Error in executing RPC on remote node");        
    }
    catch (const UniValue& objError)
    {
        rpc_result = JSONRPCReplyObj(NullUniValue, objError, jreq.id);
        response=rpc_result.write();
    }
    catch (const std::runtime_error& e)
    {
        rpc_result = JSONRPCReplyObj(NullUniValue,JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
        response=rpc_result.write();
    }
    catch (const std::exception& e)
    {
        rpc_result = JSONRPCReplyObj(NullUniValue,JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
        response=rpc_result.write();
    }
    memcpy(ptr->json,response.c_str(),response.size());
    len+=response.size();
    return (len);
}

uint8_t *NSPV_getrawtx(CTransactionRef &tx,uint256 &hashBlock,int32_t *txlenp,uint256 txid)
{
    uint8_t *rawtx = 0;
    *txlenp = 0;
    {
        if (!GetTransaction(txid, tx, Params().GetConsensus(), hashBlock, false))
        {
            //fprintf(stderr,"error getting transaction %s\n",txid.GetHex().c_str());
            return(0);
        }
        std::string strHex = EncodeHexTx(*tx);
        *txlenp = (int32_t)strHex.size() >> 1;
        if ( *txlenp > 0 )
        {
            rawtx = (uint8_t *)calloc(1,*txlenp);
            decode_hex(rawtx,*txlenp,(char *)strHex.c_str());
        }
    }
    return(rawtx);
}

/*int32_t NSPV_sendrawtransaction(struct NSPV_broadcastresp *ptr,uint8_t *data,int32_t n)
{
    CTransactionRef tx;
    ptr->retcode = 0;
    if ( NSPV_txextract(tx,data,n) == 0 )
    {
        //LOCK(cs_main);
        ptr->txid = tx.GetHash();
        //fprintf(stderr,"try to addmempool transaction %s\n",ptr->txid.GetHex().c_str());
        if ( myAddtomempool(tx) != 0 )
        {
            ptr->retcode = 1;
            //int32_t i;
            //for (i=0; i<n; i++)
            //    fprintf(stderr,"%02x",data[i]);
            fprintf(stderr," relay transaction %s retcode.%d\n",ptr->txid.GetHex().c_str(),ptr->retcode);
            RelayTransaction(tx);
        } else ptr->retcode = -3;

    } else ptr->retcode = -1;
    return(sizeof(*ptr));
}

int32_t NSPV_gettxproof(struct NSPV_txproof *ptr,int32_t vout,uint256 txid,int32_t height)
{
    int32_t flag = 0,len = 0; CTransactionRef _tx; uint256 hashBlock; CBlock block; CBlockIndex *pindex;
    ptr->height = -1;
    if ( (ptr->tx= NSPV_getrawtx(_tx,hashBlock,&ptr->txlen,txid)) != 0 )
    {
        ptr->txid = txid;
        ptr->vout = vout;
        ptr->hashblock = hashBlock;
        if ( height == 0 )
            ptr->height = komodo_blockheight(hashBlock);
        else
        {
            ptr->height = height;
            if ( (pindex= komodo_chainactive(height)) != 0 && komodo_blockload(block,pindex) == 0 )
            {
                BOOST_FOREACH(const CTransactionRef&tx, block.vtx)
                {
                    if ( tx.GetHash() == txid )
                    {
                        flag = 1;
                        break;
                    }
                }
                if ( flag != 0 )
                {
                    set<uint256> setTxids;
                    CDataStream ssMB(SER_NETWORK, PROTOCOL_VERSION);
                    setTxids.insert(txid);
                    CMerkleBlock mb(block, setTxids);
                    ssMB << mb;
                    std::vector<uint8_t> proof(ssMB.begin(), ssMB.end());
                    ptr->txprooflen = (int32_t)proof.size();
                    //fprintf(stderr,"%s txproof.(%s)\n",txid.GetHex().c_str(),HexStr(proof).c_str());
                    if ( ptr->txprooflen > 0 )
                    {
                        ptr->txproof = (uint8_t *)calloc(1,ptr->txprooflen);
                        memcpy(ptr->txproof,&proof[0],ptr->txprooflen);
                    }
                    //fprintf(stderr,"gettxproof slen.%d\n",(int32_t)(sizeof(*ptr) - sizeof(ptr->tx) - sizeof(ptr->txproof) + ptr->txlen + ptr->txprooflen));
                }
            }
        }
    }
    return(sizeof(*ptr) - sizeof(ptr->tx) - sizeof(ptr->txproof) + ptr->txlen + ptr->txprooflen);
}

int32_t NSPV_getntzsproofresp(struct NSPV_ntzsproofresp *ptr,uint256 prevntztxid,uint256 nextntztxid)
{
    int32_t i; uint256 hashBlock,bhash0,bhash1,desttxid0,desttxid1; CTransactionRef tx;
    ptr->prevtxid = prevntztxid;
    ptr->prevntz = NSPV_getrawtx(tx,hashBlock,&ptr->prevtxlen,ptr->prevtxid);
    ptr->prevtxidht = komodo_blockheight(hashBlock);
    if ( NSPV_notarizationextract(0,&ptr->common.prevht,&bhash0,&desttxid0,tx) < 0 )
        return(-2);
    else if ( komodo_blockheight(bhash0) != ptr->common.prevht )
        return(-3);
    
    ptr->nexttxid = nextntztxid;
    ptr->nextntz = NSPV_getrawtx(tx,hashBlock,&ptr->nexttxlen,ptr->nexttxid);
    ptr->nexttxidht = komodo_blockheight(hashBlock);
    if ( NSPV_notarizationextract(0,&ptr->common.nextht,&bhash1,&desttxid1,tx) < 0 )
        return(-5);
    else if ( komodo_blockheight(bhash1) != ptr->common.nextht )
        return(-6);

    else if ( ptr->common.prevht > ptr->common.nextht || (ptr->common.nextht - ptr->common.prevht) > 1440 )
    {
        fprintf(stderr,"illegal prevht.%d nextht.%d\n",ptr->common.prevht,ptr->common.nextht);
        return(-7);
    }
    //fprintf(stderr,"%s -> prevht.%d, %s -> nexht.%d\n",ptr->prevtxid.GetHex().c_str(),ptr->common.prevht,ptr->nexttxid.GetHex().c_str(),ptr->common.nextht);
    ptr->common.numhdrs = (ptr->common.nextht - ptr->common.prevht + 1);
    ptr->common.hdrs = (struct NSPV_equihdr *)calloc(ptr->common.numhdrs,sizeof(*ptr->common.hdrs));
    //fprintf(stderr,"prev.%d next.%d allocate numhdrs.%d\n",prevht,nextht,ptr->common.numhdrs);
    for (i=0; i<ptr->common.numhdrs; i++)
    {
        //hashBlock = NSPV_hdrhash(&ptr->common.hdrs[i]);
        //fprintf(stderr,"hdr[%d] %s\n",prevht+i,hashBlock.GetHex().c_str());
        if ( NSPV_setequihdr(&ptr->common.hdrs[i],ptr->common.prevht+i) < 0 )
        {
            fprintf(stderr,"error setting hdr.%d\n",ptr->common.prevht+i);
            free(ptr->common.hdrs);
            ptr->common.hdrs = 0;
            return(-1);
        }
    }
    //fprintf(stderr,"sizeof ptr %ld, common.%ld lens.%d %d\n",sizeof(*ptr),sizeof(ptr->common),ptr->prevtxlen,ptr->nexttxlen);
    return(sizeof(*ptr) + sizeof(*ptr->common.hdrs)*ptr->common.numhdrs - sizeof(ptr->common.hdrs) - sizeof(ptr->prevntz) - sizeof(ptr->nextntz) + ptr->prevtxlen + ptr->nexttxlen);
}*/

int32_t NSPV_getspentinfo(struct NSPV_spentinfo *ptr,uint256 txid,int32_t vout)
{
    int32_t len = 0;
    ptr->txid = txid;
    ptr->vout = vout;
    ptr->spentvini = -1;
    len = (int32_t)(sizeof(*ptr) - sizeof(ptr->spent.tx) - sizeof(ptr->spent.txproof));
    return(len);
}

void komodo_nSPVreq(CNode *pfrom,std::vector<uint8_t> request) // received a request
{
    LogPrintf(">>> (%s) called <<<\n",__func__);
    int32_t len,slen,ind,reqheight,n; std::vector<uint8_t> response; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= request.size()) > 0 )
    {
        if ( (ind= request[0]>>1) >= sizeof(pfrom->prevtimes)/sizeof(*pfrom->prevtimes) )
            ind = (int32_t)(sizeof(pfrom->prevtimes)/sizeof(*pfrom->prevtimes)) - 1;
        if ( pfrom->prevtimes[ind] > timestamp )
            pfrom->prevtimes[ind] = 0;
        if ( request[0] == NSPV_INFO ) // info
        {
            //fprintf(stderr,"check info %u vs %u, ind.%d\n",timestamp,pfrom->prevtimes[ind],ind);
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_inforesp I;
                if ( len == 1+sizeof(reqheight) )
                    iguana_rwnum(0,&request[1],sizeof(reqheight),&reqheight);
                else reqheight = 0;
                //fprintf(stderr,"request height.%d\n",reqheight);
                memset(&I,0,sizeof(I));
                if ( (slen= NSPV_getinfo(&I,reqheight)) > 0 )
                {
                    response.resize(1 + slen);
                    response[0] = NSPV_INFORESP;
                    //fprintf(stderr,"slen.%d version.%d\n",slen,I.version);
                    if ( NSPV_rwinforesp(1,&response[1],&I) == slen )
                    {
                        //fprintf(stderr,"send info resp to id %d\n",(int32_t)pfrom->id);
                        g_rpc_node->connman->PushMessage(pfrom,CNetMsgMaker(pfrom->GetCommonVersion()).Make(NetMsgType::NSPV,response));
                        pfrom->prevtimes[ind] = timestamp;
                    }
                    NSPV_inforesp_purge(&I);
                }
            }
        }
/*        else if ( request[0] == NSPV_UTXOS )
        {
            //fprintf(stderr,"utxos: %u > %u, ind.%d, len.%d\n",timestamp,pfrom->prevtimes[ind],ind,len);
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_utxosresp U;
                if ( len < 64+5 && (request[1] == len-3 || request[1] == len-7 || request[1] == len-11) )
                {
                    int32_t skipcount = 0; char coinaddr[64]; uint8_t filter;
                    memcpy(coinaddr,&request[2],request[1]);
                    coinaddr[request[1]] = 0;
                    if ( request[1] == len-7 )
                    {
                        iguana_rwnum(0,&request[len-4],sizeof(skipcount),&skipcount);
                    }
                    else
                    {
                        iguana_rwnum(0,&request[len-8],sizeof(skipcount),&skipcount);
                        iguana_rwnum(0,&request[len-4],sizeof(filter),&filter);
                    }
                    if ( 0)
                        fprintf(stderr,"utxos %s skipcount.%d filter.%x\n",coinaddr,skipcount,filter);
                    memset(&U,0,sizeof(U));
                    if ( (slen= NSPV_getaddressutxos(&U,coinaddr,skipcount,filter)) > 0 )
                    {
                        response.resize(1 + slen);
                        response[0] = NSPV_UTXOSRESP;
                        if ( NSPV_rwutxosresp(1,&response[1],&U) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_utxosresp_purge(&U);
                    }
                }
            }
        }
        else if ( request[0] == NSPV_TXIDS )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_txidsresp T;
                if ( len < 64+5 && (request[1] == len-3 || request[1] == len-7 || request[1] == len-11) )
                {
                    int32_t skipcount = 0; char coinaddr[64]; uint32_t filter;
                    memcpy(coinaddr,&request[2],request[1]);
                    coinaddr[request[1]] = 0;
                    else if ( request[1] == len-7 )
                    {
                        iguana_rwnum(0,&request[len-4],sizeof(skipcount),&skipcount);
                    }
                    else
                    {
                        iguana_rwnum(0,&request[len-8],sizeof(skipcount),&skipcount);
                        iguana_rwnum(0,&request[len-4],sizeof(filter),&filter);
                    }
                    if ( 0 )
                        fprintf(stderr,"txids %s skipcount.%d filter.%d\n",coinaddr,skipcount,filter);
                    memset(&T,0,sizeof(T));
                    if ( (slen= NSPV_getaddresstxids(&T,coinaddr,skipcount,filter)) > 0 )
                    {
//fprintf(stderr,"slen.%d\n",slen);
                        response.resize(1 + slen);
                        response[0] = NSPV_TXIDSRESP;
                        if ( NSPV_rwtxidsresp(1,&response[1],&T) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_txidsresp_purge(&T);
                    }
                } else fprintf(stderr,"len.%d req1.%d\n",len,request[1]);
            }
        }
        else if ( request[0] == NSPV_MEMPOOL )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_mempoolresp M; char coinaddr[64];
                if ( len < sizeof(M)+64 )
                {
                    int32_t vout; uint256 txid; uint8_t funcid;
                    n = 1;
                    n += iguana_rwnum(0,&request[n],sizeof(funcid),&funcid);
                    n += iguana_rwnum(0,&request[n],sizeof(vout),&vout);
                    n += iguana_rwbignum(0,&request[n],sizeof(txid),(uint8_t *)&txid);
                    slen = request[n++];
                    if ( slen < 63 )
                    {
                        memcpy(coinaddr,&request[n],slen), n += slen;
                        coinaddr[slen] = 0;
                        memset(&M,0,sizeof(M));
                        if ( (slen= NSPV_mempooltxids(&M,coinaddr,funcid,txid,vout)) > 0 )
                        {
                            //fprintf(stderr,"NSPV_mempooltxids slen.%d\n",slen);
                            response.resize(1 + slen);
                            response[0] = NSPV_MEMPOOLRESP;
                            if ( NSPV_rwmempoolresp(1,&response[1],&M) == slen )
                            {
                                pfrom->PushMessage("nSPV",response);
                                pfrom->prevtimes[ind] = timestamp;
                            }
                            NSPV_mempoolresp_purge(&M);
                        }
                    }
                } else fprintf(stderr,"len.%d req1.%d\n",len,request[1]);
            }
        }*/
        else if ( request[0] == NSPV_NTZS )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_ntzsresp N; int32_t height;
                if ( len == 1+sizeof(height) )
                {
                    iguana_rwnum(0,&request[1],sizeof(height),&height);
                    memset(&N,0,sizeof(N));
                    if ( (slen= NSPV_getntzsresp(&N,height)) > 0 )
                    {
                        response.resize(1 + slen);
                        response[0] = NSPV_NTZSRESP;
                        if ( NSPV_rwntzsresp(1,&response[1],&N) == slen )
                        {
                            g_rpc_node->connman->PushMessage(pfrom,CNetMsgMaker(pfrom->GetCommonVersion()).Make(NetMsgType::NSPV,response));
                            //pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_ntzsresp_purge(&N);
                    }
                }
            }
        }
        /*else if ( request[0] == NSPV_NTZSPROOF )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_ntzsproofresp P; uint256 prevntz,nextntz;
                if ( len == 1+sizeof(prevntz)+sizeof(nextntz) )
                {
                    iguana_rwbignum(0,&request[1],sizeof(prevntz),(uint8_t *)&prevntz);
                    iguana_rwbignum(0,&request[1+sizeof(prevntz)],sizeof(nextntz),(uint8_t *)&nextntz);
                    memset(&P,0,sizeof(P));
                    if ( (slen= NSPV_getntzsproofresp(&P,prevntz,nextntz)) > 0 )
                    {
                        // fprintf(stderr,"slen.%d msg prev.%s next.%s\n",slen,prevntz.GetHex().c_str(),nextntz.GetHex().c_str());
                        response.resize(1 + slen);
                        response[0] = NSPV_NTZSPROOFRESP;
                        if ( NSPV_rwntzsproofresp(1,&response[1],&P) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_ntzsproofresp_purge(&P);
                    } else fprintf(stderr,"err.%d\n",slen);
                }
            }
        }
        else if ( request[0] == NSPV_TXPROOF )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_txproof P; uint256 txid; int32_t height,vout;
                if ( len == 1+sizeof(txid)+sizeof(height)+sizeof(vout) )
                {
                    iguana_rwnum(0,&request[1],sizeof(height),&height);
                    iguana_rwnum(0,&request[1+sizeof(height)],sizeof(vout),&vout);
                    iguana_rwbignum(0,&request[1+sizeof(height)+sizeof(vout)],sizeof(txid),(uint8_t *)&txid);
                    //fprintf(stderr,"got txid %s/v%d ht.%d\n",txid.GetHex().c_str(),vout,height);
                    memset(&P,0,sizeof(P));
                    if ( (slen= NSPV_gettxproof(&P,vout,txid,height)) > 0 )
                    {
                        //fprintf(stderr,"slen.%d\n",slen);
                        response.resize(1 + slen);
                        response[0] = NSPV_TXPROOFRESP;
                        if ( NSPV_rwtxproof(1,&response[1],&P) == slen )
                        {
                            //fprintf(stderr,"send response\n");
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_txproof_purge(&P);
                    } else fprintf(stderr,"gettxproof error.%d\n",slen);
                } else fprintf(stderr,"txproof reqlen.%d\n",len);
            }
        }*/
        else if ( request[0] == NSPV_SPENTINFO )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_spentinfo S; int32_t vout; uint256 txid;
                if ( len == 1+sizeof(txid)+sizeof(vout) )
                {
                    iguana_rwnum(0,&request[1],sizeof(vout),&vout);
                    iguana_rwbignum(0,&request[1+sizeof(vout)],sizeof(txid),(uint8_t *)&txid);
                    memset(&S,0,sizeof(S));
                    if ( (slen= NSPV_getspentinfo(&S,txid,vout)) > 0 )
                    {
                        response.resize(1 + slen);
                        response[0] = NSPV_SPENTINFORESP;
                        if ( NSPV_rwspentinfo(1,&response[1],&S) == slen )
                        {
                            g_rpc_node->connman->PushMessage(pfrom,CNetMsgMaker(pfrom->GetCommonVersion()).Make(NetMsgType::NSPV,response));
                            //pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_spentinfo_purge(&S);
                    }
                }
            }
        }
        /*else if ( request[0] == NSPV_BROADCAST )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_broadcastresp B; uint32_t n,offset; uint256 txid;
                if ( len > 1+sizeof(txid)+sizeof(n) )
                {
                    iguana_rwbignum(0,&request[1],sizeof(txid),(uint8_t *)&txid);
                    iguana_rwnum(0,&request[1+sizeof(txid)],sizeof(n),&n);
                    memset(&B,0,sizeof(B));
                    offset = 1 + sizeof(txid) + sizeof(n);
                    if ( request.size() == offset+n && (slen= NSPV_sendrawtransaction(&B,&request[offset],n)) > 0 )
                    {
                        response.resize(1 + slen);
                        response[0] = NSPV_BROADCASTRESP;
                        if ( NSPV_rwbroadcastresp(1,&response[1],&B) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_broadcast_purge(&B);
                    }
                }
            }
        }*/
        else if ( request[0] == NSPV_REMOTERPC )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_remoterpcresp R; int32_t p;
                p = 1;
                p+=iguana_rwnum(0,&request[p],sizeof(slen),&slen);
                memset(&R,0,sizeof(R));
                if (request.size() == p+slen && (slen=NSPV_remoterpc(&R,(char *)&request[p],slen))>0 )
                {
                    response.resize(1 + slen);
                    response[0] = NSPV_REMOTERPCRESP;
                    NSPV_rwremoterpcresp(1,&response[1],&R,slen);
                    g_rpc_node->connman->PushMessage(pfrom,CNetMsgMaker(pfrom->GetCommonVersion()).Make(NetMsgType::NSPV,response));
                    //pfrom->PushMessage("nSPV",response);
                    pfrom->prevtimes[ind] = timestamp;
                    NSPV_remoterpc_purge(&R);
                }
            }
        }
    }
}

#endif // KOMODO_NSPVFULLNODE_H
