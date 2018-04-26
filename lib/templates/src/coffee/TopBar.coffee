# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

React = require 'react'
DOM = require 'react-dom'

tags = require './tags'
{el, div} = require './tags'

{Toolbar, ToolbarGroup, ToolbarSeparator, ToolbarTitle} = require 'material-ui/Toolbar'
AppBar = require('material-ui/AppBar').default
IconButton = require('material-ui/IconButton').default

class TopBar extends React.Component
  @defaultProps:
    title: null
    app: null

  constructor: (props) ->
    super props

  render: ->
    elements = []

    if @props.title
      elements.push el AppBar, {
        key: 'appbar'
        title: @props.title
        onLeftIconButtonTouchTap: =>
          setTimeout =>
            @props.app.toggleNav()
          , 0
      }
    else
      # Just float a nav button
      elements.push el IconButton, {
        key: "opennavbutton"
        iconClassName: 'material-icons'
        touch: true
        style:
          opacity: 0.5
          position: 'fixed'
          left: 0
          top: 0
          zIndex: 2
        iconStyle:
          color: '#ffffff'
        onClick: =>
          setTimeout =>
            @props.app.toggleNav()
          , 0
      }, 'menu'

    return elements

module.exports = TopBar
