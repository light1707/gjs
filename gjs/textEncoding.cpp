/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2010 litl, LLC
// SPDX-FileCopyrightText: 2020 Evan Welsh

#include <config.h>

#include <stdint.h>
#include <string.h>  // for strcmp, memchr, strlen

#include <algorithm>
#include <vector>

#include <gio/gio.h>
#include <girepository.h>
#include <glib-object.h>
#include <glib.h>

#include <js/ArrayBuffer.h>
#include <js/CallArgs.h>
#include <js/CharacterEncoding.h>
#include <js/GCAPI.h>  // for AutoCheckCannotGC
#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>
#include <js/Utility.h>   // for UniqueChars
#include <jsapi.h>        // for JS_DefineFunctionById, JS_DefineFun...
#include <jsfriendapi.h>  // for JS_NewUint8ArrayWithBuffer, GetUint...

#include "gi/boxed.h"
#include "gjs/atoms.h"
#include "gjs/context-private.h"
#include "gjs/deprecation.h"
#include "gjs/jsapi-util-args.h"
#include "gjs/jsapi-util.h"
#include "gjs/textEncoding.h"

static void gfree_arraybuffer_contents(void* contents, void*) {
    g_free(contents);
}

static const char* FALLBACK = "\ufffd";
static size_t FALLBACK_LEN = strlen(FALLBACK);

GJS_JSAPI_RETURN_CONVENTION
static bool gjs_convert_invalid_input(JSContext* cx, uint8_t* data, size_t len,
                                      const char* to_codeset,
                                      const char* from_codeset,
                                      char** converted) {
    GError* error = nullptr;
    GjsAutoUnref<GCharsetConverter> converter(
        g_charset_converter_new(to_codeset, from_codeset, &error));

    // This should only throw if an encoding is not available.
    if (error)
        return gjs_throw_gerror_message(cx, error);

    size_t bytes_written, bytes_read;
    char buffer[1024];

    // Cast data to convert input type, calculate length.
    const char* input = reinterpret_cast<const char*>(data);
    size_t input_len = len * sizeof(char);

    // Use a vector for the ouput for easy resizing.
    std::vector<char> output;
    size_t size = 0;

    do {
        g_converter_convert(G_CONVERTER(converter.get()), input, input_len,
                            buffer, sizeof(buffer), G_CONVERTER_INPUT_AT_END,
                            &bytes_read, &bytes_written, &error);

        input += bytes_read;
        input_len -= bytes_read;

        if (bytes_written > 0) {
            output.resize(size + bytes_written);
            std::copy(buffer, buffer + bytes_written, output.data() + size);
            size += bytes_written;
        }

        if (error) {
            if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA)) {
                // Skip the invalid character
                input += sizeof(char);
                input_len -= sizeof(char);

                // Append fallback character to the output
                output.resize(size + FALLBACK_LEN);
                std::copy(FALLBACK, FALLBACK + FALLBACK_LEN,
                          output.data() + size);
                size += FALLBACK_LEN;

                g_clear_error(&error);
            } else if (bytes_written > 0 &&
                       g_error_matches(error, G_IO_ERROR,
                                       G_IO_ERROR_PARTIAL_INPUT)) {
                // Only clear a partial input error if there are no bytes
                // written. This occurs on the second loop, otherwise we could
                // error mid-input.
                g_clear_error(&error);
            } else if (g_error_matches(error, G_IO_ERROR,
                                       G_IO_ERROR_NO_SPACE)) {
                // If the buffer was full, clear the error and continue
                // converting.
                g_clear_error(&error);
            }
        }
    } while (input_len && !error);

    if (!error) {
        char* arr = reinterpret_cast<char*>(g_malloc0(output.size()));

        std::copy(output.begin(), output.end(), arr);

        *converted = arr;

        // bytes_written should be bytes in a UTF-16 string so should be a
        // multiple of 2
        g_assert((bytes_written % 2) == 0);

        return true;
    }

    return gjs_throw_gerror_message(cx, error);
}

GJS_JSAPI_RETURN_CONVENTION
bool gjs_decode_from_uint8array_slow(JSContext* cx, uint8_t* data, uint32_t len,
                                     const char* encoding, bool fatal,
                                     JS::MutableHandleValue rval) {
    size_t bytes_written, bytes_read;
    GError* error = nullptr;
    GjsAutoChar u16_str;

// Make sure the bytes of the UTF-16 string are laid out in memory
// such that we can simply reinterpret_cast<char16_t> them.
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    const char* to_codeset = "UTF-16LE";
#else
    const char* to_codeset = "UTF-16BE";
#endif

    if (fatal) {
        u16_str = g_convert(reinterpret_cast<char*>(data), len, to_codeset,
                            encoding, nullptr, /* bytes read */
                            &bytes_written, &error);

        // bytes_written should be bytes in a UTF-16 string so should be a
        // multiple of 2
        g_assert((bytes_written % 2) == 0);
    } else {
        // This will fail if the input contains invalid codepoints in the
        // from_codeset. It inserts a replacement character if the input is
        // valid but can't be represented in the output.
        u16_str = g_convert_with_fallback(reinterpret_cast<char*>(data), len,
                                          to_codeset, encoding, FALLBACK,
                                          &bytes_read, &bytes_written, &error);

        if (u16_str) {
            g_assert((bytes_written % 2) == 0);
        }

        // If the input is invalid we need to do the conversion ourselves.
        if (error && g_error_matches(error, G_CONVERT_ERROR,
                                     G_CONVERT_ERROR_ILLEGAL_SEQUENCE)) {
            // Clear the illegal sequence error.
            g_clear_error(&error);

            char* str;

            if (!gjs_convert_invalid_input(cx, data, len, to_codeset, encoding,
                                           &str)) {
                return false;
            }

            u16_str = str;
        }
    }

    if (error) {
        return gjs_throw_gerror_message(cx, error);
    }

    // g_convert 0-terminates the string, although the 0 isn't included in
    // bytes_written
    JSString* s =
        JS_NewUCStringCopyZ(cx, reinterpret_cast<char16_t*>(u16_str.get()));
    if (!s)
        return false;

    rval.setString(s);
    return true;
}

