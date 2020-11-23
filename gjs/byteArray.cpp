/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2010 litl, LLC

#include <config.h>

#include <stdint.h>
#include <string.h>  // for strcmp, memchr, strlen

#include <girepository.h>
#include <glib-object.h>
#include <glib.h>

#include <js/ArrayBuffer.h>
#include <js/CallArgs.h>
#include <js/GCAPI.h>  // for AutoCheckCannotGC
#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>
#include <js/Utility.h>   // for UniqueChars
#include <jsapi.h>        // for JS_DefineFunctionById, JS_DefineFun...
#include <jsfriendapi.h>  // for JS_NewUint8ArrayWithBuffer, GetUint...

#include "gi/boxed.h"
#include "gjs/atoms.h"
#include "gjs/byteArray.h"
#include "gjs/context-private.h"
#include "gjs/deprecation.h"
#include "gjs/jsapi-util-args.h"
#include "gjs/jsapi-util.h"
#include "gjs/textEncoding.h"

/* Callbacks to use with JS::NewExternalArrayBuffer() */

static void bytes_unref_arraybuffer(void* contents [[maybe_unused]],
                                    void* user_data) {
    auto* gbytes = static_cast<GBytes*>(user_data);
    g_bytes_unref(gbytes);
}


/* Workaround to keep existing code compatible. This function is tacked onto
 * any Uint8Array instances created in situations where previously a ByteArray
 * would have been created. It logs a compatibility warning. */
GJS_JSAPI_RETURN_CONVENTION
static bool instance_to_string_func(JSContext* cx, unsigned argc,
                                    JS::Value* vp) {
    GJS_GET_THIS(cx, argc, vp, args, this_obj);
    JS::UniqueChars encoding;

    _gjs_warn_deprecated_once_per_callsite(
        cx, GjsDeprecationMessageId::ByteArrayInstanceToString);

    if (!gjs_parse_call_args(cx, "toString", args, "|s", "encoding", &encoding))
        return false;

    if (!JS_IsUint8Array(this_obj)) {
        gjs_throw(cx, "Argument to ByteArray.toString() must be a Uint8Array");
        return false;
    }

    return gjs_decode_from_uint8array(cx, this_obj, encoding.get(), true,
                                      args.rval());
}

static bool define_to_string_func(JSContext* cx, unsigned argc, JS::Value* vp) {
    const GjsAtoms& atoms = GjsContextPrivate::atoms(cx);
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject obj(cx);

    if (!gjs_parse_call_args(cx, "defineToString", args, "o", "obj", &obj))
        return false;

    if (!JS_DefineFunctionById(cx, obj, atoms.to_string(),
                               instance_to_string_func, 1, 0))
        return false;

    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool from_gbytes_func(JSContext* context, unsigned argc, JS::Value* vp) {
    JS::CallArgs argv = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject bytes_obj(context);
    GBytes *gbytes;

    if (!gjs_parse_call_args(context, "fromGBytes", argv, "o",
                             "bytes", &bytes_obj))
        return false;

    if (!BoxedBase::typecheck(context, bytes_obj, nullptr, G_TYPE_BYTES))
        return false;

    gbytes = BoxedBase::to_c_ptr<GBytes>(context, bytes_obj);
    if (!gbytes)
        return false;

    size_t len;
    const void* data = g_bytes_get_data(gbytes, &len);
    JS::RootedObject array_buffer(
        context,
        JS::NewExternalArrayBuffer(
            context, len,
            const_cast<void*>(data),  // the ArrayBuffer won't modify the data
            bytes_unref_arraybuffer, gbytes));
    if (!array_buffer)
        return false;
    g_bytes_ref(gbytes);  // now owned by both ArrayBuffer and BoxedBase

    JS::RootedObject obj(
        context, JS_NewUint8ArrayWithBuffer(context, array_buffer, 0, -1));
    if (!obj)
        return false;

    const GjsAtoms& atoms = GjsContextPrivate::atoms(context);
    if (!JS_DefineFunctionById(context, obj, atoms.to_string(),
                               instance_to_string_func, 1, 0))
        return false;

    argv.rval().setObject(*obj);
    return true;
}

JSObject* gjs_byte_array_from_data(JSContext* cx, size_t nbytes, void* data) {
    JS::RootedObject array_buffer(cx);
    // a null data pointer takes precedence over whatever `nbytes` says
    if (data)
        array_buffer =
            JS::NewArrayBufferWithContents(cx, nbytes, g_memdup(data, nbytes));
    else
        array_buffer = JS::NewArrayBuffer(cx, 0);
    if (!array_buffer)
        return nullptr;

    JS::RootedObject array(cx,
                           JS_NewUint8ArrayWithBuffer(cx, array_buffer, 0, -1));

    const GjsAtoms& atoms = GjsContextPrivate::atoms(cx);
    if (!JS_DefineFunctionById(cx, array, atoms.to_string(),
                               instance_to_string_func, 1, 0))
        return nullptr;
    return array;
}

JSObject* gjs_byte_array_from_byte_array(JSContext* cx, GByteArray* array) {
    return gjs_byte_array_from_data(cx, array->len, array->data);
}

GBytes* gjs_byte_array_get_bytes(JSObject* obj) {
    bool is_shared_memory;
    uint32_t len;
    uint8_t* data;

    js::GetUint8ArrayLengthAndData(obj, &len, &is_shared_memory, &data);
    return g_bytes_new(data, len);
}

GByteArray* gjs_byte_array_get_byte_array(JSObject* obj) {
    return g_bytes_unref_to_array(gjs_byte_array_get_bytes(obj));
}

static JSFunctionSpec gjs_byte_array_module_funcs[] = {
    JS_FN("fromGBytes", from_gbytes_func, 1, 0),
    JS_FN("defineToString", define_to_string_func, 1, 0), JS_FS_END};

bool gjs_define_byte_array_stuff(JSContext* cx,
                                 JS::MutableHandleObject module) {
    module.set(JS_NewPlainObject(cx));
    return JS_DefineFunctions(cx, module, gjs_byte_array_module_funcs);
}
