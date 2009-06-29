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
Release build script designed to automate as much of the proces as possible
and minimize errors.

Pushing an update to mac client is involved. Files that must be changed:

* Info.plist
* conf.php and mac-ipupdater-relnotes-$ver.html 
* IpUpdaterAppCast.xml
  (update pubDate, sparkle:version and sparkle:shortVersionString)

Checklist for pushing a new release:
* edit Info.plist to set new version
* create mac-ipupdater-relnotes-$ver.html, check it in and deploy it
* run this script
* verify it made the right changes to IpUpdaterAppCast.xml
* checkin and deploy the binary to the website
* update conf.php to account for new version, check it in and deploy to website
* checkin and deploy IpUpdaterCast.xml
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
    return "OpenDNS-Dynamic-IP-Mac-%s.zip" % version

def zip_path(version):
    return os.path.join(RELEASE_BUILD_DIR, zip_name(version))

def zip_path_on_website(version):
    return os.path.join(WEBSITE_DESKTOP_DIR, zip_name(version))

def relnotes_path(version):
    return os.path.join(WEBSITE_DESKTOP_DIR, "mac-ipupdater-relnotes-%s.html" % version)

# update sparkle:releaseNotesLink, sparkle:version and pubDate element
def update_app_cast(path, version, length):
    appcast = readfile(path)
    newver = "sparkle:version=\"%s\"" % version
    appcast = re.sub("sparkle:version=\"[^\"]+\"", newver, appcast)
    pubdate = time.strftime("%a, %d %b %y %H:%M:%S %z", time.gmtime())
    newpubdate = "<pubDate>%s</pubDate>" % pubdate
    appcast = re.sub('<pubDate>.?</pubDate>', newpubdate, appcast)
    url = "http://www.opendns.com/desktop/mac-ipupdater-relnotes-%s.html" % version
    newrelnotes = "<sparkle:releaseNotesLink>%s</sparkle:releaseNotesLink>" % url
    appcast = re.sub('<sparkle:releaseNotesLink>.+</sparkle:releaseNotesLink>', newrelnotes, appcast)
    newlen = 'length="%d"' % length
    appcast = re.sub("length=\"[^\"]?\"", newlen, appcast)
    writefile(path, appcast)
    print("Updates '%s', make sure to check it in" % path)

def build_and_zip(version):
    os.chdir(SRC_DIR)
    print("Cleaning release target...")
    xcodeproj = "OpenDNS Updater.xcodeproj"
    run_cmd_throw("xcodebuild", "-project", xcodeproj, "-configuration", "Release", "clean");
    print("Building release target...")
    (out, err) = run_cmd_throw("xcodebuild", "-project", xcodeproj, "-configuration", "Release", "-target", "OpenDNS Updater")
    ensure_dir_exists(RELEASE_BUILD_DIR)
    os.chdir(RELEASE_BUILD_DIR)
    (out, err) = run_cmd_throw("zip", "-9", "-r", zip_name(version), "BTerm.app")

def main():
    ensure_dir_exists(WEBSITE_DESKTOP_DIR)
    ensure_dir_exists(APPENGINE_SRC_DIR)
    ensure_file_exists(INFO_PLIST_PATH)
    ensure_file_exists(APP_CAST_PATH)
    version = extract_version_from_plist(INFO_PLIST_PATH)
    ensure_valid_version(version)
    ensure_file_doesnt_exist(zip_path_on_website(version))
    ensure_file_exists(relnotes_path(version))

    build_and_zip(version)
    ensure_file_exists(zip_path(version))

    src = zip_path(version)
    dst = zip_path_on_website(version)
    shutil.copyfile(src, dst)
    print("Don't forget to checkin and deploy '%s'" % dst)
    length = get_file_size(zip_path(version))
    update_app_cast(APP_CAST_PATH, version, length)
    print("Don't forget to checkin and deploy '%s'" % APP_CAST_PATH)

if __name__ == "__main__":
    main()
