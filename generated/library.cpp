// dspsim includes
#include <dspsim/dspsim.h>

// nanobind includes
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/filesystem.h>

// Include all generated modules in project

#include "HellModel.h"
#include "Skid.h"
#include "SimpleModel.h"
#include "SimpleModel16.h"
#include "SimpleModel8.h"

namespace nb = nanobind;

NB_MODULE(library, m)
{
    m.doc() = "library module";

    // Import dspsim._framework into the current module. Needed for proper type-stubs.
    auto _framework = m.import_("dspsim._framework");

    // Bind all generated modules here.
    
    HellModel::bind_HellModel(m);
    Skid::bind_Skid(m);
    SimpleModel::bind_SimpleModel(m);
    SimpleModel16::bind_SimpleModel16(m);
    SimpleModel8::bind_SimpleModel8(m);
}