#!/usr/bin/env python3
import sys
import ast

locales = sys.argv[1:]
text = {}

for file in locales:
    with open(file) as f:
        for line in f:
            if line.startswith("msgid "):
                msgid = ast.literal_eval(line[6:])

                if msgid not in text:
                    text[msgid] = {}

            if line.startswith("msgstr "):
                msgstr = ast.literal_eval(line[7:])
                text[msgid][file] = msgstr

print(",".join(locales))

for key, value in text:
    print(",".join(repr(value.get())))
