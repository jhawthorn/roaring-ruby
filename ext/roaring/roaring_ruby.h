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

// Converts a Range to closed bounds within [0, limit]. Returns false when the range is empty.
static inline bool
roaring_ruby_range_bounds(VALUE range, uint64_t limit, uint64_t *min, uint64_t *max)
{
    VALUE beg, end;
    int excl;
    rb_range_values(range, &beg, &end, &excl);

    uint64_t lo = NIL_P(beg) ? 0 : NUM2UINT64(beg);
    uint64_t hi = limit;
    if (!NIL_P(end)) {
        hi = NUM2UINT64(end);
        if (excl) {
            if (hi == 0) return false;
            hi--;
        }
    }
    if (lo > hi) return false;
    if (hi > limit) {
        rb_raise(rb_eRangeError, "Integer %"PRIu64" too big to use with Roaring::Bitmap", hi);
    }

    *min = lo;
    *max = hi;
    return true;
}

void rb_roaring32_init();
void rb_roaring64_init();

#endif
