# This logs into gmail, up to the point where it hits the
# security redirect implemented as a refresh. It will probably
# stop working altogether when they next change gmail but 
# it's still an example of posting with curb...

$:.unshift(File.join(File.dirname(__FILE__), '..', 'ext'))
$:.unshift(File.join(File.dirname(__FILE__), '..', 'lib'))
require 'curb'

$EMAIL = '<YOUR GMAIL LOGIN>'
$PASSWD = '<YOUR GMAIL PASSWORD>'

url = 'https://www.google.com/accounts/ServiceLoginAuth'

fields = [
  Curl::PostField.content('ltmpl','m_blanco'),
  Curl::PostField.content('ltmplcache', '2'),
  Curl::PostField.content('continue', 
                          'http://mail.google.com/mail/?ui.html&amp;zy=l'),
  Curl::PostField.content('service', 'mail'),
  Curl::PostField.content('rm', 'false'),
  Curl::PostField.content('rmShown', '1'),
  Curl::PostField.content('PersistentCookie', ''),
  Curl::PostField.content('Email', $EMAIL),
  Curl::PostField.content('Passwd', $PASSWD)
]

c = Curl::Easy.http_post(url, *fields) do |curl|
  # Gotta put yourself out there...
  curl.headers["User-Agent"] = "Curl/Ruby"

  # Let's see what happens under the hood
  curl.verbose = true

  # Google will redirect us a bit
  curl.follow_location = true

  # Google will make sure we retain cookies
  curl.enable_cookies = true
end

puts "FINISHED: HTTP #{c.response_code}"
puts c.body_str

# As an alternative to passing the PostFields, we could have supplied 
# individual pre-encoded option strings, or a single string with the
# entire form data.

