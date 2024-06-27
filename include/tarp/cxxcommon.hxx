#pragma once

/*
 * Concise way to declare delete the copy and move constructors and
 * assignment operators for a class.
 */
#define DISALLOW_COPY_AND_MOVE(CLASSNAME)             \
    CLASSNAME(const CLASSNAME &) = delete;            \
    CLASSNAME(CLASSNAME &&) = delete;                 \
    CLASSNAME &operator=(const CLASSNAME &) = delete; \
    CLASSNAME &operator=(CLASSNAME &&) = delete

/*
 * Concise way to RAII-lock a std::mutex member variable.
 */
#define LOCK(mtx) std::unique_lock<std::mutex> lock(mtx)

/*
 * Stringify token x to allow concatenation with string literals */
#define tostring__(x) #x
#define tkn2str(x)    tostring__(x)
