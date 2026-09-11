#include "roaring_ruby.h"

VALUE rb_mRoaring;
VALUE rb_cRoaringBitmap;

RUBY_FUNC_EXPORTED void
Init_roaring(void)
{
  rb_mRoaring = rb_define_module("Roaring");

  // Abstract base class of Bitmap32 and Bitmap64. The two widths are distinct
  // serialization formats, so this is never instantiated directly.
  rb_cRoaringBitmap = rb_define_class_under(rb_mRoaring, "Bitmap", rb_cObject);
  rb_undef_alloc_func(rb_cRoaringBitmap);

  rb_roaring32_init();
  rb_roaring64_init();
}
