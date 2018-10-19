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
    /* Alphabet Size */
    int NextState[256];                 // 1024
    int FailState;                      // 4

    struct ACSM3_PATTERN* MatchList;    // 8
};

void kernel ac_gpu(
        __global uchar* Pbuff,          // text body
        __global int* Plen,             // text length
        __global struct ACSM3_STATETABLE* StateTable,
        __global int* Nfound            // number of matched bytes
    ) {

    int index = get_global_id(0);
    int n = Plen[index];
    int state = 0;

    /* Max Packet Size */
    __global uchar* T = Pbuff + index * (16 * 1024);
    __global uchar* Tend = T + n;

    Nfound[index] = 0;

    for(; T < Tend; T++) {
        state = StateTable[state].NextState[*T];

        /*
        printf("%c %d %p\n", *T, state, StateTable[state].MatchList);
        */

        if(StateTable[state].MatchList != 0) {
            Nfound[index]++;
        }
    }
}
