require 'rubygems'
$:.unshift File.expand_path(File.dirname(__FILE__))
require '_usage'

N = (ARGV.shift || 50).to_i
BURL = 'http://127.0.0.1/zeros-2k'

Memory.usage("Net::HTTP(#{N})") do
  require 'net/http/persistent'

  http = Net::HTTP::Persistent.new
  uri = URI.parse BURL

  N.times do |n|
    http.request uri
  end

end
