# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

React = require 'react'
DOM = require 'react-dom'

tags = require './tags'
utils = require './utils'
{el, div, table, tr, td, tbody} = require './tags'

TopBar = require './TopBar'
ImageRenderer = require './ImageRenderer'
InfoPanel = require './InfoPanel'

as8Bit = (v, depth) ->
  return utils.clamp(Math.round(v / ((1 << depth)-1) * 255), 0, 255)

class PixelsView extends React.Component
  @defaultProps:
    app: null

  constructor: (props) ->
    super props
    @state =
      x: -1
      y: -1

    @rawPixels = @props.app.rawPixels
    @highlightInfos = @props.app.highlightInfos

  setPos: (x, y) ->
    @setState { x: x, y: y }

  render: ->
    elements = []
    elements.push el TopBar, {
      key: "TopBar"
      app: @props.app
    }

    infoPanelWidth = 300

    title = "Pixels"
    image = COLORIST_DATA.visual
    maxNits = COLORIST_DATA.icc.luminance
    if @props.name == 'srgb'
      title = "sRGB Highlight (#{COLORIST_DATA.srgb.highlightLuminance})"
      image = COLORIST_DATA.srgb.visual
      maxNits = COLORIST_DATA.srgb.highlightLuminance

    elements.push el ImageRenderer, {
      width: @props.width - infoPanelWidth
      height: @props.height
      url: image
      listener: this
    }

    sections = []

    horseshoeX = -1
    horseshoeY = -1
    if (@state.x >= 0) and (@state.y >= 0)
      pixel = @rawPixels.get(@state.x, @state.y)
      sections.push {
        name: "Raw"
        rows: [
          ["R", pixel.r, "", as8Bit(pixel.r, COLORIST_DATA.depth)]
          ["G", pixel.g, "", as8Bit(pixel.g, COLORIST_DATA.depth)]
          ["B", pixel.b, "", as8Bit(pixel.b, COLORIST_DATA.depth)]
          ["A", pixel.a, "", as8Bit(pixel.a, COLORIST_DATA.depth)]
        ]
      }

      @pixelInfos
      highlightInfo = @highlightInfos.get(@state.x, @state.y)
      xyY = [highlightInfo.x, highlightInfo.y, highlightInfo.Y]
      pixelLuminance = highlightInfo.nits
      maxPixelLuminance = highlightInfo.maxNits
      outofSRGB = highlightInfo.outOfGamut

      sections.push {
        name: "xyY"
        rows: [
          ["x", utils.fr(xyY[0], 4)]
          ["y", utils.fr(xyY[1], 4)]
          ["Y", utils.fr(xyY[2], 4)]
          ["Nits:", utils.fr(pixelLuminance, 2), "/", utils.fr(maxPixelLuminance, 2)]
        ]
      }
      horseshoeX = xyY[0]
      horseshoeY = xyY[1]

      if @props.name != 'pixels'
        p = (pixelLuminance / maxPixelLuminance)
        REASONABLY_OVERBRIGHT = 0.0001 # this should match the constant in context_report.c's calcOverbright()
        if p > (1.0 + REASONABLY_OVERBRIGHT)
          luminancePercentage = utils.fr(p * 100, 2) + "%"
        else
          luminancePercentage = "--"
        if outofSRGB > 0
          outofSRGB = utils.fr(100 * outofSRGB, 2) + "%"
        else
          outofSRGB = "--"
        sections.push {
          name: "sRGB Overranging (#{maxNits} nits)"
          rows: [
            ["Luminance", luminancePercentage]
            ["Out Gamut", outofSRGB]
          ]
        }

    elements.push el InfoPanel, {
      title: title
      left: @props.width - infoPanelWidth
      width: infoPanelWidth
      height: @props.height

      url: COLORIST_DATA.uri
      sections: sections

      usesPixelPos: true
      x: @state.x
      y: @state.y

      horseshoeX: horseshoeX
      horseshoeY: horseshoeY
    }
    return elements

module.exports = PixelsView
