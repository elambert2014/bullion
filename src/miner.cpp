// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2013-2018 The Bullion Foundation
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "hash.h"
#include "main.h"
#include "masternode-sync.h"
#include "net.h"
#include "pow.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "MutexHelper.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// BullionMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.
// The COrphan class keeps track of these 'temporary orphans' while
// CreateBlock is figuring out which transactions to include.
//
class COrphan
{
public:
    const CTransaction* ptx;
    set<uint256> setDependsOn;
    CFeeRate feeRate;
    double dPriority;

    COrphan(const CTransaction* ptxIn) : ptx(ptxIn), feeRate(0), dPriority(0)
    {
    }
};

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;
int64_t nLastCoinStakeSearchInterval = 0;

// We want to sort transactions by priority and fee rate, so:
typedef boost::tuple<double, CFeeRate, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;

public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) {}

    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee) {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        } else {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

void UpdateTime(CBlockHeader* pblock, const CBlockIndex* pindexPrev)
{
    pblock->nTime = std::max(pindexPrev->GetMedianTimePast() + 1, GetAdjustedTime());
}

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn, CWallet* pwallet, const std::pair<const CWalletTx*, unsigned int> * coin, bool fKeepAlife)
{
    CReserveKey reservekey(pwallet);

    // Create new block
    auto_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if (!pblocktemplate.get())
        return NULL;
    CBlock* pblock = &pblocktemplate->block; // pointer for convenience
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (Params().MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    if(chainActive.Tip()->nHeight >= HARDFORK_HEIGHTV5)
        pblock->nVersion = HARDFORKV5_BLOCKVERSION;

    if(coin)
        pblock->nExtraFlag = fKeepAlife ? BLOCK_HEADER_FLAG_MASK_KA : BLOCK_HEADER_FLAG_MASK_BPNPOSP;

    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    
    txNew.vout[0].SetEmpty();

    // Add our coinbase tx as first transaction
    pblock->vtx.push_back(txNew);
    pblocktemplate->vTxFees.push_back(-1);   // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // if coinstake available add coinstake tx
    static int64_t nLastCoinStakeSearchTime[] = {GetAdjustedTime(), GetAdjustedTime()}; // only initialized at startup

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", MAX_BLOCK_SIZE_GEN / 2);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE - 1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Fee-per-kilobyte amount considered the same as "free"
    // Be careful setting this: if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    //CAmount nMinTxFee = CWallet::minTxFee.GetFeePerK();

    // Collect memory pool transactions into the block
    CAmount nFees = 0;

    {
        LOCK2(cs_main, mempool.cs);

        CBlockIndex* pindexPrev = chainActive.Tip();
        const int nHeight = pindexPrev->nHeight + 1;
        CCoinsViewCache view(pcoinsTip);


        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        bool fPrintPriority = GetBoolArg("-printpriority", false);

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (map<uint256, CTxMemPoolEntry>::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi) {
            const CTransaction& tx = mi->second.GetTx();
            if (tx.IsCoinBase() || tx.IsCoinStake() || !IsFinalTx(tx, nHeight))
                continue;

            COrphan* porphan = NULL;
            double dPriority = 0;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;
            BOOST_FOREACH (const CTxIn& txin, tx.vin) {
                // Read prev transaction
                if (!view.HaveCoins(txin.prevout.hash)) {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash)) {
                        LogPrintf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan) {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].GetTx().vout[txin.prevout.n].nValue;
                    continue;
                }
                const CCoins* coins = view.AccessCoins(txin.prevout.hash);
                assert(coins);

                CAmount nValueIn = coins->vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = nHeight - coins->nHeight;

                dPriority += (double)nValueIn * nConf;
            }
            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

            uint256 hash = tx.GetHash();
            mempool.ApplyDeltas(hash, dPriority, nTotalIn);

            CFeeRate feeRate(nTotalIn - tx.GetValueOut(), nTxSize);

            if (porphan) {
                porphan->dPriority = dPriority;
                porphan->feeRate = feeRate;
            } else
                vecPriority.push_back(TxPriority(dPriority, feeRate, &mi->second.GetTx()));
        }


        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        while (!vecPriority.empty()) {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            CFeeRate feeRate = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            CAmount nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
            if (fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || !AllowFree(dPriority))) {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            if (!view.HaveInputs(tx))
                continue;

            CAmount nTxFees = view.GetValueIn(tx) - tx.GetValueOut();

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true))
                continue;

            CTxUndo txundo;
            UpdateCoins(tx, state, view, txundo, nHeight);

            // Added
            tx.UpdateHash();
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fPrintPriority) {
                LogPrintf("priority %.1f fee %s txid %s\n",
                    dPriority, feeRate.ToString(), tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash)) {
                BOOST_FOREACH (COrphan* porphan, mapDependers[hash]) {
                    if (!porphan->setDependsOn.empty()) {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty()) {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->feeRate, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        boost::this_thread::interruption_point();
        pblock->nTime = GetAdjustedTime();
        pindexPrev = chainActive.Tip();
        if(!coin || fKeepAlife)
            pblock->nBits = GetNextTargetRequired(pindexPrev, true);
        else
            pblock->nBits = GetNextTargetRequiredBPN(pindexPrev);
        
        CMutableTransaction txCoinStake;
        int64_t nSearchTime = pblock->nTime; // search to current time
        bool fStakeFound = false;
        uint256 finalCoinStakeHash = 0;
        
        if (nSearchTime >= nLastCoinStakeSearchTime[coin ? 1 : 0]) {
            unsigned int nTxNewTime = 0;
            if ((!coin && pwallet->CreateCoinStake(*pwallet, pblock->nBits, nSearchTime - nLastCoinStakeSearchTime[0], txCoinStake, nTxNewTime, nFees))
                || (coin && pwallet->CreateBPNCoinStake(*coin, *pwallet, pblock->nBits, nSearchTime - nLastCoinStakeSearchTime[1], txCoinStake, nTxNewTime, nFees, fKeepAlife))) {
                // Make sure coinstake would meet timestamp protocol
                // as it would be the same as the block timestamp
                pblock->nTime = pblock->vtx[0].nTime = nTxNewTime;
                pblock->vtx[0].vout[0].SetEmpty();
                CTransaction txFinalCoinStake(txCoinStake);
                txFinalCoinStake.nTime = nTxNewTime;
                txFinalCoinStake.UpdateHash();
                finalCoinStakeHash = txFinalCoinStake.GetHash();

                // We have to make sure that we have no future timestamps in
                // our transactions set
                for (vector<CTransaction>::iterator it = pblock->vtx.begin(); it != pblock->vtx.end();)
                    if (it->nTime > nTxNewTime) { it = pblock->vtx.erase(it); } else { ++it; }

                // Coinstake must be in position 1
                pblock->vtx.insert(pblock->vtx.begin() + 1, txFinalCoinStake);
                fStakeFound = true;
            }
            nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime[coin ? 1 : 0];
            nLastCoinStakeSearchTime[coin ? 1 : 0] = nSearchTime;
        }

        if (!fStakeFound){
            return NULL;
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("CreateNewBlock(): total size %u\n", nBlockSize);


        // Compute final coinbase transaction.
        pblock->vtx[0].vin[0].scriptSig = CScript() << nHeight << OP_0;

        // Fill in header
        pblock->hashPrevBlock = pindexPrev->GetBlockHash();
        pblock->nNonce = 0;
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

        if(coin) { // Fill the block information regarding BPN PoSP / KA
            pblock->hashPrevKA = coin->first->hashBlock;
            pblock->kaTx = finalCoinStakeHash;

            CBlockIndex* pindex = mapBlockIndex.find(coin->first->hashBlock)->second;
            pblock->bpnAddress = pindex->bpnAddress;
            pblock->bpnTx = pindex->bpnTx;
        }

        CValidationState state;
        if (!TestBlockValidity(state, *pblock, pindexPrev, false, false)) {
            LogPrintf("CreateNewBlock() : TestBlockValidity failed\n");
            return NULL;
        }
    }

    return pblocktemplate.release();
}

void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock) {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight + 1; // Height first in coinbase required for block.version=2
    pblock->vtx[0].vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(pblock->vtx[0].vin[0].scriptSig.size() <= 100);

    pblock->vtx[0].UpdateHash();
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}

#ifdef ENABLE_WALLET
//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey, CWallet* pwallet, const std::pair<const CWalletTx*, unsigned int> * coin, bool fKeepAlife)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
    {
        return NULL;
    }

    CScript scriptPubKey = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKey, pwallet, coin, fKeepAlife);
}

bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("BullionMiner : generated block is stale");
    }

    // Remove key from key pool if used (CBX PoS blocks use originating address key)
    if (!pblock->IsProofOfStake())
        reservekey.KeepKey();

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash()] = 0;
    }

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, NULL, pblock))
        return error("BullionMiner : ProcessNewBlock, block not accepted");

    return true;
}

void BitcoinMiner(CWallet* pwallet, bool fProofOfStake)
{
    LogPrintf("BullionMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("bullion-miner");

    // Each thread has its own key and counter
    CReserveKey reservekey(pwallet);
    unsigned int nExtraNonce = 0;

    //control the amount of times the client will check for mintable coins
    static bool fMintableCoins = false;
    static int nMintableLastCheck = 0;

    while (fProofOfStake) {
        if (fProofOfStake) {

            if (GetTime() - nMintableLastCheck > 1 * 60) // 1 minute check time
            {
                nMintableLastCheck = GetTime();
                fMintableCoins = pwallet->MintableCoins();
            }

            if (vNodes.empty() || pwallet->IsLocked() || !fMintableCoins || nReserveBalance >= pwallet->GetBalance() || !masternodeSync.IsBlockchainSynced()) {
                nLastCoinStakeSearchInterval = 0;
                MilliSleep(5000);
                continue;
            }

            if (mapHashedBlocks.count(chainActive.Tip()->nHeight)) //search our map of hashed blocks, see if bestblock has been hashed yet
            {
                if (GetTime() - mapHashedBlocks[chainActive.Tip()->nHeight] < max(pwallet->nHashInterval, (unsigned int)1)) // wait half of the nHashDrift with max wait of 3 minutes
                {
                    MilliSleep(1000);
                    continue;
                }
            }

            MilliSleep(50); // Prevent any odd situation (e.g. time violation) from hogging the CPU
        }

        //
        // Create new block
        //
        CBlockIndex* pindexPrev = chainActive.Tip();
        if (!pindexPrev)
            continue;

        auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey, pwallet));
        if (!pblocktemplate.get())
            continue;

        CBlock* pblock = &pblocktemplate->block;
        IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

        //Stake miner main
        if (fProofOfStake) {
            LogPrintf("CPUMiner : proof-of-stake block found %s \n", pblock->GetHash().ToString().c_str());

            if (!pblock->SignBlock(*pwallet)) {
                LogPrintf("BitcoinMiner(): Signing new block failed \n");
                continue;
            }

            LogPrintf("CPUMiner : proof-of-stake block was signed %s \n", pblock->GetHash().ToString().c_str());
            SetThreadPriority(THREAD_PRIORITY_NORMAL);
            ProcessBlockFound(pblock, *pwallet, reservekey);
            SetThreadPriority(THREAD_PRIORITY_LOWEST);

            continue;
        }
    }
}

