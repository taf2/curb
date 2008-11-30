begin
  require 'curb_core'
rescue LoadError
  $: << File.dirname(__FILE__)
  require 'curb_core'
end

module Curl
  class Easy
    class << self
    
      # call-seq:
      #   Curl::Easy.download(url, filename = url.split(/\?/).first.split(/\//).last) { |curl| ... }
      #   
      # Stream the specified url (via perform) and save the data directly to the
      # supplied filename (defaults to the last component of the URL path, which will 
      # usually be the filename most simple urls).
      # 
      # If a block is supplied, it will be passed the curl instance prior to the
      # perform call.
      # 
      # *Note* that the semantics of the on_body handler are subtly changed when using
      # download, to account for the automatic routing of data to the specified file: The 
      # data string is passed to the handler *before* it is written
      # to the file, allowing the handler to perform mutative operations where 
      # necessary. As usual, the transfer will be aborted if the on_body handler
      # returns a size that differs from the data chunk size - in this case, the 
      # offending chunk will *not* be written to the file, the file will be closed,
      # and a Curl::Err::AbortedByCallbackError will be raised.
      def download(url, filename = url.split(/\?/).first.split(/\//).last, &blk)
        curl = Curl::Easy.new(url, &blk)
        
        File.open(filename, "wb") do |output|
          old_on_body = curl.on_body do |data| 
            result = old_on_body ?  old_on_body.call(data) : data.length
            output << data if result == data.length
            result
          end
          
          curl.perform
        end        

        return curl
      end
    end
  end
end

