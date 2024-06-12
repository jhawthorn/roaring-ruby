#ifndef ROARING_RUBY_H
#define ROARING_RUBY_H

#include <ruby.h>

#include "roaring.h"

#ifndef RBOOL
#define RBOOL(x) ((x) ? Qtrue : Qfalse)
#endif

extern VALUE rb_mRoaring;

void rb_roaring32_init();

#endif
