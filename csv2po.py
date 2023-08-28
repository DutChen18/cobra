#!/usr/bin/env python3
import csv
import sys
import ast
import json

reader = csv.DictReader(sys.stdin)
locales = { l: {} for l in reader.fieldnames if l.startswith("locale/") }

for row in reader:
    for key, value in row.items():
        if not key.startswith("locale/"):
            continue

        locales[key][row["locale/en_US.po"]] = value

for file, strings in locales.items():
    result = ""

    with open(file) as f:
        for line in f:
            if line.startswith("msgid "):
                result += line
                msgid = ast.literal_eval(line[6:])
            elif line.startswith("msgstr "):
                if msgid != "" and msgid in strings:
                    msgstr = strings[msgid].replace("\"", "\\\"")
                    result += f"msgstr \"{msgstr}\"\n"
                else:
                    result += line

                    if msgid != "":
                        print(f"missing {file} translation for {msgid!r}")
            elif line.startswith("\""):
                if msgid == "" or msgid not in strings:
                    result += line
            else:
                result += line

    with open(file, "w") as f:
        f.write(result)
