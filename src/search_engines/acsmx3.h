#ifndef ACSMX3_H
#define ACSMX3_H

#include <cstdint>

#include "search_common.h"

namespace snort
{
struct SnortConfig;
}

#define ALPHABET_SIZE    256
#define ACSM_FAIL_STATE   (-1)

struct ACSM_USERDATA
{
    void* id;
    uint32_t ref_count;
};

struct ACSM_PATTERN
{
    ACSM_PATTERN* next;
    ACSM_USERDATA* udata;

    uint8_t* patrn;
    uint8_t* casepatrn;

    void* rule_option_tree;
    void* neg_list;

    int n;
    int nocase;
    int negative;
};

struct ACSM_STATETABLE
{
    /* Next state - based on input character */
    int NextState[ ALPHABET_SIZE ];

    /* Failure state - used while building NFA & DFA  */
    int FailState;

    /* List of patterns that end here, if any */
    ACSM_PATTERN* MatchList;
};

/*
* State machine Struct
*/
struct ACSM_STRUCT
{
    int acsmMaxStates;
    int acsmNumStates;

    ACSM_PATTERN* acsmPatterns;
    ACSM_STATETABLE* acsmStateTable;

    int bcSize;
    short bcShift[256];

    int numPatterns;
    const MpseAgent* agent;
};

/*
*   Prototypes
*/
void acsm3_init_xlatcase();

ACSM_STRUCT* acsm3New(const MpseAgent*);

int acsm3AddPattern(ACSM_STRUCT* p, const uint8_t* pat, unsigned n,
    bool nocase, bool negative, void* id);

int acsm3Compile(snort::SnortConfig*, ACSM_STRUCT*);

int acsm3Search(ACSM_STRUCT * acsm, const uint8_t* T,
    int n, MpseMatch, void* context, int* current_state);

void acsm3Free(ACSM_STRUCT* acsm);
int acsm3PatternCount(ACSM_STRUCT* acsm);

int acsm3PrintDetailInfo(ACSM_STRUCT*);

int acsm3PrintSummaryInfo();

#endif

