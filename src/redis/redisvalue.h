/*
 * Copyright (C) Alex Nekipelov (alex@nekipelov.net)
 * License: MIT
 */

#pragma once

#include <boost/variant.hpp>
#include <string>
#include <vector>


class RedisValue {
public:
    inline RedisValue();
    inline RedisValue(int i);
    inline RedisValue(const char *s);
    inline RedisValue(const std::string &s);
    inline RedisValue(const std::vector<RedisValue> &array);

    // Return the value as a std::string if 
    // type is a std::string; otherwise returns an empty std::string.
    inline std::string toString() const;
    
    // Return the value as a std::vector<RedisValue> if 
    // type is an int; otherwise returns 0.
    inline int toInt() const;
    
    // Return the value as an array if type is an array;
    // otherwise returns an empty array.
    inline std::vector<RedisValue> toArray() const;

    // Return the string representation of the value. Use
    // for dump content of the value.
    inline std::string inspect() const;

    // Return true if this is a null.
    inline bool isNull() const;
    // Return true if type is an int
    inline bool isInt() const;
    // Return true if type is a string
    inline bool isString() const;
    // Return true if type is an array
    inline bool isArray() const;

    inline bool operator == (const RedisValue &rhs) const;
    inline bool operator != (const RedisValue &rhs) const;

protected:
    template<typename T>
    inline T castTo() const;

    template<typename T>
    inline bool typeEq() const;

private:
    struct NullTag {
        inline bool operator == (const NullTag &) const {
            return true;
        }
    };


    boost::variant<NullTag, int, std::string, std::vector<RedisValue> > value;
};


template<typename T>
T RedisValue::castTo() const
{
    if( value.type() == typeid(T) )
        return boost::get<T>(value);
    else
        return T();
}

template<typename T>
bool RedisValue::typeEq() const
{
    if( value.type() == typeid(T) )
        return true;
    else
        return false;
}

#include "impl/redisvalue.cpp"
