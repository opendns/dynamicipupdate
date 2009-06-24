#!/usr/bin/env python

import sys
import os.path
import re
import time

"""
Release build script designed to automate as much of the proces as possible
and minimize errors.

Pushing an update to mac client is involved. Files that must be changed:

* Info.plist
* conf.php and mac-ipupdater-relnotes-$ver.html in 
  svn+ssh://svn.office.opendns.com/var/lib/svn/projects/opendns-website/trunk/desktop/
* IpUpdaterAppCast.xml in 
  svn+ssh://svn.office.opendns.com/var/lib/svn/projects/appengine-opendnsupdate/trunk
  (update pubDate, sparkle:version and sparkle:shortVersionString)

To build the new version:
* set the right version in Info.plist
* check that this is a new version (*.zip with this name doesn't exist yet)
* build a new *.zip file, uniquely named for this release (by embedding
   version in the name)
* checkin *.zip file
* update release notes (manually)
*  update appcast.xml (programmatically, this script does that)
* update conf.php (manually)

  # To push the new version:
  # * push relase notes at http://www.opendns.com/desktop/mac-ipupdater-relnotes.html
  # * push appcast.xml in opendnsudpate.appspot.com
  # * push 

"""


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
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

def main():
    ensure_dir_exists(WEBSITE_DESKTOP_DIR)
    ensure_dir_exists(APPENGINE_SRC_DIR)
    ensure_file_exists(INFO_PLIST_PATH)
    ensure_file_exists(APP_CAST_PATH)
    version = extract_version_from_plist(INFO_PLIST_PATH)
    ensure_valid_version(version)
    ensure_file_doesnt_exist(zip_path(version))
    ensure_file_exists(relnotes_path(version))

    length = 1234
    update_app_cast(APP_CAST_PATH, version, length)

if __name__ == "__main__":
    main()
