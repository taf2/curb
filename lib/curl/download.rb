# frozen_string_literal: true

require 'uri'
require 'tempfile'

module Curl
  class DownloadTargetExistsError < Errno::EEXIST; end
  DOWNLOAD_OPTION_KEYS = [:download_dir, :overwrite].freeze

  class << self
    def download_options_hash?(value)
      value.is_a?(Hash) && value.keys.all? { |key| DOWNLOAD_OPTION_KEYS.include?(key) }
    end

    def normalize_download_arguments(filename, options)
      if download_options_hash?(filename) && options.empty?
        options = filename
        filename = nil
      end

      [filename, options || {}]
    end

    def parse_download_options(options)
      raise ArgumentError, "download options must be a Hash" unless options.is_a?(Hash)

      options = options.dup
      download_dir = options.delete(:download_dir)
      overwrite = options.key?(:overwrite) ? !!options.delete(:overwrite) : false
      raise ArgumentError, "unsupported download option(s): #{options.keys.join(', ')}" unless options.empty?

      [download_dir, overwrite]
    end

    def resolve_download_output(url, filename = nil, options = {})
      filename, options = normalize_download_arguments(filename, options)
      download_dir, overwrite = parse_download_options(options)

      if filename.is_a?(IO)
        filename.binmode if filename.respond_to?(:binmode)
        return [nil, filename, false, overwrite]
      end

      path = if download_dir
        safe_download_path(url, download_dir, filename: filename)
      elsif filename.nil?
        safe_download_path(url)
      else
        filename.to_s
      end

      [path, nil, true, overwrite]
    end

    def prepare_download_output(url, filename = nil, options = {})
      path, io, safe_output, overwrite = resolve_download_output(url, filename, options)
      return [path, io, safe_output] unless safe_output

      [path, open_safe_download_output(path, overwrite: overwrite), true]
    end

    def download_filename_from_url(url)
      path = begin
        URI.parse(url.to_s).path
      rescue URI::InvalidURIError
        url.to_s.split(/\?/, 2).first
      end

      basename = File.basename(path.to_s)
      validate_download_filename!(basename)
    end

    def safe_download_path(url, destination_dir = Dir.pwd, filename: nil)
      dir = File.expand_path(destination_dir.to_s)
      raise ArgumentError, "download destination directory does not exist: #{destination_dir}" unless File.directory?(dir)

      File.join(dir, filename.nil? ? download_filename_from_url(url) : validate_download_filename!(filename))
    end

    def validate_download_filename!(filename)
      filename = filename.to_s
      invalid = filename.empty? ||
                filename == '.' ||
                filename == '..' ||
                filename == File::SEPARATOR ||
                filename.start_with?('.') ||
                filename.include?("\0") ||
                filename.include?(File::SEPARATOR) ||
                filename.include?('\\') ||
                filename.match?(/\A[A-Za-z]:/) ||
                (File::ALT_SEPARATOR && filename.include?(File::ALT_SEPARATOR))

      raise ArgumentError, "unsafe download filename derived from URL: #{filename.inspect}" if invalid

      filename
    end

    def open_safe_download_output(path, overwrite: false)
      SafeDownloadOutput.new(path, overwrite: overwrite)
    end
  end

  class SafeDownloadOutput
    def initialize(path, overwrite: false)
      @path = File.expand_path(path.to_s)
      @overwrite = overwrite
      raise DownloadTargetExistsError, @path if !@overwrite && File.exist?(@path)

      @tmp = Tempfile.new(['.curb-download-', '.tmp'], File.dirname(@path))
      @tmp.binmode
      @closed = false
    end

    attr_reader :path

    def write(data)
      @tmp.write(data)
    end

    def <<(data)
      write(data)
    end

    def close(success = false)
      return if @closed

      @tmp.flush unless @tmp.closed?
      @tmp.close unless @tmp.closed?
      install_tmp_file if success
    ensure
      @tmp.close! if @tmp
      @closed = true
    end

    private

    def install_tmp_file
      if @overwrite
        File.rename(@tmp.path, @path)
      else
        File.link(@tmp.path, @path)
      end
    rescue Errno::EEXIST
      raise DownloadTargetExistsError, @path
    rescue NotImplementedError
      install_tmp_file_by_copy
    end

    def install_tmp_file_by_copy
      flags = File::WRONLY | File::CREAT | File::EXCL
      flags |= File::BINARY if File.const_defined?(:BINARY)

      File.open(@path, flags) do |dest|
        File.open(@tmp.path, 'rb') { |src| IO.copy_stream(src, dest) }
      end
    rescue Errno::EEXIST
      raise DownloadTargetExistsError, @path
    end
  end
end
