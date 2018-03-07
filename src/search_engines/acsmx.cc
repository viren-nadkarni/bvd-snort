//--------------------------------------------------------------------------
// Copyright (C) 2014-2017 Cisco and/or its affiliates. All rights reserved.
// Copyright (C) 2002-2013 Sourcefire, Inc.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------
/*
** Multi-Pattern Search Engine
**
** Aho-Corasick State Machine -  uses a Deterministic Finite Automata - DFA
** Marc Norton
**
**
**   Reference - Efficient String matching: An Aid to Bibliographic Search
**               Alfred V Aho and Margaret J Corasick
**               Bell Laboratories
**               Copyright (C) 1975 Association for Computing Machinery,Inc
**
**   Implemented from the 4 algorithms in the paper by Aho & Corasick
**   and some implementation ideas from 'Practical Algorithms in C'
**
**   Notes:
**     1) This version uses about 1024 bytes per pattern character - heavy  on the memory.
**     2) This algorithm finds all occurrences of all patterns within a
**        body of text.
**     3) Support is included to handle upper and lower case matching.
**     4) Some compilers optimize the search routine well, others don't, this makes all the difference.
**     5) Aho inspects all bytes of the search text, but only once so it's very efficient,
**        if the patterns are all large than the Modified Wu-Manbar method is often faster.
**     6) I don't subscribe to any one method is best for all searching needs,
**        the data decides which method is best,
**        and we don't know until after the search method has been tested on the specific data sets.
**
**  May 2002  : Marc Norton 1st Version
**  June 2002 : Modified interface for SNORT, added case support
**  Aug 2002  : Cleaned up comments, and removed dead code.
**  Nov 2,2002: Fixed queue_init() , added count=0
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "acsmx.h"

#include <list>

#include "main/thread.h"
#include "utils/util.h"

//Added
#include <iostream>     // std::cout
#include <fstream>      // std::ifstream

#define MEMASSERT(p,s) if (!(p)) { fprintf(stderr,"ACSM-No Memory: %s\n",s); exit(0); }

static int max_memory = 0;

static void* AC_MALLOC(int n)
{
    void* p = snort_calloc(n);
    max_memory += n;
    return p;
}

static void AC_FREE(void* p)
{
    if (p)
        snort_free(p);
}

/*
** Case Translation Table
*/
static uint8_t xlatcase[256];

/*
*
*/
void acsmx_init_xlatcase()
{
    int i;
    for (i = 0; i < 256; i++)
    {
        xlatcase[i] = (uint8_t)toupper (i);
    }
}

/*
*
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
static ACSM_PATTERN* CopyMatchListEntry(ACSM_PATTERN* px)
{
    ACSM_PATTERN* p;
    p = (ACSM_PATTERN*)AC_MALLOC(sizeof(ACSM_PATTERN));
    MEMASSERT(p, "CopyMatchListEntry");
    memcpy(p, px, sizeof (ACSM_PATTERN));
    px->udata->ref_count++;
    p->next = nullptr;
    return p;
}

/*
*  Add a pattern to the list of patterns terminated at this state.
*  Insert at front of list.
*/
static void AddMatchListEntry(ACSM_STRUCT* acsm, int state, ACSM_PATTERN* px)
{
    ACSM_PATTERN* p;
    p = (ACSM_PATTERN*)AC_MALLOC(sizeof(ACSM_PATTERN));
    MEMASSERT(p, "AddMatchListEntry");
    memcpy(p, px, sizeof (ACSM_PATTERN));
    p->next = acsm->acsmStateTable[state].MatchList;
    acsm->acsmStateTable[state].MatchList = p;
}

/*
   Add Pattern States
*/
static void AddPatternStates(ACSM_STRUCT* acsm, ACSM_PATTERN* p)
{
    int state = 0;
    int n = p->n;
    uint8_t* pattern = p->patrn;

    /*
     *  Match up pattern with existing states
     */
    for (; n > 0; pattern++, n--)
    {
        int next = acsm->acsmStateTable[state].NextState[*pattern];
        if (next == ACSM_FAIL_STATE)
            break;
        state = next;
    }

    /*
     *   Add new states for the rest of the pattern bytes, 1 state per byte
     */
    for (; n > 0; pattern++, n--)
    {
        acsm->acsmNumStates++;
        acsm->acsmStateTable[state].NextState[*pattern] = acsm->acsmNumStates;
        state = acsm->acsmNumStates;
    }

    AddMatchListEntry (acsm, state, p);
}