inline bool is_utf8_label(const char* encoding) {
    if (encoding) {
        /* maybe we should be smarter about utf8 synonyms here.
         * doesn't matter much though. encoding_is_utf8 is
         * just an optimization anyway.
         */
        if (strcasecmp(encoding, "utf-8") == 0) {
            return true;
        } else {
            GjsAutoChar stripped(g_strdup(encoding));
            return (strcasecmp(g_strstrip(stripped), "utf-8") == 0);
        }
    } else {
        return true;
    }
}

GJS_JSAPI_RETURN_CONVENTION
bool gjs_decode_from_uint8array(JSContext* cx, JS::HandleObject uint8array,
                                const char* encoding, bool fatal,
                                JS::MutableHandleValue rval) {
    if (!JS_IsUint8Array(uint8array)) {
        gjs_throw(
            cx, "Argument to gjs_decode_from_uint8array must be a Uint8Array");
        return false;
    }

    bool encoding_is_utf8 = is_utf8_label(encoding);
    uint8_t* data;

    uint32_t len;
    bool is_shared_memory;
    js::GetUint8ArrayLengthAndData(uint8array, &len, &is_shared_memory, &data);

    if (len == 0) {
        rval.setString(JS_GetEmptyString(cx));
        return true;
    }

    if (!encoding_is_utf8)
        return gjs_decode_from_uint8array_slow(cx, data, len, encoding, fatal,
                                               rval);

    // optimization, avoids iconv overhead and runs libmozjs hardwired
    // utf8-to-utf16

    // If there are any 0 bytes, including the terminating byte, stop at the
    // first one
    if (data[len - 1] == 0 || memchr(data, 0, len)) {
        if (fatal) {
            if (!gjs_string_from_utf8(cx, reinterpret_cast<char*>(data), rval))
                return false;
        } else {
            if (!gjs_lossy_string_from_utf8(cx, reinterpret_cast<char*>(data),
                                            rval))
                return false;
        }
    } else {
        if (fatal) {
            if (!gjs_string_from_utf8_n(cx, reinterpret_cast<char*>(data), len,
                                        rval))
                return false;
        } else {
            if (!gjs_lossy_string_from_utf8_n(cx, reinterpret_cast<char*>(data),
                                              len, rval))
                return false;
        }
    }

    uint8_t* current_data;
    uint32_t current_len;
    bool ignore_val;

    // If a garbage collection occurs between when we call
    // js::GetUint8ArrayLengthAndData and return from gjs_string_from_utf8, a
    // use-after-free corruption can occur if the garbage collector shifts the
    // location of the Uint8Array's private data. To mitigate this we call
    // js::GetUint8ArrayLengthAndData again and then compare if the length and
    // pointer are still the same. If the pointers differ, we use the slow path
    // to ensure no data corruption occurred. The shared-ness of an array cannot
    // change between calls, so we ignore it.
    js::GetUint8ArrayLengthAndData(uint8array, &current_len, &ignore_val,
                                   &current_data);

    // Ensure the private data hasn't changed
    if (current_len == len && current_data == data)
        return true;

    // This was the UTF-8 optimized path, so we explicitly pass the encoding
    return gjs_decode_from_uint8array_slow(cx, current_data, current_len,
                                           "UTF-8", fatal, rval);
}

GJS_JSAPI_RETURN_CONVENTION
static bool Decode(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::UniqueChars encoding;
    bool fatal = false;
    JS::RootedObject uint8array(cx);

    if (!gjs_parse_call_args(cx, "toString", args, "o|bs", "uint8array",
                             &uint8array, "fatal", &fatal, "encoding",
                             &encoding))
        return false;

    return gjs_decode_from_uint8array(cx, uint8array, encoding.get(), fatal,
                                      args.rval());
}

