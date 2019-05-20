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
  typedef std::tuple<R, Args...> call_type;
  typedef std::vector<call_type> call_list_type;
  typedef std::map<void*, call_list_type> func_map_type;

  static func_map_type functions;

  mock_function_map() {
    // Add std::function to size and clear queue.
  }

  static void cleanup(void* fn) {
    auto itr = functions.find(fn);

    CPPUNIT_ASSERT_MESSAGE("mock_cleanup_map expected function calls not completed",
                           itr == functions.end());
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

struct mock_void {
  bool operator == (mock_void rhs) const { return true; }
};

template<typename R, typename... Args>
typename mock_function_map<R, Args...>::func_map_type mock_function_map<R, Args...>::functions;

template<typename R, typename... Args>
struct mock_function_type {
  typedef mock_function_map<R, Args...> type;

  static bool compare_expected(typename type::call_type lhs, Args... rhs) {
    return lhs == std::tuple_cat(std::tuple<R>(std::get<0>(lhs)), std::tuple<Args...>(rhs...));
  }

  static R ret_erase(void* fn) { return type::ret_erase(fn); }
};

template<typename... Args>
struct mock_function_type<void, Args...> {
  typedef mock_function_map<mock_void, Args...> type;

  static bool compare_expected(typename type::call_type lhs, Args... rhs) {
    return lhs == std::tuple_cat(std::make_tuple(mock_void()), std::tuple<Args...>(rhs...));
  }

  static void ret_erase(void* fn) { type::ret_erase(fn); }
};

template<typename R, typename... Args>
void
mock_init_map(R fn[[gnu::unused]](Args...)) {
  mock_function_type<R, Args...>::type::functions.clear();
}

template<typename R, typename... Args>
void
mock_cleanup_map(R fn[[gnu::unused]](Args...)) {
  mock_function_type<R, Args...>::type::cleanup(reinterpret_cast<void*>(fn));
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
mock_call(R fn(Args...), Args... args) -> decltype(fn(args...)) {
  typedef mock_function_type<R, Args...> mock_type;

  auto itr = mock_type::type::functions.find(reinterpret_cast<void*>(fn));

  CPPUNIT_ASSERT_MESSAGE("mock_call expected function calls exhausted",
                         itr != mock_type::type::functions.end());
  CPPUNIT_ASSERT_MESSAGE("mock_call expected function call arguments mismatch",
                         mock_type::compare_expected(itr->second.front(), args...));

  return mock_type::ret_erase(reinterpret_cast<void*>(fn));
}

#endif
