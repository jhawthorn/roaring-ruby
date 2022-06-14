#include "ruby.h"

VALUE rb_mRoaring;

void
Init_roaring(void)
{
  rb_mRoaring = rb_define_module("Roaring");
}
