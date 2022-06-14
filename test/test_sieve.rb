# frozen_string_literal: true

require "test_helper"
require "prime"

# A simple prime number sieve as a light stress test
# not fast just easy to implement
class TestSieve < Minitest::Test
  class Sieve
    def initialize
      @bitmap = Roaring::Bitmap.new
      @bitmap << 2
      @limit = @bitmap.max
    end

    def include?(num)
      if @limit <= num
        calculate(num)
      end
      @bitmap.include?(num)
    end

    private

    def calculate(num)
      (@limit + 1).upto(num) do |i|
        prime = @bitmap.none? do |divisor|
          num % divisor == 0
        end
        @bitmap << i if prime
      end
    end
  end

  def test_sieve_against_ruby_prime
    s = Sieve.new
    limit = 100

    expected = Prime.each(limit).to_a

    actual = 0.upto(limit).select do |i|
      s.include?(i)
    end

    assert_equal expected, actual
  end
end
