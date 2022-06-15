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

    alias first min
    alias last max

    def initialize(enum = nil)
      return unless enum

      if enum.instance_of?(Roaring::Bitmap)
        initialize_copy(enum)
      else
        enum.each { |x| self << x }
      end
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
      "#<#{self.class} cardinality=#{cardinality}>"
    end
  end
end
