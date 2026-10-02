#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "Argon2Hasher.h"

using lms::data::Argon2Hasher;

TEST(Argon2HasherTest, HashUsesArgon2id) {
    Argon2Hasher hasher;
    EXPECT_EQ(hasher.hash("Secret123").rfind("$argon2id$", 0), 0u);
}

TEST(Argon2HasherTest, HashDoesNotContainThePassword) {
    Argon2Hasher hasher;
    EXPECT_EQ(hasher.hash("Secret123").find("Secret123"), std::string::npos);
}

TEST(Argon2HasherTest, SamePasswordGivesDifferentHashes) {
    Argon2Hasher hasher;
    EXPECT_NE(hasher.hash("Secret123"), hasher.hash("Secret123"));
}

TEST(Argon2HasherTest, CorrectPasswordVerifies) {
    Argon2Hasher hasher;
    const std::string stored = hasher.hash("Secret123");
    EXPECT_TRUE(hasher.verify("Secret123", stored));
}

TEST(Argon2HasherTest, WrongPasswordFails) {
    Argon2Hasher hasher;
    const std::string stored = hasher.hash("Secret123");
    EXPECT_FALSE(hasher.verify("secret123", stored));
    EXPECT_FALSE(hasher.verify("Secret1234", stored));
}

TEST(Argon2HasherTest, EmptyPasswordCannotBeHashed) {
    Argon2Hasher hasher;
    EXPECT_THROW(hasher.hash(""), std::invalid_argument);
}

TEST(Argon2HasherTest, EmptyPasswordNeverVerifies) {
    Argon2Hasher hasher;
    const std::string stored = hasher.hash("Secret123");
    EXPECT_FALSE(hasher.verify("", stored));
}

TEST(Argon2HasherTest, GarbageStoredHashFails) {
    Argon2Hasher hasher;
    EXPECT_FALSE(hasher.verify("Secret123", "not-a-hash"));
}

TEST(Argon2HasherTest, EmptyStoredHashFails) {
    Argon2Hasher hasher;
    EXPECT_FALSE(hasher.verify("Secret123", ""));
}
