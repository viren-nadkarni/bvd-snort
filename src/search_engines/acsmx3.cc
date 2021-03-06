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


void cl_printf_callback(
        const char *buffer, size_t length, size_t final, void *user_data) {
     fwrite(buffer, 1, length, stdout);
}

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

/*
 * Initialises lookup table for case conversion. Called by ac_gpu.c:ac_init()
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

/*
 * Convert case using lookup table
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
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        int s = acsm->acsmStateTable[0].NextState[i];

        if (s) {
            queue.push_back(s);
            acsm->acsmStateTable[s].FailState = 0;
        }
    }

    /* Build the fail state transitions for each valid state */
    for ( auto r : queue ) {
        /* Find Final States for any Failure */
        for (int i = 0; i < ALPHABET_SIZE; i++) {
            int s = acsm->acsmStateTable[r].NextState[i];

            if ( s != ACSM3_FAIL_STATE ) {
                queue.push_back(s);
                int fs = acsm->acsmStateTable[r].FailState;
                int next;

                /*
                 *  Locate the next valid state for 'i' starting at s
                 */
                while ((next=acsm->acsmStateTable[fs].NextState[i]) ==
                        ACSM3_FAIL_STATE) {
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
 * Build Deterministic Finite Automata from NFA
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

    cl_context_properties context_properties[] = {
        CL_PRINTF_CALLBACK_ARM, (cl_context_properties) cl_printf_callback,
        CL_PRINTF_BUFFERSIZE_ARM, (cl_context_properties) 0x100000,
        0
    };
    p->context = cl::Context(p->default_device, context_properties, NULL, NULL, NULL);
    p->queue = cl::CommandQueue(p->context, p->default_device);

    /* Kernel source */
    std::ifstream kernel_file("ac_gpu.cl");
    std::string kernel_code(std::istreambuf_iterator<char>(kernel_file),
            (std::istreambuf_iterator<char>()));
    /* TODO move abv src to inline once stable */
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

    p->kernel = cl::Kernel(p->program, "ac_gpu");

    /* Create cl packet buffers */
    p->cl_packet = cl::Buffer(
            p->context, CL_MEM_READ_ONLY,
            sizeof(uint8_t) * MAX_PACKET_SIZE * PACKET_BUFFER_SIZE);

    p->cl_packet_length = cl::Buffer(
            p->context, CL_MEM_READ_ONLY,
            sizeof(int) * PACKET_BUFFER_SIZE);

    p->cl_nfound = cl::Buffer(
            p->context, CL_MEM_READ_WRITE,
            sizeof(int) * PACKET_BUFFER_SIZE);

    /* map mem to cl packet buffers */
    p->packet_buffer = (uint8_t*) p->queue.enqueueMapBuffer(
            p->cl_packet, CL_TRUE, CL_MAP_WRITE, 0,
            MAX_PACKET_SIZE * PACKET_BUFFER_SIZE * sizeof(uint8_t));

    p->packet_length_buffer = (int*) p->queue.enqueueMapBuffer(
            p->cl_packet_length, CL_TRUE, CL_MAP_WRITE, 0,
            PACKET_BUFFER_SIZE * sizeof(int));

    /* initialise to zero */
    p->packet_buffer_index = 0;

    memset(p->packet_length_buffer, 0,
            PACKET_BUFFER_SIZE * sizeof(int));

    return p;
}

/*
 * Add a pattern to the list of patterns for this state machine
 */
int acsm3AddPattern(
        ACSM3_STRUCT* p, const uint8_t* pat, unsigned n, bool nocase,
        bool negative, void* user) {
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

    /* Create State machine buffer. This is not created in acsmNew because
     * acsmMaxStates is known after building the state mx */
    acsm->cl_state_table = cl::Buffer(
            acsm->context, CL_MEM_READ_ONLY,
            sizeof(ACSM3_STATETABLE) * acsm->acsmMaxStates);

    /* Copy state machine to cl buffer */
    acsm->queue.enqueueWriteBuffer(acsm->cl_state_table, CL_TRUE, 0,
            sizeof(ACSM3_STATETABLE)*acsm->acsmMaxStates, acsm->acsmStateTable);

    //acsm3PrintDetailInfo(acsm);

    return 0;
}

static THREAD_LOCAL uint8_t Tc[MAX_PACKET_SIZE];

/*
 * Search Text or Binary Data for Pattern matches
 */
int acsm3Search(
        ACSM3_STRUCT* acsm, const uint8_t* Tx, int n, MpseMatch match,
        void* context, int* current_state) {
    int state = 0;
    int index;

    const uint8_t* T;

    /* TODO perform case translation on GPU and see if it is faster (it should be) */
    ConvertCaseEx(Tc, Tx, n);
    T = Tc;

    int packet_space = MAX_PACKET_SIZE * sizeof(uint8_t);

    memcpy(acsm->packet_buffer + packet_space * acsm->packet_buffer_index,
            T, n);

    acsm->packet_length_buffer[acsm->packet_buffer_index] = n;
    acsm->packet_buffer_index++;

    if(acsm->packet_buffer_index == PACKET_BUFFER_SIZE) {
        cl_dispatch(acsm);
    }

}

void cl_dispatch(ACSM3_STRUCT* acsm) {
    /* unmap buffers before invoking kernel */
    acsm->queue.enqueueUnmapMemObject(acsm->cl_packet, acsm->packet_buffer);
    acsm->queue.enqueueUnmapMemObject(
            acsm->cl_packet_length, acsm->packet_length_buffer);

    /* Assign args to pass to kernel */
    acsm->kernel.setArg(0, acsm->cl_packet);
    acsm->kernel.setArg(1, acsm->cl_packet_length);
    acsm->kernel.setArg(2, acsm->cl_state_table);
    acsm->kernel.setArg(3, acsm->cl_nfound);

    /* Dispatch to kernel */
    acsm->queue.enqueueNDRangeKernel(
            acsm->kernel,
            cl::NullRange,
            cl::NDRange(CL_GLOBAL_SIZE),
            cl::NDRange(CL_LOCAL_SIZE)
    );

    /* Fetch results */
    int* nfound = (int*) acsm->queue.enqueueMapBuffer(
            acsm->cl_nfound, CL_TRUE, CL_MAP_READ, 0,
            sizeof(int) * PACKET_BUFFER_SIZE);

    /* count matches */
    for(int i = 0; i < PACKET_BUFFER_SIZE; ++i) {
        match_instances += nfound[i];
        if(nfound[i]) {
            match_packets += 1;
        }
    }

    /* re-map buffer */
    acsm->packet_buffer = (uint8_t*) acsm->queue.enqueueMapBuffer(
            acsm->cl_packet, CL_TRUE, CL_MAP_WRITE, 0,
            PACKET_BUFFER_SIZE * MAX_PACKET_SIZE * sizeof(uint8_t));

    acsm->packet_length_buffer = (int*) acsm->queue.enqueueMapBuffer(
            acsm->cl_packet_length, CL_TRUE, CL_MAP_WRITE, 0,
            PACKET_BUFFER_SIZE * sizeof(int));

    /* clear packet lengths */
    memset(acsm->packet_length_buffer, 0,
            PACKET_BUFFER_SIZE * sizeof(int));

    /* reset packets in buffer count */
    acsm->packet_buffer_index = 0;

    /* clean up */
    acsm->queue.enqueueUnmapMemObject(acsm->cl_nfound, nfound);
}

/*
 *   Free all memory
 */
void acsm3Free(ACSM3_STRUCT* acsm) {
    if(acsm->packet_buffer_index)
        cl_dispatch(acsm);

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

static void Print_DFA(ACSM3_STRUCT * acsm) {
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
    Print_DFA(acsm);
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
