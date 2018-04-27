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
StructArray = require './StructArray'

class RawView extends React.Component
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

    elements.push el ImageRenderer, {
      width: @props.width - infoPanelWidth
      height: @props.height
      url: COLORIST_DATA.uri
      listener: this
    }

    sections = []
    if (@state.x >= 0) and (@state.y >= 0)
      pixel = @rawPixels.get(@state.x, @state.y)
      sections.push {
        name: "Raw"
        rows: [
          ["R", pixel.r]
          ["G", pixel.g]
          ["B", pixel.b]
          ["A", pixel.a]
        ]
      }

    elements.push el InfoPanel, {
      title: "Raw Pixels"
      left: @props.width - infoPanelWidth
      width: infoPanelWidth
      height: @props.height

      url: COLORIST_DATA.uri
      sections: sections

      usesPixelPos: true
      x: @state.x
      y: @state.y
    }
    return elements

module.exports = RawView
