const GObject = imports.gi.GObject;
const Lang = imports.lang;

const Router = new Lang.Class({
    Name: 'Router',
    GTypeName: 'Router',
    Extends: GObject.Object,

    Signals: {
        'path-not-handled': {}
    },

    _init: function (props) {
        this.parent(props);

        this._routes = [];
    },

    add_route: function (httpMethod, path, cb) {
        let pathMatcher = this._parse_route(path);
        this._routes.push({
            method: httpMethod,
            matcher: pathMatcher,
            handler: cb
        });
    },
    
    // takes a path spec (string) and returns a function which takes a test path
    // and returns false if that path doesn't match the spec, or an object
    // associating named spec keys to their matched values if it does
    _parse_route: function (pathSpec) {
        let components = pathSpec.split('/');
        let pathSpecKeys = [];
        let pathRegExp = components.map(function (component) {
            if (component.indexOf(':') === 0) {
                pathSpecKeys.push(component.slice(1));
                return '([^\\/]+)';
            } else {
                return component;
            }
        }).join('\\/');

        // make sure that we only match if the entire string matches
        let regExp = new RegExp('^' + pathRegExp + '$');

        return function (testPath) {
            let match = testPath.match(regExp);
            if (match === null) {
                return false;
            } else {
                // for every key in the pathSpec, get its value in the matched
                // path, and return an object like { pathSpecKey : val }
                let res = {};
                for (let i=0; i < pathSpecKeys.length; i++) {
                    res[pathSpecKeys[i]] = match[i + 1];
                }
                return res;
            }
        };
    },

    handle_route: function (method, path, query, soupMsg) {
        let methodRoutes = this._routes.filter(function (route) {
            return route.method === method;
        });
        for (let route of methodRoutes) {
            let matchResults = route.matcher(path);
            // found our handler
            if (matchResults !== false) {
                route.handler(matchResults, query, soupMsg);
                return;
            }
        }

        this.emit('path-not-handled');
    }
});
