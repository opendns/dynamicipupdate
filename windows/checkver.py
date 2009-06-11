import sys
import os

# make sure version number embedded in the code is the same as version
# number given as argument. to be used as a verification check in build
# script

misc_util_h = os.path.join("src", "MiscUtil.h")
files = [misc_util_h]

def string_in_file(path, s):
    filedata = file(path).read()
    return s in filedata

def usage():
    print("Usage: checkver.py ${version}")

def main():
    if len(sys.argv) != 2:
        usage()
        return 1
    version = sys.argv[1]
    for f in files:
        if not string_in_file(f, version):
          print("%s cannot be found in file %s. Probably didn't update version number." % (version, f))
          return 1
    return 0
    return 0

if __name__ == "__main__":
    exitcode = main()
    sys.exit(exitcode)
