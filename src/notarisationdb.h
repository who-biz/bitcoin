#ifndef NOTARISATIONDB_H
#define NOTARISATIONDB_H

#include "uint256.h"
#include "streams.h"
#include "dbwrapper.h"
#include "komodo_validation021.h"


class NotarisationDB : public CDBWrapper
{
public:
    NotarisationDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
};



/*
 * Data from notarisation OP_RETURN from chain being notarised
 */
class NotarisationData
{
public:
    int IsBackNotarisation = 0;
    uint256 blockHash      = uint256();
    uint32_t height        = 0;
    uint256 txHash         = uint256();
    char symbol[64];
    uint256 MoM            = uint256();
    uint16_t MoMDepth      = 0;
    //uint16_t ccId          = 0;
    uint256 MoMoM          = uint256();
    uint32_t MoMoMDepth    = 0;

    NotarisationData(int IsBack=2) : IsBackNotarisation(IsBack) {
        symbol[0] = '\0';
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {

        bool IsBack = IsBackNotarisation;
        if (2 == IsBackNotarisation) IsBack = DetectBackNotarisation(s, ser_action);

        READWRITE(blockHash);
        READWRITE(height);
        if (IsBack)
            READWRITE(txHash);
        SerSymbol(s, ser_action);
        if (s.size() == 0) return;
        READWRITE(MoM);
        READWRITE(MoMDepth);
        //READWRITE(ccId);
        if (s.size() == 0) return;
        if (IsBack) {
            READWRITE(MoMoM);
            READWRITE(MoMoMDepth);
        }
    }
    
    template <typename Stream>
    void SerSymbol(Stream& s, CSerActionSerialize act)
    {
        s.write(symbol, strlen(symbol)+1);
    }

    template <typename Stream>
    void SerSymbol(Stream& s, CSerActionUnserialize act)
    {
        size_t readlen = std::min(sizeof(symbol), s.size());
        char *nullPos = (char*) memchr(&s[0], 0, readlen);
        if (!nullPos)
            throw std::ios_base::failure("couldn't parse symbol");
        s.read(symbol, nullPos-&s.str().data()[0]+1);
    }

    template <typename Stream>
    bool DetectBackNotarisation(Stream& s, CSerActionUnserialize act)
    {
        if (ASSETCHAINS_SYMBOL[0]) return 1;
        if (s.size() >= 72) {
            if (strcmp("BTC", &s.str().data()[68]) == 0) return 1;
            if (strcmp("KMD", &s.str().data()[68]) == 0) return 1;
        }
        return 0;
    }
    
    template <typename Stream>
    bool DetectBackNotarisation(Stream& s, CSerActionSerialize act)
    {
        return !txHash.IsNull();
    }
};

extern NotarisationDB *pnotarisations;

typedef std::pair<uint256,NotarisationData> Notarisation;
typedef std::vector<Notarisation> NotarisationsInBlock;

NotarisationsInBlock ScanBlockNotarisations(const CBlock &block, int nHeight);
bool GetBlockNotarisations(uint256 blockHash, NotarisationsInBlock &nibs);
bool GetBackNotarisation(uint256 notarisationHash, Notarisation &n);
void WriteBackNotarisations(const NotarisationsInBlock notarisations, CDBBatch &batch);
void EraseBackNotarisations(const NotarisationsInBlock notarisations, CDBBatch &batch);
int ScanNotarisationsDB(int height, std::string symbol, int scanLimitBlocks, Notarisation& out);
int ScanNotarisationsDB2(int height, std::string symbol, int scanLimitBlocks, Notarisation& out);
bool IsTXSCL(const char* symbol);

#endif  /* NOTARISATIONDB_H */
