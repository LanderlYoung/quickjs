/* test quickjs.c internal functions */
#include "../quickjs.c" /* HACK: include c file to access static functions */

#ifdef NDEBUG
#undef NDEBUG
// re-include assert
#include <assert.h>
#endif /* ifdef  NDEBUG */

static void test_weak_ref(JSRuntime *rt)
{
    JSContext *ctx = JS_NewContext(rt);
    JSValue obj = JS_NewObject(ctx);
    JSValue weak_ref = JS_NewWeakRef(ctx, obj);
    JSValue deref;

    assert(!JS_IsException(weak_ref));
    deref = JS_DerefWeakRef(ctx, weak_ref);
    assert(JS_StrictEq(ctx, obj, deref));

    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, deref);

    deref = JS_DerefWeakRef(ctx, weak_ref);
    assert(JS_IsUndefined(deref));

    JS_FreeValue(ctx, weak_ref);
    JS_FreeContext(ctx);
}

typedef struct {
    JSHashEntry entry;
    int key;
    int value;
} TestEntry;

static void *__get_key(JSHashMap *_, JSHashEntry *entry)
{
    return &container_of(entry, TestEntry, entry)->key;
}

static uint32_t __hash_key(JSHashMap *_, void *key)
{
    int k = *(int *)key;
    return k * k; /* make conflicts */
}

static BOOL __key_equals(JSHashMap *_, void *key1, void *key2)
{
    return *(int *)key1 == *(int *)key2;
}

#define SIZE 1024
static void test_hash_map(JSRuntime *rt)
{
    { /* add/remove */
        JSHashMap map;
        TestEntry e[SIZE];
        int i;
        assert(!js_hash_map_init(
            rt, &map,
            JS_HASH_MAP_DEFAULT_SIZE,
            JS_HASH_MAP_DEFAULT_LOAD_FACTOR,
            JS_HASH_MAP_DEFAULT_SHRINK_FACTOR,
            FALSE, __get_key, __hash_key, __key_equals));

        assert(js_hash_map_size(&map) == 0);
        for (i = 0; i < SIZE; i++) {
            js_hash_map_init_entry(&e[i].entry);
            e[i].key = e[i].value = i;
            assert(!js_hash_map_add_entry(rt, &map, &e[i].entry));
            assert(js_hash_map_size(&map) == i + 1);
        }

        for (i = 0; i < SIZE; i++) {
            JSHashEntry *entry = js_hash_map_find_entry(&map, &i);
            assert(entry);
            assert(container_of(entry, TestEntry, entry)->key == i);
            assert(container_of(entry, TestEntry, entry)->value == i);

            js_hash_map_remove_entry(rt, &map, entry);
            assert(js_hash_map_find_entry(&map, &i) == NULL);
            assert(js_hash_map_size(&map) == SIZE - i - 1);
        }

        // re-add
        assert(!js_hash_map_add_entry(rt, &map, &e[0].entry));
        assert(js_hash_map_find_entry(&map, &e[0].key));

        js_hash_map_release(rt, &map, NULL, NULL);
        assert(js_hash_map_size(&map) == 0);
    }

    { /* enlarge/shrink */
        JSHashMap map;
        TestEntry e[SIZE];
        int i;
        size_t initial_size = 8;
        float load_factor = 0.5;
        float shrink_factor = 0.2;

        assert(!js_hash_map_init(
            rt, &map,
            initial_size,
            load_factor,
            shrink_factor,
            FALSE, __get_key, __hash_key, __key_equals));

        assert(map.capacity == initial_size);

        // enlarege size == 5 == initial_size * load_factor + 1
        for(i = 0; i < 5; i++) {
            js_hash_map_init_entry(&e[i].entry);
            e[i].key = e[i].value = i;
            assert(!js_hash_map_add_entry(rt, &map, &e[i].entry));
            if (i < 4) {
                assert(map.capacity == 8);
            } else {
                assert(map.capacity == 16 /* initial_size * 2 */); /* enlarge */
            }
        }

        // shrink size == 3.2
        js_hash_map_remove_entry(rt, &map, &e[0].entry);
        assert(map.capacity == 16);
        js_hash_map_remove_entry(rt, &map, &e[1].entry);
        assert(map.capacity == 8); /* shrink */

        js_hash_map_release(rt, &map, NULL, NULL);
    }

    { /* iterate */
        JSHashMap map;
        TestEntry e[SIZE];
        JSHashEntry *el;
        BOOL v[SIZE];
        int i;
        assert(!js_hash_map_init(
            rt, &map,
            JS_HASH_MAP_DEFAULT_SIZE,
            JS_HASH_MAP_DEFAULT_LOAD_FACTOR,
            JS_HASH_MAP_DEFAULT_SHRINK_FACTOR,
            FALSE, __get_key, __hash_key, __key_equals));

        for (i = 0; i < SIZE; i++) {
            js_hash_map_init_entry(&e[i].entry);
            e[i].key = e[i].value = i;
            assert(!js_hash_map_add_entry(rt, &map, &e[i].entry));
        }

        for (i = 0; i < SIZE; i++) v[i] = FALSE;
        el = NULL;
        while ((el = js_hash_map_next_entry(&map, el))) {
            TestEntry *te = container_of(el, TestEntry, entry);
            assert(!v[te->key]);
            v[te->key] = TRUE;
        }
        for (i = 0; i < SIZE; i++) {
            assert(v[i]);
        }

        js_hash_map_release(rt, &map, NULL, NULL);
    }
}

