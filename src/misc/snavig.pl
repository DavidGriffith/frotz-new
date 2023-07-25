#!/usr/bin/env perl

# Snavig -- Change an object's shape.
#
# This script's purpose is to transmogrify Frotz source code such that
# very limited linkers can handle the symbols and output the transformed
# sources.  Then these source files can be uploaded into a machine
# wherein compilation is done.
#
# The original purpose of this script and its ancestor is to prepare
# Frotz to be compiled under TOPS20 running on a PDP10 mainframe.  This
# transmogrification is not as easy as it sounds, because:
#
# https://github.com/PDP-10/panda/blob/master/files/kcc-6/kcc/user.doc#L519
#
# "However, the situation is different for symbols with external
# linkage, which must be exported to the PDP-10 linker.  Such names are
# truncated to 6 characters and case is no longer significant."
#
# So you need to map all the symbols in the header files, everything declared
#  "extern" in the source files, and also make sure that your input file names
#  are unambiguous.
#
# This script is heavily based on one by Adam Thornton for pulling in a
# remote git repository of Dumb Frotz, processing it, and then emitting
# source code for use on a PDP10.  This version is meant to be called
# from the Unix Frotz's top level Makefile and work on the files present.

# TODO: Save the names of header files that need to be shortened.  Then
# go through the .c files and shorten them there too.


use strict;
use warnings;

use Cwd;
use File::Copy;
use File::Basename;
use File::Temp qw(tempfile);
use Getopt::Long qw(:config no_ignore_case);


my %options;
GetOptions('usage|?' => \$options{usage},
	'h|help' => \$options{help},
	'v|verbose' => \$options{verbose},
	'q|quiet' => \$options{quiet},
	't|type=s' => \$options{type},
	'i|internal' => \$options{internal},
	);

my $filename_length;
my $transform_symbols;
my $dos_end;		# To convert \n line endings to \r\n

my $type = $options{type};

my $myname = "snavig";
my $topdir = getcwd();
my $target;
my @sources;
my @inputfiles;

my $external_sed = 1;
my $sed = "sed";	# We'll check what sed we have,
my $sed_real;		# and store its identity here.
my $sedfile = "urbzig.sed";
my $sedfilepath;
my $sedinplace = "-i.bak";
my $startbound = "\\b";
my $endbound = "\\b";

my %symbolmap = ();
my %includesmap = ();
my $counter = 0;

my $argc = @ARGV;

print "  Snavig -- Change an object's shape.\n";

if ($argc < 2 || $options{help} || !$type) { usage(); }


if ($options{internal}) { 
	$external_sed = 0;
	print "  Using internal conversion...\n";
} else {
	if (!($sed_real = checksed($sed))) {
		die "  Sed not found.  Either GNU or BSD sed are required.\n";
	}
	print "  Using the $sed_real version of sed...\n";
	if ($sed_real ne "gnu") {
		$startbound = "\[\[:<:\]\]";
		$endbound = "\[\[:>:\]\]";
	}
}

print "  Preparing files for $type.\n";
if ($type eq "tops20") {
	$filename_length = 6;
	$transform_symbols = 1;
	$dos_end = 1;
} elsif ($type eq "dos") {
	$filename_length = 8;
	$dos_end = 1;
} else {
	print "  Unknown target type $type.\n";
	usage();
}

# All parameters but the last one is a source directory.
# The last parameter is the target directory, so pop the last push.
for my $arg (@ARGV) {
	push @sources, $arg;
}
$target = pop @sources;

$sedfilepath = "$topdir/$target/$sedfile";

print "  Copying sources from: ";
my $source_dir_count = @sources;
foreach my $source_dir (@sources) {
	print "$source_dir/";

	if ($source_dir_count == 2 && $counter == $source_dir_count - 2) {
		print " and ";
	} elsif ($source_dir_count > 2 && $counter < $source_dir_count) {
		if ($counter != $source_dir_count - 1) {
			print ", ";
		}
		if ($counter == $source_dir_count - 2) {
			print "and ";
		} 
	}
	$counter++;
}
print " to $target/\n";

# Get a list of input files.
for my $sourcedir (@sources) {
	push @inputfiles, glob("$sourcedir/*.[ch] $sourcedir/*.asm");
}

