xmlparse

OVERVIEW
This program is used to parse an xml format page (such as a web page), search
for data, and extract fields of information.

USAGE
 xmlparse parse.cfg filename [arg0 ...]

PARSE.CFG FORMAT
  :label

	This identifies a label, which is used in loopuntileof

  searchfortag tag

	This skips through the input file for <tag>

  printuntiltag tag

	This prints out all of the text in the file (not in tags) until
	the specified tag is encountered.

  print string

	This prints the given string to the output

  searchforstring string

	This searches for the specified string

  printuntilstring string

	This prints out all text until the specified string is encountered

  loopuntileof label

	This jumps to the specified label, until end of file is encountered

  end

	This ends the parsing.

  $0-$9

	values of arg0-arg9 specified on the command line, which can be used
	in print statements.

  $.

	Space, which can be used in print statements

  $$

	$, which can be used in print statements

  $n

	newline, which can be used in print statements

SIMPLE EXAMPLE
parse.cfg
---------

searchfortag opentag
printuntiltag /closetag
print $n
end

filename
--------

this is a test file<opentag>tag data
more tag <ignoretag>tagged data</ignoretag>data on the next line
</closetag> more data
the final closing line

command line
------------

xmlparse parse.cfg filename

output
------

tag data more tag tagged data
