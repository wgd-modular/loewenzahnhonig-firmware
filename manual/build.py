#!/usr/bin/env python
#
# Builds the PDF manual. Every firmware in src/ gets its own chapter, taken
# from its README.md: the first `#` heading is the firmware's name, any text
# before the first section heading is its introduction, and every heading of
# the shallowest level below the title starts a section of the chapter. Who
# worked on a firmware and when it last changed comes from the git history of
# its folder. The result is written to
# manual/build/loewenzahnhonig-manual.pdf.
#
import datetime
import json
import os
import pathlib
import posixpath
import re
import subprocess
import sys
import urllib.parse
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
MANUAL = ROOT / "manual"
OUT = MANUAL / "build"
REPO = os.environ.get("GITHUB_REPOSITORY",
                      "wgd-modular/loewenzahnhonig-firmware")

HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
FENCE = re.compile(r"^\s*(```|~~~)")
LINK = re.compile(r"(?<!!)\[([^\]]*)\]\(([^)\s]+)\)")
# Sections for people who build or flash a firmware themselves, which the
# manual leaves out: the web flasher and the building chapter cover that.
BUILDER_SECTION = re.compile(
    r"^(build\w*|flashing\b.*|cpu load|performance)\s*:?$", re.I)

LOGINS = {}
NOREPLY = re.compile(r"^(?:\d+\+)?([^@]+)@users\.noreply\.github\.com$")


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True,
                          text=True, check=True).stdout


def parse_readme(text):
    """Splits a README into its title, introduction and sections.

    The sections are the headings of the shallowest level below the title,
    so a README that only uses `###` headings is split just like one that
    uses `##`. Deeper headings stay inside their section.
    """
    lines = text.splitlines()
    marks = {}
    fenced = False
    for number, line in enumerate(lines):
        if FENCE.match(line):
            fenced = not fenced
        match = None if fenced else HEADING.match(line)
        if match:
            marks[number] = (len(match.group(1)), match.group(2))

    title_line = next((number for number in sorted(marks)
                       if marks[number][0] == 1), None)
    title = marks[title_line][1] if title_line is not None else None
    below = [level for number, (level, _) in marks.items()
             if number != title_line and level > 1]
    split = min(below) if below else None

    intro, sections = [], []
    for number, line in enumerate(lines):
        if number == title_line:
            continue
        if number in marks and marks[number][0] == split:
            sections.append({"title": marks[number][1], "lines": []})
        elif sections:
            sections[-1]["lines"].append(line)
        else:
            intro.append(line)
    for section in sections:
        section["body"] = "\n".join(section.pop("lines")).strip()
    return title, "\n".join(intro).strip(), sections


def split_controls(body):
    """Splits a controls section into its overview and the detail below it.

    The overview is everything up to the first sub-heading, a table or a
    list of the controls. The manual puts it in a plaque.
    """
    lines = body.splitlines()
    fenced = False
    for number, line in enumerate(lines):
        if FENCE.match(line):
            fenced = not fenced
        if not fenced and HEADING.match(line):
            return ("\n".join(lines[:number]).strip(),
                    "\n".join(lines[number:]).strip())
    return body, ""


def plain(text):
    text = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", text)
    return re.sub(r"[*_`]", "", " ".join(text.split()))


def summary(intro, sections):
    """The first sentence of a firmware's description, as plain text.

    This is the flasher's rule: the first prose paragraph, skipping sections
    that only credit people (Author, Version History).
    """
    candidates = [intro] + [section["body"] for section in sections
                            if not re.match(r"(author|version history)\b",
                                            section["title"], re.I)]
    for text in candidates:
        for block in text.split("\n\n"):
            if block.strip() and block.lstrip()[0] not in "|-*>!<#`0123456789":
                sentence = first_sentence(plain(block))
                if sentence:
                    return sentence
    return ""


def first_sentence(text):
    match = re.match(r"(.+?[.!?:])(\s|$)", text)
    sentence = match.group(1) if match else text
    return sentence[:-1] + "." if sentence.endswith(":") else sentence


def display_title(title, folder):
    title = title or folder
    if title == title.lower():
        title = title.replace("-", " ").title()
    return title


def resolve_links(text, folder, ids):
    """Points relative links at a chapter or at the repository on GitHub.

    A link whose text is just the path of a README is shown as the name of
    that firmware instead.
    """
    def replace(match):
        label, url = match.groups()
        if re.match(r"([a-z][a-z0-9+.-]*:|#)", url, re.I):
            return match.group(0)
        path = posixpath.normpath(posixpath.join("src", folder, url))
        path, _, anchor = path.partition("#")
        chapter = re.fullmatch(r"src/([^/]+)/README\.md", path)
        if chapter and chapter.group(1) in ids:
            if label.endswith("README.md"):
                label = display_title(None, chapter.group(1))
            return "[{}](chapter:{})".format(label, chapter.group(1))
        return "[{}](https://github.com/{}/blob/main/{})".format(
            label, REPO, path)
    return LINK.sub(replace, text)


def github_api(path):
    request = urllib.request.Request("https://api.github.com/" + path,
                                     headers={"Accept":
                                              "application/vnd.github+json"})
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        request.add_header("Authorization", "Bearer " + token)
    try:
        with urllib.request.urlopen(request, timeout=10) as response:
            return json.load(response)
    except (OSError, ValueError):
        return None


