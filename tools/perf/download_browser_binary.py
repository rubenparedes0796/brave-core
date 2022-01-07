#!/usr/bin/env python
# Copyright (c) 2021 The Brave Authors. All rights reserved.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# you can obtain one at http://mozilla.org/MPL/2.0/.

"""A tool to download release browser binaries
"""

from StringIO import StringIO
from zipfile import ZipFile
from urllib import urlopen

import logging
import os.path
import argparse


BRAVE_URL = 'https://github.com/brave/brave-browser/releases/download/%s/brave-%s-%s.zip'


def get_brave_url(tag, platform):
  return BRAVE_URL % (tag, tag, platform)


def download_and_unpack(output_directory, url):
  resp = urlopen(url)
  zipfile = ZipFile(StringIO(resp.read()))
  zipfile.extractall(output_directory)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--tag',
      required=True,
      type=str,
      help='The tag to download')
  parser.add_argument(
      '--product',
      default='brave',
      type=str,
      help='{brave, chrome}')
  parser.add_argument(
      '--directory',
      required=True,
      type=str,
      help='Directory to store')
  parser.add_argument(
      '--platform',
      required=True,
      type=str,
      help='{win32-x64, darwin-x64, ..}')
  args = parser.parse_args()

  url = None
  if args.product == 'brave':
    url = get_brave_url(args.tag, args.platform)
  out_directory = os.path.join(args.directory, args.tag)
  logging.info('Downloading' + url)
  download_and_unpack(out_directory, url)
  logging.info('Done')

if __name__ == '__main__':
  sys.exit(main())
