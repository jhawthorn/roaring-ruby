#include "roaring_ruby.h"

#include <stdio.h>

static VALUE cRoaringBitmap32;

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

// Replaces the contents of `self` with another bitmap
static VALUE rb_roaring32_replace(VALUE self, VALUE other) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    roaring_bitmap_overwrite(self_data, other_data);

    return self;
}

// @return [Integer] the number of elements in the bitmap
static VALUE rb_roaring32_cardinality(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    uint64_t cardinality = roaring_bitmap_get_cardinality(data);
    return ULONG2NUM(cardinality);
}

// Adds an element from the bitmap
// @param val [Integer] the value to add
static VALUE rb_roaring32_add(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    roaring_bitmap_add(data, num);
    return self;
}

// Adds an element from the bitmap
// @see {add}
// @return `self` if value was add, `nil` if value was already in the bitmap
static VALUE rb_roaring32_add_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    return roaring_bitmap_add_checked(data, num) ? self : Qnil;
}

// Removes an element from the bitmap
static VALUE rb_roaring32_remove(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    roaring_bitmap_remove(data, num);
    return self;
}

// Removes an element from the bitmap
//
// @see {remove}
// @return [self,nil] `self` if value was removed, `nil` if the value wasn't in the bitmap
static VALUE rb_roaring32_remove_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    return roaring_bitmap_remove_checked(data, num) ? self : Qnil;
}

// @return [Boolean] `true` if the bitmap is contains `val`, otherwise `false`
static VALUE rb_roaring32_include_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    return RBOOL(roaring_bitmap_contains(data, num));
}

// @return [Boolean] `true` if the bitmap is empty, otherwise `false`
static VALUE rb_roaring32_empty_p(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    return RBOOL(roaring_bitmap_is_empty(data));
}

// Removes all elements from the bitmap
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

// Iterates over every element in the bitmap
// @return [self]
static VALUE rb_roaring32_each(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    roaring_iterate(data, rb_roaring32_each_i, NULL);
    return self;
}

// Find the nth smallest integer in the bitmap
// @return [Integer,nil] The nth integer in the bitmap, or `nil` if `rankv` is `>= cardinality`
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

// Find the smallest integer in the bitmap
// @return [Integer,nil] The smallest integer in the bitmap, or `nil` if it is empty
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

// Find the largest integer in the bitmap
// @return [Integer,nil] The largest integer in the bitmap, or `nil` if it is empty
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

// Attemps to internally optimize the representation of bitmap by finding runs, consecutive sequences of integers.
// @return [Boolean] whether the result has at least one run container
static VALUE rb_roaring32_run_optimize(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    return RBOOL(roaring_bitmap_run_optimize(data));
}

// Serializes a bitmap into a string
// @return [string]
static VALUE rb_roaring32_serialize(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);

    size_t size = roaring_bitmap_portable_size_in_bytes(data);
    VALUE str = rb_str_buf_new(size);

    size_t written = roaring_bitmap_portable_serialize(data, RSTRING_PTR(str));
    rb_str_set_len(str, written);

    return str;
}

// Loads a previously serialized bitmap
// @return [Bitmap32]
static VALUE rb_roaring32_deserialize(VALUE self, VALUE str)
{
    roaring_bitmap_t *bitmap = roaring_bitmap_portable_deserialize_safe(RSTRING_PTR(str), RSTRING_LEN(str));

    return TypedData_Wrap_Struct(cRoaringBitmap32, &roaring_type, bitmap);
}

// Provides statistics about the internal layout of the bitmap
// @return [Hash]
static VALUE rb_roaring32_statistics(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);

    roaring_statistics_t stat;

    roaring_bitmap_statistics(data, &stat);

    VALUE ret = rb_hash_new();
#define ADD_STAT(name) \
    rb_hash_aset(ret, rb_id2sym(rb_intern(#name)), ULL2NUM(stat.name))

    ADD_STAT(n_containers);
    ADD_STAT(n_array_containers);
    ADD_STAT(n_run_containers);
    ADD_STAT(n_bitset_containers);
    ADD_STAT(n_values_array_containers);
    ADD_STAT(n_values_run_containers);
    ADD_STAT(n_values_bitset_containers);
    ADD_STAT(n_bytes_array_containers);
    ADD_STAT(n_bytes_run_containers);
    ADD_STAT(n_bytes_bitset_containers);
    ADD_STAT(max_value);
    ADD_STAT(min_value);
    // ADD_STAT(sum_value); // deprecated, skipped
    ADD_STAT(cardinality);

#undef ADD_STAT

    return ret;
}

typedef roaring_bitmap_t *binary_func(const roaring_bitmap_t *, const roaring_bitmap_t *);
static VALUE rb_roaring32_binary_op(VALUE self, VALUE other, binary_func func) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    roaring_bitmap_t *result = func(self_data, other_data);

    return TypedData_Wrap_Struct(cRoaringBitmap32, &roaring_type, result);
}

typedef void binary_func_inplace(roaring_bitmap_t *, const roaring_bitmap_t *);
static VALUE rb_roaring32_binary_op_inplace(VALUE self, VALUE other, binary_func_inplace func) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    func(self_data, other_data);

    return self;
}

typedef bool binary_func_bool(const roaring_bitmap_t *, const roaring_bitmap_t *);
static VALUE rb_roaring32_binary_op_bool(VALUE self, VALUE other, binary_func_bool func) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    bool result = func(self_data, other_data);
    return RBOOL(result);
}

// Inplace version of {and}
// @return [self] the modified Bitmap
static VALUE rb_roaring32_and_inplace(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_inplace(self, other, roaring_bitmap_and_inplace);
}

