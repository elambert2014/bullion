// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2013-2018 The Bullion Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "random.h"
#include "util.h"
#include "utilstrencodings.h"
#include "bignum.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

/**
 * Main network
 */

//! Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress>& vSeedsOut, const SeedSpec6* data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    for (unsigned int i = 0; i < count; i++) {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

//   What makes a good checkpoint block?
// + Is surrounded by blocks with reasonable timestamps
//   (no blocks before with a timestamp after, none after with
//    timestamp before)
// + Contains no strange transactions
static Checkpoints::MapCheckpoints mapCheckpoints =
    boost::assign::map_list_of
    (95002, uint256("000000002005b8fcc4e971b14ef655535fde9285c913c52f3c7064d889cd6226"))
    (128580, uint256("0000000005bbf12779485036c7aa23d0c017182dabdbb58e86b66384e929fd2c"))
    (150002, uint256("00000000023d5687abb2208b1df44974a50c519f3a6f11c29b891dbe8cc84ebf"))
    (230000, uint256("00000000469a52e1ecddf74ebd18d034ae42122729687ecbf5e233562fdfd39f"))
    (320000, uint256("00000001d4ee504e892fd7597c0342079f8ec14a6bd7f0e38d77be78baa4442f"))
    (402000, uint256("00000002c544f0f8c83671bb5dd48b6ced70dc3db5f6588643b910c079689ce0"))
    (510000, uint256("00000000114e79134fa556d603c1e5230e122bd0a78f6192888362bd5091bd18"))
    (610000, uint256("0000000677b1841abad721ec3167b3c7b0463d53a22d78fbcd85c2eb53af56f2"))
    (730000, uint256("fde685b6116963359b221fc5235a6736f00d24c093bc4de5f3e0b0394a51c56a"))
    (850000, uint256("000000008b4658c03a8f13866ddb046227de8a96b3ee21078a828aa54d37e90d"))
    (950000, uint256("000000013d1baea87356546d2a145ab914ce3e150a1a815255c94467926b8afa"))
    (1000000, uint256("00000002c6f432ca95d5ea89b84d2bfd701e3253d8edef06d18fed7ec09de30c"))
    (1100000, uint256("000000015ac05d2bd89aba131cc9d57cdd7edda818320516cf2f346a2515b9be"))
    (1200000, uint256("000000007a41df6df9174de6483a00c6a70bfa1c2a383d723def0776b6e3601f"))
    (1302573, uint256("51050155f2159ecbaf3470725c50db7f34bb2a8590f1bb638474c6519e99e6ac")) // First PoSP block
    (1309160, uint256("97ba36ecbcd66d6894b2766aec2b74c128afb5070ed4ba2243ae36f996fbfca8"))
    (1310670, uint256("0abd54495d29078ea17efaae129c97224f6db03292bde04a905ca72bf1bd4547"))
    (1500000, uint256("81b9320ee44170d4b29b6eba1c6a0c82fa8aa613ba5e097523f91bbc7b094e30"))
    (1700000, uint256("77616ab999a81c3a68cd9d39e5528a76dfab06bcacad11cef3b1004d2210e3e1"))
    (2000000, uint256("82800b5d862bae5238142c97ff2fd930e3add1289d42c017220252098af1c305"))
    (2200000, uint256("bdd4eb12b3ae0e6d9079f52ffc981527022eb39acd79333cd1fd42ac22fbca9f"))
    (2301066, uint256("2329851bd87353a15f9893d7cef6248484e695d04e31651ede2245ab183d2060"))
    (2318178, uint256("fc3c5cd0ee81c62e1166c887d79977f040f2620ce189812f7aa0b701217f3441"))
    (2319474, uint256("d19281d0ae3644a487cb3b339a0e8f29093df69e26de068d38d148eb996bcbc5"))
    ;

static const Checkpoints::CCheckpointData data = {
    &mapCheckpoints,
    1520786127, // * UNIX timestamp of last checkpoint block
    3568491,    // * total number of transactions between genesis and last checkpoint
                //   (the tx=... number in the SetBestChain debug.log lines)
    3000        // * estimated number of transactions per day after checkpoint
};

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
    boost::assign::map_list_of(0, uint256("0x001"));
static const Checkpoints::CCheckpointData dataTestnet = {
    &mapCheckpointsTestnet,
    1372351925,
    0,
    250};

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
    boost::assign::map_list_of(0, uint256("0x001"));
