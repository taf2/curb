#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'
require 'objspace'
require 'optparse'
require 'rbconfig'
require 'tmpdir'

TOPDIR = File.expand_path('..', __dir__)
LIBDIR = File.join(TOPDIR, 'lib')
EXTDIR = File.join(TOPDIR, 'ext')
$LOAD_PATH.unshift(LIBDIR)
$LOAD_PATH.unshift(EXTDIR)

require 'curb'

module LeakTrace
  Record = Struct.new(:identifier, :created_location, :closed, keyword_init: true)

  module_function

  def install_multi_trace!
    return if @multi_trace_installed

    @multi_trace_installed = true
    @multi_records = {}

    multi_singleton = class << Curl::Multi
      self
    end

    multi_singleton.alias_method(:__leak_trace_new, :new)
    multi_singleton.define_method(:new) do |*args, **kwargs, &block|
      multi = if kwargs.empty?
        __leak_trace_new(*args, &block)
      else
        __leak_trace_new(*args, **kwargs, &block)
      end

      LeakTrace.multi_records[multi.object_id] ||= Record.new(
        identifier: multi.object_id,
        created_location: caller_locations(1, 6).map(&:to_s),
        closed: false
      )
      multi
    end

    Curl::Multi.class_eval do
      alias_method :__leak_trace__close, :_close

      def _close(*args, **kwargs, &block)
        record = LeakTrace.multi_records[object_id]
        record.closed = true if record

        if kwargs.empty?
          __leak_trace__close(*args, &block)
        else
          __leak_trace__close(*args, **kwargs, &block)
        end
      end
    end
  end

  def multi_records
    @multi_records ||= {}
  end

  def live_multis
    ObjectSpace.each_object(Curl::Multi).to_a
  end

  def fixture_path
    File.expand_path('helper.rb', __dir__)
  end

  def fixture_url
    path = fixture_path.tr('\\', '/')
    "file://#{path.start_with?('/') ? '' : '/'}#{path}"
  end

  def full_gc(compact: false)
    GC.start(full_mark: true, immediate_sweep: true)
    GC.compact if compact && GC.respond_to?(:compact)
    GC.start(full_mark: true, immediate_sweep: true)
  end

  def close_multi(multi)
    return unless multi

    multi.instance_variable_set(:@requests, {}) if multi.respond_to?(:instance_variable_set)
    multi._close
  rescue StandardError
    multi.close rescue nil
  end

  def multi_perform(iterations:, handles:, close:, compact:)
    iterations.times do
      multi = Curl::Multi.new
      handles.times { multi.add(Curl::Easy.new(fixture_url)) }
      multi.perform
    ensure
      close_multi(multi) if close
      multi = nil
      full_gc(compact: compact)
    end
  end

  def multi_gc_cleanup(iterations:, handles:, compact:)
    iterations.times do
      multi = Curl::Multi.new
      handles.times { multi.add(Curl::Easy.new(fixture_url)) }
      multi.perform
      multi = nil
      full_gc(compact: compact)
    end
  end

  def easy_perform(iterations:, compact:)
    iterations.times do
      easy = Curl::Easy.new(fixture_url)
      easy.perform
      easy.close
      easy = nil
      full_gc(compact: compact)
    end
  end

  def download(iterations:, compact:)
    iterations.times do |i|
      Dir.mktmpdir("curb-leak-trace-#{i}") do |dir|
        Curl::Easy.download(fixture_url, File.join(dir, 'helper-copy.rb'))
      end
      full_gc(compact: compact)
    end
  end

  def report(io:, verbose:)
    live = live_multis
    tracked_live = live.filter_map { |multi| multi_records[multi.object_id] }

    io.puts "scanned live Curl::Multi objects: #{live.size}"
    io.puts "tracked Curl::Multi allocations: #{multi_records.size}"
    io.puts "tracked Curl::Multi closes: #{multi_records.count { |_id, record| record.closed }}"

    grouped = tracked_live.group_by { |record| record.created_location.first || '<unknown>' }
    grouped.sort_by { |location, records| [-records.size, location] }.each do |location, records|
      io.puts "live #{records.size}: #{location}"
      next unless verbose

      records.each do |record|
        io.puts "  object_id=#{record.identifier}"
        record.created_location.drop(1).each do |line|
          io.puts "    #{line}"
        end
      end
    end
  end
end

options = {
  scenario: 'multi_perform',
  iterations: 25,
  handles: 3,
  close: true,
  compact: false,
  verbose: false,
  fail_on_live: false
}

OptionParser.new do |opts|
  opts.banner = 'Usage: ruby tests/leak_trace.rb [options]'

  opts.on('--scenario NAME', 'multi_perform, multi_gc_cleanup, easy_perform, download') do |value|
    options[:scenario] = value
  end

  opts.on('--iterations N', Integer, 'number of iterations to run') do |value|
    options[:iterations] = value
  end

  opts.on('--handles N', Integer, 'number of easy handles per multi iteration') do |value|
    options[:handles] = value
  end

  opts.on('--[no-]close', 'explicitly close multi handles when the scenario finishes') do |value|
    options[:close] = value
  end

  opts.on('--compact', 'run GC.compact between iterations when available') do
    options[:compact] = true
  end

  opts.on('--verbose', 'print full creation stacks for live multi handles') do
    options[:verbose] = true
  end

  opts.on('--fail-on-live', 'exit non-zero when any live Curl::Multi objects remain') do
    options[:fail_on_live] = true
  end
end.parse!

LeakTrace.install_multi_trace!

case options[:scenario]
when 'multi_perform'
  LeakTrace.multi_perform(
    iterations: options[:iterations],
    handles: options[:handles],
    close: options[:close],
    compact: options[:compact]
  )
when 'multi_gc_cleanup'
  LeakTrace.multi_gc_cleanup(
    iterations: options[:iterations],
    handles: options[:handles],
    compact: options[:compact]
  )
when 'easy_perform'
  LeakTrace.easy_perform(
    iterations: options[:iterations],
    compact: options[:compact]
  )
when 'download'
  LeakTrace.download(
    iterations: options[:iterations],
    compact: options[:compact]
  )
else
  warn "unknown scenario: #{options[:scenario]}"
  exit 64
end

LeakTrace.full_gc(compact: options[:compact])
LeakTrace.report(io: $stdout, verbose: options[:verbose])

exit 1 if options[:fail_on_live] && LeakTrace.live_multis.any?
