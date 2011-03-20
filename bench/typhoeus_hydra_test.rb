require 'rubygems'
$:.unshift File.expand_path(File.dirname(__FILE__))
require '_usage'

N = (ARGV.shift || 50).to_i

Memory.usage("Typhoeus::Hydra(#{N})") do

  require 'typhoeus'

  hydra = Typhoeus::Hydra.new
  reqs = []

  N.times do
    req = Typhoeus::Request.new('http://127.0.0.1/zeros-2k')
    reqs << req
    hydra.queue req
  end
  hydra.run

end
