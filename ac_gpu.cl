struct ACSM3_USERDATA {
    void* id;
    uint ref_count;
};

struct ACSM3_PATTERN {
    struct ACSM3_PATTERN* next;
    struct ACSM3_USERDATA* udata;

    uchar* patrn;
    uchar* casepatrn;

    void* rule_option_tree;
    void* neg_list;

    int n;
    int nocase;
    int negative;
};

struct ACSM3_STATETABLE {
    int NextState[ 256 ]; /* Alphabet Size */
    int FailState;

    struct ACSM3_PATTERN* MatchList;
};

void kernel ac_gpu(
        global uchar* T,                    // text body
        global int* n,                      // text length
        global struct ACSM3_STATETABLE* StateTable,
        global int* nfound                 // number of matched bytes
        ) {

    *nfound = 0;

    global uchar* Tend = T + *n;

    int state = 0;

    for (; T < Tend; T++)
    {
        state = StateTable[state].NextState[*T];
        printf("%c %d %p\n", *T, state, StateTable[state].MatchList);

        if ( StateTable[state].MatchList != 0 )
        {
            (*nfound)++;
        }
    }
    return;
}


