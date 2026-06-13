# frozen_string_literal: true
require 'curl/download'
module Curl
  class Multi
    class DownloadError < RuntimeError
      attr_accessor :errors
    end

    IDLE_EASY_REFERENCES_USE_WEAK_MAP = begin
      probe = ObjectSpace::WeakMap.new
      probe[Object.new.freeze] = true
      true
    rescue ArgumentError, FrozenError, NameError
      false
    end

    class << self
      # call-seq:
      #   Curl::Multi.get(['url1','url2','url3','url4','url5'], :follow_location => true) do|easy|
      #     easy
      #   end
      #
      # Blocking call to fetch multiple url's in parallel.
      def get(urls, easy_options={}, multi_options={}, &blk)
        url_confs = []
        urls.each do|url|
          url_confs << {:url => url, :method => :get}.merge(easy_options)
        end
        self.http(url_confs, multi_options) {|c,code,method| blk.call(c) if blk }
      end

      # call-seq:
      #
      #   Curl::Multi.post([{:url => 'url1', :post_fields => {'field1' => 'value1', 'field2' => 'value2'}},
      #                     {:url => 'url2', :post_fields => {'field1' => 'value1', 'field2' => 'value2'}},
      #                     {:url => 'url3', :post_fields => {'field1' => 'value1', 'field2' => 'value2'}}],
      #                    { :follow_location => true, :multipart_form_post => true },
      #                    {:pipeline => Curl::CURLPIPE_HTTP1}) do|easy|
      #     easy_handle_on_request_complete
      #   end
      #
      # Blocking call to POST multiple form's in parallel.
      #
      # urls_with_config: is a hash of url's pointing to the postfields to send
      # easy_options: are a set of common options to set on all easy handles
      # multi_options: options to set on the Curl::Multi handle
      #
      def post(urls_with_config, easy_options={}, multi_options={}, &blk)
        url_confs = []
        urls_with_config.each do|uconf|
          url_confs << uconf.merge(:method => :post).merge(easy_options)
        end
        self.http(url_confs, multi_options) {|c,code,method| blk.call(c) }
      end

      # call-seq:
      #
      #   Curl::Multi.put([{:url => 'url1', :put_data => "some message"},
      #                    {:url => 'url2', :put_data => IO.read('filepath')},
      #                    {:url => 'url3', :put_data => "maybe another string or socket?"],
      #                    {:follow_location => true},
      #                    {:pipeline => Curl::CURLPIPE_HTTP1}) do|easy|
      #     easy_handle_on_request_complete
      #   end
      #
      # Blocking call to POST multiple form's in parallel.
      #
      # urls_with_config: is a hash of url's pointing to the postfields to send
      # easy_options: are a set of common options to set on all easy handles
      # multi_options: options to set on the Curl::Multi handle
      #
      def put(urls_with_config, easy_options={}, multi_options={}, &blk)
        url_confs = []
        urls_with_config.each do|uconf|
          url_confs << uconf.merge(:method => :put).merge(easy_options)
        end
        self.http(url_confs, multi_options) {|c,code,method| blk.call(c) }
      end


      # call-seq:
      #
      # Curl::Multi.http( [
      #   { :url => 'url1', :method => :post,
      #     :post_fields => {'field1' => 'value1', 'field2' => 'value2'} },
      #   { :url => 'url2', :method => :get,
      #     :follow_location => true, :max_redirects => 3 },
      #   { :url => 'url3', :method => :put, :put_data => File.open('file.txt','rb') },
      #   { :url => 'url4', :method => :head }
      # ], {:pipeline => Curl::CURLPIPE_HTTP1})
      #
      # Blocking call to issue multiple HTTP requests with varying verb's.
      #
      # urls_with_config: is a hash of url's pointing to the easy handle options as well as the special option :method, that can by one of [:get, :post, :put, :delete, :head], when no verb is provided e.g. :method => nil -> GET is used
      # multi_options: options for the multi handle
      # blk: a callback, that yeilds when a handle is completed
      #
      def http(urls_with_config, multi_options={}, &blk)
        m = Curl::Multi.new

        # maintain a sane number of easy handles
        multi_options[:max_connects] = max_connects = multi_options.key?(:max_connects) ? multi_options[:max_connects] : 10

        free_handles = [] # keep a list of free easy handles

        # configure the multi handle
        multi_options.each { |k,v| m.send("#{k}=", v) }
        callbacks = [:on_progress,:on_debug,:on_failure,:on_success,:on_redirect,:on_missing,:on_body,:on_header]

        add_free_handle = proc do|conf, easy|
          c       = conf.dup # avoid being destructive to input
          url     = c.delete(:url)
          method  = c.delete(:method)
          headers = c.delete(:headers)
          internal_info = c.delete(:__curb_internal_info)

          easy    = Curl::Easy.new if easy.nil?

          easy.url = url

          # assign callbacks
          callbacks.each do |cb|
            cbproc = c.delete(cb)
            easy.send(cb,&cbproc) if cbproc
          end

          case method
          when :post
            fields = c.delete(:post_fields)
            # set the post post using the url fields
            easy.post_body = fields.map{|f,k| "#{easy.escape(f)}=#{easy.escape(k)}"}.join('&')
          when :put
            easy.put_data = c.delete(:put_data)
          when :head
            easy.head = true
          when :delete
            easy.delete = true
          when :get
          else
            # XXX: nil is treated like a GET
          end

          # headers is a special key
          headers.each {|k,v| easy.headers[k] = v } if headers

          #
          # use the remaining options as specific configuration to the easy handle
          # bad options should raise an undefined method error
          #
          c.each { |k,v| easy.send("#{k}=",v) }

          easy.on_complete {|curl|
            free_handles << curl
            if blk
              if internal_info
                blk.call(curl,curl.response_code,method,internal_info)
              else
                blk.call(curl,curl.response_code,method)
              end
            end
          }
          m.add(easy)
        end

        max_connects.times do
          conf = urls_with_config.pop
          add_free_handle.call(conf, nil) if conf
          break if urls_with_config.empty?
        end

        consume_free_handles = proc do
          # as we idle consume free handles
          if urls_with_config.size > 0 && free_handles.size > 0
            easy = free_handles.pop
            conf = urls_with_config.pop
            add_free_handle.call(conf, easy) if conf
          end
        end

        begin
          if urls_with_config.empty?
            m.perform
          else
            until urls_with_config.empty?
              m.perform do
                consume_free_handles.call
              end
              consume_free_handles.call
            end
            free_handles = nil
          end
        ensure
          m.close
        end

      end

      # call-seq:
      #
      # Curl::Multi.download(['http://example.com/p/a/t/h/file1.txt','http://example.com/p/a/t/h/file2.txt']){|c|}
      #
      # will create 2 new files file1.txt and file2.txt, unless either file
      # already exists. Auto-derived filenames are safely derived from the last
      # URL path component. Pass <tt>:download_dir</tt> as the fifth argument to
      # treat download_paths as basenames inside a trusted directory and reject
      # absolute, parent-directory, dotfile, and nested names.
      #
      # 2 files will be opened, and remain open until the call completes
      #
      # when using the :post or :put method, urls should be a hash, including the individual post fields per post
      #
      def download(urls,easy_options={},multi_options={},download_paths=nil,download_options={},&blk)
        errors = []
        procs = []
        files = []
        urls_with_config = []
        seen_download_paths = {}
        download_infos = []

        if Curl.download_options_hash?(download_paths) && download_options.empty?
          download_options = download_paths
          download_paths = nil
        end

        urls.each_with_index do|urlcfg,i|
          if urlcfg.is_a?(Hash)
            url = urlcfg[:url]
          else
            url = urlcfg
          end

          download_path_arg = download_paths && download_paths[i]
          download_path, file, safe_output, overwrite = Curl.resolve_download_output(url, download_path_arg, download_options)

          if safe_output
            expanded_path = File.expand_path(download_path)
            raise ArgumentError, "duplicate download destination: #{download_path}" if seen_download_paths[expanded_path]

            seen_download_paths[expanded_path] = true
          end

          download_infos << {
            :url => url,
            :urlcfg => urlcfg,
            :path => download_path,
            :file => file,
            :safe_output => safe_output,
            :overwrite => overwrite
          }
        end

        download_infos.each do |info|
          info[:file] ||= Curl.open_safe_download_output(info[:path], :overwrite => info[:overwrite])
          file = info[:file]
          procs << (lambda {|data| file.write data; data.size })
          files << file

          if info[:urlcfg].is_a?(Hash)
            urls_with_config << info[:urlcfg].merge({:on_body => procs.last, :__curb_internal_info => info}.merge(easy_options))
          else
            urls_with_config << {:url => info[:url], :on_body => procs.last, :method => :get, :__curb_internal_info => info}.merge(easy_options)
          end
        end

        finalize_download = lambda do |curl, info|
          file = info[:file]
          files.reject!{|f| f == file }

          if curl.last_result != 0
            begin
              if info[:safe_output]
                file.close(false)
              else
                file.close
              end
            rescue => e
              errors << e
            end
            err_class, err_summary = Curl::Easy.error(curl.last_result)
            err_detail = curl.last_error
            errors << err_class.new([err_summary, err_detail].compact.join(": "))
            false
          else
            begin
              if info[:safe_output]
                file.close(true)
              else
                file.close
              end
              true
            rescue => e
              errors << e
              false
            end
          end
        end

        Curl::Multi.http(urls_with_config, multi_options) do |c,code,method,info|
          if finalize_download.call(c, info) && blk
            blk.call(c,info[:path])
          end
        end

      ensure
        pending_exception = $!
        files.each {|f|
          begin
            if f.is_a?(Curl::SafeDownloadOutput)
              f.close(false)
            else
              f.close
            end
          rescue => e
            errors << e
          end
        }
        if errors.any? && !pending_exception
          de = Curl::Multi::DownloadError.new
          de.errors = errors
          raise de
        end
      end
    end

    def cancel!
      requests.each do |_,easy|
        remove(easy)
      end
    end

    def idle?
      requests.empty?
    end

    def requests
      @requests ||= {}
    end

    def __curb_native_safety_signatures
      @__curb_native_safety_signatures ||= {}
    end

    def __curb_safety_signature_for(easy)
      safety_signature = if Curl.respond_to?(:safety_signature_for, true)
        Curl.__send__(:safety_signature_for, easy)
      end

      [
        safety_signature,
        easy.respond_to?(:network_policy) ? easy.network_policy : nil,
        easy.respond_to?(:allowed_hosts) ? easy.allowed_hosts : nil,
        easy.respond_to?(:allowed_cidrs) ? easy.allowed_cidrs : nil
      ]
    end

    def __record_native_safety_signature(easy)
      __curb_native_safety_signatures[easy.object_id] = __curb_safety_signature_for(easy)
    end

    def __idle_easy_references
      @__curb_idle_easy_references ||= __new_idle_easy_references
    end
    
    def __register_idle_easy_reference(easy)
      if IDLE_EASY_REFERENCES_USE_WEAK_MAP
        __idle_easy_references[easy] = true
      else
        __idle_easy_references[easy.object_id] = true
      end
      self
    end
    
    def __unregister_idle_easy_reference(easy)
      return self unless instance_variable_defined?(:@__curb_idle_easy_references)

      if !IDLE_EASY_REFERENCES_USE_WEAK_MAP
        @__curb_idle_easy_references.delete(easy.object_id)
      elsif @__curb_idle_easy_references.respond_to?(:delete)
        @__curb_idle_easy_references.delete(easy)
      else
        retained_references = ObjectSpace::WeakMap.new
        @__curb_idle_easy_references.each_key do |tracked_easy|
          next if tracked_easy.equal?(easy)

          retained_references[tracked_easy] = true
        end
        @__curb_idle_easy_references = retained_references
      end

      self
    end
    
    def __clear_idle_easy_references
      return unless instance_variable_defined?(:@__curb_idle_easy_references)

      if IDLE_EASY_REFERENCES_USE_WEAK_MAP
        @__curb_idle_easy_references.keys.each do |easy|
          easy.multi = nil if easy.multi.equal?(self)
        end
      else
        @__curb_idle_easy_references.keys.each do |easy_object_id|
          begin
            easy = ObjectSpace._id2ref(easy_object_id)
          rescue RangeError
            next
          end

          next unless easy.is_a?(Curl::Easy)

          easy.multi = nil if easy.multi.equal?(self)
        end
      end
      @__curb_idle_easy_references = __new_idle_easy_references
    end

    def __new_idle_easy_references
      IDLE_EASY_REFERENCES_USE_WEAK_MAP ? ObjectSpace::WeakMap.new : {}
    end

    private :__idle_easy_references, :__curb_native_safety_signatures,
            :__curb_safety_signature_for, :__record_native_safety_signature,
            :__register_idle_easy_reference,
            :__unregister_idle_easy_reference, :__clear_idle_easy_references,
            :__new_idle_easy_references

    alias_method :_curb_native_perform, :perform

    def perform(*args, &block)
      requests.each_value do |easy|
        Curl.__send__(:apply_safety!, easy) if Curl.respond_to?(:apply_safety!, true)
        signature = __curb_safety_signature_for(easy)
        if __curb_native_safety_signatures[easy.object_id] != signature &&
           easy.respond_to?(:__curb_native_setup!, true)
          easy.__send__(:__curb_native_setup!)
          __curb_native_safety_signatures[easy.object_id] = signature
        end
      end

      _curb_native_perform(*args, &block)
    end

    def add(easy)
      return self if requests[easy.object_id]
      # Once a deferred callback exception is pending, Multi#perform is
      # draining existing transfers only and must not start replacement work.
      return self if instance_variable_defined?(:@__curb_deferred_exception)
      Curl.__send__(:apply_safety!, easy) if Curl.respond_to?(:apply_safety!, true)
      _add(easy)
      __unregister_idle_easy_reference(easy)
      requests[easy.object_id] = easy
      __record_native_safety_signature(easy)
      self
    end

    def remove(easy)
      return self if !requests[easy.object_id]
      requests.delete(easy.object_id)
      __curb_native_safety_signatures.delete(easy.object_id)
      _remove(easy)
      self
    end

    def close
      __close(true)
    end

    def _autoclose
      __close(false)
    end

    private :_autoclose

    private

    def __close(permanent)
      requests.values.each {|easy|
        _remove(easy)
      }
      __clear_idle_easy_references if permanent
      @requests = {}
      _close
      _mark_closed if permanent
      self
    end

  end
end