static const Checkpoints::CCheckpointData dataRegtest = {
    &mapCheckpointsRegtest,
    1372351925,
    0,
    100};

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        pchMessageStart[0] = 0xe4;
        pchMessageStart[1] = 0xe8;
        pchMessageStart[2] = 0xe9;
        pchMessageStart[3] = 0xe5;
        vAlertPubKey = ParseHex("0335c9262fc75c3e251f610ecfbb1afaf847b6f0214582034fe047b85262034fa5");
        nDefaultPort = 7695;
        nSubsidyHalvingInterval = 210000;
        nMaxReorganizationDepth = 100;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nTargetTimespan = 0.16 * 24 * 60 * 60;
        nTargetSpacing = 65;
        nMaturity = 5;
        nMasternodeCountDrift = 20;
        nModifierUpdateBlock = 615800;
        bnProofOfWorkLimit = ~uint256(0) >> 20;

        //CBlock(hash=0000068e0b99f3db472b, ver=1, hashPrevBlock=00000000000000000000,
        // hashMerkleRoot=ea6fed5e25, nTime=1368496587, nBits=1e0fffff, nNonce=578618, vtx=1, vchBlockSig=)
        //  Coinbase(hash=ea6fed5e25, nTime=1368496567, ver=1, vin.size=1, vout.size=1, nLockTime=0)
        //    CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001d020f2706787878787878)
        //    CTxOut(empty)
        //vMerkleTree: ea6fed5e2
        const char* pszTimestamp = "San Francisco Gets Ready to Marry ‘Anyone Who Wants To’ -Jun 26, 2013 10:01 PM MT";
        CTransaction txNew;
        txNew.nTime = 1372351910;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        //txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        //txNew.vout[0].scriptPubKey = CScript() << ParseHex("04c10e83b2703ccf322f7dbd62dd5855ac7c10bd055814ce121ba32607d573b8810c02c0582aed05b4deb9c4b77b26d92428c61256cd42774babea0a073b2ed0c9") << OP_CHECKSIG;
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(9999) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].SetEmpty();
        //txNew.nVersion = 1;
        txNew.UpdateHash();
        genesis.vtx.push_back(txNew);
        txNew.UpdateHash();
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime = 1372351925;

        CBigNum tmpnBits(~uint256(0) >> 20);

        genesis.nBits = 0x1e0fffff;
        genesis.nNonce = 173202;

        hashGenesisBlock = genesis.GetHash();

        if (false  && (genesis.GetHash() != uint256("0x00000536d98b9b6401186fb1980a14b7fe01ae4ebf748ceaf3eae3dfb299af16")))
       {
           // This will figure out a valid hash and Nonce if you're
           // creating a different genesis block:
           uint256 hashTarget = CBigNum().SetCompact(genesis.nBits).getuint256();
           while (genesis.GetHash() > hashTarget)
           {
               ++genesis.nNonce;
               if (genesis.nNonce == 0)
               {
                   printf("NONCE WRAPPED, incrementing time");
                   ++genesis.nTime;
               }
           }
       }

        //// debug print
        /*genesis.print();
        printf("block.GetHash() == %s\n", genesis.GetHash().ToString().c_str());
        printf("block.hashMerkleRoot == %s\n", genesis.hashMerkleRoot.ToString().c_str());
        printf("block.nTime = %u \n", genesis.nTime);
        printf("block.nNonce = %u \n", genesis.nNonce);
        printf("txNew.GetHash() = %s\n", txNew.GetHash().ToString().c_str());*/

        assert(hashGenesisBlock == uint256("0x000002655a721555160ea9bcb1072faf63ff0c8dffed173d60b1d3556a134dc1"));

        vFixedSeeds.clear();
        vSeeds.clear();
        /*vSeeds.push_back(CDNSSeedData("bullionseeds.pw", "test.seed.bullionseeds.pw"));*/

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,11);      // Mainnet CBX addresses start with '5'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,8);       // Mainnet CBX script addresses start with ??
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1,128 + 11);    // Mainnet CBX keys start with 'M'
        // Mainnet CBX BIP32 pubkeys start with 'cbxP'
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0xa7)(0xb5)(0x24)(0x74).convert_to_container<std::vector<unsigned char> >();
        // Mainnet CBX BIP32 prvkeys start with 'cbxV'
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0xa7)(0xb5)(0x2c)(0xa2).convert_to_container<std::vector<unsigned char> >();
        // 	BIP44 coin type is from https://github.com/satoshilabs/slips/blob/master/slip-0044.md
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x00)(0xcb)(0xcb).convert_to_container<std::vector<unsigned char> >();

        //convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fSkipProofOfWorkCheck = false;
        fTestnetToBeDeprecatedFieldRPC = false;
        fHeadersFirstSyncingActive = true;

        nPoolMaxTransactions = 3;
        nStartMasternodePayments = 9005253600 ; // 1st september, 12:00 GMT
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return data;
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams
{
public:
    CTestNetParams()
    {
        networkID = CBaseChainParams::TESTNET;
        strNetworkID = "test";
        pchMessageStart[0] = 0x45;
        pchMessageStart[1] = 0x76;
        pchMessageStart[2] = 0x65;
        pchMessageStart[3] = 0xba;
        vAlertPubKey = ParseHex("000010e83b2703ccf322f7dbd62dd5855ac7c10bd055814ce121ba32607d573b8810c02c0582aed05b4deb9c4b77b26d92428c61256cd42774babea0a073b2ed0c9");
        nDefaultPort = 51474;
        nEnforceBlockUpgradeMajority = 51;
        nRejectBlockOutdatedMajority = 75;
        nToCheckBlockUpgradeMajority = 100;
        nTargetTimespan = 0.16 * 24 * 60 * 60;
        nTargetSpacing = 65;
        nMaturity = 5;
        nModifierUpdateBlock = 51197; //approx Mon, 17 Apr 2017 04:00:00 GMT

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1372351925;
        genesis.nNonce = 2402015;

        hashGenesisBlock = genesis.GetHash();
       // assert(hashGenesisBlock == uint256("0x0000041e482b9b9691d98eefb48473405c0b8ec31b76df3797c74a78680ef818"));

        vFixedSeeds.clear();
        vSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 111);    // Testnet CBX addresses start with 'x' or y'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 196);    // Testnet CBX script addresses start with ??
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 128 + 111);  // Testnet CBX keys start with ??
        // Testnet CBX BIP32 pubkeys start with 'tbxP'
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0xf3)(0x13)(0x7c)(0x80).convert_to_container<std::vector<unsigned char> >();
        // Testnet CBX BIP32 prvkeys start with 'tbxV'
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0xf3)(0x13)(0x86)(0x0a).convert_to_container<std::vector<unsigned char> >();
        // Testnet CBX BIP44 coin type is '1' (All coin's testnet default)
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x00)(0x00)(0x01).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        nPoolMaxTransactions = 2;
        nStartMasternodePayments = 1420837558; //Fri, 09 Jan 2015 21:05:58 GMT
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataTestnet;
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams
{
public:
    CRegTestParams()
    {
        networkID = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";
        strNetworkID = "regtest";
        pchMessageStart[0] = 0xa1;
        pchMessageStart[1] = 0xcf;
        pchMessageStart[2] = 0x7e;
        pchMessageStart[3] = 0xac;
        nSubsidyHalvingInterval = 150;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nTargetTimespan = 0.16 * 24 * 60 * 60;
        nTargetSpacing = 65;
        bnProofOfWorkLimit = ~uint256(0) >> 1;
        genesis.nTime = 1372351925;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = 12345;

        hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 51476;
       // assert(hashGenesisBlock == uint256("0x4f023a2120d9127b21bbad01724fdb79b519f593f2a85b60d3d79160ec5f29df"));

        vFixedSeeds.clear(); //! Testnet mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Testnet mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataRegtest;
    }
};
static CRegTestParams regTestParams;

