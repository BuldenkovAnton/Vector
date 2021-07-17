#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    RawMemory& operator = (const RawMemory&) = delete;

    RawMemory& operator = (RawMemory&& other) {
        Swap(other);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:



    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(
            data_.GetAddress(), size
        );
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {

        std::uninitialized_copy_n(
            other.data_.GetAddress(), other.Size(), data_.GetAddress()
        );
    }


    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }


    Vector& operator = (Vector& other) {
        if (other.size_ > data_.Capacity()) {
            Vector tmp(other);
            Swap(tmp);
        }
        else {

            size_t i = size_ < other.size_ ? size_ : other.size_;
            std::copy_n(other.begin(), i, begin());

            if (size_ < other.size_) {
                std::uninitialized_copy_n(
                    other.data_.GetAddress(), other.size_ - size_, data_.GetAddress() + size_
                );
            }
            else if (size_ > other.size_) {
                std::destroy_n(
                    data_.GetAddress() + other.size_,
                    size_ - other.size_
                );
            }
            size_ = other.size_;
        }
        return *this;
    }

    Vector& operator = (Vector&& other) noexcept {
        Swap(other);
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacit) {
        if (new_capacit > data_.Capacity()) {
            RawMemory<T> data_tmp(new_capacit);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(
                    data_.GetAddress(), size_, data_tmp.GetAddress()
                );
            }
            else {
                std::uninitialized_copy_n(
                    data_.GetAddress(), size_, data_tmp.GetAddress()
                );
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(data_tmp);
        }
    }

    void Resize(size_t new_size) {
        if (size_ < new_size) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(
                data_.GetAddress() + size_, new_size - size_
            );
        }
        else if (size_ > new_size) {
            std::destroy_n(
                data_.GetAddress() + new_size, size_ - new_size
            );
        }
        size_ = new_size;
    }

    template <typename ... Args>
    T& EmplaceBack(Args&& ... args) {
        return InsertInVector(std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void PushBack(Args&& ... args) {
        InsertInVector(std::forward<Args>(args)...);
    }

    void PopBack() {
        assert(size_ > 0);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }


    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return  data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) noexcept {
        const size_t result_index = pos - begin(); 
        if (size_ == result_index) {
            EmplaceBack(std::forward<Args>(args)...);
        }
        else if (size_ < Capacity()) {
            T tmp = T(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(data_[size_ - 1u]));
            std::move_backward(data_.GetAddress() + result_index, end() - 1, end());
            data_[result_index] = std::move(tmp);

        }
        else {
            RawMemory<T> data_tmp(size_ * 2);
            new(data_tmp + result_index) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), result_index, data_tmp.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), result_index, data_tmp.GetAddress());
            }

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress() + result_index, size_ - result_index, data_tmp.GetAddress() + result_index + 1);
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress() + result_index, size_ - result_index, data_tmp.GetAddress() + result_index + 1);
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(data_tmp);

        }
        size_++;
        return (data_.GetAddress() + result_index);
    }

    iterator Erase(const_iterator pos) noexcept {
        size_t i = pos - cbegin();
        if (i + 1 < size_) {
            std::move(data_.GetAddress() + i + 1, data_.GetAddress() + size_, data_.GetAddress() + i);
        }
        PopBack();
        return begin() + i;
    }

    iterator Insert(const_iterator pos, const T& value) noexcept {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) noexcept {
        return Emplace(pos, std::forward<T>(value));
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    private:
        RawMemory<T> data_;
        size_t size_ = 0;
        

        template <typename ... Args>
        T& InsertInVector(Args&& ... args) {
            if (size_ == data_.Capacity()) {
                size_t capacity_tmp = 0;
                capacity_tmp = size_ == 0 ? 1 : size_ * 2;
                RawMemory<T> data_tmp(capacity_tmp);
                auto elem = new (data_tmp + size_) T(std::forward<Args>(args)...);

                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), size_, data_tmp.GetAddress());
                }
                else {
                    std::uninitialized_copy_n(data_.GetAddress(), size_, data_tmp.GetAddress());
                }
                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(data_tmp);
                ++size_;
                return *elem;
            }
            else {
                auto elem = new (data_ + size_) T(std::forward<Args>(args)...);
                ++size_;
                return *elem;
            }
        }
};