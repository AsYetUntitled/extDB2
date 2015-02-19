/*
 * Copyright (C) Alex Nekipelov (alex@nekipelov.net)
 * License: MIT
 */

#pragma once

#include <stack>

#include "redisvalue.h"


class RedisParser
{
public:
    RedisParser();

    enum ParseResult {
        Completed,
        Incompleted,
        Error,
    };

    std::pair<size_t, ParseResult> parse(const char *ptr, size_t size);

    RedisValue result();

protected:
    std::pair<size_t, ParseResult> parseChunk(const char *ptr, size_t size);
    std::pair<size_t, ParseResult> parseArray(const char *ptr, size_t size);

    static bool isChar(int c)
    {
        return c >= 0 && c <= 127;
    }

    static bool isControl(int c)
    {
        return (c >= 0 && c <= 31) || (c == 127);
    }

private:
    enum State {
        Start = 0,

        String = 1,
        StringLF = 2,

        ErrorString = 3,
        ErrorLF = 4,

        Integer = 5,
        IntegerLF = 6,

        BulkSize = 7,
        BulkSizeLF = 8,
        Bulk = 9,
        BulkCR = 10,
        BulkLF = 11,

        ArraySize = 12,
        ArraySizeLF = 13,

    } state;

    long int bulkSize;
    std::string string;
    std::stack<long int> arrayStack;
    std::stack<RedisValue> valueStack;

    static const char stringReply = '+';
    static const char errorReply = '-';
    static const char integerReply = ':';
    static const char bulkReply = '$';
    static const char arrayReply = '*';
};
