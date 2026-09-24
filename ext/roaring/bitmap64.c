#include "roaring_ruby.h"

#include <stdio.h>

static VALUE cRoaringBitmap64;
static ID id_each;

typedef struct {
    roaring64_bitmap_t *bitmap;
    int iter_lev;
} rb_roaring64_t;

static void rb_roaring64_free(void *ptr)
{
    rb_roaring64_t *data = ptr;
    roaring64_bitmap_free(data->bitmap);
}

static size_t rb_roaring64_memsize(const void *ptr)
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
    .flags = RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_EMBEDDABLE,
};

static VALUE rb_roaring64_wrap(VALUE klass, roaring64_bitmap_t *bitmap)
{
    rb_roaring64_t *data;
    VALUE obj = TypedData_Make_Struct(klass, rb_roaring64_t, &roaring64_type, data);
    data->bitmap = bitmap;
    return obj;
}

static VALUE rb_roaring64_alloc(VALUE self)
{
    return rb_roaring64_wrap(self, roaring64_bitmap_create());
}

static rb_roaring64_t *get_data(VALUE obj) {
    rb_roaring64_t *data;
    TypedData_Get_Struct(obj, rb_roaring64_t, &roaring64_type, data);
    return data;
}

static roaring64_bitmap_t *get_bitmap(VALUE obj) {
    return get_data(obj)->bitmap;
}

static roaring64_bitmap_t *get_mutable_bitmap(VALUE obj) {
    rb_check_frozen(obj);
    rb_roaring64_t *data = get_data(obj);
    if (data->iter_lev > 0) {
        rb_raise(rb_eRuntimeError, "can't modify bitmap during iteration");
    }
    return data->bitmap;
}

static VALUE rb_roaring64_replace(VALUE self, VALUE other) {
    roaring64_bitmap_t *self_data = get_mutable_bitmap(self);
    roaring64_bitmap_t *other_data = get_bitmap(other);

    roaring64_bitmap_overwrite(self_data, other_data);

    return self;
}

static VALUE rb_roaring64_cardinality(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);
    uint64_t cardinality = roaring64_bitmap_get_cardinality(data);
    return ULL2NUM(cardinality);
}

static VALUE rb_roaring64_add(VALUE self, VALUE val)
{
    roaring64_bitmap_t *data = get_mutable_bitmap(self);

    uint64_t num = NUM2UINT64(val);
    roaring64_bitmap_add(data, num);
    return self;
}

static VALUE rb_roaring64_add_p(VALUE self, VALUE val)
{
    roaring64_bitmap_t *data = get_mutable_bitmap(self);

    uint64_t num = NUM2UINT64(val);
    return roaring64_bitmap_add_checked(data, num) ? self : Qnil;
}

static VALUE rb_roaring64_add_range_closed(VALUE self, VALUE minv, VALUE maxv)
{
    roaring64_bitmap_t *data = get_mutable_bitmap(self);

    uint64_t min = NUM2UINT64(minv);
    uint64_t max = NUM2UINT64(maxv);

    roaring64_bitmap_add_range_closed(data, min, max);

    return self;
}

static VALUE rb_roaring64_remove(VALUE self, VALUE val)
{
    roaring64_bitmap_t *data = get_mutable_bitmap(self);

    uint64_t num = NUM2UINT64(val);
    roaring64_bitmap_remove(data, num);
    return self;
}

static VALUE rb_roaring64_remove_p(VALUE self, VALUE val)
{
    roaring64_bitmap_t *data = get_mutable_bitmap(self);

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
    roaring64_bitmap_t *data = get_mutable_bitmap(self);
    roaring64_bitmap_clear(data);
    return self;
}

bool rb_roaring64_each_i(uint64_t value, void *param) {
    rb_yield(ULL2NUM(value));
    return true;  // iterate till the end
}

