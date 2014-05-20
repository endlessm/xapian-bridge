const Router = imports.router.Router;

describe('Path router', function () {
    var router;
    beforeEach(function () {
        router = new Router();
    });

    it('should be constructable', function () {
        expect(router).toBeDefined();
    });

    it('should have an add_route method', function () {
        expect(router.add_route).toBeDefined();
    });

    it('should handle routes based on what has been added', function () {
        var fooHandler = jasmine.createSpy('fooHandler');
        router.add_route('GET', '/foo', fooHandler);
        router.handle_route('GET', '/foo');
        expect(fooHandler).toHaveBeenCalled();
    });

    it('should emit path-not-handled if path had no handler', function () {
        var notFoundHandler = jasmine.createSpy('notFoundHandler');
        var fooHandler = jasmine.createSpy('fooHandler');
        router.add_route('GET', '/foo', fooHandler);
        router.connect('path-not-handled', notFoundHandler);

        router.handle_route('GET', '/bar');
        expect(fooHandler).not.toHaveBeenCalled();
        expect(notFoundHandler).toHaveBeenCalled();
    });

    it('should allow different handlers for the same route, muxed on method', function () {
        var getHandler = jasmine.createSpy('getHandler');
        var postHandler = jasmine.createSpy('postHandler');
        router.add_route('GET', '/foo', getHandler);
        router.add_route('POST', '/foo', postHandler);

        router.handle_route('GET', '/foo');
        expect(getHandler).toHaveBeenCalled();
        expect(postHandler).not.toHaveBeenCalled();

        getHandler.calls.reset();
        postHandler.calls.reset();

        router.handle_route('POST', '/foo');
        expect(postHandler).toHaveBeenCalled();
        expect(getHandler).not.toHaveBeenCalled();
    });

    it('should not emit path-not-handled if path was handled', function () {
        var notFoundHandler = jasmine.createSpy('notFoundHandler');
        var fooHandler = jasmine.createSpy('fooHandler');
        router.add_route('GET', '/foo', fooHandler);
        router.connect('path-not-handled', notFoundHandler);

        router.handle_route('GET', '/foo');
        expect(fooHandler).toHaveBeenCalled();
        expect(notFoundHandler).not.toHaveBeenCalled();
    });

    describe('matchers', function () {
        it('should pattern match a path', function () {
            var anyHandler = jasmine.createSpy('anyHandler');
            router.add_route('GET', '/:whatever', anyHandler);

            router.handle_route('GET', '/foo');
            router.handle_route('GET', '/bar');
            expect(anyHandler.calls.count()).toBe(2);
        });

        it('should match more complex paths', function () {
            var thingHandler = jasmine.createSpy('thingHandler');
            var thangHandler = jasmine.createSpy('thangHandler');
            var aurHandler = jasmine.createSpy('aurHandler');
            router.add_route('GET', '/bron/:thing', thingHandler)
            router.add_route('GET', '/bron/:thing/aur', aurHandler)
            router.add_route('GET', '/bron/:thing/aur/:thang', thangHandler)

            router.handle_route('GET', '/bron/yr');
            router.handle_route('GET', '/bron/yr/aur');
            router.handle_route('GET', '/bron/yr/aur/stomp');

            expect(thingHandler.calls.count()).toBe(1);
            expect(thangHandler.calls.count()).toBe(1);
            expect(aurHandler.calls.count()).toBe(1);
        });

        it('should not match only the suffix of a path', function () {
            var thingHandler = jasmine.createSpy('thingHandler');
            router.add_route('GET', '/:anything', thingHandler);

            router.handle_route('GET', '/foo/bar');
            expect(thingHandler).not.toHaveBeenCalled();
        });

        it('should call the handler with a dict of keys => vals', function () {
            var thangHandler = jasmine.createSpy('thangHandler');
            router.add_route('GET', '/bron/:thing/aur/:thang', thangHandler)
            
            router.handle_route('GET', '/bron/yr/aur/stomp');
            expect(thangHandler).toHaveBeenCalledWith({
                'thing': 'yr',
                'thang': 'stomp'
            }, undefined, undefined);
        });
    });
})
