#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <ostream>
#include <sstream>
#include <optional>
#include <vector>
#include <unordered_map>
#include <format>
#include <iomanip>

class PacketInfo {
public:
    uint64_t time;
    std::string connection;
    uint64_t length;
    double weight;
    bool should_print_with_weight;

    friend std::ostream &operator<<(std::ostream &os, const PacketInfo &self) {
        if (self.should_print_with_weight) {
            return os << std::format("{} {} {} {:.2f}", self.time, self.connection, self.length, self.weight);
        }
        else {
            return os << self.time << " " << self.connection << " " << self.length;
        }
    }

    double priority() const {
        return weight / length;
    }

    std::partial_ordering operator<=>(const PacketInfo &other) const {
        return priority() <=> other.priority();
    }

    bool operator==(const PacketInfo &other) const {
        return (*this <=> other) == 0;
    }
};

class PacketReader {
    std::optional<PacketInfo> next_packet;
    std::unordered_map<std::string, double> connection_weights;

public:
    PacketInfo parse_packet(const char *input_line) {
        PacketInfo result;
        char sadd[32], sport[32], dadd[32], dport[32];
        int num_read = sscanf(
            input_line,
            "%llu %31s %31s %31s %31s %llu %lf",
            &result.time, sadd, sport, dadd, dport, &result.length, &result.weight
        );
        result.connection = std::format("{} {} {} {}", sadd, sport, dadd, dport);
        if (num_read == 6) {
            auto iter = connection_weights.find(result.connection);
            result.weight = (iter == connection_weights.end() ? 1 : iter->second);
            result.should_print_with_weight = false;
        }
        else if (num_read == 7) {
            connection_weights.insert_or_assign(result.connection, result.weight);
            result.should_print_with_weight = true;
        }
        else {
            std::cerr << "bad input line: " << input_line << std::endl;
            std::abort();
        }
        return result;
    }

    size_t read_batch_with_timeout(uint64_t max_time, std::vector<PacketInfo> &output) {
        std::string line;
        size_t orig_size = output.size();
        while (true) {
            if (!next_packet.has_value()) {
                if (!std::getline(std::cin, line)) break;
                next_packet = parse_packet(line.c_str());
            }
            if (next_packet->time > max_time) break;
            max_time = std::min(max_time, next_packet->time);
            output.push_back(*next_packet);
            next_packet.reset();
        }
        return output.size() - orig_size;
    }

    size_t read_batch(std::vector<PacketInfo> &output) {
        return read_batch_with_timeout(std::numeric_limits<uint64_t>::max(), output);
    }

    size_t read_with_timeout(uint64_t max_time, std::vector<PacketInfo> &output) {
        size_t sum = 0;
        while (true) {
            size_t count = read_batch_with_timeout(max_time, output);
            if (count == 0) break;
            sum += count;
        }
        return sum;
    }
};

int main() {
    PacketReader reader;
    uint64_t virtual_time = 0;
    std::vector<PacketInfo> current_packets;
    while (true) {
        if (current_packets.empty()) {
            reader.read_batch(current_packets);
            if (current_packets.empty()) break;
            virtual_time = current_packets[0].time;
        }
        std::vector<PacketInfo>::iterator to_send = std::max_element(current_packets.begin(), current_packets.end());
        std::cout << virtual_time << ": " << *to_send << std::endl;
        virtual_time += to_send->length;
        current_packets.erase(to_send);
        reader.read_with_timeout(virtual_time, current_packets);
    }
}