/* fromString() function implementation */
GJS_JSAPI_RETURN_CONVENTION
bool gjs_encode_to_uint8array(JSContext* cx, JS::HandleString str,
                              const char* encoding,
                              JS::MutableHandleValue rval) {
    bool encoding_is_utf8 = is_utf8_label(encoding);

    JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, str);
    JS::RootedObject obj(cx), array_buffer(cx);

    if (encoding_is_utf8) {
        /* optimization? avoids iconv overhead and runs
         * libmozjs hardwired utf16-to-utf8.
         */
        size_t len = strlen(utf8.get());
        array_buffer = JS::NewArrayBufferWithContents(cx, len, utf8.release());
    } else {
        GError* error = nullptr;
        char* encoded = nullptr;
        gsize bytes_written;

        /* Scope for AutoCheckCannotGC, will crash if a GC is triggered
         * while we are using the string's chars */
        {
            JS::AutoCheckCannotGC nogc;
            size_t len;

            if (JS_StringHasLatin1Chars(str)) {
                const JS::Latin1Char* chars =
                    JS_GetLatin1StringCharsAndLength(cx, nogc, str, &len);
                if (chars == NULL)
                    return false;

                encoded = g_convert(reinterpret_cast<const char*>(chars), len,
                                    encoding,  // to_encoding
                                    "LATIN1",  /* from_encoding */
                                    NULL,      /* bytes read */
                                    &bytes_written, &error);
            } else {
                const char16_t* chars =
                    JS_GetTwoByteStringCharsAndLength(cx, nogc, str, &len);
                if (chars == NULL)
                    return false;

                encoded =
                    g_convert(reinterpret_cast<const char*>(chars), len * 2,
                              encoding,  // to_encoding
                              "UTF-16",  /* from_encoding */
                              NULL,      /* bytes read */
                              &bytes_written, &error);
            }
        }

        if (!encoded)
            return gjs_throw_gerror_message(cx, error);  // frees GError

        array_buffer = JS::NewExternalArrayBuffer(
            cx, bytes_written, encoded, gfree_arraybuffer_contents, nullptr);
    }

    if (!array_buffer)
        return false;
    obj = JS_NewUint8ArrayWithBuffer(cx, array_buffer, 0, -1);

    rval.setObject(*obj);
    return true;
}

GJS_JSAPI_RETURN_CONVENTION
bool gjs_encode_into_uint8array(JSContext* cx, JS::HandleString str,
                                JS::HandleObject uint8array,
                                JS::MutableHandleValue rval) {
    if (!JS_IsUint8Array(uint8array)) {
        gjs_throw(
            cx, "Argument to gjs_encode_into_uint8array must be a Uint8Array");
        return false;
    }

    auto len = JS_GetTypedArrayByteLength(uint8array);
    bool shared;

    // TODO(ewlsh): Garbage collection cannot occur from here...
    auto data =
        JS_GetUint8ArrayData(uint8array, &shared, JS::AutoCheckCannotGC(cx));

    if (shared) {
        gjs_throw(cx, "Cannot encode data into shared memory.");
        return false;
    }

    auto maybe = JS_EncodeStringToUTF8BufferPartial(
        cx, str, mozilla::AsWritableChars(mozilla::Span(data, len)));
    // ... to here

    if (!maybe) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    size_t read, written;

    mozilla::Tie(read, written) = *maybe;

    g_assert(written <= len);

    JS::RootedObject result(cx, JS_NewPlainObject(cx));
    JS::RootedValue readv(cx, JS::NumberValue(read)),
        writtenv(cx, JS::NumberValue(written));

    if (!JS_SetProperty(cx, result, "read", readv) ||
        !JS_SetProperty(cx, result, "written", writtenv)) {
        return false;
    }

    rval.setObject(*result);
    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool Encode(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::UniqueChars encoding;
    JS::UniqueChars utf8;

    if (!gjs_parse_call_args(cx, "Encode", args, "s|s", "string", &utf8,
                             "encoding", &encoding))
        return false;

    if (!args[0].isString()) {
        gjs_throw(cx, "First argument to encode() must be a string.");
        return false;
    }

    JS::RootedString str(cx, args[0].toString());

    return gjs_encode_to_uint8array(cx, str, encoding.get(), args.rval());
}

GJS_JSAPI_RETURN_CONVENTION
static bool EncodeInto(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::UniqueChars utf8;
    JS::RootedObject uint8array(cx);

    if (!gjs_parse_call_args(cx, "EncodeInto", args, "so", "string", &utf8,
                             "uint8array", &uint8array))
        return false;

    if (!args[0].isString()) {
        gjs_throw(cx, "First argument to encode() must be a string.");
        return false;
    }

    JS::RootedString str(cx, args[0].toString());

    return gjs_encode_into_uint8array(cx, str, uint8array, args.rval());
}

static JSFunctionSpec gjs_text_encoding_module_funcs[] = {
    JS_FN("encodeInto", EncodeInto, 2, 0), JS_FN("encode", Encode, 2, 0),
    JS_FN("decode", Decode, 3, 0), JS_FS_END};

bool gjs_define_text_encoding_stuff(JSContext* cx,
                                    JS::MutableHandleObject module) {
    module.set(JS_NewPlainObject(cx));
    return JS_DefineFunctions(cx, module, gjs_text_encoding_module_funcs);
}