/*
*   Build Non-Deterministic Finite Automata
*/
static void Build_NFA(ACSM_STRUCT* acsm)
{
    ACSM_PATTERN* mlist=nullptr;
    ACSM_PATTERN* px=nullptr;

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

            if ( s != ACSM_FAIL_STATE )
            {
                queue.push_back(s);
                int fs = acsm->acsmStateTable[r].FailState;
                int next;

                /*
                 *  Locate the next valid state for 'i' starting at s
                 */
                while ((next=acsm->acsmStateTable[fs].NextState[i]) ==
                    ACSM_FAIL_STATE)
                {
                    fs = acsm->acsmStateTable[fs].FailState;
                }

                /*
                 *  Update 's' state failure state to point to the next valid state
                 */
                acsm->acsmStateTable[s].FailState = next;

                /*
                 *  Copy 'next' states MatchList to 's' states MatchList,
                 *  we copy them so each list can be AC_FREE'd later,
                 *  else we could just manipulate pointers to fake the copy.
                 */
                for (mlist  = acsm->acsmStateTable[next].MatchList;
                    mlist != nullptr;
                    mlist  = mlist->next)
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
static void Convert_NFA_To_DFA(ACSM_STRUCT* acsm)
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

            if ( s != ACSM_FAIL_STATE )
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
ACSM_STRUCT* acsmNew(const MpseAgent* agent)
{
    ACSM_STRUCT* p = (ACSM_STRUCT*)AC_MALLOC (sizeof (ACSM_STRUCT));
    MEMASSERT(p, "acsmNew");

    if (p)
    {
        p->agent = agent;

        // ADDED -----------------------------------------------------------------
        
        std::ofstream out("/home/odroid/Documents/Clort/se_init_out.txt");
        std::streambuf *coutbuf = std::cout.rdbuf();
        std::cout.rdbuf(out.rdbuf());

        //get default plattform
        cl::Platform::get(&(p->all_platforms));
        if(p->all_platforms.size()==0){
            std::cout<<"No platforms \n";
        }

        p->default_platform=p->all_platforms[0];

        //get default device of the default platform, also print how many found
        
        
        p->default_platform.getDevices(CL_DEVICE_TYPE_GPU, &(p->all_devices));

        if(p->all_devices.size()==0){
           std::cout<<"No devices \n";
        }

        p->default_device=p->all_devices[0];
        std::cout<< "GPUs on Odriod: " << p->all_devices.size()<<"\n";
        std::cout<< "Using device: "<< p->default_device.getInfo<CL_DEVICE_NAME>()<<"\n";

        p->context = cl::Context(p->default_device);    //cl::Context context({default_device}); Are curly brackets needed??

        p->queue = cl::CommandQueue(p->context,p->default_device);

        // Read source file
        std::ifstream sourceFile("/home/odroid/Documents/Clort/findMatches.cl");
        std::string sourceCode(
            std::istreambuf_iterator<char>(sourceFile),
            (std::istreambuf_iterator<char>()));
        
        p->sources.push_back({sourceCode.c_str(),sourceCode.length()}); //curly brackets needed around arguments?

        //std::cout << sources;

        p->program = cl::Program(p->context,p->sources);
        if(p->program.build({p->default_device})!=CL_SUCCESS){
            std::cout<<" Error building: "<<p->program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(p->default_device)<<"\n";         
        }

        p->kernel = cl::Kernel(p->program, "findMatches");

        std::cout.rdbuf(coutbuf);

        // ADDED END -------------------------------------------------------------
    }
    return p;
}

/*
*   Add a pattern to the list of patterns for this state machine
*/
int acsmAddPattern(
    ACSM_STRUCT* p, const uint8_t* pat, unsigned n, bool nocase,
    bool negative, void* user)
{
    ACSM_PATTERN* plist;
    plist = (ACSM_PATTERN*)AC_MALLOC (sizeof (ACSM_PATTERN));
    MEMASSERT(plist, "acsmAddPattern");
    plist->patrn = (uint8_t*)AC_MALLOC (n);
    ConvertCaseEx (plist->patrn, pat, n);
    plist->casepatrn = (uint8_t*)AC_MALLOC (n);
    memcpy(plist->casepatrn, pat, n);

    plist->udata = (ACSM_USERDATA*)AC_MALLOC(sizeof(ACSM_USERDATA));
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

static void acsmBuildMatchStateTrees(SnortConfig* sc, ACSM_STRUCT* acsm)
{
    ACSM_PATTERN* mlist;

    /* Find the states that have a MatchList */
    for (int i = 0; i < acsm->acsmMaxStates; i++)
    {
        for ( mlist=acsm->acsmStateTable[i].MatchList; mlist!=nullptr; mlist=mlist->next )
        {
            if (mlist->udata->id)
            {
                if (mlist->negative)
                {
                    acsm->agent->negate_list(
                        mlist->udata->id, &acsm->acsmStateTable[i].MatchList->neg_list);
                }
                else
                {
                    acsm->agent->build_tree(sc, mlist->udata->id,
                        &acsm->acsmStateTable[i].MatchList->rule_option_tree);
                }
            }
        }

        if (acsm->acsmStateTable[i].MatchList)
        {
            /* Last call to finalize the tree */
            acsm->agent->build_tree(sc, nullptr,
                &acsm->acsmStateTable[i].MatchList->rule_option_tree);
        }
    }
}

/*
*   Compile State Machine
*/
static inline int _acsmCompile(ACSM_STRUCT* acsm)
{
    int i, k;
    ACSM_PATTERN* plist;

    /* Count number of states */
    acsm->acsmMaxStates = 1;
    for (plist = acsm->acsmPatterns; plist != nullptr; plist = plist->next)
    {
        acsm->acsmMaxStates += plist->n;
    }
    acsm->acsmStateTable =
        (ACSM_STATETABLE*)AC_MALLOC (sizeof (ACSM_STATETABLE) * acsm->acsmMaxStates);
    MEMASSERT(acsm->acsmStateTable, "_acsmCompile");

    /* Initialize state zero as a branch */
    acsm->acsmNumStates = 0;

    /* Initialize all States NextStates to FAILED */
    for (k = 0; k < acsm->acsmMaxStates; k++)
    {
        for (i = 0; i < ALPHABET_SIZE; i++)
        {
            acsm->acsmStateTable[k].NextState[i] = ACSM_FAIL_STATE;
        }
    }

    /* Add each Pattern to the State Table */
    for (plist = acsm->acsmPatterns; plist != nullptr; plist = plist->next)
    {
        AddPatternStates (acsm, plist);
    }

    /* Set all failed state transitions to return to the 0'th state */
    for (i = 0; i < ALPHABET_SIZE; i++)
    {
        if (acsm->acsmStateTable[0].NextState[i] == ACSM_FAIL_STATE)
        {
            acsm->acsmStateTable[0].NextState[i] = 0;
        }
    }

    /* Build the NFA  */
    Build_NFA (acsm);

    /* Convert the NFA to a DFA */
    Convert_NFA_To_DFA (acsm);

    return 0;
}

int acsmCompile(
    SnortConfig* sc, ACSM_STRUCT* acsm)
{
    if ( int rval = _acsmCompile (acsm) )
        return rval;

    if ( acsm->agent )
        acsmBuildMatchStateTrees(sc, acsm);

    //Build list array
    acsm->stateArray = new int [acsm->acsmMaxStates*(acsm->ALPHABET_SIZE+1)];
    int next;
    ACSM_PATTERN* mlist;
    for (int k = 0; k < acsm->acsmMaxStates; k++)
    {
        for (int i = 0; i < ALPHABET_SIZE+1; i++)
        {
            if(i % 257){
                acsm->stateArray[k*i] = acsm->acsmStateTable[k].NextState[i];
            }
            else{
                mlist = acsm->acsmStateTable[i].MatchList;
                if(mlist){
                    acsm->stateArray[k*i] = 1;
                }
                //int countPatterns = 0;
                //while (mlist)
                //{
                //    mlist = mlist->next;
                //    countPatterns++;
                //}
                
        }
    }

    return 0;
}

static THREAD_LOCAL uint8_t Tc[64*1024];

/*
*   Search Text or Binary Data for Pattern matches
*/
int acsmSearch(
    ACSM_STRUCT* acsm, const uint8_t* Tx, int n, MpseMatch match,
    void* context, int* current_state)
{
    int state = 0;
    ACSM_PATTERN* mlist;
    const uint8_t* Tend;
    ACSM_STATETABLE* StateTable = acsm->acsmStateTable;
    int nfound = 0;
    const uint8_t* T;
    int index;

    /* Case conversion */
    ConvertCaseEx (Tc, Tx, n);
    T = Tc;
    Tend = T + n;

    if ( !current_state )
    {
        return 0;
    }

    state = *current_state;

    for (; T < Tend; T++)
    {
        state = StateTable[state].NextState[*T];

        if ( StateTable[state].MatchList != nullptr )
        {
            mlist = StateTable[state].MatchList;
            index = T + 1 - Tc;
            nfound++;
            if (match(mlist->udata->id, mlist->rule_option_tree, index, context,
                mlist->neg_list) > 0)
            {
                *current_state = state;
                return nfound;
            }
        }
    }
    *current_state = state;

    cl_int err;
    // Create memory buffers
    cl::Buffer stateBuffer = cl::Buffer(acsm->context, CL_MEM_READ_ONLY, acsm->acsmNumStates*258*sizeof(int));
    cl::Buffer xlatBuffer = cl::Buffer(acsm->context, CL_MEM_READ_ONLY, 256 * sizeof(uint8_t));
    cl::Buffer textBuffer = cl::Buffer(acsm->context, CL_MEM_READ_ONLY, n * sizeof(uint8_t));
    cl::Buffer lengthBuffer  = cl::Buffer(acsm->context, CL_MEM_READ_ONLY, sizeof(int));
    cl::Buffer resultBuffer = cl::Buffer(acsm->context, CL_MEM_WRITE_ONLY, 10 * sizeof(int));

    int length = n;

    // Copy lists A and B to the memory buffers
    err = acsm->queue.enqueueWriteBuffer(stateBuffer, CL_TRUE, 0, acsm->acsmMaxStates*257*sizeof(int), acsm->stateArray);
    if(err != CL_SUCCESS){
        printf("Error state");
    }
    err = acsm->queue.enqueueWriteBuffer(xlatBuffer, CL_TRUE, 0, 256 * sizeof(uint8_t), xlatcase);
    if(err != CL_SUCCESS){
        printf("Error xlat ");
    }
    err = acsm->queue.enqueueWriteBuffer(textBuffer, CL_TRUE, 0, n * sizeof(uint8_t), T);
    if(err != CL_SUCCESS){
        printf("Error text");
    }
    err = acsm->queue.enqueueWriteBuffer(lengthBuffer, CL_TRUE, 0, sizeof(int), &length);
    if(err != CL_SUCCESS){
       printf("Error length");
    }

    // Set arguments to kernel
    err = acsm->kernel.setArg(0, stateBuffer);
    if(err != CL_SUCCESS){
        printf("Error in setarg stateBuffer %d \n", err);
    }
    err = acsm->kernel.setArg(1, xlatBuffer);
    if(err != CL_SUCCESS){
        printf("Error in setarg xlatBuffer %d \n", err);
    }    
    err = acsm->kernel.setArg(2, textBuffer);
    if(err != CL_SUCCESS){
        printf("Error in setarg textBuffer %d \n" , err);
    }
    err = acsm->kernel.setArg(3, lengthBuffer);
    if(err != CL_SUCCESS){
        printf("Error in setarg lengthBuffer %d \n", err);
    }
    err = acsm->kernel.setArg(4, resultBuffer);
    if(err != CL_SUCCESS){
        printf("Error in setarg resultBuffer %d \n", err);
    }
    
    // Run the kernel on specific ND range
    cl::NDRange global(10);
    cl::NDRange local(1);
    err = acsm->queue.enqueueNDRangeKernel(acsm->kernel, cl::NullRange, global, local);
    if(err != CL_SUCCESS){
        printf("Error in enqueue %d \n", err);
    }

    int C[10];

    acsm->queue.finish();
    acsm->queue.enqueueReadBuffer(resultBuffer, CL_TRUE, 0, 10 * sizeof(int), C);
    
    int count = 0;
    for(int i = 0; i < 10; i ++)
    {
        count += C[i];
    }
    printf("Total found GPU: %d \n", count);
    printf("Total found CPU: %d \n", nfound);
    return nfound;
}

/*
*   Free all memory
*/
void acsmFree(ACSM_STRUCT* acsm)
{
    int i;
    ACSM_PATTERN* mlist, * ilist;
    for (i = 0; i < acsm->acsmMaxStates; i++)
    {
        mlist = acsm->acsmStateTable[i].MatchList;
        while (mlist)
        {
            ilist = mlist;
            mlist = mlist->next;

            ilist->udata->ref_count--;
            if (ilist->udata->ref_count == 0)
            {
                if (acsm->agent && ilist->udata->id)
                    acsm->agent->user_free(ilist->udata->id);

                AC_FREE(ilist->udata);
            }

            if (ilist->rule_option_tree && acsm->agent)
            {
                acsm->agent->tree_free(&(ilist->rule_option_tree));
            }

            if (ilist->neg_list && acsm->agent)
            {
                acsm->agent->list_free(&(ilist->neg_list));
            }

            AC_FREE (ilist);
        }
    }
    AC_FREE (acsm->acsmStateTable);
    mlist = acsm->acsmPatterns;
    while (mlist)
    {
        ilist = mlist;
        mlist = mlist->next;
        AC_FREE(ilist->patrn);
        AC_FREE(ilist->casepatrn);
        AC_FREE(ilist);
    }
    AC_FREE (acsm);
}

int acsmPatternCount(ACSM_STRUCT* acsm)
{
    return acsm->numPatterns;
}

static void Print_DFA( ACSM_STRUCT * acsm )
{
    int k;
    int i;
    int next;

    for (k = 0; k < acsm->acsmMaxStates; k++)
    {
      for (i = 0; i < ALPHABET_SIZE; i++)
    {
      next = acsm->acsmStateTable[k].NextState[i];

      if( next == 0 || next ==  ACSM_FAIL_STATE )
      {
           if( isprint(i) )
             printf("%3c->%-5d\t",i,next);
           else
             printf("%3d->%-5d\t",i,next);
      }
    }
      printf("\n");
    }

}

int acsmPrintDetailInfo(ACSM_STRUCT* acsm)
{
    Print_DFA( acsm );
    return 0;
}

int acsmPrintSummaryInfo()
{
#if 0
    // FIXIT-L should output summary similar to acsmPrintSummaryInfo2()
    printf ("ACSMX-Max Memory: %d bytes, %d states\n", max_memory,
        acsm->acsmMaxStates);
#endif

    return 0;
}

#ifdef ACSMX_MAIN

/*
*  Text Data Buffer
*/
uint8_t text[512];

/*
*    A Match is found
*/
int MatchFound(unsigned id, int index, void* data)
{
    fprintf (stdout, "%s\n", (char*)id);
    return 0;
}

/*
*
*/
int main(int argc, char** argv)
{
    int i, nocase = 0;
    ACSM_STRUCT* acsm;
    if (argc < 3)
    {
        fprintf (stderr,
            "Usage: acsmx pattern word-1 word-2 ... word-n  -nocase\n");
        exit (0);
    }
    acsm = acsmNew ();
    strncpy(text, argv[1], sizeof(text));
    for (i = 1; i < argc; i++)
        if (strcmp (argv[i], "-nocase") == 0)
            nocase = 1;
    for (i = 2; i < argc; i++)
    {
        if (argv[i][0] == '-')
            continue;
        acsmAddPattern (acsm, (uint8_t*)argv[i], strlen (argv[i]), nocase, 0, 0,
            argv[i], i - 2);
    }
    acsmCompile (acsm);
    acsmSearch (acsm, text, strlen (text), MatchFound, (void*)0);
    acsmFree (acsm);
    printf ("normal pgm end\n");
    return (0);
}

#endif /*  */

