#include "roaring_ruby.h"

#include <stdio.h>

static VALUE cRoaringBitmap64;

static inline uint64_t
NUM2UINT64(VALUE num) {
    if (!FIXNUM_P(num) && !RB_TYPE_P(num, T_BIGNUM)) {
        rb_raise(rb_eTypeError, "wrong argument type %s (expected Integer)", rb_obj_classname(num));
    } else if ((SIGNED_VALUE)num < (SIGNED_VALUE)INT2FIX(0)) {
        rb_raise(rb_eRangeError, "Integer %"PRIdVALUE " must be >= 0 to use with Roaring::Bitmap32", num);
    } else {
        return NUM2ULL(num);
    }
}

static void rb_roaring64_free(void *data)
{
    roaring64_bitmap_free(data);
}

static size_t rb_roaring64_memsize(const void *data)
{
    // This is probably an estimate, "frozen" refers to the "frozen"
    // serialization format, which mimics the in-memory representation.
    //return sizeof(roaring64_bitmap_t) + roaring64_bitmap_frozen_size_in_bytes(data);
    return 0;
}

static const rb_data_type_t roaring64_type = {
    .wrap_struct_name = "roaring/bitmap64",
    .function = {
        .dfree = rb_roaring64_free,
        .dsize = rb_roaring64_memsize
    },
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE rb_roaring64_alloc(VALUE self)
{
    roaring64_bitmap_t *data = roaring64_bitmap_create();
    return TypedData_Wrap_Struct(self, &roaring64_type, data);
}

static roaring64_bitmap_t *get_bitmap(VALUE obj) {
    roaring64_bitmap_t *bitmap;
    TypedData_Get_Struct(obj, roaring64_bitmap_t, &roaring64_type, bitmap);
    return bitmap;
}

static VALUE rb_roaring64_replace(VALUE self, VALUE other) {
    roaring64_bitmap_t *self_data = get_bitmap(self);
    roaring64_bitmap_t *other_data = get_bitmap(other);

    // FIXME: Very likely a newer version of CRoaring will have
    //roaring64_bitmap_overwrite(self_data, other_data);

    roaring64_bitmap_clear(self_data);
    roaring64_bitmap_or_inplace(self_data, other_data);

    return self;
}

static VALUE rb_roaring64_cardinality(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);
    uint64_t cardinality = roaring64_bitmap_get_cardinality(data);
    return ULONG2NUM(cardinality);
}

static VALUE rb_roaring64_add(VALUE self, VALUE val)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    uint64_t num = NUM2UINT64(val);
    roaring64_bitmap_add(data, num);
    return self;
}

static VALUE rb_roaring64_add_p(VALUE self, VALUE val)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    uint64_t num = NUM2UINT64(val);
    return roaring64_bitmap_add_checked(data, num) ? self : Qnil;
}

static VALUE rb_roaring64_remove(VALUE self, VALUE val)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    uint64_t num = NUM2UINT64(val);
    roaring64_bitmap_remove(data, num);
    return self;
}

static VALUE rb_roaring64_remove_p(VALUE self, VALUE val)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    uint64_t num = NUM2UINT64(val);
    return roaring64_bitmap_remove_checked(data, num) ? self : Qnil;
}

static VALUE rb_roaring64_include_p(VALUE self, VALUE val)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    uint64_t num = NUM2UINT64(val);
    return RBOOL(roaring64_bitmap_contains(data, num));
}

static VALUE rb_roaring64_empty_p(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);
    return RBOOL(roaring64_bitmap_is_empty(data));
}

static VALUE rb_roaring64_clear(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);
    roaring64_bitmap_clear(data);
    return self;
}

bool rb_roaring64_each_i(uint64_t value, void *param) {
    rb_yield(ULL2NUM(value));
    return true;  // iterate till the end
}

static VALUE rb_roaring64_each(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);
    roaring64_bitmap_iterate(data, rb_roaring64_each_i, NULL);
    return self;
}

static VALUE rb_roaring64_aref(VALUE self, VALUE rankv)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    uint64_t rank = NUM2UINT64(rankv);
    uint64_t val;

    if (roaring64_bitmap_select(data, rank, &val)) {
        return ULL2NUM(val);
    } else {
        return Qnil;
    }
    return self;
}

static VALUE rb_roaring64_min(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    if (roaring64_bitmap_is_empty(data)) {
        return Qnil;
    } else {
        uint64_t val = roaring64_bitmap_minimum(data);
        return ULL2NUM(val);
    }
}

static VALUE rb_roaring64_max(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    if (roaring64_bitmap_is_empty(data)) {
        return Qnil;
    } else {
        uint64_t val = roaring64_bitmap_maximum(data);
        return ULL2NUM(val);
    }
}

static VALUE rb_roaring64_run_optimize(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);
    return RBOOL(roaring64_bitmap_run_optimize(data));
}

static VALUE rb_roaring64_serialize(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    size_t size = roaring64_bitmap_portable_size_in_bytes(data);
    VALUE str = rb_str_buf_new(size);

    size_t written = roaring64_bitmap_portable_serialize(data, RSTRING_PTR(str));
    rb_str_set_len(str, written);

    return str;
}

static VALUE rb_roaring64_deserialize(VALUE self, VALUE str)
{
    roaring64_bitmap_t *bitmap = roaring64_bitmap_portable_deserialize_safe(RSTRING_PTR(str), RSTRING_LEN(str));

    return TypedData_Wrap_Struct(cRoaringBitmap64, &roaring64_type, bitmap);
}

