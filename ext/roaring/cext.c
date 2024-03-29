#include "ruby.h"
#include "roaring.h"

#include <stdio.h>

static VALUE cRoaringBitmap;
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

static roaring_bitmap_t *get_bitmap(VALUE obj) {
    roaring_bitmap_t *bitmap;
    TypedData_Get_Struct(obj, roaring_bitmap_t, &roaring_type, bitmap);
    return bitmap;
}

static VALUE rb_roaring_initialize_copy(VALUE self, VALUE other) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    roaring_bitmap_overwrite(self_data, other_data);

    return self;
}

static VALUE rb_roaring_cardinality(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    uint64_t cardinality = roaring_bitmap_get_cardinality(data);
    return ULONG2NUM(cardinality);
}

static VALUE rb_roaring_add(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    roaring_bitmap_add(data, num);
    return self;
}

static VALUE rb_roaring_add_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    return roaring_bitmap_add_checked(data, num) ? self : Qnil;
}

static VALUE rb_roaring_remove(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    roaring_bitmap_remove(data, num);
    return self;
}

static VALUE rb_roaring_remove_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    return roaring_bitmap_remove_checked(data, num) ? self : Qnil;
}

static VALUE rb_roaring_include_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    return RBOOL(roaring_bitmap_contains(data, num));
}

static VALUE rb_roaring_empty_p(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    return RBOOL(roaring_bitmap_is_empty(data));
}

static VALUE rb_roaring_clear(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    roaring_bitmap_clear(data);
    return self;
}

bool rb_roaring_each_i(uint32_t value, void *param) {
    rb_yield(UINT2NUM(value));
    return true;  // iterate till the end
}

static VALUE rb_roaring_each(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    roaring_iterate(data, rb_roaring_each_i, NULL);
    return self;
}

static VALUE rb_roaring_aref(VALUE self, VALUE rankv)
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

static VALUE rb_roaring_min(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);

    if (roaring_bitmap_is_empty(data)) {
        return Qnil;
    } else {
        uint32_t val = roaring_bitmap_minimum(data);
        return UINT2NUM(val);
    }
}

static VALUE rb_roaring_max(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);

    if (roaring_bitmap_is_empty(data)) {
        return Qnil;
    } else {
        uint32_t val = roaring_bitmap_maximum(data);
        return UINT2NUM(val);
    }
}

static VALUE rb_roaring_run_optimize(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    return RBOOL(roaring_bitmap_run_optimize(data));
}

static VALUE rb_roaring_serialize(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);

    size_t size = roaring_bitmap_portable_size_in_bytes(data);
    VALUE str = rb_str_buf_new(size);

    size_t written = roaring_bitmap_portable_serialize(data, RSTRING_PTR(str));
    rb_str_set_len(str, written);

    return str;
}

static VALUE rb_roaring_deserialize(VALUE self, VALUE str)
{
    roaring_bitmap_t *bitmap = roaring_bitmap_portable_deserialize_safe(RSTRING_PTR(str), RSTRING_LEN(str));

    return TypedData_Wrap_Struct(cRoaringBitmap, &roaring_type, bitmap);
}

typedef roaring_bitmap_t *binary_func(const roaring_bitmap_t *, const roaring_bitmap_t *);
static VALUE rb_roaring_binary_op(VALUE self, VALUE other, binary_func func) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    roaring_bitmap_t *result = func(self_data, other_data);

    return TypedData_Wrap_Struct(cRoaringBitmap, &roaring_type, result);
}

typedef bool binary_func_bool(const roaring_bitmap_t *, const roaring_bitmap_t *);
static VALUE rb_roaring_binary_op_bool(VALUE self, VALUE other, binary_func_bool func) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    bool result = func(self_data, other_data);
    return RBOOL(result);
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

static VALUE rb_roaring_eq(VALUE self, VALUE other)
{
    return rb_roaring_binary_op_bool(self, other, roaring_bitmap_equals);
}

static VALUE rb_roaring_lt(VALUE self, VALUE other)
{
    return rb_roaring_binary_op_bool(self, other, roaring_bitmap_is_strict_subset);
}

static VALUE rb_roaring_lte(VALUE self, VALUE other)
{
    return rb_roaring_binary_op_bool(self, other, roaring_bitmap_is_subset);
}

static VALUE rb_roaring_intersect_p(VALUE self, VALUE other)
{
    return rb_roaring_binary_op_bool(self, other, roaring_bitmap_intersect);
}

void
Init_roaring(void)
{
  rb_mRoaring = rb_define_module("Roaring");

  cRoaringBitmap = rb_define_class_under(rb_mRoaring, "Bitmap", rb_cObject);
  rb_define_alloc_func(cRoaringBitmap, rb_roaring_alloc);
  rb_define_method(cRoaringBitmap, "initialize_copy", rb_roaring_initialize_copy, 1);
  rb_define_method(cRoaringBitmap, "empty?", rb_roaring_empty_p, 0);
  rb_define_method(cRoaringBitmap, "clear", rb_roaring_clear, 0);
  rb_define_method(cRoaringBitmap, "cardinality", rb_roaring_cardinality, 0);
  rb_define_method(cRoaringBitmap, "add", rb_roaring_add, 1);
  rb_define_method(cRoaringBitmap, "add?", rb_roaring_add_p, 1);
  rb_define_method(cRoaringBitmap, "<<", rb_roaring_add, 1);
  rb_define_method(cRoaringBitmap, "remove", rb_roaring_remove, 1);
  rb_define_method(cRoaringBitmap, "remove?", rb_roaring_remove_p, 1);
  rb_define_method(cRoaringBitmap, "include?", rb_roaring_include_p, 1);
  rb_define_method(cRoaringBitmap, "each", rb_roaring_each, 0);
  rb_define_method(cRoaringBitmap, "[]", rb_roaring_aref, 1);

  rb_define_method(cRoaringBitmap, "&", rb_roaring_and, 1);
  rb_define_method(cRoaringBitmap, "|", rb_roaring_or, 1);
  rb_define_method(cRoaringBitmap, "^", rb_roaring_xor, 1);
  rb_define_method(cRoaringBitmap, "-", rb_roaring_andnot, 1);

  rb_define_method(cRoaringBitmap, "==", rb_roaring_eq, 1);
  rb_define_method(cRoaringBitmap, "<", rb_roaring_lt, 1);
  rb_define_method(cRoaringBitmap, "<=", rb_roaring_lte, 1);
  rb_define_method(cRoaringBitmap, "intersect?", rb_roaring_intersect_p, 1);

  rb_define_method(cRoaringBitmap, "min", rb_roaring_min, 0);
  rb_define_method(cRoaringBitmap, "max", rb_roaring_max, 0);

  rb_define_method(cRoaringBitmap, "run_optimize", rb_roaring_run_optimize, 0);

  rb_define_method(cRoaringBitmap, "serialize", rb_roaring_serialize, 0);
  rb_define_singleton_method(cRoaringBitmap, "deserialize", rb_roaring_deserialize, 1);
}