# Copy input files to output directory.
for my $inputfile (@inputfiles) {
	copy($inputfile, $target);
}

my %transformations;
my %includes;

if ($dos_end) {
	print "  Adding rule to convert line endings of LF to CR LF...\n";
	$transformations{"\$"} = "\\r";
}

if ($transform_symbols) {
	# Scan source code and build a symbol map.
	print "  Building list of symbols to convert...\n";
	%symbolmap = build_symbolmap($target, 6, 6);
	for my $k (reverse(sort(keys %symbolmap))) {
		my $symbol = $symbolmap{$k}{'original'};
		my $newsym = $symbolmap{$k}{'new'};
		$transformations{$symbol} = $newsym;
	}
}


# Shorten the filenames and note shortened headers for rewriting later.
if ($filename_length) {
	print "  Shortening filenames...\n";
	shorten_filenames($target, $filename_length);
	%includesmap = shorten_includes($target, $filename_length);
	for my $k (reverse(sort(keys %includesmap))) {
		my $symbol = $includesmap{$k}{'original'};
		my $newsym = $includesmap{$k}{'new'};
		$includes{$symbol} = $newsym;
	}
}


# Print this as a reference.  May remove later on.
# This file will do the transformations when submitted to GNU sed.
print "  Building sed file $sedfile for reference...\n";
open my $mapfile, '>', $sedfilepath || die "Unable to write $sedfilepath: $!\n";
while (my($symbol, $newsym) = each %transformations) {
	print $mapfile "s/\\b" . $symbol . "\\b/" . $newsym . "/g\n";
}
while (my($oldfilename, $newfilename) = each %includes) {
	print $mapfile "s/" . $oldfilename . "/" . $newfilename . "/g\n";
}
close $mapfile;


print "  Running conversion...\n";
if ($external_sed) {
	chdir $target;
	`$sed -r $sedinplace -f $sedfilepath *c *h`;
} else {
	chdir "$topdir/$target";
	print "  Processing files in $topdir/$target\n";

	my $oldsymbols = join '|', keys %transformations;
	my $oldfilenames = join '|', keys %includes; 
	local $^I = '.bak'; 
	local @ARGV = glob("*.c *.h");
	while (<>) {
		s/\K\b($oldsymbols)\b(?=.)/$transformations{$1}/g;
		s/($oldfilenames)/$includes{$1}/g;
		print;
	}
}


# Maybe remove later.
unlink glob("*.bak");
chdir $topdir;
print "  Done!\n";
exit;

############################################################################

sub usage {
	print "Usage: $myname -t <type> <source dir> [<source dir>] ... <dest dir>\n";
	print "Types supported:\n";
	print "  tops20\n";
	print "  dos\n";
	exit;
}


# Check to see if we have GNU sed.
#
# GNU sed differs from BSD sed in several important and
# mutually-incompatible ways.  Unfortunately one of these affects
# applying snavig to source code.  We need to identify the boundaries of
# words.  That is, you want to match "message", but not "messages". With
# GNU sed, you enclose the troublesome match with "\b".  BSD sed, which
# is found in macOS, requires the use of "[[:<:]]" at the beginning and
# "[[:>:]]" at the end. For background, see
# https://riptutorial.com/sed/topic/9436/bsd-macos-sed-vs--gnu-sed-vs--the-posix-sed-specification
sub checksed {
	my ($sed, @junk) = @_;

	my @sed_result = split(/\s/, `$sed --version 2>&1`);

	# Return -1 if the program doesn't exist or exists but is not sed.
	if (!($sed_result[0] =~ /^.*sed/)) { return -1; }	

	# GNU sed always has "GNU" in the second word.
	if ($sed_result[1] =~ /GNU/) { return "gnu"; }

	# So, this might be a BSD-style sed.
	# If not, we'll know soon enough.
	return "bsd";
}


