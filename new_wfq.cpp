#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <ostream>
#include <sstream>
#include <optional>
#include <vector>
#include <queue>
#include <compare>
#include <unordered_map>
#include <format>
#include <iomanip>

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

std::unordered_map<std::string, double>  channelsIndexMap;
std::unordered_map<std::string, ChannelInfo*>  channelsMap;
std::list<ChannelInfo> channels;
std::optional<PacketInfo> next_packet;

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
		if (next_packet->time > max_time) break;
		max_time = std::min(max_time, next_packet->time);
		// Find the channel for this packet, or create a new one if it doesn't exist.
		auto iter = channelsMap.find(next_packet->connection);
		if (iter == channelsMap.end()) {
			// If the channel does not exist, create a new one.
			ChannelInfo new_channel = ChannelInfo();
			auto index_iter = channelsIndexMap.find(next_packet->connection);
			if (index_iter != channelsIndexMap.end()) {
				new_channel.index = index_iter->second;
			}
			else {
				new_channel.index = channelsIndexMap.size(); // Assign a new index.
				channelsIndexMap[next_packet->connection] = new_channel.index;
			}
			new_channel.connection = next_packet->connection;
			if (next_packet->has_explicit_weight) {
				new_channel.weight = next_packet->weight;
			}
			else {
				new_channel.weight = 1.0; // Default weight if not specified.
			}
			new_channel.Q.push(*next_packet);
			channels.push_back(new_channel);
			channelsMap[next_packet->connection] = &channels.back();
		}
		else {
			// If the channel already exists, add the packet to its queue.
			iter->second->Q.push(*next_packet);
			// Update the weight if the packet has an explicit weight.
			if (next_packet->has_explicit_weight) {
				iter->second->weight = next_packet->weight;
			}
		}
		// Reset the next_packet so we can read the next one in the next iteration.
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

// The main function that processes the packets.
int main() {
	uint64_t time = 0;
	while (true) {
		if (channels.empty()) {
			if (read_batch() == 0) break;
			time = channels.front().Q.front().time;
		}
		// Find the channel with the earliest packet.
		channels.erase(std::remove_if(channels.begin(), channels.end(),
			[](const ChannelInfo& ch) { return ch.Q.empty(); }),
			channels.end());
		if (channels.empty()) break;
		auto earliest_channel_iter = std::min_element(channels.begin(), channels.end(),
			[](const ChannelInfo& a, const ChannelInfo& b) {
				// Compare by the finish time of the first packet in the queue.
				if (a.Q.front().length / a.weight != b.Q.front().length / b.weight) {
					return a.Q.front().length / a.weight < b.Q.front().length / b.weight;
				}
				else
				{
					return a.index < b.index; // If finish time is equal, compare by index to maintain a stable order.
				}
			});
		if (earliest_channel_iter == channels.end()) break; // No channels left to process.
		ChannelInfo& earliest_channel = *earliest_channel_iter;
		// Process the earliest packet in the queue of the earliest channel.
		PacketInfo p = earliest_channel.Q.front(); earliest_channel.Q.pop();
		std::cout << time << ": " << p << std::endl;
		// Update the virtual time.
		time += p.length;
		// Update the virtual time for the channel.
		if (earliest_channel.Q.empty()) {
			channelsMap.erase(earliest_channel.connection);
			channels.erase(earliest_channel_iter);
		}
		// Read new packets that arrived while the previous packet was being transmitted.
		read_with_timeout(time);
	}
}