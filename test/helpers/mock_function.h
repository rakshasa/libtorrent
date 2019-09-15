#ifndef LIBTORRENT_HELPERS_MOCK_FUNCTION_H
#define LIBTORRENT_HELPERS_MOCK_FUNCTION_H

#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <cppunit/extensions/HelperMacros.h>

#include "helpers/mock_compare.h"

template<typename R, typename... Args>
struct mock_function_map {
  typedef std::tuple<R, Args...> call_type;
  typedef std::vector<call_type> call_list_type;
  typedef std::map<void*, call_list_type> func_map_type;

  typedef std::function<R (Args...)> function_type;
  typedef std::map<void*, function_type> redirect_map_type;

  static func_map_type functions;
  static redirect_map_type redirects;

  static bool cleanup(void* fn) {
    redirects.erase(fn);
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
template<typename R, typename... Args>
typename mock_function_map<R, Args...>::redirect_map_type mock_function_map<R, Args...>::redirects;

struct mock_void {};

template<typename R, typename... Args>
struct mock_function_type {
  typedef mock_function_map<R, Args...> type;

  static int compare_expected(typename type::call_type lhs, Args... rhs) {
    return mock_compare_tuple<sizeof...(Args)>(lhs, std::make_tuple(rhs...));
  }

  static R    ret_erase(void* fn) { return type::ret_erase(fn); }
  static bool has_redirect(void* fn) { return type::redirects.find(fn) != type::redirects.end(); }
  static R    call_redirect(void* fn, Args... args) { return type::redirects.find(fn)->second(args...); }
};

template<typename... Args>
struct mock_function_type<void, Args...> {
  typedef mock_function_map<mock_void, Args...> type;

  static int compare_expected(typename type::call_type lhs, Args... rhs) {
    return mock_compare_tuple<sizeof...(Args)>(lhs, std::make_tuple(rhs...));
  }

  static void ret_erase(void* fn) { type::ret_erase(fn); }
  static bool has_redirect(void* fn) { return type::redirects.find(fn) != type::redirects.end(); }
  static void call_redirect(void* fn, Args... args) { type::redirects.find(fn)->second(args...); }
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
void
mock_redirect(R fn(Args...), std::function<R (Args...)> func) {
  typedef mock_function_map<R, Args...> mock_map;
  mock_map::redirects[reinterpret_cast<void*>(fn)] = func;
}

template<typename R, typename... Args>
auto
mock_call_direct(std::string name, R fn(Args...), Args... args) -> decltype(fn(args...)) {
  typedef mock_function_type<R, Args...> mock_type;

  auto itr = mock_type::type::functions.find(reinterpret_cast<void*>(fn));
  CPPUNIT_ASSERT_MESSAGE(("mock_call expected function calls exhausted by '" + name + "'").c_str(),
                         itr != mock_type::type::functions.end());

  auto mismatch_arg = mock_type::compare_expected(itr->second.front(), args...);
  CPPUNIT_ASSERT_MESSAGE(("mock_call expected function call argument " + std::to_string(mismatch_arg) + " mismatch for '" + name + "'").c_str(),
                         mismatch_arg == 0);

  return mock_type::ret_erase(reinterpret_cast<void*>(fn));
}

template<typename R, typename... Args>
auto
mock_call(std::string name, R fn(Args...), Args... args) -> decltype(fn(args...)) {
  typedef mock_function_type<R, Args...> mock_type;

  if (mock_type::has_redirect(reinterpret_cast<void*>(fn)))
    return mock_type::call_redirect(reinterpret_cast<void*>(fn), args...);

  return mock_call_direct(name, fn, args...);
}

#endif
