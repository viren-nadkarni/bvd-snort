#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "acsmx3.h"

#include <iostream>
#include <fstream>
#include <list>

#include "main/thread.h"
#include "utils/util.h"

#define MEMASSERT(p,s) if (!(p)) { fprintf(stderr,"ACSM-No Memory: %s\n",s); exit(0); }

#define BUFFER_SIZE 256 /* max number of packets in the buffer */
#define MAX_PACKET_SIZE 64*1024
/* tests run on smallFlows gave:
 * max: 1460 bytes
 * mean: 1106.87
 * min: 2
 * count: 7569 packets
 * besides, ethernet has mtu of 1500 bytes
 */
#define CL_GLOBAL_SIZE 256 /* total number of work-items */
#define CL_LOCAL_SIZE 32 /* number of work-items in each work-group */

static int max_memory = 0;

static void* AC3_MALLOC(int n)
{
    void* p = snort_calloc(n);
    max_memory += n;
    return p;
}

static void AC3_FREE(void* p)
{
    if (p)
        snort_free(p);
}

/*
 * Case Translation Table
 */
static uint8_t xlatcase[256];

/* Initialises lookup table for case conversion. Called by ac_gpu.c:ac_init()
 * Faster than calling toupper() everytime
 */
void acsm3_init_xlatcase()
{
    int i;
    for (i = 0; i < 256; i++)
    {
        xlatcase[i] = (uint8_t)toupper (i);
    }
}

/* Convert case using the case lookup table
 */
static inline void ConvertCaseEx(uint8_t* d, const uint8_t* s, int m)
{
    int i;
    for (i = 0; i < m; i++)
    {
        d[i] = xlatcase[s[i]];
    }
}

/*
 *
 */
static ACSM3_PATTERN* CopyMatchListEntry(ACSM3_PATTERN* px)
{
    ACSM3_PATTERN* p;

    p = (ACSM3_PATTERN*)AC3_MALLOC(sizeof(ACSM3_PATTERN));
    MEMASSERT(p, "CopyMatchListEntry");

    memcpy(p, px, sizeof (ACSM3_PATTERN));
    px->udata->ref_count++;
    p->next = nullptr;

    return p;
}

/*
 * Add a pattern to the list of patterns terminated at this state.
 * Insert at front of list.
 */
static void AddMatchListEntry(ACSM3_STRUCT* acsm, int state, ACSM3_PATTERN* px)
{
    ACSM3_PATTERN* p;

    p = (ACSM3_PATTERN*)AC3_MALLOC(sizeof(ACSM3_PATTERN));
    MEMASSERT(p, "AddMatchListEntry");

    memcpy(p, px, sizeof (ACSM3_PATTERN));
    p->next = acsm->acsmStateTable[state].MatchList;
    acsm->acsmStateTable[state].MatchList = p;
}

/*
 * Add Pattern States
 */
static void AddPatternStates(ACSM3_STRUCT* acsm, ACSM3_PATTERN* p)
{
    int state = 0;
    int n = p->n;
    uint8_t* pattern = p->patrn;

    /*
     *  Match up pattern with existing states
     */
    for (; n > 0; pattern++, n--) {
        int next = acsm->acsmStateTable[state].NextState[*pattern];
        if (next == ACSM3_FAIL_STATE)
            break;
        state = next;
    }

    /*
     *   Add new states for the rest of the pattern bytes, 1 state per byte
     */
    for (; n > 0; pattern++, n--) {
        acsm->acsmNumStates++;
        acsm->acsmStateTable[state].NextState[*pattern] = acsm->acsmNumStates;
        state = acsm->acsmNumStates;
    }

    AddMatchListEntry (acsm, state, p);
}

/*
 * Build Non-Deterministic Finite Automata
 */
static void Build_NFA(ACSM3_STRUCT* acsm)
{
    ACSM3_PATTERN* mlist=nullptr;
    ACSM3_PATTERN* px=nullptr;

    std::list<int> queue;

    /* Add the state 0 transitions 1st */
    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        int s = acsm->acsmStateTable[0].NextState[i];

        if (s)
        {
            queue.push_back(s);
            acsm->acsmStateTable[s].FailState = 0;
        }
    }

    /* Build the fail state transitions for each valid state */
    for ( auto r : queue )
    {
        /* Find Final States for any Failure */
        for (int i = 0; i < ALPHABET_SIZE; i++)
        {
            int s = acsm->acsmStateTable[r].NextState[i];

            if ( s != ACSM3_FAIL_STATE )
            {
                queue.push_back(s);
                int fs = acsm->acsmStateTable[r].FailState;
                int next;

                /*
                 *  Locate the next valid state for 'i' starting at s
                 */
                while ((next=acsm->acsmStateTable[fs].NextState[i]) ==
                    ACSM3_FAIL_STATE)
                {
                    fs = acsm->acsmStateTable[fs].FailState;
                }

                /*
                 *  Update 's' state failure state to point to the next valid state
                 */
                acsm->acsmStateTable[s].FailState = next;

                /*
                 *  Copy 'next' states MatchList to 's' states MatchList,
                 *  we copy them so each list can be AC3_FREE'd later,
                 *  else we could just manipulate pointers to fake the copy.
                 */
                for (mlist = acsm->acsmStateTable[next].MatchList;
                        mlist != nullptr;
                        mlist = mlist->next)
                {
                    px = CopyMatchListEntry (mlist);
                    /* Insert at front of MatchList */
                    px->next = acsm->acsmStateTable[s].MatchList;
                    acsm->acsmStateTable[s].MatchList = px;
                }
            }
        }
    }
}

