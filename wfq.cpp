#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <ostream>
#include <sstream>
#include <optional>
#include <vector>

class PacketInfo {
public:
    uint64_t time;
    std::string sadd;
    std::string sport;
    std::string dadd;
    std::string dport;
    uint64_t length;
    uint64_t weight;

    friend std::ostream &operator<<(std::ostream &os, const PacketInfo &self) {
        return os
            << "Packet Info: time = " << self.time
            << ", sadd = " << self.sadd
            << ", sport = " << self.sport
            << ", dadd = " << self.dadd
            << ", dport = " << self.dport
            << ", length = " << self.length
            << ", weight = " << self.weight;
    }

    // Suppose that packet 1 has length L1 and weight W1,
    // and packet 2 has length L2 and weight W2.
    // Then packet 1 has higher precedence iff W1/L1 > W2/L2.
    std::strong_ordering operator<=>(const PacketInfo &other) const {
        return weight * other.length <=> other.weight * length;
    }

    bool operator==(const PacketInfo &other) const {
        return (*this <=> other) == 0;
    }
};

class PacketReader {
    std::optional<PacketInfo> next_packet;

public:
    static PacketInfo parse_packet(const char *input_line) {
        PacketInfo result;
        char sadd_buff[32], sport_buff[32], dadd_buff[32], dport_buff[32];
        int num_read = sscanf(
            input_line,
            "%llu %31s %31s %31s %31s %llu %llu",
            &result.time, sadd_buff, sport_buff, dadd_buff, dport_buff,
            &result.length, &result.weight
        );
        switch (num_read) {
            case 6:
                result.weight = 1;
                break;
            case 7:
                break;
            default:
                std::cerr << "bad input line: " << input_line << std::endl;
                std::abort();
        }
        result.sadd = sadd_buff;
        result.sport = sport_buff;
        result.dadd = dadd_buff;
        result.dport = dport_buff;
        return result;
    }

    std::vector<PacketInfo> next_batch_with_timeout(uint64_t max_time) {
        std::vector<PacketInfo> result;
        std::string line;
        while (true) {
            if (!next_packet.has_value()) {
                if (!std::getline(std::cin, line)) break;
                next_packet = parse_packet(line.c_str());
            }
            if (next_packet->time > max_time) break;
            max_time = std::min(max_time, next_packet->time);
            result.push_back(*next_packet);
            next_packet.reset();
        }
        return result;
    }

    std::vector<PacketInfo> next_batch() {
        return next_batch_with_timeout(std::numeric_limits<uint64_t>::max());
    }
};

int main() {
    PacketReader reader;
    for (uint64_t i = 1;; i++) {
        std::vector<PacketInfo> batch = reader.next_batch();
        if (batch.empty()) break;
        std::cout << "Batch " << i << std::endl;
        for (auto &packet : batch) {
            std::cout << packet << std::endl;
        }
        std::cout << std::endl;
    }
}
