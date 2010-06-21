require 'rubygems'
require 'rmem'
smem = RMem::Report.memory
require 'typhoeus'

N = (ARGV.shift || 50).to_i
t = Time.now

hydra = Typhoeus::Hydra.new
reqs = []

N.times do
  req = Typhoeus::Request.new('http://127.0.0.1/zeros-2k')
  reqs << req
  hydra.queue req
end
hydra.run

emem = RMem::Report.memory
puts "\tDuration #{Time.now-t} seconds memory total: #{emem} - growth: #{(emem-smem)/1024.0} kbytes"