/**
 * Unit test
 */
class CUnitTestParams : public CMainParams, public CModifiableParams
{
public:
    CUnitTestParams()
    {
        networkID = CBaseChainParams::UNITTEST;
        strNetworkID = "unittest";
        nDefaultPort = 51478;
        vFixedSeeds.clear(); //! Unit test mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Unit test mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fAllowMinDifficultyBlocks = false;
        fMineBlocksOnDemand = true;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        // UnitTest share the same checkpoints as MAIN
        return data;
    }

    //! Published setters to allow changing values in unit test cases
    virtual void setSubsidyHalvingInterval(int anSubsidyHalvingInterval) { nSubsidyHalvingInterval = anSubsidyHalvingInterval; }
    virtual void setEnforceBlockUpgradeMajority(int anEnforceBlockUpgradeMajority) { nEnforceBlockUpgradeMajority = anEnforceBlockUpgradeMajority; }
    virtual void setRejectBlockOutdatedMajority(int anRejectBlockOutdatedMajority) { nRejectBlockOutdatedMajority = anRejectBlockOutdatedMajority; }
    virtual void setToCheckBlockUpgradeMajority(int anToCheckBlockUpgradeMajority) { nToCheckBlockUpgradeMajority = anToCheckBlockUpgradeMajority; }
    virtual void setDefaultConsistencyChecks(bool afDefaultConsistencyChecks) { fDefaultConsistencyChecks = afDefaultConsistencyChecks; }
    virtual void setAllowMinDifficultyBlocks(bool afAllowMinDifficultyBlocks) { fAllowMinDifficultyBlocks = afAllowMinDifficultyBlocks; }
    virtual void setSkipProofOfWorkCheck(bool afSkipProofOfWorkCheck) { fSkipProofOfWorkCheck = afSkipProofOfWorkCheck; }
};
static CUnitTestParams unitTestParams;


static CChainParams* pCurrentParams = 0;

CModifiableParams* ModifiableParams()
{
    assert(pCurrentParams);
    assert(pCurrentParams == &unitTestParams);
    return (CModifiableParams*)&unitTestParams;
}

const CChainParams& Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(CBaseChainParams::Network network)
{
    switch (network) {
    case CBaseChainParams::MAIN:
        return mainParams;
    case CBaseChainParams::TESTNET:
        return testNetParams;
    case CBaseChainParams::REGTEST:
        return regTestParams;
    case CBaseChainParams::UNITTEST:
        return unitTestParams;
    default:
        assert(false && "Unimplemented network");
        return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}
