#!/usr/bin/env python3
"""Compare ScreenScraper lookup methods on a sample of the games on the MiSTer (an experiment).

    python3 -I tools/screenscraper-compare.py [--per-system 5] [--systems snes,GBA] [--no-hash]
                                              [--max-minutes 25] [--host root@192.168.64.191]

For each system with games it takes N games spread evenly over the system's game list and looks
every one up three ways:

  H   by ROM hash   jeuInfos.php with the system, file name, size and crc/md5/sha1 of the ROM
                    (a zip is opened and its single file is hashed; if that finds nothing, the hash
                    of the zip itself is tried as well, to learn which one ScreenScraper knows)
  T1  by title      jeuRecherche.php with the title and the system
  T2  by title      jeuRecherche.php with the title only, no system

and records whether a game was found, which one, and whether it has a video. The ROM files are only
read over SSH to compute hashes; nothing is stored, copied or uploaded except the hash, name and size.
Disc images and files over 16 MB are not hashed.

Care taken with the shared service: one request at a time, at least 1.3 s apart, and the run stops at
once on any limit or "closed" answer (429, 430, 431, 401, 423, 426). Credentials come from
`.screenscraper.fr.env`, go to curl on standard input and are never printed or saved.
Results: _vnext/files/api/compare-results.jsonl (one line per game) and a summary on the terminal.
"""
import argparse
import hashlib
import importlib.util
import io
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
import time
import unicodedata
import zipfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
spec = importlib.util.spec_from_file_location("probe", os.path.join(ROOT, "tools", "screenscraper-probe.py"))
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)

MIN_SPACING = 1.3
HARD_STOPS = {429, 430, 431, 401, 423, 426}
MAX_HASH_BYTES = 16 * 1024 * 1024
DISC_SYSTEMS = {"PSX", "Saturn", "MegaCD", "PCE-CD", "NeoGeo-CD", "Jaguar-CD"}

# MiSTer system key -> names ScreenScraper uses (RetroPie / Recalbox naming from systemesListe).
SYSTEM_NAMES = {
    "snes": "snes", "NES": "nes", "MegaDrive": "megadrive", "SMS": "mastersystem",
    "GAMEBOY": "gb", "GBA": "gba", "GameGear": "gamegear", "N64": "n64", "PSX": "psx",
    "Saturn": "saturn", "MegaCD": "segacd", "Atari2600": "atari2600", "Jaguar": "atarijaguar",
    "AtariLynx": "atarilynx", "Astrocade": "astrocade", "C128": "c128", "Arcade": "arcade",
    "MACPLUS": "macintosh", "TI-99_4A": "ti99", "PC8801": "pc88", "FM-7": "fm7", "QL": "ql",
    "TRS-80": "trs-80", "CoCo2": "coco", "COCO3": "coco", "Apple-IIgs": "apple2gs",
    "AcornAtom": "atom", "Altair8800": "altair8800", "PET2001": "pet", "Studio-II": "studio2",
}


class Stop(Exception):
    pass


def ssh(host, *args, binary=False):
    # ssh joins its arguments for the remote shell, so every one is quoted (names have spaces and brackets).
    command = " ".join(shlex.quote(a) for a in args)
    result = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", host, command],
                            capture_output=True)
    if result.returncode != 0:
        return None
    return result.stdout if binary else result.stdout.decode("utf-8", "replace")


def normalize(text):
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii").lower()
    text = re.sub(r"\([^)]*\)|\[[^\]]*\]", " ", text)
    text = text.replace("&", " and ")
    m = re.match(r"^(.*?),\s*(the|a|an)\s*$", text.strip())
    if m:
        text = m.group(2) + " " + m.group(1)
    return re.sub(r"[^a-z0-9]+", "", text)


