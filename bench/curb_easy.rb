require 'rubygems'
require 'rmem'
smem = RMem::Report.memory
$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','ext'))
$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','lib'))

N = (ARGV.shift || 50).to_i
BURL = 'http://127.0.0.1/zeros-2k'

require 'curb'
c = Curl::Easy.new

t = Time.now
N.times do|n|
  c.url = BURL
  c.on_body{|d| d.size} # don't buffer
  c.perform
end
emem = RMem::Report.memory
puts "\tDuration #{Time.now-t} seconds memory total: #{emem} - growth: #{(emem-smem)/1024.0} kbytes"
