#include <autoattach/lua_module.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/utility/path_helper.h>
#include <config/config.h>
#include <resolver/lua_signature.h>
#include <util/log.h>

#include <fstream>
#include <gumpp.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string_view>

using namespace std::string_view_literals;
namespace luadebug::autoattach {
    Config config;

    lua_version Config::get_lua_version() const {
        const auto key = "version"sv;

        auto it = values->find(key);
        if (!values || it == values->end()) {
            return lua_version::unknown;
        }

        return lua_version_from_string(it->get<std::string>());
    }

    std::optional<signture> Config::get_lua_signature(const std::string& key) const {
        const auto signture_key = "functions"sv;
        auto it                 = values->find(signture_key);
        if (it == values->end()) {
            return std::nullopt;
        }
        try {
            const auto& json = (*it)[key];
            // searilize json to signture
            signture res = {};
            json["start_offset"].get_to(res.start_offset);
            json["end_offset"].get_to(res.end_offset);
            json["pattern"].get_to(res.pattern);
            json["pattern_offset"].get_to(res.pattern_offset);
            json["hit_offset"].get_to(res.hit_offset);
            return res;
        } catch (const nlohmann::json::exception& e) {
            log::info("get_lua_signature {} error: {}", key, e.what());
        }
        return std::nullopt;
    }

    std::string Config::get_lua_module() const {
        const auto key = "module"sv;

        auto it = values->find(key);
        if (it == values->end()) {
            return {};
        }
        const auto& value = it->get<std::string>();
        if (!fs::exists(value))
            return {};

        return value;
    }

    bool Config::is_signature_mode() const {
        return !values->is_null();
    }

    bool Config::init_from_file() {
        config.values = std::make_unique<nlohmann::json>();
        auto dllpath  = bee::path_helper::dll_path();
        if (!dllpath) {
            return false;
        }

        auto filename = std::format("{}/tmp/pid_{}_config", dllpath.value().parent_path().parent_path().generic_string(), Gum::Process::get_id());

        std::ifstream s(filename, s.in);
        if (!s.is_open())
            return false;
        try {
            s >> *config.values;
        } catch (const nlohmann::json::exception& e) {
            log::info("init_from_file error: {}", e.what());
        }

        return true;
    }

}  // namespace luadebug::autoattach