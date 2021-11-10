#!/usr/bin/env python

import os, glob

for src_name in glob.glob("*.c"):
  binary_name = src_name[:-2]

  if os.path.exists(binary_name):
    os.remove(binary_name)

  build_str = "gcc -static -O0 -g -o %s %s" % (binary_name, src_name)
  os.system(build_str); print build_str
