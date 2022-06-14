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
  end
end
