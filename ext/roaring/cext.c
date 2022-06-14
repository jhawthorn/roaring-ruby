#include "ruby.h"
#include "roaring.h"

#include <stdio.h>

static VALUE cRoaringBitmap;
VALUE rb_mRoaring;

static void rb_roaring_free(void *data)
{
    roaring_bitmap_free(data);
}

static size_t rb_roaring_memsize(const void *data)
{
    // This is probably an estimate, "frozen" refers to the "frozen"
    // serialization format, which mimics the in-memory representation.
    return sizeof(roaring_bitmap_t) + roaring_bitmap_frozen_size_in_bytes(data);
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
    return TypedData_Wrap_Struct(self, &roaring_type, data);
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

static VALUE rb_roaring_include_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);

    uint32_t num = NUM2UINT(val);
    return roaring_bitmap_contains(data, num) ? Qtrue : Qfalse;
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

static VALUE rb_roaring_min(VALUE self)
{
    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);

    uint32_t val = roaring_bitmap_minimum(data);
    return INT2NUM(val);
}

static VALUE rb_roaring_max(VALUE self)
{
    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);

    uint32_t val = roaring_bitmap_maximum(data);
    return INT2NUM(val);
}

static VALUE rb_roaring_run_optimize(VALUE self)
{
    roaring_bitmap_t *data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, data);

    return roaring_bitmap_run_optimize(data) ? Qtrue : Qfalse;
}

typedef roaring_bitmap_t *binary_func(const roaring_bitmap_t *, const roaring_bitmap_t *);
static VALUE rb_roaring_binary_op(VALUE self, VALUE other, binary_func func) {
    roaring_bitmap_t *self_data;
    TypedData_Get_Struct(self, roaring_bitmap_t, &roaring_type, self_data);

    roaring_bitmap_t *other_data;
    TypedData_Get_Struct(other, roaring_bitmap_t, &roaring_type, other_data);

    roaring_bitmap_t *result = func(self_data, other_data);

    return TypedData_Wrap_Struct(cRoaringBitmap, &roaring_type, result);
}

static VALUE rb_roaring_and(VALUE self, VALUE other)
{
    return rb_roaring_binary_op(self, other, roaring_bitmap_and);
}

static VALUE rb_roaring_or(VALUE self, VALUE other)
{
    return rb_roaring_binary_op(self, other, roaring_bitmap_or);
}

static VALUE rb_roaring_xor(VALUE self, VALUE other)
{
    return rb_roaring_binary_op(self, other, roaring_bitmap_xor);
}

static VALUE rb_roaring_andnot(VALUE self, VALUE other)
{
    return rb_roaring_binary_op(self, other, roaring_bitmap_andnot);
}

void
Init_roaring(void)
{
  rb_mRoaring = rb_define_module("Roaring");

  cRoaringBitmap = rb_define_class_under(rb_mRoaring, "Bitmap", rb_cObject);
  rb_define_alloc_func(cRoaringBitmap, rb_roaring_alloc);
  rb_define_method(cRoaringBitmap, "empty?", rb_roaring_empty_p, 0);
  rb_define_method(cRoaringBitmap, "clear", rb_roaring_clear, 0);
  rb_define_method(cRoaringBitmap, "cardinality", rb_roaring_cardinality, 0);
  rb_define_method(cRoaringBitmap, "add", rb_roaring_add, 1);
  rb_define_method(cRoaringBitmap, "<<", rb_roaring_add, 1);
  rb_define_method(cRoaringBitmap, "include?", rb_roaring_include_p, 1);
  rb_define_method(cRoaringBitmap, "each", rb_roaring_each, 0);

  rb_define_method(cRoaringBitmap, "&", rb_roaring_and, 1);
  rb_define_method(cRoaringBitmap, "|", rb_roaring_or, 1);
  rb_define_method(cRoaringBitmap, "^", rb_roaring_xor, 1);
  rb_define_method(cRoaringBitmap, "-", rb_roaring_andnot, 1);

  rb_define_method(cRoaringBitmap, "min", rb_roaring_min, 0);
  rb_define_method(cRoaringBitmap, "max", rb_roaring_max, 0);

  rb_define_method(cRoaringBitmap, "run_optimize", rb_roaring_run_optimize, 0);
}
