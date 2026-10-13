#pragma once
#include <dspsim/ndarray.h>
#include <dspsim/port.h>
#include <dspsim/signal.h>

#include <concepts>
#include <cstddef>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace dspsim
{
    class SignalStructArray;
    class PortStructArray;

    /*
        The type of what a field of a struct refers to: Signal<T>, InputArray<T>, PortStruct, ...

        Two of these are equal if they are of the same type, wherever each was made. That is not so for
        std::type_info: a model in an extension module of its own and the framework each have a std::type_info
        for a type, and with some standard libraries (libc++ on macOS) the two do not compare equal. This
        compares the names of the types, which are the same.
    */
    class StructFieldType
    {
    public:
        StructFieldType(const std::type_info &type) : name_(type.name()) {}

        friend bool operator==(const StructFieldType &a, const StructFieldType &b)
        {
            return a.name_ == b.name_ || std::strcmp(a.name_, b.name_) == 0;
        }

    private:
        const char *name_;
    };

    namespace detail
    {
        /// Names of the elements of an array, in row-major order: "name[0][1]".
        std::vector<std::string> struct_element_names(const std::string &name, const Shape &shape);

        // Errors of struct_field_elements(), which is a template.
        [[noreturn]] void throw_no_struct_field(const std::string &name, const std::string &field);
        [[noreturn]] void throw_struct_field_type(const std::string &name, const std::string &field);
        [[noreturn]] void throw_struct_field_shape(const std::string &name, const std::string &field);

        /// One field of every struct of an array, as the elements of one array in row-major order: the Scalar
        /// (Signal<T>, Input<T>, ...) of each struct, or the elements of the Array that the field is in each
        /// struct, whose shape is then appended to `shape`. Throws std::invalid_argument if a struct has no such
        /// field, or one of another type or shape.
        template <typename Scalar, typename Array, typename Structs>
        std::vector<Scalar *> struct_field_elements(const Structs &structs, const std::string &field, Shape &shape)
        {
            std::vector<Scalar *> elements;
            Shape field_shape;
            bool first = true;
            for (auto &element : structs)
            {
                auto *f = element.find(field);
                if (!f)
                {
                    throw_no_struct_field(element.name(), field);
                }
                Shape element_shape;
                if (f->type == typeid(Scalar))
                {
                    elements.push_back(static_cast<Scalar *>(f->object));
                }
                else if (f->type == typeid(Array))
                {
                    auto *array = static_cast<Array *>(f->object);
                    element_shape = array->shape();
                    for (auto &e : *array)
                    {
                        elements.push_back(&e);
                    }
                }
                else
                {
                    throw_struct_field_type(element.name(), field);
                }
                if (!first && element_shape != field_shape)
                {
                    throw_struct_field_shape(element.name(), field);
                }
                field_shape = std::move(element_shape);
                first = false;
            }
            shape.insert(shape.end(), field_shape.begin(), field_shape.end());
            return elements;
        }
    } // namespace detail

    /*
        A group of signals with named fields, like the value of a SystemVerilog struct or a VHDL record. A field
        is a signal, an array of signals, another SignalStruct or an array of them (a SignalStructArray).

        The signals for the ports of a module come from the port struct: `auto bus = dut.bus_out.signals("bus");`
        creates one signal per field, and `dut.bus_out.bind(bus); other.bus_in.bind(bus);` connects two modules.
        A SignalStruct can also be put together from existing signals with add().

        Copies of a SignalStruct refer to the same signals.
    */
    class SignalStruct
    {
    public:
        /// One field: the signal, signal array or nested struct that it refers to, with its type erased.
        struct Field
        {
            std::string name;
            /// The type of the object: Signal<T>, SignalArray<T>, SignalStruct or SignalStructArray.
            StructFieldType type;
            void *object;
            /// Keeps the object alive, if the struct owns it. Empty for an object that was added by reference.
            std::shared_ptr<void> owner;
        };

        explicit SignalStruct(std::string name = "");

        const std::string &name() const;
        /// The fields, in the order they were added.
        const std::vector<Field> &fields() const;
        /// The field of that name, or nullptr.
        const Field *find(const std::string &field) const;

        /// Add a field that refers to an existing signal or array, which must outlive the struct. Throws
        /// std::invalid_argument if there is a field of that name already.
        template <typename T>
        SignalStruct &add(const std::string &field, Signal<T> &signal)
        {
            return add_field({field, typeid(Signal<T>), &signal, nullptr});
        }
        template <typename T>
        SignalStruct &add(const std::string &field, SignalArray<T> &signals)
        {
            return add_field({field, typeid(SignalArray<T>), &signals, nullptr});
        }
        /// Add a field that the struct keeps alive.
        template <typename T>
        SignalStruct &add(const std::string &field, std::shared_ptr<Signal<T>> signal)
        {
            auto *object = signal.get();
            return add_field({field, typeid(Signal<T>), object, std::move(signal)});
        }
        template <typename T>
        SignalStruct &add(const std::string &field, std::shared_ptr<SignalArray<T>> signals)
        {
            auto *object = signals.get();
            return add_field({field, typeid(SignalArray<T>), object, std::move(signals)});
        }
        /// Add a nested struct, or an array of structs. It refers to the same signals as the argument.
        SignalStruct &add(const std::string &field, SignalStruct signals);
        SignalStruct &add(const std::string &field, SignalStructArray signals);

        /// The signal of a field. Throws std::invalid_argument if there is no such field or it is not a Signal<T>.
        template <typename T>
        Signal<T> &signal(const std::string &field) const
        {
            return *static_cast<Signal<T> *>(get(field, typeid(Signal<T>)));
        }
        /// The signal array of a field.
        template <typename T>
        SignalArray<T> &array(const std::string &field) const
        {
            return *static_cast<SignalArray<T> *>(get(field, typeid(SignalArray<T>)));
        }
        /// The nested struct of a field.
        SignalStruct &nested(const std::string &field) const;
        /// The array of structs of a field.
        SignalStructArray &struct_array(const std::string &field) const;

    private:
        SignalStruct &add_field(Field field);
        void *get(const std::string &field, const StructFieldType &type) const;

        std::string name_;
        std::vector<Field> fields_;
    };

    /*
        An array of signal structs, like the value of an array of SystemVerilog structs: what a PortStructArray
        binds to. `buses[{i, j}]` is a SignalStruct. The same signals are also reachable field by field, as a
        struct of arrays would have them: `buses.array<int16_t>("data")` is a view of the "data" signal of every
        element, with the shape of the array.

        Copies and views of a SignalStructArray refer to the same structs.
    */
    class SignalStructArray : public detail::NdView<SignalStruct>
    {
    public:
        /// The elements are given in row-major order. Throws std::invalid_argument if the shape has another
        /// number of elements.
        SignalStructArray(std::string name, Shape shape, std::vector<SignalStruct> elements);

        const std::string &name() const;

        /// Views of the whole array or a sub-selection. One Slice per leading dimension; the rest are kept whole.
        SignalStructArray view() const;
        SignalStructArray slice(const Slices &slices) const;
        SignalStructArray select(const std::vector<Range> &ranges) const;

        /// The signals of one field of every element, as one array. If the field is a signal array in each
        /// element, its dimensions come after those of this array. Throws std::invalid_argument if an element
        /// has no such field or it is not a Signal<T> or SignalArray<T>.
        template <typename T>
        SignalArrayView<T> array(const std::string &field) const
        {
            Shape shape = this->shape();
            auto elements = detail::struct_field_elements<Signal<T>, SignalArray<T>>(*this, field, shape);
            return detail::NdView<Signal<T>>(std::move(shape), std::move(elements));
        }
        /// The struct of one field of every element, as an array of structs. If the field is an array of
        /// structs in each element, its dimensions come after those of this array.
        SignalStructArray nested(const std::string &field) const;

    private:
        // The structs of an array and of its views.
        using Storage = std::shared_ptr<std::vector<SignalStruct>>;

        SignalStructArray(std::string name, Shape shape, Storage storage);
        SignalStructArray(std::string name, detail::NdView<SignalStruct> view, Storage storage);

        std::string name_;
        Storage storage_;
    };

    /*
        A group of ports with named fields: the struct (or record) port of a module. A field is an input, an
        output, an array of them, another PortStruct or an array of them (a PortStructArray). The ports are
        members of a class derived from PortStruct, which adds them in its constructor:

            struct BusIn : PortStruct
            {
                Input<int16_t> data;
                Input<uint8_t> valid;

                BusIn(const std::string &name) : PortStruct(name), data(name + ".data"), valid(name + ".valid")
                {
                    add("data", data);
                    add("valid", valid);
                }
            };

        The fields can be bound one by one (`bus_in.data.bind(sig)`) or all at once with bind(), which matches
        fields by name. The two sides must have the same fields, of the same types.
    */
    class PortStruct
    {
        // An array of structs checks and binds its elements as part of the struct that it is a field of.
        friend class PortStructArray;

    public:
        struct Field;

        /// What bind() and signals() do with a field of one type.
        struct Ops
        {
            /// typeid of what the field binds to in a SignalStruct.
            const std::type_info *signal_type;
            /// Bind the port to the object of a SignalStruct field of type signal_type.
            void (*bind_signal)(void *port, void *signal);
            /// Bind the port to a port of the same type.
            void (*bind_port)(void *port, void *other);
            /// Create what the port binds to, with that name, and add it to signals as a field.
            void (*add_signal)(const void *port, SignalStruct &signals, const std::string &field, const std::string &name);
        };

        /// One field: the port, port array or nested struct that it refers to, with its type erased.
        struct Field
        {
            std::string name;
            /// The type of the object: Input<T>, Output<T>, InputArray<T>, OutputArray<T>, PortStruct or
            /// PortStructArray.
            StructFieldType type;
            void *object;
            const Ops *ops;
        };

        explicit PortStruct(std::string name);
        // The fields point at the members of the derived class.
        PortStruct(const PortStruct &) = delete;
        PortStruct &operator=(const PortStruct &) = delete;

        const std::string &name() const;
        /// The fields, in the order they were added.
        const std::vector<Field> &fields() const;
        /// The field of that name, or nullptr.
        const Field *find(const std::string &field) const;

        /// Bind every field to the field of the same name in a struct of signals. Throws std::invalid_argument,
        /// without binding anything, unless both have the same fields and the signals have the types of the ports.
        void bind(const SignalStruct &signals);
        /// Bind every field to the field of the same name in the port struct of a parent module.
        void bind(PortStruct &ports);

        /// Create a signal (or signal array) for every field, with the type, width and shape of its port. The
        /// signals are named name.field and the context shares ownership of them.
        SignalStruct signals(const std::string &name) const;

        /// The port of a field. Throws std::invalid_argument if there is no such field or it has another type.
        template <typename T>
        Input<T> &input(const std::string &field) const
        {
            return *static_cast<Input<T> *>(get(field, typeid(Input<T>)));
        }
        template <typename T>
        Output<T> &output(const std::string &field) const
        {
            return *static_cast<Output<T> *>(get(field, typeid(Output<T>)));
        }
        template <typename T>
        InputArray<T> &input_array(const std::string &field) const
        {
            return *static_cast<InputArray<T> *>(get(field, typeid(InputArray<T>)));
        }
        template <typename T>
        OutputArray<T> &output_array(const std::string &field) const
        {
            return *static_cast<OutputArray<T> *>(get(field, typeid(OutputArray<T>)));
        }
        /// The nested struct of a field.
        PortStruct &nested(const std::string &field) const;
        /// The array of structs of a field.
        PortStructArray &struct_array(const std::string &field) const;

        /// Add a field that refers to a port, which must outlive the struct: normally a member of the derived
        /// class. Throws std::invalid_argument if there is a field of that name already.
        template <typename T>
        void add(const std::string &field, Input<T> &port);
        template <typename T>
        void add(const std::string &field, Output<T> &port);
        template <typename T>
        void add(const std::string &field, InputArray<T> &ports);
        template <typename T>
        void add(const std::string &field, OutputArray<T> &ports);
        void add(const std::string &field, PortStruct &ports);
        void add(const std::string &field, PortStructArray &ports);

    private:
        void add_field(Field field);
        void *get(const std::string &field, const StructFieldType &type) const;
        // Append what keeps this struct from binding to the other to errors, each with the path of its field.
        void check(const SignalStruct &signals, const std::string &path, std::vector<std::string> &errors) const;
        void check(const PortStruct &ports, const std::string &path, std::vector<std::string> &errors) const;
        void bind_checked(const SignalStruct &signals);
        void bind_checked(PortStruct &ports);

        std::string name_;
        std::vector<Field> fields_;
    };

    /*
        An array of port structs: the port of a module that is an array of structs. `buses[{i, j}]` is the
        PortStruct of one element, with ports named "buses[i][j].field". The ports of all elements are also
        reachable field by field, as a struct of arrays would have them: `buses.input_array<int16_t>("data")` is
        a view of the "data" port of every element, with the shape of the array.

        bind() binds every element to the element at the same place in a SignalStructArray, which signals()
        creates, or in the PortStructArray of a parent module.

        This class refers to structs that something else owns, and so does a view of it: the structs must
        outlive both. PortStructArrayOf<S> is the array that owns its structs.
    */
    class PortStructArray : public detail::NdView<PortStruct>
    {
        // A struct with an array of structs as a field checks and binds it with its other fields.
        friend class PortStruct;

    public:
        /// The elements are given in row-major order. Throws std::invalid_argument if the shape has another
        /// number of elements.
        PortStructArray(std::string name, Shape shape, std::vector<PortStruct *> elements);

        const std::string &name() const;

        /// Views of the whole array or a sub-selection. One Slice per leading dimension; the rest are kept whole.
        PortStructArray view() const;
        PortStructArray slice(const Slices &slices) const;
        PortStructArray select(const std::vector<Range> &ranges) const;

        /// Bind every element to the struct at the same place in an array of signal structs. Throws
        /// std::invalid_argument, without binding anything, unless the shapes are the same and every element
        /// binds to its struct (PortStruct::bind).
        void bind(const SignalStructArray &signals);
        /// Bind every element to the struct at the same place in the array of a parent module.
        void bind(const PortStructArray &ports);

        /// Create the signals of every element (PortStruct::signals). The structs are named name[i][j].
        SignalStructArray signals(const std::string &name) const;

        /// The ports of one field of every element, as one array. If the field is a port array in each element,
        /// its dimensions come after those of this array. Throws std::invalid_argument if an element has no
        /// such field or it has another type.
        template <typename T>
        InputArrayView<T> input_array(const std::string &field) const
        {
            Shape shape = this->shape();
            auto elements = detail::struct_field_elements<Input<T>, InputArray<T>>(*this, field, shape);
            return detail::NdView<Input<T>>(std::move(shape), std::move(elements));
        }
        template <typename T>
        OutputArrayView<T> output_array(const std::string &field) const
        {
            Shape shape = this->shape();
            auto elements = detail::struct_field_elements<Output<T>, OutputArray<T>>(*this, field, shape);
            return detail::NdView<Output<T>>(std::move(shape), std::move(elements));
        }
        /// The struct of one field of every element, as an array of structs. If the field is an array of
        /// structs in each element, its dimensions come after those of this array.
        PortStructArray nested(const std::string &field) const;

    private:
        PortStructArray(std::string name, detail::NdView<PortStruct> view);

        // Append what keeps this array from binding to the other to errors, each with the path of its field.
        void check(const SignalStructArray &signals, const std::string &path, std::vector<std::string> &errors) const;
        void check(const PortStructArray &ports, const std::string &path, std::vector<std::string> &errors) const;
        void bind_checked(const SignalStructArray &signals);
        void bind_checked(const PortStructArray &ports);

        std::string name_;
    };

    /*
        An array of port structs of class S (derived from PortStruct) that owns its structs:

            PortStructArrayOf<BusIn> buses{"buses", {2, 3}};
            buses[{1, 2}].data.read();

        The structs are constructed with their names, "buses[1][2]", or by a function that is given the name
        and the row-major index of each struct.
    */
    template <typename S>
    class PortStructArrayOf : public PortStructArray
    {
    public:
        using Factory = std::function<std::unique_ptr<S>(const std::string &, std::size_t)>;

        PortStructArrayOf(const std::string &name, const Shape &shape)
            requires std::constructible_from<S, const std::string &>
            : PortStructArrayOf(name, shape, [](const std::string &n, std::size_t)
                                { return std::make_unique<S>(n); })
        {
        }
        /// make(element_name, flat_index) constructs each struct.
        PortStructArrayOf(const std::string &name, const Shape &shape, const Factory &make)
            : PortStructArrayOf(name, shape, make_structs(name, shape, make))
        {
        }

        /// The struct of an element, as its own class. Bounds checked: throws std::out_of_range.
        S &flat(std::size_t i) const { return static_cast<S &>(PortStructArray::flat(i)); }
        S &at(const Index &idx) const { return static_cast<S &>(PortStructArray::at(idx)); }
        S &operator[](const Index &idx) const { return at(idx); }
        S &at(std::initializer_list<std::size_t> idx) const { return static_cast<S &>(PortStructArray::at(idx)); }
        S &operator[](std::initializer_list<std::size_t> idx) const { return at(idx); }

    private:
        using Structs = std::vector<std::unique_ptr<S>>;

        PortStructArrayOf(const std::string &name, const Shape &shape, Structs structs)
            : PortStructArray(name, shape, pointers(structs)), structs_(std::move(structs))
        {
        }

        static Structs make_structs(const std::string &name, const Shape &shape, const Factory &make)
        {
            Structs structs;
            std::size_t i = 0;
            for (auto &element_name : detail::struct_element_names(name, shape))
            {
                structs.push_back(make(element_name, i++));
            }
            return structs;
        }

        static std::vector<PortStruct *> pointers(const Structs &structs)
        {
            std::vector<PortStruct *> result;
            for (auto &s : structs)
            {
                result.push_back(s.get());
            }
            return result;
        }

        Structs structs_;
    };

    namespace detail
    {
        /// Field operations of a port P (Input<T> or Output<T>) that binds to a signal S (Signal<T>).
        template <typename P, typename S>
        struct PortFieldOps
        {
            static void bind_signal(void *port, void *signal)
            {
                static_cast<P *>(port)->bind(*static_cast<S *>(signal));
            }
            static void bind_port(void *port, void *other)
            {
                static_cast<P *>(port)->bind(*static_cast<P *>(other));
            }
            static void add_signal(const void *port, SignalStruct &signals, const std::string &field, const std::string &name)
            {
                signals.add(field, S::create(name, static_cast<const P *>(port)->width()));
            }
            static constexpr PortStruct::Ops ops{&typeid(S), &bind_signal, &bind_port, &add_signal};
        };

        /// Field operations of a port array P (InputArray<T> or OutputArray<T>) that binds to a SignalArray<T> S.
        template <typename P, typename S>
        struct PortArrayFieldOps
        {
            static void bind_signal(void *port, void *signal)
            {
                static_cast<P *>(port)->bind(*static_cast<S *>(signal));
            }
            static void bind_port(void *port, void *other)
            {
                static_cast<P *>(port)->bind(*static_cast<P *>(other));
            }
            static void add_signal(const void *port, SignalStruct &signals, const std::string &field, const std::string &name)
            {
                // The signals get the width of the first element. A port wider than 64 bits has a narrower
                // last word, which its signal does not need to know.
                auto *ports = static_cast<const P *>(port);
                if (ports->size() > 0)
                {
                    signals.add(field, S::create(name, ports->shape(), ports->flat(0).width()));
                }
                else
                {
                    signals.add(field, S::create(name, ports->shape()));
                }
            }
            static constexpr PortStruct::Ops ops{&typeid(S), &bind_signal, &bind_port, &add_signal};
        };
    } // namespace detail

    template <typename T>
    void PortStruct::add(const std::string &field, Input<T> &port)
    {
        add_field({field, typeid(Input<T>), &port, &detail::PortFieldOps<Input<T>, Signal<T>>::ops});
    }

    template <typename T>
    void PortStruct::add(const std::string &field, Output<T> &port)
    {
        add_field({field, typeid(Output<T>), &port, &detail::PortFieldOps<Output<T>, Signal<T>>::ops});
    }

    template <typename T>
    void PortStruct::add(const std::string &field, InputArray<T> &ports)
    {
        add_field({field, typeid(InputArray<T>), &ports, &detail::PortArrayFieldOps<InputArray<T>, SignalArray<T>>::ops});
    }

    template <typename T>
    void PortStruct::add(const std::string &field, OutputArray<T> &ports)
    {
        add_field({field, typeid(OutputArray<T>), &ports, &detail::PortArrayFieldOps<OutputArray<T>, SignalArray<T>>::ops});
    }
} // namespace dspsim
