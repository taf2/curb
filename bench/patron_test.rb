require 'rubygems'
require 'rmem'
smem = RMem::Report.memory
require 'patron'

N = (ARGV.shift || 50).to_i

sess = Patron::Session.new
sess.base_url = 'http://127.0.0.1'
t = Time.now
N.times do |n|
  sess.get('/zeros-2k')
end
emem = RMem::Report.memory
puts "\tDuration #{Time.now-t} seconds memory total: #{emem} - growth: #{(emem-smem)/1024.0} kbytes"
