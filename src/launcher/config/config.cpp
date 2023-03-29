#include <autoattach/lua_module.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/utility/path_helper.h>
#include <config/config.h>
#include <util/log.h>

#include <fstream>
#include <gumpp.hpp>
#include <nlohmann/json.hpp>
#include <string_view>

using namespace std::string_view_literals;
namespace luadebug::config {
    using namespace autoattach;
    static lua_version get_lua_version(nlohmann::json& values) {
        const auto key = "version"sv;

        auto it = values.find(key);
        if (!values || it == values.end()) {
            return lua_version::unknown;
        }

        return lua_version_from_string(it->get<std::string>());
    }

    static std::string get_lua_module(nlohmann::json& values) {
        const auto key = "module"sv;

        auto it = values.find(key);
        if (it == values.end()) {
            return {};
        }
        auto value = it->get<std::string>();
        if (!fs::exists(value))
            return value;

        do {
            if (!fs::is_symlink(value))
                return value;
            auto link = fs::read_symlink(value);
            if (link.is_absolute())
                value = link.string();
            else
                value = (fs::path(value).parent_path() / link).lexically_normal().string();
        } while (true);
    }

    std::optional<Config> init_from_file() {
        Config config;
        nlohmann::json values;

        auto tmp = get_tmp_dir();
        if (!tmp)
            return std::nullopt;
        auto filename = ((*tmp) / std::format("ipc_{}_config", Gum::Process::get_id())).string();

        std::ifstream s(filename, s.binary);
        if (!s.is_open())
            return std::nullopt;
        try {
            s >> values;
        } catch (const nlohmann::json::exception& e) {
            log::info("init_from_file error: {}", e.what());
        }

        config.version    = get_lua_version(values);
        config.lua_module = get_lua_module(values);

        return config;
    }

    std::optional<fs::path> get_plugin_root() {
        auto dllpath = bee::path_helper::dll_path();
        if (!dllpath) {
            return std::nullopt;
        }
        return dllpath.value().parent_path().parent_path();
    }

    std::optional<fs::path> get_tmp_dir() {
        auto root = get_plugin_root();
        if (!root)
            return std::nullopt;
        return (*root) / "tmp";
    }

    std::optional<fs::path> get_runtime_dir() {
        auto root = get_plugin_root();
        if (!root)
            return std::nullopt;
        auto os =
#if defined(_WIN32)
            "windows";
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
            "darwin";
#else
            "linux";
#endif
        auto arch =
#if defined(_M_ARM64) || defined(__aarch64__)
            "arm64";
#elif defined(_M_IX86) || defined(__i386__)
            "x86";
#elif defined(_M_X64) || defined(__x86_64__)
            "x86_64";
#else
#    error "Unknown architecture"
#endif
        auto platform = std::format("{}-{}", os, arch);
        return (*root) / "runtime" / platform;
    }

    std::optional<fs::path> get_lua_runtime_dir(lua_version version) {
        auto runtime = get_runtime_dir();
        if (!runtime)
            return std::nullopt;
        return (*runtime) / lua_version_to_string(version);
    }

    std::optional<fs::path> get_luadebug_path(lua_version version) {
        auto runtime = get_lua_runtime_dir(version);
        if (!runtime)
            return std::nullopt;
#define LUADEBUG_FILE "luadebug"

#if defined(_WIN32)
#    define EXT ".dll"
#else
#    define EXT ".so"
#endif
        return (*runtime) / (LUADEBUG_FILE EXT);
    }

}  // namespace luadebug::autoattach