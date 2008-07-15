# DO NOT REMOVE THIS COMMENT - PART OF TESTMODEL.
# Copyright (c)2006 Ross Bamford. See LICENSE.
$CURB_TESTING = true
require 'uri'

$TOPDIR = File.expand_path(File.join(File.dirname(__FILE__), '..'))
$EXTDIR = File.join($TOPDIR, 'ext')
$LIBDIR = File.join($TOPDIR, 'lib')
$:.unshift($LIBDIR)
$:.unshift($EXTDIR)

require 'curb'
require 'test/unit'

$TEST_URL = "file://#{URI.escape(File.expand_path(__FILE__).tr('\\','/').tr(':','|'))}"
