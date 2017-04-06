/* * * * * * * * * * * * * * libjwdpmi * * * * * * * * * * * * * */
/* Copyright (C) 2017 J.W. Jagersma, see COPYING.txt for details */

#pragma once
#include <jw/common.h>

namespace jw
{
    namespace video
    {
        union [[gnu::packed]] text_attr
        {
            struct [[gnu::packed]]
            {
                unsigned foreground : 4;
                unsigned background : 3;
                bool blink : 1;
            };
            std::uint8_t raw_value;

            constexpr text_attr() noexcept : text_attr(7, 0, false) { }
            constexpr text_attr(byte fcol, byte bcol, bool _blink) noexcept : foreground(fcol), background(bcol), blink(_blink) { }
        };

        union alignas(2) [[gnu::packed]] text_char
        {
            struct [[gnu::packed]] char_with_attr
            {
                char character;
                text_attr attr;

                constexpr char_with_attr(char c, byte fcol, byte bcol, bool _blink) noexcept : character(c), attr(fcol, bcol, _blink) { }
                constexpr char_with_attr(char c) noexcept : character(c), attr() { }
            } value;
            std::uint16_t raw_value;

            constexpr text_char() noexcept : text_char(' ') { }
            constexpr text_char(char c, byte fcol = 7, byte bcol = 0, bool _blink = false) noexcept : value(c, fcol, bcol, _blink) { }
            constexpr explicit text_char(std::uint16_t v) noexcept : raw_value(v) { }
            constexpr explicit operator std::uint16_t() const noexcept{ return raw_value; }
            constexpr text_char& operator=(char c) noexcept { value.character = c; return *this; }
            constexpr operator char() const noexcept{ return value.character; }
        };
        static_assert(sizeof(text_char) == 2 && alignof(text_char) == 2, "text_char has incorrect size or alignment.");

        struct alignas(0x10) bgra_ffff
        {
            using T = float;
            T b { }, g { }, r { }, a { };

            bgra_ffff() noexcept = default;
            constexpr bgra_ffff(T cr, T cg, T cb, T ca) noexcept : b(cb), g(cg), r(cr), a(ca) { }

            static constexpr T rx = 1.0f;
            static constexpr T gx = 1.0f;
            static constexpr T bx = 1.0f;
            static constexpr T ax = 1.0f;
            static constexpr bool has_alpha = true;
        };

        struct alignas(4) [[gnu::packed]] bgra_8888
        {
            using T = std::uint8_t;
            T b, g, r, a;

            bgra_8888() noexcept = default;
            constexpr bgra_8888(T cr, T cg, T cb, T ca) noexcept : b(cb), g(cg), r(cr), a(ca) { }

            static constexpr T rx = 255;
            static constexpr T gx = 255;
            static constexpr T bx = 255;
            static constexpr T ax = 255;
            static constexpr bool has_alpha = true;
        };

        struct [[gnu::packed]] bgra_8880
        {
            using T = std::uint8_t;
            T b, g, r;

            bgra_8880() noexcept = default;
            constexpr bgra_8880(T cr, T cg, T cb, T) noexcept : b(cb), g(cg), r(cr) { }

            static constexpr T rx = 255;
            static constexpr T gx = 255;
            static constexpr T bx = 255;
            static constexpr T ax = 0;
            static constexpr bool has_alpha = false;
        };

        struct [[gnu::packed]] bgra_6668
        {
            using T = unsigned;
            T b : 6, : 2;
            T g : 6, : 2;
            T r : 6, : 2;
            T a : 8;

            bgra_6668() noexcept = default;
            constexpr bgra_6668(T cr, T cg, T cb, T ca) noexcept : b(cb), g(cg), r(cr), a(ca) { }

            static constexpr T rx = 63;
            static constexpr T gx = 63;
            static constexpr T bx = 63;
            static constexpr T ax = 255;
            static constexpr bool has_alpha = true;
        };

        struct alignas(2) [[gnu::packed]] bgra_5650
        {
            using T = unsigned;
            T b : 5;
            T g : 6;
            T r : 5;

            bgra_5650() noexcept = default;
            constexpr bgra_5650(T cr, T cg, T cb, T) noexcept : b(cb), g(cg), r(cr) { }

            static constexpr T rx = 31;
            static constexpr T gx = 63;
            static constexpr T bx = 31;
            static constexpr T ax = 0;
            static constexpr bool has_alpha = false;
        };

        struct alignas(2) [[gnu::packed]] bgra_5551
        {
            using T = unsigned;
            T b : 5;
            T g : 5;
            T r : 5;
            T a : 1;

            bgra_5551() noexcept = default;
            constexpr bgra_5551(T cr, T cg, T cb, T ca) noexcept : b(cb), g(cg), r(cr), a(ca) { }

            static constexpr T rx = 31;
            static constexpr T gx = 31;
            static constexpr T bx = 31;
            static constexpr T ax = 1;
            static constexpr bool has_alpha = true;
        };

        struct px { };

        template<typename P>
        struct alignas(P) [[gnu::packed]] pixel : public P, public px
        {
            using T = typename P::T;

