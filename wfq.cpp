#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <sstream>
#include <optional>
#include <vector>
#include <queue>
#include <unordered_map>
#include <format>
#include <iomanip>
#include <limits>
#include <string>
#include <functional>

// Information about a packet: index, arrival time, connection, length, and weight.
//
// PacketInfo also implements the comparison operators (==, !=, <, >, <=, >=), and they all compare by priority.
// This means, for example, that you can call std::max() on PacketInfo's,
// and it will return the packet with the higher priority.
class PacketInfo {
public:
    // The time when the packet arrived.
    uint64_t time = 0;
    // The packet's connection (source IP, source port, destination IP, destination port).
    std::string connection = "";
    // The packet's length.
    uint64_t length = 0;
    // The packet's weight.
    double weight = 1.0;
    // This field is true if the packet was received with the weight written explicitly.
    bool has_explicit_weight = false;

    // Implementation of printing for PacketInfo.
    friend std::ostream& operator<<(std::ostream& os, const PacketInfo& self) {
        if (self.has_explicit_weight) {
            return os << std::format("{} {} {} {:.2f}", self.time, self.connection, self.length, self.weight);
        }
        else {
            return os << self.time << " " << self.connection << " " << self.length;
        }
    }
};

// An object that contains information about a channel.
// A channel is defined by its index, weight, connection, and a queue of packets that are waiting to be transmitted on this channel.
class ChannelInfo {
public:
    // The channel's index in the input.
    uint64_t index = 0;
    // The channel's weight.
    double weight = 1.0;
    // The channel's connection (source IP, source port, destination IP, destination port).
    std::string connection = "";
    // A queue of packets that are waiting to be transmitted on this channel.
    std::queue<PacketInfo> Q = {};
};

// A map that contains the index of each channel by its connection string.
std::unordered_map<std::string, uint64_t> channelsIndexMap;
// A vector of ChannelInfo objects, each representing a channel.
std::vector<ChannelInfo> channels;
// A queue of PacketInfo objects, each representing a packet that is waiting to be processed.
std::optional<PacketInfo> next_packet;

// A priority queue that contains active channels, sorted by their priority and index.
struct ActiveChannelEntry {
    uint64_t index;
    double priority_snapshot;

    bool operator<(const ActiveChannelEntry& other) const {
        if (priority_snapshot != other.priority_snapshot)
            return priority_snapshot > other.priority_snapshot;
        return index > other.index;
    }
};
std::priority_queue<ActiveChannelEntry> active_channels;

// Reads a PacketInfo from an input string.
// Also assigns it a weight according to connection_weights, or updates connection_weight, as required.
PacketInfo parse_packet(const char* input_line) {
    PacketInfo result;
    char sadd[32], sport[32], dadd[32], dport[32];
    int num_read = sscanf(
        input_line,
        "%llu %31s %31s %31s %31s %llu %lf",
        &result.time, sadd, sport, dadd, dport, &result.length, &result.weight
    );
    result.connection = std::format("{} {} {} {}", sadd, sport, dadd, dport);
    if (num_read == 6) {
        result.has_explicit_weight = false;
    }
    else if (num_read == 7) {
        result.has_explicit_weight = true;
    }
    else {
        // If the input line did not contain exactly 6 or 7 parameters, that's an error.
        std::cerr << "bad input line: " << input_line << std::endl;
        std::abort();
    }
    return result;
}

