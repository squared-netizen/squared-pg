#pragma once

#include <squared/sq/error.hpp>

#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace squared::sq {

/**
 * @brief Non-throwing public result container used by SQ operations.
 *
 * Accessing the inactive alternative is a programming error and throws
 * std::logic_error. Operational failures are represented by Error.
 */
template<typename T>
class Result {
public:
    /**
     * @brief Construct a successful result.
     */
    Result(T value)
        : storage_(std::move(value))
    {
    }

    /**
     * @brief Construct a failed result.
     */
    Result(Error error)
        : storage_(std::move(error))
    {
    }

    /**
     * @brief Return true when this result owns a value.
     */
    [[nodiscard]] bool has_value() const noexcept
    {
        return std::holds_alternative<T>(storage_);
    }

    /**
     * @brief Return true when this result owns a value.
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return has_value();
    }

    /**
     * @brief Return mutable value access.
     * @throws std::logic_error when this result contains an Error.
     */
    T& value()
    {
        if (!has_value()) {
            throw std::logic_error("SQ Result does not contain a value");
        }
        return std::get<T>(storage_);
    }

    /**
     * @brief Return immutable value access.
     * @throws std::logic_error when this result contains an Error.
     */
    const T& value() const
    {
        if (!has_value()) {
            throw std::logic_error("SQ Result does not contain a value");
        }
        return std::get<T>(storage_);
    }

    /**
     * @brief Return mutable error access.
     * @throws std::logic_error when this result contains a value.
     */
    Error& error()
    {
        if (has_value()) {
            throw std::logic_error("SQ Result does not contain an error");
        }
        return std::get<Error>(storage_);
    }

    /**
     * @brief Return immutable error access.
     * @throws std::logic_error when this result contains a value.
     */
    const Error& error() const
    {
        if (has_value()) {
            throw std::logic_error("SQ Result does not contain an error");
        }
        return std::get<Error>(storage_);
    }

private:
    std::variant<T, Error> storage_;
};

/**
 * @brief Result specialization for successful operations without a value.
 */
template<>
class Result<void> {
public:
    /**
     * @brief Construct a successful result without a value.
     */
    Result() = default;

    /**
     * @brief Construct a failed result.
     */
    Result(Error error)
        : error_(std::move(error))
    {
    }

    /**
     * @brief Return true when the operation succeeded.
     */
    [[nodiscard]] bool has_value() const noexcept
    {
        return !error_.has_value();
    }

    /**
     * @brief Return true when the operation succeeded.
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return has_value();
    }

    /**
     * @brief Return immutable error access.
     * @throws std::logic_error when the operation succeeded.
     */
    const Error& error() const
    {
        if (!error_) {
            throw std::logic_error("SQ Result does not contain an error");
        }
        return *error_;
    }

private:
    std::optional<Error> error_;
};

}  // namespace squared::sq
