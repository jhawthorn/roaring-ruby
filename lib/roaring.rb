# frozen_string_literal: true

require_relative "roaring/version"
require_relative "roaring/roaring"
require "set"

module Roaring
  class Error < StandardError; end

  class Bitmap
    include Enumerable

    alias size   cardinality
    alias length cardinality
    alias count  cardinality

    alias + |
    alias union |
    alias intersection &
    alias difference -

    alias delete remove
    alias delete? remove?

    alias first min
    alias last max

    alias === include?

    def initialize(enum = nil)
      return unless enum

      if enum.instance_of?(Roaring::Bitmap)
        initialize_copy(enum)
      else
        enum.each { |x| self << x }
      end
    end

    def hash
      to_a.hash
    end

    alias eql? ==

    def replace(other)
      # FIXME: this should probably be initialize_copy and replace should be in C
      initialize_copy(other)
    end

    def self.[](*args)
      if args.size == 0
        new
      elsif args.size == 1 && !(Integer === args[0])
        new(args[0])
      else
        new(args)
      end
    end

    def >(other)
      other < self
    end

    def >=(other)
      other <= self
    end

    alias subset? <=
    alias proper_subset? <
    alias superset? >=
    alias proper_superset? >

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

    def self._load args
      deserialize(args)
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
end