// Inplace version of {or}
// @return [self] the modified Bitmap
static VALUE rb_roaring32_or_inplace(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_inplace(self, other, roaring_bitmap_or_inplace);
}

// Inplace version of {xor}
// @return [self] the modified Bitmap
static VALUE rb_roaring32_xor_inplace(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_inplace(self, other, roaring_bitmap_xor_inplace);
}

// Inplace version of {andnot}
// @return [self] the modified Bitmap
static VALUE rb_roaring32_andnot_inplace(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_inplace(self, other, roaring_bitmap_andnot_inplace);
}

// Computes the intersection between two bitmaps
// @return [Bitmap32] a new bitmap containing all elements in both `self` and `other`
static VALUE rb_roaring32_and(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op(self, other, roaring_bitmap_and);
}

// Computes the union between two bitmaps
// @return [Bitmap32] a new bitmap containing all elements in either `self` or `other`
static VALUE rb_roaring32_or(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op(self, other, roaring_bitmap_or);
}

// Computes the exclusive or between two bitmaps
// @return [Bitmap32] a new bitmap containing all elements in one of `self` or `other`, but not both
static VALUE rb_roaring32_xor(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op(self, other, roaring_bitmap_xor);
}

// Computes the difference between two bitmaps
// @return [Bitmap32] a new bitmap containing all elements in `self`, but not in `other`
static VALUE rb_roaring32_andnot(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op(self, other, roaring_bitmap_andnot);
}

// Compare equality between two bitmaps
// @return [Boolean] `true` if both bitmaps contain all the same elements, otherwise `false`
static VALUE rb_roaring32_eq(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_bool(self, other, roaring_bitmap_equals);
}

// Check if `self` is a strict subset of `other`. A strict subset requires every element in `self` is also in `other`, but they aren't exactly equal.
// @return [Boolean] `true` if `self` is a strict subset of `other`, otherwise `false`
static VALUE rb_roaring32_lt(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_bool(self, other, roaring_bitmap_is_strict_subset);
}

// Check if `self` is a (non-strict) subset of `other`. A subset requires that every element in `self` is also in `other`. They may be equal.
// @return [Boolean] `true` if `self` is a subset of `other`, otherwise `false`
static VALUE rb_roaring32_lte(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_bool(self, other, roaring_bitmap_is_subset);
}

// Checks whether `self` intersects `other`
// @return [Boolean] `true` if `self` intersects `other`, otherwise `false`
static VALUE rb_roaring32_intersect_p(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_bool(self, other, roaring_bitmap_intersect);
}

void
rb_roaring32_init(void)
{
  cRoaringBitmap32 = rb_define_class_under(rb_mRoaring, "Bitmap32", rb_cObject);
  rb_define_alloc_func(cRoaringBitmap32, rb_roaring32_alloc);
  rb_define_method(cRoaringBitmap32, "replace", rb_roaring32_replace, 1);
  rb_define_method(cRoaringBitmap32, "empty?", rb_roaring32_empty_p, 0);
  rb_define_method(cRoaringBitmap32, "clear", rb_roaring32_clear, 0);
  rb_define_method(cRoaringBitmap32, "cardinality", rb_roaring32_cardinality, 0);
  rb_define_method(cRoaringBitmap32, "add", rb_roaring32_add, 1);
  rb_define_method(cRoaringBitmap32, "add?", rb_roaring32_add_p, 1);
  rb_define_method(cRoaringBitmap32, "remove", rb_roaring32_remove, 1);
  rb_define_method(cRoaringBitmap32, "remove?", rb_roaring32_remove_p, 1);
  rb_define_method(cRoaringBitmap32, "include?", rb_roaring32_include_p, 1);
  rb_define_method(cRoaringBitmap32, "each", rb_roaring32_each, 0);
  rb_define_method(cRoaringBitmap32, "[]", rb_roaring32_aref, 1);

  rb_define_method(cRoaringBitmap32, "and!", rb_roaring32_and_inplace, 1);
  rb_define_method(cRoaringBitmap32, "or!", rb_roaring32_or_inplace, 1);
  rb_define_method(cRoaringBitmap32, "xor!", rb_roaring32_xor_inplace, 1);
  rb_define_method(cRoaringBitmap32, "andnot!", rb_roaring32_andnot_inplace, 1);

  rb_define_method(cRoaringBitmap32, "and", rb_roaring32_and, 1);
  rb_define_method(cRoaringBitmap32, "or", rb_roaring32_or, 1);
  rb_define_method(cRoaringBitmap32, "xor", rb_roaring32_xor, 1);
  rb_define_method(cRoaringBitmap32, "andnot", rb_roaring32_andnot, 1);

  rb_define_method(cRoaringBitmap32, "==", rb_roaring32_eq, 1);
  rb_define_method(cRoaringBitmap32, "<", rb_roaring32_lt, 1);
  rb_define_method(cRoaringBitmap32, "<=", rb_roaring32_lte, 1);
  rb_define_method(cRoaringBitmap32, "intersect?", rb_roaring32_intersect_p, 1);

  rb_define_method(cRoaringBitmap32, "min", rb_roaring32_min, 0);
  rb_define_method(cRoaringBitmap32, "max", rb_roaring32_max, 0);

  rb_define_method(cRoaringBitmap32, "run_optimize", rb_roaring32_run_optimize, 0);
  rb_define_method(cRoaringBitmap32, "statistics", rb_roaring32_statistics, 0);

  rb_define_method(cRoaringBitmap32, "serialize", rb_roaring32_serialize, 0);
  rb_define_singleton_method(cRoaringBitmap32, "deserialize", rb_roaring32_deserialize, 1);
}
