React = require 'react'
DOM = require 'react-dom'

ImageCache = require './ImageCache'
TouchDiv = require './TouchDiv'
tags = require './tags'
{el, div} = require './tags'

class ImageRenderer extends React.Component
  @defaultProps:
    width: 1
    height: 1
    url: ""
    listener: null

  constructor: (props) ->
    super props
    @MAX_SCALE = 32
    @state =
      loaded: false
      error: false
      touchCount: false
    @imageCache = new ImageCache()
    @imageCache.load @props.url, (info) =>
      # is this a notification about the image we're currently trying to display?
      if info.url == @props.url
        if info.error
          # console.log "imageCache returned error"
          @setState { error: true }
        else
          # console.log "imageCache setting loaded state"
          imageSize = @calcImageSize(info.width, info.height, 1)
          imagePos = @calcImageCenterPos(imageSize.width, imageSize.height)
          @setState {
            loaded: true
            originalImageWidth: info.width
            originalImageHeight: info.height
            imageX: imagePos.x
            imageY: imagePos.y
            imageWidth: imageSize.width
            imageHeight: imageSize.height
            imageScale: 1
          }

  componentDidMount: ->
    # console.log "ImageRenderer componentDidMount"
    @setState { touchCount: 0 }

  componentWillUnmount: ->
    # console.log "ImageRenderer componentWillUnmount"
    @setState { touchCount: 0 }
    @imageCache.flush()

  componentWillReceiveProps: (nextProps) ->
    if (@props.width != nextProps.width) or (@props.height != nextProps.height)
      # Size of screen changed. Unzoom and recenter.
      @setScale(1, false)

  moveImage: (x, y, width, height, scale) ->
    # console.log("moveImage(#{x}, #{y}, #{width}, #{height}, #{scale})")
    centerPos = @calcImageCenterPos(width, height)

    if width < @props.width
      # width fits completely, just center it
      x = centerPos.x
    else
      # clamp to fit in the screen bounds
      if x > 0
        x = 0
      if (x + width) < @props.width
        x = @props.width - width

    if height < @props.height
      # height fits completely, just center it
      y = centerPos.y
    else
      # clamp to fit in the screen bounds
      if y > 0
        y = 0
      if (y + height) < @props.height
        y = @props.height - height

    @setState {
      imageX: x
      imageY: y
      imageWidth: width
      imageHeight: height
      imageScale: scale
    }

  setScale: (scale) ->
    imageSize = @calcImageSize(@state.originalImageWidth, @state.originalImageHeight, scale)
    @moveImage(@state.imageX, @state.imageY, imageSize.width, imageSize.height, scale)
    return

  onClick: (x, y) ->

  onRClick: (x, y) ->
    @notifyImagePos(x, y)

  onDoubleTap: (x, y) ->
    scaleTiers = [1, 2, 4, 8, 16, 32]
    scaleIndex = 0
    for s, index in scaleTiers
      if @state.imageScale < s
        break
      scaleIndex = index
    # scaleIndex is now the closest scale to something in scaleTiers
    # Now advance to the 'next' tier
    scaleIndex = (scaleIndex + 1) % scaleTiers.length
    # console.log "onDoubleTap(#{x}, #{y}), scaling to index #{scaleIndex} (#{scaleTiers[scaleIndex]})"
    @zoomTo(x, y, scaleTiers[scaleIndex])

  onNoTouches: ->
    autoZoomOutThreshold = 1.1
    if (@state.imageScale > 1) and (@state.imageScale < autoZoomOutThreshold)
      # Too close to unzoomed, just force it
      @setScale(1, false)

  onTouchCount: (touchCount) ->
    console.log "onTouchCount(#{touchCount})"
    @setState { touchCount: touchCount }
    if touchCount == 0
      @moveImage(@state.imageX, @state.imageY, @state.imageWidth, @state.imageHeight, @state.imageScale)

  onDrag: (dx, dy, dragOriginX, dragOriginY) ->
    console.log "onDrag #{dx} #{dy}"
    if not @state.loaded
      return
    newX = @state.imageX + dx
    newY = @state.imageY + dy
    @moveImage(newX, newY, @state.imageWidth, @state.imageHeight, @state.imageScale)

  onZoom: (x, y, dist) ->
    console.log "onZoom #{x} #{y} #{dist}"
    if not @state.loaded
      return
    imageScale = @state.imageScale + (dist / 100)
    if imageScale < 1
      imageScale = 1
    if imageScale > @MAX_SCALE
      imageScale = @MAX_SCALE

    @zoomTo(x, y, imageScale)

  notifyImagePos: (x, y) -> # in client coord space
    # console.log "onHover #{x}, #{y}"
    imageR = @state.imageX + @state.imageWidth
    imageB = @state.imageY + @state.imageHeight
    if @props.listener
      if (x < @state.imageX) or (y < @state.imageY) or (x >= imageR) or (y >= imageB)
        # console.log "not over image"
        @props.listener.setPos(-1, -1)
      else
        posX = Math.floor((x - @state.imageX) * @state.originalImageWidth / @state.imageWidth)
        posY = Math.floor((y - @state.imageY) * @state.originalImageWidth / @state.imageWidth)
        @props.listener.setPos(posX, posY)
    return

  onHover: (x, y, buttons) ->
    if buttons == 2 # only right mouse button held
      @notifyImagePos(x, y)
    return

  zoomTo: (x, y, imageScale) ->
    # calculate the cursor position in normalized image coords
    normalizedImagePosX = (x - @state.imageX) / @state.imageWidth
    normalizedImagePosY = (y - @state.imageY) / @state.imageHeight

    imageSize = @calcImageSize(@state.originalImageWidth, @state.originalImageHeight, imageScale)
    imagePos = {
      x: x - (normalizedImagePosX * imageSize.width)
      y: y - (normalizedImagePosY * imageSize.height)
    }
    @moveImage(imagePos.x, imagePos.y, imageSize.width, imageSize.height, imageScale)

  calcImageSize: (imageWidth, imageHeight, imageScale) ->
    viewAspectRatio = @props.width / @props.height
    imageAspectRatio = imageWidth / imageHeight
    if viewAspectRatio < imageAspectRatio
      size = {
        width: @props.width
        height: @props.width / imageAspectRatio
      }
    else
      size = {
        width: @props.height * imageAspectRatio
        height: @props.height
      }
    size.width *= imageScale
    size.height *= imageScale
    return size

  calcImageCenterPos: (imageWidth, imageHeight) ->
    return {
      x: (@props.width  - imageWidth ) >> 1
      y: (@props.height - imageHeight) >> 1
    }

  inLandscape: ->
    return (@props.width > @props.height)

  render: ->
    if not @state.loaded
      return []

    elements = []

    elements.push el TouchDiv, {
      key: 'image'
      listener: this
      width: @props.width
      height: @props.height
      style:
        id: 'page'
        position: 'absolute'
        left: @props.left
        top: @props.top
        width: @props.width
        height: @props.height
        backgroundColor: '#bbbbbb'
        backgroundImage: "url(\"#{@props.url}\")"
        backgroundRepeat: 'no-repeat'
        backgroundPosition: "#{@state.imageX}px #{@state.imageY}px"
        backgroundSize: "#{@state.imageWidth}px #{@state.imageHeight}px"
        imageRendering: 'pixelated'
    }

    return tags.div {}, elements

module.exports = ImageRenderer
