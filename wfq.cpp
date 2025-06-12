#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <ostream>
#include <sstream>
#include <optional>
#include <vector>
#include <unordered_map>
#include <format>
#include <iomanip>

// Information about a packet: arrival time, connection, length, and weight.
//
// A packet's priority is defined as the weight divided by the length.
//
// PacketInfo also implements the comparison operators (==, !=, <, >, <=, >=),
// and they all compare by priority.
// This means, for example, that you can call std::max() on PacketInfo's,
// and it will return the packet with the higher priority.
class PacketInfo {
public:
    // The time when the packet arrived.
    uint64_t time;
    // The packet's connection (source IP, source port, destination IP, destination port).
    std::string connection;
    // The packet's length.
    uint64_t length;
    // The packet's weight.
    double weight;
    // This field is true if the packet was received with the weight written explicitly,
    // and thus should also be output with an explicit weight.
    bool should_print_with_weight;

    // Implementation of printing for PacketInfo.
    friend std::ostream &operator<<(std::ostream &os, const PacketInfo &self) {
        if (self.should_print_with_weight) {
            return os << std::format("{} {} {} {:.2f}", self.time, self.connection, self.length, self.weight);
        }
        else {
            return os << self.time << " " << self.connection << " " << self.length;
        }
    }

    // Gets the priority of the packet.
    // Higher-priority packets will be transmitted first.
    double priority() const {
        return weight / length;
    }

    // An implementation of comparison for PacketInfo.
    // A packet with a higher priority compares greater.
    std::partial_ordering operator<=>(const PacketInfo &other) const {
        return priority() <=> other.priority();
    }

    // An implementation of equality for packets.
    // Two packets are equal if they have the same priority.
    bool operator==(const PacketInfo &other) const {
        return (*this <=> other) == 0;
    }
};

// An object that reads packet from stdin.
// A PacketReader can read a single packet, or a sequence of packets.
class PacketReader {
    // A packet that has been read but not yet yielded by one of the read_* methods.
    std::optional<PacketInfo> next_packet;

    // The current weight of each connection.
    std::unordered_map<std::string, double> connection_weights;

    // Reads a PacketInfo from an input string.
    // Also assigns it a weight according to connection_weights, or updates connection_weight, as required.
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
            // If a weight was not explicitly given in the input line,
            // take the existing weight from connection_weights, or 1.
            auto iter = connection_weights.find(result.connection);
            result.weight = (iter == connection_weights.end() ? 1 : iter->second);
            result.should_print_with_weight = false;
        }
        else if (num_read == 7) {
            // If a weight was explicitly given in the input line,
            // update connection_weights accordingly.
            connection_weights.insert_or_assign(result.connection, result.weight);
            result.should_print_with_weight = true;
        }
        else {
            // If the input line did not contain exactly 6 or 7 parameters, that's an error.
            std::cerr << "bad input line: " << input_line << std::endl;
            std::abort();
        }
        return result;
    }

public:
    // Reads a batch of PacketInfo's from stdin.
    // A batch is defined as a sequence of packets with the same arrival time.
    // Does not read packets whose arrival time is greater than max_time.
    // Adds all the read PacketInfo's into output.
    // Returns the number of PacketInfo's read, which may be 0.
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

    // Reads a batch of PacketInfo's from stdin.
    // A batch is defined as a sequence of packets with the same arrival time.
    // Adds all the read PacketInfo's into output.
    // Returns the number of PacketInfo's read, which may be 0.
    size_t read_batch(std::vector<PacketInfo> &output) {
        return read_batch_with_timeout(std::numeric_limits<uint64_t>::max(), output);
    }

    // Reads a sequence of PacketInfo's from stdin.
    // Only reads PacketInfo's whose arrival time is no more than max_time.
    // Adds all the read PacketInfo's into output.
    // Returns the number of PacketInfo's read, which may be 0.
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
    // An object that reads PacketInfo's from stdin.
    PacketReader reader;
    // The time that has passed while transmitting packets or waiting.
    uint64_t virtual_time = 0;
    // Packets waiting to be transmitted.
    std::vector<PacketInfo> current_packets;
    // Repeat as long as there are packets to transmit.
    while (true) {
        // If there is nothing to transmit, wait for new packets.
        if (current_packets.empty()) {
            reader.read_batch(current_packets);
            if (current_packets.empty()) break;
            virtual_time = current_packets[0].time;
        }
        // Once there are packets to transmit, choose the one with the highest priority.
        // Transmit it, and check if new packets arrived in the meantime.
        std::vector<PacketInfo>::iterator to_send = std::max_element(current_packets.begin(), current_packets.end());
        std::cout << virtual_time << ": " << *to_send << std::endl;
        virtual_time += to_send->length;
        current_packets.erase(to_send);
        reader.read_with_timeout(virtual_time, current_packets);
    }
}
