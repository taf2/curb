require 'rubygems'
$:.unshift File.expand_path(File.dirname(__FILE__))
require '_usage'

$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','ext'))
$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','lib'))

N = (ARGV.shift || 50).to_i
BURL = 'http://127.0.0.1/zeros-2k'

Memory.usage("Curl::Easy(#{N})") do

  require 'curb'
  c = Curl::Easy.new

  N.times do|n|
    c.url = BURL
    c.on_body {|d| d.size} # don't buffer
    c.perform
  end

end
