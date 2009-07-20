#!/usr/bin/env python

import sys
import os
import os.path
import re
import time
import subprocess
import stat 
import shutil

try:
    import boto.s3
    from boto.s3.key import Key
except:
    print("You need boto library (http://code.google.com/p/boto/)")
    print("svn checkout http://boto.googlecode.com/svn/trunk/ boto")
    print("cd boto; python setup.py install")
    raise

try:
    import awscreds
except:
    print "awscreds.py file needed with access and secret globals for aws access"
    sys.exit(1)

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
BUNDLE_DIR = os.path.join(RELEASE_BUILD_DIR, "OpenDNS Updater.app")
INFO_PLIST_PATH = os.path.realpath(os.path.join(SCRIPT_DIR, "..", "Info.plist"))
WEBSITE_DESKTOP_DIR = os.path.realpath(os.path.join(SCRIPT_DIR, "..", "..", "..", "website", "desktop"))
APPENGINE_SRC_DIR = os.path.realpath(os.path.join(SCRIPT_DIR, "..", "..", "..", "appengine-opendnsupdate"))
APP_CAST_PATH = os.path.join(APPENGINE_SRC_DIR, "IpUpdaterAppCast.xml")

S3_BUCKET = "opendns"
g_s3conn = None

def s3connection():
    global g_s3conn
    if g_s3conn is None:
        g_s3conn = boto.s3.connection.S3Connection(awscreds.access, awscreds.secret, True)
    return g_s3conn

def s3PubBucket(): return s3connection().get_bucket(S3_BUCKET)

def ul_cb(sofar, total):
    print("So far: %d, total: %d" % (sofar , total))

def s3UploadFilePublic(local_file_name, remote_file_name):
    print("Uploading public '%s' as '%s'" % (local_file_name, remote_file_name))
    bucket = s3PubBucket()
    k = Key(bucket)
    k.key = remote_file_name
    k.set_contents_from_filename(local_file_name, cb=ul_cb)
    k.make_public()

def s3UploadFilePrivate(local_file_name, remote_file_name):
    print("Uploading private '%s' as '%s'" % (local_file_name, remote_file_name))
    bucket = s3PubBucket()
    k = Key(bucket)
    k.key = remote_file_name
    k.set_contents_from_filename(local_file_name, cb=ul_cb)

def s3UploadDataPublic(data, remote_file_name):
    bucket = s3PubBucket()
    k = Key(bucket)
    k.key = remote_file_name
    k.set_contents_from_string(data)
    k.make_public()

def s3KeyExists(key):
    k = Key(s3PubBucket(), key)
    return k.exists()

def ensure_s3_doesnt_exist(key):
    if s3KeyExists(key):
        print("'%s' already exists in s3. Forgot to update version number?" % key)
        sys.exit(1)

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

def zip_path_on_website(version):
    return os.path.join(WEBSITE_DESKTOP_DIR, zip_name(version))

def dmg_name(version):
    return "OpenDNS-Updater-Mac-%s.dmg" % version

def dmg_path(version):
    return os.path.join(SCRIPT_DIR, dmg_name(version))

def dmg_path_on_website(version):
    return os.path.join(WEBSITE_DESKTOP_DIR, dmg_name(version))

def dmg_path_on_s3(version):
    return "software/mac/dynamicupdate/" + version + "/" + dmg_name(version)

SYMS_NAME = "OpenDNS Updater.app.dSYM"

def syms_path():
    return os.path.join(RELEASE_BUILD_DIR, SYMS_NAME)

SYMS_ZIP_NAME = "OpenDNS Updater.app.dSYM.zip"

def syms_zip_path():
    return os.path.join(RELEASE_BUILD_DIR, SYMS_ZIP_NAME)

def syms_zip_path_on_s3(version):
    return "software/mac/dynamicupdate/" + version + "/" + SYMS_ZIP_NAME

def relnotes_path(version):
    return os.path.join(WEBSITE_DESKTOP_DIR, "mac-ipupdater-relnotes-%s.html" % version)

# update sparkle:releaseNotesLink, sparkle:version and pubDate element
def update_app_cast(path, version, length):
    appcast = readfile(path)
    newver = "sparkle:version=\"%s\"" % version
    appcast = re.sub("sparkle:version=\"[^\"]+\"", newver, appcast)

    pubdate = time.strftime("%a, %d %b %y %H:%M:%S %z", time.gmtime())
    newpubdate = "<pubDate>%s</pubDate>" % pubdate
    appcast = re.sub('<pubDate>.*</pubDate>', newpubdate, appcast)

    url = "http://www.opendns.com/desktop/mac-ipupdater-relnotes-%s.html" % version
    newrelnotes = "<sparkle:releaseNotesLink>%s</sparkle:releaseNotesLink>" % url
    appcast = re.sub('<sparkle:releaseNotesLink>.*</sparkle:releaseNotesLink>', newrelnotes, appcast)

    newlen = 'length="%d"' % length
    appcast = re.sub("length=\"[^\"]*\"", newlen, appcast)
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
    (out, err) = run_cmd_throw("zip", "-9", "-r", zip_name(version), "OpenDNS Updater.app")

