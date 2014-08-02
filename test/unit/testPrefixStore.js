const PrefixStore = imports.prefixStore.PrefixStore;

describe('Prefix store', function () {
    let ps;
    beforeEach(function () {
        ps = new PrefixStore();
    });

    it('should be constructable', function () {
        expect(ps).toBeDefined();
    });

    it('should store prefixes by their lang', function () {
        ps.store_prefix('en', {
            field: 'foo',
            prefix: 'bar'
        });
        ps.store_boolean_prefix('en', {
            field: 'bar',
            prefix: 'baz'
        });
    });

    it('should be able to retrieve prefixes by their lang', function () {
        let pfx = {
            field: 'foo',
            prefix: 'bar'
        };
        let bool_pfx = {
            field: 'bar',
            prefix: 'baz'
        };
        ps.store_prefix('en', pfx);
        ps.store_boolean_prefix('en', bool_pfx);
        expect(ps.get('en')).toEqual({
            prefixes: [pfx],
            booleanPrefixes: [bool_pfx]
        });
    });

    it('should not store duplicate prefixes', function () {
        let pfx = {
            field: 'foo',
            prefix: 'bar'
        };
        ps.store_prefix('en', pfx);
        ps.store_prefix('en', pfx);
        ps.store_prefix('en', pfx);
        ps.store_prefix('en', pfx);
        expect(ps.get('en').prefixes.length).toBe(1);
    });

    it('should return a union across all languages for get_all', function () {
        let en_pfx = {
            field: 'foo',
            prefix: 'bar'
        };
        let es_pfx = {
            field: 'baz',
            prefix: 'bang'
        };
        let en_bool_pfx = {
            field: 'wu',
            prefix: 'tang'
        };
        let es_bool_pfx = {
            field: 'el',
            prefix: 'gato'
        };
        ps.store_prefix('en', en_pfx);
        ps.store_prefix('es', es_pfx);
        ps.store_boolean_prefix('en', en_bool_pfx);
        ps.store_boolean_prefix('es', es_bool_pfx);
        expect(ps.get_all()).toEqual({
            prefixes: [en_pfx, es_pfx],
            booleanPrefixes: [en_bool_pfx, es_bool_pfx]
        });
    });

    it('should not return duplicate prefixes from get_all', function () {
        let en_pfx = {
            field: 'foo',
            prefix: 'bar'
        };
        let es_pfx = {
            field: 'foo',
            prefix: 'bar'
        };
        let es_bool_pfx = {
            field: 'wu',
            prefix: 'tang'
        };
        let en_bool_pfx = {
            field: 'wu',
            prefix: 'tang'
        };
        ps.store_prefix('en', en_pfx);
        ps.store_prefix('es', es_pfx);
        ps.store_boolean_prefix('en', en_bool_pfx);
        ps.store_boolean_prefix('es', es_bool_pfx);
        expect(ps.get_all()).toEqual({
            prefixes: [en_pfx],
            booleanPrefixes: [en_bool_pfx]
        });
    });

    it('should be able to store whole prefix maps at a time', function () {
        let pfx = {
            field: 'wu', prefix: 'tang'
        };
        let bool_pfx = {
            field: 'foo',
            prefix: 'bar'
        };
        let prefix_map = {
            prefixes: [pfx],
            booleanPrefixes: [bool_pfx]
        };

        ps.store_prefix_map('en', prefix_map);
        expect(ps.get('en')).toEqual(prefix_map);
    });
});
