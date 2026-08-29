#!/usr/bin/env python3
"""Generate the Shark Exynos8895 C catalog fragment from debugfs all_dump."""

from __future__ import print_function

import argparse
import hashlib
import re
import sys


UINT_MAX = (1 << 32) - 1


def field(line, label):
    prefix = "[{}] : ".format(label)
    line = line.strip()
    if line.startswith(prefix):
        return line[len(prefix):].strip()
    return None


def number(value, base=10):
    if value == "NONE":
        return UINT_MAX
    return int(value, base)


def section_ranges(lines):
    starts = []
    for idx, line in enumerate(lines):
        if line.startswith("[ECT] : "):
            starts.append((line[len("[ECT] : "):].strip(), idx))
    result = {}
    for pos, (name, start) in enumerate(starts):
        end = starts[pos + 1][1] if pos + 1 < len(starts) else len(lines)
        result[name] = lines[start + 1:end]
    return result


def collect_rows(lines, table_index, rows, base=10):
    result = []
    idx = table_index + 1
    while idx < len(lines) and len(result) < rows:
        text = lines[idx].strip()
        idx += 1
        if not text:
            continue
        if text.startswith("["):
            raise ValueError("table ended after {} of {} rows at {}".format(
                len(result), rows, text))
        result.append([int(token, base) for token in text.split()])
    if len(result) != rows:
        raise ValueError("expected {} rows, got {}".format(rows, len(result)))
    return result, idx


def parse_pll(lines):
    plls = []
    idx = 0
    while idx < len(lines):
        name = field(lines[idx], "PLL NAME")
        if name is None:
            idx += 1
            continue
        pll = {"name": name, "type": None, "count": None, "rows": []}
        idx += 1
        while idx < len(lines) and field(lines[idx], "PLL NAME") is None:
            value = field(lines[idx], "PLL TYPE")
            if value is not None:
                pll["type"] = number(value)
            value = field(lines[idx], "NUM OF FREQUENCY")
            if value is not None:
                pll["count"] = number(value)
            value = field(lines[idx], "FREQUENCY")
            if value is not None:
                row = [number(value)]
                for label in ("P", "M", "S", "K"):
                    idx += 1
                    item = field(lines[idx], label)
                    if item is None:
                        raise ValueError("{}: missing {}".format(name, label))
                    row.append(number(item))
                pll["rows"].append(row)
            idx += 1
        if pll["type"] is None or pll["count"] != len(pll["rows"]):
            raise ValueError("invalid PLL {}".format(name))
        plls.append(pll)
    return plls


def parse_dvfs(lines):
    domains = []
    starts = [idx for idx, line in enumerate(lines)
              if field(line, "DOMAIN NAME") is not None]
    for pos, start in enumerate(starts):
        end = starts[pos + 1] if pos + 1 < len(starts) else len(lines)
        chunk = lines[start:end]
        dom = {
            "name": field(chunk[0], "DOMAIN NAME"),
            "boot": UINT_MAX, "resume": UINT_MAX,
            "max": UINT_MAX, "min": UINT_MAX,
            "sfr": [], "levels": [], "enabled": [], "params": [],
        }
        table_idx = None
        num_level = None
        num_clock = None
        for idx, line in enumerate(chunk):
            for key, label in (("boot", "BOOT LEVEL IDX"),
                               ("resume", "RESUME LEVEL IDX"),
                               ("max", "MAX FREQ"), ("min", "MIN FREQ")):
                value = field(line, label)
                if value is not None:
                    dom[key] = number(value)
            value = field(line, "NUM OF SFR")
            if value is not None:
                num_clock = number(value)
            value = field(line, "SFR ADDRESS")
            if value is not None:
                dom["sfr"].append(number(value, 16))
            value = field(line, "NUM OF LEVEL")
            if value is not None:
                num_level = number(value)
            value = field(line, "LEVEL")
            if value is not None:
                match = re.fullmatch(r"(\d+)\(([OX])\)", value)
                if not match:
                    raise ValueError("{}: malformed level {}".format(dom["name"], value))
                dom["levels"].append(int(match.group(1)))
                dom["enabled"].append(1 if match.group(2) == "O" else 0)
            if line.strip() == "[TABLE]":
                table_idx = idx
        if num_level is None or num_clock is None or table_idx is None:
            raise ValueError("{}: incomplete DVFS domain".format(dom["name"]))
        dom["params"], _ = collect_rows(chunk, table_idx, num_level)
        if len(dom["sfr"]) != num_clock or len(dom["levels"]) != num_level:
            raise ValueError("{}: inconsistent DVFS shape".format(dom["name"]))
        if any(len(row) != num_clock for row in dom["params"]):
            raise ValueError("{}: inconsistent DVFS parameter row".format(dom["name"]))
        domains.append(dom)
    return domains


