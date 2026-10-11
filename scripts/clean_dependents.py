#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026-present ROCKNIX (https://github.com/ROCKNIX)

# Build stamps only cover a package's own recipe. Print the packages of a build
# plan whose dependencies changed what they install since they were built, or
# that link a library no package of the image provides.

import argparse
import concurrent.futures
import glob
import hashlib
import json
import os
import re
import subprocess
import sys

IFACE_DIRS = ("usr/include/", "usr/share/aclocal/", "usr/share/pkgconfig/", "usr/share/wayland-protocols/")
IFACE_FILES = (".a", ".la", ".o", ".cmake", ".pc")
VERSION_FILES = ("ConfigVersion.cmake", "-config-version.cmake")


def digest(lines):
    return hashlib.sha256("\n".join(sorted(lines)).encode()).hexdigest()


def file_digest(path, skip=None):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for line in f:
            if not (skip and line.startswith(skip)):
                h.update(line)
    return h.hexdigest()


def is_lib(name):
    return name.endswith(".so") or ".so." in name


def lib_stem(name):
    return name[:name.find(".so") + 3] if ".so" in name else name


def elf_machine(path):
    try:
        with open(path, "rb") as f:
            head = f.read(20)
    except OSError:
        return None
    if len(head) < 20 or head[:4] != b"\x7fELF":
        return None
    return int.from_bytes(head[18:20], "little" if head[5] == 1 else "big")


def dynamic(readelf, path):
    out = subprocess.run([readelf, "-d", path], capture_output=True, text=True).stdout
    soname = re.search(r"\(SONAME\).*\[(.*)\]", out)
    return (soname.group(1) if soname else None), re.findall(r"\(NEEDED\).*\[(.*)\]", out)


# The interface is what dependents build against: headers, pkg-config, cmake
# and libtool files, static libraries, and shared libraries by soname, without
# version-only metadata. Also returns the libraries the package provides and
# the ones its binaries for the target machine need.
def scan(root, readelf, machine, pool):
    entries, provides, elves = [], set(), []
    for dirpath, dirnames, filenames in os.walk(root):
        for name in dirnames + filenames:
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, root)
            if os.path.islink(path):
                if is_lib(name):
                    provides.add(name)
                if rel.startswith(IFACE_DIRS):
                    entries.append(f"L {rel} {os.readlink(path)}")
                continue
            if name not in filenames or name.endswith(VERSION_FILES):
                continue
            m = elf_machine(path) if is_lib(name) or os.access(path, os.X_OK) else None
            if m is not None:
                elves.append((rel, m))
            if is_lib(name):
                provides.add(name)
                if m is None and rel.startswith("usr/lib/"):
                    entries.append(f"F {rel} {file_digest(path)}")
            elif name.endswith(".pc"):
                entries.append(f"F {rel} {file_digest(path, b'Version:')}")
            elif rel.startswith(IFACE_DIRS) or name.endswith(IFACE_FILES):
                entries.append(f"F {rel} {file_digest(path)}")

    needs = set()
    results = pool.map(lambda e: dynamic(readelf, os.path.join(root, e[0])), elves)
    for (rel, m), (soname, needed) in zip(elves, results):
        if soname:
            provides.add(soname)
        if is_lib(os.path.basename(rel)) and rel.startswith("usr/lib/"):
            entries.append(f"S {os.path.dirname(rel)}/{soname or os.path.basename(rel)}")
        if m == machine:
            needs.update(needed)
    return digest(entries), sorted(provides), sorted(needs)


def install_dir(build, name, target):
    found = []
    for d in glob.glob(os.path.join(build, "install_init" if target == "init" else "install_pkg", glob.escape(name) + "-*")):
        try:
            with open(os.path.join(d, ".rocknix-package")) as f:
                if f.read().strip() == f'INFO_PKG_NAME="{name}"':
                    found.append(d)
        except OSError:
            pass
    return max(found, key=os.path.getmtime) if found else None


