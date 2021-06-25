// Copyright (c) 2021 barrystyle
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/system.h>

std::string gen_random(const int len)
{
    std::string tmpstr;
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < len; i++) {
        tmpstr += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return tmpstr;
}

bool bitcoinConfigCreator(std::string& strErrRet)
{
    boost::filesystem::path pathConfigFile = GetConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME));
    boost::filesystem::ifstream streamConfig(pathConfigFile);

    if (!streamConfig.good()) {
        FILE* configFile = fopen(pathConfigFile.string().c_str(), "a");
        if (configFile != nullptr) {
            std::string strHeader = \
                          "daemon=1\n"
                          "txindex=1\n"
                          "rpcuser=" + gen_random(32) + "\n" +
                          "rpcpassword=" + gen_random(32) + "\n" +
                          "rpcbind=127.0.0.1\n"
                          "rpcallowip=127.0.0.1\n"
                          "prune=0\n";
            fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);
            fclose(configFile);
        }
        return true;
    }
    streamConfig.close();
    return true;
}

