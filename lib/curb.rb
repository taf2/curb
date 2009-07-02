require 'curb_core'

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

    # Allow the incoming cert string to be file:password
    # but be careful to not use a colon from a windows file path
    # as the split point. Mimic what curl's main does
    alias_method :native_cert=, :cert=
    def cert=(cert_file)
      pos = cert_file.rindex(':')
      if pos && pos > 1
        self.native_cert= cert_file[0..pos-1]
        self.certpassword= cert_file[pos+1..-1]
      else
        self.native_cert= cert_file
      end
      self.cert
    end
  end
  class Multi
    class << self
      # call-seq:
      #   Curl::Multi.get('url1','url2','url3','url4','url5', :follow_location => true) do|easy|
      #     easy
      #   end
      # 
      # Blocking call to fetch multiple url's in parallel.
      def get(urls, easy_options={}, multi_options={}, &blk)
        m = Curl::Multi.new
        # configure the multi handle
        multi_options.each do|k,v|
          m.send("#{k}=", v)
        end

        # create and configure each easy handle
        urls.each do|url|
          c = Curl::Easy.new(url)
          easy_options.each do|k,v|
            c.send("#{k}=",v)
          end
          c.on_complete {|curl| blk.call curl }
          m.add(c)
        end
        m.perform
      end
    end
  end
end
