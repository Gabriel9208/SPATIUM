#pragma once

class EventID {
public:
    template<typename T>
    static int get() {
        static int id = counter++;
        return id;
    }

private:
    static inline int counter = 0;
};
