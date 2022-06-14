#include "ruby.h"
#include "roaring.h"

#include <stdio.h>

VALUE rb_mRoaring;

static void rb_roaring_free(void *data)
{
    roaring_bitmap_free(data);
}

static size_t rb_roaring_memsize(const void *data)
{
    // This is probably an estimate, "frozen" refers to the "frozen"
    // serialization format, which mimics the in-memory representation.
    return roaring_bitmap_frozen_size_in_bytes(data);
}

static const rb_data_type_t roaring_type = {
    .wrap_struct_name = "roaring/bitmap",
    .function = {
        .dfree = rb_roaring_free,
        .dsize = rb_roaring_memsize
    },
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE rb_roaring_alloc(VALUE self)
{
    roaring_bitmap_t *data = roaring_bitmap_create();
    return TypedData_Make_Struct(self, roaring_bitmap_t, &roaring_type, data);
}

static VALUE rb_roaring_cardinality(VALUE self)
{

    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);
    uint64_t cardinality = roaring_bitmap_get_cardinality(data);
    return ULONG2NUM(cardinality);
}

static VALUE rb_roaring_add(VALUE self, VALUE val)
{
    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);

    uint32_t num = NUM2UINT(val);
    roaring_bitmap_add(data, num);
    return self;
}

static VALUE rb_roaring_empty_p(VALUE self)
{
    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);

    return roaring_bitmap_is_empty(data) ? Qtrue : Qfalse;
}

static VALUE rb_roaring_clear(VALUE self)
{
    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);

    roaring_bitmap_clear(data);
    return self;
}

bool rb_roaring_each_i(uint32_t value, void *param) {
    rb_yield(UINT2NUM(value));
    return true;  // iterate till the end
}

static VALUE rb_roaring_each(VALUE self)
{
    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);

    roaring_iterate(data, rb_roaring_each_i, NULL);
    return self;
}

void
Init_roaring(void)
{
  rb_mRoaring = rb_define_module("Roaring");

  VALUE cRoaringBitmap = rb_define_class_under(rb_mRoaring, "Bitmap", rb_cObject);
  rb_define_alloc_func(cRoaringBitmap, rb_roaring_alloc);
  rb_define_method(cRoaringBitmap, "empty?", rb_roaring_empty_p, 0);
  rb_define_method(cRoaringBitmap, "clear", rb_roaring_clear, 0);
  rb_define_method(cRoaringBitmap, "cardinality", rb_roaring_cardinality, 0);
  rb_define_method(cRoaringBitmap, "add", rb_roaring_add, 1);
  rb_define_method(cRoaringBitmap, "<<", rb_roaring_add, 1);
  rb_define_method(cRoaringBitmap, "each", rb_roaring_each, 0);
}
