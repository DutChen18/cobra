#!/usr/bin/env python3
import csv
import sys

reader = csv.DictReader(sys.stdin)
locales = { l: {} for l in reader.fieldnames }

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

                if msgid not in text:
                    text[msgid] = {}
            elif line.startswith("msgstr "):
                if msgid in strings:
                    result += f"msgstr {strings[msgid]!r}\n"
                else:
                    msgstr = ast.literal_eval(line[7:])
                    result += f"msgstr {msgstr!r}\n"
                    print(f"missing {file} translation for {msgid}")
            else:
                result += line

    with open(file, "w") as f:
        f.write(result)
