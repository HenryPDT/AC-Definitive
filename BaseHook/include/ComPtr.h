#pragma once

// Minimal internal COM smart pointer (BaseHook-only).
// We intentionally avoid WRL/ATL to keep dependencies minimal.

template <class T>
class ComPtr
{
public:
    ComPtr() = default;
    ComPtr(std::nullptr_t) {}

    ~ComPtr()
    {
        Reset();
    }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept
    {
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
    }

    ComPtr& operator=(ComPtr&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T* Get() const { return m_ptr; }
    T* const* GetAddressOf() const { return &m_ptr; }
    T** GetAddressOf() { return &m_ptr; }

    T* operator->() const { return m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }

    void Reset()
    {
        if (m_ptr)
        {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }

    // For APIs that return a new COM pointer into an out-param, after releasing current.
    T** ReleaseAndGetAddressOf()
    {
        Reset();
        return &m_ptr;
    }

    // Take ownership of an existing pointer (no AddRef).
    void Attach(T* p)
    {
        Reset();
        m_ptr = p;
    }

    // Release ownership without releasing the underlying COM object.
    T* Detach()
    {
        T* tmp = m_ptr;
        m_ptr = nullptr;
        return tmp;
    }

private:
    T* m_ptr = nullptr;
};


