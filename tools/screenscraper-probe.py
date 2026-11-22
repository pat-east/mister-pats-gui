#!/usr/bin/env python3
"""Probe the ScreenScraper.fr API from the development machine (a proof of concept, not part of the GUI).

    python3 -I tools/screenscraper-probe.py [--gameid 5396] [--download] [--from-device root@HOST]

Reads the credentials from `.screenscraper.fr.env` in the repository root (ignored by Git), or with
--from-device straight from the settings file on the MiSTer (over SSH, into memory only), and
makes the fewest requests it can:

  1. ssuserInfos.php   your account: level, threads, quotas
  2. jeuInfos.php      the game (by gameid): names, system, regions, and the list of media
  3. with --download: mediaVideoJeu.php, one video, saved under _vnext/files/ (ignored by Git)

What it keeps off the screen and off the disk: credentials are given to curl on standard input
(never as arguments, so they do not show up in the process list), never printed, and removed from
the saved answer before it is written. Raw answers are saved to _vnext/files/api/ (ignored by Git).
No debug parameters are used, so nothing here touches the debug-mode allowance.
"""
import argparse
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENV_FILE = os.path.join(ROOT, ".screenscraper.fr.env")
OUT_DIR = os.path.join(ROOT, "_vnext", "files", "api")
BASE = "https://api.screenscraper.fr/api2/"

SECRET_PARAMS = ("devid", "devpassword", "ssid", "sspassword", "softname")

HTTP_MEANING = {
    400: "malformed request (a field is missing or wrong)",
    401: "API closed to this account, or the server is overloaded",
    403: "developer login rejected: check devid / devpassword",
    404: "not found",
    423: "API fully closed by ScreenScraper right now",
    426: "this software is blocked by ScreenScraper (not compliant or obsolete)",
    429: "too many requests at once or per minute: slow down",
    430: "daily quota used up: try again tomorrow",
    431: "too many unrecognized ROMs today",
}


def parse_env(lines):
    values = {}
    for line in lines:
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        values[key.strip()] = value
    return values


def load_env(path):
    with open(path, encoding="utf-8") as handle:
        return parse_env(handle.read().splitlines())


def load_from_device(target, path):
    """Read the settings file from the MiSTer over SSH, into memory only (nothing is saved here)."""
    result = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", target, "cat", path],
                            capture_output=True, text=True)
    if result.returncode != 0:
        return None
    return parse_env(result.stdout.splitlines())


def redact(text):
    """Remove credential values from any text before it is stored or shown."""
    pattern = r"((?:%s)=)[^&\"'\s<>]*" % "|".join(SECRET_PARAMS)
    return re.sub(pattern, r"\1***", text)


def curl_config_escape(value):
    return value.replace("\\", "\\\\").replace('"', '\\"')


def request(endpoint, params, output_path, timeout=120):
    """GET one API endpoint. Parameters travel to curl on standard input."""
    lines = ["get", 'url = "%s"' % (BASE + endpoint)]
    for key, value in params.items():
        lines.append('data-urlencode = "%s=%s"' % (key, curl_config_escape(str(value))))
    config = "\n".join(lines) + "\n"
    result = subprocess.run(
        ["curl", "-sS", "--proto", "=https", "-m", str(timeout), "-o", output_path,
         "-w", "%{http_code}", "-K", "-"],
        input=config, capture_output=True, text=True)
    if result.returncode != 0:
        return 0, redact(result.stderr.strip())
    try:
        return int(result.stdout.strip()), ""
    except ValueError:
        return 0, "unexpected curl output"


def describe_failure(status, body_path):
    reason = HTTP_MEANING.get(status, "unexpected answer")
    snippet = ""
    try:
        with open(body_path, "rb") as handle:
            snippet = redact(handle.read(160).decode("utf-8", "replace")).strip().replace("\n", " ")
    except OSError:
        pass
    # 403 has two causes with different texts: the developer login and the user login.
    if status == 403 and "utilisateur" in snippet.lower():
        reason = "USER login rejected: check ssid / sspassword"
    elif status == 403 and ("veloppeur" in snippet.lower()):
        reason = "DEVELOPER login rejected: check devid / devpassword"
    return "HTTP %s: %s%s" % (status, reason, (" (" + snippet + ")") if snippet else "")


