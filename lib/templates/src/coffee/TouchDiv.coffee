# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------


# React
React = require 'react'
DOM = require 'react-dom'

$ = require "jquery"
require('jquery-mousewheel')($)

# Local requires
tags = require './tags'
{el} = require './tags'

class TouchDiv extends React.Component
  constructor: (props) ->
    super props

    # how many pixels can you drag before it is actually considered a drag
    @ENGAGE_DRAG_DISTANCE = 10

    # How fast you must double tap for it to count as a double tap
    @DOUBLE_CLICK_MS = 400

    @MOUSE_ID = 100
    @mouseDown = false
    @trackedTouches = []
    @dragX = 0
    @dragY = 0
    @dragging = false
    @dblclickTime = null

  componentDidMount: ->
    node = DOM.findDOMNode(this)
    $(node).on 'mousedown', (event) =>
      event.preventDefault()
      if event.which == 3
        # Right mouse click
        return
      @onTouchesBegan [{
        identifier: @MOUSE_ID
        clientX: event.clientX
        clientY: event.clientY
      }]
      @mouseDown = true
    $(node).on 'mouseup', (event) =>
      event.preventDefault()
      if event.which == 3
        # Right mouse click
        return
      @onTouchesEnded [{
        identifier: @MOUSE_ID
        clientX: event.clientX
        clientY: event.clientY
      }]
      @mouseDown = false
    $(node).on 'mousemove', (event) =>
      event.preventDefault()
      if @mouseDown
        @onTouchesMoved [{
          identifier: @MOUSE_ID
          clientX: event.clientX
          clientY: event.clientY
        }]
      @props.listener.onHover(event.clientX, event.clientY, event.buttons)
    $(node).on 'contextmenu', (event) =>
      return if event.ctrlKey
      event.preventDefault()
      @props.listener.onRClick(event.clientX, event.clientY)
    $(node).on 'touchstart', (event) =>
      event.preventDefault()
      @onTouchesBegan event.originalEvent.changedTouches
    $(node).on 'touchend', (event) =>
      event.preventDefault()
      @onTouchesEnded event.originalEvent.changedTouches, event.originalEvent.touches.length
    $(node).on 'touchmove', (event) =>
      event.preventDefault()
      @onTouchesMoved event.originalEvent.changedTouches
    $(node).on 'mousewheel', (event) =>
      event.preventDefault()
      @props.listener.onZoom(event.clientX, event.clientY, event.deltaY * event.deltaFactor / 4)

  componentWillUnmount: ->
    node = DOM.findDOMNode(this)
    $(node).off 'mousedown'
    $(node).off 'mouseup'
    $(node).off 'mousemove'
    $(node).off 'contextmenu'
    $(node).off 'touchstart'
    $(node).off 'touchend'
    $(node).off 'touchmove'
    $(node).off 'mousewheel'

  render: ->
    tags.div {
      style: @props.style
    }

  calcDistance: (x1, y1, x2, y2) ->
    dx = x2 - x1
    dy = y2 - y1
    return Math.sqrt(dx*dx + dy*dy)

  setDragPoint: ->
    @dragX = @trackedTouches[0].x
    @dragY = @trackedTouches[0].y

  calcPinchAnchor: ->
    if @trackedTouches.length >= 2
      @pinchX = Math.floor((@trackedTouches[0].x + @trackedTouches[1].x) / 2)
      @pinchY = Math.floor((@trackedTouches[0].y + @trackedTouches[1].y) / 2)
      # console.log "pinch anchor set at #{@pinchX}, #{@pinchY}"

  addTouch: (id, x, y) ->
    for t in @trackedTouches
      if t.id == id
        return
    @trackedTouches.push {
      id: id
      x: x
      y: y
    }
    if @trackedTouches.length == 1
      @setDragPoint()
    if @trackedTouches.length == 2
      # We just added a second touch spot. Calculate the anchor for pinching now
      @calcPinchAnchor()
    # console.log "adding touch #{id}, tracking #{@trackedTouches.length} touches"
    @props.listener.onTouchCount(@trackedTouches.length)

  removeTouch: (id, x, y) ->
    index = -1
    for i in [0...@trackedTouches.length]
      if @trackedTouches[i].id == id
        index = i
        break
    if index != -1
      @trackedTouches.splice(index, 1)
      if @trackedTouches.length == 1
        @setDragPoint()
      if index < 2
        # We just forgot one of our pinch touches. Pick a new anchor spot.
        @calcPinchAnchor()
      # console.log "forgetting id #{id}, tracking #{@trackedTouches.length} touches"
    @props.listener.onTouchCount(@trackedTouches.length)

  updateTouch: (id, x, y) ->
    index = -1
    for i in [0...@trackedTouches.length]
      if @trackedTouches[i].id == id
        index = i
        break
    if index != -1
      # console.log "updating touch #{id}, tracking #{@trackedTouches.length} touches"
      @trackedTouches[index].x = x
      @trackedTouches[index].y = y

  onDoubleTap: (x, y) ->
    # console.log "onDoubleTap(#{x}, #{y})"
    @props.listener.onDoubleTap(x, y)

  onTouchesBegan: (touches) ->
    if @trackedTouches.length == 0
      @dragging = false
    for t in touches
      id = t.identifier
      x = t.clientX
      y = t.clientY
      @addTouch id, x, y
    if @trackedTouches.length > 1
      # They're pinching, don't even bother to emit a click
      @dragging = true
      @dblclickTime = null
    else if not @dragging
      # Track double clicks
      now = new Date().getTime()
      if @dblclickTime != null
        # second click, if the first click was recent
        clickDelta = now - @dblclickTime
        # console.log "clickdelta #{clickDelta}"
        if clickDelta < @DOUBLE_CLICK_MS
          @dblclickTime = null # require a full new pair of taps to emit a second one
          @onDoubleTap(@trackedTouches[0].x, @trackedTouches[0].y)
          return
      # Remember this click's time in case another comes soon
      @dblclickTime = now

    # console.log "onTouchesBegan: #{JSON.stringify(touches)} touches, trackedTouches now #{JSON.stringify(@trackedTouches)}"
    return

  onTouchesMoved: (touches) ->
    prevDistance = 0
    if @trackedTouches.length >= 2
      prevDistance = @calcDistance(@trackedTouches[0].x, @trackedTouches[0].y, @trackedTouches[1].x, @trackedTouches[1].y)
    if @trackedTouches.length == 1
      prevX = @trackedTouches[0].x
      prevY = @trackedTouches[0].y

    # console.log "onTouchesMoved: #{touches.length} touches"
    for t in touches
      @updateTouch(t.identifier, t.clientX, t.clientY)

    if @trackedTouches.length == 1
      # single touch, consider dragging
      dragDistance = @calcDistance @dragX, @dragY, @trackedTouches[0].x, @trackedTouches[0].y
      if @dragging or (dragDistance > @ENGAGE_DRAG_DISTANCE)
        @dragging = true
        if dragDistance > 0.5
          dx = @trackedTouches[0].x - @dragX
          dy = @trackedTouches[0].y - @dragY
          #console.log "single drag: #{dx}, #{dy}"
          @props.listener.onDrag(dx, dy, @dragX, @dragY)
        @setDragPoint()

    else if @trackedTouches.length >= 2
      # at least two fingers present, check for pinch/zoom
      currDistance = @calcDistance(@trackedTouches[0].x, @trackedTouches[0].y, @trackedTouches[1].x, @trackedTouches[1].y)
      deltaDistance = currDistance - prevDistance
      if deltaDistance != 0
        #console.log "distance dragged apart: #{deltaDistance} [anchor: #{@pinchX}, #{@pinchY}]"
        @props.listener.onZoom(@pinchX, @pinchY, deltaDistance)

    return

  onTouchesEnded: (touches, touchesRemaining) ->
    if @trackedTouches.length == 1
      if not @dragging
        @props.listener.onClick(touches[0].clientX, touches[0].clientY)
      @props.listener.onNoTouches()
    for t in touches
      @removeTouch t.identifier, t.clientX, t.clientY
    if touchesRemaining == 0
      # Remove the rest. Thanks iOS!
      toRemove = @trackedTouches.slice(0)
      for t in toRemove
        @removeTouch t.id, t.x, t.y
    # console.log "onTouchesEnded[rem:#{touchesRemaining}]: #{JSON.stringify(touches)} touches, trackedTouches now #{JSON.stringify(@trackedTouches)}"
    return

module.exports = TouchDiv