static VALUE rb_roaring64_each_size(VALUE self, VALUE args, VALUE eobj)
{
    return rb_roaring64_cardinality(self);
}

static VALUE rb_roaring64_each_call(VALUE self)
{
    roaring64_bitmap_iterate(get_bitmap(self), rb_roaring64_each_i, NULL);
    return self;
}

static VALUE rb_roaring64_each_ensure(VALUE self)
{
    get_data(self)->iter_lev--;
    return Qnil;
}

static VALUE rb_roaring64_iterate(VALUE self, VALUE (*body)(VALUE), VALUE arg)
{
    if (RB_OBJ_FROZEN(self)) {
        return body(arg);
    }

    get_data(self)->iter_lev++;
    return rb_ensure(body, arg, rb_roaring64_each_ensure, self);
}

static VALUE rb_roaring64_each(VALUE self)
{
    RETURN_SIZED_ENUMERATOR(self, 0, 0, rb_roaring64_each_size);
    return rb_roaring64_iterate(self, rb_roaring64_each_call, self);
}

static VALUE rb_roaring64_reverse_each_loop(VALUE arg)
{
    roaring64_iterator_t *it = (roaring64_iterator_t *)arg;
    while (roaring64_iterator_has_value(it)) {
        rb_yield(ULL2NUM(roaring64_iterator_value(it)));
        roaring64_iterator_previous(it);
    }
    return Qnil;
}

static VALUE rb_roaring64_iterator_free_ensure(VALUE arg)
{
    roaring64_iterator_free((roaring64_iterator_t *)arg);
    return Qnil;
}

static VALUE rb_roaring64_reverse_each_call(VALUE self)
{
    roaring64_iterator_t *it = roaring64_iterator_create_last(get_bitmap(self));
    rb_ensure(rb_roaring64_reverse_each_loop, (VALUE)it, rb_roaring64_iterator_free_ensure, (VALUE)it);
    return self;
}

// Iterates over every element in the bitmap in descending order
// @return [self,Enumerator] `self`, or an Enumerator if no block is given
static VALUE rb_roaring64_reverse_each(VALUE self)
{
    RETURN_SIZED_ENUMERATOR(self, 0, 0, rb_roaring64_each_size);
    return rb_roaring64_iterate(self, rb_roaring64_reverse_each_call, self);
}

struct rb_roaring64_each_from_args {
    VALUE self;
    uint64_t min;
};

static VALUE rb_roaring64_each_from_size(VALUE self, VALUE args, VALUE eobj)
{
    roaring64_bitmap_t *data = get_bitmap(self);
    uint64_t min = NUM2UINT64(RARRAY_AREF(args, 0));
    uint64_t below = min == 0 ? 0 : roaring64_bitmap_rank(data, min - 1);
    return ULL2NUM(roaring64_bitmap_get_cardinality(data) - below);
}

static VALUE rb_roaring64_each_from_loop(VALUE arg)
{
    roaring64_iterator_t *it = (roaring64_iterator_t *)arg;
    while (roaring64_iterator_has_value(it)) {
        rb_yield(ULL2NUM(roaring64_iterator_value(it)));
        roaring64_iterator_advance(it);
    }
    return Qnil;
}

static VALUE rb_roaring64_each_from_call(VALUE arg)
{
    struct rb_roaring64_each_from_args *args = (void *)arg;
    roaring64_iterator_t *it = roaring64_iterator_create(get_bitmap(args->self));
    roaring64_iterator_move_equalorlarger(it, args->min);
    rb_ensure(rb_roaring64_each_from_loop, (VALUE)it, rb_roaring64_iterator_free_ensure, (VALUE)it);
    return args->self;
}

// Iterates in ascending order over every element greater than or equal to `min`
// @return [self,Enumerator] `self`, or an Enumerator if no block is given
static VALUE rb_roaring64_each_from(VALUE self, VALUE minv)
{
    struct rb_roaring64_each_from_args args = { self, NUM2UINT64(minv) };
    RETURN_SIZED_ENUMERATOR(self, 1, &minv, rb_roaring64_each_from_size);
    return rb_roaring64_iterate(self, rb_roaring64_each_from_call, (VALUE)&args);
}

