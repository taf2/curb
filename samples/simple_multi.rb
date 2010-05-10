$:.unshift(File.join(File.dirname(__FILE__), '..', 'ext'))
$:.unshift(File.join(File.dirname(__FILE__), '..', 'lib'))
require 'curb'


Curl::Multi.get(
       ["http://boingboing.net",
        "http://www.yahoo.com",
        "http://slashdot.org",
        "http://www.google.com",
        "http://www.yelp.com",
        "http://digg.com",
        "http://www.google.co.uk/",
        "http://www.ruby-lang.org/"], {:follow_location => true,:max_redirects => 3,:timeout => 4}) do|easy|
  puts "[#{easy.last_effective_url} #{easy.total_time}] #{easy.body_str[0,30].gsub("\n", '')}..."
end
