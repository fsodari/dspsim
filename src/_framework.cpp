#include <dspsim/dspsim.h>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <string>

namespace nb = nanobind;

NB_MODULE(_framework, m)
{
  m.doc() = "nanobind hello module...";

  m.def("hello_from_bin", &dspsim::hello_from_bin);
  m.def("foo42", []()
        { return 42; });
}
