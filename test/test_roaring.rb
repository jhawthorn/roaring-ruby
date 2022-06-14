# frozen_string_literal: true

require "test_helper"

class TestRoaring < Minitest::Test
  def test_that_it_has_a_version_number
    refute_nil ::Roaring::VERSION
  end

  def test_it_does_something_useful
    bitmap = Roaring::Bitmap.new
    bitmap << 1 << 2
    bitmap.add(5)
    bitmap << 2
    p bitmap.cardinality
  end
end