def parse_asv(lines):
    domains = []
    starts = [idx for idx, line in enumerate(lines)
              if field(line, "DOMAIN NAME") is not None]
    for pos, start in enumerate(starts):
        end = starts[pos + 1] if pos + 1 < len(starts) else len(lines)
        chunk = lines[start:end]
        dom = {"name": field(chunk[0], "DOMAIN NAME"), "groups": None,
               "levels": None, "freqs": [], "tables": []}
        idx = 1
        while idx < len(chunk):
            value = field(chunk[idx], "NUM OF ASV GROUP")
            if value is not None:
                dom["groups"] = number(value)
            value = field(chunk[idx], "NUM OF LEVEL")
            if value is not None:
                dom["levels"] = number(value)
            value = field(chunk[idx], "FREQUENCY")
            if value is not None:
                dom["freqs"].append(number(value))
            value = field(chunk[idx], "TABLE VERSION")
            if value is not None:
                table = {"version": number(value), "boot": UINT_MAX,
                         "resume": UINT_MAX, "rows": []}
                idx += 1
                while idx < len(chunk):
                    boot = field(chunk[idx], "BOOT LEVEL IDX")
                    if boot is not None:
                        table["boot"] = number(boot)
                    resume = field(chunk[idx], "RESUME LEVEL IDX")
                    if resume is not None:
                        table["resume"] = number(resume)
                    if chunk[idx].strip() == "[TABLE]":
                        table["rows"], idx = collect_rows(
                            chunk, idx, dom["levels"])
                        break
                    idx += 1
                dom["tables"].append(table)
                continue
            idx += 1
        if dom["levels"] != len(dom["freqs"]):
            raise ValueError("{}: inconsistent ASV frequencies".format(dom["name"]))
        for table in dom["tables"]:
            if any(len(row) != dom["groups"] for row in table["rows"]):
                raise ValueError("{} v{}: inconsistent ASV row".format(
                    dom["name"], table["version"]))
            for row in table["rows"]:
                for voltage in row:
                    if voltage and voltage % 6250:
                        raise ValueError("{} v{}: voltage {} is off-grid".format(
                            dom["name"], table["version"], voltage))
        domains.append(dom)
    return domains


def parse_named_tables(lines, name_label, cols_label, rows_label, base=10):
    result = []
    idx = 0
    while idx < len(lines):
        name = field(lines[idx], name_label)
        if name is None:
            idx += 1
            continue
        table = {"name": name, "cols": None, "rows_count": None, "rows": []}
        idx += 1
        while idx < len(lines) and field(lines[idx], name_label) is None:
            value = field(lines[idx], cols_label)
            if value is not None:
                table["cols"] = number(value)
            value = field(lines[idx], rows_label)
            if value is not None:
                table["rows_count"] = number(value)
            if lines[idx].strip() == "[TABLE]":
                table["rows"], idx = collect_rows(
                    lines, idx, table["rows_count"], base)
                break
            idx += 1
        if any(len(row) != table["cols"] for row in table["rows"]):
            raise ValueError("{}: inconsistent table row".format(name))
        result.append(table)
    return result


def parse_newtime(lines):
    tables = parse_named_tables(lines, "PARAMETER KEY",
                                "NUM OF TIMING PARAMETER", "NUM OF LEVEL", 16)
    for table in tables:
        table["mode"] = 2 if any(value > UINT_MAX for row in table["rows"]
                                      for value in row) else 1
    return tables


