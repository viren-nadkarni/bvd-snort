struct ACSM3_USERDATA {
    void* id;                       // 4 bytes
    uint ref_count;                 // 4 bytes
};

struct ACSM3_PATTERN {
    struct ACSM3_PATTERN* next;     // 8
    struct ACSM3_USERDATA* udata;   // 8

    uchar* patrn;                   // 8
    uchar* casepatrn;               // 8

    void* rule_option_tree;         // 8
    void* neg_list;                 // 8

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

/*
    printf("sizeof ACSM3_STATETABLE=%d\n", sizeof(struct ACSM3_STATETABLE));
    printf("sizeof ACSM3_USERDATA=%d\n", sizeof(struct ACSM3_USERDATA));
    printf("sizeof ACSM3_PATTERN=%d\n", sizeof(struct ACSM3_PATTERN));
*/

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


