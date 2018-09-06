// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2013-2018 The Bullion Foundation
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include "primitives/transaction.h"
#include "keystore.h"
#include "serialize.h"
#include "uint256.h"
#include "constants.h"

#define BLOCK_HEADER_FLAG_MASK_MUTEX    0x0000000f
#define BLOCK_HEADER_FLAG_MASK_KA       0x000000f0
#define BLOCK_HEADER_FLAG_MASK_BPNPOSP  0x00000f00

/** The maximum allowed size for a serialized block, in bytes (network rule) */
static const unsigned int MAX_BLOCK_SIZE = 1000000;

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
public:
    // header
    static const int32_t CURRENT_VERSION=5;
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;
    uint32_t nExtraFlag;
    uint256 hashPrevKA;
    uint256 kaTx;
    uint160 bpnAddress;
    uint256 bpnTx;

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);

        if(this->nVersion >= HARDFORKV5_BLOCKVERSION)
        {
            READWRITE(nExtraFlag);
            READWRITE(hashPrevKA);
            READWRITE(kaTx);
            READWRITE(bpnAddress);
            READWRITE(bpnTx);
        }
    }

    void SetNull()
    {
        nVersion = CBlockHeader::CURRENT_VERSION;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        nExtraFlag = 0;
        hashPrevKA.SetNull();
        kaTx.SetNull();
        bpnAddress.SetNull();
        bpnTx.SetNull();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    bool IsMutex() const
    {
        return nExtraFlag & BLOCK_HEADER_FLAG_MASK_MUTEX;
    }

    bool IsKeepAlive() const
    {
        return nExtraFlag & BLOCK_HEADER_FLAG_MASK_KA;
    }

    bool IsBPNPoSP() const
    {
        return nExtraFlag & BLOCK_HEADER_FLAG_MASK_BPNPOSP;
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransaction> vtx;

    // ppcoin: block signature - signed by one of the coin base txout[N]'s owner
    std::vector<unsigned char> vchBlockSig;

    // memory only
    mutable CScript payee;
    mutable std::vector<uint256> vMerkleTree;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *((CBlockHeader*)this) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CBlockHeader*)this);
        // ConnectBlock depends on vtx following header to generate CDiskTxPos
        if (!(nType & (SER_GETHASH|SER_BLOCKHEADERONLY)))
        {
            READWRITE(vtx);
            READWRITE(vchBlockSig);
        }
        else if (ser_action.ForRead())
        {
            const_cast<CBlock*>(this)->vtx.clear();
            const_cast<CBlock*>(this)->vchBlockSig.clear();
        }
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        vMerkleTree.clear();
        payee = CScript();
        vchBlockSig.clear();
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion              = nVersion;
        block.hashPrevBlock         = hashPrevBlock;
        block.hashMerkleRoot        = hashMerkleRoot;
        block.nTime                 = nTime;
        block.nBits                 = nBits;
        block.nNonce                = nNonce;
        block.nExtraFlag            = nExtraFlag;
        block.hashPrevKA            = hashPrevKA;
        block.kaTx                  = kaTx;
        block.bpnAddress            = bpnAddress;
        block.bpnTx                 = bpnTx;

        return block;
    }

    // ppcoin: two types of block: proof-of-work or proof-of-stake
    bool IsProofOfStake() const
    {
        return (vtx.size() > 1 && vtx[1].IsCoinStake());
    }

    bool IsProofOfWork() const
    {
        return !IsProofOfStake() && !IsMutex();
    }

    unsigned int ComputeStakeEntropyBit(unsigned int nHeight) const
    {
        // Protocol switch to support p2pool at CryptoBullion block #9689
        if (nHeight >= 9689)
        {
            // Take last bit of block hash as entropy bit
            unsigned int nEntropyBit = ((GetHash().Get64()) & 1llu);
            if (fDebug && GetBoolArg("-printstakemodifier"))
                printf("GetStakeEntropyBit: nHeight=%u hashBlock=%s nEntropyBit=%u\n", nHeight, GetHash().ToString().c_str(), nEntropyBit);
            return nEntropyBit;
        }
        // Before CryptoBullion block #9689 - old protocol

        uint160 hashSig = Hash160(vchBlockSig);
        if (fDebug && GetBoolArg("-printstakemodifier"))
            printf("GetStakeEntropyBit: hashSig=%s, height=%d", hashSig.ToString().c_str(), nHeight);
        hashSig >>= 159; // take the first bit of the hash
        return hashSig.Get64();
    }

    bool SignBlock(const CKeyStore& keystore);
    bool CheckBlockSignature() const;

    std::pair<COutPoint, unsigned int> GetProofOfStake() const
    {
        return IsProofOfStake()? std::make_pair(vtx[1].vin[0].prevout, nTime) : std::make_pair(COutPoint(), (unsigned int)0);
    }

    // Build the in-memory merkle tree for this block and return the merkle root.
    // If non-NULL, *mutated is set to whether mutation was detected in the merkle
    // tree (a duplication of transactions in the block leading to an identical
    // merkle root).
    uint256 BuildMerkleTree(bool* mutated = NULL) const;

    std::vector<uint256> GetMerkleBranch(int nIndex) const;
    static uint256 CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex);
    std::string ToString() const;
    void print() const;
};


/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    CBlockLocator(const std::vector<uint256>& vHaveIn)
    {
        vHave = vHaveIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull()
    {
        return vHave.empty();
    }
};

#endif // BITCOIN_PRIMITIVES_BLOCK_H
