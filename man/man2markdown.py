#!/usr/bin/env python

# man2markdown.py - A crappy man page to markdown converter
#
# Copyright (C) 2010  Denis Washington <dwashington@gmx.net>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import re
import string
import sys
import textwrap

class Man2Markdown (object):

    def __init__(self, input_file):
        self.__input = input_file
        self.__peeked_line = None
        self.__buffer = []
        self.__indent = ""
        self.__fill = True
        self.__line_width = 75

    def write(self, s):
        if not len(s.strip(" ")):
            return
        elif "\n" in s:
            lines = s.split("\n")
            for i in range(0, len(lines)):
                self.write(lines[i])
                if i < len(lines) - 1:
                    self.flush()
        else:
            self.__buffer.append(s)

    def flush(self):
        line = " ".join(self.__buffer)
        if self.__fill:
            line = textwrap.fill(line, self.__line_width,
                                 initial_indent = self.__indent,
                                 subsequent_indent = self.__indent)
        else:
            line = "    " + line

        sys.stdout.write(line)
        sys.stdout.write("\n")
        del self.__buffer[:]

    def run(self):
        while True:
            line = self.next_line()
            if line == None:
                break
            self.handle_line(line)
        self.flush()

    def next_line(self):
        if self.__peeked_line != None:
            line = self.__peeked_line
            self.__peeked_line = None
        else:
            line = self.__input.readline()
            if line:
                line = line.rstrip("\n")
            else:
                line = None
        return line

    def peek_line(self):
        if self.__peeked_line:
            return self.__peeked_line
        else:
            self.__peeked_line = self.__input.readline()
            return self.__peeked_line

    def handle_line(self, line):
        if line.startswith("."):
            words = line.split(" ")
            macro = words[0][1:]
            if macro == "\\\"":
                return

            args = []
            i = 1
            while i < len(words):
                if words[i].startswith('"'):
                    quoted = []
                    quoted.append(words[i])
                    if not words[i].endswith('"') or words[i] == '"':
                        i += 1
                        for j in range(i, len(words)):
                            quoted.append(words[j])
                            if quoted[-1].endswith('"'):
                                break
                            else:
                                i += 1
                    args.append((" ".join(quoted))[1:-1])
                    if not quoted[-1].endswith('"'):
                        break
                elif len(words[i]):
                    args.append(words[i])
                i += 1

            if self.__fill:
                handler_name = "handle_macro_{0}".format(macro)
            else:
                handler_name = "handle_macro_{0}_nf".format(macro)

            handler = Man2Markdown.__dict__.get(handler_name, None)
            if handler:
                handler(self, args)
            else:
                if self.__fill:
                    sep = " "
                else:
                    sep = ""
                self.handle_simple_line(sep.join(args))
        else:
            self.handle_simple_line(line)

    def handle_simple_line(self, line):
        if self.__fill:
            for word in line.split():
                self.write(word)
        else:
            self.write("{0}\n".format(line))

    def handle_macro_B(self, args):
        """
        .B
        """
        self.write("**{0}**".format(" ".join(args)))

    def handle_macro_B_nf(self, args):
        """
        .B (no-fill)
        """
        self.write("{0}\n".format(" ".join(args)))

    def handle_macro_BI(self, args):
        """
        .BI
        """
        text = []
        for i in range(0, len(args)):
            if i % 2:
                text.append("_{0}_".format(args[i]))
            else:
                text.append("**{0}**".format(args[i]))
        self.write("".join(text))

    def handle_macro_BR(self, args):
        """
        .BR
        """
        text = []
        for i in range(0, len(args)):
            if i % 2:
                text.append(args[i])
            else:
                text.append("**{0}**".format(args[i]))
        self.write("".join(text))

    def handle_macro_br(self, args):
        """
        .br
        """
        if self.__fill:
            self.write("\n")

    def handle_macro_br_nf(self, args):
        """
        .br (no-fill)
        """
        pass

    def handle_macro_fi_nf(self, args):
        """
        .fi (no-fill)
        """
        self.__fill = True

    def handle_macro_I(self, args):
        """
        .I
        """
        self.write("_{0}_".format(" ".join(args)))

    def handle_macro_in(self, args):
        """
        .in
        """
        pass

    def handle_macro_IP(self, args):
        """
        .IP
        """
        if len(args) < 1:
            return
        self.write("\n")
        self.__indent = ""
        if args[0] == "\\(bu" or args[0] == "\\(em":
            self.write("\n*\n")
            self.__indent = "    "
        else:
            match = re.match(r"[0-9]+\.", args[0])
            if match and match.endpos == len(args[0]):
                self.write("\n{0}\n".format(args[0])) 
                self.__indent = "    "

    def handle_macro_IR(self, args):
        """
        .IR
        """
        text = []
        for i in range(0, len(args)):
            if i % 2:
                text.append(args[i])
            else:
                text.append("_{0}_".format(args[i]))
        self.write("".join(text))

    def handle_macro_nf(self, args):
        """
        .nf
        """
        self.write("\n")
        self.__fill = False

    def handle_macro_PP(self, args):
        """
        .PP
        """
        self.write("\n\n")
        self.__indent = ""

    def handle_macro_SH(self, args):
        """
        .SH
        """
        self.write("\n")
        self.__indent = ""
        self.write("\n## {0}\n\n".format(" ".join(args)))

    def handle_macro_SS(self, args):
        """
        .SH
        """
        self.write("\n")
        self.__indent = ""
        self.write("\n### {0}\n\n".format(" ".join(args)))

    def handle_macro_sp(self, args):
        """
        .sp
        """
        self.write("\n\n")

    def handle_macro_sp_nf(self, args):
        """
        .sp (no-fill)
        """
        self.write("\n")

    def handle_macro_TH(self, args):
        """
        .TH
        """
        pass

    def handle_macro_TP(self, args):
        """
        .TP
        """
        first_line = self.next_line()
        if not first_line:
            return
        self.write("\n")
        self.__indent = ""
        self.write("\n*  ")
        self.handle_line(first_line)
        self.write("\n")
        self.__indent = "    "

if __name__ == "__main__":
    input_file = file(sys.argv[1])
    man2md = Man2Markdown(input_file)
    man2md.run()
