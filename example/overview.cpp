// Copyright Louis Dionne 2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include "../test/testing.hpp"


//////////////////////////////////////////////////////////////////////////////
// Important: Keep this file in sync with the Overview in the README
//////////////////////////////////////////////////////////////////////////////
#include <dyno.hpp>
#include <iostream>
using namespace dyno::literals;

// Define the interface of something that can be drawn
struct Drawable : decltype(dyno::requires(
  "draw"_s = dyno::function<void (dyno::T const&, std::ostream&)>,
  "draw"_s = dyno::function<void (dyno::T const&, int, std::ostream&)>  
)) { };
// Define how concrete types can fulfill that interface
template <typename T>
auto const dyno::default_concept_map<Drawable, T> = dyno::make_concept_map(
  "draw"_s = [](T const& self, std::ostream& out) { self.draw(out); },
  "draw"_s = [](T const& self, int x, std::ostream& out) { self.draw(x, out); }
  
);
// Define an object that can hold anything that can be drawn.
struct drawable {
  template <typename T>
  drawable(T x) : poly_{x} { }

  void draw(std::ostream& out) const
  { poly_.virtual_("draw"_s)(poly_, out); }
  void draw(int x, std::ostream& out) const
  { poly_.virtual_("draw"_s)(poly_, (int)x, out); }

private:
  dyno::poly<Drawable> poly_;
};

struct Square {
  void draw(std::ostream& out) const { out << "Square"; }
  void draw(int x, std::ostream& out) const { out << x << "Square"; }
};

struct Circle {
  void draw(std::ostream& out) const { out << "Circle"; }
  void draw(int x, std::ostream& out) const { out << x << "Circle"; }
  
};

void f(drawable const& d) {
  d.draw(std::cout);
}
void f(int x, drawable const& d) {
  d.draw(x, std::cout);
}
int main() {
  using call_t = boost::hana::basic_type<void(int, int, std::ostream&)>;
  using name_t = dyno::detail::NameWithHash<decltype("draw"_s), call_t>;


  f(Square{}); // prints Square
  std::cout << std::endl;
  f(Circle{}); // prints Circle
  std::cout << std::endl;
  f(5,Square{}); // prints 5Square
  std::cout << std::endl;
  f(5,Circle{}); // prints 5Circle
  std::cout << std::endl;
}
//////////////////////////////////////////////////////////////////////////////
