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

typedef struct {
    roaring_bitmap_t *bitmap;
    int iter_lev;
} rb_roaring32_t;

static void rb_roaring32_free(void *ptr)
{
    rb_roaring32_t *data = ptr;
    roaring_bitmap_free(data->bitmap);
}

static size_t rb_roaring32_memsize(const void *ptr)
{
    const rb_roaring32_t *data = ptr;
    // This is probably an estimate, "frozen" refers to the "frozen"
    // serialization format, which mimics the in-memory representation.
    return sizeof(roaring_bitmap_t) + roaring_bitmap_frozen_size_in_bytes(data->bitmap);
}

static const rb_data_type_t roaring_type = {
    .wrap_struct_name = "roaring/bitmap",
    .function = {
        .dfree = rb_roaring32_free,
        .dsize = rb_roaring32_memsize
    },
    .flags = RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_EMBEDDABLE,
};

static VALUE rb_roaring32_wrap(VALUE klass, roaring_bitmap_t *bitmap)
{
    rb_roaring32_t *data;
    VALUE obj = TypedData_Make_Struct(klass, rb_roaring32_t, &roaring_type, data);
    data->bitmap = bitmap;
    return obj;
}

static VALUE rb_roaring32_alloc(VALUE self)
{
    return rb_roaring32_wrap(self, roaring_bitmap_create());
}

static rb_roaring32_t *get_data(VALUE obj) {
    rb_roaring32_t *data;
    TypedData_Get_Struct(obj, rb_roaring32_t, &roaring_type, data);
    return data;
}

static roaring_bitmap_t *get_bitmap(VALUE obj) {
    return get_data(obj)->bitmap;
}

static roaring_bitmap_t *get_mutable_bitmap(VALUE obj) {
    rb_check_frozen(obj);
    rb_roaring32_t *data = get_data(obj);
    if (data->iter_lev > 0) {
        rb_raise(rb_eRuntimeError, "can't modify bitmap during iteration");
    }
    return data->bitmap;
}

// Replaces the contents of `self` with another bitmap
static VALUE rb_roaring32_replace(VALUE self, VALUE other) {
    roaring_bitmap_t *self_data = get_mutable_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    roaring_bitmap_overwrite(self_data, other_data);

    return self;
}

// @return [Integer] the number of elements in the bitmap
static VALUE rb_roaring32_cardinality(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    uint64_t cardinality = roaring_bitmap_get_cardinality(data);
    return ULL2NUM(cardinality);
}

// Adds an element from the bitmap
// @param val [Integer] the value to add
static VALUE rb_roaring32_add(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_mutable_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    roaring_bitmap_add(data, num);
    return self;
}

// Adds an element from the bitmap
// @see {add}
// @return `self` if value was add, `nil` if value was already in the bitmap
static VALUE rb_roaring32_add_p(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_mutable_bitmap(self);

    uint32_t num = NUM2UINT32(val);
    return roaring_bitmap_add_checked(data, num) ? self : Qnil;
}

static VALUE rb_roaring32_add_range_closed(VALUE self, VALUE minv, VALUE maxv)
{
    roaring_bitmap_t *data = get_mutable_bitmap(self);

    uint32_t min = NUM2UINT32(minv);
    uint32_t max = NUM2UINT32(maxv);

    roaring_bitmap_add_range_closed(data, min, max);

    return self;
}

// Removes an element from the bitmap
static VALUE rb_roaring32_remove(VALUE self, VALUE val)
{
    roaring_bitmap_t *data = get_mutable_bitmap(self);

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
    roaring_bitmap_t *data = get_mutable_bitmap(self);

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
    roaring_bitmap_t *data = get_mutable_bitmap(self);
    roaring_bitmap_clear(data);
    return self;
}

bool rb_roaring32_each_i(uint32_t value, void *param) {
    rb_yield(UINT2NUM(value));
    return true;  // iterate till the end
}

static VALUE rb_roaring32_each_size(VALUE self, VALUE args, VALUE eobj)
{
    return rb_roaring32_cardinality(self);
}

// Iterates over every element in the bitmap
// @return [self,Enumerator] `self`, or an Enumerator if no block is given
static VALUE rb_roaring32_each_call(VALUE self)
{
    roaring_iterate(get_bitmap(self), rb_roaring32_each_i, NULL);
    return self;
}

