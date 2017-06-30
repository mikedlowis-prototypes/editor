#!/usr/bin/env ruby

if not ARGV[0] then
  $stderr.puts "Usage: tide-hl [FILE]"
  exit 1
end

require 'stringio'
require 'strscan'
require 'set'

Styles = {
    # Color Palette
    Base03: 0, Yellow:  8,
    Base02: 1, Orange:  9,
    Base01: 2, Red:     10,
    Base00: 3, Magenta: 11,
    Base0:  4, Violet:  12,
    Base1:  5, Blue:    13,
    Base2:  6, Cyan:    14,
    Base3:  7, Green:   15,

    # Default Highlight Styles
    Normal:       (1 << 4),
    Comment:      (2 << 4),
    Constant:     (3 << 4),
    String:       (4 << 4),
    Char:         (5 << 4),
    Number:       (6 << 4),
    Boolean:      (7 << 4),
    Float:        (8 << 4),
    Variable:     (9 << 4),
    Function:     (10 << 4),
    Keyword:      (11 << 4),
    Operator:     (12 << 4),
    PreProcessor: (13 << 4),
    Type:         (14 << 4),
    Statement:    (15 << 4),
    Special:      (16 << 4),
}

$language = nil
$buf = StringIO.new
$scan = StringScanner.new("")
Rules = []

def languages(langmap)
  file = ARGV[0]
  ext  = File.extname(file).downcase
  langmap.each do |k,v|
    if (v.member?(ext) || v.member?(file))
      return ($language = k)
    end
  end
end

def language(name, &block)
  if $language == name
    block.call()
  end
end

def style_range(spos, epos, style)
  if (Styles[style])
    puts "#{spos},#{epos},#{Styles[style]}"
  end
end

def style_match(s)
  style_range($scan.pos - $scan.matched_size, $scan.pos, s)
end

def rule(regex, &block)
  Rules << [regex, block]
end

def range(start=nil, stop=nil, color=nil)
  rule start do
    beg = $scan.pos - $scan.matched_size
    if not $scan.scan_until stop
    $scan.pos += $scan.rest_size
    end
    style_range(beg, $scan.pos, color)
  end
end

def match(regex, style)
  Rules << [regex, style]
end

def match_sets(regex, setmap)
  rule regex do |m|
    setmap.each do |k,v|
      if v.member?(m)
        style_match(k)
        break;
      end
    end
  end
end

#-------------------------------------------------------------------------------

languages({
  "Ruby" => Set.new(%w[Rakefile Rakefile.rb .rb gpkgfile]),
  "C"    => Set.new(%w[.c .h .cpp .hpp .cc .c++ .cxx]),
})

language "C" do
  types = Set.new %w[
    bool short int long unsigned signed char size_t
    void extern static inline struct typedef union volatile auto const
    int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t
  ]

  keywords = Set.new %w[
    goto break return continue asm case default if else switch while for do sizeof
  ]

  constants = Set.new %w[
    NULL true false
  ]

  range start=/\/\*/, stop=/\*\//, :Comment
  match /\/\/.*$/, :Comment
  match /#\s*\w+/, :PreProcessor
  match /"(\\"|[^"\n])*"/, :String
  match /'(\\.|[^'\n\\])'/, :Char

  match_sets /[a-zA-Z_][0-9a-zA-Z_]*/,
    :Type => types, :Keyword => keywords, :Constant => constants

  match /0x[0-9a-fA-F]+[ulUL]*/, :Number
  match /[0-9]+[ulUL]*/, :Number
end

language "Ruby" do
  keywords = Set.new %w[
    if not then else elsif end def do exit nil
    goto break return continue case default switch while for
  ]

  range start=/%[qQrilwWs]\[/, stop=/\]/, :String
  match /#.*$/, :Comment
  match /"(\\"|[^"\n])*"/, :String
  match /'(\\'|[^'\n])*'/, :String
  match /\/(\\\/|[^\/\n])*\//, :String
  match /[A-Z][0-9a-zA-Z_]*/, :Type
  match_sets /[a-z_][0-9a-zA-Z_]*/, :Keyword => keywords
  match /0x[0-9a-fA-F]+/, :Number
  match /[0-9]+/, :Number
end

#-------------------------------------------------------------------------------

while (not $stdin.eof?) do
  # Read in the input chunk
  $buf = StringIO.new
  len = $stdin.gets.to_i
  if len > 0
    len.times { $buf << $stdin.getc }
    $scan = StringScanner.new($buf.string)
    # scan the input for rule matches
    while (not $scan.eos?) do
      match = false
      Rules.each do |r|
        if $scan.scan(r[0]) then
          if r[1].class == Symbol
            style_match(r[1])
          else
            r[1].call($scan.matched)
          end
          match = true
          break;
        end
      end
      $scan.pos += 1 if (not match)
    end
    puts "0,0,0"
    $stdout.flush
  end
end
