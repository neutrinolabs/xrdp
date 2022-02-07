
# detached brackets
--style=allman

# 4 spaces, no tabs
--indent=spaces=4

# for C++ files only
--indent-classes

# Indent 'switch' blocks so that the 'case X:' statements are indented in the switch block.
# The entire case block is indented.
--indent-switches

# Add extra indentation to namespace blocks. This option has no effect on Java files.
--indent-namespaces

# Converts tabs into spaces in the non-indentation part of the line.
--convert-tabs

# requires --convert-tabs to work properly
--indent-preprocessor

--indent-col1-comments

--min-conditional-indent=2

--max-instatement-indent=40

# Insert space padding around operators.
--pad-oper

# Insert space padding after paren headers only (e.g. 'if', 'for', 'while'...).
--pad-header


# Add brackets to unbracketed one line conditional statements (e.g. 'if', 'for', 'while'...).
--add-brackets

--align-pointer=name

# Do not retain a backup of the original file. The original file is purged after it is formatted.
--suffix=none

# For each directory in the command line, process all subdirectories recursively.
--recursive

# Exclude git submodule directories and generated files.
--exclude=libpainter
--exclude=librfxcodec
--exclude=xrdp_configure_options.h

# ignore errors from generated files that do not exist
--ignore-exclude-errors

# Preserve the original file's date and time modified.
--preserve-date

# Formatted files display mode. Display only the files that have been formatted.
# Do not display files that are unchanged.
--formatted

--lineend=linux
