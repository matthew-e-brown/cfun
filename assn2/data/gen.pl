#!/usr/bin/env perl

# Don't judge, I've never written Perl before :D

use v5.10;
use strict;
use warnings;
use File::Basename;
use Data::Dumper;

my $this_pl = basename $0;

my $in_file = shift;
my $out_file = shift;
if ( !defined $in_file or !defined $out_file ) {
    die "Usage: $0 <in_file> <out_file>\n";
} elsif ( -e $out_file ) {
    die "$out_file already exists!";
}

open(my $fh, '<', $in_file) or die "Could not open $in_file\n";

# =============================================================================
# Step 1: Read in all properties and generate a map
# =============================================================================

say "Parsing $in_file...";

my $prop_name;
my @missing;
my $hit_missing;

# Read the first chunk of lines
while (<$fh>) {
    chomp;

    if ( /^#\s+Property:\s+([A-Za-z_]+)/ ) {
        $prop_name = $1;
        next;
    }
    
    elsif ( /^#\s+\@missing:\s+([0-9A-F]+)\.\.([0-9A-F]+);\s+([A-Za-z_]+)/ ) {
        my $lo = hex($1);
        my $hi = hex($2);
        my $val = $3;
        
        # NB: () is the usual syntax for arrays in Perl; [] creates an
        # arrayref in-place.
        push @missing, [$hi + 1, $val];
        
        $hit_missing = 1;
        next;
    }
    
    # After hitting our first '@missing' line, the next time we see a
    # line that isn't one, we are done.
    elsif ( $hit_missing ) {
        last;
    }
}

sub find_missing {
    my ($cp) = @_;
    for my $tuple (@missing) {
        my ($max, $val) = @$tuple;
        if ( $cp < $max ) {
            return $val;
        }
    }
    die "missing \@missing for codepoint $cp";
}

my %value_lut;      # codepoint -> property value
my %index_lut;      # property value -> numerical index
my @vals_sort;      # keeps names of values in the order we found them

sub value_idx {
    my ($val) = @_;
    if ( !exists $index_lut{$val} ) {
        $index_lut{$val} = scalar keys %index_lut;
        push @vals_sort, $val;
    }
    return $index_lut{$val};
}

