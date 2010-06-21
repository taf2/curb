require 'rubygems'
require 'rmem'
smem = RMem::Report.memory

$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','ext'))
$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','lib'))

N = (ARGV.shift || 50).to_i
BURL = 'http://127.0.0.1/zeros-2k'

require 'curb'

urls = []

def fetch(urls)
  Curl::Multi.get(urls, {}, {:max_connects => 10, :pipeline => true}) {|c| }
end

t = Time.now
N.times do|n|
  if urls.size >10
    t = Time.now
    fetch(urls)
    urls = []
  else
    urls << BURL
  end
end
fetch(urls)
emem = RMem::Report.memory
puts "\tDuration #{Time.now-t} seconds memory total: #{emem} - growth: #{(emem-smem)/1024.0} kbytes"
