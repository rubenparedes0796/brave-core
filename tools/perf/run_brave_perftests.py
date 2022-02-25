#!/usr/bin/env python
# Copyright (c) 2021 The Brave Authors. All rights reserved.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# you can obtain one at http://mozilla.org/MPL/2.0/.
"""A tool run telemetry perftests and report the results to dashboard


The tool:
1. Download a proper binary.
2. Run a subset of telemetry perftests from the config.
3. Report the result to brave-perf-dashboard.appspot.com
The tools should be run on a special prepared hardware/OS to minimize
the result flakiness.

Example usage:
 vpython run_brave_perftests.py --tags v1.36.23
                                --platform=win32-x64
                                --configuration_name=test-agent
                                --work_directory=e:\work\tmp\perf0\
                                --overwrite_results
                                --extra_args="--use-live-sites"
"""
import os
import subprocess
import json
import sys
import logging
import shutil
import re
from StringIO import StringIO
from zipfile import ZipFile
from urllib2 import urlopen
from distutils.dir_util import copy_tree
import argparse

src_dir = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

perf_path = os.path.join(src_dir, 'brave', 'tools', 'perf')

test_config_file_path = os.path.join(perf_path, 'perftest_config.json')

json_config = {}
with open(test_config_file_path, 'r') as config_file:
  json_config = json.load(config_file)

# Workaround to add our wpr files
page_set_data_dir = os.path.join(perf_path, 'page_sets_data')
chromium_page_set_data_dir = os.path.join(src_dir, 'tools', 'perf', 'page_sets',
                                          'data')
for item in os.listdir(page_set_data_dir):
  shutil.copy(os.path.join(page_set_data_dir, item), chromium_page_set_data_dir)


# Returns pair [revision_number, sha1]. revision_number is a number "primary"
# commits from the begging to `revision`.
# Use this to get the commit from a revision number:
# git rev-list --topo-order --first-parent --reverse origin/master
# | head -n <rev_num> | tail -n 1 | git log -n 1 --stdin
def GetRevisionNumberAndHash(revision):
  brave_dir = os.path.join(src_dir, 'brave')
  subprocess.check_call(['git', 'fetch', 'origin', revision], cwd=brave_dir)
  hash = subprocess.check_output(['git', 'rev-parse', 'FETCH_HEAD'],
                                 cwd=brave_dir).rstrip()
  rev_number_args = [
      'git', 'rev-list', '--topo-order', '--first-parent', '--count',
      'FETCH_HEAD'
  ]
  rev_number = subprocess.check_output(rev_number_args, cwd=brave_dir).rstrip()
  return [rev_number, hash]


def RunTest(binary, config, out_dir, is_ref, extra_args=[]):
  args = [
      sys.executable,
      os.path.join(src_dir, 'testing', 'scripts', 'run_performance_tests.py'),
      os.path.join(src_dir, 'tools', 'perf', 'run_benchmark')
  ]
  benchmark = config['benchmark']
  if is_ref:
    benchmark += '.reference'
  args.append('--benchmarks=%s' % benchmark)
  args.append('--browser=exact')
  args.append('--browser-executable=%s' % binary)
  args.append('--isolated-script-test-output=%s' % out_dir + '\\' +
              config['benchmark'] + '\\output.json')
  args.append('--pageset-repeat=%d' % config['pageset_repeat'])
  if 'stories' in config:
    for story in config['stories']:
      args.append('--story=' + story)

  args.extend(extra_args)

  subprocess.check_call(args, cwd=os.path.join(src_dir, 'tools', 'perf'))


BRAVE_URL = 'https://github.com/brave/brave-browser/releases/download/%s/brave-%s-%s.zip'
BRAVE_WIN_INSTALLER_URL = 'https://github.com/brave/brave-browser/releases/download/%s/BraveBrowserStandaloneSilentNightlySetup.exe'


def GetBraveUrl(tag, platform):
  return BRAVE_URL % (tag, tag, platform)


def ParseVersion(version_string):
  return map(int, version_string.split("."))


