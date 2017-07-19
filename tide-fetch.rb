#!/usr/bin/env ruby

if not ARGV[0] then
  $stderr.puts "Usage: tide-fetch.rb [ITEM]"
  exit 1
end

$item  = ARGV[0]
$attr  = {}
$match = []
UserRules = "#{ENV["HOME"]}/.config/tide/fetch-rules.rb"
Rules = []
Apps = {}

# Define the Rule Language
#-------------------------------------------------------------------------------
class RuleError < StandardError; end

def rule(&block)
  Rules << block
end

def match(regex)
  $match = $item.match(regex)
  if not $match then
    raise RuleError.new()
  end
end

def open_file
  system("xdg-open #{$item}")
end

def open_with(app)
  app = Apps[app] || ENV[app.to_s.upcase]
  raise RuleError.new() if not app
  system("#{app} #{$item}")
end

def find_files(file)
  files = Dir.glob("./**/#{file}").sort_by(&:length)
  raise RuleError.new() if files.length == 0
  files
end

def find_file(file)
  find_files(file).first
end

def mimetype(regex)
  mtype = `file -i #{$item} | cut -d' ' -f2`
  if not mtype.match(regex) then
    raise RuleError.new()
  end
end

# Builtin Rules
#-------------------------------------------------------------------------------

# Run user rules first
if File.exists?(UserRules)
  load UserRules
end

# open urls in the browser
rule do
  match /(https?|ftp):\/\/[a-zA-Z0-9_@\-]+([.:][a-zA-Z0-9_@\-]+)*\/?[a-zA-Z0-9_?,%#~&\/\-+=]+([:.][a-zA-Z0-9_?,%#~&\/\-+=]+)*/
  open_with :browser
end

# open html files with browser
rule do
  match /^.+\.html?/
  $item = find_file($item)
  open_with :browser
end

# open files with address in the text editor
rule do
  match /^([^:]+):([0-9]+)/
  f = find_file($match[1])
  $item = "#{f}:#{$match[2]}"
  open_with :editor
end

# if the file is a text file, edit it
rule do
  $item = find_file($item)
  mimetype /^text\//
  open_with :editor
end

# Main Execution
#-------------------------------------------------------------------------------

Rules.each do |rule|
  begin
    rule.call($item)
    exit 0 # Found a match, positive response
  rescue RuleError
  end
end
exit 1 # No match return error
