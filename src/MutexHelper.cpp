#include "MutexHelper.h"
#include "init.h"
#include "timedata.h"
#include "constants.h"

bool CreateMutex(const CKey & bpnAddress, const CKey & changeAddress, uint256 & mutexTx)
{
    if(!pwalletMain)
        return false;
    
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev)
        return false;

    LOCK(pwalletMain->cs_wallet);
    
    CTransaction txNew;
    unsigned int nTxNewTime;
    if(!pwalletMain->CreateMutexTx(bpnAddress, changeAddress, txNew, nTxNewTime))
        return false;
    
    CBlock block;
    block.nVersion = HARDFORKV5_BLOCKVERSION;
    block.vtx.push_back(txNew);
    block.nTime = GetAdjustedTime();
    block.hashPrevBlock = pindexPrev->GetBlockHash();
    block.nNonce = 0;
    block.bpnAddress = bpnAddress.GetPubKey().GetID();
    block.nExtraFlag = BLOCK_HEADER_FLAG_MASK_MUTEX;
    block.vtx[0].UpdateHash();
    block.bpnTx = block.vtx[0].GetHash();
    block.nBits = block.bpnTx.GetCompact();
    block.hashMerkleRoot = block.BuildMerkleTree();
    block.kaTx = block.bpnTx;

    mutexTx = block.bpnTx;

    {
        LOCK(cs_main);
        if (block.hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("BullionMiner : generated block is stale");
    }

    pwalletMain->mapRequestCount[block.GetHash()] = 0;

    CValidationState state;
    return ProcessNewBlock(state, NULL, &block);
}

bool CheckMutex(const CBlock block, CBlockIndex* const pindexPrev)
{
    if(!pindexPrev || !block.IsMutex())
        return false;

    if(pindexPrev->IsMutex()) // You are not allowed to create two mutex in a row
        return error("CheckMutex(): Trying to create two mutexes in a row");

    if(pindexPrev->nTime >= block.nTime)
        return error("CheckMutex(): Invalid mutex time"); // no drift allowed

    if(block.vtx.size() != 1) // A mutex block can only contains it's own transaction
        return error("CheckMutex(): Block contains too much transactions");

    if(block.vtx[0].GetHash() != block.bpnTx)
        return error("CheckMutex(): Block BPN TX is not correct");

    CAmount totalOut = 0;
    CAmount outForBPNAddress = 0;
    for(unsigned int i = 0; i<block.vtx[0].vout.size(); ++i)
    {
        if(totalOut > REQUIRED_AMOUNT_MASTERNODE * COIN) // You can only output strict minimum to create the BPN
            return error("CheckMutex(): The transaction of the mutex try to output too much");
        
        totalOut += block.vtx[0].vout[i].nValue;

        CTxDestination address;
        CKeyID keyId;

        if (ExtractDestination(block.vtx[0].vout[i].scriptPubKey, address))
            if(CBitcoinAddress(address).GetKeyID(keyId) && (uint160) keyId == block.bpnAddress)
                outForBPNAddress += block.vtx[0].vout[i].nValue;
        
    }

    if(outForBPNAddress != REQUIRED_AMOUNT_MASTERNODE * COIN) // Not enough to create a BPN
        return error("CheckMutex(): Wrong amount in transaction to create a mutex");

    return true; // Congrats, it's a mutex !
}

bool IsMutexAlive(const uint64_t nTime, const CKeyID & bpnAddress, const uint256 & tx)
{
    if(mapKAMutex.find(make_pair((uint160) bpnAddress, tx)) == mapKAMutex.end()) // We are dead or never existed :(
        return false;

    if(mapKAMutex[make_pair((uint160) bpnAddress, tx)]->nTime > nTime) // This case should never happens
        return false;

    if(mapKAMutex[make_pair((uint160) bpnAddress, tx)]->nTime >= nTime-MUTEX_LIFE)
        return true;

    return false;
}

int GetBPNPlace(const int64_t nMutexLastUpdateTime, const int64_t nCurrentTime)
{
    int place = 1;

    if(nMutexLastUpdateTime < nCurrentTime-MUTEX_LIFE || nMutexLastUpdateTime > nCurrentTime)
        return -1;

    for(set<int64_t>::iterator rit=mapMutexIndex.begin(); rit!=mapMutexIndex.end(); ++rit)
        if(*rit >= nCurrentTime-MUTEX_LIFE && *rit <= nCurrentTime){
            
            if(*rit < nMutexLastUpdateTime)
                ++place;
            else
                return place;
        }
        else if(*rit > nCurrentTime)
            return place;

    return place;
}