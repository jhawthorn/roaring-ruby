#include "ruby.h"
#include "roaring.h"

#include <stdio.h>

static VALUE cRoaringBitmap32;
VALUE rb_mRoaring;

#ifndef RBOOL
#define RBOOL(x) ((x) ? Qtrue : Qfalse)
#endif

static inline uint32_t
NUM2UINT32(VALUE num) {
    if (!FIXNUM_P(num) && !RB_TYPE_P(num, T_BIGNUM)) {
        rb_raise(rb_eTypeError, "wrong argument type %s (expected Integer)", rb_obj_classname(num));
    } else if ((SIGNED_VALUE)num < (SIGNED_VALUE)INT2FIX(0)) {
        rb_raise(rb_eRangeError, "Integer %"PRIdVALUE " must be >= 0 to use with Roaring::Bitmap", num);
    } else {
        return FIX2UINT(num);
    }
}

static void rb_roaring32_free(void *data)
{
    roaring_bitmap_free(data);
}

static size_t rb_roaring32_memsize(const void *data)
{
    // This is probably an estimate, "frozen" refers to the "frozen"
    // serialization format, which mimics the in-memory representation.
    return sizeof(roaring_bitmap_t) + roaring_bitmap_frozen_size_in_bytes(data);
}

static const rb_data_type_t roaring_type = {
    .wrap_struct_name = "roaring/bitmap",
    .function = {
        .dfree = rb_roaring32_free,
        .dsize = rb_roaring32_memsize
    },
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE rb_roaring32_alloc(VALUE self)
{
    roaring_bitmap_t *data = roaring_bitmap_create();
    return TypedData_Wrap_Struct(self, &roaring_type, data);
}

static roaring_bitmap_t *get_bitmap(VALUE obj) {
    roaring_bitmap_t *bitmap;
    TypedData_Get_Struct(obj, roaring_bitmap_t, &roaring_type, bitmap);
    return bitmap;
}

static VALUE rb_roaring32_replace(VALUE self, VALUE other) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    roaring_bitmap_overwrite(self_data, other_data);

    return self;
}

static VALUE rb_roaring32_cardinality(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    uint64_t cardinality = roaring_bitmap_get_cardinality(data);
    return ULONG2NUM(cardinality);
}

static VALUE rb_roaring32_add(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    roaring_bitmap_add(data, num);
    return self;
}

static VALUE rb_roaring32_add_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    return roaring_bitmap_add_checked(data, num) ? self : Qnil;
}

static VALUE rb_roaring32_remove(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    roaring_bitmap_remove(data, num);
    return self;
}

static VALUE rb_roaring32_remove_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    return roaring_bitmap_remove_checked(data, num) ? self : Qnil;
}

static VALUE rb_roaring32_include_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    return RBOOL(roaring_bitmap_contains(data, num));
}

static VALUE rb_roaring32_empty_p(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    return RBOOL(roaring_bitmap_is_empty(data));
}

static VALUE rb_roaring32_clear(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    roaring_bitmap_clear(data);
    return self;
}

bool rb_roaring32_each_i(uint32_t value, void *param) {
    rb_yield(UINT2NUM(value));
    return true;  // iterate till the end
}

static VALUE rb_roaring32_each(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    roaring_iterate(data, rb_roaring32_each_i, NULL);
    return self;
}

static VALUE rb_roaring32_aref(VALUE self, VALUE rankv)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t rank = NUM2UINT32(rankv);
    uint32_t val;

    if (roaring_bitmap_select(data, rank, &val)) {
        return UINT2NUM(val);
    } else {
        return Qnil;
    }
    return self;
}

static VALUE rb_roaring32_min(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);

    if (roaring_bitmap_is_empty(data)) {
        return Qnil;
    } else {
        uint32_t val = roaring_bitmap_minimum(data);
        return UINT2NUM(val);
    }
}

static VALUE rb_roaring32_max(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);

    if (roaring_bitmap_is_empty(data)) {
        return Qnil;
    } else {
        uint32_t val = roaring_bitmap_maximum(data);
        return UINT2NUM(val);
    }
}

static VALUE rb_roaring32_run_optimize(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    return RBOOL(roaring_bitmap_run_optimize(data));
}

static VALUE rb_roaring32_serialize(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);

    size_t size = roaring_bitmap_portable_size_in_bytes(data);
    VALUE str = rb_str_buf_new(size);

    size_t written = roaring_bitmap_portable_serialize(data, RSTRING_PTR(str));
    rb_str_set_len(str, written);

    return str;
}

