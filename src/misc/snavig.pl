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
#	print "$oldfilename ---> $newfilename\n";
}
close $mapfile;


print "  Running conversion...\n";
if ($external_sed) {
	chdir $target;
	`$sed -r $sedinplace -f $sedfilepath *c *h`;
} else {
	chdir "$topdir/$target";
	print "  Processing files in $topdir/$target\n";

#	my $oldsymbols = join '|', keys %transformations;
	my @poop = keys %transformations;


	# WTF does this not result in matching the whole damn file?
#	my $mysymbols = "zoptopt|init_setup|print_string|ostream_record|countdown|z_throw|undo_struct|stream_new_line|enable_wrapping|zscii_to_latin1|z_show_status|script_open|reset_screen|stream_mssg_on|encoded|init_process|os_stop_sample|timeout|dumb_show_screen|z_set_window|z_get_parent|screen_mssg_off|memory_open|hide_lines|mem_diff|stream_word|os_start_sample|load_operand|show_cell_bbcode|z_buffer_mode|os_peek_colour|z_call_n|os_erase_area|default_name|get_window_font|z_set_attr|print_version|erase_window|dumb_init_input|split_window|os_storyfile_seek|z_check_arg_count|hot_key_undo|dumb_output_handle_setting|z_art_shift|init_memory|z_erase_window|script_write_input|get_max_width|os_set_cursor|time_ahead|screen_cells|read_number|z_print_unicode|mouse_y|dumb_discard_old_input|rv_mode|screen_data|myresource|z_rtrue|option_zcode_path|z_get_prop_addr|direct_call|read_long|os_process_arguments|$|os_init_screen|z_get_prop|z_restore|z_insert_obj|build_timestamp|os_finish_with_sample|xgetchar|mark_all_unchanged|prop_addr|z_print_addr|os_set_colour|winarg2|z_log_shift|os_read_line|z_erase_line|free_undo|blorb_err|runtime_usage|error_count|z_split_window|visual_bell|tokenise_line|os_get_text_style|dumb_read_misc_line|enable_scrolling|os_init_sound|show_line_prefix|f_setup_t|show_line_numbers|z_print_char|dumb_display_char|z_window_style|script_erase_input|current_bg|zputchar|font_height|z_inc_chk|os_display_string|object_address|update_cursor|dumb_row|my_strndup|z_print_form|set_window|z_rfalse|utf8_to_zchar|z_new_line|z_put_wind_prop|record_write_key|rv_names|script_char|user_text_height|unlink_object|memory_close|make_cell|z_scan_table|console_read_input|undo_count|format_t|init_err|hot_key_restart|counter|flush_buffer|load_all_operands|script_close|latin1_to_ascii|hot_key_seed|current_fg|story_size|z_restart|os_tick|z_loadw|z_print_ret|decoded|option_sound|z_make_menu|z_print_paddr|z_print_obj|z_header|os_check_unicode|latin1_to_ibm|z_verify|end_of_sound|winarg0|runtime_error|print_num|padding|os_char_width|z_set_colour|init_buffer|z_set_margins|z_encode_text|z_read_mouse|bbcode_colour|enable_scripting|round_div|__illegal__|interval|reset_cursor|z_mouse_window|mouse_x|need_newline_at_exit|stream_mssg_off|dest_row|screen_word|font_width|dumb_getline|init_undo|z_set_text_style|err_messages|dumb_set_picture_cell|z_header_t|amiga_screen_model|screen_new_line|lookup_text|routine|screen_mssg_on|z_input_stream|pad_status_line|z_remove_obj|resize_screen|dumb_read_line|mem_undiff|first_property|dumb_init_output|read_key_buffer|blorb_fp|units_left|z_get_cursor|os_display_char|start_next_sample|frotz_to_dumb|story_fp|start_sample|z_print|replay_read_input|screen_write_input|show_row|blorb_map|auxilary_name|f_setup|compression_names|pict_info|dumb_set_cell|tokenise_text|hot_key_recording|restart_header|read_word|z_storeb|__extended__|alphabet|z_get_prop_len|z_catch|validate_click|replay_read_key|my_strdup|show_cell|os_warn|script_width|script_mssg_off|z_copy_table|check_timeout|z_ret_popped|dumb_display_user_input|show_cell_irc|story_id|dumb_changes_row|input_redraw|z_num_to_index|completion|sampledata_t|ostream_screen|translate_to_zscii|is_terminator|z_picture_table|plain_ascii|os_repaint_window|istream_replay|load_string|z_clear_attr|z_call_s|discarding|z_put_prop|os_from_true_colour|record_close|blorb_res|hot_key_quit|z_get_sibling|specifier|encode_text|z_erase_picture|os_init_setup|more_prompts|uint32_t|replay_code|z_get_next_prop|playing|record_char|erase_screen|mouse_button|uint16_t|read_line_buffer|os_reset_screen|set_more_prompts|stream_char|ostream_memory|compression_mode|story_name|print_char|os_fatal|save_undo|replay_open|os_scroll_area|os_read_file_name|zgetopt|update_attributes|z_restore_undo|os_restart_game|z_read_char|reserve_mem|spurious_getchar|current_style|finished|z_print_table|dumb_dump_screen|show_cell_ansi|reset_window|print_object|os_draw_picture|num_pictures|read_string|truncate_question_mark|z_print_num|stream_read_key|show_line_types|translate_from_zscii|do_more_prompts|user_random_seed|enable_buffering|os_font_data|os_beep|seed_random|zoptarg|dumb_handle_setting|restore_quetzal|new_line|os_read_key|z_get_child|os_storyfile_tell|init_sound|next_sample|decode_text|z_push_stack|screen_char|z_window_size|ostream_script|hot_key_debugging|refresh_text_style|z_save_undo|user_text_width|dumb_copy_cell|os_set_font|stream_read_input|z_scroll_window|z_check_unicode|os_set_text_style|frame_count|restart_screen|z_sound_effect|restore_undo|z_output_stream|z_set_cursor|input_window|screen_erase_input|z_draw_picture|my_memmove|is_blank|z_get_wind_prop|handle_hot_key|z_tokenise|z_picture_data|next_property|z_dec_chk|show_cell_normal|z_test_attr|dumb_show_prompt|interpret|replay_char|record_write_input|translate_special_chars|z_random|script_mssg_on|memory_word|next_volume|script_new_line|object_name|hot_key_playback|replay_close|zoptind|string_type|z_pop_stack|first_undo|os_picture_data|z_set_true_colour|os_more_prompt|record_code|reset_memory|memory_new_line|script_word|cursor_row|z_loadb|z_set_font|z_store|Zwindow|z_storew|print_long|init_header|get_window_colours|unicode_to_zscii|os_quit|message|os_string_width|z_move_window|dumb_elide_more_prompt|show_pictures|get_default_name|z_piracy|read_yes_or_no|dumb_init_pictures|save_quetzal|menu_selected|colour_in_use|redirect|console_read_key|set_header_extension|will_print_blank|quiet_mode|input_type|dumb_blorb_stop|record_open|uint8_t|os_random_seed|prev_zmp|os_prepare_sample|hot_key_help|rv_blank_str|screen_changes";
#	my @poop = split(/\|/, $mysymbols);

#	$oldsymbols = "zoptopt|init_setup|print_string|ostream_record|countdown|z_throw|undo_struct|stream_new_line|enable_wrapping|zscii_to_latin1|z_show_status|script_open|reset_screen|stream_mssg_on|encoded|init_process|os_stop_sample|timeout|dumb_show_screen|z_set_window|z_get_parent|screen_mssg_off|memory_open|hide_lines|mem_diff|stream_word|os_start_sample|load_operand|show_cell_bbcode|z_buffer_mode|os_peek_colour|z_call_n|os_erase_area|default_name|get_window_font|z_set_attr|print_version|erase_window|dumb_init_input|split_window|os_storyfile_seek|z_check_arg_count|hot_key_undo|dumb_output_handle_setting|z_art_shift|init_memory|z_erase_window|script_write_input|get_max_width|os_set_cursor|time_ahead|screen_cells|read_number|z_print_unicode|mouse_y|dumb_discard_old_input|rv_mode|screen_data|myresource|z_rtrue|option_zcode_path|z_get_prop_addr|direct_call|read_long|os_process_arguments|$|os_init_screen|z_get_prop|z_restore|z_insert_obj|build_timestamp|os_finish_with_sample|xgetchar|mark_all_unchanged|prop_addr|z_print_addr|os_set_colour|winarg2|z_log_shift|os_read_line|z_erase_line|free_undo|blorb_err|runtime_usage|error_count|z_split_window|visual_bell|tokenise_line|os_get_text_style|dumb_read_misc_line|enable_scrolling|os_init_sound|show_line_prefix|f_setup_t|show_line_numbers|z_print_char|dumb_display_char|z_window_style|script_erase_input|current_bg|zputchar|font_height|z_inc_chk|os_display_string|object_address|update_cursor|dumb_row|my_strndup|z_print_form|set_window|z_rfalse|utf8_to_zchar|z_new_line|z_put_wind_prop|record_write_key|rv_names|script_char|user_text_height|unlink_object|memory_close|make_cell|z_scan_table|console_read_input|undo_count|format_t|init_err|hot_key_restart|counter|flush_buffer|load_all_operands|script_close|latin1_to_ascii|hot_key_seed|current_fg|story_size|z_restart|os_tick|z_loadw|z_print_ret|decoded|option_sound|z_make_menu|z_print_paddr|z_print_obj|z_header|os_check_unicode|latin1_to_ibm|z_verify|end_of_sound|winarg0|runtime_error|print_num|padding|os_char_width|z_set_colour|init_buffer|z_set_margins|z_encode_text|z_read_mouse|bbcode_colour|enable_scripting|round_div|__illegal__|interval|reset_cursor|z_mouse_window|mouse_x|need_newline_at_exit|stream_mssg_off|dest_row|screen_word|font_width|dumb_getline|init_undo|z_set_text_style|err_messages|dumb_set_picture_cell|z_header_t|amiga_screen_model|screen_new_line|lookup_text|routine|screen_mssg_on|z_input_stream|pad_status_line|z_remove_obj|resize_screen|dumb_read_line|mem_undiff|first_property|dumb_init_output|read_key_buffer|blorb_fp|units_left|z_get_cursor|os_display_char|start_next_sample|frotz_to_dumb|story_fp|start_sample|z_print|replay_read_input|screen_write_input|show_row|blorb_map|auxilary_name|f_setup|compression_names|pict_info|dumb_set_cell|tokenise_text|hot_key_recording|restart_header|read_word|z_storeb|__extended__|alphabet|z_get_prop_len|z_catch|validate_click|replay_read_key|my_strdup|show_cell|os_warn|script_width|script_mssg_off|z_copy_table|check_timeout|z_ret_popped|dumb_display_user_input|show_cell_irc|story_id|dumb_changes_row|input_redraw|z_num_to_index|completion|sampledata_t|ostream_screen|translate_to_zscii|is_terminator|z_picture_table|plain_ascii|os_repaint_window|istream_replay|load_string|z_clear_attr|z_call_s|discarding|z_put_prop|os_from_true_colour|record_close|blorb_res|hot_key_quit|z_get_sibling|specifier|encode_text|z_erase_picture|os_init_setup|more_prompts|uint32_t|replay_code|z_get_next_prop|playing|record_char|erase_screen|mouse_button|uint16_t|read_line_buffer|os_reset_screen|set_more_prompts|stream_char|ostream_memory|compression_mode|story_name|print_char|os_fatal|save_undo|replay_open|os_scroll_area|os_read_file_name|zgetopt|update_attributes|z_restore_undo|os_restart_game|z_read_char|reserve_mem|spurious_getchar|current_style|finished|z_print_table|dumb_dump_screen|show_cell_ansi|reset_window|print_object|os_draw_picture|num_pictures|read_string|truncate_question_mark|z_print_num|stream_read_key|show_line_types|translate_from_zscii|do_more_prompts|user_random_seed|enable_buffering|os_font_data|os_beep|seed_random|zoptarg|dumb_handle_setting|restore_quetzal|new_line|os_read_key|z_get_child|os_storyfile_tell|init_sound|next_sample|decode_text|z_push_stack|screen_char|z_window_size|ostream_script|hot_key_debugging|refresh_text_style|z_save_undo|user_text_width|dumb_copy_cell|os_set_font|stream_read_input|z_scroll_window|z_check_unicode|os_set_text_style|frame_count|restart_screen|z_sound_effect|restore_undo|z_output_stream|z_set_cursor|input_window|screen_erase_input|z_draw_picture|my_memmove|is_blank|z_get_wind_prop|handle_hot_key|z_tokenise|z_picture_data|next_property|z_dec_chk|show_cell_normal|z_test_attr|dumb_show_prompt|interpret|replay_char|record_write_input|translate_special_chars|z_random|script_mssg_on|memory_word|next_volume|script_new_line|object_name|hot_key_playback|replay_close|zoptind|string_type|z_pop_stack|first_undo|os_picture_data|z_set_true_colour|os_more_prompt|record_code|reset_memory|memory_new_line|script_word|cursor_row|z_loadb|z_set_font|z_store|Zwindow|z_storew|print_long|init_header|get_window_colours|unicode_to_zscii|os_quit|message|os_string_width|z_move_window|dumb_elide_more_prompt|show_pictures|get_default_name|z_piracy|read_yes_or_no|dumb_init_pictures|save_quetzal|menu_selected|colour_in_use|redirect|console_read_key|set_header_extension|will_print_blank|quiet_mode|input_type|dumb_blorb_stop|record_open|uint8_t|os_random_seed|prev_zmp|os_prepare_sample|hot_key_help|rv_blank_str|screen_changes";
#	my $stuff = scalar keys %transformations;

#	my $thingy = scalar @poop;
#	my $i;
#	print "POOP1: $thingy\n";
#	for ($i = $thingy; $i >= 60; $i--) {
#		print "$i\n";
#		shift @poop;
#	} 


my	$oldsymbols = join '|', @poop;
	chomp $oldsymbols;

	my $oldfilenames = join '|', keys %includes; 
	my @foo;
	my $filename;
	local $^I = '.bak'; 
	local @ARGV = glob("*.c *.h");
#	local @ARGV = glob("buffer.c");
	while (<>) {
		my $burp = $_;
		my @foo;
#		s/\b($oldsymbols)\b/$transformations{$1}/g;
#		s/($oldfilenames)/$1-POOP/g;

#		@foo = grep { $burp =~ m/\b$_\b/ } keys %transformations;
		@foo = grep { $burp =~ m/^.*$_.*$/ } keys %transformations;
		
#		print STDERR "FOO: $first_match\n";

		shift @foo;

		print STDERR "BURP: $burp";
		print STDERR "FOO: ";
		foreach my $i (@foo) {
			print STDERR "|$i| ";
		}
		print STDERR "\n--------\n";

# FIXME working here
		# I want this to execute ONLY IF $burp contains
		# something in $oldsymbols.  Instead $burp contains
		# every line in the file being examined.

#		if ($burp =~ /\b($oldsymbols)\b/) {
#		if ($burp =~ /\binit_buffer\b/) {
#		if ($burp =~ /\b(buffering|Frotz)\b/) {
#			chomp $burp;
#			print STDERR "BURP: $burp\n";
#			print STDERR "BURP: 1) $burp  2) $includes{$burp}\n";
#			s/\b($oldsymbols)\b/$transformations{$burp}/g;
#		}

		# This might work because $doot always contains
		# the entirety of what we're looking for.
#		if ($doot =~ /$oldfilenames/) {
#			chomp $doot;
#			print STDERR "DOOT: 1) $doot  2) $includes{$doot}\n";
#			s/$doot/$includes{$doot}/g;
#		}

		# Remove leads from includes
#		if (/^\s*#\s*include\s*\"/) {
#			@foo = split(/\"/,$_);
#			$filename = basename(@foo[1]);
#			@foo = split(/\./,$filename);
#			if ($filename_length) {
#				$filename = substr $foo[0], 0, $filename_length;
#			}
#			print "#include \"$filename.h\"\n";
#			next;
#		}
		print;
	}
#	print "$oldsymbols\n";
#	print "$oldfilenames\n";
}


# Maybe remove later.
#unlink glob("*.bak");
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

			# In headers, we need to fix up declarations too.
			# Our typedefs, enums, and structs happen to have short
			#   names, so don't bother.  AT
			# For the current Frotz codebase, we need to check
			# typedefs, enums, and structs too.  DG
			unless (/^extern\s/ or /^void\s/ or /^zchar\s/ or
				/^char\s/ or
				/^zbyte\s/ or /^int\s/ or /^bool\s/ or
				/^typedef\s/ or /^enum\s/ or /^struct\s/ or
				/^static\s/ or /^[}]\s/) {
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
