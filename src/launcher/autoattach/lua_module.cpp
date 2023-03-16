#include <autoattach/lua_module.h>
#include <hook/create_watchdog.h>
#include <util/log.h>
#include <config/config.h>

#include <bee/nonstd/format.h>
#include <bee/utility/path_helper.h>
#include <charconv>
#include <gumpp.hpp>

#include <dlfcn.h>

namespace luadebug::autoattach {
    static const char* lua_version_to_string(lua_version v) {
        switch (v) {
        case lua_version::lua51:
            return "lua51";
        case lua_version::lua52:
            return "lua52";
        case lua_version::lua53:
            return "lua53";
        case lua_version::lua54:
            return "lua54";
        case lua_version::luajit:
            return "luajit";
        default:
            return "unknown";
        }
    }

    static bool in_module(const lua_module& m, void* addr) {
        return addr > m.memory_address && addr <= (void*)((intptr_t)m.memory_address + m.memory_size);
    }

    static lua_version get_lua_version(const lua_module& m) {
		auto version = config.get_lua_version();
		if (version != lua_version::unknown)
			return version;
        /*
            luaJIT_version_2_1_0_beta3
            luaJIT_version_2_1_0_beta2
            luaJIT_version_2_1_0_beta1
            luaJIT_version_2_1_0_alpha
        */
        for (void* addr : Gum::SymbolUtil::find_matching_functions("luaJIT_version_2_1_0*", true)){
            if (in_module(m, addr))
                return lua_version::luajit;
        }
        auto p = Gum::Process::module_find_symbol_by_name(m.path.c_str(), "lua_ident");;
        const char *lua_ident = (const char *) p;
        if (!lua_ident)
            return lua_version::unknown;
        auto id = std::string_view(lua_ident);
        using namespace std::string_view_literals;
        constexpr auto key = "$Lua"sv;
        if (id.substr(0, key.length()) != key) {
            return lua_version::unknown;
        }
        if (id[key.length()] == ':') {
            //$Lua: Lua 5.1.*
            return lua_version::lua51;
        }
        id = id.substr("$LuaVersion: Lua 5."sv.length());
        int patch;
        auto res = std::from_chars(id.data(), id.data() + 3, patch, 10);
        if (res.ec != std::errc()) {
            return lua_version::unknown;
        }
        switch (patch) {
        case 1:
            return lua_version::lua51;
        case 2:
            return lua_version::lua52;
        case 3:
            return lua_version::lua53;
        case 4:
            return lua_version::lua54;
        default:
            return lua_version::unknown;
        }
		//TODO: from signature 
    }

	bool load_remotedebug_dll(lua_version version) {
		if (version != lua_version::unknown)
			return false;
		
		auto dllpath = bee::path_helper::dll_path();
		if (!dllpath) {
			return false;
		}
		auto os = 
#if defined(_WIN32)
		"windows"
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
		"darwin"
#else
		;return false;
#endif
		;
		auto arch = 
#if defined(_M_ARM64) || defined(__aarch64__)
		"arm64"
#elif defined(_M_IX86) || defined(__i386__)
		"x86"
#elif defined(_M_X64) || defined(__x86_64__)
		"x86_64"
#elif defined(__arm__)
		"arm"
#elif defined(__riscv)
		"riscv"
#else
		;return false;
#endif
		;
		auto platform = std::format("{}-{}", os, arch);
		auto path = dllpath.value().parent_path().parent_path() / "runtime" / platform /lua_version_to_string(version);
		
		dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);

        Gum::Process::module_enumerate_import(path.c_str(), [](const Gum::ImportDetails& details)->bool {
            if (std::string_view (details.name).find_first_of("lua") != 0){
                return true;
            }
            return true;
        });
        return true;
	}

    bool lua_module::initialize(fn_attach attach_lua_vm) {
        resolver.module_name = path;
        auto error_msg = lua::initialize(resolver);
        if (error_msg) {
            log::fatal("lua initialize failed, can't find {}", error_msg);
            return false;
        }
        version = get_lua_version(*this);
        log::info("current lua version: {}", lua_version_to_string(version));

		if (config.is_remotedebug_by_signature()) {
            load_remotedebug_dll(version);
		}

        watchdog = create_watchdog(attach_lua_vm, version, resolver);
        if (!watchdog) {
            //TODO: more errmsg
           log::fatal("watchdog initialize failed");
           return false;
        }
        return true;
    }
}