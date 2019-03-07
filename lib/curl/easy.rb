# frozen_string_literal: true
module Curl
  class Easy

    alias post http_post
    alias put http_put
    alias body body_str
    alias head header_str

    class Error < StandardError
      attr_accessor :message, :code
      def initialize(code, msg)
        self.message = msg
        self.code = code
      end
    end

    #
    # call-seq:
    #   easy.status  => String
    #
    def status
      # Matches the last HTTP Status - following the HTTP protocol specification 'Status-Line = HTTP-Version SP Status-Code SP (Opt:)Reason-Phrase CRLF'
      statuses = self.header_str.scan(/HTTP\/\d(\.\d)?\s(\d+\s.*)\r\n/).map{ |match| match[1] }
      statuses.last.strip if statuses.length > 0
    end

    #
    # call-seq:
    #   easy.set :sym|Fixnum, value
    #
    # set options on the curl easy handle see http://curl.haxx.se/libcurl/c/curl_easy_setopt.html
    #
    def set(opt,val)
      if opt.is_a?(Symbol)
        option = sym2curl(opt)
      else
        option = opt.to_i
      end

      begin
        setopt(option, val)
      rescue TypeError
        raise TypeError, "Curb doesn't support setting #{opt} [##{option}] option"
      end
    end

    #
    # call-seq:
    #   easy.sym2curl :symbol => Fixnum
    #
    #  translates ruby symbols to libcurl options
    #
    def sym2curl(opt)
      Curl.const_get("CURLOPT_#{opt.to_s.upcase}")
    end

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
      self.multi.remove self

      if Curl::Multi.autoclose
        self.multi.close
        self.multi = nil
      end

      if self.last_result != 0 && self.on_failure.nil?
        error = Curl::Easy.error(self.last_result)
        raise error.first.new(self.head)
      end

      ret
    end

    #
    # call-seq:
    #
    # easy = Curl::Easy.new
    # easy.nosignal = true
    #
    def nosignal=(onoff)
      set :nosignal, !!onoff
    end

    #
    # call-seq:
    #   easy = Curl::Easy.new("url") do|c|
    #    c.delete = true
    #   end
    #   easy.perform
    #
    def delete=(onoff)
      set :customrequest, onoff ? 'DELETE' : nil
      onoff
    end
    #
    # call-seq:
    #
    #  easy = Curl::Easy.new("url")
    #  easy.version = Curl::HTTP_2_0
    #  easy.version = Curl::HTTP_1_1
    #  easy.version = Curl::HTTP_1_0
    #  easy.version = Curl::HTTP_NONE
    #
    def version=(http_version)
      set :http_version, http_version
    end

    #
    # call-seq:
    #   easy.url = "http://some.url/"                    => "http://some.url/"
    #
    # Set the URL for subsequent calls to +perform+. It is acceptable
    # (and even recommended) to reuse Curl::Easy instances by reassigning
    # the URL between calls to +perform+.
    #
    def url=(u)
      set :url, u
    end

    #
    # call-seq:
    #   easy.proxy_url = string                          => string
    #
    # Set the URL of the HTTP proxy to use for subsequent calls to +perform+.
    # The URL should specify the the host name or dotted IP address. To specify
    # port number in this string, append :[port] to the end of the host name.
    # The proxy string may be prefixed with [protocol]:// since any such prefix
    # will be ignored. The proxy's port number may optionally be specified with
    # the separate option proxy_port .
    #
    # When you tell the library to use an HTTP proxy, libcurl will transparently
    # convert operations to HTTP even if you specify an FTP URL etc. This may have
    # an impact on what other features of the library you can use, such as
    # FTP specifics that don't work unless you tunnel through the HTTP proxy. Such
    # tunneling is activated with proxy_tunnel = true.
    #
    # libcurl respects the environment variables *http_proxy*, *ftp_proxy*,
    # *all_proxy* etc, if any of those is set. The proxy_url option does however
    # override any possibly set environment variables.
    #
    # Starting with libcurl 7.14.1, the proxy host string given in environment
    # variables can be specified the exact same way as the proxy can be set with
    # proxy_url, including protocol prefix (http://) and embedded user + password.
    #
    def proxy_url=(url)
      set :proxy, url
    end

    def ssl_verify_host=(value)
      value = 1 if value.class == TrueClass
      value = 0 if value.class == FalseClass
      self.ssl_verify_host_integer=value
    end

    #
    # call-seq:
    #   easy.ssl_verify_host?                            => boolean
    #
    # Deprecated: call easy.ssl_verify_host instead
    # can be one of [0,1,2]
    #
    # Determine whether this Curl instance will verify that the server cert
    # is for the server it is known as.
    #
    def ssl_verify_host?
      ssl_verify_host.nil? ? false : (ssl_verify_host > 0)
    end

    #
    # call-seq:
    #   easy.interface = string                          => string
    #
    # Set the interface name to use as the outgoing network interface.
    # The name can be an interface name, an IP address or a host name.
    #
    def interface=(value)
      set :interface, value
    end

    #
    # call-seq:
    #   easy.userpwd = string                            => string
    #
    # Set the username/password string to use for subsequent calls to +perform+.
    # The supplied string should have the form "username:password"
    #
    def userpwd=(value)
      set :userpwd, value
    end

    #
    # call-seq:
    #   easy.proxypwd = string                           => string
    #
    # Set the username/password string to use for proxy connection during
    # subsequent calls to +perform+. The supplied string should have the
    # form "username:password"
    #
    def proxypwd=(value)
      set :proxyuserpwd, value
    end

    #
    # call-seq:
    #   easy.cookies = "name1=content1; name2=content2;" => string
    #
    # Set cookies to be sent by this Curl::Easy instance. The format of the string should
    # be NAME=CONTENTS, where NAME is the cookie name and CONTENTS is what the cookie should contain.
    # Set multiple cookies in one string like this: "name1=content1; name2=content2;" etc.
    #
    def cookies=(value)
      set :cookie, value
    end

    #
    # call-seq:
    #   easy.cookiefile = string                         => string
    #
    # Set a file that contains cookies to be sent in subsequent requests by this Curl::Easy instance.
    #
    # *Note* that you must set enable_cookies true to enable the cookie
    # engine, or this option will be ignored.
    #
    def cookiefile=(value)
      set :cookiefile, value
    end

    #
    # call-seq:
    #   easy.cookiejar = string                          => string
    #
    # Set a cookiejar file to use for this Curl::Easy instance.
    # Cookies from the response will be written into this file.
    #
    # *Note* that you must set enable_cookies true to enable the cookie
    # engine, or this option will be ignored.
    #
    def cookiejar=(value)
      set :cookiejar, value
    end

    #
    # call-seq:
    #  easy = Curl::Easy.new("url") do|c|
    #   c.head = true
    #  end
    #  easy.perform
    #
    def head=(onoff)
      set :nobody, onoff
    end

    #
    # call-seq:
    #   easy.follow_location = boolean                   => boolean
    #
    # Configure whether this Curl instance will follow Location: headers
    # in HTTP responses. Redirects will only be followed to the extent
    # specified by +max_redirects+.
    #
    def follow_location=(onoff)
      set :followlocation, onoff
    end

    #
    # call-seq:
    #   easy.http_head                                   => true
    #
    # Request headers from the currently configured URL using the HEAD
    # method and current options set for this Curl::Easy instance. This
    # method always returns true, or raises an exception (defined under
    # Curl::Err) on error.
    #
    def http_head
      set :nobody, true
      ret = self.perform
      set :nobody, false
      ret
    end

    #
    # call-seq:
    #   easy.http_get                                    => true
    #
    # GET the currently configured URL using the current options set for
    # this Curl::Easy instance. This method always returns true, or raises
    # an exception (defined under Curl::Err) on error.
    #
    def http_get
      set :httpget, true
      http :GET
    end
    alias get http_get

    #
    # call-seq:
    #   easy.http_delete
    #
    # DELETE the currently configured URL using the current options set for
    # this Curl::Easy instance. This method always returns true, or raises
    # an exception (defined under Curl::Err) on error.
    #
    def http_delete
      self.http :DELETE
    end
    alias delete http_delete

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
        c = Curl::Easy.new(*args)
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
        c = Curl::Easy.new(*args)
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
        c = Curl::Easy.new(*args)
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
        c.http_post(*args)
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
        c = Curl::Easy.new(*args)
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
    if respond_to?(:cert=)
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
end