def github_login(email, sha=None):
    """The GitHub account behind a commit email, if GitHub knows it."""
    noreply = NOREPLY.match(email)
    if noreply:
        return noreply.group(1)
    if sha:
        commit = github_api("repos/{}/commits/{}".format(REPO, sha)) or {}
        login = (commit.get("author") or {}).get("login")
        if login:
            return login
    # Co-authors are not linked on the commit, look for any commit of theirs.
    found = github_api("search/commits?per_page=1&q=author-email:" +
                       urllib.parse.quote(email)) or {}
    for item in found.get("items", []):
        return (item.get("author") or {}).get("login")
    return None


def history(folder):
    """Authors, first and last change of a folder, oldest author first."""
    log = git("log", "--reverse", "--format=%H%x1f%an%x1f%ae%x1f%as%x1f%s%x1f"
              "%(trailers:key=Co-authored-by,valueonly,separator=%x1e)%x1d",
              "--", str(folder.relative_to(ROOT)))
    entries = [entry.strip("\n").split("\x1f")
               for entry in log.split("\x1d") if entry.strip()]
    if not entries:
        return None

    people = {}
    for sha, name, email, date, subject, coauthors in entries:
        candidates = [(name, email)]
        for trailer in filter(None, coauthors.split("\x1e")):
            match = re.match(r"\s*(.*?)\s*<(.*)>", trailer)
            if match:
                candidates.append(match.groups())
        for person_name, person_email in candidates:
            if person_name.endswith("[bot]"):
                continue
            key = person_email.lower()
            if key not in people:
                people[key] = {"name": person_name, "email": key}
            if person_email == email:
                people[key].setdefault("sha", sha)

    # The same person often commits under more than one name or address,
    # merge them by GitHub account and keep the most complete name.
    authors = {}
    for person in people.values():
        if person["email"] not in LOGINS:
            LOGINS[person["email"]] = github_login(person["email"],
                                                   person.get("sha"))
        login = LOGINS[person["email"]]
        key = login or person["email"]
        name = person["name"]
        if key in authors:
            known = authors[key]["name"]
            name = known if " " in known or " " not in name else name
        authors[key] = {"name": name, "login": login}

    first, last = entries[0], entries[-1]
    return {
        "authors": list(authors.values()),
        "added": first[3],
        "updated": last[3],
        "commit": last[0][:7],
        "subject": last[4],
    }


def order(firmwares):
    """Alphabetical, with variants right behind the firmware they link to.

    A firmware whose README links to the README of another firmware (the
    Nimbus and Rings variants do) is a variant of that one.
    """
    ids = {fw["id"] for fw in firmwares}
    for fw in firmwares:
        fw["base"] = None
        for _, url in LINK.findall(fw["readme"]):
            target = re.fullmatch(r"\.\./([^/]+)/README\.md", url)
            base = target.group(1) if target else None
            if base in ids and base != fw["id"]:
                fw["base"] = base
                break
    ordered = []
    for fw in sorted(firmwares, key=lambda fw: fw["id"]):
        if fw["base"] is None:
            ordered.append(fw)
            ordered.extend(sorted((other for other in firmwares
                                   if other["base"] == fw["id"]),
                                  key=lambda other: other["id"]))
    return ordered


def collect():
    firmwares = []
    readmes = sorted((ROOT / "src").glob("*/README.md"))
    ids = {readme.parent.name for readme in readmes}
    for readme in readmes:
        folder = readme.parent
        text = readme.read_text()
        title, intro, sections = parse_readme(resolve_links(
            text, folder.name, ids))
        sections = [section for section in sections
                    if not BUILDER_SECTION.match(section["title"])]
        for section in sections:
            if section["title"] == section["title"].lower():
                section["title"] = section["title"].capitalize()
            section["controls"] = bool(re.fullmatch(
                r"controls?\s*:?", section["title"], re.I))
            if section["controls"]:
                section["body"], section["rest"] = split_controls(
                    section["body"])
        firmwares.append({
            "id": folder.name,
            "title": display_title(title, folder.name),
            "intro": intro,
            "summary": summary(intro, sections),
            "sections": sections,
            "path": str(folder.relative_to(ROOT)),
            "readme": text,
            "history": history(folder),
        })
    firmwares = order(firmwares)
    for fw in firmwares:
        del fw["readme"]

    names = {}
    for firmware in firmwares:
        for author in (firmware["history"] or {}).get("authors", []):
            known = names.get(author["login"], "")
            if author["login"] and (" " in author["name"] or not known):
                names[author["login"]] = author["name"]
    for firmware in firmwares:
        for author in (firmware["history"] or {}).get("authors", []):
            author["name"] = names.get(author["login"], author["name"])

    manifest = json.loads((ROOT / ".release-please-manifest.json").read_text())
    head = git("log", "-1", "--format=%H%x1f%cs").strip().split("\x1f")
    version = manifest.get(".")
    tags = git("tag", "--points-at", "HEAD").split()
    return {
        "repo": REPO,
        "version": version,
        "release": "v{}".format(version) in tags,
        "commit": head[0][:7],
        "date": head[1],
        "built": datetime.date.today().isoformat(),
        "firmwares": firmwares,
    }


def main():
    OUT.mkdir(exist_ok=True)
    data = collect()
    (OUT / "data.json").write_text(json.dumps(data, indent=2,
                                              ensure_ascii=False))
    print("Collected {} firmwares".format(len(data["firmwares"])))
    pdf = OUT / "loewenzahnhonig-manual.pdf"
    result = subprocess.call([
        "typst", "compile", "--root", str(ROOT),
        "--font-path", str(MANUAL / "fonts"), "--ignore-system-fonts",
        str(MANUAL / "manual.typ"), str(pdf),
    ])
    if result == 0:
        print("  -> {}".format(pdf.relative_to(ROOT)))
    sys.exit(result)


if __name__ == "__main__":
    main()
