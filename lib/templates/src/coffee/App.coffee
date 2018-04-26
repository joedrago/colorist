# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

DOM = require 'react-dom'
React = require 'react'

tags = require './tags'
{el, div} = require './tags'
Drawer = require('material-ui/Drawer').default
MenuItem = require('material-ui/MenuItem').default

SummaryView = require './SummaryView'

class App extends React.Component
  constructor: (props) ->
    super props

    @state =
      width: 0
      height: 0
      navOpen: false
      view: null

    @views =
      summary: SummaryView

    @navigate(true)
    window.addEventListener('hashchange', (event) =>
      @navigate()
    , false)

  navigate: (fromConstructor = false) ->
    newHash = decodeURIComponent(window.location.hash.replace(/^#\/?|\/$/g, ''))
    view = newHash.split('/')[0]
    viewArg = newHash.substring(view.length+1)
    if not @views.hasOwnProperty(view)
      view = 'summary'
      viewArg = ''
      @redirect('#summary')

    if fromConstructor
      @state.view = view
      @state.viewArg = viewArg
    else
      @setState { view: view, viewArg: viewArg }

  componentDidMount: ->
    # Calculate size. TODO: Detect resize and fix
    containerDom = document.getElementById("appcontainer")
    console.log "#{containerDom.clientWidth}x#{containerDom.clientHeight}"
    if (@state.width != containerDom.clientWidth) || (@state.height != containerDom.clientHeight)
      setTimeout =>
        @setState({ width: containerDom.clientWidth, height: containerDom.clientHeight })
      , 0

  redirect: (newHash) ->
    window.location.hash = newHash
    return

  toggleNav: ->
    @setState { navOpen: !@state.navOpen }

  render: ->
    if (@state.width == 0) or (@state.height == 0)
      return []

    cie = document.getElementById("cie")

    elements = []

    # Left navigation panel
    navMenuItems = [
      el MenuItem, {
        key: "menu.title"
        primaryText: "Available Reports"
        disabled: true
      }

      el MenuItem, {
        key: "menu.summary"
        primaryText: "Summary"
        leftIcon: tags.icon 'event_note'
        onClick: (e) =>
          e.preventDefault()
          @redirect('#summary')
          @setState { navOpen: false }
      }
    ]
    elements.push el Drawer, {
      key: 'leftnav'
      docked: false
      open: @state.navOpen
      disableSwipeToOpen: true
      onRequestChange: (open) => @setState { navOpen: open }
    }, navMenuItems

    # Main view
    if @state.view != null
      elements.push el @views[@state.view], {
        key: @state.view
        width: @state.width
        height: @state.height
        app: this
      }
    return elements

Number.prototype.clamp = (min, max) ->
  return Math.min(Math.max(this, min), max)

module.exports = App
