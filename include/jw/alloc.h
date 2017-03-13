/******************************* libjwdpmi **********************************
Copyright (C) 2016-2017  J.W. Jagersma

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include <memory>
#include <experimental/vector>

namespace jw
{
    template<typename Alloc>
    struct allocator_delete
    {
        using rebind = typename std::allocator_traits<Alloc>::template rebind_alloc<Alloc>;

        Alloc* alloc { nullptr };
        allocator_delete() noexcept { }
        allocator_delete(const Alloc& a) noexcept 
            : alloc(rebind { a }.allocate(1))
        { 
            new(alloc) Alloc { a }; 
        }

        ~allocator_delete() 
        { 
            rebind a { *alloc };
            alloc->~Alloc();
            a.deallocate(alloc, 1);
        }

        void operator()(auto* p)
        {
            if (p == nullptr) return;
            using T = std::remove_reference_t<decltype(*p)>;
            p->~T();
            alloc->deallocate(p, 1);
        }
    };

    template<typename T, typename Alloc, typename... Args>
    auto allocate_unique(const Alloc& alloc, Args&&... args)
    {
        using rebind = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
        using deleter = allocator_delete<rebind>;

        auto d = deleter { rebind { alloc } };
        auto* p = d.alloc->allocate(1);
        try { p = new(p) T { std::forward<Args>(args)... }; }
        catch (...) { d.alloc->deallocate(p, 1); throw; }

        return std::unique_ptr<T, deleter> { p, d };
    }

    template<typename T, typename Alloc>
    auto create_unique(const Alloc& alloc)
    {
        using rebind = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
        using deleter = allocator_delete<rebind>;

        return std::unique_ptr<T, deleter> { nullptr, deleter { rebind { alloc } } };
    }
}