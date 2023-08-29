#!/usr/bin/env python3
import sys
import csv
import os

reader = csv.DictReader(sys.stdin)
locales = { l: {} for l in reader.fieldnames if l.startswith("locale/") }

for row in reader:
    for key, value in row.items():
        if not key.startswith("locale/"):
            continue

        locales[key][row["locale/en_US.po"]] = value

print("#include \"cobra/locale.hh\"")
print("")
print("std::unordered_map<std::string, std::unordered_map<std::string, std::string>> cobra::locale = {")

def escape(s):
    s = s.replace("\\", "\\\\")
    s = s.replace("\"", "\\\"")
    return f"\"{s}\""

for file, strings in locales.items():
    lang = os.path.splitext(os.path.basename(file))[0] + ".UTF-8"

    print(f"\t{{ {escape(lang)}, {{")

    for key, value in strings.items():
        print(f"\t\t{{ {escape(key)}, {escape(value)} }},")

    print("\t} },")

print("};")