def parse_thermal(lines):
    result = []
    starts = [idx for idx, line in enumerate(lines)
              if field(line, "FUNCTION NAME") is not None]
    labels = ("LOWER BOUND TEMPERATURE", "UPPER BOUND TEMPERATURE",
              "MAX FREQUENCY", "SW TRIP", "FLAG")
    for pos, start in enumerate(starts):
        end = starts[pos + 1] if pos + 1 < len(starts) else len(lines)
        chunk = lines[start:end]
        item = {"name": field(chunk[0], "FUNCTION NAME"), "count": None,
                "rows": []}
        current = []
        for line in chunk[1:]:
            value = field(line, "NUM OF RANGE")
            if value is not None:
                item["count"] = number(value)
            for label in labels:
                value = field(line, label)
                if value is not None:
                    current.append(number(value))
                    if len(current) == len(labels):
                        item["rows"].append(current)
                        current = []
        if current or item["count"] != len(item["rows"]):
            raise ValueError("{}: inconsistent thermal rows".format(item["name"]))
        result.append(item)
    return result


def parse_minlock(lines):
    result = []
    starts = [idx for idx, line in enumerate(lines)
              if field(line, "DOMAIN NAME") is not None]
    pattern = re.compile(r"\[Frequency\] : \(MAIN\)(\d+), \(SUB\)(\d+)")
    for pos, start in enumerate(starts):
        end = starts[pos + 1] if pos + 1 < len(starts) else len(lines)
        item = {"name": field(lines[start], "DOMAIN NAME"), "rows": []}
        for line in lines[start + 1:end]:
            match = pattern.fullmatch(line.strip())
            if match:
                item["rows"].append([int(match.group(1)), int(match.group(2))])
        result.append(item)
    return result


def ident(*parts):
    text = "_".join(parts).lower()
    text = re.sub(r"[^a-z0-9]+", "_", text).strip("_")
    if text and text[0].isdigit():
        text = "n_" + text
    return "shark_soc_cat_" + text


def flatten(rows):
    return [value for row in rows for value in row]


def add_table(tables, block, name, subname, kind, rows, cols,
              aux0=0, aux1=0, data=None):
    if data is None:
        data = []
    if len(data) != rows * cols:
        raise ValueError("{}:{}:{} shape {}x{} != {} values".format(
            block, name, subname, rows, cols, len(data)))
    array_name = ident(block, name, subname, kind, str(len(tables)))
    tables.append({"block": block, "name": name, "subname": subname,
                   "kind": kind, "rows": rows, "cols": cols,
                   "aux0": aux0, "aux1": aux1, "data": data,
                   "array": array_name})


def build_catalog(parsed):
    tables = []
    for pll in parsed["pll"]:
        add_table(tables, "PLL", pll["name"], "rates", "PLL_RATES",
                  len(pll["rows"]), 5, pll["type"], 0, flatten(pll["rows"]))
    for dom in parsed["dvfs"]:
        add_table(tables, "DVFS", dom["name"], "levels", "DVFS_LEVELS",
                  1, len(dom["levels"]), dom["boot"], dom["resume"], dom["levels"])
        add_table(tables, "DVFS", dom["name"], "enabled", "DVFS_ENABLED",
                  1, len(dom["enabled"]), dom["max"], dom["min"], dom["enabled"])
        add_table(tables, "DVFS", dom["name"], "sfr", "DVFS_SFR",
                  1, len(dom["sfr"]), len(dom["sfr"]), 0, dom["sfr"])
        add_table(tables, "DVFS", dom["name"], "params", "DVFS_PARAMS",
                  len(dom["params"]), len(dom["sfr"]), 0, 0,
                  flatten(dom["params"]))
    for dom in parsed["asv"]:
        add_table(tables, "ASV", dom["name"], "frequencies", "ASV_FREQS",
                  1, len(dom["freqs"]), dom["groups"], len(dom["tables"]), dom["freqs"])
        for table in dom["tables"]:
            add_table(tables, "ASV", dom["name"],
                      "table_v{}".format(table["version"]), "ASV_TABLE",
                      len(table["rows"]), dom["groups"],
                      table["boot"], table["resume"], flatten(table["rows"]))
    for table in parsed["gen"]:
        add_table(tables, "GEN", table["name"], "parameters", "GEN_TABLE",
                  table["rows_count"], table["cols"], 0, 0,
                  flatten(table["rows"]))
    for table in parsed["newtime"]:
        add_table(tables, "NEWTIME", table["name"], "parameters", "NEWTIME_TABLE",
                  table["rows_count"], table["cols"], table["mode"], 0,
                  flatten(table["rows"]))
    for item in parsed["thermal"]:
        add_table(tables, "THERMAL", item["name"], "ranges", "THERMAL_RANGES",
                  len(item["rows"]), 5, 0, 0, flatten(item["rows"]))
    for item in parsed["minlock"]:
        add_table(tables, "MINLOCK", item["name"], "levels", "MINLOCK_TABLE",
                  len(item["rows"]), 2, 0, 0, flatten(item["rows"]))
    return tables


