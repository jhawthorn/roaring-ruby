# Roaring

A Ruby wrapper for CRoaring (a C/C++ implementation at https://github.com/RoaringBitmap/CRoaring)

Roaring Bitmaps are a compressed bitmap/bitset format/library.
More information and papers can be found at https://roaringbitmap.org/

## Installation

Install the gem and add to the application's Gemfile by executing:

    $ bundle add roaring

## Usage

``` ruby
require "roaring"

# Bitmap32 can efficiently store a large range of 32-bit integers
bitmap = Roaring::Bitmap32[1, 2, 3, 999]
bitmap << (2**32 - 1)

# Bitmap64 can efficiently store a large range of 64-bit integers
bitmap = Roaring::Bitmap64[1, 2, 3, 999]
bitmap << (2**32 + 1)
bitmap << (2**64 - 1)

# Element access
bitmap.each { }
bitmap.first # => 1
bitmap.min   # => 1
bitmap.max   # => 4294967295
bitmap.last  # => 4294967295
bitmap[3]    # => 999

b1 = Roaring::Bitmap64.new(200...500)
b2 = Roaring::Bitmap64.new(100...1000)

# Support common set operations
(b1 & b2).size # => 300
(b1 ^ b2).size # => 600
(b2 - b1).size # => 600
(b1 - b2).empty? # => true
b1 < b2 # => true
(b2 - b1) == (b1 ^ b2) # => true

# (De)Serialization (also available via Marshal#{dump,load})
dump = bitmap.serialize
loaded = Roaring::Bitmap64.deserialize(dump)
loaded == bitmap # => true
```

## Development

After checking out the repo, run `bin/setup` to install dependencies. Then, run `rake test` to run the tests. You can also run `bin/console` for an interactive prompt that will allow you to experiment.

To install this gem onto your local machine, run `bundle exec rake install`. To release a new version, update the version number in `version.rb`, and then run `bundle exec rake release`, which will create a git tag for the version, push git commits and the created tag, and push the `.gem` file to [rubygems.org](https://rubygems.org).

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/jhawthorn/roaring. This project is intended to be a safe, welcoming space for collaboration, and contributors are expected to adhere to the [code of conduct](https://github.com/jhawthorn/roaring/blob/main/CODE_OF_CONDUCT.md).

## License

The gem is available as open source under the terms of the [Apache License, Version 2.0](https://opensource.org/licenses/Apache-2.0).

## Code of Conduct

Everyone interacting in the Roaring project's codebases, issue trackers, chat rooms and mailing lists is expected to follow the [code of conduct](https://github.com/jhawthorn/roaring/blob/main/CODE_OF_CONDUCT.md).
