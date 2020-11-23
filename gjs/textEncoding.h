/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh

#ifndef GJS_TEXTENCODING_H_
#define GJS_TEXTENCODING_H_

#include <config.h>

#include <stddef.h>  // for size_t

#include <glib.h>

#include <js/TypeDecls.h>

#include "gjs/macros.h"

GJS_JSAPI_RETURN_CONVENTION
bool gjs_decode_from_uint8array(JSContext* cx, JS::HandleObject uint8array,
                                const char* encoding, bool fatal,
                                JS::MutableHandleValue rval);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_encode_to_uint8array(JSContext* cx, JS::HandleString str,
                              const char* encoding,
                              JS::MutableHandleValue rval);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_define_text_encoding_stuff(JSContext* cx,
                                    JS::MutableHandleObject module);

#endif  // GJS_TEXTENCODING_H_
