#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "EventID.h"

class EventBus {
public:
    template<typename T>
    void subscribe(std::function<void(const T&)> listener) {
        int id = EventID::get<T>();
        listeners_[id].push_back([listener](const void* e) {
            listener(*static_cast<const T*>(e));
        });
    }

    template<typename T>
    void emit(const T& event) {
        int id = EventID::get<T>();
        auto it = listeners_.find(id);
        if (it != listeners_.end()) {
            // Copy before iterating to guard against re-entrant subscribe/emit calls
            auto callbacks = it->second;
            for (auto& cb : callbacks)
                cb(&event);
        }
    }

private:
    std::unordered_map<int, std::vector<std::function<void(const void*)>>> listeners_;
};
