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

task default: %i[clobber compile test]
