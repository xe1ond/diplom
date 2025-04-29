#include <stdint.h>
#include <string.h>

#define MAX_DEVICES 10

// Структура для хранения информации о маршруте
typedef struct {
    uint8_t device_addr[8];
    uint8_t next_hop[8];
    uint8_t valid;  // Индикатор, что маршрут занят
} route_entry_t;

route_entry_t route_table[MAX_DEVICES];

// Функция для добавления маршрута
int add_route(uint8_t *device_addr, uint8_t *next_hop) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        // Ищем пустое место для нового маршрута
        if (route_table[i].valid == 0) {  // Если маршрут ещё не занят
            memcpy(route_table[i].device_addr, device_addr, 8);
            memcpy(route_table[i].next_hop, next_hop, 8);
            route_table[i].valid = 1;  // Устанавливаем флаг занятости
            return 0;  // Успешно добавлено
        }
    }
    return -1;  // Таблица маршрутов переполнена
}

// Функция для поиска маршрута
uint8_t *find_route(uint8_t *device_addr) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (route_table[i].valid == 1 && memcmp(route_table[i].device_addr, device_addr, 8) == 0) {
            return route_table[i].next_hop;
        }
    }
    return NULL;  // Если маршрут не найден
}
