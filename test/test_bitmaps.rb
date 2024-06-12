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
        bitmap = bitmap_class.new
        assert_raises RangeError do
          bitmap << i
        end
      else
        bitmap = bitmap_class[i]
        assert_equal i, bitmap.first
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
    assert_raises RangeError do
      bitmap << 2**65
    end
    assert_raises RangeError do
      bitmap << 2**33
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
    assert_equal [3, 4], result.to_a
  end

  def test_or
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1 | r2
    assert_equal [1, 2, 3, 4, 5, 6], result.to_a
  end

  def test_xor
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1 ^ r2
    assert_equal [1, 2, 5, 6], result.to_a
  end

  def test_difference
    r1 = bitmap_class[1, 2, 3, 4]
    r2 = bitmap_class[3, 4, 5, 6]
    result = r1 - r2
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

  def test_inspect
    bitmap = bitmap_class[]
    assert_equal "#<#{bitmap_class} {}>", bitmap.inspect

    bitmap = bitmap_class[1, 2, 3, 4]
    assert_equal "#<#{bitmap_class} {1, 2, 3, 4}>", bitmap.inspect

    bitmap = bitmap_class[0...1000]
    assert_equal "#<#{bitmap_class} (1000 values)>", bitmap.inspect
  end
end

class Bitmap32Test < Minitest::Test
  include BitmapTests

  def bitmap_class
    Roaring::Bitmap32
  end
end

class Bitmap32Test < Minitest::Test
  include BitmapTests

  def bitmap_class
    Roaring::Bitmap64
  end
end
