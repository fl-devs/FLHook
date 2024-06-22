from shutil import copy2
from pathlib import Path
import os
from argparse import ArgumentParser, ArgumentError
from sys import exit

copy_path = os.environ.get("FLHOOK_COPY_PATH") or ""
if copy_path == "":
    print("FLHOOK_COPY_PATH not defined, not running post build")
    exit()

copied_files = []
bin_dir = None

if __name__ != "__main__":
    raise NotImplementedError("This file should not be imported")

parser = ArgumentParser()
parser.add_argument("--bin-dir", "-o", dest="bin", type=str, required=True)
args = parser.parse_args()

if not os.path.isdir(args.bin):
    raise ArgumentError(argument=args.bin, message="Provided Bin Directory was not a valid directory")


def process_folder(files, destination):
    for src in files:
        # Don't copy the same file multiple times
        file_name = Path(src).stem
        if file_name in copied_files:
            continue

        source_path = str(os.path.join(args.bin, src))
        print("Copying {0} to {1}".format(src, destination))
        if os.path.isdir(source_path):
            out_dir = os.path.join(destination, file_name)
            if not os.path.exists(out_dir):
                os.mkdir(out_dir)

            process_folder([os.path.join(source_path, d) for d in os.listdir(source_path)], out_dir)
        else:
            copy2(source_path, destination)
            copied_files.append(file_name)


print("Copying files from {0} to {1}".format(args.bin, copy_path))
process_folder([os.path.join(args.bin, d) for d in os.listdir(args.bin)], copy_path)