/*
*   Build Deterministic Finite Automata from NFA
*/
static void Convert_NFA_To_DFA(ACSM3_STRUCT* acsm)
{
    std::list<int> queue;

    /* Add the state 0 transitions 1st */
    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        if ( int s = acsm->acsmStateTable[0].NextState[i] )
            queue.push_back(s);
    }

    /* Start building the next layer of transitions */
    for ( auto r : queue )
    {
        /* State is a branch state */
        for (int i = 0; i < ALPHABET_SIZE; i++)
        {
            int s = acsm->acsmStateTable[r].NextState[i];

            if ( s != ACSM3_FAIL_STATE )
            {
                queue.push_back(s);
            }
            else
            {
                acsm->acsmStateTable[r].NextState[i] =
                    acsm->acsmStateTable[acsm->acsmStateTable[r].FailState].NextState[i];
            }
        }
    }
}

/*
*
*/
ACSM3_STRUCT* acsm3New(const MpseAgent* agent)
{
    ACSM3_STRUCT* p = (ACSM3_STRUCT*)AC3_MALLOC (sizeof (ACSM3_STRUCT));
    MEMASSERT(p, "acsm3New");

    p->agent = agent;

    /* Initialise OpenCL */
    std::vector<cl::Platform> all_platforms;
    std::vector<cl::Device> all_devices;

    cl::Platform::get(&all_platforms);

    if(all_platforms.size() == 0) {
        std::cout << "No OpenCL platforms detected\n";
        exit(1);
    }

    p->default_platform = all_platforms[0];
    p->default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);

    if(all_devices.size() == 0) {
        std::cout << "No OpenCL devices detected\n";
        exit(1);
    }

    p->default_device = all_devices[0];

    std::cout << "Platform: " << p->default_platform.getInfo<CL_PLATFORM_NAME>()
        << std::endl;
    std::cout << "Device: " << p->default_device.getInfo<CL_DEVICE_NAME>()
        << std::endl;

    /* Create context and command queue for default device */
    p->context = cl::Context({p->default_device});
    p->queue = cl::CommandQueue(p->context, p->default_device);

    /* Kernel source */
    /* TODO move src to inline once kernel is stable */
    std::ifstream kernel_file("ac_gpu.cl");
    std::string kernel_code(std::istreambuf_iterator<char>(kernel_file),
            (std::istreambuf_iterator<char>()));
    /*
    std::string kernel_code = R"SIMONGOBACK(
    )SIMONGOBACK";
    */

    p->sources.push_back({kernel_code.c_str(), kernel_code.length()});
    p->program = cl::Program(p->context, p->sources);

    /* Compile kernel */
    if(p->program.build({p->default_device}) != CL_SUCCESS) {
        std::cout << "OpenCL kernel build error:\n"
            << p->program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(p->default_device)
            << std::endl;
        exit(1);
    }

    return p;
}

/*
*   Add a pattern to the list of patterns for this state machine
*/
int acsm3AddPattern(
    ACSM3_STRUCT* p, const uint8_t* pat, unsigned n, bool nocase,
    bool negative, void* user)
{
    ACSM3_PATTERN* plist;

    plist = (ACSM3_PATTERN*)AC3_MALLOC (sizeof (ACSM3_PATTERN));
    MEMASSERT(plist, "acsmAddPattern");

    plist->patrn = (uint8_t*)AC3_MALLOC (n);
    ConvertCaseEx (plist->patrn, pat, n);

    plist->casepatrn = (uint8_t*)AC3_MALLOC (n);
    memcpy(plist->casepatrn, pat, n);

    plist->udata = (ACSM3_USERDATA*)AC3_MALLOC(sizeof(ACSM3_USERDATA));
    MEMASSERT(plist->udata, "acsmAddPattern");

    plist->udata->ref_count = 1;
    plist->udata->id = user;
    plist->n = n;
    plist->nocase = nocase;
    plist->negative = negative;
    plist->next = p->acsmPatterns;

    p->acsmPatterns = plist;
    p->numPatterns++;

    return 0;
}

