#include "dbwrapper.h"
#include "notarisationdb.h"
#include "uint256.h"

#include <boost/foreach.hpp>


NotarisationDB *pnotarisations;


NotarisationDB::NotarisationDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(gArgs.GetDataDirNet() / "notarisations", nCacheSize, fMemory, fWipe, false) { }


bool IsTXSCL(const char* symbol)
{
    return strlen(symbol) >= 5 && strncmp(symbol, "TXSCL", 5) == 0;
}


bool GetBlockNotarisations(uint256 blockHash, NotarisationsInBlock &nibs)
{
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
    LogPrintf(">>> (%s) called <<<\n",__func__);
    if (height < 0 || height > g_rpc_node->chainman->ActiveChain().Height())
        return false;

    for (int i=0; i<scanLimitBlocks; i++) {
        if (i > height) break;
        NotarisationsInBlock notarisations;
        uint256 blockHash = *g_rpc_node->chainman->ActiveChain()[height-i]->phashBlock;
        if (!GetBlockNotarisations(blockHash, notarisations))
            continue;

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
