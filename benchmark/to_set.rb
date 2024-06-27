require "benchmark/ips"
require "roaring"

small_bitmap = Roaring::Bitmap32[1,2,3,4]
large_bitmap = Roaring::Bitmap32[1..1000]

Benchmark.ips do |x|
  x.report "small_bitmap.to_set" do
    small_bitmap.to_set
  end

  x.report "large_bitmap.to_set" do
    large_bitmap.to_set
  end
end
