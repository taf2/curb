source 'https://rubygems.org'

gemspec

gem 'mixlib-shellout'
gem 'pry'

group 'development', 'test' do
  gem 'rdoc'
  gem 'rake', '>= 13.0.1'
  gem 'test-unit'
end

# Need to specify ruby standard library gems for rbx
# see http://www.benjaminfleischer.com/2013/12/05/testing-rubinius-on-travis-ci/
platforms :rbx do
  install_if -> { RUBY_VERSION !~ /10.0/ } do
    gem 'rubysl', '~> 2.0'
    gem 'rubysl-test-unit' # see https://github.com/rubysl/rubysl-test-unit/issues/1
  end
  gem 'minitest'
end
