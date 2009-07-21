# Script to build dynamicip executable for windows and upload
# it to our S3 account

import sys
import string
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

SRC_DIR = os.path.dirname(os.path.realpath(__file__))
MISC_UTIL_H_PATH = os.path.join(SRC_DIR, "src", "MiscUtil.h")

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
  bucket = s3PubBucket()
  k = Key(bucket)
  k.key = remote_file_name
  k.set_contents_from_filename(local_file_name, cb=ul_cb)
  k.make_public()

def s3UploadFilePrivate(local_file_name, remote_file_name):
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

# like cmdrun() but throws an exception on failure
def run_cmd_throw(*args):
  cmd = " ".join(args)
  print("\nrun_cmd_throw: '%s'" % cmd)
  cmdproc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  res = cmdproc.communicate()
  errcode = cmdproc.returncode
  if 0 != errcode:
    print("Failed with error code %d" % errcode)
    print("Stdout:")
    print(res[0])
    print("Stderr:")
    print(res[1])
    raise Exception("'%s' failed with error code %d" % (cmd, errcode))
  return (res[0], res[1])

def ensure_file_exists(path):
    if not os.path.exists(path) or not os.path.isfile(path):
        exit_with_error("File '%s' desn't exist" % path)

def ensure_file_doesnt_exist(path):
    if os.path.exists(path):
        exit_with_error("File '%s' already exists and shouldn't. Forgot to update version in Info.plist?" % path)

# version number is in MiscUtil.h, in the following form:
# PROGRAM_VERSION  _T("2.0b7")
def extract_version():
    data = file(MISC_UTIL_H_PATH).read()
    regex = re.compile("PROGRAM_VERSION\s+_T\(\"([^\"]+)\"\)", re.DOTALL | re.MULTILINE)
    m = regex.search(data)
    return m.group(1)

def installer_name(version):
    return "OpenDNS-Updater-%s.exe" % version

def installer_path(version):
    return installer_name(version)

def exe_name():
    return "OpenDNSUpdater.exe"

def exe_path():
    return os.path.join(SRC_DIR, "UpdaterUI", "Release", exe_name())

def pdb_name():
    return "OpenDNSUpdater.pdb"

def pdb_path():
    return os.path.join(SRC_DIR, "UpdaterUI", "Release", pdb_name())

def s3_path(version):
    return "software/win/dynamicipudater/" + version + "/"

def s3_exe_key(version):
    return s3_path(version) + exe_name()

def s3_installer_key(version):
    return s3_path(version) + installer_name(version)

def s3_pdb_key(version):
    return s3_path(version) + pdb_name()

def build():
    run_cmd_throw("devenv", "OpenDNSUpdater.sln", "/Project", "UpdaterUI\UpdaterUI.vcproj", "/ProjectConfig", "Release", "/Rebuild")
 
def nsis(version):
    run_cmd_throw("makensis", "/DVERSION=%s" % version, "installer")

def sign(version):
    run_cmd_throw("signtool", "sign", "/f", "opendns-sign.pfx", "/p", "bulba", "/d", '"OpenDNS Updater"', "/du", '"http://www.opendns.com/support/"', "/t", "http://timestamp.comodoca.com/authenticode", installer_name(version))

def valid_api_key(key):
    valid_chars = "0123456789ABCDEF"
    for c in key:
        if c not in valid_chars:
            return False
    return True

def ensure_valid_api_key():
    api_key_path = os.path.join(SRC_DIR, "src", "ApiKey.h")
    data = readfile(api_key_path)
    regex = re.compile("#define API_KEY \"([^\"]+)\"", re.DOTALL | re.MULTILINE)
    m = regex.search(data)
    apikey = m.group(1)
    if not valid_api_key(apikey):
        exit_with_error("NOT A VALID API KEY: '%s'" % apikey)

def main():
    version = extract_version()
    print("version: '%s'" % version)
    ensure_s3_doesnt_exist(s3_exe_key(version))
    ensure_file_exists(pdb_path())
    ensure_file_exists(exe_path())
    build()
    nsis(version)
    ensure_file_exists(installer_path(version))
    sign(version)
    s3UploadFilePrivate(installer_path(version), s3_installer_key(version))
    s3UploadFilePublic(exe_path(), s3_exe_key(version))
    s3UploadFilePrivate(pdb_path(), s3_pdb_key(version))

if __name__ == "__main__":
    main()
 