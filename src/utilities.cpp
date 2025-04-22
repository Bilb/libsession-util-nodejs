#include "utilities.hpp"

#include <oxenc/hex.h>

#include <chrono>

#include "session/config/namespaces.hpp"
#include "session/util.hpp"

namespace session::nodeapi {

void assertInfoLength(const Napi::CallbackInfo& info, const int expected) {
    checkOrThrow(info.Length() == expected, "Invalid number of arguments");
}

void assertInfoMinLength(const Napi::CallbackInfo& info, const int minLength) {
    checkOrThrow(info.Length() < minLength, "Invalid number of min length arguments");
}

void assertIsStringOrNull(const Napi::Value& val) {
    checkOrThrow(val.IsString() || val.IsNull(), "Wrong arguments: expected string or null");
}

void assertIsNumber(const Napi::Value& val, const std::string& identifier) {
    checkOrThrow(
            val.IsNumber() && !val.IsEmpty() && !val.IsNull() && !val.IsUndefined(),
            std::string("Wrong arguments: expected number" + identifier).c_str());
}

void assertIsArray(const Napi::Value& val, const std::string& identifier) {
    checkOrThrow(
            val.IsArray(), std::string("Wrong arguments: expected array:" + identifier).c_str());
}

void assertIsObject(const Napi::Value& val) {
    checkOrThrow(
            val.IsObject() && !val.IsEmpty() && !val.IsNull() && !val.IsUndefined(),
            "Wrong arguments: expected object");
}

static bool IsUint8Array(const Napi::Value& val) {
    return val.IsTypedArray() && val.As<Napi::TypedArray>().TypedArrayType() == napi_uint8_array;
}

void assertIsUInt8ArrayOrNull(const Napi::Value& val) {
    checkOrThrow(val.IsNull() || IsUint8Array(val), "Wrong arguments: expected uint8Array or null");
}

void assertIsUInt8Array(const Napi::Value& val, const std::string& identifier) {
    checkOrThrow(
            IsUint8Array(val),
            std::string("Wrong arguments: expected uint8Array" + identifier).c_str());
}

void assertIsString(const Napi::Value& val) {
    checkOrThrow(val.IsString(), "Wrong arguments: expected string");
}

void assertIsBoolean(const Napi::Value& val) {
    checkOrThrow(val.IsBoolean(), "Wrong arguments: expected boolean");
}

std::string toCppString(Napi::Value x, const std::string& identifier) {
    if (x.IsNull() || x.IsUndefined()) {
        throw std::invalid_argument{
                "toCppString called with null or undefined with identifier: " + identifier};
    }
    if (x.IsString())
        return x.As<Napi::String>().Utf8Value();

    if (x.IsBuffer()) {
        auto buf = x.As<Napi::Buffer<char>>();
        return {buf.Data(), buf.Length()};
    }

    throw std::invalid_argument{"toCppString unsupported type with identifier: " + identifier};
}

std::optional<std::string> maybeNonemptyString(Napi::Value x, const std::string& identifier) {
    if (x.IsNull() || x.IsUndefined())
        return std::nullopt;
    if (x.IsString()) {
        auto str = x.As<Napi::String>().Utf8Value();
        if (str.empty())
            return std::nullopt;
        return str;
    }

    throw std::invalid_argument{"maybeNonemptyString with invalid type, called from " + identifier};
}

// Converts to a std::span<const unsigned char> that views directly into the Uint8Array data of `x`.
// Throws if x is not a Uint8Array.  The view must not be used beyond the lifetime of `x`.
std::span<const unsigned char> toCppBufferView(Napi::Value x, const std::string& identifier) {
    if (x.IsNull() || x.IsUndefined())
        throw std::invalid_argument(
                "toCppBuffer called with null or undefined with identifier: " + identifier);

    if (!IsUint8Array(x))
        throw std::invalid_argument{"toCppBuffer unsupported type with identifier: " + identifier};

    auto u8Array = x.As<Napi::Uint8Array>();
    return {u8Array.Data(), u8Array.ByteLength()};
}

std::vector<unsigned char> toCppBuffer(Napi::Value x, const std::string& identifier) {
    return session::to_vector(toCppBufferView(x, std::move(identifier)));
}

std::optional<std::vector<unsigned char>> maybeNonemptyBuffer(
        Napi::Value x, const std::string& identifier) {
    if (x.IsNull() || x.IsUndefined())
        return std::nullopt;

    std::optional<std::vector<unsigned char>> buf{toCppBuffer(x, identifier)};
    if (buf->empty())
        buf.reset();
    return buf;
}

int64_t toCppInteger(Napi::Value x, const std::string& identifier, bool allowUndefined) {
    if (allowUndefined && (x.IsNull() || x.IsUndefined()))
        return 0;
    if (x.IsNumber())
        return x.As<Napi::Number>().Int64Value();

    throw std::invalid_argument{"Unsupported type for "s + identifier + ": expected a number"};
}

std::optional<int64_t> maybeNonemptyInt(Napi::Value x, const std::string& identifier) {
    if (x.IsNull() || x.IsUndefined())
        return std::nullopt;
    if (x.IsNumber()) {
        auto num = x.As<Napi::Number>().Int64Value();
        return num;
    }

    throw std::invalid_argument{"maybeNonemptyInt with invalid type, called from " + identifier};
}

std::optional<bool> maybeNonemptyBoolean(Napi::Value x, const std::string& identifier) {
    if (x.IsNull() || x.IsUndefined())
        return std::nullopt;
    if (x.IsBoolean()) {

        return x.As<Napi::Boolean>().Value();
    }

    throw std::invalid_argument{
            "maybeNonemptyBoolean with invalid type, called from " + identifier};
}

bool toCppBoolean(Napi::Value x, const std::string& identifier) {
    if (x.IsNull() || x.IsUndefined())
        return false;

    if (x.IsBoolean() || x.IsNumber())
        return x.ToBoolean();

    throw std::invalid_argument{"Unsupported type for "s + identifier + ": expected a boolean"};
}

std::string printable(std::string_view x) {
    std::string p;
    for (auto c : x) {
        if (c >= 0x20 && c <= 0x7e)
            p += c;
        else
            p += "\\x" + oxenc::to_hex(&c, &c + 1);
    }
    return p;
}

std::string printable(const char* x, size_t n) {
    return printable(std::string_view{x, n});
}

std::string printable(std::span<const unsigned char> x) {
    return printable(reinterpret_cast<const char*>(x.data()), x.size());
}

int64_t toPriority(Napi::Value x, int64_t currentPriority) {
    auto newPriority = toCppInteger(x, "toPriority", true);
    if (newPriority > 0)
        // keep the existing priority if it is already set
        return std::max<int64_t>(currentPriority, 1);

    // newPriority being < 0 means that that conversation is hidden (and so
    // unpinned)
    return newPriority;
}

int64_t unix_timestamp_now() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

Napi::Object push_result_to_JS(
        const Napi::Env& env,
        const push_entry_t& push_entry,
        const session::config::Namespace& push_namespace) {
    auto obj = Napi::Object::New(env);

    obj["seqno"] = toJs(env, std::get<0>(push_entry));
    obj["data"] = toJs(env, std::get<1>(push_entry));
    obj["hashes"] = toJs(env, std::get<2>(push_entry));
    obj["namespace"] = toJs(env, push_namespace);

    return obj;
};

Napi::Object push_key_entry_to_JS(
        const Napi::Env& env,
        const std::span<const unsigned char>& key_data,
        const session::config::Namespace& push_namespace) {
    auto obj = Napi::Object::New(env);

    obj["data"] = toJs(env, key_data);
    obj["namespace"] = toJs(env, push_namespace);

    return obj;
};

Napi::Object decrypt_result_to_JS(
        const Napi::Env& env, const std::pair<std::string, std::vector<unsigned char>> decrypted) {
    auto obj = Napi::Object::New(env);

    obj["pubkeyHex"] = toJs(env, decrypted.first);
    obj["plaintext"] = toJs(env, decrypted.second);

    return obj;
}

confirm_pushed_entry_t confirm_pushed_entry_from_JS(const Napi::Env& env, const Napi::Object& obj) {
    auto seqnoJsValue = obj.Get("seqno");
    assertIsNumber(seqnoJsValue, "confirm_pushed_entry_from_JS.seqno");
    int64_t seqno = toCppInteger(seqnoJsValue, "confirm_pushed_entry_from_JS.seqno", false);
    auto hashesJsValue = obj.Get("hashes");
    assertIsArray(hashesJsValue, "confirm_pushed_entry_from_JS.hashes");

    auto hashesJs = hashesJsValue.As<Napi::Array>();
    std::unordered_set<std::string> hashes;
    for (uint32_t i = 0; i < hashesJs.Length(); i++) {
        auto hashValue = hashesJs.Get(i);
        assertIsString(hashValue);
        std::string hash = toCppString(hashValue, "confirm_pushed_entry_from_JS.hashes.hash");
        hashes.insert(hash);
    }
    confirm_pushed_entry_t confirmed_pushed_entry{seqno, hashes};
    return confirmed_pushed_entry;
}

}  // namespace session::nodeapi