static VALUE rb_roaring32_deserialize(VALUE self, VALUE str)
{
    roaring_bitmap_t *bitmap = roaring_bitmap_portable_deserialize_safe(RSTRING_PTR(str), RSTRING_LEN(str));

    return TypedData_Wrap_Struct(cRoaringBitmap32, &roaring_type, bitmap);
}

typedef roaring_bitmap_t *binary_func(const roaring_bitmap_t *, const roaring_bitmap_t *);
static VALUE rb_roaring32_binary_op(VALUE self, VALUE other, binary_func func) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    roaring_bitmap_t *result = func(self_data, other_data);

    return TypedData_Wrap_Struct(cRoaringBitmap32, &roaring_type, result);
}

typedef bool binary_func_bool(const roaring_bitmap_t *, const roaring_bitmap_t *);
static VALUE rb_roaring32_binary_op_bool(VALUE self, VALUE other, binary_func_bool func) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    bool result = func(self_data, other_data);
    return RBOOL(result);
}


static VALUE rb_roaring32_and(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op(self, other, roaring_bitmap_and);
}

static VALUE rb_roaring32_or(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op(self, other, roaring_bitmap_or);
}

static VALUE rb_roaring32_xor(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op(self, other, roaring_bitmap_xor);
}

static VALUE rb_roaring32_andnot(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op(self, other, roaring_bitmap_andnot);
}

static VALUE rb_roaring32_eq(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_bool(self, other, roaring_bitmap_equals);
}

static VALUE rb_roaring32_lt(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_bool(self, other, roaring_bitmap_is_strict_subset);
}

static VALUE rb_roaring32_lte(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_bool(self, other, roaring_bitmap_is_subset);
}

static VALUE rb_roaring32_intersect_p(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_bool(self, other, roaring_bitmap_intersect);
}

void
Init_roaring(void)
{
  rb_mRoaring = rb_define_module("Roaring");

  cRoaringBitmap32 = rb_define_class_under(rb_mRoaring, "Bitmap32", rb_cObject);
  rb_define_alloc_func(cRoaringBitmap32, rb_roaring32_alloc);
  rb_define_method(cRoaringBitmap32, "replace", rb_roaring32_replace, 1);
  rb_define_method(cRoaringBitmap32, "empty?", rb_roaring32_empty_p, 0);
  rb_define_method(cRoaringBitmap32, "clear", rb_roaring32_clear, 0);
  rb_define_method(cRoaringBitmap32, "cardinality", rb_roaring32_cardinality, 0);
  rb_define_method(cRoaringBitmap32, "add", rb_roaring32_add, 1);
  rb_define_method(cRoaringBitmap32, "add?", rb_roaring32_add_p, 1);
  rb_define_method(cRoaringBitmap32, "<<", rb_roaring32_add, 1);
  rb_define_method(cRoaringBitmap32, "remove", rb_roaring32_remove, 1);
  rb_define_method(cRoaringBitmap32, "remove?", rb_roaring32_remove_p, 1);
  rb_define_method(cRoaringBitmap32, "include?", rb_roaring32_include_p, 1);
  rb_define_method(cRoaringBitmap32, "each", rb_roaring32_each, 0);
  rb_define_method(cRoaringBitmap32, "[]", rb_roaring32_aref, 1);

  rb_define_method(cRoaringBitmap32, "&", rb_roaring32_and, 1);
  rb_define_method(cRoaringBitmap32, "|", rb_roaring32_or, 1);
  rb_define_method(cRoaringBitmap32, "^", rb_roaring32_xor, 1);
  rb_define_method(cRoaringBitmap32, "-", rb_roaring32_andnot, 1);

  rb_define_method(cRoaringBitmap32, "==", rb_roaring32_eq, 1);
  rb_define_method(cRoaringBitmap32, "<", rb_roaring32_lt, 1);
  rb_define_method(cRoaringBitmap32, "<=", rb_roaring32_lte, 1);
  rb_define_method(cRoaringBitmap32, "intersect?", rb_roaring32_intersect_p, 1);

  rb_define_method(cRoaringBitmap32, "min", rb_roaring32_min, 0);
  rb_define_method(cRoaringBitmap32, "max", rb_roaring32_max, 0);

  rb_define_method(cRoaringBitmap32, "run_optimize", rb_roaring32_run_optimize, 0);

  rb_define_method(cRoaringBitmap32, "serialize", rb_roaring32_serialize, 0);
  rb_define_singleton_method(cRoaringBitmap32, "deserialize", rb_roaring32_deserialize, 1);
}