def clean_title(name):
    stem = re.sub(r"\.[A-Za-z0-9]{1,4}$", "", name)
    stem = re.sub(r"\s*(\([^)]*\)|\[[^\]]*\])", "", stem).strip()
    m = re.match(r"^(.*?),\s*(The|A|An)$", stem)
    if m:
        stem = m.group(2) + " " + m.group(1)
    return stem.strip()


class Api:
    def __init__(self, auth, deadline):
        self.auth = auth
        self.deadline = deadline
        self.last = 0.0
        self.count = 0
        self.tmp = tempfile.mkdtemp()

    def call(self, endpoint, params):
        if time.time() > self.deadline:
            raise Stop("time limit reached")
        wait = MIN_SPACING - (time.time() - self.last)
        if wait > 0:
            time.sleep(wait)
        path = os.path.join(self.tmp, "r.json")
        status, error = probe.request(endpoint, dict(self.auth, output="json", **params), path, timeout=60)
        self.last = time.time()
        self.count += 1
        if status in HARD_STOPS:
            raise Stop("stopped: %s" % probe.describe_failure(status, path))
        if status != 200:
            return status, None
        try:
            with open(path, encoding="utf-8") as handle:
                return 200, json.load(handle)
        except (OSError, ValueError):
            return 200, None


def media_summary(game):
    types = {m.get("type") for m in (game.get("medias") or []) if isinstance(m, dict)}
    return {"video": "video" in types, "video_normalized": "video-normalized" in types,
            "box2d": "box-2D" in types, "ss": "ss" in types}


def game_names(game):
    names = []
    for entry in game.get("noms") or []:
        if isinstance(entry, dict) and entry.get("text"):
            names.append(entry["text"])
    if game.get("nom"):
        names.append(game["nom"])
    return names


def game_info(game):
    names = game_names(game)
    return {"id": game.get("id"), "name": names[0] if names else "?",
            "system": (game.get("systeme") or {}).get("id") if isinstance(game.get("systeme"), dict) else None,
            **media_summary(game)}


def search(api, title, system_id):
    params = {"recherche": title}
    if system_id:
        params["systemeid"] = system_id
    status, data = api.call("jeuRecherche.php", params)
    if data is None:
        return {"status": status, "results": 0, "exact": None, "first": None}
    games = [g for g in (data.get("response", {}).get("jeux") or []) if isinstance(g, dict) and g.get("id")]
    wanted = normalize(title)
    exact = None
    exact_right_system = None
    for g in games:
        if any(normalize(n) == wanted for n in game_names(g)):
            if exact is None:
                exact = g
            sid = (g.get("systeme") or {}).get("id") if isinstance(g.get("systeme"), dict) else None
            if system_id and str(sid) == str(system_id) and exact_right_system is None:
                exact_right_system = g
    return {"status": status, "results": len(games),
            "exact": game_info(exact) if exact else None,
            "exact_right_system": game_info(exact_right_system) if exact_right_system else None,
            "first": game_info(games[0]) if games else None}


def hashes(data):
    return {"crc": "%08X" % (zlib.crc32(data) & 0xFFFFFFFF), "md5": hashlib.md5(data).hexdigest().upper(),
            "sha1": hashlib.sha1(data).hexdigest().upper()}


def hash_lookup(api, system_id, file_name, content, kind):
    h = hashes(content)
    params = {"systemeid": system_id, "romtype": "rom", "romnom": file_name, "romtaille": len(content), **h}
    status, data = api.call("jeuInfos.php", params)
    if data is None:
        return {"status": status, "kind": kind, "found": None}
    game = data.get("response", {}).get("jeu")
    return {"status": status, "kind": kind, "found": game_info(game) if isinstance(game, dict) else None}


