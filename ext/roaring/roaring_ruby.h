#ifndef ROARING_RUBY_H
#define ROARING_RUBY_H

#include <ruby.h>

#include "roaring.h"

#ifndef RBOOL
#define RBOOL(x) ((x) ? Qtrue : Qfalse)
#endif

#ifndef RUBY_TYPED_EMBEDDABLE
#define RUBY_TYPED_EMBEDDABLE 0
#endif

extern VALUE rb_mRoaring;
extern VALUE rb_cRoaringBitmap;

static inline void
roaring_ruby_check_unsigned(VALUE num) {
    if (!FIXNUM_P(num) && !RB_TYPE_P(num, T_BIGNUM)) {
        rb_raise(rb_eTypeError, "wrong argument type %s (expected Integer)", rb_obj_classname(num));
    } else if ((SIGNED_VALUE)num < (SIGNED_VALUE)INT2FIX(0)) {
        rb_raise(rb_eRangeError, "Integer %"PRIdVALUE " must be >= 0 to use with Roaring::Bitmap", num);
    }
}

static inline uint32_t
NUM2UINT32(VALUE num) {
    roaring_ruby_check_unsigned(num);
    return FIX2UINT(num);
}

static inline uint64_t
NUM2UINT64(VALUE num) {
    roaring_ruby_check_unsigned(num);
    return NUM2ULL(num);
}

void rb_roaring32_init();
void rb_roaring64_init();

#endif
