# frozen_string_literal: true

require "test_helper"

class TestRoaring < Minitest::Test
  include Roaring

  def test_that_it_has_a_version_number
    refute_nil ::Roaring::VERSION
  end

  # Bitmap is abstract: the widths are distinct serialization formats, so a
  # concrete one always has to be chosen.
  def test_bitmap_cannot_be_instantiated
    assert_raises(TypeError) { Roaring::Bitmap.new }
    assert_raises(TypeError) { Roaring::Bitmap.allocate }
  end
end
