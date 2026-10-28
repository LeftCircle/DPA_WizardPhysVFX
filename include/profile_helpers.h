#pragma once

#include <string>


// from effective modern cpp book
// auto time_func = [](std::string&& name, auto&& func, auto&&... params){
//     auto start_time = std::chrono::high_resolution_clock::now();
//     auto result = std::forward<decltype(func)>(func)(std::forward<decltype(params)>(params)...);
//     auto end_time = std::chrono::high_resolution_clock::now();
//     std::chrono::duration<double> elapsed = end_time - start_time;
//     std::cout << name << " took " << elapsed.count() << " seconds\n";
//     return result;
// };

auto time_func = [](std::string_view name, auto&& func, auto&&... args) {
    const auto start = std::chrono::high_resolution_clock::now();

    if constexpr (std::is_void_v<std::invoke_result_t<
                      decltype(func), decltype(args)...>>) {
        std::forward<decltype(func)>(func)(std::forward<decltype(args)>(args)...);

        const auto end = std::chrono::high_resolution_clock::now();
        std::cout << name << " took "
                  << std::chrono::duration<double>(end - start).count()
                  << " seconds\n";
    } else {
        auto result = std::forward<decltype(func)>(func)(std::forward<decltype(args)>(args)...);
        const auto end = std::chrono::high_resolution_clock::now();
        std::cout << name << " took "
                  << std::chrono::duration<double>(end - start).count()
                  << " seconds\n";
        return result;
    }
};