static VALUE rb_roaring32_each_ensure(VALUE self)
{
    get_data(self)->iter_lev--;
    return Qnil;
}

static VALUE rb_roaring32_iterate(VALUE self, VALUE (*body)(VALUE), VALUE arg)
{
    if (RB_OBJ_FROZEN(self)) {
        return body(arg);
    }

    get_data(self)->iter_lev++;
    return rb_ensure(body, arg, rb_roaring32_each_ensure, self);
}

static VALUE rb_roaring32_each(VALUE self)
{
    RETURN_SIZED_ENUMERATOR(self, 0, 0, rb_roaring32_each_size);
    return rb_roaring32_iterate(self, rb_roaring32_each_call, self);
}

struct rb_roaring32_each_from_args {
    VALUE self;
    uint32_t min;
};

static VALUE rb_roaring32_each_from_size(VALUE self, VALUE args, VALUE eobj)
{
    roaring_bitmap_t *data = get_bitmap(self);
    uint32_t min = NUM2UINT32(RARRAY_AREF(args, 0));
    uint64_t below = min == 0 ? 0 : roaring_bitmap_rank(data, min - 1);
    return ULL2NUM(roaring_bitmap_get_cardinality(data) - below);
}

static VALUE rb_roaring32_each_from_call(VALUE arg)
{
    struct rb_roaring32_each_from_args *args = (void *)arg;
    roaring_uint32_iterator_t it;
    roaring_iterator_init(get_bitmap(args->self), &it);
    roaring_uint32_iterator_move_equalorlarger(&it, args->min);
    while (it.has_value) {
        rb_yield(UINT2NUM(it.current_value));
        roaring_uint32_iterator_advance(&it);
    }
    return args->self;
}

// Iterates in ascending order over every element greater than or equal to `min`
// @return [self,Enumerator] `self`, or an Enumerator if no block is given
static VALUE rb_roaring32_each_from(VALUE self, VALUE minv)
{
    struct rb_roaring32_each_from_args args = { self, NUM2UINT32(minv) };
    RETURN_SIZED_ENUMERATOR(self, 1, &minv, rb_roaring32_each_from_size);
    return rb_roaring32_iterate(self, rb_roaring32_each_from_call, (VALUE)&args);
}

static VALUE rb_roaring32_reverse_each_call(VALUE self)
{
    roaring_uint32_iterator_t it;
    roaring_iterator_init_last(get_bitmap(self), &it);
    while (it.has_value) {
        rb_yield(UINT2NUM(it.current_value));
        roaring_uint32_iterator_previous(&it);
    }
    return self;
}

// Iterates over every element in the bitmap in descending order
// @return [self,Enumerator] `self`, or an Enumerator if no block is given
static VALUE rb_roaring32_reverse_each(VALUE self)
{
    RETURN_SIZED_ENUMERATOR(self, 0, 0, rb_roaring32_each_size);
    return rb_roaring32_iterate(self, rb_roaring32_reverse_each_call, self);
}

static bool rb_roaring32_hash_i(uint32_t value, void *param) {
    st_index_t *hash = param;
    *hash = rb_hash_uint(*hash, value);
    return true;
}

// @return [Integer] a hash value consistent with {#eql?}
static VALUE rb_roaring32_hash(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);

    st_index_t hash = rb_hash_start((st_index_t)rb_obj_class(self));
    roaring_iterate(data, rb_roaring32_hash_i, &hash);

    return ST2FIX(rb_hash_end(hash));
}

static bool rb_roaring32_to_a_i(uint32_t value, void *param) {
    rb_ary_push((VALUE)param, UINT2NUM(value));
    return true;
}

