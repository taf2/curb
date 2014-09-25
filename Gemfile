source 'https://rubygems.org'

gemspec

group 'development', 'test' do
  gem 'rdoc'
  gem 'rake'
  gem 'test-unit'
end

# Need to specify ruby standard library gems for rbx
# see http://www.benjaminfleischer.com/2013/12/05/testing-rubinius-on-travis-ci/
platforms :rbx do
  gem 'rubysl', '~> 2.0'
  gem 'minitest'
  gem 'rubysl-test-unit' # see https://github.com/rubysl/rubysl-test-unit/issues/1
end