            constexpr pixel() noexcept = default;
            constexpr pixel(T cr, T cg, T cb, T ca) noexcept : P(cr, cg, cb, ca) { }
            constexpr pixel(T cr, T cg, T cb) noexcept : P(cr, cg, cb, P::ax) { }
            constexpr pixel(const pixel& p) noexcept = default;
            constexpr pixel(pixel&& p) noexcept = default;

            template <typename U> constexpr pixel<U> cast(std::true_type) const noexcept
            { 
                return pixel<U> 
                { 
                    static_cast<typename U::T>(P::r * max<U, P>(U::rx) / max<U, P>(P::rx)),
                    static_cast<typename U::T>(P::g * max<U, P>(U::gx) / max<U, P>(P::gx)),
                    static_cast<typename U::T>(P::b * max<U, P>(U::bx) / max<U, P>(P::bx)),
                    static_cast<typename U::T>(P::a * max<U, P>(U::ax) / max<U, P>(P::ax))
                };
            };

            template <typename U> constexpr pixel<U> cast(std::false_type) const noexcept
            { 
                return pixel<U> 
                { 
                    static_cast<typename U::T>(P::r * max<U, P>(U::rx) / max<U, P>(P::rx)),
                    static_cast<typename U::T>(P::g * max<U, P>(U::gx) / max<U, P>(P::gx)),
                    static_cast<typename U::T>(P::b * max<U, P>(U::bx) / max<U, P>(P::bx))
                };
            };

            template <typename U> constexpr pixel<U> cast() const noexcept { return cast<U>(std::is_void<std::conditional_t<P::has_alpha, void, bool>> { }); }
            template <typename U> constexpr operator pixel<U>() const noexcept { return cast<U>(); }

            template <typename U> constexpr pixel& operator=(const pixel<U>& other) noexcept { return blend(other); }
            constexpr pixel& operator=(const pixel& other) noexcept { return blend(other); }
            template <typename U> constexpr pixel& operator=(pixel<U>&& other) noexcept { return *this = static_cast<pixel&&>(other); }
            constexpr pixel& operator=(pixel&& o) noexcept = default;// { new(this) pixel { std::move(o) }; }

            template <typename U, typename V, std::enable_if_t<(std::is_integral<typename U::T>::value && std::is_integral<typename V::T>::value), bool> = { }> 
            static constexpr std::uint32_t max(auto max) noexcept { return max + 1; }
            template <typename U, typename V, std::enable_if_t<(std::is_floating_point<typename U::T>::value || std::is_floating_point<typename V::T>::value), bool> = { }> 
            static constexpr float max(auto max) noexcept { return max; }

            template <typename U, std::enable_if_t<(std::is_floating_point<typename U::T>::value || std::is_floating_point<typename P::T>::value), bool> = { }>
            constexpr pixel& blend(const pixel<U>& other)
            {
                pixel<std::conditional_t<std::is_floating_point<U>::value, U, P>> src { other };
                pixel<std::conditional_t<std::is_floating_point<P>::value, P, U>> dst { *this };
                //auto ta = src.a + dst.a * (1 - src.a);
                //dst.b = (src.b * src.a + dst.b * dst.a * (1 - src.a)) / ta;
                //dst.g = (src.g * src.a + dst.g * dst.a * (1 - src.a)) / ta;
                //dst.r = (src.r * src.a + dst.r * dst.a * (1 - src.a)) / ta;
                //dst.a = ta;
                return *this = std::move(src);
            }

            template <typename U, std::enable_if_t<(std::is_integral<typename U::T>::value && std::is_integral<typename P::T>::value), bool> = { }> 
            constexpr pixel& blend(const pixel<U>& o)
            {
                return *this = std::move(o);
            }
        };

        struct [[gnu::packed]] px8
        {
            byte value { };

            constexpr px8() noexcept = default;
            constexpr px8(byte v) : value(v) { }
            constexpr px8& operator=(byte p) { value = (p == 0 ? value : p); return *this; }
            constexpr px8& operator=(const px8& p) { value = (p.value == 0 ? value : p.value); return *this; }
            constexpr operator byte() { return value; }

            template<typename T> constexpr auto cast(const auto& pal) { const auto& p = pal[value]; return T { p.r, p.g, p.b, p.a }; }
        };

        using pxf = pixel<bgra_ffff>;
        using px32 = pixel<bgra_8888>;
        using px24 = pixel<bgra_8880>;
        using px16 = pixel<bgra_5650>;
        using px15 = pixel<bgra_5551>;
        using pxvga = pixel<bgra_6668>;

        static_assert(sizeof(pxf  ) == 16, "check sizeof pixel");
        static_assert(sizeof(px32 ) ==  4, "check sizeof pixel");
        static_assert(sizeof(px24 ) ==  3, "check sizeof pixel");
        static_assert(sizeof(px16 ) ==  2, "check sizeof pixel");
        static_assert(sizeof(px15 ) ==  2, "check sizeof pixel");
        static_assert(sizeof(pxvga) ==  4, "check sizeof pixel");
    }
}
