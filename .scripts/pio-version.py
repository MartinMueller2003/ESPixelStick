# Get CI version number
Import('env')
import os
import subprocess

version = "dev"

if "CI" in os.environ:
    with open("firmware/VERSION", "r") as file:
        version = file.readline().rstrip()
    env.Append(BUILD_FLAGS=[f"-DESPS_VERSION=\"{version}\""])

# The commit the image was built from, for the serial banner and the Admin page. The version
# string itself is not touched. "-dirty" marks a tree with uncommitted source changes.
try:
    project_dir = env.subst("$PROJECT_DIR")
    commit = subprocess.check_output(["git", "rev-parse", "--short=8", "HEAD"], cwd=project_dir, text=True).strip()
    changes = subprocess.check_output(["git", "status", "--porcelain", "--", "src", "include", "html", "platformio.ini"], cwd=project_dir, text=True).strip()
    if changes:
        commit += "-dirty"
    env.Append(BUILD_FLAGS=[f"-DESPS_BUILD_COMMIT=\"{commit}\""])
except Exception:
    pass
