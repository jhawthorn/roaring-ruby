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

  def test_each
    bitmap = Roaring::Bitmap.new
    bitmap << 1 << 2 << 5 << 7
    result = []
    bitmap.each do |x|
      result << x
    end
    assert_equal [1, 2, 5, 7], result
  end

  def test_each_with_break
    bitmap = Roaring::Bitmap.new
    bitmap << 1 << 2 << 3
    result = bitmap.each do
      break 123
    end
    assert_equal 123, result
  end

  def test_map
    bitmap = Roaring::Bitmap.new
    bitmap << 1 << 2 << 5 << 7
    result = bitmap.map(&:itself)
    assert_equal [1, 2, 5, 7], result
  end

  def test_and
    r1 = Roaring::Bitmap.new
    r2 = Roaring::Bitmap.new
    r1 << 1 << 2 << 3 << 4
    r2 << 3 << 4 << 5 << 6
    result = r1 & r2
    assert_equal [3, 4], result.to_a
  end

  def test_or
    r1 = Roaring::Bitmap.new
    r2 = Roaring::Bitmap.new
    r1 << 1 << 2 << 3 << 4
    r2 << 3 << 4 << 5 << 6
    result = r1 | r2
    assert_equal [1, 2, 3, 4, 5, 6], result.to_a
  end

  def test_xor
    r1 = Roaring::Bitmap.new
    r2 = Roaring::Bitmap.new
    r1 << 1 << 2 << 3 << 4
    r2 << 3 << 4 << 5 << 6
    result = r1 ^ r2
    assert_equal [1, 2, 5, 6], result.to_a
  end

  def test_difference
    r1 = Roaring::Bitmap.new
    r2 = Roaring::Bitmap.new
    r1 << 1 << 2 << 3 << 4
    r2 << 3 << 4 << 5 << 6
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
