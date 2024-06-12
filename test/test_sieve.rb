# frozen_string_literal: true

require "test_helper"
require "prime"

# A simple prime number sieve as a light stress test
# not fast just easy to implement
class TestSieve < Minitest::Test
  class Sieve
    def initialize
      @bitmap = self.class::Bitmap.new
      @bitmap << 2
      @limit = @bitmap.max
    end

    def include?(num)
      if @limit <= num
        calculate(num)
      end
      @bitmap.include?(num)
    end

    def each(limit)
      0.upto(limit).select do |i|
        include?(i)
      end
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

  class Sieve32 < Sieve
    Bitmap = Roaring::Bitmap32
  end

  class Sieve64 < Sieve
    Bitmap = Roaring::Bitmap64
  end

  LIMIT = 100

  def test_sieve32_against_ruby_prime
    expected = Prime.each(LIMIT).to_a
    actual = Sieve32.new.each(LIMIT).to_a
    assert_equal expected, actual
  end

  def test_sieve64_against_ruby_prime
    expected = Prime.each(LIMIT).to_a
    actual = Sieve64.new.each(LIMIT).to_a
    assert_equal expected, actual
  end
end
