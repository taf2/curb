
require 'rubygems'
require 'net/http/persistent'

N = (ARGV.shift || 50).to_i
BURL = 'http://127.0.0.1/zeros-2k'

http = Net::HTTP::Persistent.new
uri = URI.parse BURL

t = Time.now
N.times do |n|
  http.request uri
end
puts "Duration #{Time.now-t} seconds"
