// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2013-2018 The Bullion Foundation
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// clang-format off
#include "main.h"
#include "masternode-sync.h"
#include "util.h"
#include "addrman.h"
// clang-format on

class CMasternodeSync;
CMasternodeSync masternodeSync;

CMasternodeSync::CMasternodeSync()
{
    Reset();
}
// CBX: This function is needed; probably to be moved somewhere else
bool CMasternodeSync::IsBlockchainSynced()
{
    static bool fBlockchainSynced = false;
    static int64_t lastProcess = GetTime();

    // if the last call to this function was more than 6 * 60 minutes ago (client was in sleep mode) reset the sync process
    if (GetTime() - lastProcess > 6 * 60 * 60) {
        Reset();
        fBlockchainSynced = false;
    }
    lastProcess = GetTime();

    if (fBlockchainSynced) return true;

    if (fImporting || fReindex) return false;

    TRY_LOCK(cs_main, lockMain);
    if (!lockMain) return false;

    CBlockIndex* pindex = chainActive.Tip();
    if (pindex == NULL) return false;


    if (pindex->nTime + 6 * 60 * 60 < GetTime())
        return false;

    fBlockchainSynced = true;

    return true;
}

void CMasternodeSync::Reset()
{

}
