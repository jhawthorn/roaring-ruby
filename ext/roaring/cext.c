#include "ruby.h"
#include "roaring.h"

#include <stdio.h>

VALUE rb_mRoaring;

int test_roaring() {
    roaring_bitmap_t *r1 = roaring_bitmap_create();
    for (uint32_t i = 100; i < 1000; i++) roaring_bitmap_add(r1, i);
    printf("cardinality = %d\n", (int) roaring_bitmap_get_cardinality(r1));
    roaring_bitmap_free(r1);
    return 0;
}

static const rb_data_type_t roaring_type = {
    .wrap_struct_name = "roaring/bitmap",
    .function = {
    },
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE rb_roaring_new(VALUE self)
{
    roaring_bitmap_t *data = roaring_bitmap_create();
    return TypedData_Make_Struct(self, roaring_bitmap_t, &roaring_type, data);
}

static VALUE rb_roaring_cardinality(VALUE self)
{

    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);
    uint64_t cardinality = roaring_bitmap_get_cardinality(data);
    return ULL2NUM(cardinality);
}

static VALUE rb_roaring_add(VALUE self, VALUE val)
{
    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);

    uint64_t num = NUM2ULL(val);
    roaring_bitmap_add(data, num);
    return self;
}

void
Init_roaring(void)
{
  rb_mRoaring = rb_define_module("Roaring");

  VALUE rb_cRoaringBitmap = rb_define_class_under(rb_mRoaring, "Bitmap", rb_cObject);
  rb_define_singleton_method(rb_cRoaringBitmap, "new", rb_roaring_new, 0);
  rb_define_method(rb_cRoaringBitmap, "cardinality", rb_roaring_cardinality, 0);
  rb_define_method(rb_cRoaringBitmap, "add", rb_roaring_add, 1);
  rb_define_method(rb_cRoaringBitmap, "<<", rb_roaring_add, 1);
}
