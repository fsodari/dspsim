#include <dspsim/dspsim.h>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <string>

namespace nb = nanobind;

NB_MODULE(_framework, m)
{
  m.doc() = "nanobind hello module...";

  m.def("hello_from_bin", &hello_from_bin);
}
