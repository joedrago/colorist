# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

React = require 'react'
DOM = require 'react-dom'

cm = require './colormath'
tags = require './tags'
utils = require './utils'
{el, div, table, tr, td, tbody} = require './tags'

TopBar = require './TopBar'
ImageRenderer = require './ImageRenderer'
InfoPanel = require './InfoPanel'
StructArray = require './StructArray'

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

    @rawPixels = new StructArray(COLORIST_DATA.raw)

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
    if @props.name == 'srgb100'
      title = "sRGB Highlight (100)"
      image = COLORIST_DATA.srgb100.visual
      maxNits = 100
    else if @props.name == 'srgb300'
      title = "sRGB Highlight (300)"
      image = COLORIST_DATA.srgb300.visual
      maxNits = 300
    overbrightScale = COLORIST_DATA.icc.luminance / maxNits

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

      # floating point, (inverse) gamma space
      floatIG = cm.rawToFloat([pixel.r, pixel.g, pixel.b, pixel.a], COLORIST_DATA.depth)

      # floating point, linear space
      floatLin = cm.vecPow(floatIG, COLORIST_DATA.icc.gamma)

      toXYZ = cm.matDeriveRGBToXYZ(COLORIST_DATA.icc.primaries)
      XYZ = cm.matEval(toXYZ, floatLin)
      xyY = cm.convertXYZtoXYY(XYZ, [COLORIST_DATA.icc.primaries[6], COLORIST_DATA.icc.primaries[7]])

      pixelLuminance = xyY[2] * COLORIST_DATA.icc.luminance
      maxPixelLuminance = maxNits * cm.calcMaxY(xyY, COLORIST_DATA.icc.primaries)

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
        overbright = cm.calcOverbright(xyY, overbrightScale, COLORIST_DATA.icc.primaries)
        outofSRGB = cm.calcOutofSRGB(xyY[0], xyY[1], COLORIST_DATA.icc.primaries)
        luminancePercentage = 100 * (pixelLuminance / maxPixelLuminance)
        if luminancePercentage > 100
          luminancePercentage = utils.fr(luminancePercentage, 2) + "%"
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
