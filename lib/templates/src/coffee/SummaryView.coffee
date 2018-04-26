React = require 'react'
DOM = require 'react-dom'

tags = require './tags'
{el, div} = require './tags'

class SummaryView extends React.Component
  @defaultProps: {}

  constructor: (props) ->
    super props

  render: ->
    elements = []
    elements.push div {
      style:
        paddingLeft: '50px'
        paddingTop: '50px'
        textAlign: 'center'
    }, "Here is a sweet summary!"

    return div {
      style:
        width: @props.width
        height: @props.height
        backgroundColor: '#333333'
        color: '#ffffff'
    }, elements

module.exports = SummaryView
