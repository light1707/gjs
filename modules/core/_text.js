const ByteArray = imports.byteArray;

var TextDecoder = class TextDecoder {
    decode(bytes) {
        return ByteArray.toString(bytes);
    }
}

var TextEncoder = class TextEncoder {
    encode(string) {
        return ByteArray.fromString(string);
    }
}