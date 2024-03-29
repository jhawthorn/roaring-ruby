# frozen_string_literal: true

require_relative "lib/roaring/version"

Gem::Specification.new do |spec|
  spec.name = "roaring"
  spec.version = Roaring::VERSION
  spec.authors = ["John Hawthorn"]
  spec.email = ["john@hawthorn.email"]

  spec.summary = "Roaring bitmaps for Ruby"
  spec.description = spec.summary
  spec.homepage = "https://github.com/jhawthorn/roaring-ruby"
  spec.license = "Apache-2.0"
  spec.required_ruby_version = ">= 2.6.0"

  spec.metadata["source_code_uri"] = spec.metadata["changelog_uri"] = spec.metadata["homepage_uri"] = spec.homepage

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files = Dir.chdir(__dir__) do
    `git ls-files --recurse-submodules -z`.split("\x0").reject do |f|
      (f == __FILE__) || f.match(%r{\A(?:(?:bin|test|spec|features)/|\.(?:git|travis|circleci)|appveyor)})
    end
  end
  spec.bindir = "exe"
  spec.executables = spec.files.grep(%r{\Aexe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
  spec.extensions = ["ext/roaring/extconf.rb"]

  spec.add_development_dependency "prime"
end
