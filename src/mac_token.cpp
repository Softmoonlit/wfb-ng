// src/mac_token.cpp
#include "mac_token.h"

void serialize_token(const TokenFrame& token, uint8_t* buffer) {
    std::memcpy(buffer, &token, sizeof(TokenFrame));
}

TokenFrame parse_token(const uint8_t* buffer) {
    TokenFrame token;
    std::memcpy(&token, buffer, sizeof(TokenFrame));
    return token;
}
