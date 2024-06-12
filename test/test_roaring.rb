# frozen_string_literal: true

require "test_helper"

class TestRoaring < Minitest::Test
  include Roaring

  def test_that_it_has_a_version_number
    refute_nil ::Roaring::VERSION
  end
end