// Reads a batch of PacketInfo's from stdin.
// A batch is defined as a sequence of packets with the same arrival time.
// Does not read packets whose arrival time is greater than max_time.
// Adds all the PacketInfo's read into channels.
// Returns the number of PacketInfo's read, which may be 0.
size_t read_batch_with_timeout(uint64_t max_time) {
    std::string line;
    size_t num_read;
    for (num_read = 0;; num_read++) {
        if (!next_packet.has_value()) {
            if (!std::getline(std::cin, line)) break;
            next_packet = parse_packet(line.c_str());
        }
		// If the next packet's time is greater than max_time, stop reading.
        if (next_packet->time > max_time) break;
        max_time = std::min(max_time, next_packet->time);

        auto iter = channelsIndexMap.find(next_packet->connection);
        uint64_t idx;
        if (iter == channelsIndexMap.end()) {
			// If the channel is not in the map, add it.
            idx = channels.size();
            channelsIndexMap[next_packet->connection] = idx;
            ChannelInfo new_channel;
            new_channel.index = idx;
            new_channel.connection = next_packet->connection;
            new_channel.weight = next_packet->has_explicit_weight ? next_packet->weight : 1.0;
            channels.push_back(new_channel);
        }
        else {
            idx = iter->second;
            if (next_packet->has_explicit_weight) {
				// If the packet has an explicit weight, update the channel's weight.
                channels[idx].weight = next_packet->weight;
                if (!channels[idx].Q.empty()) {
                    double prio = static_cast<double>(channels[idx].Q.front().length) / channels[idx].weight;
                    active_channels.push({ idx, prio });
                }
            }
        }
        channels[idx].Q.push(*next_packet);
        if (channels[idx].Q.size() == 1) {
			// If this is the first packet in the channel, calculate its priority and add it to the active channels.
            double prio = static_cast<double>(channels[idx].Q.front().length) / channels[idx].weight;
            active_channels.push({ idx, prio });
        }
        next_packet.reset();
    }
    return num_read;
}

// Reads a batch of PacketInfo's from stdin.
// A batch is defined as a sequence of packets with the same arrival time.
// Adds all the PacketInfo's read into channels.
// Returns the number of PacketInfo's read, which may be 0.
size_t read_batch() {
    return read_batch_with_timeout(std::numeric_limits<uint64_t>::max());
}

// Reads a sequence of PacketInfo's from stdin.
// Only reads PacketInfo's whose arrival time is no more than max_time.
// Adds all the PacketInfo's read into channels.
// Returns the number of PacketInfo's read, which may be 0.
size_t read_with_timeout(uint64_t max_time) {
    size_t sum = 0;
    while (true) {
        size_t count = read_batch_with_timeout(max_time);
        if (count == 0) break;
        sum += count;
    }
    return sum;
}

// The main function that processes the input and outputs the results.
int main() {
    uint64_t time = 0;
    while (true) {
        if (active_channels.empty()) {
			// If there are no active channels, read a batch of packets.
			if (read_batch() == 0) break; // If no packets were read, exit the loop.

            for (size_t i = 0; i < channels.size(); ++i) {
                if (!channels[i].Q.empty()) {
					// If the channel has packets, calculate its priority and add it to the active channels.
                    double prio = static_cast<double>(channels[i].Q.front().length) / channels[i].weight;
                    active_channels.push({ i, prio });
                }
            }
            time = channels[active_channels.top().index].Q.front().time;
        }

        while (!active_channels.empty()) {
			// Process the channel with the highest priority.
            auto top = active_channels.top();
            const auto& ch = channels[top.index];
            if (ch.Q.empty()) {
                active_channels.pop();
                continue;
            }
			// If the channel's priority has changed, update it in the priority queue.
            double current_priority = static_cast<double>(ch.Q.front().length) / ch.weight;
            if (current_priority != top.priority_snapshot) {
                active_channels.pop();
                active_channels.push({ top.index, current_priority });
                continue;
            }

            active_channels.pop();
            PacketInfo p = channels[top.index].Q.front();
            channels[top.index].Q.pop();
            std::cout << time << ": " << p << std::endl;
            time += p.length;

            if (!channels[top.index].Q.empty()) {
				// If there are more packets in the channel, calculate the next priority and add it back to the active channels.
                double next_priority = static_cast<double>(channels[top.index].Q.front().length) / channels[top.index].weight;
                active_channels.push({ top.index, next_priority });
            }

            read_with_timeout(time);
            break;
        }
    }
}