# hdiutil attach returns to stdout sth. that looks like:
"""
/dev/disk1        	                      	/Volumes/OpenDNS Updater"""
# This function parses this output and returns dev_file (/dev/disk1) and vol_path
# (/Volumes/OpenDNS Updater)
def parse_hdiutil_attach_out(txt):
    dev_file = None
    vol_path = None
    lines = txt.split("\n")
    for l in lines:
        parts = l.split()
        if len(parts) < 2:
            continue
        #print l
        dev_file = parts[0]
        assert dev_file.startswith("/dev/disk")
        vol_path = " ".join(parts[1:])
        assert vol_path.startswith("/Volumes/OpenDNS")
        return (dev_file, vol_path)
    assert 0

def create_dmg(version):
    sparse_src = os.path.join(SCRIPT_DIR, "template.sparseimage.bz2")
    sparse_tmp_src = os.path.join(SCRIPT_DIR, "template-tmp.sparseimage.bz2")
    sparse_tmp = os.path.join(SCRIPT_DIR, "template-tmp.sparseimage")
    if os.path.exists(sparse_tmp_src):
        os.remove(sparse_tmp_src)
    if os.path.exists(sparse_tmp):
        os.remove(sparse_tmp)
    shutil.copy(sparse_src, sparse_tmp_src)
    run_cmd_throw("bzip2", "-d", sparse_tmp_src)

    ensure_file_exists(sparse_tmp)

    (hdiutil_out, hdutil_err) = run_cmd_throw("hdiutil", "attach", sparse_tmp)
    #print("hdiutil_out:\n%s" % hdiutil_out)
    (dev_file, vol_path) = parse_hdiutil_attach_out(hdiutil_out)
    #print("Copying files to dmg")

    #shutil.copytree(BUNDLE_DIR, os.path.join(vol_path, "OpenDNS Updater.app"), True)
    src_dir = os.path.join(BUNDLE_DIR, "Contents")
    dst_dir = os.path.join(vol_path, "OpenDNS Updater.app", "Contents")
    run_cmd_throw("ditto", src_dir, dst_dir)

    #os.symlink("/Applications", os.path.join(vol_path, "Applications"))
    run_cmd_throw("hdiutil", "detach", dev_file)
    #hdiutil convert "${PRODUCT}.sparseimage" -format 'UDBZ' -o "${PRODUCT}.dmg"
    run_cmd_throw("hdiutil", "convert", "-quiet", sparse_tmp, "-format", "UDZO", "-imagekey", "zlib-level=9", "-o", dmg_path(version))
    #run_cmd_throw("hdiutil", "unflatten", dmg_path())

def create_syms_zip():
    os.chdir(RELEASE_BUILD_DIR)
    (out, err) = run_cmd_throw("zip", "-9", "-r", SYMS_ZIP_NAME, SYMS_NAME)


def build_and_dmg(version):
    os.chdir(SRC_DIR)
    print("Cleaning release target...")
    xcodeproj = "OpenDNS Updater.xcodeproj"
    run_cmd_throw("xcodebuild", "-project", xcodeproj, "-configuration", "Release", "clean");
    print("Building release target...")
    (out, err) = run_cmd_throw("xcodebuild", "-project", xcodeproj, "-configuration", "Release", "-target", "OpenDNS Updater")
    ensure_dir_exists(RELEASE_BUILD_DIR)
    create_dmg(version)

def main():
    test = "-test" in sys.argv or "--test" in sys.argv
    if test: print("Running in test mode. Will build but not upload")
    ensure_dir_exists(WEBSITE_DESKTOP_DIR)
    ensure_dir_exists(APPENGINE_SRC_DIR)
    ensure_file_exists(INFO_PLIST_PATH)
    ensure_file_exists(APP_CAST_PATH)
    version = extract_version_from_plist(INFO_PLIST_PATH)
    print("Building mac updater version '%s'" % version)
    ensure_valid_version(version)
    if not test:
        ensure_file_doesnt_exist(dmg_path_on_website(version))
        ensure_s3_doesnt_exist(dmg_path_on_s3(version))
    ensure_file_exists(relnotes_path(version))

    build_and_dmg(version)
    create_syms_zip()
    ensure_file_exists(dmg_path(version))
    ensure_file_exists(syms_zip_path())

    # don't upload in test mode
    if test: return

    s3UploadFilePublic(dmg_path(version), dmg_path_on_s3(version))
    s3UploadFilePrivate(syms_zip_path(), syms_zip_path_on_s3(version))

    src = dmg_path(version)
    dst = dmg_path_on_website(version)
    shutil.copyfile(src, dst)
    print("Don't forget to checkin and deploy '%s'" % dst)
    length = get_file_size(dmg_path(version))
    update_app_cast(APP_CAST_PATH, version, length)
    print("Don't forget to checkin and deploy '%s'" % APP_CAST_PATH)

if __name__ == "__main__":
    main()
