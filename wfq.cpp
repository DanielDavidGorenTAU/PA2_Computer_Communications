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
#include <cassert>

// A program that implements a Weighted Fair Queueing (WFQ) algorithm for packet scheduling.

// Virtual time, which is used to calculate the priority of channels.
double virtual_time = 0;

// Information about a packet: arrival time, connection, length, and weight.
class PacketInfo {
public:
    // The time when the packet arrived.
    uint64_t time = 0;
    // The packet's connection (source IP, source port, destination IP, destination port).
    std::string connection = "";
    // The packet's length.
    uint64_t length = 0;
    // The packet's weight, if written explicitly.
    std::optional<double> weight;

    // Implementation of printing for PacketInfo.
    friend std::ostream& operator<<(std::ostream& os, const PacketInfo& self) {
        if (self.weight.has_value()) {
            return os << std::format("{} {} {} {:.2f}", self.time, self.connection, self.length, *self.weight);
        }
        else {
            return os << self.time << " " << self.connection << " " << self.length;
        }
    }

    // Reads a PacketInfo from a string.
    static PacketInfo parse(const char* input_line) {
        PacketInfo result;
        char sadd[32], sport[32], dadd[32], dport[32];
        double weight;
        int num_read = sscanf(
            input_line,
            "%llu %31s %31s %31s %31s %llu %lf",
            &result.time, sadd, sport, dadd, dport, &result.length, &weight
        );
        result.connection = std::format("{} {} {} {}", sadd, sport, dadd, dport);
        if (num_read == 6) {
            result.weight = std::nullopt;
        }
        else if (num_read == 7) {
            result.weight = weight;
        }
        else {
            // If the input line did not contain exactly 6 or 7 parameters, that's an error.
            std::cerr << "bad input line: " << input_line << std::endl;
            std::abort();
        }
        return result;
    }
};

// An object that contains information about a channel.
// A channel is defined by its index, weight, and a queue of packets that are waiting to be transmitted on this channel.
class ChannelInfo {
public:
    // The channel's index in the input.
    uint64_t index = 0;
    // The channel's weight.
    double weight = 1.0;
	// Last finish time of the channel.
    double last_finish_time = 0;
	// A flag that indicates whether the channel is currently active (has packets ready to send).
    bool is_active = false;
    // A queue of packets that are waiting to be transmitted on this channel.
    std::queue<PacketInfo> Q = {};
};

// A map that maps connection strings (source ip, source port, destination ip, destination port) to channels.
std::unordered_map<std::string, ChannelInfo> channelMap;
// A small buffer for a packet that has been read from stdin but not yet added to a channel.
std::optional<PacketInfo> next_packet;

// Get a channel from channelMap, or create it if it doesnt exist yet.
ChannelInfo &get_or_create_channel(const std::string& connection) {
    auto iter = channelMap.find(connection);
    // If the channel already exists, return it.
    if (iter != channelMap.end()) return iter->second;
    // If the channel is not in the map, add it.
    ChannelInfo new_channel{.index = channelMap.size()};
    auto iter_and_bool = channelMap.insert({ connection, new_channel });
    return iter_and_bool.first->second;
}

// A priority queue that contains active channels, sorted by their priority and index.
struct ActiveChannelEntry {
    // The channel.
    // Note: this pointer points directly into channelMap.
    // This is OK because unordered_map entries are guaranteed to have a stable address in memory.
    ChannelInfo *channel;
    // The channel's priority when it was added to the priority queue.
    double priority_snapshot;

    // Compares the priority of two channels.
    bool operator<(const ActiveChannelEntry& other) const {
        if (priority_snapshot != other.priority_snapshot)
            return priority_snapshot > other.priority_snapshot;
        return channel->index > other.channel->index;
    }
};
// Channels that have packets ready to send, ordered by priority.
std::priority_queue<ActiveChannelEntry> active_channels;

// Add a new channel to active_channels.
void mark_channel_active(ChannelInfo& channel) {
    assert(!channel.Q.empty());
    const PacketInfo& packet = channel.Q.front();

    // Compute start time for this packet
    double start_time = std::max(virtual_time, channel.last_finish_time);
    // Compute virtual finish time based on weight
    double finish_time = start_time + static_cast<double>(packet.length) / channel.weight;

    // Save for the next packet from this channel
    channel.last_finish_time = finish_time;
    channel.is_active = true;
    // Insert into priority queue with finish time as the priority
    active_channels.push({ &channel, finish_time });
}


// Reads a batch of PacketInfo's from stdin.
// A batch is defined as a sequence of packets with the same arrival time.
// Does not read packets whose arrival time is greater than max_time.
// Adds all the PacketInfo's read into the appropriate channels (and creates new channels if necessary).
// Returns the number of PacketInfo's read, which may be 0.
size_t read_batch_with_timeout(uint64_t max_time) {
    std::string line;
    size_t num_read;
    for (num_read = 0;; num_read++) {
        if (!next_packet.has_value()) {
            // If no packet has already been read, read a packet from stdin.
            if (!std::getline(std::cin, line)) break;
            next_packet = PacketInfo::parse(line.c_str());
        }
		// If the next packet's time is greater than max_time, stop reading.
        if (next_packet->time > max_time) break;
        // Don't read further packets if their arrival time is greater than the current packet's.
        max_time = next_packet->time;

        // Get or create a channel, and add the new packet to it.
        ChannelInfo &channel = get_or_create_channel(next_packet->connection);
        channel.Q.push(*next_packet);
        if (next_packet->weight.has_value()) {
            // If the packet has an explicit weight, update the channel's weight.
            channel.weight = *next_packet->weight;
        }
        if (channel.Q.size() == 1) {
			mark_channel_active(channel);
        }
        next_packet.reset();
    }
    return num_read;
}

// Reads a batch of PacketInfo's from stdin.
// A batch is defined as a sequence of packets with the same arrival time.
// Adds all the PacketInfo's read into the appropriate channels (and creates new channels if necessary).
// Returns the number of PacketInfo's read, which may be 0.
size_t read_batch() {
    return read_batch_with_timeout(std::numeric_limits<uint64_t>::max());
}

// Reads a sequence of PacketInfo's from stdin.
// Does not read packets whose arrival time is greater than max_time.
// Adds all the PacketInfo's read into the appropriate channels (and creates new channels if necessary).
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
            time = active_channels.top().channel->Q.front().time;
        }
        // Process the channel with the highest priority.
        virtual_time = std::max(virtual_time, active_channels.top().priority_snapshot);
        auto* channel = active_channels.top().channel;
        active_channels.pop();
        channel->is_active = false;
        auto& ch = *channel;
        PacketInfo p = ch.Q.front();
        ch.Q.pop();

        std::cout << time << ": " << p << std::endl;
        time += p.length;

        if (!ch.Q.empty()) {
            mark_channel_active(ch);
        }
        
        // Check if, while sending this packet, new packets have arrived.
        read_with_timeout(time);
    }
}