// @return [Array<Integer>] the elements in ascending order
static VALUE rb_roaring32_to_a(VALUE self)
{
    roaring_bitmap_t *data = get_bitmap(self);
    VALUE ary = rb_ary_new_capa(roaring_bitmap_get_cardinality(data));
    roaring_iterate(data, rb_roaring32_to_a_i, (void *)ary);
    return ary;
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

// @return [Integer,nil,Array<Integer>] the smallest element (or `nil`), or the first `n` elements when `n` is given
static VALUE rb_roaring32_first(int argc, VALUE *argv, VALUE self)
{
    rb_check_arity(argc, 0, 1);
    roaring_bitmap_t *data = get_bitmap(self);

    if (argc == 0) {
        return roaring_bitmap_is_empty(data) ? Qnil : UINT2NUM(roaring_bitmap_minimum(data));
    }

    long n = NUM2LONG(argv[0]);
    if (n < 0) {
        rb_raise(rb_eArgError, "attempt to take negative size");
    }
    uint64_t cardinality = roaring_bitmap_get_cardinality(data);
    if ((uint64_t)n > cardinality) {
        n = (long)cardinality;
    }

    VALUE ary = rb_ary_new_capa(n);
    roaring_uint32_iterator_t it;
    roaring_iterator_init(data, &it);
    for (long i = 0; i < n; i++) {
        rb_ary_push(ary, UINT2NUM(it.current_value));
        roaring_uint32_iterator_advance(&it);
    }
    return ary;
}

// Find the smallest integer in the bitmap
// @return [Integer,nil] The smallest integer in the bitmap, or `nil` if it is empty
static VALUE rb_roaring32_min(int argc, VALUE *argv, VALUE self)
{
    if (argc > 0 || rb_block_given_p()) {
        return rb_call_super(argc, argv);
    }

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
static VALUE rb_roaring32_max(int argc, VALUE *argv, VALUE self)
{
    if (argc > 0 || rb_block_given_p()) {
        return rb_call_super(argc, argv);
    }

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
    roaring_bitmap_t *data = get_mutable_bitmap(self);
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
// @param str [String] a string previously returned from {#serialize}
// @raise [ArgumentError] if the string isn't a valid serialized bitmap
// @return [Bitmap32]
static VALUE rb_roaring32_deserialize(VALUE self, VALUE str)
{
    StringValue(str);

    roaring_bitmap_t *bitmap = roaring_bitmap_portable_deserialize_safe(RSTRING_PTR(str), RSTRING_LEN(str));
    RB_GC_GUARD(str);

    if (!bitmap) {
        rb_raise(rb_eArgError, "invalid Roaring::Bitmap32 serialization");
    }

    return rb_roaring32_wrap(self, bitmap);
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

    return rb_roaring32_wrap(rb_obj_class(self), result);
}

typedef void binary_func_inplace(roaring_bitmap_t *, const roaring_bitmap_t *);
static VALUE rb_roaring32_binary_op_inplace(VALUE self, VALUE other, binary_func_inplace func) {
    roaring_bitmap_t *self_data = get_mutable_bitmap(self);
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

typedef uint64_t binary_func_cardinality(const roaring_bitmap_t *, const roaring_bitmap_t *);
static VALUE rb_roaring32_binary_op_cardinality(VALUE self, VALUE other, binary_func_cardinality func) {
    roaring_bitmap_t *self_data = get_bitmap(self);
    roaring_bitmap_t *other_data = get_bitmap(other);

    uint64_t result = func(self_data, other_data);
    return ULL2NUM(result);
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

// Computes the cardinality of the intersection between two bitmaps without
// materializing the result
// @return [Integer] the number of elements in both `self` and `other`
static VALUE rb_roaring32_and_cardinality(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_cardinality(self, other, roaring_bitmap_and_cardinality);
}

// Computes the cardinality of the union between two bitmaps without
// materializing the result
// @return [Integer] the number of elements in either `self` or `other`
static VALUE rb_roaring32_or_cardinality(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_cardinality(self, other, roaring_bitmap_or_cardinality);
}

// Computes the cardinality of the exclusive or between two bitmaps without
// materializing the result
// @return [Integer] the number of elements in one of `self` or `other`, but not both
static VALUE rb_roaring32_xor_cardinality(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_cardinality(self, other, roaring_bitmap_xor_cardinality);
}

// Computes the cardinality of the difference between two bitmaps without
// materializing the result
// @return [Integer] the number of elements in `self` but not in `other`
static VALUE rb_roaring32_andnot_cardinality(VALUE self, VALUE other)
{
    return rb_roaring32_binary_op_cardinality(self, other, roaring_bitmap_andnot_cardinality);
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
    if (!rb_obj_is_kind_of(other, cRoaringBitmap32)) {
        return Qfalse;
    }
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
  cRoaringBitmap32 = rb_define_class_under(rb_mRoaring, "Bitmap32", rb_cRoaringBitmap);
  rb_define_alloc_func(cRoaringBitmap32, rb_roaring32_alloc);
  rb_define_method(cRoaringBitmap32, "replace", rb_roaring32_replace, 1);
  rb_define_method(cRoaringBitmap32, "empty?", rb_roaring32_empty_p, 0);
  rb_define_method(cRoaringBitmap32, "clear", rb_roaring32_clear, 0);
  rb_define_method(cRoaringBitmap32, "cardinality", rb_roaring32_cardinality, 0);
  rb_define_method(cRoaringBitmap32, "add", rb_roaring32_add, 1);
  rb_define_method(cRoaringBitmap32, "add?", rb_roaring32_add_p, 1);
  rb_define_method(cRoaringBitmap32, "add_range_closed", rb_roaring32_add_range_closed, 2);
  rb_define_method(cRoaringBitmap32, "remove", rb_roaring32_remove, 1);
  rb_define_method(cRoaringBitmap32, "remove?", rb_roaring32_remove_p, 1);
  rb_define_method(cRoaringBitmap32, "include?", rb_roaring32_include_p, 1);
  rb_define_method(cRoaringBitmap32, "each", rb_roaring32_each, 0);
  rb_define_method(cRoaringBitmap32, "reverse_each", rb_roaring32_reverse_each, 0);
  rb_define_method(cRoaringBitmap32, "each_from", rb_roaring32_each_from, 1);
  rb_define_method(cRoaringBitmap32, "[]", rb_roaring32_aref, 1);

  rb_define_method(cRoaringBitmap32, "and!", rb_roaring32_and_inplace, 1);
  rb_define_method(cRoaringBitmap32, "or!", rb_roaring32_or_inplace, 1);
  rb_define_method(cRoaringBitmap32, "xor!", rb_roaring32_xor_inplace, 1);
  rb_define_method(cRoaringBitmap32, "andnot!", rb_roaring32_andnot_inplace, 1);

  rb_define_method(cRoaringBitmap32, "and", rb_roaring32_and, 1);
  rb_define_method(cRoaringBitmap32, "or", rb_roaring32_or, 1);
  rb_define_method(cRoaringBitmap32, "xor", rb_roaring32_xor, 1);
  rb_define_method(cRoaringBitmap32, "andnot", rb_roaring32_andnot, 1);

  rb_define_method(cRoaringBitmap32, "and_cardinality", rb_roaring32_and_cardinality, 1);
  rb_define_method(cRoaringBitmap32, "or_cardinality", rb_roaring32_or_cardinality, 1);
  rb_define_method(cRoaringBitmap32, "xor_cardinality", rb_roaring32_xor_cardinality, 1);
  rb_define_method(cRoaringBitmap32, "andnot_cardinality", rb_roaring32_andnot_cardinality, 1);

  rb_define_method(cRoaringBitmap32, "==", rb_roaring32_eq, 1);
  rb_define_method(cRoaringBitmap32, "hash", rb_roaring32_hash, 0);
  rb_define_method(cRoaringBitmap32, "to_a", rb_roaring32_to_a, 0);
  rb_define_method(cRoaringBitmap32, "<", rb_roaring32_lt, 1);
  rb_define_method(cRoaringBitmap32, "<=", rb_roaring32_lte, 1);
  rb_define_method(cRoaringBitmap32, "intersect?", rb_roaring32_intersect_p, 1);

  rb_define_method(cRoaringBitmap32, "first", rb_roaring32_first, -1);
  rb_define_method(cRoaringBitmap32, "min", rb_roaring32_min, -1);
  rb_define_method(cRoaringBitmap32, "max", rb_roaring32_max, -1);

  rb_define_method(cRoaringBitmap32, "run_optimize", rb_roaring32_run_optimize, 0);
  rb_define_method(cRoaringBitmap32, "statistics", rb_roaring32_statistics, 0);

  rb_define_method(cRoaringBitmap32, "serialize", rb_roaring32_serialize, 0);
  rb_define_singleton_method(cRoaringBitmap32, "deserialize", rb_roaring32_deserialize, 1);
}