static bool rb_roaring64_hash_i(uint64_t value, void *param) {
    st_index_t *hash = param;
    *hash = rb_hash_uint(*hash, (st_index_t)value);
    return true;
}

static VALUE rb_roaring64_hash(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);

    st_index_t hash = rb_hash_start((st_index_t)rb_obj_class(self));
    roaring64_bitmap_iterate(data, rb_roaring64_hash_i, &hash);

    return ST2FIX(rb_hash_end(hash));
}

static bool rb_roaring64_to_a_i(uint64_t value, void *param) {
    rb_ary_push((VALUE)param, ULL2NUM(value));
    return true;
}

// @return [Array<Integer>] the elements in ascending order
static VALUE rb_roaring64_to_a(VALUE self)
{
    roaring64_bitmap_t *data = get_bitmap(self);
    VALUE ary = rb_ary_new_capa(roaring64_bitmap_get_cardinality(data));
    roaring64_bitmap_iterate(data, rb_roaring64_to_a_i, (void *)ary);
    return ary;
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

static VALUE rb_roaring64_first(int argc, VALUE *argv, VALUE self)
{
    rb_check_arity(argc, 0, 1);
    roaring64_bitmap_t *data = get_bitmap(self);

    if (argc == 0) {
        return roaring64_bitmap_is_empty(data) ? Qnil : ULL2NUM(roaring64_bitmap_minimum(data));
    }

    long n = NUM2LONG(argv[0]);
    if (n < 0) {
        rb_raise(rb_eArgError, "attempt to take negative size");
    }
    uint64_t cardinality = roaring64_bitmap_get_cardinality(data);
    if ((uint64_t)n > cardinality) {
        n = (long)cardinality;
    }

    VALUE tmp;
    uint64_t *buf = ALLOCV_N(uint64_t, tmp, n);
    roaring64_iterator_t *it = roaring64_iterator_create(data);
    n = (long)roaring64_iterator_read(it, buf, n);
    roaring64_iterator_free(it);

    VALUE ary = rb_ary_new_capa(n);
    for (long i = 0; i < n; i++) {
        rb_ary_push(ary, ULL2NUM(buf[i]));
    }
    ALLOCV_END(tmp);
    return ary;
}

static VALUE rb_roaring64_min(int argc, VALUE *argv, VALUE self)
{
    if (argc > 0 || rb_block_given_p()) {
        return rb_call_super(argc, argv);
    }

    roaring64_bitmap_t *data = get_bitmap(self);

    if (roaring64_bitmap_is_empty(data)) {
        return Qnil;
    } else {
        uint64_t val = roaring64_bitmap_minimum(data);
        return ULL2NUM(val);
    }
}

static VALUE rb_roaring64_max(int argc, VALUE *argv, VALUE self)
{
    if (argc > 0 || rb_block_given_p()) {
        return rb_call_super(argc, argv);
    }

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
    roaring64_bitmap_t *data = get_mutable_bitmap(self);
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
    StringValue(str);

    roaring64_bitmap_t *bitmap = roaring64_bitmap_portable_deserialize_safe(RSTRING_PTR(str), RSTRING_LEN(str));
    RB_GC_GUARD(str);

    if (!bitmap) {
        rb_raise(rb_eArgError, "invalid Roaring::Bitmap64 serialization");
    }

    return rb_roaring64_wrap(self, bitmap);
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

typedef bool binary_func_bool(const roaring64_bitmap_t *, const roaring64_bitmap_t *);
static VALUE rb_roaring64_binary_op_bool(VALUE self, VALUE other, binary_func_bool func) {
    roaring64_bitmap_t *self_data = get_bitmap(self);
    roaring64_bitmap_t *other_data = get_bitmap(other);

    bool result = func(self_data, other_data);
    return RBOOL(result);
}

typedef uint64_t binary_func_cardinality(const roaring64_bitmap_t *, const roaring64_bitmap_t *);
static VALUE rb_roaring64_binary_op_cardinality(VALUE self, VALUE other, binary_func_cardinality func) {
    roaring64_bitmap_t *self_data = get_bitmap(self);
    roaring64_bitmap_t *other_data = get_bitmap(other);

    uint64_t result = func(self_data, other_data);
    return ULL2NUM(result);
}

struct rb_roaring64_add_each_args {
    roaring64_bitmap_t *bitmap;
    roaring64_bulk_context_t context;
};

static VALUE rb_roaring64_add_i(RB_BLOCK_CALL_FUNC_ARGLIST(val, arg))
{
    struct rb_roaring64_add_each_args *args = (void *)arg;
    roaring64_bitmap_add_bulk(args->bitmap, &args->context, NUM2UINT64(val));
    return Qnil;
}

struct rb_roaring64_op {
    roaring64_bitmap_t *(*func)(const roaring64_bitmap_t *, const roaring64_bitmap_t *);
    void (*inplace)(roaring64_bitmap_t *, const roaring64_bitmap_t *);
    void (*range_inplace)(roaring64_bitmap_t *, uint64_t, uint64_t);
};

static const struct rb_roaring64_op rb_roaring64_or_op = {
    roaring64_bitmap_or, roaring64_bitmap_or_inplace, roaring64_bitmap_add_range_closed
};

static const struct rb_roaring64_op rb_roaring64_andnot_op = {
    roaring64_bitmap_andnot, roaring64_bitmap_andnot_inplace, roaring64_bitmap_remove_range_closed
};

static const struct rb_roaring64_op rb_roaring64_xor_op = {
    roaring64_bitmap_xor, roaring64_bitmap_xor_inplace, roaring64_bitmap_flip_closed_inplace
};

static void rb_roaring64_keep_range_closed(roaring64_bitmap_t *r, uint64_t min, uint64_t max)
{
    if (min > 0) roaring64_bitmap_remove_range_closed(r, 0, min - 1);
    if (max < UINT64_MAX) roaring64_bitmap_remove_range_closed(r, max + 1, UINT64_MAX);
}

static const struct rb_roaring64_op rb_roaring64_and_op = {
    roaring64_bitmap_and, roaring64_bitmap_and_inplace, rb_roaring64_keep_range_closed
};

struct rb_roaring64_operand {
    const roaring64_bitmap_t *bitmap;
    uint64_t min, max;
    VALUE tmp;
};

static void rb_roaring64_operand(VALUE other, struct rb_roaring64_operand *out)
{
    out->bitmap = NULL;
    out->tmp = Qnil;

    if (rb_typeddata_is_kind_of(other, &roaring64_type)) {
        out->bitmap = get_bitmap(other);
    } else if (rb_obj_is_kind_of(other, rb_cRange)) {
        if (!roaring_ruby_range_bounds(other, UINT64_MAX, &out->min, &out->max)) {
            out->tmp = rb_roaring64_alloc(cRoaringBitmap64);
            out->bitmap = get_bitmap(out->tmp);
        }
    } else if (rb_respond_to(other, id_each)) {
        out->tmp = rb_roaring64_alloc(cRoaringBitmap64);
        struct rb_roaring64_add_each_args args = { get_bitmap(out->tmp), {0} };
        rb_block_call(other, id_each, 0, NULL, rb_roaring64_add_i, (VALUE)&args);
        out->bitmap = args.bitmap;
    } else {
        rb_raise(rb_eTypeError, "wrong argument type %s (expected Roaring::Bitmap64, Range or Enumerable)", rb_obj_classname(other));
    }
}

static VALUE rb_roaring64_op_inplace(VALUE self, VALUE other, const struct rb_roaring64_op *op)
{
    roaring64_bitmap_t *self_data = get_mutable_bitmap(self);
    struct rb_roaring64_operand o;
    rb_roaring64_operand(other, &o);

    if (o.bitmap) {
        op->inplace(self_data, o.bitmap);
    } else {
        op->range_inplace(self_data, o.min, o.max);
    }
    RB_GC_GUARD(o.tmp);
    return self;
}

static VALUE rb_roaring64_op(VALUE self, VALUE other, const struct rb_roaring64_op *op)
{
    roaring64_bitmap_t *self_data = get_bitmap(self);
    struct rb_roaring64_operand o;
    rb_roaring64_operand(other, &o);

    roaring64_bitmap_t *result;
    if (o.bitmap) {
        result = op->func(self_data, o.bitmap);
    } else {
        result = roaring64_bitmap_copy(self_data);
        op->range_inplace(result, o.min, o.max);
    }
    RB_GC_GUARD(o.tmp);
    return rb_roaring64_wrap(rb_obj_class(self), result);
}

static VALUE rb_roaring64_and_inplace(VALUE self, VALUE other)
{
    return rb_roaring64_op_inplace(self, other, &rb_roaring64_and_op);
}

// Inplace version of {or}. `other` may be a Bitmap64, a Range, or any Enumerable of Integers.
// @return [self] the modified Bitmap
static VALUE rb_roaring64_or_inplace(VALUE self, VALUE other)
{
    return rb_roaring64_op_inplace(self, other, &rb_roaring64_or_op);
}

static VALUE rb_roaring64_xor_inplace(VALUE self, VALUE other)
{
    return rb_roaring64_op_inplace(self, other, &rb_roaring64_xor_op);
}

static VALUE rb_roaring64_andnot_inplace(VALUE self, VALUE other)
{
    return rb_roaring64_op_inplace(self, other, &rb_roaring64_andnot_op);
}

static VALUE rb_roaring64_and(VALUE self, VALUE other)
{
    return rb_roaring64_op(self, other, &rb_roaring64_and_op);
}

static VALUE rb_roaring64_or(VALUE self, VALUE other)
{
    return rb_roaring64_op(self, other, &rb_roaring64_or_op);
}

static VALUE rb_roaring64_xor(VALUE self, VALUE other)
{
    return rb_roaring64_op(self, other, &rb_roaring64_xor_op);
}

static VALUE rb_roaring64_and_cardinality(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_cardinality(self, other, roaring64_bitmap_and_cardinality);
}

static VALUE rb_roaring64_or_cardinality(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_cardinality(self, other, roaring64_bitmap_or_cardinality);
}

static VALUE rb_roaring64_xor_cardinality(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_cardinality(self, other, roaring64_bitmap_xor_cardinality);
}

static VALUE rb_roaring64_andnot_cardinality(VALUE self, VALUE other)
{
    return rb_roaring64_binary_op_cardinality(self, other, roaring64_bitmap_andnot_cardinality);
}

static VALUE rb_roaring64_andnot(VALUE self, VALUE other)
{
    return rb_roaring64_op(self, other, &rb_roaring64_andnot_op);
}

static VALUE rb_roaring64_eq(VALUE self, VALUE other)
{
    if (!rb_obj_is_kind_of(other, cRoaringBitmap64)) {
        return Qfalse;
    }
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
  id_each = rb_intern("each");
  cRoaringBitmap64 = rb_define_class_under(rb_mRoaring, "Bitmap64", rb_cRoaringBitmap);
  rb_define_alloc_func(cRoaringBitmap64, rb_roaring64_alloc);
  rb_define_method(cRoaringBitmap64, "replace", rb_roaring64_replace, 1);
  rb_define_method(cRoaringBitmap64, "empty?", rb_roaring64_empty_p, 0);
  rb_define_method(cRoaringBitmap64, "clear", rb_roaring64_clear, 0);
  rb_define_method(cRoaringBitmap64, "cardinality", rb_roaring64_cardinality, 0);
  rb_define_method(cRoaringBitmap64, "add", rb_roaring64_add, 1);
  rb_define_method(cRoaringBitmap64, "add?", rb_roaring64_add_p, 1);
  rb_define_method(cRoaringBitmap64, "add_range_closed", rb_roaring64_add_range_closed, 2);
  rb_define_method(cRoaringBitmap64, "<<", rb_roaring64_add, 1);
  rb_define_method(cRoaringBitmap64, "remove", rb_roaring64_remove, 1);
  rb_define_method(cRoaringBitmap64, "remove?", rb_roaring64_remove_p, 1);
  rb_define_method(cRoaringBitmap64, "include?", rb_roaring64_include_p, 1);
  rb_define_method(cRoaringBitmap64, "each", rb_roaring64_each, 0);
  rb_define_method(cRoaringBitmap64, "reverse_each", rb_roaring64_reverse_each, 0);
  rb_define_method(cRoaringBitmap64, "each_from", rb_roaring64_each_from, 1);
  rb_define_method(cRoaringBitmap64, "[]", rb_roaring64_aref, 1);

  rb_define_method(cRoaringBitmap64, "and!", rb_roaring64_and_inplace, 1);
  rb_define_method(cRoaringBitmap64, "or!", rb_roaring64_or_inplace, 1);
  rb_define_method(cRoaringBitmap64, "xor!", rb_roaring64_xor_inplace, 1);
  rb_define_method(cRoaringBitmap64, "andnot!", rb_roaring64_andnot_inplace, 1);

  rb_define_method(cRoaringBitmap64, "and", rb_roaring64_and, 1);
  rb_define_method(cRoaringBitmap64, "or", rb_roaring64_or, 1);
  rb_define_method(cRoaringBitmap64, "xor", rb_roaring64_xor, 1);
  rb_define_method(cRoaringBitmap64, "andnot", rb_roaring64_andnot, 1);

  rb_define_method(cRoaringBitmap64, "and_cardinality", rb_roaring64_and_cardinality, 1);
  rb_define_method(cRoaringBitmap64, "or_cardinality", rb_roaring64_or_cardinality, 1);
  rb_define_method(cRoaringBitmap64, "xor_cardinality", rb_roaring64_xor_cardinality, 1);
  rb_define_method(cRoaringBitmap64, "andnot_cardinality", rb_roaring64_andnot_cardinality, 1);

  rb_define_method(cRoaringBitmap64, "==", rb_roaring64_eq, 1);
  rb_define_method(cRoaringBitmap64, "hash", rb_roaring64_hash, 0);
  rb_define_method(cRoaringBitmap64, "to_a", rb_roaring64_to_a, 0);
  rb_define_method(cRoaringBitmap64, "<", rb_roaring64_lt, 1);
  rb_define_method(cRoaringBitmap64, "<=", rb_roaring64_lte, 1);
  rb_define_method(cRoaringBitmap64, "intersect?", rb_roaring64_intersect_p, 1);

  rb_define_method(cRoaringBitmap64, "first", rb_roaring64_first, -1);
  rb_define_method(cRoaringBitmap64, "min", rb_roaring64_min, -1);
  rb_define_method(cRoaringBitmap64, "max", rb_roaring64_max, -1);

  rb_define_method(cRoaringBitmap64, "run_optimize", rb_roaring64_run_optimize, 0);
  rb_define_method(cRoaringBitmap64, "statistics", rb_roaring64_statistics, 0);

  rb_define_method(cRoaringBitmap64, "serialize", rb_roaring64_serialize, 0);
  rb_define_singleton_method(cRoaringBitmap64, "deserialize", rb_roaring64_deserialize, 1);
}
