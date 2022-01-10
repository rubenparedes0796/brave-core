#!/usr/bin/env python
# Copyright (c) 2021 The Brave Authors. All rights reserved.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# you can obtain one at http://mozilla.org/MPL/2.0/.
"""A tool to perform authorization to https://brave-perf-dashboard.appspot.com/

The tool performs OAuth2 flow, stores/loads credentials, refreshes a token.
Returns a valid token to stdout if succeeded.
"""

import logging
import os.path
import sys

#vpython -m pip install google-auth google-auth-oauthlib
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow

SCOPES = ['openid', 'https://www.googleapis.com/auth/userinfo.email']
CLIENT_ID_FILE = os.path.join(os.path.expanduser("~"),
                              '.perf_dashboard_client_id.json')
PERF_CREDENTIAL_FILE = os.path.join(os.path.expanduser("~"),
                                    '.perf_dashboard_credentials.json')


def GetDashboardCredentials():
  credentials = None

  if os.path.exists(PERF_CREDENTIAL_FILE):
    credentials = Credentials.from_authorized_user_file(PERF_CREDENTIAL_FILE,
                                                        SCOPES)

  if credentials and credentials.expired and credentials.refresh_token:
    credentials.refresh(Request())

  if not credentials or not credentials.valid or credentials.expired:
    flow = InstalledAppFlow.from_client_secrets_file(CLIENT_ID_FILE, SCOPES)
    credentials = flow.run_console()

    with open(PERF_CREDENTIAL_FILE, 'w') as credentials_dat:
      credentials_dat.write(credentials.to_json())
  return credentials


def GetDashboardToken():
  credentials = GetDashboardCredentials()
  if credentials == None:
    raise RuntimeError('Error generating authentication token')
  return credentials.token


def main():
  try:
    token = GetDashboardToken()
    print(token)
  except Exception as e:
    logging.error(e)
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
