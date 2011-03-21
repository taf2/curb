require 'rubygems'
$:.unshift File.expand_path(File.dirname(__FILE__))
require '_usage'

$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','ext'))
$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','lib'))

N = (ARGV.shift || 50).to_i
BURL = 'http://127.0.0.1/zeros-2k'
URLS = []
N.times do
  URLS << BURL
end

Memory.usage("Curl::Multi(#{N})") do
  require 'curb'

  Curl::Multi.get(URLS)

end
