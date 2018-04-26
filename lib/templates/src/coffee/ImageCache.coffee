# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

class ImageCache
  constructor: ->
    @cache = {}
    @MAX_RETRIES = 3

  notify: (entry) ->
    info =
      url: entry.url
      loaded: entry.loaded
      error: entry.error
      width: entry.width
      height: entry.height
    for cb in entry.callbacks
      if cb
        setTimeout ->
          cb(info)
        , 0
    entry.callbacks = []
    return

  flush: ->
    @cache = {}

  load: (url, cb) ->
    entry = @cache[url]
    if entry and (entry.loaded or entry.error)
      entry.callbacks.push cb
      @notify(entry)
      return

    image = new Image()
    entry =
      url: url
      image: image
      callbacks: [cb]
      loaded: false
      error: false
      errorCount: 0
      width: 0
      height: 0
    @cache[url] = entry

    image.onload = =>
      entry.loaded = true
      entry.error = false
      entry.width = entry.image.width
      entry.height = entry.image.height
      @notify(entry)
    image.onerror = =>
      entry.loaded = false
      entry.errorCount += 1
      if entry.errorCount < @MAX_RETRIES
        cacheBreakerUrl = entry.url + '?' + +new Date
        entry.image.src = cacheBreakerUrl
      else
        entry.error = true
        @notify(entry)
    image.src = url

module.exports = ImageCache
