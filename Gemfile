source 'https://rubygems.org'

gemspec

group 'development', 'test' do
  gem 'rdoc'
  gem 'rake'
end

# Need to specify ruby standard library gems for rbx
# see http://www.benjaminfleischer.com/2013/12/05/testing-rubinius-on-travis-ci/
platforms :rbx do
  gem 'rubysl', '~> 2.0'
end
