#pragma once
#include <dspsim/ndarray.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

/*
    What the value of a signal is made of, for code that handles a value without knowing its type: the Python
    bindings read and write a struct as a numpy structured value, and a field of a struct is reached by its
    name.

    A number, an enumeration and a std::array of them are described as they are. A struct is described by the
    list of its fields, which DSPSIM_STRUCT makes:

        struct Sample
        {
            uint32_t data = 0;
            std::array<int16_t, 2> iq{};
            bool operator==(const Sample &) const = default;
        };
        DSPSIM_STRUCT(Sample, data, iq)

    The macro goes after the struct, in the namespace of the struct. It does not change the struct: it defines
    a function that returns the name, the offset and the type of each listed field (a StructLayout), with the
    offsets and types taken from the struct itself. A field is a number, an enumeration, a struct that has a
    DSPSIM_STRUCT of its own, or a std::array of those; anything else is a compile error. A field that is
    not listed is still part of the value, but cannot be seen from Python.

    A struct only needs this to be used from Python, or for SignalBase::field(name). In C++ a signal of a
    struct works without it.
*/
namespace dspsim
{
    class SignalBase;
    struct StructLayout;

    /// What a value type is made of: a number, a struct, or an array of them.
    struct ValueType
    {
        enum class Kind : uint8_t
        {
            boolean,
            unsigned_integer,
            signed_integer,
            floating,
            structure,
        };

        Kind kind = Kind::unsigned_integer;
        /// Bytes of one element.
        std::size_t size = 0;
        /// Dimensions of an array (nested std::arrays), empty for a single value.
        Shape shape;
        /// The fields of a structure. nullptr for the other kinds.
        const StructLayout *layout = nullptr;
    };

    namespace detail
    {
        /// The storage of a value that a field is a part of: the signal that holds the value, and the field
        /// in its pending and in its committed value.
        struct FieldRoot
        {
            SignalBase *signal = nullptr;
            void *d = nullptr;
            const void *q = nullptr;
        };

        /// Creates the signal that is a view of one field (or of one element of an array field): a
        /// FieldSignal<E>, owned by the context.
        using FieldSignalFactory = SignalBase &(*)(const std::string &name, const FieldRoot &field);

        /// Defined in <dspsim/field_signal.h>.
        template <typename E>
        SignalBase &make_field_signal(const std::string &name, const FieldRoot &field);
    }

    /// One field of a struct.
    struct StructField
    {
        std::string name;
        /// Bytes from the start of the struct.
        std::size_t offset = 0;
        ValueType type;
        /// Creates a signal that is a view of this field, or of one element of it if it is an array.
        detail::FieldSignalFactory make_signal = nullptr;
    };

    /// The fields of a struct, in the order they were listed. See DSPSIM_STRUCT.
    struct StructLayout
    {
        std::string name;
        /// sizeof the struct, with its padding.
        std::size_t size = 0;
        std::vector<StructField> fields;

        /// The field of that name, or nullptr.
        const StructField *find(std::string_view field) const;
    };

    namespace detail
    {
        template <typename T>
        concept HasStructLayout = requires(const T *p) {
            { dspsim_struct_layout(p) } -> std::convertible_to<const StructLayout &>;
        };

        template <typename T>
        struct is_std_array : std::false_type
        {
        };
        template <typename T, std::size_t N>
        struct is_std_array<std::array<T, N>> : std::true_type
        {
        };

        /// The element type of nested std::arrays: T itself if it is not one.
        template <typename T>
        struct array_element
        {
            using type = T;
        };
        template <typename T, std::size_t N>
        struct array_element<std::array<T, N>> : array_element<T>
        {
        };
        template <typename T>
        using array_element_t = typename array_element<T>::type;

        /// A type that a ValueType can describe.
        template <typename T>
        concept DescribedValue = std::is_arithmetic_v<array_element_t<T>> || std::is_enum_v<array_element_t<T>> ||
                                 HasStructLayout<array_element_t<T>>;

        template <typename T>
        void describe_value(ValueType &type)
        {
            if constexpr (is_std_array<T>::value)
            {
                type.shape.push_back(std::tuple_size_v<T>);
                describe_value<typename T::value_type>(type);
            }
            else if constexpr (std::is_enum_v<T>)
            {
                describe_value<std::underlying_type_t<T>>(type);
            }
            else if constexpr (std::is_arithmetic_v<T>)
            {
                type.kind = std::is_same_v<T, bool>          ? ValueType::Kind::boolean
                            : std::is_floating_point_v<T>    ? ValueType::Kind::floating
                            : std::is_signed_v<T>            ? ValueType::Kind::signed_integer
                                                             : ValueType::Kind::unsigned_integer;
                type.size = sizeof(T);
            }
            else
            {
                type.kind = ValueType::Kind::structure;
                type.size = sizeof(T);
                type.layout = &dspsim_struct_layout(static_cast<const T *>(nullptr));
            }
        }

        // The next name of a list "a, b, c", which is then removed from the list.
        std::string next_field_name(std::string_view &names);

        template <typename S, typename F>
        void add_struct_field(StructLayout &layout, std::string_view &names, const S &base, const F &member)
        {
            static_assert(DescribedValue<F>,
                          "A field of a DSPSIM_STRUCT must be a number, an enumeration, a struct with a DSPSIM_STRUCT of its own, or a std::array of those");
            StructField field;
            field.name = next_field_name(names);
            field.offset = static_cast<std::size_t>(reinterpret_cast<const char *>(&member) - reinterpret_cast<const char *>(&base));
            describe_value<F>(field.type);
            field.make_signal = &make_field_signal<array_element_t<F>>;
            layout.fields.push_back(std::move(field));
        }

        /// The layout of S from the names of its fields ("a, b, c") and the fields themselves, as members of
        /// `base`.
        template <typename S, typename... F>
        StructLayout make_struct_layout(const char *name, std::string_view names, const S &base, const F &...fields)
        {
            static_assert(std::is_trivially_copyable_v<S>, "A DSPSIM_STRUCT must be trivially copyable: its values are copied as bytes");
            StructLayout layout;
            layout.name = name;
            layout.size = sizeof(S);
            (add_struct_field(layout, names, base, fields), ...);
            return layout;
        }
    } // namespace detail

    /// The description of a value type, or nullptr for a type that has none: a struct without a DSPSIM_STRUCT,
    /// or any other class.
    template <typename T>
    const ValueType *value_type_of()
    {
        if constexpr (detail::DescribedValue<T>)
        {
            static const ValueType type = []
            {
                ValueType result;
                detail::describe_value<T>(result);
                return result;
            }();
            return &type;
        }
        else
        {
            return nullptr;
        }
    }
} // namespace dspsim

/*
    Describe the fields of the struct S, for Python and for SignalBase::field(name). See the top of this file.

    Inside a member function of a class derived from S, the names of the fields are expressions, so the macro
    needs nothing per field: it passes the names as a string and the fields as references.
*/
#define DSPSIM_STRUCT(S, ...)                                                                                     \
    inline const ::dspsim::StructLayout &dspsim_struct_layout(const S *)                                          \
    {                                                                                                             \
        struct DspsimFields : S                                                                                   \
        {                                                                                                         \
            ::dspsim::StructLayout dspsim_layout() const                                                          \
            {                                                                                                     \
                return ::dspsim::detail::make_struct_layout<S>(#S, #__VA_ARGS__, static_cast<const S &>(*this), __VA_ARGS__); \
            }                                                                                                     \
        };                                                                                                        \
        static const ::dspsim::StructLayout layout = DspsimFields{}.dspsim_layout();                              \
        return layout;                                                                                            \
    }
