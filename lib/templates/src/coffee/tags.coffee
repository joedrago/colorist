React = require 'react'
FontIcon = require('material-ui/FontIcon').default

tags = ['a', 'div', 'hr', 'img', 'span', 'table', 'tr','td', 'canvas']

module.exports = {}

do ->
  for elementName in tags
    module.exports[elementName] = React.createFactory(elementName)

  module.exports.el = React.createElement

  module.exports.icon = (which) ->
    return React.createElement(FontIcon, { className: 'material-icons' }, which)
