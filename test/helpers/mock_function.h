#ifndef LIBTORRENT_HELPER_MOCK_FUNCTION_H
#define LIBTORRENT_HELPER_MOCK_FUNCTION_H

#include <functional>
#include <map>
#include <tuple>
#include <utility>
#include <cppunit/extensions/HelperMacros.h>

template<typename R, typename... Args>
struct mock_function_map {
  // TODO: Add string name?
  typedef std::vector<std::tuple<R, Args...>> call_list_type;
  typedef std::map<void*, call_list_type> func_map_type;

  // typedef R func_type(Args...);
  // typedef R result_type;

  static func_map_type functions;

  mock_function_map() {
    // Add std::function to size and clear queue.
  }
};

template<typename R, typename... Args>
typename mock_function_map<R, Args...>::func_map_type mock_function_map<R, Args...>::functions;

template<typename R, typename... Args>
void
mock_init_map(R fn[[gnu::unused]](Args...)) {
  mock_function_map<R, Args...>::functions.clear();
}

template<typename R, typename... Args>
void
mock_cleanup_map(R fn[[gnu::unused]](Args...)) {
  typedef mock_function_map<R, Args...> mock_map;
  auto itr = mock_map::functions.find(reinterpret_cast<void*>(fn));

  CPPUNIT_ASSERT_MESSAGE("mock_cleanup_map expected function calls not completed",
                         itr == mock_map::functions.end());
}

template<typename R, typename... Args>
void
mock_expect(R fn(Args...), R ret, Args... args) {
  typedef mock_function_map<R, Args...> mock_map;

  mock_map::functions[reinterpret_cast<void*>(fn)].push_back(std::tuple<R, Args...>(ret, args...));
}


template<typename T, typename U>
mock_compare_value(T t, U u) {
  return t == u;
}

template<std::size_t I = 0, typename Front, typename... Args>
std::enable_if<I == 0, sizeof...(Args)>
mock_compare_args(



template<typename R, typename... Args>
bool
mock_compare_each(const std::tuple<R, Args...>& lhs, const std::tuple<R, Args...>& rhs) {
}


template<typename R, typename... Args>
auto
mock_call(R fn(Args...), Args... args) -> decltype(fn(args...)) {
  typedef mock_function_map<R, Args...> mock_map;
  auto itr = mock_map::functions.find(reinterpret_cast<void*>(fn));

  CPPUNIT_ASSERT_MESSAGE("mock_call expected function calls exhausted",
                         itr != mock_map::functions.end());
  CPPUNIT_ASSERT_MESSAGE("mock_call expected function call arguments mismatch",
                         itr->second.front() == std::tuple_cat(std::tuple<R>(std::get<0>(itr->second.front())), std::tuple<Args...>(args...)));

  auto ret = std::get<0>(itr->second.front());
  itr->second.erase(itr->second.begin());

  if (itr->second.empty())
    mock_map::functions.erase(itr);

  return ret;
}

#endif
