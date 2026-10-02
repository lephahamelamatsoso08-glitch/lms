#include "Argon2Hasher.h"

#include <sodium.h>

#include <stdexcept>

namespace lms::data {

Argon2Hasher::Argon2Hasher() {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium could not be initialised");
    }
}

std::string Argon2Hasher::hash(const std::string& password) const {
    if (password.empty()) {
        throw std::invalid_argument("password must not be empty");
    }
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(out, password.c_str(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw std::runtime_error("password hashing failed (not enough memory)");
    }
    return std::string(out);
}

bool Argon2Hasher::verify(const std::string& password, const std::string& storedHash) const {
    if (password.empty() || storedHash.empty()) {
        return false;
    }
    return crypto_pwhash_str_verify(storedHash.c_str(), password.c_str(), password.size()) == 0;
}

}  // namespace lms::data