# A rebuild with an unchanged recipe writes the same stamp, so its time tells builds apart.
def read_stamp(path):
    try:
        with open(path, "rb") as f:
            data = f.read()
            mtime = os.fstat(f.fileno()).st_mtime_ns
    except OSError:
        return None, None
    deephash = re.search(rb'^STAMP_PKG_DEEPHASH="(.*)"$', data, re.M)
    return f"{hashlib.sha256(data).hexdigest()} {mtime}", deephash.group(1).decode() if deephash else ""


parser = argparse.ArgumentParser(description="Print the packages to rebuild because of changed dependencies.")
parser.add_argument("--plan", required=True, help="build plan (plan.json)")
parser.add_argument("--build", required=True, help="build directory")
parser.add_argument("--stamps", required=True, help="build stamps directory")
parser.add_argument("--readelf", default="readelf")
parser.add_argument("--machine", type=int, required=True, help="ELF e_machine of the target")
parser.add_argument("--compiler", default="", help="target compiler version")
parser.add_argument("--state", help="state file (default: <build>/.rocknix-dependents.json)")
args = parser.parse_args()

state_file = args.state or os.path.join(args.build, ".rocknix-dependents.json")
try:
    with open(state_file) as f:
        state = json.load(f)
except (OSError, ValueError):
    state = {}

with open(args.plan) as f:
    plan = json.load(f)

iface = {}
built = set()
scanned = set()
stale = {}

with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
    # the plan lists dependencies before their dependents
    for job in plan:
        node = job["name"]
        name, _, target = node.partition(":")
        deps = [f"{d} {iface[d]}" for d in job["wants"] if iface.get(d)]

        if job["section"] == "virtual":
            iface[node] = digest(deps) if deps else ""
            continue

        # host tools do not change what target packages link; the compiler is tracked by version
        if target not in ("target", "init"):
            continue

        deps.append(f"compiler {args.compiler}")
        deps_digest = digest(deps)
        stamp, deephash = read_stamp(os.path.join(args.stamps, name, f"build_{target}"))
        prev = state.get(node)

        if stamp is None:
            iface[node] = prev["iface"] if prev else ""
            continue

        built.add(node)
        if prev and prev["stamp"] == stamp:
            iface[node] = prev["iface"]
            if prev["deps"] != deps_digest:
                stale[name] = "dependencies changed"
        else:
            # built since the last check, so against the dependencies there are now
            d = install_dir(args.build, name, target)
            if d:
                iface[node], provides, needs = scan(d, args.readelf, args.machine, pool)
            else:
                iface[node], provides, needs = deephash, [], []
            state[node] = {"stamp": stamp, "iface": iface[node], "deps": deps_digest,
                           "provides": provides, "needs": needs}
            scanned.add(node)

# Only an image plan holds every package that provides libraries. The first
# check rebuilds packages linked to another version of a library the image has;
# after that a package is rebuilt when it lost a library. A library still
# missing right after a rebuild, or not provided at all, is only reported.
warnings = []
if any(job["task"] == "install" for job in plan):
    first_check = not state.get("linkage-checked")
    image = [job["name"] for job in plan if job["name"].endswith(":target") and job["name"] in state]
    provided = set().union(*(state[node].get("provides", []) for node in image))
    provided_stems = {lib_stem(lib) for lib in provided}
    for node in image:
        if node not in built:
            continue
        name = node.partition(":")[0]
        entry = state[node]
        missing = sorted(set(entry.get("needs", [])) - provided)
        if first_check:
            accepted = [lib for lib in missing if lib_stem(lib) not in provided_stems]
        elif node in scanned or "missing" not in entry:
            accepted = missing
        else:
            accepted = entry["missing"]
        lost = sorted(set(missing) - set(accepted))
        if lost:
            stale[name] = "needs " + " ".join(lost)
        elif missing and (first_check or node in scanned):
            warnings.append(f"{name} needs {' '.join(missing)}, not provided by any package of the image")
        entry["missing"] = accepted
    state["linkage-checked"] = True

with open(state_file + ".tmp", "w") as f:
    json.dump(state, f, indent=1, sort_keys=True)
os.replace(state_file + ".tmp", state_file)

for warning in sorted(warnings):
    print(f"WARNING: {warning}", file=sys.stderr)
for name in sorted(stale):
    print(f"{name}: {stale[name]}", file=sys.stderr)
    print(name)
