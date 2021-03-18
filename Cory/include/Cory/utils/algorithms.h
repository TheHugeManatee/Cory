

namespace util {
// use as 
// ```
// std::variant<int, float> v{1.0f};
// std::visit(multi_visitor{
//          [](const int v){ /* ... */}, 
//          [](const float v){/* ... */}}, 
// v);
// ```

template<class... Ts> struct multi_visitor : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> multi_visitor(Ts...) -> multi_visitor<Ts...>;

}