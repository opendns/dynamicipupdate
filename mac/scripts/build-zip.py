#!/usr/bin/env python

import sys
import os
import os.path
import re
import time
import subprocess
import stat 
import shutil

"""
Builds a zip of release version.
"""

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.realpath(os.path.join(SCRIPT_DIR, ".."))
RELEASE_BUILD_DIR = os.path.join(SRC_DIR, "build", "Release")
INFO_PLIST_PATH = os.path.realpath(os.path.join(SCRIPT_DIR, "..", "Info.plist"))
WEBSITE_DESKTOP_DIR = os.path.realpath(os.path.join(SCRIPT_DIR, "..", "..", "..", "website", "desktop"))
APPENGINE_SRC_DIR = os.path.realpath(os.path.join(SCRIPT_DIR, "..", "..", "..", "appengine-opendnsupdate"))
APP_CAST_PATH = os.path.join(APPENGINE_SRC_DIR, "IpUpdaterAppCast.xml")

def exit_with_error(s):
    print(s)
    sys.exit(1)

def ensure_dir_exists(path):
    if not os.path.exists(path) or not os.path.isdir(path):
        exit_with_error("Directory '%s' desn't exist" % path)

def ensure_file_exists(path):
    if not os.path.exists(path) or not os.path.isfile(path):
        exit_with_error("File '%s' desn't exist" % path)

def ensure_file_doesnt_exist(path):
    if os.path.exists(path):
        exit_with_error("File '%s' already exists and shouldn't. Forgot to update version in Info.plist?" % path)

def readfile(path):
    fo = open(path)
    data = fo.read()
    fo.close()
    return data

def writefile(path, data):
    fo = open(path, "w")
    fo.write(data)
    fo.close()

def get_file_size(filename):
    st = os.stat(filename)
    return st[stat.ST_SIZE]

def run_cmd_throw(*args):
    cmd = " ".join(args)
    print("Running '%s'" % cmd)
    cmdproc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    res = cmdproc.communicate()
    errcode = cmdproc.returncode
    if 0 != errcode:
        print "Failed with error code %d" % errcode
        print "Stdout:"
        print res[0]
        print "Stderr:"
        print res[1]
        raise Exception("'%s' failed with error code %d" % (cmd, errcode))
    return (res[0], res[1])

# a really ugly way to extract version from Info.plist
def extract_version_from_plist(plist_path):
    plist = readfile(plist_path)
    #print(plist)
    regex = re.compile("CFBundleVersion</key>(.+?)<key>", re.DOTALL | re.MULTILINE)
    m = regex.search(plist)
    version_element = m.group(1)
    #print("version_element: '%s'" % version_element)
    regex2 = re.compile("<string>(.+?)</string>")
    m = regex2.search(version_element)
    version = m.group(1)
    version = version.strip()
    #print("version: '%s'" % version)
    return version

# build version is either x.y or x.y.z
def ensure_valid_version(version):
    m = re.match("\d+\.\d+", version)
    if m: return
    m = re.match("\d+\.\d+\.\d+", version)
    if m: return
    print("version ('%s') should be in format: x.y or x.y.z" % version)
    sys.exit(1)

def zip_name(version):
    return "OpenDNS-Updater-Mac-%s.zip" % version

def zip_path(version):
    return os.path.join(RELEASE_BUILD_DIR, zip_name(version))

def build_and_zip(version):
    os.chdir(SRC_DIR)
    print("Cleaning release target...")
    xcodeproj = "OpenDNS Updater.xcodeproj"
    run_cmd_throw("xcodebuild", "-project", xcodeproj, "-configuration", "Release", "clean");
    print("Building release target...")
    (out, err) = run_cmd_throw("xcodebuild", "-project", xcodeproj, "-configuration", "Release", "-target", "OpenDNS Updater")
    ensure_dir_exists(RELEASE_BUILD_DIR)
    os.chdir(RELEASE_BUILD_DIR)
    (out, err) = run_cmd_throw("zip", "-9", "-r", zip_name(version), "OpenDNS Updater.app")

def main():
    version = extract_version_from_plist(INFO_PLIST_PATH)
    print("Building mac updater version '%s'" % version)
    ensure_valid_version(version)

    build_and_zip(version)
    ensure_file_exists(zip_path(version))
    print("Built '%s'" % zip_path(version))

if __name__ == "__main__":
    main()
