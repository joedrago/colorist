# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

React = require 'react'
FontIcon = require('material-ui/FontIcon').default

tags = ['a', 'div', 'hr', 'img', 'span', 'table', 'tbody', 'tr', 'td', 'canvas']

module.exports = {}

do ->
  for elementName in tags
    module.exports[elementName] = React.createFactory(elementName)

  module.exports.el = React.createElement

  module.exports.icon = (which) ->
    return React.createElement(FontIcon, { className: 'material-icons' }, which)
