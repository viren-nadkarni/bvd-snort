#ifndef ACSMX3_H
#define ACSMX3_H

#include <cstdint>
#include <vector>

#include "search_common.h"
#include "CL/cl.hpp"
#include "CL/cl_ext.h"
#include "main.h"

namespace snort
{
struct SnortConfig;
}

#define ALPHABET_SIZE    256
#define ACSM3_FAIL_STATE   (-1)

struct ACSM3_USERDATA
{
    void* id;
    uint32_t ref_count;
};

struct ACSM3_PATTERN
{
    ACSM3_PATTERN* next;
    ACSM3_USERDATA* udata;

    uint8_t* patrn;
    uint8_t* casepatrn;

    void* rule_option_tree;
    void* neg_list;

    int n;
    int nocase;
    int negative;
};

struct ACSM3_STATETABLE
{
    /* Next state - based on input character */
    int NextState[ ALPHABET_SIZE ];

    /* Failure state - used while building NFA & DFA  */
    int FailState;

    /* List of patterns that end here, if any */
    ACSM3_PATTERN* MatchList;
};

/*
* State machine Struct
*/
struct ACSM3_STRUCT
{
    int acsmMaxStates;
    int acsmNumStates;

    ACSM3_PATTERN* acsmPatterns;
    ACSM3_STATETABLE* acsmStateTable;

    int bcSize;
    short bcShift[256];

    int numPatterns;
    const MpseAgent* agent;

    char* packet_buffer;

    cl::Buffer cl_packet;
    cl::Buffer cl_state_table;
    cl::Buffer cl_packet_length;
    cl::Buffer cl_nfound;

    cl::Platform default_platform;
    cl::Device default_device;

    cl::Context context;
    cl::Program::Sources sources;
    cl::Program program;

    cl::CommandQueue queue;

    cl::Kernel kernel;
};

/*
*   Prototypes
*/
void acsm3_init_xlatcase();

ACSM3_STRUCT* acsm3New(const MpseAgent*);

int acsm3AddPattern(ACSM3_STRUCT* p, const uint8_t* pat, unsigned n,
    bool nocase, bool negative, void* id);

int acsm3Compile(snort::SnortConfig*, ACSM3_STRUCT*);

int acsm3Search(ACSM3_STRUCT * acsm, const uint8_t* T,
    int n, MpseMatch, void* context, int* current_state);

void acsm3Free(ACSM3_STRUCT* acsm);
int acsm3PatternCount(ACSM3_STRUCT* acsm);

int acsm3PrintDetailInfo(ACSM3_STRUCT*);

int acsm3PrintSummaryInfo();

void cl_printf_callback(const char *, size_t, size_t final, void *);

#endif

