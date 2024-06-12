# frozen_string_literal: true

require_relative "roaring/version"
require_relative "roaring/roaring"
require "set"

module Roaring
  class Error < StandardError; end

  module BitmapCommon
    def self.included(base)
      super

      base.extend ClassMethods

      base.alias_method :size, :cardinality
      base.alias_method :length, :cardinality
      base.alias_method :count, :cardinality

      base.alias_method :+, :|
      base.alias_method :union, :|
      base.alias_method :intersection, :&
      base.alias_method :difference, :-

      base.alias_method :delete, :remove
      base.alias_method :delete?, :remove?

      base.alias_method :first, :min
      base.alias_method :last, :max

      base.alias_method :eql?, :==

      base.alias_method :===, :include?

      base.alias_method :subset?, :<=
      base.alias_method :proper_subset?, :<
      base.alias_method :superset?, :>=
      base.alias_method :proper_superset?, :>
    end

    module ClassMethods
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

    def >(other)
      other < self
    end

    def >=(other)
      other <= self
    end

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

    def to_a
      map(&:itself)
    end

    def to_set
      ::Set.new(to_a)
    end

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
  end
end