def pick(mapping, *names, default="?"):
    for name in names:
        if isinstance(mapping, dict) and name in mapping and mapping[name] not in (None, ""):
            return mapping[name]
    return default


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--gameid", type=int, default=5396)
    parser.add_argument("--download", action="store_true", help="also download the video (one more request)")
    parser.add_argument("--media", default="", help="media type to download with --download (default: the first video)")
    parser.add_argument("--guest", action="store_true", help="ignore the user account in the settings and run as a guest")
    parser.add_argument("--from-device", metavar="USER@HOST",
                        help="read the settings file from the MiSTer over SSH instead of the local env file")
    parser.add_argument("--device-file", default="/media/fat/mister-pat/screenscraper-fr-settings.ini")
    args = parser.parse_args()

    if args.from_device:
        env = load_from_device(args.from_device, args.device_file)
        if env is None:
            print("could not read %s on %s" % (args.device_file, args.from_device), file=sys.stderr)
            return 1
    else:
        if not os.path.isfile(ENV_FILE):
            print("missing %s (create it, see its template)" % ENV_FILE, file=sys.stderr)
            return 1
        env = load_env(ENV_FILE)
    needed = {
        "devid": env.get("SCREENSCRAPER_DEVID", ""),
        "devpassword": env.get("SCREENSCRAPER_DEVPASSWORD", ""),
        "softname": env.get("SCREENSCRAPER_SOFTNAME", ""),
    }
    missing = [name for name, value in needed.items() if not value]
    if missing:
        print("the env file has no value for: " + ", ".join(missing), file=sys.stderr)
        return 1
    auth = dict(needed)
    ssid, sspassword = env.get("SCREENSCRAPER_SSID", ""), env.get("SCREENSCRAPER_SSPASSWORD", "")
    if args.guest:
        pass
    elif ssid and sspassword:
        auth["ssid"] = ssid
        auth["sspassword"] = sspassword
    else:
        print("note: no user account in the env file, running as a guest")
    os.makedirs(OUT_DIR, exist_ok=True)
    os.chmod(OUT_DIR, 0o700)

    # 1. Account (only with a user account; as a guest this request is skipped)
    if "ssid" in auth:
        path = os.path.join(OUT_DIR, "ssuserInfos.json")
        status, error = request("ssuserInfos.php", dict(auth, output="json"), path)
        print("== account (ssuserInfos.php)")
        if status != 200:
            print("   failed: " + (error or describe_failure(status, path)))
            return 2
        try:
            with open(path, encoding="utf-8") as handle:
                data = json.load(handle)
            user = data["response"]["ssuser"]
        except (OSError, ValueError, KeyError):
            print("   the answer is not the expected JSON")
            return 2
        for key in ("niveau", "contribution", "maxthreads", "maxdownloadspeed", "requeststoday",
                    "requestskotoday", "maxrequestspermin", "maxrequestsperday", "maxrequestskoperday"):
            if key in user:
                print("   %-22s %s" % (key, user[key]))
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(redact(json.dumps(user, indent=1)))
    else:
        print("== account: skipped (guest run, no user name in the settings)")

    # 2. Game
    path = os.path.join(OUT_DIR, "jeuInfos-%d.json" % args.gameid)
    status, error = request("jeuInfos.php", dict(auth, output="json", gameid=args.gameid), path)
    print("== game %d (jeuInfos.php)" % args.gameid)
    if status != 200:
        print("   failed: " + (error or describe_failure(status, path)))
        return 3
    try:
        with open(path, encoding="utf-8") as handle:
            data = json.load(handle)
        game = data["response"]["jeu"]
    except (OSError, ValueError, KeyError):
        print("   the answer is not the expected JSON")
        return 3
    raw_text = redact(json.dumps(data, indent=1))
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(raw_text)

    names = game.get("noms") or []
    first_name = pick(names[0], "text") if names and isinstance(names[0], dict) else pick(game, "nom")
    system = game.get("systeme", {})
    print("   name      : %s" % first_name)
    print("   system    : %s (id %s)" % (pick(system, "text", "nom"), pick(system, "id")))
    medias = game.get("medias") or []
    print("   media     : %d entries" % len(medias))
    types = {}
    for media in medias:
        if isinstance(media, dict):
            types.setdefault(media.get("type", "?"), []).append(media)
    for media_type in sorted(types):
        entries = types[media_type]
        formats = sorted({str(m.get("format", "?")) for m in entries})
        regions = sorted({str(m.get("region", "-")) for m in entries})
        print("     %-24s %2d  formats %s  regions %s" % (media_type, len(entries), ",".join(formats), ",".join(regions)))
    videos = [m for m in medias if isinstance(m, dict) and str(m.get("type", "")).startswith("video")]
    print("   videos    : %d" % len(videos))
    for video in videos:
        size = video.get("size", "?")
        print("     type %s, format %s, region %s, size %s bytes" % (
            video.get("type"), video.get("format"), video.get("region", "-"), size))
    print("   saved (credentials removed): %s" % os.path.relpath(path, ROOT))

    # 3. Video
    if not args.download:
        print("\nNo video downloaded. Run again with --download to fetch it (one more request).")
        return 0
    if not videos:
        print("\nThis game has no video to download.")
        return 0
    video = videos[0]
    if args.media:
        chosen = [v for v in videos if v.get("type") == args.media]
        if not chosen:
            print("\nNo video of type %s. Available: %s" % (args.media, ", ".join(str(v.get("type")) for v in videos)))
            return 0
        video = chosen[0]
    media_type = video.get("type", "video")
    system_id = pick(system, "id", default="")
    params = dict(auth, jeuid=args.gameid, media=media_type)
    if system_id:
        params["systemeid"] = system_id
    target = os.path.join(ROOT, "_vnext", "files", "api-%d-%s.%s" % (args.gameid, media_type, video.get("format", "mp4")))
    status, error = request("mediaVideoJeu.php", params, target, timeout=300)
    print("== video (mediaVideoJeu.php, media=%s)" % media_type)
    if status != 200:
        print("   failed: " + (error or describe_failure(status, target)))
        try:
            os.remove(target)
        except OSError:
            pass
        return 4
    size = os.path.getsize(target)
    with open(target, "rb") as handle:
        head = handle.read(12)
    looks_like_mp4 = len(head) >= 8 and head[4:8] == b"ftyp"
    print("   saved %s (%d bytes, %s)" % (os.path.relpath(target, ROOT), size,
                                          "MP4 header ok" if looks_like_mp4 else "NOT an MP4 (maybe an error text)"))
    if not looks_like_mp4:
        print("   first bytes: " + redact(head.decode("utf-8", "replace")))
        return 5
    return 0


if __name__ == "__main__":
    sys.exit(main())
