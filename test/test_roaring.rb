# frozen_string_literal: true

require "test_helper"

class TestRoaring < Minitest::Test
  def test_that_it_has_a_version_number
    refute_nil ::Roaring::VERSION
  end

  def test_simple_example
    bitmap = Roaring::Bitmap.new
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
    2 ** 64,
    2 ** 65,
  ]

  INTS_TO_TEST.each do |i|
    name = "test_#{"negative_" if i < 0}#{i.abs}"

    if i < 0 || i >= 2**32
      define_method("#{name}_is_rejected") do
        bitmap = Roaring::Bitmap.new
        assert_raises RangeError do
          bitmap << i
        end
      end
    else
      define_method("#{name}_round_trips") do
        bitmap = Roaring::Bitmap[i]
        assert_equal i, bitmap.first
      end
    end
  end

  def test_type_error
    bitmap = Roaring::Bitmap.new
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

  def test_empty_p
    bitmap = Roaring::Bitmap.new
    assert bitmap.empty?

    bitmap << 1
    refute bitmap.empty?
  end

  def test_clear
    bitmap = Roaring::Bitmap.new
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
    bitmap = Roaring::Bitmap.new
    assert_raises RangeError do
      bitmap << 2**65
    end
    assert_raises RangeError do
      bitmap << 2**33
    end
  end

  def test_include
    bitmap = Roaring::Bitmap[1, 2, 5, 7]

    result = (0..10).select {|x| bitmap.include?(x) }
    assert_equal [1, 2, 5, 7], result
  end

  def test_each
    bitmap = Roaring::Bitmap[1, 2, 5, 7]
    result = []
    bitmap.each do |x|
      result << x
    end
    assert_equal [1, 2, 5, 7], result
  end

  def test_each_with_break
    bitmap = Roaring::Bitmap[1, 2, 3]
    result = bitmap.each do
      break 123
    end
    assert_equal 123, result
  end

  def test_map
    bitmap = Roaring::Bitmap[1, 2, 5, 7]
    result = bitmap.map(&:itself)
    assert_equal [1, 2, 5, 7], result
  end

  def test_eq
    r1 = Roaring::Bitmap[1, 2, 3, 4]
    r2 = Roaring::Bitmap[1, 2, 3, 4]
    r3 = Roaring::Bitmap[1, 2, 3]

    assert_equal r1, r1
    assert_equal r1, r2
    assert_equal r2, r1
    refute_equal r1, r3
    refute_equal r3, r1
  end

  def test_comparisons
    r1 = Roaring::Bitmap[1, 2, 3, 4]
    r2 = Roaring::Bitmap[1, 2, 3]

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

  def test_and
    r1 = Roaring::Bitmap[1, 2, 3, 4]
    r2 = Roaring::Bitmap[3, 4, 5, 6]
    result = r1 & r2
    assert_equal [3, 4], result.to_a
  end

  def test_or
    r1 = Roaring::Bitmap[1, 2, 3, 4]
    r2 = Roaring::Bitmap[3, 4, 5, 6]
    result = r1 | r2
    assert_equal [1, 2, 3, 4, 5, 6], result.to_a
  end

  def test_xor
    r1 = Roaring::Bitmap[1, 2, 3, 4]
    r2 = Roaring::Bitmap[3, 4, 5, 6]
    result = r1 ^ r2
    assert_equal [1, 2, 5, 6], result.to_a
  end

  def test_difference
    r1 = Roaring::Bitmap[1, 2, 3, 4]
    r2 = Roaring::Bitmap[3, 4, 5, 6]
    result = r1 - r2
    assert_equal [1, 2], result.to_a
  end

  def test_min_and_max
    bitmap = Roaring::Bitmap.new
    bitmap << 5 << 2 << 9 << 7
    assert_equal 2, bitmap.min
    assert_equal 2, bitmap.first
    assert_equal 9, bitmap.max
    assert_equal 9, bitmap.last
  end

  def test_aref
    bitmap = Roaring::Bitmap[1, 2, 99]
    assert_equal 1, bitmap[0]
    assert_equal 2, bitmap[1]
    assert_equal 99, bitmap[2]
    assert_nil bitmap[3]
    assert_nil bitmap[9999]
  end

  def test_serialize
    original = Roaring::Bitmap[1, 2, 3, 4]

    dump = original.serialize
    bitmap = Roaring::Bitmap.deserialize(dump)

    assert_equal original, bitmap
  end

  def test_marshal
    original = Roaring::Bitmap[1, 2, 3, 4]

    dump = Marshal.dump(original)
    bitmap = Marshal.load(dump)

    assert_equal original, bitmap
  end

  def test_dup
    r1 = Roaring::Bitmap[1, 2, 3, 4]

    assert_equal r1.dup, r1
    assert_equal r1.clone, r1

    r2 = r1.dup
    r1 << 5

    refute_equal r1, r2
    assert_equal [5], (r1 - r2).to_a
  end

  def test_memsize
    require "objspace"

    bitmap = Roaring::Bitmap.new
    assert ObjectSpace.memsize_of(bitmap) > 80

    0.upto(1_000_000) do |i|
      bitmap.add(i)
    end

    assert ObjectSpace.memsize_of(bitmap) > 100_000
    assert ObjectSpace.memsize_of(bitmap) < 1_000_000

    bitmap.run_optimize

    assert ObjectSpace.memsize_of(bitmap) < 1000
  end
end
