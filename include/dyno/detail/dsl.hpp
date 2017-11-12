// Copyright Louis Dionne 2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef DYNO_DETAIL_DSL_HPP
#define DYNO_DETAIL_DSL_HPP

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
#include <utility>

#include "ctti/type_id.hpp"

namespace dyno {

// Right-hand-side of a clause in a concept that signifies a function with the
// given signature.
template <typename Signature>
constexpr boost::hana::basic_type<Signature> function{};

// Placeholder type representing the type of ref-unqualified `*this` when
// defining a clause in a concept.
struct T;

namespace detail {
  template <typename Name, typename ...Args>
  struct delayed_call {
    boost::hana::tuple<Args...> args;

    // All the constructors are private so that only `dyno::string` can
    // construct an instance of this. The intent is that we can only
    // manipulate temporaries of this type.
  private:
    template <char ...c> friend struct string;

    template <typename ...T>
    constexpr delayed_call(T&& ...t) : args{std::forward<T>(t)...} { }
    delayed_call(delayed_call const&) = default;
    delayed_call(delayed_call&&) = default;
  };

  
  template<typename PrefixStr, std::uint64_t Hash>
  struct append_ctti_hash {
    using tmp = std::integral_constant<char, Hash % 10 + '0'>;
    using cur_str = decltype(PrefixStr{} + boost::hana::string<tmp::value>{});
    using str = typename append_ctti_hash<cur_str, Hash / 10>::str;
  };
  template<typename PrefixStr>
  struct append_ctti_hash<PrefixStr, 0> {
    using str = PrefixStr;
  };

  template<typename Ret, typename Arg, typename... Rest>
  static boost::hana::tuple<Rest...> all_but_first_argument_helper(Ret(*) (Arg, Rest...));
  
  template<typename Ret, typename F, typename Arg, typename... Rest>
  static boost::hana::tuple<Rest...> all_but_first_argument_helper(Ret(F::*) (Arg, Rest...));
  
  template<typename Ret, typename F, typename Arg, typename... Rest>
  static boost::hana::tuple<Rest...> all_but_first_argument_helper(Ret(F::*) (Arg, Rest...) const);

  template<typename Ret, typename F>
  static boost::hana::tuple<> all_but_first_argument_helper(Ret(F::*) ());
  
  template<typename Ret, typename F>
  static boost::hana::tuple<> all_but_first_argument_helper(Ret(F::*) () const);
  
  template <typename F>
  static decltype(all_but_first_argument_helper( &F::operator() )) all_but_first_argument_helper(F);  

  template <typename F>
  using all_but_first_argument = decltype(all_but_first_argument_helper(std::declval<F>()));

  template<typename T>
  struct type_hash {
    using F = decltype(&T::operator());
    using C = all_but_first_argument<F>;
    static constexpr auto hash = ctti::type_id<all_but_first_argument<F>>().hash();
  };
  template<typename Ret, typename This, typename ...Args>
  struct type_hash<boost::hana::basic_type<Ret(This, Args...)>> {
    using arg_tuple = boost::hana::tuple<Args...>;
    static constexpr auto hash = ctti::type_id<arg_tuple>().hash();
  };
  template<typename Ret>
  struct type_hash<boost::hana::basic_type<Ret()>> {
    using arg_tuple = boost::hana::tuple<>;
    static constexpr auto hash = ctti::type_id<arg_tuple>().hash();
  };
  template<typename F, typename Ret, typename This, typename ...Args>
  struct type_hash<Ret (F::*)(This, Args...)> {
    using arg_tuple = boost::hana::tuple<Args...>;
    typename arg_tuple::aa a;
    static constexpr auto hash = ctti::type_id<arg_tuple>().hash();
  };
  template<typename F, typename Ret, typename This, typename ...Args>
  struct type_hash<Ret (F::*)(This, Args...) const> {
    using arg_tuple = boost::hana::tuple<Args...>;
    static constexpr auto hash = ctti::type_id<arg_tuple>().hash();
  };
  template<typename F, typename Ret>
  struct type_hash<Ret (F::*)()> {
    using arg_tuple = boost::hana::tuple<>;
    static constexpr auto hash = ctti::type_id<arg_tuple>().hash();
  };
  template<typename F, typename Ret>
  struct type_hash<Ret (F::*)() const> {
    using arg_tuple = boost::hana::tuple<>;
    static constexpr auto hash = ctti::type_id<arg_tuple>().hash();
  };
  
  template<typename PrefixStr, typename Function>
  using NameWithHash = typename append_ctti_hash<PrefixStr, type_hash<Function>::hash>::str;
  

  template <char ...c>
  struct string : boost::hana::string<c...> {
    template <typename Function>
    constexpr auto operator=(Function f) const {
      static_assert(std::is_empty<Function>{},
        "Only stateless function objects can be used to define vtables");

      using Name = decltype(*this);
      return boost::hana::pair<NameWithHash<Name, Function>, Function>{{}, f};
    }

    template <typename ...Args>
    constexpr auto operator()(Args&& ...args) const {
      return detail::delayed_call<string, Args&&...>{std::forward<Args>(args)...};
    }

    using hana_tag = typename boost::hana::tag_of<boost::hana::string<c...>>::type;
  };

  template <typename S, std::size_t ...N>
  constexpr detail::string<S::get()[N]...> prepare_string_impl(std::index_sequence<N...>)
  { return {}; }

  template <typename S>
  constexpr auto prepare_string(S) {
    return detail::prepare_string_impl<S>(std::make_index_sequence<S::size()>{});
  }
} // end namespace detail

inline namespace literals {
  // Creates a compile-time string that can be used as the left-hand-side when
  // defining clauses or filling concept maps.
  template <typename CharT, CharT ...c>
  constexpr auto operator""_s() { return detail::string<c...>{}; }
} // end namespace literals

// Creates a Dyno compile-time string without requiring the use of a
// user-defined literal.
//
// The user-defined literal is non-standard as of C++17, and it requires
// brining the literal in scope (through a using declaration or such),
// which is not always convenient or possible.
#define DYNO_STRING(s)                                                      \
  (::dyno::detail::prepare_string([]{                                       \
      struct tmp {                                                          \
          /* exclude null terminator in size() */                           \
          static constexpr std::size_t size() { return sizeof(s) - 1; }     \
          static constexpr char const* get() { return s; }                  \
      };                                                                    \
      return tmp{};                                                         \
  }()))                                                                     \
/**/

} // end namespace dyno

#endif // DYNO_DETAIL_DSL_HPP
