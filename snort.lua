require('snort_config')

snort_lua_path = os.getenv('SNORT_LUA_PATH')

HOME_NET = 'any'
EXTERNAL_NET = 'any'

dofile(snort_lua_path .. '/snort_defaults.lua')
dofile(snort_lua_path .. '/file_magic.lua')

latency =
{
    packet = { max_time = 1500 },
    rule = { max_time = 200 },
}

references = default_references
classifications = default_classifications

--search_engine = { search_method = 'ac_full' }
--search_engine = { search_method = 'ac_std' }
search_engine = { search_method = 'ac_gpu' }