void BPNMiner(CWallet* pwallet)
{
    LogPrintf("BPNMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("bullion-bpnminer");

    // Each thread has its own key and counter
    CReserveKey reservekey(pwallet);
    unsigned int nExtraNonce = 0;

    //control the amount of times the client will check for mintable coins
    static bool fMintableCoins = false;
    static int nMintableLastCheck = 0;
    std::set<std::pair<const CWalletTx*, unsigned int> > setCoins;

    while (!false) { // funny because it's true

        if (GetTime() - nMintableLastCheck > 5 * 60) // 5 minutes check time
        {
            setCoins.clear();
            nMintableLastCheck = GetTime();
            pwallet->SelectBPNStakeCoins(setCoins);
            fMintableCoins = !setCoins.empty();
        }

        if (vNodes.empty() || pwallet->IsLocked() || !fMintableCoins || !masternodeSync.IsBlockchainSynced()) {
            nLastCoinStakeSearchInterval = 0;
            MilliSleep(5000);
            continue;
        }

        if (mapHashedBlocks.count(chainActive.Tip()->nHeight)) //search our map of hashed blocks, see if bestblock has been hashed yet
        {
            if (GetTime() - mapHashedBlocks[chainActive.Tip()->nHeight] < max(pwallet->nHashInterval, (unsigned int)1)) // wait half of the nHashDrift with max wait of 3 minutes
            {
                MilliSleep(1000);
                continue;
            }
        }

        for(std::set<std::pair<const CWalletTx*, unsigned int> >::iterator it=setCoins.begin(); it != setCoins.end(); ++it)
        {
            CTxDestination ctxDest;
            CKeyID keyId;
            const std::pair<const CWalletTx*, unsigned int> coin = *it;
            uint256 txHash = coin.first->GetHash();
            ExtractDestination(coin.first->vout[0].scriptPubKey, ctxDest); // The vout[0] should always give the correct BPN address
            CBitcoinAddress(ctxDest).GetKeyID(keyId);

            if(mapKAMutex.empty())
                continue;

            if(ExtractDestination(coin.first->vout[0].scriptPubKey, ctxDest) // The vout[0] should always give the correct BPN address
                    && CBitcoinAddress(ctxDest).GetKeyID(keyId)
                    && mapKAMutex.find(make_pair((uint160) keyId, txHash)) != mapKAMutex.end()
                    && IsMutexAlive(mapKAMutex[make_pair((uint160) keyId, txHash)]->nTime, keyId, txHash))
            {
                bool fKeepAlife = GetBPNPlace(mapKAMutex[make_pair((uint160) keyId, txHash)]->nTime, GetAdjustedTime()) > MAX_MASTERNODE;
                
                MilliSleep(100); // Prevent any odd situation (e.g. time violation) from hogging the CPU
                //
                // Create new block
                //
                CBlockIndex* pindexPrev = chainActive.Tip();
                if (!pindexPrev)
                    continue;

                auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey, pwallet, &coin, fKeepAlife));
                if (!pblocktemplate.get())
                    continue;

                CBlock* pblock = &pblocktemplate->block;
                IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

                //Stake miner main
                LogPrintf("BPNMiner : YAY - block found %s \n", pblock->GetHash().ToString().c_str());

                if (!pblock->SignBlock(*pwallet)) {
                    LogPrintf("BPNMiner(): Signing new block failed \n");
                    continue;
                }

                LogPrintf("BPNMiner : - block was signed %s \n", pblock->GetHash().ToString().c_str());
                SetThreadPriority(THREAD_PRIORITY_NORMAL);
                ProcessBlockFound(pblock, *pwallet, reservekey);
                SetThreadPriority(THREAD_PRIORITY_LOWEST);
            }

        }
    }
    
}

#endif // ENABLE_WALLET
