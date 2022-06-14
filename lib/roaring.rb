# frozen_string_literal: true

require_relative "roaring/version"
require_relative "roaring/roaring"

module Roaring
  class Error < StandardError; end

  class Bitmap
    include Enumerable

    alias size   cardinality
    alias length cardinality
    alias count  cardinality

    def to_a
      map(&:itself)
    end

    def inspect
      "#<#{self.class} cardinality=#{cardinality}>"
    end
  end
end