static VALUE rb_roaring64_statistics(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    roaring64_statistics_t stat;

    roaring64_bitmap_statistics(data, &stat);

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

typedef roaring64_bitmap_t *binary_func(const roaring64_bitmap_t *, const roaring64_bitmap_t *);
static VALUE rb_roaring64_binary_op(VALUE self, VALUE other, binary_func func) {
    roaring64_bitmap_t *self_data = get_bitmap(self);
    roaring64_bitmap_t *other_data = get_bitmap(other);

    roaring64_bitmap_t *result = func(self_data, other_data);

    return TypedData_Wrap_Struct(cRoaringBitmap64, &roaring64_type, result);
}

typedef void binary_func_inplace(roaring64_bitmap_t *, const roaring64_bitmap_t *);
static VALUE rb_roaring64_binary_op_inplace(VALUE self, VALUE other, binary_func_inplace func) {
    roaring64_bitmap_t *self_data = get_bitmap(self);
    roaring64_bitmap_t *other_data = get_bitmap(other);

    func(self_data, other_data);

    return self;
}

typedef bool binary_func_bool(const roaring64_bitmap_t *, const roaring64_bitmap_t *);
static VALUE rb_roaring64_binary_op_bool(VALUE self, VALUE other, binary_func_bool func) {
    roaring64_bitmap_t *self_data = get_bitmap(self);
    roaring64_bitmap_t *other_data = get_bitmap(other);

    bool result = func(self_data, other_data);
    return RBOOL(result);
}

static VALUE rb_roaring64_and_inplace(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_inplace(self, other, roaring64_bitmap_and_inplace);
}

static VALUE rb_roaring64_or_inplace(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_inplace(self, other, roaring64_bitmap_or_inplace);
}

static VALUE rb_roaring64_xor_inplace(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_inplace(self, other, roaring64_bitmap_xor_inplace);
}

static VALUE rb_roaring64_andnot_inplace(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_inplace(self, other, roaring64_bitmap_andnot_inplace);
}

static VALUE rb_roaring64_and(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op(self, other, roaring64_bitmap_and);
}

static VALUE rb_roaring64_or(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op(self, other, roaring64_bitmap_or);
}

static VALUE rb_roaring64_xor(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op(self, other, roaring64_bitmap_xor);
}

static VALUE rb_roaring64_andnot(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op(self, other, roaring64_bitmap_andnot);
}

static VALUE rb_roaring64_eq(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_bool(self, other, roaring64_bitmap_equals);
}

static VALUE rb_roaring64_lt(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_bool(self, other, roaring64_bitmap_is_strict_subset);
}

static VALUE rb_roaring64_lte(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_bool(self, other, roaring64_bitmap_is_subset);
}

static VALUE rb_roaring64_intersect_p(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_bool(self, other, roaring64_bitmap_intersect);
}

void
rb_roaring64_init(void)
{
  cRoaringBitmap64 = rb_define_class_under(rb_mRoaring, "Bitmap64", rb_cObject);
  rb_define_alloc_func(cRoaringBitmap64, rb_roaring64_alloc);
  rb_define_method(cRoaringBitmap64, "replace", rb_roaring64_replace, 1);
  rb_define_method(cRoaringBitmap64, "empty?", rb_roaring64_empty_p, 0);
  rb_define_method(cRoaringBitmap64, "clear", rb_roaring64_clear, 0);
  rb_define_method(cRoaringBitmap64, "cardinality", rb_roaring64_cardinality, 0);
  rb_define_method(cRoaringBitmap64, "add", rb_roaring64_add, 1);
  rb_define_method(cRoaringBitmap64, "add?", rb_roaring64_add_p, 1);
  rb_define_method(cRoaringBitmap64, "<<", rb_roaring64_add, 1);
  rb_define_method(cRoaringBitmap64, "remove", rb_roaring64_remove, 1);
  rb_define_method(cRoaringBitmap64, "remove?", rb_roaring64_remove_p, 1);
  rb_define_method(cRoaringBitmap64, "include?", rb_roaring64_include_p, 1);
  rb_define_method(cRoaringBitmap64, "each", rb_roaring64_each, 0);
  rb_define_method(cRoaringBitmap64, "[]", rb_roaring64_aref, 1);

  rb_define_method(cRoaringBitmap64, "and!", rb_roaring64_and_inplace, 1);
  rb_define_method(cRoaringBitmap64, "or!", rb_roaring64_or_inplace, 1);
  rb_define_method(cRoaringBitmap64, "xor!", rb_roaring64_xor_inplace, 1);
  rb_define_method(cRoaringBitmap64, "andnot!", rb_roaring64_andnot_inplace, 1);

  rb_define_method(cRoaringBitmap64, "&", rb_roaring64_and, 1);
  rb_define_method(cRoaringBitmap64, "|", rb_roaring64_or, 1);
  rb_define_method(cRoaringBitmap64, "^", rb_roaring64_xor, 1);
  rb_define_method(cRoaringBitmap64, "-", rb_roaring64_andnot, 1);

  rb_define_method(cRoaringBitmap64, "==", rb_roaring64_eq, 1);
  rb_define_method(cRoaringBitmap64, "<", rb_roaring64_lt, 1);
  rb_define_method(cRoaringBitmap64, "<=", rb_roaring64_lte, 1);
  rb_define_method(cRoaringBitmap64, "intersect?", rb_roaring64_intersect_p, 1);

  rb_define_method(cRoaringBitmap64, "min", rb_roaring64_min, 0);
  rb_define_method(cRoaringBitmap64, "max", rb_roaring64_max, 0);

  rb_define_method(cRoaringBitmap64, "run_optimize", rb_roaring64_run_optimize, 0);
  rb_define_method(cRoaringBitmap64, "statistics", rb_roaring64_statistics, 0);

  rb_define_method(cRoaringBitmap64, "serialize", rb_roaring64_serialize, 0);
  rb_define_singleton_method(cRoaringBitmap64, "deserialize", rb_roaring64_deserialize, 1);
}
