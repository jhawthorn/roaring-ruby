# frozen_string_literal: true

require_relative "roaring/version"
require "roaring/roaring"
require "set"

module Roaring
  class Error < StandardError; end

  module BitmapCommon
    module ClassMethods
      # @private
      # @!macro [attach] property
      #   @!parse alias_method :<<, :add
      #
      #   @!parse alias_method :size, :cardinality
      #   @!parse alias_method :length, :cardinality
      #   @!parse alias_method :count, :cardinality
      #
      #   @!parse alias_method :&, :and
      #   @!parse alias_method :|, :or
      #   @!parse alias_method :^, :xor
      #   @!parse alias_method :-, :andnot
      #   @!parse alias_method :+, :or
      #   @!parse alias_method :union, :or
      #   @!parse alias_method :intersection, :and
      #   @!parse alias_method :difference, :andnot
      #
      #   @!parse alias_method :delete, :remove
      #   @!parse alias_method :delete?, :remove?
      #
      #   @!parse alias_method :first, :min
      #   @!parse alias_method :last, :max
      #
      #   @!parse alias_method :eql?, :==
      #
      #   @!parse alias_method :===, :include?
      #
      #   @!parse alias_method :subset?, :<=
      #   @!parse alias_method :proper_subset?, :<
      def define_roaring_aliases!
        alias_method :<<, :add

        alias_method :size, :cardinality
        alias_method :length, :cardinality
        alias_method :count, :cardinality

        alias_method :&, :and
        alias_method :|, :or
        alias_method :^, :xor
        alias_method :-, :andnot
        alias_method :+, :or
        alias_method :union, :or
        alias_method :intersection, :and
        alias_method :difference, :andnot

        alias_method :delete, :remove
        alias_method :delete?, :remove?

        alias_method :first, :min
        alias_method :last, :max

        alias_method :eql?, :==

        alias_method :===, :include?

        alias_method :subset?, :<=
        alias_method :proper_subset?, :<
      end

      # Convenience method for building a bitmap
      def [](*args)
        if args.size == 0
          new
        elsif args.size == 1 && !(Integer === args[0])
          new(args[0])
        else
          new(args)
        end
      end

      def _load args
        deserialize(args)
      end
    end

    include Enumerable

    def initialize(enum = nil)
      return unless enum

      if enum.instance_of?(self.class)
        replace(enum)
      elsif Range === enum
        if enum.exclude_end?
          add_range(enum.begin, enum.end)
        else
          add_range_closed(enum.begin, enum.end)
        end
      else
        enum.each { |x| self << x }
      end
    end

    def hash
      to_a.hash
    end

    def initialize_copy(other)
      replace(other)
    end

    # Check if `self` is a superset of `other`. A superset requires that `self` contain all of `other`'s elemtents. They may be equal.
    # @return [Boolean] `true` if `self` is a strict subset of `other`, otherwise `false`
    def superset?(other)
      other <= self
    end

    # Check if `self` is a strict superset of `other`. A strict superset requires that `self` contain all of `other`'s elemtents, but that they aren't exactly equal.
    # @return [Boolean] `true` if `self` is a strict subset of `other`, otherwise `false`
    def proper_superset?(other)
      other < self
    end

    alias_method :>=, :superset?
    alias_method :>, :proper_superset?

    def add_range(min, max)
      return if max <= min
      add_range_closed(min, max - 1)
    end

    # @return [Integer] Returns 0 if the bitmaps are equal, -1 / +1 if the set is a subset / superset of the given set, or nil if they both have unique elements.
    def <=>(other)
      if self == other
        0
      elsif subset?(other)
        -1
      elsif superset?(other)
        1
      else
        nil
      end
    end

    def disjoint?(other)
      !intersect?(other)
    end

    def _dump level
      serialize
    end

    def to_set
      ::Set.new(self)
    end

    # @example Small bitmap
    #   Roaring::Bitmap32[1,2,3].inspect #=> "#<Roaring::Bitmap32 {1, 2, 3}>"
    # @example Large bitmap
    #   Roaring::Bitmap32[1..1000].inspect #=> "#<Roaring::Bitmap32 (1000 values)>"
    # @return [String] a programmer-readable representation of the bitmap
    def inspect
      cardinality = self.cardinality
      if cardinality < 64
        "#<#{self.class} {#{to_a.join(", ")}}>"
      else
        "#<#{self.class} (#{cardinality} values)>"
      end
    end
  end

  class Bitmap32
    include BitmapCommon
    extend BitmapCommon::ClassMethods

    define_roaring_aliases!

    MIN = 0
    MAX = (2**32) - 1
    RANGE = MIN..MAX
  end

  class Bitmap64
    include BitmapCommon
    extend BitmapCommon::ClassMethods

    define_roaring_aliases!

    MIN = 0
    MAX = (2**64) - 1
    RANGE = MIN..MAX
  end
end
