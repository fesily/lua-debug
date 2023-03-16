#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <nlohmann/json_fwd.hpp>

namespace luadebug::autoattach {

    enum class lua_version;
    struct signture;

    class Config {
        std::unique_ptr<nlohmann::json> values;

    public:
        lua_version get_lua_version() const;

        std::optional<signture> get_lua_signature(const std::string& key) const;

        std::string get_lua_module() const;

        bool is_remotedebug_by_signature() const;

        static bool init_from_file();
    };

    extern Config config;
}// namespace luadebug::autoattach