def GetNearestChromiumVersionAndUrl(tag):
  chrome_versions = {}
  with open(os.path.join(perf_path, 'chrome_releases.json'),
            'r') as config_file:
    chrome_versions = json.load(config_file)

  subprocess.check_call(['git', 'fetch', 'origin', r'refs/tags/*:refs/tags/*'],
                        cwd=os.path.join(src_dir, 'brave'))
  package_json = json.loads(
      subprocess.check_output(
          ['git', 'show', '%s:package.json' % tag],
          cwd=os.path.join(src_dir, 'brave')))
  requested_version = package_json['config']['projects']['chrome']['tag']

  parsed_requested_version = ParseVersion(requested_version)
  best_candidate = None
  for version in chrome_versions:
    parsed_version = ParseVersion(version)
    if parsed_version[0] == parsed_requested_version[
        0] and parsed_version >= parsed_requested_version:
      if not best_candidate or best_candidate > parsed_version:
        best_candidate = parsed_version

  if best_candidate:
    string_version = '.'.join(map(str, best_candidate))
    logging.info("Use chromium version %s for requested %s " %
                 (best_candidate, requested_version))
    return string_version, chrome_versions[string_version]['url']

  logging.error("No chromium version found for %s" % requested_version)
  return None, None

def DownloadArchiveAndUnpack(output_directory, url):
  resp = urlopen(url)
  zipfile = ZipFile(StringIO(resp.read()))
  zipfile.extractall(output_directory)
  return os.path.join(output_directory, 'brave.exe')


def DownloadWinInstallerAndExtract(out_dir, url, expected_install_path, binary):
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)
  installer_filename = os.path.join(out_dir, os.pardir, 'temp_installer.exe')
  logging.info('Downloading %s' % url)
  f = urlopen(url)
  data = f.read()
  with open(installer_filename, "wb") as output_file:
    output_file.write(data)
  logging.info('Run installer %s' % installer_filename)
  subprocess.check_call([installer_filename, '--chrome-sxs'])
  try:
    subprocess.check_call(['taskkill.exe', '/f', '/im', binary])
  except subprocess.CalledProcessError:
    logging.info('failed to kill %s' % binary)

  if not os.path.exists(expected_install_path):
    raise RuntimeError('No files found in %s' % expected_install_path)

  full_version = None
  logging.info('Copy files to %s' % out_dir)
  copy_tree(expected_install_path, out_dir)
  for file in os.listdir(expected_install_path):
    #TODO: check brave tag? file.endswith(tag[2:])
    if re.search("\d.\d.\d.\d", file):
      assert (full_version == None)
      full_version = file
  assert (full_version != None)
  logging.info('Detected version %s' % full_version)
  setup_filename = os.path.join(expected_install_path, full_version,
                                'Installer', 'setup.exe')
  logging.info('Run uninstall %s' % setup_filename)
  try:
    subprocess.check_call(
        [setup_filename, '--uninstall', '--force-uninstall', '--chrome-sxs'])
  except subprocess.CalledProcessError:
    logging.info('setup.exe returns non-zero code.')

  shutil.rmtree(expected_install_path, True)
  return os.path.join(out_dir, binary)


def DownloadBraveWinInstallerAndExtract(out_dir, tag):
  install_path = os.path.join(os.path.expanduser("~"), 'AppData', 'Local',
                              'BraveSoftware', 'Brave-Browser-Nightly',
                              'Application')
  url = BRAVE_WIN_INSTALLER_URL % tag
  return DownloadWinInstallerAndExtract(out_dir, url, install_path, 'brave.exe')


def DownloadChromeWinInstallerAndExtract(out_dir, tag):
  [chromium_version, url] = GetNearestChromiumVersionAndUrl(tag)
  if not url:
    raise RuntimeError("Fail to find nearest chromium binary for %s" % tag)

  install_path = os.path.join(os.path.expanduser("~"), 'AppData', 'Local',
                              'Google', 'Chrome SxS', 'Application')
  return DownloadWinInstallerAndExtract(out_dir, url, install_path,
                                        'chrome.exe')


