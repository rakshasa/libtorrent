#ifndef LIBTORRENT_HELPER_MOCK_FUNCTION_H
#define LIBTORRENT_HELPER_MOCK_FUNCTION_H

#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <cppunit/extensions/HelperMacros.h>
#include <torrent/net/socket_address.h>

template<typename R, typename... Args>
struct mock_function_map {
  typedef std::tuple<R, Args...> call_type;
  typedef std::vector<call_type> call_list_type;
  typedef std::map<void*, call_list_type> func_map_type;

  static func_map_type functions;

  mock_function_map() {
    // Add std::function to size and clear queue.
  }

  static bool cleanup(void* fn) {
    return functions.erase(fn) == 0;
  }

  static R ret_erase(void* fn) {
    auto itr = functions.find(fn);
    auto ret = std::get<0>(itr->second.front());
    itr->second.erase(itr->second.begin());

    if (itr->second.empty())
      functions.erase(itr);

    return ret;
  }
};

template<typename R, typename... Args>
typename mock_function_map<R, Args...>::func_map_type mock_function_map<R, Args...>::functions;

struct mock_void {};

template <typename Arg>
inline bool mock_compare_arg(const Arg& lhs, const Arg& rhs) { return lhs == rhs; }
inline bool mock_compare_arg(const sockaddr* lhs, const sockaddr* rhs) {
  return lhs != nullptr && rhs != nullptr && torrent::sa_equal(lhs, rhs);
}

template <int I, typename A, typename... Args>
typename std::enable_if<I == 1, int>::type
mock_compare_tuple(const std::tuple<A, Args...>& lhs, const std::tuple<Args...>& rhs) {
  return mock_compare_arg(std::get<I>(lhs), std::get<I - 1>(rhs)) ? 0 : 1;
}

template <int I, typename A, typename... Args>
typename std::enable_if<1 < I, int>::type
mock_compare_tuple(const std::tuple<A, Args...>& lhs, const std::tuple<Args...>& rhs) {
  auto res = mock_compare_tuple<I - 1>(lhs, rhs);

  if (res != 0)
    return res;

  return mock_compare_arg(std::get<I>(lhs), std::get<I - 1>(rhs)) ? 0 : I;
}

template<typename R, typename... Args>
struct mock_function_type {
  typedef mock_function_map<R, Args...> type;

  static int compare_expected(typename type::call_type lhs, Args... rhs) {
    return mock_compare_tuple<sizeof...(Args)>(lhs, std::make_tuple(rhs...));
  }

  static R ret_erase(void* fn) { return type::ret_erase(fn); }
};

template<typename... Args>
struct mock_function_type<void, Args...> {
  typedef mock_function_map<mock_void, Args...> type;

  static int compare_expected(typename type::call_type lhs, Args... rhs) {
    return mock_compare_tuple<sizeof...(Args)>(lhs, std::make_tuple(rhs...));
  }

  static void ret_erase(void* fn) { type::ret_erase(fn); }
};

void mock_init();
void mock_cleanup();

template<typename R, typename... Args>
bool
mock_cleanup_map(R fn[[gnu::unused]](Args...)) {
  return mock_function_type<R, Args...>::type::cleanup(reinterpret_cast<void*>(fn));
}

template<typename R, typename... Args>
void
mock_expect(R fn(Args...), R ret, Args... args) {
  typedef mock_function_map<R, Args...> mock_map;
  mock_map::functions[reinterpret_cast<void*>(fn)].push_back(std::tuple<R, Args...>(ret, args...));
}

template<typename... Args>
void
mock_expect(void fn(Args...), Args... args) {
  typedef mock_function_map<mock_void, Args...> mock_map;
  mock_map::functions[reinterpret_cast<void*>(fn)].push_back(std::tuple<mock_void, Args...>(mock_void(), args...));
}

template<typename R, typename... Args>
auto
mock_call(std::string name, R fn(Args...), Args... args) -> decltype(fn(args...)) {
  typedef mock_function_type<R, Args...> mock_type;
  auto itr = mock_type::type::functions.find(reinterpret_cast<void*>(fn));

  // Throwing here causes undefined behaviour as it crosses C language
  // barriers.
  CPPUNIT_ASSERT_MESSAGE(("mock_call expected function calls exhausted by '" + name + "'").c_str(),
                         itr != mock_type::type::functions.end());

  auto mismatch_arg = mock_type::compare_expected(itr->second.front(), args...);
  CPPUNIT_ASSERT_MESSAGE(("mock_call expected function call argument " + std::to_string(mismatch_arg) + " mismatch for '" + name + "'").c_str(),
                         mismatch_arg == 0);

  return mock_type::ret_erase(reinterpret_cast<void*>(fn));
}

#endif
