# frozen_string_literal: true

require "test_helper"

# Smoke test against Set's behaviour
class TestSetBehavior < Minitest::Test
  def self.build_example_sets(values)
    values.size.times.flat_map do |i|
      values.combination(i).to_a
    end
  end

  EXAMPLE_VALUES = (0..4).to_a
  EXAMPLE_SETS = build_example_sets(EXAMPLE_VALUES)

  binary_methods = %i[
    < <= > >= == != <=> eql?
    subset? proper_subset? superset? proper_superset?
    disjoint? intersect?
    + - & | ^
    union intersection difference

    replace
  ]

  unary_methods = %i[
    empty? length size
    clear
    to_a
    min max
  ]

  arg_methods = %i[
    << add
    add? delete?
    delete
    include? ===
  ]

  arg_methods.each do |method_name|
    define_method(:"test_#{method_name}") do
      assert_respond_to method_name

      EXAMPLE_SETS.each do |ex_set|
        EXAMPLE_VALUES.each do |value|
          with_both(ex_set) do |x|
            x.public_send(method_name, value)
          end
        end
      end
    end
  end

  binary_methods.each do |method_name|
    define_method(:"test_#{method_name}") do
      assert_respond_to method_name

      EXAMPLE_SETS.repeated_combination(2).each do |ex_a, ex_b|
        with_both(ex_a, ex_b) do |a, b|
          a.public_send(method_name, b)
        end
      end
    end
  end

  unary_methods.each do |method_name|
    define_method(:"test_#{method_name}") do
      assert_respond_to method_name

      EXAMPLE_SETS.each do |example|
        with_both(example) do |x|
          x.public_send(method_name)
        end
      end
    end
  end

  def assert_respond_to(method_name)
    [Set, Roaring::Bitmap32].each do |klass|
      assert klass.new.respond_to?(method_name),
        "#{klass} doesn't respond_to #{method_name}"
    end
  end

  def with_both(*args)
    sets = args.map { |a| Set.new(a) }
    bitmap32 = args.map { |a| Roaring::Bitmap32.new(a) }
    bitmap64 = args.map { |a| Roaring::Bitmap64.new(a) }

    set_result = yield(*sets)
    bitmap32_result = yield(*bitmap32)
    bitmap64_result = yield(*bitmap64)

    # Check that they are equal in case of mutation
    sets.zip(bitmap32) do |set, bitmap|
      assert_equivalent set, bitmap
    end
    sets.zip(bitmap64) do |set, bitmap|
      assert_equivalent set, bitmap
    end

    # Check return value
    case set_result
    when Set
      assert_equivalent set_result, bitmap32_result
      assert_equivalent set_result, bitmap64_result
    when nil
      assert_nil bitmap32_result
      assert_nil bitmap64_result
    else
      assert_equal set_result, bitmap32_result
      assert_equal set_result, bitmap64_result
    end
  end

  def assert_equivalent(set, bitmap)
    assert_kind_of Set, set
    assert_kind_of Roaring::BitmapCommon, bitmap

    assert_equal set.to_a.sort, bitmap.to_a
  end
end
