#include "roaring_ruby.h"

VALUE rb_mRoaring;

void
Init_roaring(void)
{
  rb_mRoaring = rb_define_module("Roaring");
  rb_roaring32_init();
  rb_roaring64_init();
}
