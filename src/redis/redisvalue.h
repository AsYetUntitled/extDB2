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
    RedisValue();
    RedisValue(int i);
    RedisValue(const char *s);
    RedisValue(const std::string &s);
    RedisValue(const std::vector<RedisValue> &array);

    // Return the value as a std::string if 
    // type is a std::string; otherwise returns an empty std::string.
    std::string toString() const;
    
    // Return the value as a std::vector<RedisValue> if 
    // type is an int; otherwise returns 0.
    int toInt() const;
    
    // Return the value as an array if type is an array;
    // otherwise returns an empty array.
    std::vector<RedisValue> toArray() const;

    // Return the string representation of the value. Use
    // for dump content of the value.
    std::string inspect() const;

    // Return true if this is a null.
    bool isNull() const;
    // Return true if type is an int
    bool isInt() const;
    // Return true if type is a string
    bool isString() const;
    // Return true if type is an array
    bool isArray() const;

    bool operator == (const RedisValue &rhs) const;
    bool operator != (const RedisValue &rhs) const;

protected:
    template<typename T>
    T castTo() const;

    template<typename T>
    bool typeEq() const;

private:
    struct NullTag {
        bool operator == (const NullTag &) const {
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
