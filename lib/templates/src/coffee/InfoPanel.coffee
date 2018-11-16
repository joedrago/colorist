# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

React = require 'react'
DOM = require 'react-dom'

utils = require './utils'
tags = require './tags'
{el, div} = require './tags'

stockPrimaries =
 "bt709":  [ 0.64, 0.33, 0.30, 0.60, 0.15, 0.06, 0.3127, 0.3290 ]
 "bt2020": [ 0.708, 0.292, 0.170, 0.797, 0.131, 0.046, 0.3127, 0.3290 ]
 "p3":     [ 0.68, 0.32, 0.265, 0.690, 0.150, 0.060, 0.3127, 0.3290 ]

{BottomNavigation, BottomNavigationItem} = require('material-ui/BottomNavigation')
Paper = require('material-ui/Paper').default

horseshoeWidth = 250
horseshoeHeight = 277
horseshoeOrigin = [20, 255]
horseshoeGraphScale = 277
xyToHorseshoeCoord = (x, y) ->
  return [utils.clamp(horseshoeOrigin[0] + (x * horseshoeGraphScale), 0, horseshoeWidth), utils.clamp(horseshoeOrigin[1] - (y * horseshoeGraphScale), 0, horseshoeHeight)]

drawPrimariesTriangle = (ctx, primaries, dashed = false) ->
  ctx.beginPath()
  if dashed
    ctx.setLineDash([5])
  else
    ctx.setLineDash([])
  triangle = [
    xyToHorseshoeCoord(primaries[0], primaries[1])
    xyToHorseshoeCoord(primaries[2], primaries[3])
    xyToHorseshoeCoord(primaries[4], primaries[5])
  ]
  ctx.moveTo(triangle[0][0], triangle[0][1])
  ctx.lineTo(triangle[1][0], triangle[1][1])
  ctx.lineTo(triangle[2][0], triangle[2][1])
  ctx.lineTo(triangle[0][0], triangle[0][1])
  ctx.stroke()

class InfoPanel extends React.Component
  @defaultProps:
    title: "Info"
    usesPixelPos: false
    x: -1
    y: -1
    horseshoeX: -1
    horseshoeY: -1
    sections: []

  constructor: (props) ->
    super props

  makeTableRow: (data, cols = 0, align = 'right') ->
    if cols == 0
      cols = data.length

    attrs = {}
    rows = []
    percent = Math.floor(100 / cols)
    for d, index in data
      if (index == 0) and (d.length > 0)
        fontStyle = 'italic'
        borderRight = '1px solid black'
        paddingRight = '10px'
      else
        fontStyle = 'normal'
        borderRight = '0px'
        paddingRight = '0px'
      rows.push tags.td({
        align: align
        style:
          fontStyle: fontStyle
          borderRight: borderRight
          paddingRight: paddingRight
          width: "#{percent}%"
      }, d)

    return tags.tr(attrs, rows)

  makeSingleHeaderRow: (title, cols = 4) ->
    return tags.tr {}, [
      tags.td {
        colSpan: cols
        style:
          fontWeight: 900
          borderBottom: '1px solid black'
      }, title
    ]

  makeSeparatorRow: (cols = 4) ->
    return tags.tr {}, [
      tags.td {
        colSpan: cols
        style:
          height: '20px'
      }, ""
    ]

  render: ->
    scrollElements = []

    scrollElements.push div {
      key: 'title'
      style:
        color: '#000000'
        fontWeight: '900'
        fontSize: '1.4em'
        textAlign: 'center'
        marginBottom: '5px'
        marginBottom: '10px'
    }, @props.title

    rows = []
    hasPixelPos = (@props.x >= 0) and (@props.y >= 0)

    if @props.usesPixelPos
      if hasPixelPos
        scrollElements.push div {
          style:
            textAlign: 'center'
            marginTop: 10
            marginBottom: 10
        }, "[ #{@props.x}, #{@props.y} ]"
      else
        scrollElements.push div {}, "Right click to choose a pixel."

    for section in @props.sections
      rows.push @makeSingleHeaderRow(section.name)
      for row in section.rows
        paddedRow = row.slice()
        while paddedRow.length < 4
          paddedRow.push ""
        rows.push @makeTableRow(paddedRow)
      rows.push @makeSeparatorRow()

    if rows.length > 0
      scrollElements.push tags.table {
        style:
          border: '0px'
          padding: 0
          margin: 0
          borderCollapse: 'collapse'
          width: '100%'
      }, rows

    if (@props.horseshoeX != -1) and (@props.horseshoeX != -1)
      scrollElements.push tags.canvas {
        id: "horseshoe"
        width: horseshoeWidth
        height: horseshoeHeight
      }
      canvas = document.getElementById("horseshoe")
      if canvas?
        ctx = canvas.getContext("2d")
        cie = document.getElementById("cie")
        crosshairs = document.getElementById("crosshairs")
        ctx.clearRect(0, 0, 250, 277)
        ctx.drawImage(cie, 0, 0)

        # Draw stock gamuts as dashed triangles
        drawPrimariesTriangle(ctx, stockPrimaries.bt709, true)
        drawPrimariesTriangle(ctx, stockPrimaries.p3, true)
        drawPrimariesTriangle(ctx, stockPrimaries.bt2020, true)

        # Fill in this image's gamut with a solid triangle
        drawPrimariesTriangle(ctx, COLORIST_DATA.icc.primaries, false)

        # Draw the white point
        ctx.beginPath()
        whitePointCoord = xyToHorseshoeCoord(COLORIST_DATA.icc.primaries[6], COLORIST_DATA.icc.primaries[7])
        ctx.arc(whitePointCoord[0], whitePointCoord[1], 4, 0, 2 * Math.PI)
        ctx.fill()

        crosshairCoord = xyToHorseshoeCoord(@props.horseshoeX, @props.horseshoeY)
        ctx.drawImage(crosshairs, crosshairCoord[0] - 16, crosshairCoord[1] - 16)

    elements = []

    elements.push el Paper, {
      key: 'scrollcontainer'
      style:
        paddingLeft: '5px'
        paddingRight: '5px'
        height: @props.height
        overflow: 'auto'
    }, scrollElements

    panelDiv = div {
      key: "info"
      style:
        id: 'info'
        position: 'fixed'
        left: @props.left
        top: @props.top
        width: @props.width
        height: @props.height
        backgroundColor: '#cccccc'
    }, elements
    return panelDiv

module.exports = InfoPanel