static void acsmBuildMatchStateTrees(snort::SnortConfig* sc, ACSM3_STRUCT* acsm)
{
    ACSM3_PATTERN* mlist;

    /* Find the states that have a MatchList */
    for (int i = 0; i < acsm->acsmMaxStates; i++) {
        for ( mlist = acsm->acsmStateTable[i].MatchList;
                mlist != nullptr;
                mlist=mlist->next ) {
            if (mlist->udata->id) {
                if (mlist->negative) {
                    acsm->agent->negate_list(
                            mlist->udata->id,
                            &acsm->acsmStateTable[i].MatchList->neg_list
                    );
                } else {
                    acsm->agent->build_tree(
                            sc, mlist->udata->id,
                            &acsm->acsmStateTable[i].MatchList->rule_option_tree
                    );
                }
            }
        }

        if (acsm->acsmStateTable[i].MatchList)
        {
            /* Last call to finalize the tree */
            acsm->agent->build_tree(
                    sc, nullptr,
                    &acsm->acsmStateTable[i].MatchList->rule_option_tree
            );
        }
    }
}

/*
*   Compile State Machine
*/
static inline int _acsmCompile(ACSM3_STRUCT* acsm)
{
    int i, k;
    ACSM3_PATTERN* plist;

    /* Count number of states */
    acsm->acsmMaxStates = 1;
    for (plist = acsm->acsmPatterns; plist != nullptr; plist = plist->next) {
        acsm->acsmMaxStates += plist->n;
    }

    /* Allocate memory */
    acsm->acsmStateTable =
        (ACSM3_STATETABLE*)AC3_MALLOC (sizeof (ACSM3_STATETABLE) * acsm->acsmMaxStates);
    MEMASSERT(acsm->acsmStateTable, "_acsmCompile");

    /* Initialize state zero as a branch */
    acsm->acsmNumStates = 0;

    /* Initialize all States NextStates to FAILED */
    for (k = 0; k < acsm->acsmMaxStates; k++) {
        for (i = 0; i < ALPHABET_SIZE; i++) {
            acsm->acsmStateTable[k].NextState[i] = ACSM3_FAIL_STATE;
        }
    }

    /* Add each Pattern to the State Table */
    for (plist = acsm->acsmPatterns; plist != nullptr; plist = plist->next) {
        AddPatternStates (acsm, plist);
    }

    /* Set all failed state transitions to return to the 0'th state */
    for (i = 0; i < ALPHABET_SIZE; i++) {
        if (acsm->acsmStateTable[0].NextState[i] == ACSM3_FAIL_STATE) {
            acsm->acsmStateTable[0].NextState[i] = 0;
        }
    }

    /* Build the NFA  */
    Build_NFA (acsm);

    /* Convert the NFA to a DFA */
    Convert_NFA_To_DFA (acsm);

    return 0;
}

int acsm3Compile(snort::SnortConfig* sc, ACSM3_STRUCT* acsm)
{
    if ( int rval = _acsmCompile (acsm) )
        return rval;

    if ( acsm->agent )
        acsmBuildMatchStateTrees(sc, acsm);

    /* Create cl buffers */
    acsm->cl_packet = cl::Buffer(
            acsm->context, CL_MEM_READ_ONLY, MAX_PACKET_SIZE);

    acsm->cl_packet_length = cl::Buffer(
            acsm->context, CL_MEM_READ_ONLY, sizeof(int));

    std::cout << "ACSM States: " << acsm->acsmMaxStates << std::endl;

    acsm->cl_state_table = cl::Buffer(
            acsm->context, CL_MEM_READ_ONLY,
            sizeof(ACSM3_STATETABLE) * acsm->acsmMaxStates);

    acsm->cl_nfound = cl::Buffer(
            acsm->context, CL_MEM_READ_WRITE, sizeof(int));

    return 0;
}

static THREAD_LOCAL uint8_t Tc[MAX_PACKET_SIZE];

/*
 *   Search Text or Binary Data for Pattern matches
 */
