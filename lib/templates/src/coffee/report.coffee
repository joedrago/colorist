# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

App = require './App'
DOM = require 'react-dom'
React = require 'react'
MuiThemeProvider = require('material-ui/styles/MuiThemeProvider').default

DOM.render(React.createElement(MuiThemeProvider, {}, React.createElement(App, { key: 'app' })), document.getElementById('appcontainer'))
