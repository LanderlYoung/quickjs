/* test quickjs.c internal functions */
#include "../quickjs.c.h" /* HACK: include c file to access static functions */

void test_hash_map(JSRuntime *rt)
{
    JSHashMap map;
    js_hash_map_init(rt, &map, 
                     JS_HASH_MAP_DEFAULT_SIZE,
                     float load_factor, float shrink_factor, int is_linked, void *(*get_key)(JSHashMap *, JSHashEntry *), uint32_t (*key_hash)(JSHashMap *, void *), int (*BOOL)(int *))

}

void test_weak_ref(JSRuntime *rt)
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

int main() {
    JSRuntime *rt = JS_NewRuntime();
    test_weak_ref(rt);

    JS_FreeRuntime(rt);
    return 0;
}
