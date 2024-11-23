#include <dspsim/dspsim.h>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <string>

#include "VSomeModel.h"

namespace nb = nanobind;

void some_func()
{
  VSomeModel some_model;
}

NB_MODULE(_framework, m)
{
  m.doc() = "nanobind hello module";

  m.def("hello_from_bin", &hello_from_bin);
  m.def("some_func", &some_func);
}
