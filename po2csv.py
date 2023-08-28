#!/usr/bin/env python3
import sys
import ast
import csv

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

            if line.startswith("\""):
                text[msgid][file] += ast.literal_eval(line)

writer = csv.DictWriter(sys.stdout, locales)
writer.writeheader()
writer.writerows(text.values())
