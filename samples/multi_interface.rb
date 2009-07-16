# Use local curb
#

$:.unshift(File.join(File.dirname(__FILE__), '..', 'ext'))
$:.unshift(File.join(File.dirname(__FILE__), '..', 'lib'))
require 'curb'

# An example for Multi Interface
#
# Some URLs to smack in our demo.
#

urls = ["http://boingboing.net",
        "http://www.yahoo.com",
        "http://slashdot.org",
        "http://www.google.com",
        "http://www.yelp.com",
        "http://www.givereal.com",
        "http://www.google.co.uk/",
        "http://www.ruby-lang.org/"]

responses = {}
m = Curl::Multi.new
# add a few easy handles
urls.each do |url|
  responses[url] = Curl::Easy.new(url)
  m.add(responses[url])
end

m.perform

urls.each do |url|
  puts "[#{url} #{responses[url].total_time}] #{responses[url].body_str[0,30].gsub("\n", '')}..."
  puts
end