def c_string(value):
    return '"{}"'.format(value.replace("\\", "\\\\").replace('"', '\\"'))


def emit_array(out, table):
    print("static const u64 {}[] = {{".format(table["array"]), file=out)
    line = "\t"
    for value in table["data"]:
        token = "{}ULL,".format(value)
        if len(line) + len(token) + 1 > 100:
            print(line.rstrip(), file=out)
            line = "\t"
        line += token + " "
    if line.strip():
        print(line.rstrip(), file=out)
    print("};\n", file=out)


def emit_catalog(out, tables, digest):
    print("/* Generated by scripts/shark/ect_dump_to_c.py for this translation unit. */", file=out)
    print("#define SHARK_SOC_DUMP_SHA256 {}\n".format(c_string(digest)), file=out)
    for table in tables:
        emit_array(out, table)
    print("static const struct shark_soc_catalog_table shark_soc_static_catalog[] = {", file=out)
    for table in tables:
        print("\t{{ {}, {}, {}, SHARK_SOC_CATALOG_{}, {}U, {}U, {}U, {}U, {} }},".format(
            c_string(table["block"]), c_string(table["name"]),
            c_string(table["subname"]), table["kind"], table["rows"],
            table["cols"], table["aux0"], table["aux1"], table["array"]), file=out)
    print("};\n", file=out)
    print("#define SHARK_SOC_STATIC_CATALOG_COUNT ARRAY_SIZE(shark_soc_static_catalog)\n", file=out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dump")
    parser.add_argument("-o", "--output")
    args = parser.parse_args()
    raw = open(args.dump, "rb").read()
    lines = raw.decode("ascii").splitlines()
    sections = section_ranges(lines)
    required = ["PLL Information", "DVFS Information", "ASV Voltage Information",
                "General-Parameter Information", "New Timing-Parameter Information",
                "AP Thermal Information", "Minlock Information"]
    missing = [name for name in required if name not in sections]
    if missing:
        raise ValueError("missing sections: {}".format(", ".join(missing)))
    parsed = {
        "pll": parse_pll(sections["PLL Information"]),
        "dvfs": parse_dvfs(sections["DVFS Information"]),
        "asv": parse_asv(sections["ASV Voltage Information"]),
        "gen": parse_named_tables(sections["General-Parameter Information"],
                                  "TABLE NAME", "NUM OF COLUMN", "NUM OF ROW"),
        "newtime": parse_newtime(sections["New Timing-Parameter Information"]),
        "thermal": parse_thermal(sections["AP Thermal Information"]),
        "minlock": parse_minlock(sections["Minlock Information"]),
    }
    expected = {"pll": 5, "dvfs": 10, "asv": 10, "gen": 20,
                "newtime": 35, "thermal": 2, "minlock": 3}
    for key, count in expected.items():
        if len(parsed[key]) != count:
            raise ValueError("{} count {} != {}".format(key, len(parsed[key]), count))
    dvfs = {item["name"]: item for item in parsed["dvfs"]}
    for item in parsed["asv"]:
        peer = dvfs.get(item["name"])
        if peer and peer["levels"] != [freq * 1000 for freq in item["freqs"]]:
            raise ValueError("DVFS/ASV frequency mismatch for {}".format(item["name"]))
    tables = build_catalog(parsed)
    destination = open(args.output, "w") if args.output else sys.stdout
    try:
        emit_catalog(destination, tables, hashlib.sha256(raw).hexdigest())
    finally:
        if args.output:
            destination.close()
    print("generated {} catalog tables".format(len(tables)), file=sys.stderr)


if __name__ == "__main__":
    main()
