#ifndef FUNCTION_H
#define FUNCTION_H
#include <memory>
#include <type_traits>
#include <cstdlib>
static const size_t MAX_SMALL_SIZE = 64;
namespace my
{
template <typename>
struct function;

template <typename T, typename ...Args>
struct function<T(Args...)>
{
    struct callable
    {
        virtual T call(Args &&... args) const = 0;
        virtual void build_copy(void *buf) const = 0;
        virtual void move_copy(void *buf) noexcept = 0;
        virtual std::unique_ptr<callable> copy() const = 0;
        virtual ~callable() = default;
    };

    template <typename F>
    struct callable_impl : public callable
    {
        callable_impl(F &&ff) : f(std::move(ff)) {}
        callable_impl(F const &ff) : f(ff) {}
        T call(Args &&...args) const override { return f(std::forward<Args>(args)...); }
        void build_copy(void *buf) const override { new (buf) callable_impl<F>(f); }
        void move_copy(void *buf) noexcept override { new (buf) callable_impl<F>(std::move(f));  }
        ~callable_impl() override = default;
        std::unique_ptr<callable> copy() const override { return std::make_unique<callable_impl<F>>(f); }

    private:
        F f;
    };

    function() noexcept : ptr(nullptr), is_small(false) {}
    function(std::nullptr_t) noexcept : ptr(nullptr), is_small(false) {}

    template <typename F>
    function(F f)
    {
        if (std::is_nothrow_move_constructible<F>::value &&        //if constexpr...
           sizeof(callable_impl<F>) <= MAX_SMALL_SIZE &&
           alignof(callable_impl<F>) <= alignof(size_t))
        {
            is_small = true;
            new (&buffer) callable_impl<F>(std::move(f));
        }
        else
        {
            is_small = false;
            new (&buffer) std::unique_ptr<callable>(std::make_unique<callable_impl<F>>(std::move(f)));
        }
    }
    function(const function &other)
    {
        if (other.is_small)
        {
            is_small = true;
            callable const *other_callable = reinterpret_cast<callable const *>(&other.buffer);
            other_callable->build_copy(&buffer);
        }
        else
        {
            is_small = false;
            new (&buffer) std::unique_ptr<callable>(other.ptr->copy());
        }
     }
    ~function()
     {
         if (is_small) { (reinterpret_cast<callable*>(&buffer))->~callable(); }
         else { (reinterpret_cast<std::unique_ptr<callable>*>(&buffer))->~unique_ptr(); }
     }
    void move_impl(function &&other) noexcept
    {
        if (other.is_small)
        {
            is_small = true;
            callable *other_callable = reinterpret_cast<callable*>(&other.buffer);
            other_callable->move_copy(&buffer);
            other_callable->~callable();
            other.is_small = false;
            new (&other.buffer) std::unique_ptr<callable>(nullptr);
        }
        else
        {
            is_small = false;
            new (&buffer) std::unique_ptr<callable>(std::move(other.ptr));
        }
    }
    function(function &&other) noexcept { move_impl(std::move(other)); }
    void swap(function &other) noexcept
    {
        function f(std::move(other));
        other = std::move(*this);
        *this = std::move(f);
    }
    function &operator=(const function &other)
    {
        function temp(other);
        swap(temp);
        return *this;
    }
    function &operator=(function &&other) noexcept
    {
        if (this->is_small) { (reinterpret_cast<callable*>(&buffer))->~callable(); }
        else { (reinterpret_cast<std::unique_ptr<callable>*>(&buffer))->~unique_ptr(); }
        move_impl(std::move(other));
        return *this;
     }
    T operator()(Args &&... args) const
    {
        if (is_small) { return reinterpret_cast<callable const *>(&buffer)->call(std::forward<Args>(args)...); }
        else { return ptr->call(std::forward<Args>(args)...); }
    }
    explicit operator bool() const noexcept
    {
        return is_small || ptr;
    }
    private:
        union
        {
            mutable typename std::aligned_storage<MAX_SMALL_SIZE, alignof(size_t)>::type buffer;
            std::unique_ptr<callable> ptr;
        };
        bool is_small;
    };

template <typename F, typename... Args>
void swap(function<F(Args...)> &one, function<F(Args...)> &two) noexcept
{
   one.swap(two);
}
}

#endif // FUNCTION_H
