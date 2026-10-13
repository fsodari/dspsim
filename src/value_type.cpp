#include <dspsim/value_type.h>

namespace dspsim
{
    const StructField *StructLayout::find(std::string_view field) const
    {
        for (auto &f : fields)
        {
            if (f.name == field)
            {
                return &f;
            }
        }
        return nullptr;
    }

    namespace detail
    {
        std::string next_field_name(std::string_view &names)
        {
            const auto comma = names.find(',');
            std::string_view name = names.substr(0, comma);
            names = comma == std::string_view::npos ? std::string_view{} : names.substr(comma + 1);
            // The preprocessor leaves one space after each comma.
            while (!name.empty() && name.front() == ' ')
            {
                name.remove_prefix(1);
            }
            while (!name.empty() && name.back() == ' ')
            {
                name.remove_suffix(1);
            }
            return std::string(name);
        }
    } // namespace detail
} // namespace dspsim