int acsm3Search(
    ACSM3_STRUCT* acsm, const uint8_t* Tx, int n, MpseMatch match,
    void* context, int* current_state)
{
    int state = 0;
    int index;
    int nfound = 0;

    const uint8_t* T;

    /* TODO perform case translation on GPU and see if it is faster (it should be) */
    ConvertCaseEx(Tc, Tx, n);
    T = Tc;

    if(false) {
        ACSM3_PATTERN* mlist;
        ACSM3_STATETABLE* StateTable = acsm->acsmStateTable;

        const uint8_t* Tend;
        Tend = T + n;

        if ( !current_state ) {
            return 0;
        }

        state = *current_state;

        for (; T < Tend; T++) {
            state = StateTable[state].NextState[*T];

            if ( StateTable[state].MatchList != nullptr ) {
                mlist = StateTable[state].MatchList;
                index = T + 1 - Tc;
                nfound++;
                if (match(mlist->udata->id, mlist->rule_option_tree, index, context,
                        mlist->neg_list) > 0) {
                    *current_state = state;

                    return nfound;
                }
            }
        }
        *current_state = state;

        return nfound;
    }

    /* Fill up cl buffers with necessary function params */
    acsm->queue.enqueueWriteBuffer(acsm->cl_packet, CL_TRUE, 0, n, T);

    acsm->queue.enqueueWriteBuffer(acsm->cl_packet_length, CL_TRUE, 0,
            sizeof(int), &n);

    acsm->queue.enqueueWriteBuffer(acsm->cl_state_table, CL_TRUE, 0,
            sizeof(ACSM3_STATETABLE)*acsm->acsmMaxStates, &(acsm->acsmStateTable));

    acsm->queue.enqueueWriteBuffer(acsm->cl_nfound, CL_TRUE, 0,
            sizeof(int), &nfound);

    /* TODO see if global instantiaion of kernel improves perf */
    acsm->kernel = cl::Kernel(acsm->program, "ac_gpu");

    /* Assign args to pass to kernel */
    acsm->kernel.setArg(0, acsm->cl_packet);
    acsm->kernel.setArg(1, acsm->cl_packet_length);
    acsm->kernel.setArg(2, acsm->cl_state_table);
    acsm->kernel.setArg(3, acsm->cl_nfound);

    /* Dispatch job */
    acsm->queue.enqueueNDRangeKernel(
            acsm->kernel,
            cl::NullRange,
            cl::NDRange(10),
            cl::NDRange(1)
    );

    /* Fetch results */
    acsm->queue.enqueueReadBuffer(acsm->cl_nfound, CL_TRUE, 0,
            sizeof(int), &nfound);

    foocount1 += nfound;

}

/*
 *   Free all memory
 */
void acsm3Free(ACSM3_STRUCT* acsm)
{
    int i;
    ACSM3_PATTERN* mlist, * ilist;

    for (i = 0; i < acsm->acsmMaxStates; i++)
    {
        mlist = acsm->acsmStateTable[i].MatchList;
        while (mlist)
        {
            ilist = mlist;
            mlist = mlist->next;

            ilist->udata->ref_count--;
            if (ilist->udata->ref_count == 0) {
                if (acsm->agent && ilist->udata->id)
                    acsm->agent->user_free(ilist->udata->id);

                AC3_FREE(ilist->udata);
            }

            if (ilist->rule_option_tree && acsm->agent) {
                acsm->agent->tree_free(&(ilist->rule_option_tree));
            }

            if (ilist->neg_list && acsm->agent) {
                acsm->agent->list_free(&(ilist->neg_list));
            }

            AC3_FREE (ilist);
        }
    }

    AC3_FREE (acsm->acsmStateTable);
    mlist = acsm->acsmPatterns;

    while (mlist) {
        ilist = mlist;
        mlist = mlist->next;
        AC3_FREE(ilist->patrn);
        AC3_FREE(ilist->casepatrn);
        AC3_FREE(ilist);
    }

    AC3_FREE (acsm);
}

int acsm3PatternCount(ACSM3_STRUCT* acsm) {
    return acsm->numPatterns;
}

static void Print_DFA( ACSM3_STRUCT * acsm )
{
    int k;
    int i;
    int next;

    for (k = 0; k < acsm->acsmMaxStates; k++) {
        for (i = 0; i < ALPHABET_SIZE; i++) {
            next = acsm->acsmStateTable[k].NextState[i];

            if( next == 0 || next ==  ACSM3_FAIL_STATE ) {
                if( isprint(i) )
                    printf("%3c->%-5d\t",i,next);
                else
                    printf("%3d->%-5d\t",i,next);
            }
        }
        printf("\n");
    }
}

int acsm3PrintDetailInfo(ACSM3_STRUCT* acsm) {
    Print_DFA( acsm );
    return 0;
}

int acsm3PrintSummaryInfo() {
#if 0
    // FIXIT-L should output summary similar to acsmPrintSummaryInfo2()
    printf ("ACSMX-Max Memory: %d bytes, %d states\n", max_memory,
        acsm->acsmMaxStates);
#endif
    std::cout << "~ Yo yo yo wassup, it's acmx3 in da house! ~\n";

    return 0;
}
