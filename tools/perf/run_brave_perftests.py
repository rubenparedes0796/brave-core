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
from StringIO import StringIO
from zipfile import ZipFile
from urllib import urlopen, urlretrieve
from distutils.dir_util import copy_tree
import argparse

src_dir = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

test_config_file_path = os.path.join(src_dir, 'brave', 'tools', 'perf',
                                     'perftest_config.json')
json_config = {}
with open(test_config_file_path, 'r') as config_file:
  json_config = json.load(config_file)

# Workaround to add our wpr files
page_set_data_dir = os.path.join(src_dir, 'brave', 'tools', 'perf',
                                 'page_sets_data')
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


def RunTest(binary, config, out_dir, extra_args=[]):
  args = [
      sys.executable,
      os.path.join(src_dir, 'testing', 'scripts', 'run_performance_tests.py'),
      os.path.join(src_dir, 'tools', 'perf', 'run_benchmark')
  ]
  args.append('--benchmarks=%s' % config['benchmark'])
  args.append('--browser=exact')
  args.append('--browser-executable=' + binary + '')
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


def DownloadAndUnpackBinary(output_directory, url):
  resp = urlopen(url)
  zipfile = ZipFile(StringIO(resp.read()))
  zipfile.extractall(output_directory)
  return os.path.join(output_directory, 'brave.exe')

def DownloadInstallAndCopy(output_directory, tag):
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)
  url = BRAVE_WIN_INSTALLER_URL % tag
  installer_filename = os.path.join(out_dir, os.pardir, 'installer_%s_tag.exe' % tag)
  urlretrieve(url, installer_filename)
  subprocess.check_call(['start', '/b', '/wait', installer_filename],shell=True)
  try:
    subprocess.check_call(['taskkill.exe', '/f', '/im', 'brave.exe'])
  except subprocess.CalledProcessError:
    logging.info('no brave.exe to kill')

  install_path = os.path.join(os.path.expanduser("~"), 'AppData', 'Local',
                              'BraveSoftware', 'Brave-Browser-Nightly',
                              'Application')
  full_version = None
  copy_tree(install_path, output_directory)
  for file in os.listdir(install_path):
    print (file, tag[2:])
    if file.endswith(tag[2:]):
      assert(full_version == None)
      full_version = file
  assert(full_version != None)
  setup_filename = os.path.join(install_path, full_version, 'Installer',
                                'setup.exe')
  subprocess.check_call([
      'start', '/b', '/wait', setup_filename, '--uninstall',
      '--force-uninstall', '--chrome-sxs'
  ],
                        shell=True)
  shutil.rmtree(install_path, True)
  return os.path.join(out_dir, 'brave.exe')

def ReportToDashboard(product, configuration_name, revision, output_dir):
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
    build_properties['builder_group'] = 'chromium.perf'
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


def TestBinary(product,
               revision,
               binary,
               output_dir,
               args):
  failedLogs = []
  has_failure = False
  for test_config in json_config['tests']:
    benchmark = test_config['benchmark']
    if not args.report_only:
      try:
        RunTest(binary, test_config, output_dir, args.extra_args)
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
      ReportToDashboard(product, args.configuration_name, revision, output_dir)

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
parser.add_argument('--extra_args', action='append', default=[])
parser.add_argument('--overwrite_results', action='store_true')
parser.add_argument('--skip_report', action='store_true')
parser.add_argument('--report_only', action='store_true')
parser.add_argument('--report_on_failure', action='store_true')
args = parser.parse_args()

for tag in args.tags:
  url = GetBraveUrl(tag, args.platform)
  out_dir = os.path.join(args.work_directory, tag)

  binary_path = None
  if not args.report_only:
    if args.use_win_installer:
      binary_path = DownloadInstallAndCopy(out_dir, tag)
    else:
      binary_path = DownloadAndUnpackBinary(out_dir, url)


  if args.overwrite_results and not args.report_only:
    shutil.rmtree(os.path.join(out_dir, 'results'), True)

  [binary_success,
   binary_logs] = TestBinary('brave',
                             'refs/tags/' + tag,
                             binary_path,
                             os.path.join(out_dir, 'results'),
                             args)
  if not binary_success:
    has_failure = True
    failedLogs.extend(binary_logs)

print('\n\nSummary:\n')
if has_failure:
  print('\n'.join(failedLogs))
  print('Got %d errors!' % len(failedLogs))
else:
  print('OK')
