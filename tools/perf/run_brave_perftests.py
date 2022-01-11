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
from urllib import urlopen
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
  rev_number = subprocess.check_output(
      ['git', 'rev-list', '--count', 'FETCH_HEAD'], cwd=brave_dir).rstrip()
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


def GetBraveUrl(tag, platform):
  return BRAVE_URL % (tag, tag, platform)


def DownloadAndUnpackBinary(output_directory, url):
  resp = urlopen(url)
  zipfile = ZipFile(StringIO(resp.read()))
  zipfile.extractall(output_directory)


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
               configuration_name,
               revision,
               binary,
               output_dir,
               extra_args,
               skip_report,
               skip_run):
  failedLogs = []
  has_failure = False
  for test_config in json_config['tests']:
    benchmark = test_config['benchmark']
    if not skip_run:
      try:
        RunTest(binary, test_config, output_dir, extra_args)
      except subprocess.CalledProcessError:
        has_failure = True
        error = 'Test case %s failed on revision %s' % (benchmark, revision)
        error += '\nLogs: ' + os.path.join(output_dir, benchmark, benchmark,
                                           'benchmark_log.txt')
        logging.error(error)
        failedLogs.append(error)
  try:
    if has_failure:
      error = 'Skip reporting because errors for binary ' + binary
      logging.error(error)
      failedLogs.append(error)
    elif skip_report:
      logging.info('skip reporting because report==False')
    else:
      ReportToDashboard(product, configuration_name, revision, output_dir)

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
parser.add_argument('--extra_args', action='append', default=[])
parser.add_argument('--overwrite_results', action='store_true')
parser.add_argument('--skip_reporting', action='store_true')
parser.add_argument('--report_only', action='store_true')
args = parser.parse_args()

for tag in args.tags:
  url = GetBraveUrl(tag, args.platform)
  out_dir = os.path.join(args.work_directory, tag)

  if not args.report_only:
    DownloadAndUnpackBinary(out_dir, url)

  if args.overwrite_results and not args.report_only:
    shutil.rmtree(os.path.join(out_dir, 'results'), True)

  [binary_success,
   binary_logs] = TestBinary('brave', args.configuration_name,
                             'refs/tags/' + tag,
                             os.path.join(out_dir, 'brave.exe'),
                             os.path.join(out_dir, 'results'), args.extra_args,
                             args.skip_reporting,
                             args.report_only)
  if not binary_success:
    has_failure = True
    failedLogs.extend(binary_logs)

print('\n\nSummary:\n')
if has_failure:
  print('\n'.join(failedLogs))
  print('Got %d errors!' % len(failedLogs))
else:
  print('OK')
