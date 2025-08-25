source 'https://rubygems.org'

gemspec

group 'development', 'test' do
  gem 'webrick' # for ruby 3.1
  gem 'rdoc'
  gem 'rake'
  gem 'mixlib-shellout' # for docker test builds?
  gem 'test-unit'
  gem 'ruby_memcheck'
  if Gem::Version.new(RUBY_VERSION) >= Gem::Version.new('3.1')
    gem 'async', '>= 2.20'
  end
  gem 'simplecov', require: false
  gem 'simplecov-lcov', require: false # For CI integration
end

platforms :rbx do
  gem 'minitest'
end