# Shorten included filenames to conform to filename length limit of $limit.
# This cannot be done within the build_symbolmap() subroutine because
# putting \b must be done for all substitutions.  Doing it for includes
# ends up replacing the included filename with nothing at all, not even quote
# marks.
sub shorten_includes {
	my ($dir, $limit, @junk) = @_;
	my %includemap = ();
#	my $header = 0;
	my $infile;
	my $discard = tempfile();

	for my $file (glob("$dir/*.[ch]")) {
#		if (substr($file, -2) eq ".h") {
#			$header = 1;
#		}
		open $infile, '<', "$file" || die "Unable to read $file: $1\n";
TRANS:		while (<$infile>) {
			# Rewrite includes for shortened filenames.
			if (/^#include/) {
				my $tmpline = $_;
				chomp $tmpline;
				$tmpline =~ s/\\/\//g;
				# Strip leading paths from local includes.
				if (/\"/) {
					(my $tmpline_no_ext = $tmpline) =~ s/\.[^.]+$//;
					%includemap = insert_header(\%includemap, $tmpline, $limit);
				}
				next TRANS;
			}
		}
		close $infile;
	}
	return %includemap;
}


# Identify unique symbol names and rewrite them if they are longer than
# $slimit characters to "A[0-9]{$slimit}".  This does not rewrite the
# symbols for all files.  Another pass is needed for that for which a
# hash of changed symbols is returned.
# Includes are also rewritten to conform to filename length limit of $flimit.
sub build_symbolmap {
	my ($dir, $slimit, $flimit, @junk) = @_;
	my %symbolmap = ();
	my $header = 0;
	my $infile;
	my $discard = tempfile();

	for my $file (glob("$dir/*.[ch]")) {
		if (substr($file, -2) eq ".h") {
			$header = 1;
		}
		open $infile, '<', "$file" || die "Unable to read $file: $1\n";
TRANS:		while (<$infile>) {
#			# Rewrite includes for shortened filenames.
#			if (/^#include/) {
#				my $tmpline = $_;
#				chomp $tmpline;
#				$tmpline =~ s/\\/\//g;
#				# Strip leading paths from local includes.
#				if (/\"/) {
#					(my $tmpline_no_ext = $tmpline) =~ s/\.[^.]+$//;
#					%symbolmap = insert_header(\%symbolmap, $tmpline, $flimit);
#				}
#				next TRANS;
#			}
			# Only fix up externs in C source.
			if (not $header and not /^extern\s/) {
				next TRANS;
			}

			# Don't discard lines with only a closing curly bracket.
			if (/^[}]\s$/) {
				next TRANS;
			}

			# Don't replace these.
			if (/uint8_t/ or /uint16_t/ or /uint32_t/) {
				next TRANS;
			}

			# In headers, we need to fix up declarations too.
			# Our typedefs, enums, and structs happen to have short
			#   names, so don't bother.  AT
			# For the current Frotz codebase, we need to check
			# typedefs, enums, and structs too.  DG
			unless (/^extern\s/ or /^void\s/ or /^zchar\s/ or
				/^char\s/ or /^zbyte\s/ or /^zword\s/  or
				/^zlong\s/ or /^int\s/ or /^bool\s/ or
				/^typedef\s/ or /^enum\s/ or /^struct\s/ or
				/^static\s/ or /^[}]\s/ or
				/^\#define\s/ or /^\#ifndef\s/) {
				next TRANS;
			}

			chomp;

			my $symbol;
			my $symbol2;
			my $symbol3;

			if (/^extern/) {
				# Symbol is the last word on the line before
				#  return type.
				# Drop the semicolon.
				my $tmpline = substr($_,0,-1);
				# Strip everything after a paren.
				$tmpline =~ s/\(.*//;
				$symbol = (split ' ', $tmpline)[-1];
			} elsif (/^typedef unsigned/) {
				# Symbol for this is the fourth.
				$symbol = (split ' ', $_)[3];
			} elsif (/^static\s/) {
				# Catch function type and name in 
				# "static foobar_gate_t functionname(int foo)"
				$symbol = (split ' ', $_)[1];
				$symbol2 = (split ' ', $_)[2];
				$symbol3 = (split ' ', $_)[3];
			} else {
				# Otherwise it's the second
				$symbol = (split ' ', $_)[1];
			}

			if (!$symbol) {
				next TRANS;
			}

			$symbol = clean_symbol($symbol);
			%symbolmap = insert_symbol(\%symbolmap, $symbol, $_, $slimit);
			if ($symbol2) {
				$symbol2 = clean_symbol($symbol2);
				%symbolmap = insert_symbol(\%symbolmap, $symbol2, $_, $slimit);
			}
			if ($symbol3) {
				$symbol3 = clean_symbol($symbol3);
				%symbolmap = insert_symbol(\%symbolmap, $symbol3, $_, $slimit);
			}
		}
		close $infile;
	}

	return %symbolmap;
}


