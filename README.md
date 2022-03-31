# Almost Plaintext Configuration Format

## Contents

1. [Introduction](#introduction)
2. [Syntax](#syntax)
	1. [Keys](#key-syntax)
	2. [Values](#value-syntax)
	3. [Example](#apcf-example)
3. [Build And Install](#build-and-install)
	1. [Build](#build)
	2. [Install](#install)

# Introduction

APCF is a data format intended to be easily readable and modifiable,
and quick to use without *too* much boilerplate code.

This document focuses on describing the APCF syntax, and how to build
the library.

For clarity, syntax is formally described with POSIX extended regular
expressions (hereinafter "POSIX ERE"); any non-formal description of syntax
implies that every element may *optionally* be separated by an ASCII
space, a horizontal or vertical tab, a newline character,
or a carriage return character - with the only exception of values,
as described in their own section.

# Syntax

APCF data, by this library refered to as "config",
simply stores pairs of **keys** and **values**
called "entries" that may or may not be represented as hierarchies.

An entry is represented as a key, followed by the character '=', ending with
a value.

Entries that share an initial part of their key may be grouped together, where
the shared part is the name of a "group".  
A group is represented as a key, followed by the character '{', then a number
of entries with their keys **implicitly** beginning with the group name
followed by a period; all of this followed by the character '}'.

APCF data may contain comments, with a syntax equivalent to C++:
single-line comments begin with the sequence "`//`" and end at the end of the
line, while multi-line comments begin with the sequence "`/*`" and end with the
sequence "`*/`".  
Comments are completely ignored.

## Key Syntax

A key is a non-empty sequence of characters, separated by a period
(POSIX ERE "`([a-zA-Z0-9_-]+\.)*[a-zA-Z0-9_-]+`"); a key cannot contain
multiple consecutive periods.  
The smallest possible key has exactely one character, which may be a digit,
an underscore or a hyphen.

Examples of valid keys:

- `key`
- `key.subkey`
- `1`
- `1.2.3.four.5`

Examples of **in**valid keys:

- (empty)
- `1..2`
- `.1`
- `1.`
- `.`

## Value Syntax

A value may be a base 10 number representation, a boolean value,
a string or an array of values.

If the value is followed by a key, regardless of the semantics, the two
**must** be separated by one or more whitespaces: obviously, without this
rule the sequence "`123.subkey`" could ambiguously denote:

1. the numeric value 1, followed by the key "`23.subkey`";
2. the numeric value 12, followed by the key "`3.subkey`".

This also applies for values followed by other values, in the case of
arrays, for a very similar reason.

### Boolean Values

A boolean value may be any of "*true*", "*yes*", "*y*", "*false*", "*no*"
or "*n*" - their meaning is self-explainatory.

### Numeric Values

A numeric value is a base 10 sequence of digits, with an optional period
**between** two digits.

Note: sequences such as ".2" may or may not be properly
recognized by the library, but should **not** be relied upon.

### String Values

A string value is a pair of double quote characters (`"`), separated by any
combination of any type of character.

Non representable characters, non-ASCII sequences or even raw binary data
are allowed within a string.

Within a string, the backslash character (`\`) removes any special meaning from
the following character: this is only meaningful if followed by another
backslash or a double quote, and sequences such as "\n" do not have any special
meaning within the format.

Note: libraries may or may not correctly recognize the sequence " `1"string"` "
as a number followed by a string, but this should **not** be relied upon.

### Arrays Of Values

An array is a sequence of values within square brackets (`[]`); an array may
contain nested arrays.

Values are only separated by one or more whitespaces.  
Unlike strings, the array delimiters are strictly allowed not to be
immediately preceded or followed by a whitespace.

## APCF Example

``` apcf
key.subkey1=1
key.subkey2 = 2
 key.array_of_values =[1 2 3  4 ] // This is a comment

group.name = "\"group.name\" can be a group AND a whole key simultaneously"
group.name {
	1 = "n = \n"
	2 = "the value of \"group.name.1\" is a palindrome"
}

another-key.subkey = 7
another-key { subkey = 7 }

minimized{1=1 2=[3 4[1]]group{22.73=37.22}a=[]}
```

- No indentation dependent syntax, oh hi Python.
- As described by the value of "group.name", it is allowed and non-redundant
  to use the full key of an entry as the name of a group and viceversa;
  they do not overlap.
- The lines "`another-key.subkey = 7`" and
  "`another-key { subkey = 7 }`" are equivalent and perfectly redundant:
  when multiple entries have the same key, only the last one is to be
  considered.
- As shown by the last line, despite the name of this language,
  APCF can be pretty synthetic.  
  Or just pretty, it's up to you.

# Build And Install

As of 2022-03-27, this library has mainly been tested on x86-64 GNU/Linux
systems (compiled via GCC), and should probably work with Windows 10
(compiled via MSVC) with some hacking and fidgeting.

Instructions to build and install the library are only provided for
Linux based systems, although the codebase (aside from CMake bureaucracy)
is meant to be portable.

## Build

This repository includes a Bash script named "build.sh".  
In order to compile and link a release build, open a shell on the root
of the repository and run it as follows:

``` bash
dstpath=/tmp/apcf/Release ./build.sh Release
```

If your system does not have a few megabytes of internal memory to spare
for some reason, omit setting the "dstpath" variable in order to let
the script write temporary and final files in the current directory.

## Install

This repository includes a Bash script named "install.sh".

Except for a few regular expressions, the script is fairly simple to read;
packagers should do so, and set the variables according to their needs - 
especially so given that the script may have to run with administrative
privileges, being potentially dangerous.

In order to perform a system-wide "dumb install" to "/usr/local", however,
you can run the following Bash command:

``` bash
fakeroot=''  \
prefix_lib=usr/local/lib  \
prefix_include=usr/local/include  \
buildpath=/tmp/apcf/Release  \
./install.sh
```

The "buildpath" variable must have the same value used in the build process
for "dstpath".