def run_game(api, host, system_key, system_id, roots, game, allow_hash):
    root_index, rel = game
    base = roots.get(root_index, "")
    path = base + "/" + rel
    file_name = os.path.basename(rel)
    record = {"system": system_key, "file": file_name, "title": clean_title(file_name)}

    # H: by hash
    record["H"] = None
    if allow_hash and system_key not in DISC_SYSTEMS and system_key != "Arcade" and system_id:
        size_text = ssh(host, "stat", "-c", "%s", path)
        try:
            size = int(size_text.strip()) if size_text else -1
        except ValueError:
            size = -1
        if size < 0:
            record["H"] = {"skipped": "not a readable file"}
        elif size > MAX_HASH_BYTES:
            record["H"] = {"skipped": "over 16 MB"}
        else:
            blob = ssh(host, "cat", path, binary=True)
            if blob is None:
                record["H"] = {"skipped": "read failed"}
            else:
                tries = []
                if zipfile.is_zipfile(io.BytesIO(blob)):
                    with zipfile.ZipFile(io.BytesIO(blob)) as archive:
                        members = [i for i in archive.infolist() if not i.is_dir()]
                        if len(members) == 1 and members[0].file_size <= MAX_HASH_BYTES:
                            inner = archive.read(members[0])
                            tries.append((members[0].filename, inner, "inner"))
                    tries.append((file_name, blob, "zip"))
                else:
                    tries.append((file_name, blob, "plain"))
                results = []
                for name, content, kind in tries:
                    result = hash_lookup(api, system_id, name, content, kind)
                    results.append(result)
                    if result.get("found"):
                        break   # the first variant that is known is enough
                record["H"] = {"tries": results, "found": next((r["found"] for r in results if r.get("found")), None),
                               "by": next((r["kind"] for r in results if r.get("found")), None)}
    else:
        record["H"] = {"skipped": "disc image or arcade or unknown system" if system_key in DISC_SYSTEMS or system_key == "Arcade" else "no system id"}

    record["T1"] = search(api, record["title"], system_id) if system_id else {"skipped": "no system id"}
    record["T2"] = search(api, record["title"], None)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--per-system", type=int, default=5)
    parser.add_argument("--systems", default="")
    parser.add_argument("--no-hash", action="store_true")
    parser.add_argument("--max-minutes", type=float, default=25.0)
    parser.add_argument("--host", default="root@192.168.64.191")
    args = parser.parse_args()

    env = probe.load_env(probe.ENV_FILE)
    auth = {"devid": env["SCREENSCRAPER_DEVID"], "devpassword": env["SCREENSCRAPER_DEVPASSWORD"],
            "softname": env["SCREENSCRAPER_SOFTNAME"], "ssid": env["SCREENSCRAPER_SSID"],
            "sspassword": env["SCREENSCRAPER_SSPASSWORD"]}
    api = Api(auth, time.time() + args.max_minutes * 60)

    # System ids from the saved systemesListe answer.
    with open(os.path.join(probe.OUT_DIR, "systemesListe.json"), encoding="utf-8") as handle:
        systems = json.load(handle)["response"]["systemes"]
    by_name = {}
    for system in systems:
        names = system.get("noms") or {}
        for key in ("nom_retropie", "nom_recalbox"):
            for part in str(names.get(key, "")).split(","):
                if part.strip():
                    by_name.setdefault(part.strip().lower(), system["id"])

    # Games on the device.
    base = "/media/fat/mister-pat/gamesdb/"
    catalog = ssh(args.host, "cat", base + "catalog.tsv")
    roots_text = ssh(args.host, "cat", base + "roots.tsv")
    if catalog is None or roots_text is None:
        print("cannot read the game database on %s" % args.host, file=sys.stderr)
        return 1
    roots = {}
    for line in roots_text.splitlines():
        parts = line.split("\t")
        if len(parts) >= 2 and parts[0].isdigit():
            roots[parts[0]] = parts[1]
    wanted = {s.strip() for s in args.systems.split(",") if s.strip()}

    sample = []
    for line in catalog.splitlines():
        parts = line.split("\t")
        if len(parts) < 3 or line.startswith("#"):
            continue
        key = parts[0]
        if wanted and key not in wanted:
            continue
        text = ssh(args.host, "cat", base + key + ".tsv")
        games = [tuple(l.split("\t", 1)) for l in (text or "").splitlines() if "\t" in l and not l.startswith("#")]
        if not games:
            continue
        n = min(args.per_system, len(games))
        picks = [games[min(len(games) - 1, int((i + 0.5) * len(games) / n))] for i in range(n)]
        sample.append((key, parts[1], picks))
    print("%d systems with games, %d games in the sample" % (len(sample), sum(len(p) for _, _, p in sample)))

    out_path = os.path.join(probe.OUT_DIR, "compare-results.jsonl")
    os.makedirs(probe.OUT_DIR, exist_ok=True)
    results = []
    stopped = ""
    with open(out_path, "w", encoding="utf-8") as out:
        try:
            for key, label, picks in sample:
                system_id = by_name.get(SYSTEM_NAMES.get(key, "").lower())
                print("== %s (%s) -> ScreenScraper system %s" % (label, key, system_id or "UNMAPPED"), flush=True)
                for game in picks:
                    record = run_game(api, args.host, key, system_id, roots, game, not args.no_hash)
                    results.append(record)
                    out.write(json.dumps(record, ensure_ascii=False) + "\n")
                    out.flush()
                    h = record["H"].get("found") if isinstance(record["H"], dict) else None
                    t1 = record["T1"].get("exact") if isinstance(record["T1"], dict) else None
                    t2 = record["T2"].get("exact") if isinstance(record["T2"], dict) else None
                    print("   %-48.48s  H:%-4s T1:%-4s T2:%-4s" % (
                        record["title"], "yes" if h else ("-" if isinstance(record["H"], dict) and "skipped" in record["H"] else "no"),
                        "yes" if t1 else "no", "yes" if t2 else "no"), flush=True)
        except Stop as exc:
            stopped = str(exc)
            print("\n!! " + stopped)

    # Summary.
    print("\nrequests made: %d, games looked up: %d%s" % (api.count, len(results), (" (stopped early)" if stopped else "")))
    per = {}
    for r in results:
        s = per.setdefault(r["system"], {"n": 0, "H": 0, "Hn": 0, "T1": 0, "T2": 0, "vid": 0})
        s["n"] += 1
        if isinstance(r["H"], dict) and "skipped" not in r["H"]:
            s["Hn"] += 1
            s["H"] += 1 if r["H"].get("found") else 0
        s["T1"] += 1 if isinstance(r["T1"], dict) and r["T1"].get("exact") else 0
        s["T2"] += 1 if isinstance(r["T2"], dict) and r["T2"].get("exact") else 0
        best = (r["H"].get("found") if isinstance(r["H"], dict) else None) or \
               (r["T1"].get("exact") if isinstance(r["T1"], dict) else None) or \
               (r["T2"].get("exact") if isinstance(r["T2"], dict) else None)
        s["vid"] += 1 if best and (best["video"] or best["video_normalized"]) else 0
    print("\n%-14s %5s %9s %5s %5s %7s" % ("system", "games", "hash", "T1", "T2", "w/video"))
    tot = {"n": 0, "H": 0, "Hn": 0, "T1": 0, "T2": 0, "vid": 0}
    for key, s in per.items():
        print("%-14s %5d %4d/%-4d %5d %5d %7d" % (key, s["n"], s["H"], s["Hn"], s["T1"], s["T2"], s["vid"]))
        for k in tot:
            tot[k] += s[k]
    print("%-14s %5d %4d/%-4d %5d %5d %7d" % ("ALL", tot["n"], tot["H"], tot["Hn"], tot["T1"], tot["T2"], tot["vid"]))
    print("\nT1/T2 count only exact title matches. Detailed lines: %s" % os.path.relpath(out_path, ROOT))
    return 0 if not stopped else 3


if __name__ == "__main__":
    sys.exit(main())