# Get rid of dereference, pointer, index, parameter notation, etc.
sub clean_symbol {
	my ($symbol, @junk) = @_;

	$symbol =~ s/^\*//;
	$symbol =~ s/^\&//;
	$symbol =~ s/[\[\]].*//;
	$symbol =~ s/[\(\)].*//;
	$symbol =~ s/;.*//;
	$symbol =~ s/\/\*//;
	$symbol =~ s/,.*//;

	return $symbol;
}


# Remove leading path on includes header files and if necessary, shorten
# the filename before addding it to the symbol map.  It makes the symbol
# map not really a symbol map, but, meh.
#
sub insert_header {
	my ($map_ref, $file, $length, @junk) = @_;
	my %symbolmap = %{$map_ref};
	my $to;
	my $unstripped;
	my $lkey;
	my $from;

	unless (/\"/) {
		return %symbolmap;
	}

	$from = $file;
	$to = $from;

#	$from =~ s/\#/\\#/g;
#	$from =~ s/\"/\\\"/g;
#	$from =~ s/\./\\\./g;
#	$from =~ s/\//\\\//g;

	$from =~ s/\s\*/\\s\*/g;
#	$from = "^" . $from;

	$to = $from;

	$to =~ s/\.[^.]+$//g;
	$to =~ s/^\#include\s//g;
	$to =~ s/\"//g;
	$to = basename($to);

	if (length($to) gt 6) {
		$to = substr($to, 0, $length);
	}
#	$to = "\\#include \\\"" . basename($to) . "\.h\\\"";
	$to = "\#include \"$to.h\"";

	$lkey=sprintf("%02d",length($from)) . $from;
	if (not $symbolmap{$lkey}) {
		$lkey=sprintf("%02d",length($from)) . $from;
		$symbolmap{$lkey} = ();
		$symbolmap{$lkey}{'original'} = $from;
		$symbolmap{$lkey}{'new'} = $to;
	}

	return %symbolmap;
}


# If necessary, insert symbol into the symbolmap and return the new map.
sub insert_symbol {
	my ($map_ref, $symbol, $line, $length, @junk) = @_; 
	my %symbolmap = %{$map_ref};

	# Only fix up symbols that are long enough to matter.
	if (length($symbol) <= $length) {
		return %symbolmap;
	}

	my $sedstr;
	# We want to sort substitutions by length in order
	#  to not do incorrect substring replacement.
	# We assume all identifiers are < 100 characters.
	# This is not Java.
	my $lkey=sprintf("%02d",length($symbol)) . $symbol;
	my $newid;
	my $spec = "%0" . ($length - 1) . "d";
	if (not $symbolmap{$lkey}) {
		$newid = "A" . sprintf($spec, $counter);
		$symbolmap{$lkey} = ();
		$symbolmap{$lkey}{'original'} = $symbol;
		$symbolmap{$lkey}{'new'} = $newid;
		$counter++;
		$line =~ s/$symbol/$newid/;
	} else {
		$newid = $symbolmap{$lkey}{'new'};
	}
	return %symbolmap;
}


# Shorten filenames to $length plus 1-character extension.
sub shorten_filenames {
	my ($dir, $length, @junk) = @_;
	my @files;
	my %collision = ();
	my $new;
	my $orig_dir = getcwd();

	chdir "$dir";
	@files = glob("*.[ch]");
	for my $f (@files) {
		my $asc=48; # Start with "0"
		if (length($f) > $length+2) { # counting the extension
			my $tnam = substr($f,0,$length);
			while ($collision{"$tnam".substr($f, -2)}) {
				substr($tnam,5) = chr($asc);
				$asc += 1;
				if ($asc == 58) {
					$asc=65; # Skip punctuation
				}
				# We really really should not run out of
				# numbers and capital letters.
			}
			$collision{$tnam} = 1;
			if (substr($f, -2) eq ".h") {
				$new = fileparse($tnam, qr/\.[^.]*/) . ".h";
			} else {
				$new = fileparse($tnam, qr/\.[^.]*/) . ".c";
			}
			rename $f, $new;
		}
	}
	chdir $orig_dir;
}
