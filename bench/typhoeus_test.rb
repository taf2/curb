require 'rubygems'
$:.unshift File.expand_path(File.dirname(__FILE__))
require '_usage'

N = (ARGV.shift || 50).to_i
Memory.usage("Typhoeus(#{N})") do

  require 'typhoeus'

  N.times do
    Typhoeus::Request.get('http://127.0.0.1/zeros-2k')
  end

end
