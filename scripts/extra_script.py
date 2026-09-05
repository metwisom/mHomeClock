"""
Injects two build-time macros into the firmware:

  BUILD_NUMBER - integer, auto-incremented on every build. Stored (and
                 committed) in build.json.

  BUILD_TAG    - string identifying who produced the build. Resolved in
                 order:
                   1. BUILD_TAG environment variable (one-off override,
                      never persisted)
                   2. "build_tag" in build.json
                   3. falls back to "custom"

build.json is tracked in git, so the counter and tag travel with the repo.
Override BUILD_TAG via environment variable for a one-off build without
touching the committed file.
"""
Import("env")
import json
import os

project_dir = env.subst("$PROJECT_DIR")
build_file = os.path.join(project_dir, "build.json")


def load_build_file():
    if os.path.isfile(build_file):
        try:
            with open(build_file) as f:
                return json.load(f)
        except Exception:
            pass
    return {}


data = load_build_file()

# One-time migration from older layouts, if present.
old_local_file = os.path.join(project_dir, "build.local.json")
old_number_file = os.path.join(project_dir, "build_number.txt")
old_tag_file = os.path.join(project_dir, "build_tag.local.txt")
if os.path.isfile(old_local_file):
    try:
        with open(old_local_file) as f:
            old_data = json.load(f)
        data.setdefault("build_number", old_data.get("build_number"))
        data.setdefault("build_tag", old_data.get("build_tag"))
    except Exception:
        pass
    os.remove(old_local_file)
if "build_number" not in data and os.path.isfile(old_number_file):
    try:
        with open(old_number_file) as f:
            data["build_number"] = int(f.read().strip())
    except Exception:
        pass
    os.remove(old_number_file)
if "build_tag" not in data and os.path.isfile(old_tag_file):
    with open(old_tag_file) as f:
        data["build_tag"] = f.read().strip()
    os.remove(old_tag_file)

build_number = int(data.get("build_number", 0)) + 1
data["build_number"] = build_number

env_tag = os.environ.get("BUILD_TAG", "").strip()
file_tag = str(data.get("build_tag", "")).strip()
build_tag = env_tag or file_tag or "custom"

with open(build_file, "w") as f:
    json.dump(data, f)

env.Append(CPPDEFINES=[
    ("BUILD_NUMBER", build_number),
    ("BUILD_TAG", '\\"%s\\"' % build_tag),
])

print("[build] #%d, tag: %s" % (build_number, build_tag))
