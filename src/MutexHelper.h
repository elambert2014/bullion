// Copyright (c) 2013-2018 The Bullion Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef CBX_MUTEXHELPER
#define CBX_MUTEXHELPER

#include "wallet.h"

bool CreateMutex(const CKey & bpnAddress, const CKey & changeAddress, uint256 & mutexTx);
bool CheckMutex(const CBlock block, CBlockIndex* const pindexPrev);
// TX could be the original mutex tx or any KA / BPN PoSP tx
bool IsMutexAlive(const uint64_t nTime, const CKeyID & bpnAddress, const uint256 & tx);

int GetBPNPlace(const int64_t nMutexLastUpdateTime, const int64_t nCurrentTime); // Return the BPN place given a mutex last update time, it does NOT check if mutex is valid !!

#endif