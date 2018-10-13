#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "framework/mpse.h"

#include "acsmx3.h"

using namespace snort;

class Ac3Mpse : public Mpse
{
private:
    ACSM3_STRUCT* obj;

public:
    Ac3Mpse(SnortConfig*, const MpseAgent* agent)
        : Mpse("ac_gpu")
    { obj = acsm3New(agent); }

    ~Ac3Mpse() override
    { acsm3Free(obj); }

    int add_pattern(
        SnortConfig*, const uint8_t* P, unsigned m,
        const PatternDescriptor& desc, void* user) override
    {
        return acsm3AddPattern(obj, P, m, desc.no_case, desc.negated, user);
    }

    int prep_patterns(SnortConfig* sc) override
    { return acsm3Compile(sc, obj); }

    int _search(
        const uint8_t* T, int n, MpseMatch match,
        void* context, int* current_state) override
    {
        return acsm3Search(obj, T, n, match, context, current_state);
    }

    int print_info() override
    { return acsm3PrintDetailInfo(obj); }

    int get_pattern_count() override
    { return acsm3PatternCount(obj); }
};

static Mpse* ac_ctor(
    SnortConfig* sc, class Module*, const MpseAgent* agent)
{
    return new Ac3Mpse(sc, agent);
}

static void ac_dtor(Mpse* p)
{
    delete p;
}

static void ac_init()
{
    acsm3_init_xlatcase();
}

static void ac_print()
{
    acsm3PrintSummaryInfo();
}

static const MpseApi ac3_api =
{
    {
        PT_SEARCH_ENGINE,
        sizeof(MpseApi),
        SEAPI_VERSION,
        0,
        API_RESERVED,
        API_OPTIONS,
        "ac_gpu",
        "Aho-Corasick Full MPSE GPU",
        nullptr,
        nullptr
    },
    MPSE_BASE,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    ac_ctor,
    ac_dtor,
    ac_init,
    ac_print,
};

#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
#else
const BaseApi* se_ac_gpu[] =
#endif

{
    &ac3_api.base,
    nullptr
};

