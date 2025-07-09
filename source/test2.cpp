// #include <iostream>
// #include <messenger/crypto/keys.hpp>
// #include <messenger/crypto/oaep.hpp>
// #include <utility/log.hpp>
// #include <cassert>
// #include <vector>

// void print_hex(const std::vector<uint8_t>& v) {
//     for (auto b : v) printf("%02x", b);
//     printf("\n");
// }

// int main() {
//     // Тест 1: число меньше k
//     mpz_class num1 = 0x1234;
//     size_t k1 = 8;
//     auto bytes1 = crypto::oaep::mpz_to_bytes(num1, k1);
//     assert(bytes1.size() == k1);
//     // Ожидаем: 00 00 00 00 00 00 12 34
//     std::vector<uint8_t> expected1 = {0,0,0,0,0,0,0x12,0x34};
//     assert(bytes1 == expected1);
//     print_hex(bytes1);

//     // Тест 2: число ровно k
//     mpz_class num2 = 0x1122334455667788;
//     size_t k2 = 8;
//     auto bytes2 = crypto::oaep::mpz_to_bytes(num2, k2);
//     assert(bytes2.size() == k2);
//     std::vector<uint8_t> expected2 = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
//     assert(bytes2 == expected2);
//     print_hex(bytes2);

//     // Тест 3: число больше k (обрезка)
//     mpz_class num3 = 0x123456789ABC;
//     size_t k3 = 4;
//     auto bytes3 = crypto::oaep::mpz_to_bytes(num3, k3);
//     assert(bytes3.size() == k3);
//     // Ожидаем: только младшие 4 байта (78 9A BC)
//     std::vector<uint8_t> expected3 = {0x56,0x78,0x9A,0xBC};
//     assert(bytes3 == expected3);
//     print_hex(bytes3);

//     // Тест 4: ноль
//     mpz_class num4 = 0;
//     size_t k4 = 4;
//     auto bytes4 = crypto::oaep::mpz_to_bytes(num4, k4);
//     assert(bytes4.size() == k4);
//     std::vector<uint8_t> expected4 = {0,0,0,0};
//     assert(bytes4 == expected4);
//     print_hex(bytes4);

//     std::cout << "All mpz_to_bytes tests passed!" << std::endl;
//     return 0;
// }

int main() {}