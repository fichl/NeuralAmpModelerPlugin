import zipfile, os, sys, shutil

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

iplug2_scripts = os.path.abspath(os.path.join(projectpath, os.pardir, "iPlug2", "Scripts"))

sys.path.insert(0, iplug2_scripts)

from get_archive_name import get_archive_name
from parse_config import parse_config


def add_file(zf, path, arcname=None):
    if not os.path.isfile(path):
        raise FileNotFoundError(path)
    zf.write(path, arcname or os.path.basename(path), zipfile.ZIP_DEFLATED)


def add_dir(zf, path, arcroot=None):
    if not os.path.isdir(path):
        raise FileNotFoundError(path)
    arcroot = arcroot or os.path.basename(path)
    count = 0
    for root, _, filenames in os.walk(path):
        for filename in filenames:
            fullpath = os.path.join(root, filename)
            relpath = os.path.relpath(fullpath, path)
            zf.write(fullpath, os.path.join(arcroot, relpath), zipfile.ZIP_DEFLATED)
            count += 1
    if count == 0:
        raise RuntimeError("Directory is empty: " + path)


def validate_inputs(files):
    for path, _ in files:
        if not os.path.exists(path):
            raise FileNotFoundError(path)
        if os.path.isdir(path) and not any(os.scandir(path)):
            raise RuntimeError("Directory is empty: " + path)


def first_existing(paths):
    for path in paths:
        if os.path.exists(path):
            return path
    return paths[0]


def main():
    if len(sys.argv) != 3:
        print("Usage: make_zip.py demo[0/1] zip[0/1]")
        sys.exit(1)
    else:
        demo = int(sys.argv[1])
        zip = int(sys.argv[2])

    config = parse_config(projectpath)
    bundle_name = config["BUNDLE_NAME"]
    project_name = os.path.basename(projectpath)

    dir = os.path.join(projectpath, "build-win", "out")

    if os.path.exists(dir):
        shutil.rmtree(dir)

    os.makedirs(dir)

    files = []

    if not zip:
        installer_name = bundle_name + " Installer.exe"
        installer = os.path.join("build-win", "installer", installer_name)

        if demo:
            installer = os.path.join("build-win", "installer", bundle_name + " Demo Installer.exe")

        manual = os.path.join(projectpath, "manual", bundle_name + " manual.pdf")
        if not os.path.isfile(manual):
            manual = os.path.join(projectpath, "manual", project_name + " manual.pdf")

        files = [
            (os.path.join(projectpath, installer), None),
            (os.path.join(projectpath, "installer", "changelog.txt"), None),
            (os.path.join(projectpath, "installer", "known-issues.txt"), None),
            (manual, None),
        ]
    else:
        vst3 = first_existing(
            [
                os.path.join(projectpath, "build-win", bundle_name + ".vst3"),
                os.path.join(
                    projectpath,
                    "build-win",
                    "vst3",
                    "x64",
                    "Release",
                    bundle_name + ".vst3",
                ),
            ]
        )
        app = first_existing(
            [
                os.path.join(projectpath, "build-win", bundle_name + "_x64.exe"),
                os.path.join(
                    projectpath,
                    "build-win",
                    "app",
                    "x64",
                    "Release",
                    bundle_name + ".exe",
                ),
            ]
        )
        files = [
            (vst3, bundle_name + ".vst3"),
            (app, bundle_name + "_x64.exe"),
        ]

    zipname = get_archive_name(projectpath, "win", "demo" if demo == 1 else "full")
    zip_path = os.path.join(projectpath, "build-win", "out", zipname + ".zip")
    pdb_zip_path = os.path.join(projectpath, "build-win", "out", zipname + "-pdbs.zip")

    validate_inputs(files)

    with zipfile.ZipFile(zip_path, mode="w") as zf:
        for f, arcname in files:
            print("adding " + f)
            if os.path.isdir(f):
                add_dir(zf, f, arcname)
            else:
                add_file(zf, f, arcname)
    print("wrote " + zipname)

    pdb_dir = os.path.join(projectpath, "build-win", "pdbs")
    files = []
    if os.path.isdir(pdb_dir):
        files = [
            os.path.join(pdb_dir, filename)
            for filename in os.listdir(pdb_dir)
            if filename.lower().endswith(".pdb")
        ]

    if not files:
        print("No PDB files found in " + pdb_dir)
    else:
        with zipfile.ZipFile(pdb_zip_path, mode="w") as zf:
            for f in files:
                print("adding " + f)
                add_file(zf, f)
        print("wrote " + zipname + "-pdbs")


if __name__ == "__main__":
    main()
