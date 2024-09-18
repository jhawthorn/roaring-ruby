# frozen_string_literal: true

require "bundler/gem_tasks"
require "rake/testtask"

require "yard"

YARD::Rake::YardocTask.new do |t|
  t.files = ["lib/**/*.rb", "ext/**/*.c"]
end

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

desc "Update vendored CRoaring library"
task :update_roaring do
  rm_rf "tmp/CRoaring"
  sh "git clone --depth=1 https://github.com/RoaringBitmap/CRoaring tmp/CRoaring"
  sh "cd tmp/CRoaring && sh amalgamation.sh"
  cp "tmp/CRoaring/roaring.c", "ext/roaring/roaring.c"
  cp "tmp/CRoaring/roaring.h", "ext/roaring/roaring.h"
end

task default: %i[compile test]
