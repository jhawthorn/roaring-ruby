# frozen_string_literal: true

require "test_helper"

module BitmapTests
  include Roaring

  def test_simple_example
    bitmap = bitmap_class.new
    100.upto(999) do |i|
      bitmap << i
    end
    bitmap << 155
    assert_equal 900, bitmap.cardinality
    assert_equal 900, bitmap.size
    assert_equal 900, bitmap.length
    assert_equal 900, bitmap.count
  end

  INTS_TO_TEST = [
    -(2 ** 128),
    -(2 ** 65),
    -(2 ** 64),
    -(2 ** 33),
    -(2 ** 32),
    -(2 ** 31),
    -2,
    -1,
    0,
    1,
    2,
    2 ** 31,
    2 ** 32 - 1,
    2 ** 32,
    2 ** 32 + 1,
    2 ** 33,
    2 ** 64 - 1,
    2 ** 64,
    2 ** 64 + 1,
    2 ** 65,
    2 ** 128,
  ]

  INTS_TO_TEST.each do |i|
    name = "test_#{"negative_" if i < 0}#{i.abs}"

    define_method("#{name}_is_handled_correctly") do
      if bitmap_class::RANGE.cover?(i)
        bitmap = bitmap_class[i]
        assert_equal i, bitmap.first
        assert_equal i, bitmap.min
        assert_equal i, bitmap.max
        assert_equal i, bitmap[0]
      else
        bitmap = bitmap_class.new
        assert_raises RangeError do
          bitmap << i
        end
      end
    end
  end

  def test_type_error
    bitmap = bitmap_class.new
    assert_raises TypeError do
      bitmap << "2"
    end
    assert_raises TypeError do
      bitmap << 2.0
    end
    assert_raises TypeError do
      bitmap << [2]
    end
  end

  def test_add
    bitmap = bitmap_class[1, 2]
    bitmap.add(3)
    assert_equal [1, 2, 3], bitmap.to_a
    bitmap.add(2)
    assert_equal [1, 2, 3], bitmap.to_a
  end

  def test_add_p
    bitmap = bitmap_class[1, 2]
    assert bitmap.add?(3)
    assert_equal [1, 2, 3], bitmap.to_a
    assert_nil bitmap.add?(2)
    assert_equal [1, 2, 3], bitmap.to_a
  end

  def test_add_range
    bitmap = bitmap_class[1, 2]
    bitmap.add_range(0, 1000)
    assert_equal (0...1000).to_a, bitmap.to_a

    bitmap = bitmap_class[1, 2]
    bitmap.add_range_closed(0, 1000)
    assert_equal (0..1000).to_a, bitmap.to_a

    bitmap = bitmap_class.new
    bitmap.add_range(0, 2**32)
    assert_equal 2**32, bitmap.cardinality

    bitmap = bitmap_class.new
    assert_raises RangeError do
      bitmap.add_range_closed(bitmap_class::MAX - 1000, bitmap_class::MAX + 1)
    end
  end

  def test_remove
    bitmap = bitmap_class[1, 2, 3, 4]
    assert_equal [1, 2, 3, 4], bitmap.to_a
    bitmap.remove(3)
    assert_equal [1, 2, 4], bitmap.to_a
    bitmap.remove(10)
    assert_equal [1, 2, 4], bitmap.to_a
  end

  def test_remove_p
    bitmap = bitmap_class[1, 2, 3, 4]
    assert_equal [1, 2, 3, 4], bitmap.to_a
    assert bitmap.remove?(3)
    assert_equal [1, 2, 4], bitmap.to_a
    assert_nil bitmap.remove?(10)
    assert_equal [1, 2, 4], bitmap.to_a
  end

  def test_empty_p
    bitmap = bitmap_class.new
    assert bitmap.empty?

    bitmap << 1
    refute bitmap.empty?
  end

  def test_clear
    bitmap = bitmap_class.new
    assert bitmap.empty?
    assert_equal 0, bitmap.cardinality

    bitmap << 1

    refute bitmap.empty?
    assert_equal 1, bitmap.cardinality

    bitmap.clear

    assert bitmap.empty?
    assert_equal 0, bitmap.cardinality
  end

  def test_it_raises_on_too_large_num
    bitmap = bitmap_class.new
    bitmap << bitmap_class::MAX
    assert_raises RangeError do
      bitmap << (bitmap_class::MAX + 1)
    end
    assert_raises RangeError do
      bitmap << (bitmap_class::MAX * 2)
    end
  end

  def test_include
    bitmap = bitmap_class[1, 2, 5, 7]

    result = (0..10).select {|x| bitmap.include?(x) }
    assert_equal [1, 2, 5, 7], result
  end

  def test_each
    bitmap = bitmap_class[1, 2, 5, 7]
    result = []
    bitmap.each do |x|
      result << x
    end
    assert_equal [1, 2, 5, 7], result
  end

  def test_reverse_each
    bitmap = bitmap_class[1, 2, 5, 7, bitmap_class::MAX]
    result = []
    assert_same bitmap, bitmap.reverse_each { |x| result << x }
    assert_equal [bitmap_class::MAX, 7, 5, 2, 1], result

    assert_equal [], bitmap_class.new.reverse_each.to_a

    enum = bitmap.reverse_each
    assert_kind_of Enumerator, enum
    assert_equal 5, enum.size
    assert_equal bitmap_class::MAX, enum.next

    bitmap = bitmap_class[0...100_000]
    assert_equal (0...100_000).to_a.reverse, bitmap.reverse_each.to_a

    assert_equal 2, bitmap_class[1, 2, 3].reverse_each { |x| break x if x.even? }
    assert_raises(RuntimeError) { bitmap.reverse_each { bitmap.add(1) } }
    assert_raises(ArgumentError) { bitmap.reverse_each { raise ArgumentError } }
    bitmap.add(200_000)
    assert_equal 100_001, bitmap.size
  end

  def test_each_from
    bitmap = bitmap_class[1, 2, 5, 7, bitmap_class::MAX]
    result = []
    assert_same bitmap, bitmap.each_from(3) { |x| result << x }
    assert_equal [5, 7, bitmap_class::MAX], result

    assert_equal [1, 2, 5, 7, bitmap_class::MAX], bitmap.each_from(0).to_a
    assert_equal [2, 5, 7, bitmap_class::MAX], bitmap.each_from(2).to_a
    assert_equal [bitmap_class::MAX], bitmap.each_from(bitmap_class::MAX).to_a
    assert_equal [], bitmap_class.new.each_from(0).to_a
    assert_raises(RangeError) { bitmap.each_from(-1) }
    assert_raises(RangeError) { bitmap.each_from(bitmap_class::MAX + 1) }

    enum = bitmap.each_from(5)
    assert_kind_of Enumerator, enum
    assert_equal 3, enum.size
    assert_equal 5, enum.next
    assert_equal 5, bitmap.each_from(0).size
    assert_equal 1, bitmap.each_from(bitmap_class::MAX).size

    bitmap = bitmap_class[0...100_000]
    assert_equal (70_000...100_000).to_a, bitmap.each_from(70_000).to_a
    assert_equal 30_000, bitmap.each_from(70_000).size
    assert_equal [], bitmap.each_from(100_000).to_a

    assert_equal 6, bitmap.each_from(5) { |x| break x if x.even? }
    assert_raises(RuntimeError) { bitmap.each_from(5) { bitmap.add(1) } }
    assert_raises(ArgumentError) { bitmap.each_from(5) { raise ArgumentError } }
    bitmap.add(200_000)
    assert_equal 100_001, bitmap.size
  end

  def test_to_a
    assert_equal [], bitmap_class.new.to_a
    assert_equal [0, 5, 1000, bitmap_class::MAX], bitmap_class[bitmap_class::MAX, 1000, 5, 0].to_a

    bitmap = bitmap_class[0...100_000]
    assert_equal (0...100_000).to_a, bitmap.to_a
    assert_equal bitmap.each.to_a, bitmap.to_a

    bitmap.freeze
    assert_equal (0...100_000).to_a, bitmap.to_a
    refute_predicate bitmap.to_a, :frozen?
  end

  def test_each_without_block
    bitmap = bitmap_class[1, 2, 5, 7]
    enum = bitmap.each

    assert_kind_of Enumerator, enum
    assert_equal 4, enum.size
    assert_equal 1, enum.next
    assert_equal 2, enum.next
    assert_equal [1, 2, 5, 7], enum.to_a
    assert_equal [[1, 0], [2, 1], [5, 2], [7, 3]], bitmap.each.with_index.to_a
  end

  def test_each_with_break
    bitmap = bitmap_class[1, 2, 3]
    result = bitmap.each do
      break 123
    end
    assert_equal 123, result
  end

  def test_map
    bitmap = bitmap_class[1, 2, 5, 7]
    result = bitmap.map(&:itself)
    assert_equal [1, 2, 5, 7], result
  end

  def test_eq
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[1, 2, 3, 4]
    r3 = bitmap_class[1, 2, 3]

    assert_equal r1, r1
    assert_equal r1, r2
    assert_equal r2, r1
    refute_equal r1, r3
    refute_equal r3, r1

    refute_equal r1, nil
    refute_equal r1, [1, 2, 3, 4]
    refute_equal r1, Set[1, 2, 3, 4]
    refute_equal r1, other_bitmap_class[1, 2, 3, 4]
    refute_includes [1, :sym], r1
  end

  def test_comparisons
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[1, 2, 3]

    assert r1 <= r1
    assert r1 >= r1
    refute r1 < r1
    refute r1 > r1

    assert r2 < r1
    assert r2 <= r1
    refute r2 > r1
    refute r2 >= r1

    refute r1 < r2
    refute r1 <= r2
    assert r1 > r2
    assert r1 >= r2
  end

  def test_intersect
    r1 = bitmap_class[1, 2]
    r2 = bitmap_class[2, 3]
    r3 = bitmap_class[3, 4]

    assert r1.intersect?(r2)
    assert r2.intersect?(r1)
    refute r1.disjoint?(r2)
    refute r2.disjoint?(r1)

    assert r2.intersect?(r3)
    assert r3.intersect?(r2)
    refute r2.disjoint?(r3)
    refute r3.disjoint?(r2)

    refute r1.intersect?(r3)
    refute r3.intersect?(r1)
    assert r1.disjoint?(r3)
    assert r3.disjoint?(r1)
  end

  def test_and
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1 & r2
    refute_same r1, result
    assert_equal [3, 4], result.to_a
  end

  def test_or
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1 | r2
    refute_same r1, result
    assert_equal [1, 2, 3, 4, 5, 6], result.to_a
  end

  def test_or_with_range
    r1 = bitmap_class[1, 2, 3, 4]
    result = r1 | (3...7)
    refute_same r1, result
    assert_equal [1, 2, 3, 4, 5, 6], result.to_a
    assert_equal [1, 2, 3, 4, 5, 6, 7], (r1 | (3..7)).to_a
    assert_equal [1, 2, 3, 4], r1.to_a

    frozen = bitmap_class[1].freeze
    result = frozen | (2..3)
    refute_predicate result, :frozen?
    assert_equal [1, 2, 3], result.to_a

    assert_equal [1], (bitmap_class[1] | (5...5)).to_a
    assert_equal [1, 5], (bitmap_class[1] | (5..5)).to_a
    assert_raises(RangeError) { r1 | (-1..3) }
    assert_raises(TypeError) { r1 | ("a".."b") }
  end

  def test_xor
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1 ^ r2
    refute_same r1, result
    assert_equal [1, 2, 5, 6], result.to_a
  end

  def test_and_cardinality
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    assert_equal 2, r1.and_cardinality(r2)
    assert_equal (r1 & r2).cardinality, r1.and_cardinality(r2)
  end

  def test_or_cardinality
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    assert_equal 6, r1.or_cardinality(r2)
    assert_equal (r1 | r2).cardinality, r1.or_cardinality(r2)
  end

  def test_xor_cardinality
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    assert_equal 4, r1.xor_cardinality(r2)
    assert_equal (r1 ^ r2).cardinality, r1.xor_cardinality(r2)
  end

  def test_andnot_cardinality
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    assert_equal 2, r1.andnot_cardinality(r2)
    assert_equal 2, r2.andnot_cardinality(r1)
    assert_equal (r1 - r2).cardinality, r1.andnot_cardinality(r2)
    assert_equal 0, r1.andnot_cardinality(r1)
    assert_raises(TypeError) { r1.andnot_cardinality(other_bitmap_class[1]) }
  end

  def test_difference
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1 - r2
    refute_same r1, result
    assert_equal [1, 2], result.to_a
  end

  def test_and_inplace
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1.and!(r2)
    assert_same r1, result
    assert_equal [3, 4], result.to_a
  end

  def test_or_inplace
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1.or!(r2)
    assert_same r1, result
    assert_equal [1, 2, 3, 4, 5, 6], result.to_a
  end

  def test_or_inplace_with_range
    max = bitmap_class::MAX

    bitmap = bitmap_class[1, 2]
    assert_same bitmap, bitmap.or!(0...1000)
    assert_equal (0...1000).to_a, bitmap.to_a

    assert_equal (0..1000).to_a, bitmap_class[1, 2].or!(0..1000).to_a
    assert_equal (0..5).to_a, bitmap_class.new.or!(..5).to_a
    assert_equal (0...5).to_a, bitmap_class.new.or!(...5).to_a

    bitmap = bitmap_class.new.or!(max - 10..)
    assert_equal 11, bitmap.size
    assert_equal max, bitmap.max
    assert_equal max - 10, bitmap.min
    assert_equal [max], bitmap_class.new.or!(max..max).to_a
    assert_equal 2**32, bitmap_class.new.or!(0...2**32).size

    assert_equal [7], bitmap_class[7].or!(0...0).to_a
    assert_equal [0, 7], bitmap_class[7].or!(0..0).to_a
    assert_equal [7], bitmap_class[7].or!(5...5).to_a
    assert_equal [5, 7], bitmap_class[7].or!(5..5).to_a
    assert_equal [5, 7], bitmap_class[7].or!(5...6).to_a
    assert_equal [5, 6, 7], bitmap_class[7].or!(5..6).to_a
    assert_equal [7], bitmap_class[7].or!(5..3).to_a
    assert_equal [7], bitmap_class[7].or!(5...3).to_a
    assert_equal [7], bitmap_class[7].or!(max...max).to_a
    assert_equal [7, max], bitmap_class[7].or!(max..max).to_a

    bitmap = bitmap_class[7]

    assert_raises(RangeError) { bitmap.or!(max..max + 1) }
    assert_raises(RangeError) { bitmap.or!(0...max + 2) }
    assert_raises(RangeError) { bitmap.or!(-1..3) }
    assert_raises(RangeError) { bitmap.or!(0..-1) }
    assert_raises(TypeError) { bitmap.or!("a".."b") }
    assert_raises(TypeError) { bitmap.or!(0..1.5) }
    assert_raises(TypeError) { bitmap.or!((1..10) % 3) }
    assert_equal [7], bitmap.to_a

    assert_raises(FrozenError) { bitmap_class[1].freeze.or!(0..3) }
    assert_raises(RuntimeError) { bitmap.each { bitmap.or!(0..3) } }
    assert_equal [7], bitmap.to_a
  end

  def test_xor_inplace
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1.xor!(r2)
    assert_same r1, result
    assert_equal [1, 2, 5, 6], result.to_a
  end

  def test_difference_inplace
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1.andnot!(r2)
    assert_same r1, result
    assert_equal [1, 2], result.to_a
  end

  def test_min_and_max
    bitmap = bitmap_class.new
    bitmap << 5 << 2 << 9 << 7
    assert_equal 2, bitmap.min
    assert_equal 2, bitmap.first
    assert_equal 9, bitmap.max
    assert_equal 9, bitmap.last

    assert_nil Bitmap32[].min
    assert_nil Bitmap32[].max
  end

  def test_count_min_max_with_args_and_blocks
    bitmap = bitmap_class[1, 2, 3, 10]

    assert_equal 4, bitmap.count
    assert_equal 2, bitmap.count { |x| x > 2 }
    assert_equal 1, bitmap.count(3)

    assert_equal 1, bitmap.first
    assert_equal [1, 2], bitmap.first(2)
    assert_equal [1, 2, 3, 10], bitmap.first(10)
    assert_nil bitmap_class[].first
    assert_equal [], bitmap_class[].first(2)
    assert_equal [], bitmap.first(0)
    assert_equal [1, 2, 3, 10], bitmap.first(2**40)
    assert_raises(ArgumentError) { bitmap.first(-1) }
    assert_raises(ArgumentError) { bitmap.first(1, 2) }
    assert_equal [0], bitmap_class[0, bitmap_class::MAX].first(1)
    assert_equal [0, bitmap_class::MAX], bitmap_class[0, bitmap_class::MAX].first(2)
    assert_equal (0...70_000).to_a, bitmap_class[0...100_000].first(70_000)

    assert_equal [1, 2], bitmap.min(2)
    assert_equal [10, 3], bitmap.max(2)
    assert_equal 10, bitmap.min { |a, b| b <=> a }
    assert_equal 1, bitmap.max { |a, b| b <=> a }
  end

  def test_aref
    bitmap = bitmap_class[1, 2, 99]
    assert_equal 1, bitmap[0]
    assert_equal 2, bitmap[1]
    assert_equal 99, bitmap[2]
    assert_nil bitmap[3]
    assert_nil bitmap[9999]
  end

  def test_serialize
    original = bitmap_class[1, 2, 3, 4]

    dump = original.serialize
    bitmap = bitmap_class.deserialize(dump)

    assert_equal original, bitmap
  end

  def test_deserialize_invalid_string
    assert_raises(ArgumentError) { bitmap_class.deserialize("") }
    assert_raises(ArgumentError) { bitmap_class.deserialize("not a roaring payload".b) }
  end

  def test_deserialize_truncated_string
    dump = bitmap_class[1, 2, 3, 4].serialize

    (0...dump.bytesize).each do |length|
      assert_raises(ArgumentError, "expected a #{length} byte prefix to be rejected") do
        bitmap_class.deserialize(dump.byteslice(0, length))
      end
    end
  end

  def test_deserialize_non_string
    assert_raises(TypeError) { bitmap_class.deserialize(123) }
    assert_raises(TypeError) { bitmap_class.deserialize(nil) }
  end

  def test_marshal
    original = bitmap_class[1, 2, 3, 4]

    dump = Marshal.dump(original)
    bitmap = Marshal.load(dump)

    assert_equal original, bitmap
  end

  def test_dup
    r1 = bitmap_class[1, 2, 3, 4]

    assert_equal r1.dup, r1
    assert_equal r1.clone, r1

    r2 = r1.dup
    r1 << 5

    refute_equal r1, r2
    assert_equal [5], (r1 - r2).to_a
  end

  def test_statistics
    bitmap = bitmap_class[]

    expected = {
      n_containers: 0,
      n_array_containers: 0,
      n_run_containers: 0,
      n_bitset_containers: 0,
      n_values_array_containers: 0,
      n_values_run_containers: 0,
      n_values_bitset_containers: 0,
      n_bytes_array_containers: 0,
      n_bytes_run_containers: 0,
      n_bytes_bitset_containers: 0,
      max_value: 0,
      min_value: bitmap_class::MAX,
      cardinality: 0
    }
    assert_equal expected, bitmap.statistics

    bitmap << 1
    assert_equal 1, bitmap.statistics[:n_containers]
    assert_equal 1, bitmap.statistics[:min_value]
    assert_equal 1, bitmap.statistics[:max_value]
    assert_equal 1, bitmap.statistics[:cardinality]
  end

  def test_inspect
    bitmap = bitmap_class[]
    assert_equal "#<#{bitmap_class} {}>", bitmap.inspect

    bitmap = bitmap_class[1, 2, 3, 4]
    assert_equal "#<#{bitmap_class} {1, 2, 3, 4}>", bitmap.inspect

    bitmap = bitmap_class[0...1000]
    assert_equal "#<#{bitmap_class} (1000 values)>", bitmap.inspect
  end

  def test_hash
    bitmap1 = bitmap_class[1, 2, 3, 4]
    bitmap2 = bitmap_class[1, 2, 3, 4]

    assert_equal bitmap1.hash, bitmap2.hash
    refute_equal bitmap1.hash, bitmap_class[1, 2, 3].hash
    refute_equal bitmap1.hash, other_bitmap_class[1, 2, 3, 4].hash
    refute_equal bitmap1.hash, [1, 2, 3, 4].hash

    assert_equal :x, { bitmap1 => :x }[bitmap2]
    assert_nil({ [1, 2, 3, 4] => :x }[bitmap1])
  end

  def test_subclass
    subclass = Class.new(bitmap_class)

    bitmap = subclass.new(bitmap_class[1, 2, 3])
    assert_instance_of subclass, bitmap
    assert_equal [1, 2, 3], bitmap.to_a

    assert_instance_of subclass, bitmap & subclass[2, 3]
    assert_instance_of subclass, bitmap | bitmap_class[4]
    assert_instance_of subclass, bitmap.dup
    assert_instance_of subclass, subclass.deserialize(bitmap.serialize)
    assert_instance_of bitmap_class, bitmap_class[1] & bitmap
  end

  def test_frozen
    bitmap = bitmap_class[1, 2, 3].freeze
    other = bitmap_class[4]

    assert_raises(FrozenError) { bitmap.add(4) }
    assert_raises(FrozenError) { bitmap << 4 }
    assert_raises(FrozenError) { bitmap.add?(4) }
    assert_raises(FrozenError) { bitmap.add_range(4, 6) }
    assert_raises(FrozenError) { bitmap.remove(1) }
    assert_raises(FrozenError) { bitmap.remove?(1) }
    assert_raises(FrozenError) { bitmap.clear }
    assert_raises(FrozenError) { bitmap.replace(other) }
    assert_raises(FrozenError) { bitmap.and!(other) }
    assert_raises(FrozenError) { bitmap.or!(other) }
    assert_raises(FrozenError) { bitmap.xor!(other) }
    assert_raises(FrozenError) { bitmap.andnot!(other) }
    assert_raises(FrozenError) { bitmap.run_optimize }
    assert_equal [1, 2, 3], bitmap.to_a

    assert_equal bitmap_class[1, 2, 3, 4], bitmap | other
    refute_predicate bitmap.dup, :frozen?
    assert_equal [1, 2, 3, 4], (bitmap.dup << 4).to_a
  end

  def test_mutation_during_each
    bitmap = bitmap_class[0...100]

    %i[add add? remove remove?].each do |m|
      e = assert_raises(RuntimeError) { bitmap.each { |x| bitmap.send(m, x) } }
      assert_equal "can't modify bitmap during iteration", e.message
    end
    assert_raises(RuntimeError) { bitmap.each { bitmap.clear } }
    assert_raises(RuntimeError) { bitmap.each { bitmap.replace(bitmap_class[1]) } }
    assert_raises(RuntimeError) { bitmap.each { bitmap.or!(bitmap_class[1]) } }
    assert_raises(RuntimeError) { bitmap.each { bitmap.run_optimize } }
    assert_raises(RuntimeError) { bitmap.each { bitmap.each { bitmap.add(1) } } }
    assert_equal (0...100).to_a, bitmap.to_a

    bitmap.each { break }
    assert_raises(ArgumentError) { bitmap.each { raise ArgumentError } }
    bitmap.each { bitmap.each { } }
    bitmap.add(100)
    assert_equal 101, bitmap.size

    bitmap.each { |x| bitmap.include?(x); bitmap | bitmap_class[x]; bitmap.dup.add(x) }

    bitmap.freeze
    bitmap.each { bitmap.each { } }
  end

  def test_inherits_from_bitmap
    assert_equal Roaring::Bitmap, bitmap_class.superclass
    assert_kind_of Roaring::Bitmap, bitmap_class.new
  end

  # Bitmap used to be a mixin named BitmapCommon, which is kept (empty) so that
  # existing is_a? checks keep working.
  def test_is_kind_of_bitmap_common
    assert_kind_of Roaring::BitmapCommon, bitmap_class.new
  end

end

class Bitmap32Test < Minitest::Test
  include BitmapTests

  def test_memsize
    require "objspace"

    bitmap = bitmap_class.new
    assert ObjectSpace.memsize_of(bitmap) > 80

    0.upto(1_000_000) do |i|
      bitmap.add(i)
    end

    assert ObjectSpace.memsize_of(bitmap) > 100_000
    assert ObjectSpace.memsize_of(bitmap) < 1_000_000

    bitmap.run_optimize

    assert ObjectSpace.memsize_of(bitmap) < 1000
  end

  def bitmap_class
    Roaring::Bitmap32
  end

  def other_bitmap_class
    Roaring::Bitmap64
  end
end

class Bitmap64Test < Minitest::Test
  include BitmapTests

  def bitmap_class
    Roaring::Bitmap64
  end

  def other_bitmap_class
    Roaring::Bitmap32
  end
end
