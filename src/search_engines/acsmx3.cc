#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "acsmx3.h"

#include <list>

#include "main/thread.h"
#include "utils/util.h"

#define MEMASSERT(p,s) if (!(p)) { fprintf(stderr,"ACSM-No Memory: %s\n",s); exit(0); }

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
** Case Translation Table
*/
static uint8_t xlatcase[256];

/*
*
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
*  Add a pattern to the list of patterns terminated at this state.
*  Insert at front of list.
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
   Add Pattern States
*/
static void AddPatternStates(ACSM3_STRUCT* acsm, ACSM3_PATTERN* p)
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
        if (next == ACSM3_FAIL_STATE)
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
    MEMASSERT(p, "acsmNew");

    if (p)
    {
        p->agent = agent;
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
static inline int _acsmCompile(ACSM3_STRUCT* acsm)
{
    int i, k;
    ACSM3_PATTERN* plist;

    /* Count number of states */
    acsm->acsmMaxStates = 1;
    for (plist = acsm->acsmPatterns; plist != nullptr; plist = plist->next)
    {
        acsm->acsmMaxStates += plist->n;
    }
    acsm->acsmStateTable =
        (ACSM3_STATETABLE*)AC3_MALLOC (sizeof (ACSM3_STATETABLE) * acsm->acsmMaxStates);
    MEMASSERT(acsm->acsmStateTable, "_acsmCompile");

    /* Initialize state zero as a branch */
    acsm->acsmNumStates = 0;

    /* Initialize all States NextStates to FAILED */
    for (k = 0; k < acsm->acsmMaxStates; k++)
    {
        for (i = 0; i < ALPHABET_SIZE; i++)
        {
            acsm->acsmStateTable[k].NextState[i] = ACSM3_FAIL_STATE;
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
        if (acsm->acsmStateTable[0].NextState[i] == ACSM3_FAIL_STATE)
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

int acsm3Compile(snort::SnortConfig* sc, ACSM3_STRUCT* acsm)
{
    if ( int rval = _acsmCompile (acsm) )
        return rval;

    if ( acsm->agent )
        acsmBuildMatchStateTrees(sc, acsm);

    return 0;
}

static THREAD_LOCAL uint8_t Tc[64*1024];

/*
*   Search Text or Binary Data for Pattern matches
*/
int acsm3Search(
    ACSM3_STRUCT* acsm, const uint8_t* Tx, int n, MpseMatch match,
    void* context, int* current_state)
{
    int state = 0;
    ACSM3_PATTERN* mlist;
    const uint8_t* Tend;
    ACSM3_STATETABLE* StateTable = acsm->acsmStateTable;
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
    return nfound;
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
            if (ilist->udata->ref_count == 0)
            {
                if (acsm->agent && ilist->udata->id)
                    acsm->agent->user_free(ilist->udata->id);

                AC3_FREE(ilist->udata);
            }

            if (ilist->rule_option_tree && acsm->agent)
            {
                acsm->agent->tree_free(&(ilist->rule_option_tree));
            }

            if (ilist->neg_list && acsm->agent)
            {
                acsm->agent->list_free(&(ilist->neg_list));
            }

            AC3_FREE (ilist);
        }
    }
    AC3_FREE (acsm->acsmStateTable);
    mlist = acsm->acsmPatterns;
    while (mlist)
    {
        ilist = mlist;
        mlist = mlist->next;
        AC3_FREE(ilist->patrn);
        AC3_FREE(ilist->casepatrn);
        AC3_FREE(ilist);
    }
    AC3_FREE (acsm);
}

int acsm3PatternCount(ACSM3_STRUCT* acsm)
{
    return acsm->numPatterns;
}

static void Print_DFA( ACSM3_STRUCT * acsm )
{
    int k;
    int i;
    int next;

    for (k = 0; k < acsm->acsmMaxStates; k++)
    {
      for (i = 0; i < ALPHABET_SIZE; i++)
    {
      next = acsm->acsmStateTable[k].NextState[i];

      if( next == 0 || next ==  ACSM3_FAIL_STATE )
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

int acsm3PrintDetailInfo(ACSM3_STRUCT* acsm)
{
    Print_DFA( acsm );
    return 0;
}

int acsm3PrintSummaryInfo()
{
#if 0
    // FIXIT-L should output summary similar to acsmPrintSummaryInfo2()
    printf ("ACSMX-Max Memory: %d bytes, %d states\n", max_memory,
        acsm->acsmMaxStates);
#endif
    printf("Yo yo yo wassup, it's acmx3 in da house!\n");

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
    ACSM3_STRUCT* acsm;
    if (argc < 3)
    {
        fprintf (stderr,
            "Usage: acsmx pattern word-1 word-2 ... word-n  -nocase\n");
        exit (0);
    }
    acsm = acsm3New ();
    strncpy(text, argv[1], sizeof(text));
    for (i = 1; i < argc; i++)
        if (strcmp (argv[i], "-nocase") == 0)
            nocase = 1;
    for (i = 2; i < argc; i++)
    {
        if (argv[i][0] == '-')
            continue;
        acsm3AddPattern (acsm, (uint8_t*)argv[i], strlen (argv[i]), nocase, 0, 0,
            argv[i], i - 2);
    }
    acsm3Compile (acsm);
    acsm3Search (acsm, text, strlen (text), MatchFound, (void*)0);
    acsm3Free (acsm);
    printf ("normal pgm end\n");
    return (0);
}

#endif
