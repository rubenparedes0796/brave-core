// Copyright (c) 2021 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

import * as React from 'react'
import { useDispatch } from 'react-redux'
import getBraveNewsAPI, { FeedSearchResultItem, Publisher, Publishers, PublisherType } from '../../../../api/brave_news'
import * as todayActions from '../../../../actions/today_actions'

export enum FeedInputValidity {
  Valid,
  NotValid,
  IsDuplicate,
  Pending,
  HasResults,
}

const regexThe = /^the /

function comparePublishersByName (a: Publisher, b: Publisher) {
  // TODO(petemill): culture-independent 'the' removal, perhaps
  // do the sorting on the service-side.
  const aName = a.publisherName.toLowerCase().replace(regexThe, '')
  const bName = b.publisherName.toLowerCase().replace(regexThe, '')
  if (aName < bName) {
    return -1
  }
  if (aName > bName) {
    return 1
  }
  return 0
}

function isValidFeedUrl (feedInput: string): boolean {
  // is valid url?
  try {
    const url = new URL(feedInput)
    return ['http:', 'https:'].includes(url.protocol)
  } catch { }
  return false
}

type FeedSearchResultModel = FeedSearchResultItem & { status?: FeedInputValidity }

export default function useManageDirectFeeds (publishers?: Publishers) {
  const dispatch = useDispatch()
  // Memoize user feeds
  const userFeeds = React.useMemo<Publisher[] | undefined>(() => {
    if (!publishers) {
      return
    }
    return Object.values(publishers)
      .filter(p => p.type === PublisherType.DIRECT_SOURCE)
      .sort(comparePublishersByName)
  }, [publishers])
  // Function to turn direct feed off
  const onRemoveDirectFeed = function (directFeed: Publisher) {
    dispatch(todayActions.removeDirectFeed({ directFeed }))
  }
  const [feedInputText, setFeedInputText] = React.useState<string>()
  const [feedInputIsValid, setFeedInputIsValid] = React.useState<FeedInputValidity>(FeedInputValidity.Valid)
  const [feedSearchResults, setFeedSearchResults] = React.useState<FeedSearchResultModel[]>([])
  const onChangeFeedInput = (e: React.ChangeEvent<HTMLInputElement>) => {
    setFeedInputText(e.target.value)
    setFeedInputIsValid(FeedInputValidity.Valid)
  }

  const onSearchForSources = React.useCallback(async () => {
    if (!feedInputText) {
      return
    }
    if (!isValidFeedUrl(feedInputText)) {
      setFeedInputIsValid(FeedInputValidity.NotValid)
      return
    }
    setFeedInputIsValid(FeedInputValidity.Pending)
    const api = getBraveNewsAPI()
    const { results } = await api.findFeeds({ url: feedInputText })
    if (results.length === 0) {
      setFeedInputIsValid(FeedInputValidity.NotValid)
      return
    }
    if (results.length === 1) {
      const result = await api.subscribeToNewDirectFeed(results[0].feedUrl)
      if (!result.isValidFeed) {
        setFeedInputIsValid(FeedInputValidity.NotValid)
        return
      }
      if (result.isDuplicate) {
        setFeedInputIsValid(FeedInputValidity.IsDuplicate)
        return
      }
      // Valid
      setFeedInputIsValid(FeedInputValidity.Valid)
      setFeedInputText('')
      dispatch(todayActions.dataReceived({ publishers: result.publishers }))
      return
    }
    setFeedSearchResults(results)
    setFeedInputIsValid(FeedInputValidity.HasResults)
  }, [feedInputText, setFeedSearchResults])

  const setFeedSearchResultsItemStatus = (sourceUrl: string, status: FeedInputValidity) => {
    setFeedSearchResults(existing => {
      const item = existing.find(item => item.feedUrl.url === sourceUrl)
      const others = existing.filter(other => other !== item)
      return [
        ...others,
        {
          ...item,
          status
        }
      ] as FeedSearchResultModel[]
    })
  }

  const onAddSource = React.useCallback(async (sourceUrl: string) => {
    // Ask the backend
    setFeedInputIsValid(FeedInputValidity.Pending)
    const api = getBraveNewsAPI()
    setFeedSearchResultsItemStatus(sourceUrl, FeedInputValidity.Pending)
    const result = await api.subscribeToNewDirectFeed({ url: sourceUrl })
    const status = !result.isValidFeed
      ? FeedInputValidity.NotValid
      : result.isDuplicate
        ? FeedInputValidity.IsDuplicate
        : FeedInputValidity.Valid
    setFeedSearchResultsItemStatus(sourceUrl, status)
    dispatch(todayActions.dataReceived({ publishers: result.publishers }))
  }, [feedInputText, setFeedInputIsValid, dispatch])

  return {
    userFeeds,
    feedInputIsValid,
    feedInputText,
    feedSearchResults,
    onRemoveDirectFeed,
    onChangeFeedInput,
    onSearchForSources,
    onAddSource
  }
}
