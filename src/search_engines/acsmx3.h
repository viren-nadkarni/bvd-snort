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

/* max theoretical packet size is 64k, but ethernet MTU is 1.5k
 * currently set to a safe size */
#define MAX_PACKET_SIZE 16*1024

/* how many packets can fit into the cache buffer before sent to GPU */
#define PACKET_BUFFER_SIZE 512

/* total number of work-items */
#define CL_GLOBAL_SIZE PACKET_BUFFER_SIZE

/* number of work-items in each work-group */
#define CL_LOCAL_SIZE 1

struct ACSM3_USERDATA
{
    void* id;
    uint32_t ref_count;

    uint64_t _padding0;
};

struct ACSM3_PATTERN
{
    ACSM3_PATTERN* next;        // 4 bytes
    void* _padding0;

    ACSM3_USERDATA* udata;      // 4
    void* _padding1;

    uint8_t* patrn;             // 4
    void* _padding2;

    uint8_t* casepatrn;         // 4
    void* _padding3;

    void* rule_option_tree;     // 4
    void* _padding4;

    void* neg_list;             // 4
    void* _padding5;

    uint32_t n;                 // 4
    uint32_t nocase;            // 4
    uint32_t negative;          // 4

    uint32_t _padding6;
};

struct ACSM3_STATETABLE
{
    /* Next state - based on input character */
    int NextState[ ALPHABET_SIZE ]; // 4*256=1024

    /* Failure state - used while building NFA & DFA  */
    int FailState;                  // 4

    uint32_t _padding0;             // 4

    /* List of patterns that end here, if any */
    ACSM3_PATTERN* MatchList;       // 4

    uint32_t _padding1;             // 4
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

    int packet_buffer_index;
    uint8_t* packet_buffer;
    int* packet_length_buffer;

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

void cl_dispatch(ACSM3_STRUCT*);
void cl_printf_callback(const char *, size_t, size_t final, void *);

#endif