while (<$fh>) {
    chomp;

    next unless /^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s*;\s+([A-Za-z_]+)/;
    
    # We need them as numbers to loop thru them, even tho we're gonna turn
    # them right back into hex strings to hashmap them.
    my $lo = hex($1);
    my $hi = hex($2 // $1);
    my $val = $3;
    
    my $idx = value_idx $val;
    
    for my $cp ($lo .. $hi) {
        my $hex = sprintf "%X", $cp;
        $value_lut{$hex} = $idx;
    }
}

=begin comment
    $Data::Dumper::Sortkeys = 1;
    # NB: \ creates a reference instead of passing the whole hash by-value
    print Dumper(\%value_lut);
    print Dumper(\%index_lut);
=end comment
=cut


# =============================================================================
# Step 2: Generate the LUTs
# =============================================================================

say "Generating lookup tables (LUTs)...";

# We will borrow (steal) libgrapheme's approach:
#
# For a codepoint 0xABCD12, we use the top 2 bytes (0xABCD) as an index into a
# first lookup table, and then use the last byte (0x12) as an offset from where
# that one points.
#
# That way, if two 0xABCD groups happen to have the exact same set of 256 lo
# bytes, we can share their offset into the second LUT.

sub poor_mans_memcmp {
    my ($arr1, $arr2, $a2_offset) = @_;
    # NB: passing arrays to a subroutine requires a reference (so we
    # need to access them with @$a, and pass them with \@a). Then to actually
    # index it, since it's a reference and we want a scalar, we need to do
    # $$arr[$i]. Or, the more modern and preferred syntax, is to use `->`.
    # `$arr->[$i]`. The arrow dereferences, then the sigil ($) says, "ok I'm
    # getting a scalar out of this."
    #
    # Also, finding the length of an array is done just by evaluating
    # it in scalar context. So `$len = @arr` works. But I find it clearer
    # to use an explicit `scalar`.
    
    # We want to scan `size(@$arr1)` elements in @$arr2, starting at
    # $offset. But we can't do that if arr1 is too long!
    my $a1_len = scalar @$arr1;
    my $a2_len = scalar @$arr2;
    if ( $a2_offset + $a1_len > $a2_len ) {
        return 0;
    }
    
    for my $i1 (0 .. $#$arr1) {
        my $i2 = $i1 + $a2_offset;
        return 0 if $arr1->[$i1] != $arr2->[$i2];
    }
    
    return 1;
}

my @major;
my @minor;

# Codepoints are U+000000 to U+10FFFF. So the upper 2 bytes range to 10FF.
# Then the inner loop goes 0x00 to 0xFF.
for my $upper (0x0000 .. 0x10FF) {
    # Generate the array of indices for these 256 bytes:
    my @group;
    for my $lower (0x00 .. 0xFF) {
        my $cp = ($upper << 8) + $lower;
        
        my $hex = sprintf "%X", $cp;
        my $idx = $value_lut{$cp} // (value_idx find_missing $cp);
        
        push @group, $idx;
    }
    
    # Does this subsequence exist anywhere else in 'minor' yet?
    my $minor_idx;
    for my $i (0 .. $#minor) {
        if ( poor_mans_memcmp \@group, \@minor, $i ) {
            $minor_idx = $i;
            last;
        }
    }
    
    # If it does, that's the offset we want for major! And we don't need to
    # push anything to minor. It can stay small. Otherwise, we push these
    # 256 bytes onto minor first.
    unless ( defined($minor_idx) ) {
        $minor_idx = @minor; # scalar ($) on array = gimme length
        push @minor, @group;
    }

    $major[$upper] = $minor_idx;
    
    my $upperhex = sprintf "%04X", $upper;
    # say "$upperhex => minor[$minor_idx]"
}

# Done! Give a quick readout.
my $num_minors = scalar @minor;
my $num_majors = scalar @major; # should always just be 0x10FF
my $c_ratio = sprintf "%.2f", (0x110000 / ($num_minors + $num_majors));
my $c_prcnt = sprintf "%.2f", (($num_minors + $num_majors) / 0x110000) * 100;

# 1114112 = 0x110000 = 0x10FFFF + 1;
say "LUTs: Encoded 1,114,112 codepoints as",
    " $num_majors + $num_minors entries",
    " (ratio: ${c_ratio}, ${c_prcnt}%).";

=begin comment
    print Dumper(\@major);
    print Dumper(\@minor);
=end comment
=cut


# =============================================================================
# Step 3: Output the LUTs as C arrays
# =============================================================================

say "Formatting & writing LUTs...";

# Hm... guess I gotta decide how I want everything formatted for C now.

# given Property: 'Word_Break', with values 'ALetter', 'MidNumLet',
# 'Double_Quote', 'Extend', # etc., we want:
#
# typedef enum word_break_prop {
#   WORD_BREAK_PROP_ALETTER = 0,
#   WORD_BREAK_PROP_MIDNUMLET = 1,
#   WORD_BREAK_PROP_DOUBLE_QUOTE = 2,
#   WORD_BREAK_PROP_EXTEND = 3,
#   ...
# } word_break_prop_t;
#
#
# 

# We also want to figure out what the smallest integer type we need to store
# these values is.

sub get_ctype {
    my ($n) = @_;
    if ( $n <= 0xFF ) {
        return "uint_least8_t";
    } elsif ( $n <= 0xFFFF ) {
        return "uint_least16_t";
    } elsif ( $n <= 0xFFFF_FFFF ) {
        return "uint_least32_t";
    } else {
        return "uint_least64_t";
    }
}

my $major_ty = get_ctype (scalar @major);
my $minor_ty = get_ctype (scalar @minor);

say "LUT values will fit inside of: $major_ty and $minor_ty.";

# (oh no, I also just realized that for word-break algorithm we will need to
# also parse emoji-data.txt since we need to know about Extended_Pictographic
# property... that's gonna need a small re-org of this script... but that's a
# tomorrow problem).

# when ready, we'll replace the standard printing with a filehandle.
# to print to $fh, do `say $fh "msg"`.
# open($fh, '>', $out_file);

say "/* autogenerated by $this_pl */";
say "#include <stdint.h>\n";

my $prop_lc = lc $prop_name;
my $prop_uc = uc $prop_name;

say "tyepdef enum ${prop_lc}_prop {";
for my $name (@vals_sort) {
    my $name_uc = uc $name;
    my $index = $index_lut{$name};
    say "    ${prop_uc}_PROP_${name_uc} = $index,";
}
say "    NUM_${prop_uc}_PROPS = " . scalar @vals_sort;
say "} ${prop_lc}_prop_t;";
