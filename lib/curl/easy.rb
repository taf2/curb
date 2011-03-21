module Curl
  class Easy

    #
    # call-seq:
    #   easy.perform                                     => true
    #
    # Transfer the currently configured URL using the options set for this
    # Curl::Easy instance. If this is an HTTP URL, it will be transferred via
    # the configured HTTP Verb.
    #
    def perform
      self.multi = Curl::Multi.new if self.multi.nil?
      self.multi.add self
      ret = self.multi.perform

      if self.last_result != 0 && self.on_failure.nil?
        error = Curl::Easy.error(self.last_result) 
        raise error.first
      end

      ret
    end

    class << self

      #
      # call-seq:
      #   Curl::Easy.perform(url) { |easy| ... }           => #&lt;Curl::Easy...&gt;
      #
      # Convenience method that creates a new Curl::Easy instance with
      # the specified URL and calls the general +perform+ method, before returning
      # the new instance. For HTTP URLs, this is equivalent to calling +http_get+.
      #
      # If a block is supplied, the new instance will be yielded just prior to
      # the +http_get+ call.
      #
      def perform(*args)
        c = Curl::Easy.new *args
        yield c if block_given?
        c.perform
        c
      end

      #
      # call-seq:
      #   Curl::Easy.http_get(url) { |easy| ... }          => #&lt;Curl::Easy...&gt;
      #
      # Convenience method that creates a new Curl::Easy instance with
      # the specified URL and calls +http_get+, before returning the new instance.
      #
      # If a block is supplied, the new instance will be yielded just prior to
      # the +http_get+ call.
      #
      def http_get(*args)
        c = Curl::Easy.new *args
        yield c if block_given?
        c.http_get
        c
      end

      #
      # call-seq:
      #   Curl::Easy.http_head(url) { |easy| ... }         => #&lt;Curl::Easy...&gt;
      #
      # Convenience method that creates a new Curl::Easy instance with
      # the specified URL and calls +http_head+, before returning the new instance.
      #
      # If a block is supplied, the new instance will be yielded just prior to
      # the +http_head+ call.
      #
      def http_head(*args)
        c = Curl::Easy.new *args
        yield c if block_given?
        c.http_head
        c
      end

      #
      # call-seq:
      #   Curl::Easy.http_put(url, data) {|c| ... }
      #
      # see easy.http_put
      #
      def http_put(url, data)
        c = Curl::Easy.new url
        yield c if block_given?
        c.http_put data
        c
      end

      #
      # call-seq:
      #   Curl::Easy.http_post(url, "some=urlencoded%20form%20data&and=so%20on") => true
      #   Curl::Easy.http_post(url, "some=urlencoded%20form%20data", "and=so%20on", ...) => true
      #   Curl::Easy.http_post(url, "some=urlencoded%20form%20data", Curl::PostField, "and=so%20on", ...) => true
      #   Curl::Easy.http_post(url, Curl::PostField, Curl::PostField ..., Curl::PostField) => true
      #
      # POST the specified formdata to the currently configured URL using
      # the current options set for this Curl::Easy instance. This method
      # always returns true, or raises an exception (defined under
      # Curl::Err) on error.
      #
      # If you wish to use multipart form encoding, you'll need to supply a block
      # in order to set multipart_form_post true. See #http_post for more
      # information.
      #
      def http_post(*args)
        url = args.shift
        c = Curl::Easy.new url
        yield c if block_given?
        c.http_post *args
        c
      end

      #
      # call-seq:
      #   Curl::Easy.http_delete(url) { |easy| ... }       => #&lt;Curl::Easy...&gt;
      #
      # Convenience method that creates a new Curl::Easy instance with
      # the specified URL and calls +http_delete+, before returning the new instance.
      #
      # If a block is supplied, the new instance will be yielded just prior to
      # the +http_delete+ call.
      #
      def http_delete(*args)
        c = Curl::Easy.new *args
        yield c if block_given?
        c.http_delete
        c
      end
    
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
        
        output = if filename.is_a? IO
          filename.binmode if filename.respond_to?(:binmode)
          filename
        else
          File.open(filename, 'wb')
        end
        
        begin
          old_on_body = curl.on_body do |data|
            result = old_on_body ?  old_on_body.call(data) : data.length
            output << data if result == data.length
            result
          end
          curl.perform
        ensure
          output.close rescue IOError
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
end