typedef struct {
    JSHashEntryLinked entry;
    int key;
    int value;
} TestEntryLinked;

static TestEntryLinked *__entry(JSHashEntry *entry)
{
    return container_of(container_of(entry, JSHashEntryLinked, entry),
                        TestEntryLinked, entry);
}

static void *__get_key_linked(JSHashMap *_, JSHashEntry *entry)
{
    return &__entry(entry)->key;
}

static void test_hash_map_linked(JSRuntime *rt)
{
    JSHashMap map;
    TestEntryLinked e[SIZE];
    JSHashEntry *el;
    int i;
    assert(!js_hash_map_init(
        rt, &map,
        JS_HASH_MAP_DEFAULT_SIZE,
        JS_HASH_MAP_DEFAULT_LOAD_FACTOR,
        JS_HASH_MAP_DEFAULT_SHRINK_FACTOR,
        TRUE, __get_key_linked, __hash_key, __key_equals));

    for (i = 0; i < SIZE; i++) {
        js_hash_map_init_entry_linked(&e[i].entry);
        e[i].key = e[i].value = i;
        assert(!js_hash_map_add_entry(rt, &map, &e[i].entry.entry));
    }

    i = 0;
    el = NULL;
    while ((el = js_hash_map_next_entry(&map, el))) {
        TestEntryLinked *te = __entry(el);
        assert(te->key == i);
        i++;
    }
    
    { /* iterate */
        JSHashMapIterator it;
        js_hash_map_iterator_init(&map, &it);

        i = 0;
        while ((el = js_hash_map_iterator_next(&map, &it))) {
            assert(__entry(el)->key == i);
            i++;
        }

        js_hash_map_iterator_release(&it);
    }

    { /* modify during iterate */
        JSHashMapIterator it;
        js_hash_map_iterator_init(&map, &it);

        assert(js_hash_map_iterator_next(&map, &it) == &e[0].entry.entry);
        js_hash_map_remove_entry(rt, &map, &e[0].entry.entry);
        js_hash_map_remove_entry(rt, &map, &e[1].entry.entry);
        js_hash_map_remove_entry(rt, &map, &e[2].entry.entry);
        assert(js_hash_map_iterator_next(&map, &it) == &e[3].entry.entry);

        js_hash_map_iterator_release(&it);
    }

    { /* modify during iterate - case 2 */
        JSHashMapIterator it;
        js_hash_map_iterator_init(&map, &it);

        i = 3;
        while ((el = js_hash_map_iterator_next(&map, &it))) {
            assert(__entry(el)->key == i);
            js_hash_map_remove_entry(rt, &map, el);
            i++;
            assert(js_hash_map_size(&map) == SIZE - i);
        }

        js_hash_map_iterator_release(&it);
    }

    js_hash_map_release(rt, &map, NULL, NULL);
}
#undef SIZE

static JSValue __throw_uncatchable_error(JSContext *ctx, JSValueConst _, int _1, JSValueConst *_2)
{
    JSValue error = JS_NewError(ctx);
    assert(JS_SetPropertyStr(ctx, error, "_qjs_error", JS_TRUE) >= 0);
    JS_SetUncatchableError(ctx, error, 1);
    return JS_Throw(ctx, error);
}

void test_uncatchable_error(JSRuntime *rt)
{
    JSContext *ctx = JS_NewContext(rt);
    JSValue throw_fun, test_fun, ret, exp;
    const char script[] = "(f => { try { f(); } catch (e) {} })";
    JSValueConst argv[1];

    assert(ctx);
    throw_fun = JS_NewCFunction(ctx, __throw_uncatchable_error, "throw_uncatchable_error", 0);
    test_fun = JS_Eval(ctx, script, countof(script) - 1, "<test>", 0);
    assert(JS_IsFunction(ctx, throw_fun));
    assert(JS_IsFunction(ctx, test_fun));

    argv[0] = throw_fun;
    ret = JS_Call(ctx, test_fun, JS_UNDEFINED, 1, argv);
    assert(JS_IsException(ret)); /* can not be catched */
    exp = JS_GetException(ctx);
    ret = JS_GetPropertyStr(ctx, exp, "_qjs_error");
    assert(JS_IsBool(ret) && JS_ToBool(ctx, ret) == 1);

    JS_FreeValue(ctx, throw_fun);
    JS_FreeValue(ctx, test_fun);
    JS_FreeValue(ctx, exp);
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
}

int main() {
    JSRuntime *rt = JS_NewRuntime();
    test_weak_ref(rt);
    test_hash_map(rt);
    test_hash_map_linked(rt);
    test_uncatchable_error(rt);
    JS_FreeRuntime(rt);
    return 0;
}
