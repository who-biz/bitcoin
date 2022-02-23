#include "ccutils.h"
#include "dbwrapper.h"
#include "notarisationdb.h"
#include "uint256.h"

#include <boost/foreach.hpp>


NotarisationDB *pnotarisations;


NotarisationDB::NotarisationDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(gArgs.GetDataDirNet() / "notarisations", nCacheSize, fMemory, fWipe, false) { }

bool ParseNotarisationOpReturn(const CTransactionRef &tx, NotarisationData &data)
{
    if (tx->vout.size() < 2) return false;
    std::vector<unsigned char> vdata;
    if (!GetOpReturnData(tx->vout[1].scriptPubKey, vdata)) return false;
    bool out = E_UNMARSHAL(vdata, ss >> data);
    return out;
}

NotarisationsInBlock ScanBlockNotarisations(const CBlock &block, int nHeight)
{
    NotarisationsInBlock vNotarisations;
    int timestamp = block.nTime;

    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        CTransactionRef tx = block.vtx[i];

        NotarisationData data;
        bool parsed = ParseNotarisationOpReturn(tx, data);
        if (!parsed) data = NotarisationData();
        if (strlen(data.symbol) == 0)
          continue;

        //printf("Checked notarisation data for %s \n",data.symbol);
 /*       int authority = GetSymbolAuthority(data.symbol);

        if (authority == CROSSCHAIN_KOMODO) {
            if (!eval->CheckNotaryInputs(tx, nHeight, block.nTime))
                continue;
            //printf("Authorised notarisation data for %s \n",data.symbol);
        } else if (authority == CROSSCHAIN_STAKED) {
            // We need to create auth_STAKED dynamically here based on timestamp
            int32_t staked_era = STAKED_era(timestamp);
            if (staked_era == 0) {
              // this is an ERA GAP, so we will ignore this notarization
              continue;
             if ( is_STAKED(data.symbol) == 255 )
              // this chain is banned... we will discard its notarisation. 
              continue;
            } else {
              // pass era slection off to notaries_staked.cpp file
              auth_STAKED = Choose_auth_STAKED(staked_era);
            }
            if (!CheckTxAuthority(tx, auth_STAKED))
                continue;
        }
*/
        if (parsed) {
            vNotarisations.push_back(std::make_pair(tx->GetHash(), data));
            //printf("Parsed a notarisation for: %s, txid:%s, ccid:%i, momdepth:%i\n",
            //      data.symbol, tx.GetHash().GetHex().data(), data.ccId, data.MoMDepth);
            //if (!data.MoMoM.IsNull()) printf("MoMoM:%s\n", data.MoMoM.GetHex().data());
        } else
            LogPrintf("WARNING: Couldn't parse notarisation for tx: %s at height %i\n",
                    tx->GetHash().GetHex().data(), nHeight);
    }
    return vNotarisations;
}


bool IsTXSCL(const char* symbol)
{
    return strlen(symbol) >= 5 && strncmp(symbol, "TXSCL", 5) == 0;
}


bool GetBlockNotarisations(uint256 blockHash, NotarisationsInBlock &nibs)
{
    //LogPrintf(">>> (%s) called, blockHash(%s)\n",__func__, blockHash.GetHex());
    return pnotarisations->Read(blockHash, nibs);
}


bool GetBackNotarisation(uint256 notarisationHash, Notarisation &n)
{
    LogPrintf(">>> (%s) called <<<\n",__func__);
    return pnotarisations->Read(notarisationHash, n);
}


/*
 * Write an index of KMD notarisation id -> backnotarisation
 */
void WriteBackNotarisations(const NotarisationsInBlock notarisations, CDBBatch &batch)
{
    int wrote = 0;
    BOOST_FOREACH(const Notarisation &n, notarisations)
    {
        if (!n.second.txHash.IsNull()) {
            batch.Write(n.second.txHash, n);
            wrote++;
        }
    }
}


void EraseBackNotarisations(const NotarisationsInBlock notarisations, CDBBatch &batch)
{
    BOOST_FOREACH(const Notarisation &n, notarisations)
    {
        if (!n.second.txHash.IsNull())
            batch.Erase(n.second.txHash);
    }
}

/*
 * Scan notarisationsdb backwards for blocks containing a notarisation
 * for given symbol. Return height of matched notarisation or 0.
 */
int ScanNotarisationsDB(int height, std::string symbol, int scanLimitBlocks, Notarisation& out)
{
    //LogPrintf(">>> (%s) called, height(%d), symbol(%s), scanLimitBlocks(%s) <<<\n",__func__,height,symbol,scanLimitBlocks);
    if (height < 0 || height > g_rpc_node->chainman->ActiveChain().Height())
    {
       LogPrintf(">>> (%s) breakpoint.1, height(%d) ActiveChain()->Height().(%d)\n",__func__,height,g_rpc_node->chainman->ActiveChain().Height());
       return false;
    }
    for (int i=0; i<scanLimitBlocks; i++) {
        if (i > height) break;
        NotarisationsInBlock notarisations;
        uint256 blockHash = *g_rpc_node->chainman->ActiveChain()[height-i]->phashBlock;
        LogPrintf(">>> (%s) breakpoint.2, blockHash(%s)\n",__func__,blockHash.GetHex());
        if (!GetBlockNotarisations(blockHash, notarisations))
            continue;

        LogPrintf(">>> (%s) breakpoint.3\n",__func__);
        BOOST_FOREACH(Notarisation& nota, notarisations) {
            if (strcmp(nota.second.symbol, symbol.data()) == 0) {
                out = nota;
                return height-i;
            }
        }
    }
    return 0;
}

int ScanNotarisationsDB2(int height, std::string symbol, int scanLimitBlocks, Notarisation& out)
{
    int32_t i,maxheight,ht;
    maxheight = g_rpc_node->chainman->ActiveChain().Height();
    if ( height < 0 || height > maxheight )
        return false;
    for (i=0; i<scanLimitBlocks; i++)
    {
        ht = height+i;
        if ( ht > maxheight )
            break;
        NotarisationsInBlock notarisations;
        uint256 blockHash = *g_rpc_node->chainman->ActiveChain()[ht]->phashBlock;
        if ( !GetBlockNotarisations(blockHash,notarisations) )
            continue;
        BOOST_FOREACH(Notarisation& nota,notarisations)
        {
            if ( strcmp(nota.second.symbol,symbol.data()) == 0 )
            {
                out = nota;
                return(ht);
            }
        }
    }
    return 0;
}
