#!/usr/bin/env perl

# Don't judge, I've never written Perl before :D

use v5.26;
use strict;
use warnings;
use FindBin;
use File::Basename;
use List::Util 'max';
use Data::Dumper;

# Set STDIN, STDOUT, and STDERR to UTF-8 encoding:
use open qw(:std :utf8);

my $this_pl = basename $0;


# ============================================================================== 
# Subroutines
# ==============================================================================


# Converts a number into a hexadecimal string.
#
# This is the inverse of the builtin `hex` function.
sub to_hex {
    my $x = shift // return "";
    return sprintf "%X", $x;
}


# Scans the given UCD data file (.txt) and calls the given callback
# once for every line.
#
# The callback is passed three arguments:
#
#   $lo     = The start of the codepoint range for the line.
#   $hi     = End of the range; equal to $lo for single CPs.
#   $fields = Arrayref of semicolon-separated fields on the line.
sub foreach_ucd_line {
    my $file     = shift or die "Missing \$file argument";
    my $callback = shift or die "Missing \$callback argument";
    
    open(my $fh, '<', $file) or die "Failed to open $file";
    
    while (<$fh>) {
        chomp;
        s/\s*#.*$//;        # Strip comments
        next if /^\s*$/;    # Skip if line is empty
        
        # Pull codepoints from the front of the line
        unless ( s/^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s*// ) {
            die "Missing codepoint range in $file";
        }
        
        my $lo = hex($1);
        my $hi = hex($2 // $1);
        
        # Remove single `;` between codepoints and remaining fields
        s/^;\s*//;
        
        # Split the remaining fields based on semicolons
        my @fields = split /\s*;\s*/;
        
        # Pass the codepoints and fields into the callback
        $callback->($lo, $hi, \@fields);
    }
}


# Reads properties from the given UCD file.
#
# Arguments:
#
# $1 = Path to UCD file.
# $2 = Regex that matches the property name being looked for.
#
# When $2 is not provided, it is assumed that the file contains the
# definitions for only a single property. In this case, the second field
# in the file (right after the codepoint range) is interpreted as the
# property's value.
#
# When $2 is a string or regex, it is matched against the second field
# to filter lines to the desired property; the third field is then taken
# as the property's value.
#
# Returns a nested hash of the form:
#
# {
#     val_names => [...] # List of property values found in the file
#     cp_values => {...} # Maps codepoints to value indices
# }
sub read_prop_values {
    my $file    = shift or die "Missing \$file argument";
    my $filter  = shift;
    
    my @val_names;
    my %cp_values;
    
    # Temporary; used to lookup value -> index until we're finished.
    my %val_index;
    
    foreach_ucd_line $file, sub {
        my ($lo, $hi, $fields) = @_;
        
        if ( defined($filter) ) {
            my $pname = shift @$fields or die "Expected property name to filter";
            return unless $pname eq $filter;
        }
        
        my $value = shift @$fields or die "Expected property value";
        
        # Have we seen this value before?
        my $index = $val_index{$value};
        if ( !defined($index) ) {
            # If not, push it into our list and return its index.
            push @val_names, $value;
            $index = $val_index{$value} = $#val_names;
        }
        
        # Now, for all codepoints in the range, insert this index.
        for my $cp ($lo .. $hi) {
            $cp_values{to_hex $cp} = $index;
        }
    };
    
    return {
        val_names => \@val_names,
        cp_values => \%cp_values,
    };
}


# Reads the lines in the given file and finds all codepoints that have
# the given property.
#
# Returns a hash with one key for every codepoint which has the given
# property (and a truthy value of '1').
sub find_boolean_props {
    my $file = shift or die "Missing \$file argument";
    my $prop = shift or die "Missing \$prop argument";
    
    my %cp_values;
    
    foreach_ucd_line $file, sub {
        my ($lo, $hi, $fields) = @_;
        my $name = shift @$fields or die "Expected property name";
        
        # Is this the property we're looking for?
        if ( $name eq $prop ) {
            for my $cp ($lo .. $hi) {
                $cp_values{to_hex $cp} = 1;
            }
        }
    };
    
    return \%cp_values;
}


# Checks if the elements of `$needle` appear inside `$haystack`, starting at
# `$offset`.
sub poor_mans_memcmp {
    my ($needle, $haystack, $offset) = @_;
    
    # NB: can find length of array by simply referring to it in a scalar
    # context (such as assigning to a `my $scalar`); but I find it clearer
    # to use the `scalar` keyword explicitly.
    my $nlen = scalar @$needle;
    my $hlen = scalar @$haystack;
    
    # We want to scan `len(needle)` elements of haystack; does haystack
    # have enough elements left for that, if we start at $offset?
    if ( $offset + $nlen > $hlen ) {
        return 0;
    }
    
    # NB: Since arrays are passed to subroutines as references, we need to
    # dereference them first. So `$#$` says "give me a scalar ($), which is the
    # last-index-of (#), the array *referenced* by this other scalar ($)".
    # We could fetch elements by doing `$$needle[$i]`, but the more idiomatic
    # approach is to use the `->` operator, apparently (where the `->` is doing
    # the de-referenceing, then the `$` gets me the scalar out of the array).
    for my $i (0 .. $#$needle) {
        my $j = $offset + $i;
        return 0 if $needle->[$i] != $haystack->[$j];
    }
    
    return 1;
}


# Generates a pair of major and minor lookup tables for the given hash
# of codepoint indices.
#
# ------------------------------------------
# LUT format
# ------------------------------------------
#
# To encode a LUT large enough to store 0x110000 values (U+0000 to U+10FFFF), we
# will borrow (steal) libgrapheme's approach:
#
# For a given codepoint 0xABCD12, we use the high 2 bytes (0xABCD) as an index
# into a first lookup table (called 'major'), and then use the low byte (0x12)
# as an offset into a second table ('minor'), starting where 'major' pointed.
#
# This way if two 0xABCD groups happen to have the exact same set of 256 low
# bytes, we can share their offset into the second LUT. For example, if
# codepoints U+000000 to U+0000FF have some sequence (1, 0, 0, ..., 0, 0, 0),
# then 'major' will have the value '0' at index 0x00, with 'minor' storing that
# sequence starting at index '0'. But then, if a later sequence U+A11300 to
# U+A113FF *also* has the sequence (1, 0, 0, ..., 0, 0, 0), then the 'major'
# table can have another '0' at index 0xA113; we do not need to push another
# copy of the sequence into 'minor'.
#
# This approach yields an impressive compression ratio compared to a straight
# lookup table of size 0x110000 (1,114,112!).
#
# ------------------------------------------
#
# Any codepoints not found in the map will be assigned a value of 0. To
# leave space for 0, the indices in the LUTs will be +1 from what is
# stored in the map. To disable this behaviour, pass a truthy value for
# the second argument.
#
# Returns a hash of the form:
#
# {
#     major => [...],
#     minor => [...],
# }
sub generate_luts {
    my $cp_values = shift or die "Missing \$cp_values argument";
    my $skip_incr = shift // 0;
    
    my @major;
    my @minor;
    
    for my $upper (0x0000 .. 0x10FF) {
        # Generate the array of indices for these 256 codepoints:
        my @group;
        for my $lower (0x00 .. 0xFF) {
            my $cp = ($upper << 8) | $lower;
            
            my $cp_hex = sprintf "%X", $cp;
            my $index = $cp_values->{$cp_hex};
            if ( !defined($index) ) {
                $index = 0;
            } elsif ( !$skip_incr ) {
                $index += 1;
            }
            
            push @group, $index;
        }
        
        # Does this set of indices appear anywhere in 'minor' yet?
        my $minor_index;
        for my $i ( 0 .. $#minor ) {
            if ( poor_mans_memcmp \@group, \@minor, $i ) {
                $minor_index = $i;
                last;
            }
        }
        
        # If not, we need to push it onto the end.
        if ( !defined($minor_index) ) {
            $minor_index = @minor; # @array used in $scalar context = gives length
            push @minor, @group;   # pushing array onto array pushes all items
        }
        
        $major[$upper] = $minor_index;
    }
    
    return {
        major => \@major,
        minor => \@minor,
    };
}


# Gets the smallest bit width required to store the given value. Only supports
# up to 64 bits because Perl got mad when I typed a `0xFFFFFFFF_FFFFFFFF` and I
# couldn't be bothered to investigate bigger ints.
sub get_bitwidth {
    my $n = shift or die "Missing \$n argument";
    if ( $n <= 0xFF ) {
        return 8;
    } elsif ( $n <= 0xFFFF ) {
        return 16;
    } elsif ( $n <= 0xFFFF_FFFF ) {
        return 32;
    } else {
        return 64;
    }
}

# Formats a number of bytes into a human-readable string.
sub format_bytes {
    my $bytes = shift // return '? B';
    return '0 B' if $bytes == 0;
    
    my $base = 1024;
    my @units = ('B', 'KiB', 'MiB', 'GiB');
    my $i = 0;
    while ( $bytes >= $base && $i < $#units ) {
        $bytes /= $base;
        $i++;
    }
    
    return sprintf "%.2f %s", $bytes, $units[$i];
}


# Given a hashref to one of the major+minor LUT pairs, returns the total size
# of the LUTs in bytes.
#
# Accepts an optional second scalar reference parameter as an accumulator for
# the overall size of multiple LUTs.
sub measure_luts {
    my $luts = shift or die "Missing \$luts argument";
    my $tref = shift;
    
    my $major = $luts->{major};
    my $minor = $luts->{minor};
    
    my $num_maj = scalar @$major; # Should always be 0x1100
    my $num_min = scalar @$minor; # Will vary based on LUT
    my $bits_maj = get_bitwidth max(@$major);
    my $bits_min = get_bitwidth max(@$minor);
    my $type_maj = "uint${bits_maj}_t";
    my $type_min = "uint${bits_min}_t";
    
    my $size_maj = $bits_maj / 8 * $num_maj;
    my $size_min = $bits_min / 8 * $num_min;
    my $size_tot = $size_maj + $size_min;
    my $fmt_tot = format_bytes($size_tot);
    
    if ( defined($tref) ) {
        $$tref += $size_tot;
    }
    
    # U+00D7 = \times
    return sprintf "%4d \x{00D7} $type_maj + %5d \x{00D7} $type_min = %9s",
        $num_maj,
        $num_min,
        $fmt_tot;
}


# Writes a C enum's definition (to stdout) for the given list of values, in
# order, starting at 1.
#
# Arguments:
#
# $1 = values:      Array ref for the ordered list of enum values.
# $2 = enum name:   What to name the enum and typedef (eg. "word_break_prop")
# $3 = default:     Name for the fallback (0) value of the enum (eg. "Other")
sub write_c_enum {
    my $values    = shift or die "Missing \$values argument";
    my $enum_name = shift or die "Missing \$enum_name argument";
    my $default   = shift // "OTHER";
    
    my $prefix = uc $enum_name;
    $default = uc $default;
    
    say STDOUT "typedef enum $enum_name {";
    say STDOUT "    ${prefix}_${default} = 0,";
    for my $i (0 .. $#$values) {
        my $vname = uc $values->[$i];
        my $index = $i + 1;
        say STDOUT "    ${prefix}_${vname} = $index,";
    }
    # NUM_PROPS needs to be +1 since we added an extra one variant for 'OTHER'
    say STDOUT "    NUM_${prefix}S = " . ((scalar @$values) + 1);
    say STDOUT "} ${enum_name}_t;";
    say STDOUT "";
}

# Writes a C array (to stdout) for the given list of values, with the given
# name.
sub write_c_array {
    my $values     = shift or die "Missing \$values argument";
    my $array_name = shift or die "Missing \$array_name argument";
    
    my $maxval = max @$values;
    my $bitsize = get_bitwidth $maxval;
    my $datatype = "uint${bitsize}_t";
    my $maxwidth = length sprintf("%d", $maxval);
    
    # Fit as many of these values on a line as we can within a reasonable
    # limit (16, or 32 if it fits under 80 chars, or 48, etc so on).
    my $line_length = 16;
    while ( ($maxwidth + 1) * $line_length < 80 ) {
        $line_length += 16;
    }
    
    say STDOUT "$datatype ${array_name}[] = {";
    my $l = 0;
    for my $i (0 .. $#$values) {
        my $val = $values->[$i];
        
        print STDOUT "    " if $l == 0;
        printf STDOUT "%*d", $maxwidth, $val;
        print STDOUT "," unless $i == $#$values;
        if ( ++$l == $line_length ) {
            $l = 0;
            print STDOUT "\n";
        }
    }
    print STDOUT "\n" unless $l == 0;
    say STDOUT "};";
    say STDOUT "";
}


# ============================================================================== 
# Main script start
# ==============================================================================


say STDERR "=> Reading properties from files...";

my $gb_props = read_prop_values "${FindBin::Bin}/data/GraphemeBreakProperty.txt";
say STDERR "==> Done reading GraphemeBreakProperty.txt";

my $extpicto = find_boolean_props "${FindBin::Bin}/data/emoji-data.txt", "Extended_Pictographic";
say STDERR "==> Done reading Extended_Pictographic from emoji-data.txt";

my $in_props = read_prop_values "${FindBin::Bin}/data/DerivedCoreProperties.txt", "InCB";
say STDERR "==> Done reading InCB (Indic_Conjunct_Break) from DerivedCoreProperties.txt";

my $wb_props = read_prop_values "${FindBin::Bin}/data/WordBreakProperty.txt";
say STDERR "==> Done reading WordBreakProperty.txt";

=pod
print Data::Dumper->Dump([
    $wb_props,
    $gb_props,
    $extpicto,
    $in_props,
], [qw(*wb_props *gb_props *extpicto *in_props)]);
# exit;
=cut

say STDERR "=> Generating lookup tables (LUTs) for 1,114,112 codepoints...";

my $total_bytes = 0;

my $gb_luts = generate_luts $gb_props->{cp_values}; say STDERR "==> Done GraphemeBreak LUTs:         " . measure_luts $gb_luts, \$total_bytes;
my $ep_luts = generate_luts $extpicto, 'skip';      say STDERR "==> Done Extended_Pictographic LUTs: " . measure_luts $ep_luts, \$total_bytes;
my $in_luts = generate_luts $in_props->{cp_values}; say STDERR "==> Done Indic_Conjunct_Break LUTs:  " . measure_luts $in_luts, \$total_bytes;
my $wb_luts = generate_luts $wb_props->{cp_values}; say STDERR "==> Done WordBreak LUTs:             " . measure_luts $wb_luts, \$total_bytes;
say STDERR "=> Done LUTs. Total size: " . format_bytes($total_bytes);

say STDERR "=> Writing output to stdout...";

say STDOUT "/* Generated by $this_pl */\n";
say STDOUT "#include <stdint.h>\n";

write_c_enum $wb_props->{val_names}, "word_break_prop";
write_c_enum $gb_props->{val_names}, "grapheme_break_prop";
write_c_enum $in_props->{val_names}, "indic_conjunct_break_prop", "NONE";

write_c_array $wb_luts->{major}, "word_break_major";
write_c_array $wb_luts->{minor}, "word_break_minor";

write_c_array $gb_luts->{major}, "grapheme_break_major";
write_c_array $gb_luts->{minor}, "grapheme_break_minor";

write_c_array $ep_luts->{major}, "extended_pictographic_major";
write_c_array $ep_luts->{minor}, "extended_pictographic_minor";

write_c_array $in_luts->{major}, "indic_conjunct_break_major";
write_c_array $in_luts->{minor}, "indic_conjunct_break_minor";

say STDERR "Done!";
