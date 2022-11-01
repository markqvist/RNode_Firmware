import os
import json
import hashlib

major_version = None
minor_version = None
target_version = None

file = open("Config.h", "rb")
config_data = file.read().splitlines()
for line in config_data:
    dline = line.decode("utf-8").strip()
    components = dline.split()
    if dline.startswith("#define MAJ_VERS"):
        major_version = "%01d" % ord(bytes.fromhex(dline.split()[2].split("x")[1]))
    if dline.startswith("#define MIN_VERS"):
        minor_version = "%02d" % ord(bytes.fromhex(dline.split()[2].split("x")[1]))

target_version = major_version+"."+minor_version

release_hashes = {}
target_dir = "./Release"
files = os.listdir(target_dir)
for filename in files:
    if os.path.isfile(os.path.join(target_dir, filename)):
        if filename.startswith("rnode_firmware"):
            file = open(os.path.join(target_dir, filename), "rb")
            release_hashes[filename] = {
                "hash": hashlib.sha256(file.read()).hexdigest(),
                "version": target_version
            }

print(json.dumps(release_hashes))