def ReportToDashboard(product, is_ref, configuration_name, revision,
                      output_dir):
  args = [
      sys.executable,
      os.path.join(src_dir, 'tools', 'perf', 'process_perf_results.py')
  ]
  args.append('--configuration-name=%s' % configuration_name)
  args.append('--task-output-dir=%s' % output_dir)
  args.append('--output-json=%s' % os.path.join(output_dir, 'results.json'))

  [revision_number, git_hash] = GetRevisionNumberAndHash(revision)
  build_properties = {}
  build_properties['bot_id'] = 'test_bot'
  if product == 'brave':
    build_properties['builder_group'] = 'brave.perf'
  elif product == 'chromium':
    build_properties[
        'builder_group'] = 'brave.perf' if is_ref else 'chromium.perf'
  else:
    raise RuntimeError('bad product name ' + product)

  build_properties['parent_builder_group'] = 'chromium.linux'
  build_properties['parent_buildername'] = 'Linux Builder'
  build_properties['recipe'] = 'chromium'
  build_properties['slavename'] = 'test_bot'
  build_properties['buildername'] = 'test_builder'
  build_properties['buildnumber'] = '0001'
  build_properties[
      'got_revision_cp'] = 'refs/heads/main@{#%s}' % revision_number
  build_properties['got_v8_revision'] = revision_number
  build_properties['got_webrtc_revision'] = revision_number
  build_properties['git_revision'] = git_hash
  build_properties_serialized = json.dumps(build_properties)
  args.append('--build-properties=%s' % build_properties_serialized)
  subprocess.check_call(args)


def TestBinary(product, revision, binary, output_dir, is_ref, args):
  failedLogs = []
  has_failure = False
  for test_config in json_config['tests']:
    benchmark = test_config['benchmark']
    if not args.report_only:
      try:
        RunTest(binary, test_config, output_dir, is_ref, args.extra_args)
      except subprocess.CalledProcessError:
        has_failure = True
        error = 'Test case %s failed on revision %s' % (benchmark, revision)
        error += '\nLogs: ' + os.path.join(output_dir, benchmark, benchmark,
                                           'benchmark_log.txt')
        logging.error(error)
        failedLogs.append(error)
  try:
    if has_failure and not args.report_on_failure:
      error = 'Skip reporting because errors for binary ' + binary
      logging.error(error)
      failedLogs.append(error)
    elif args.skip_report:
      logging.info('skip reporting because report==False')
    else:
      ReportToDashboard(product, is_ref, args.configuration_name, revision,
                        output_dir)

  except subprocess.CalledProcessError:
    has_failure = True
    error = 'Reporting ' + revision + ' failed'
    logging.error(error)
    failedLogs.append(error)
  return [not has_failure, failedLogs]


failedLogs = []
has_failure = False

parser = argparse.ArgumentParser()
parser.add_argument('--tags',
                    required=True,
                    action='append',
                    help='The tags to test')
parser.add_argument('--work_directory', required=True, type=str)
parser.add_argument('--configuration_name', required=True, type=str)
parser.add_argument('--platform', required=True, type=str)
parser.add_argument('--use_win_installer', action='store_true')
parser.add_argument('--chromium', action='store_true')
parser.add_argument('--extra_args', action='append', default=[])
parser.add_argument('--overwrite_results', action='store_true')
parser.add_argument('--skip_report', action='store_true')
parser.add_argument('--report_only', action='store_true')
parser.add_argument('--report_on_failure', action='store_true')
parser.add_argument('--verbose', action='store_true')
args = parser.parse_args()

log_level = logging.DEBUG if args.verbose else logging.INFO
log_format = '%(asctime)s - %(levelname)s - %(funcName)s: %(message)s'
logging.basicConfig(level=log_level, format=log_format)

for tag in args.tags:
  url = GetBraveUrl(tag, args.platform)
  out_dir = os.path.join(args.work_directory, tag)

  binary_path = None
  product = 'chromium' if args.chromium else 'brave'
  is_ref = args.chromium

  if not args.report_only:
    if args.chromium:
      binary_path = DownloadChromeWinInstallerAndExtract(out_dir, tag)
    elif args.use_win_installer:
      binary_path = DownloadBraveWinInstallerAndExtract(out_dir, tag)
    else:
      binary_path = DownloadArchiveAndUnpack(out_dir, url)

  if args.overwrite_results and not args.report_only:
    shutil.rmtree(os.path.join(out_dir, 'results'), True)

  [binary_success,
   binary_logs] = TestBinary(product, 'refs/tags/' + tag, binary_path,
                             os.path.join(out_dir, 'results'), is_ref, args)
  if not binary_success:
    has_failure = True
    failedLogs.extend(binary_logs)

logging.info('\nSummary:\n')
if has_failure:
  logging.error('\n'.join(failedLogs))
  logging.error('Got %d errors!' % len(failedLogs))
else:
  logging.info('OK')
