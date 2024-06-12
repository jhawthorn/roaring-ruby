# frozen_string_literal: true

require "bundler/gem_tasks"
require "rake/testtask"

Rake::TestTask.new(:test) do |t|
  t.libs << "test"
  t.libs << "lib"
  t.test_files = FileList["test/**/test_*.rb"]
end

require "rake/extensiontask"

task build: :compile

Rake::ExtensionTask.new("roaring") do |ext|
  ext.lib_dir = "lib/roaring"
end

task :update_roaring do
  require "open-uri"

  ["roaring.c", "roaring.h"].each do |filename|
    url = "https://github.com/RoaringBitmap/CRoaring/releases/latest/download/#{filename}"
    source = URI.open(url).read
    File.write("#{__dir__}/ext/roaring/#{filename}", source)
  end
end

task default: %i[clobber compile test]
