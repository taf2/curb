# Easy download examples
# 
# First example, downloads 10 files from the same server sequencially using the same
# easy handle with a persistent connection
#
# Second example, sends all 10 requests in parallel using 10 easy handles
#
$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), '..', 'ext')))
$:.unshift(File.expand_path(File.join(File.dirname(__FILE__), '..', 'lib')))
require 'curb'

urls = ['http://www.cnn.com/',
        'http://www.cnn.com/video/',
        'http://newspulse.cnn.com/',
        'http://www.cnn.com/US/',
        'http://www.cnn.com/WORLD/',
        'http://www.cnn.com/POLITICS/',
        'http://www.cnn.com/JUSTICE/',
        'http://www.cnn.com/SHOWBIZ/',
        'http://www.cnn.com/TECH/',
        'http://www.cnn.com/HEALTH/']

def cleanup(urls)
  urls.each do|url|
    filename = url.split(/\?/).first.split(/\//).last
    File.unlink(filename) if File.exist?(filename)
  end
end


# first sequential
def fetch_sequential(urls)
  easy = Curl::Easy.new
  easy.follow_location = true

  urls.each do|url|
    easy.url = url
    filename = url.split(/\?/).first.split(/\//).last
    print "'#{url}' :"
    File.open(filename, 'wb') do|f|
      easy.on_progress {|dl_total, dl_now, ul_total, ul_now| print "="; true }
      easy.on_body {|data| f << data; data.size }   
      easy.perform
      puts "=> '#{filename}'"
    end
  end
end


# using multi interface
def fetch_parallel(urls)
  Curl::Multi.download(urls){|c,code,method| 
    filename = c.url.split(/\?/).first.split(/\//).last
    puts filename
  }
end


fetch_sequential(urls)
cleanup(urls)
fetch_parallel(urls)
