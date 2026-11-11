#pragma once

// Qt defines an `emit` keyword macro, while libghostty-vt's formatter options
// use `emit` as a regular field name. Hide the macro while the C header is
// parsed and restore it immediately afterwards.
#pragma push_macro("emit")
#undef emit
#include <ghostty/vt.h>
#pragma pop_macro("emit")
