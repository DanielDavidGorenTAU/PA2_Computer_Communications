#define _CRT_SECURE_NO_WARNINGS

#include "iostream"
#include "ostream"

class PacketInfo {
public:
    uint64_t time;
    std::string sadd;
    std::string sport;
    std::string dadd;
    std::string dport;
    uint64_t length;
    uint64_t weight;

    PacketInfo(const char *input_line) {
        char sadd_buff[32], sport_buff[32], dadd_buff[32], dport_buff[32];
        int num_read = sscanf(
            input_line,
            "%llu %31s %31s %31s %31s %llu %llu",
            &time,
            sadd_buff,
            sport_buff,
            dadd_buff,
            dport_buff,
            &length,
            &weight
        );
        switch (num_read) {
            case 6:
                weight = 1;
                break;
            case 7:
                break;
            default:
                std::cerr << "bad input line: " << input_line << std::endl;
                std::cerr << num_read << std::endl;
                //std::cerr << sadd_buff << std::endl;
                std::abort();
        }
        sadd = sadd_buff;
        sport = sport_buff;
        dadd = dadd_buff;
        dport = dport_buff;
    }

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
};

int main() {
    PacketInfo pi("0 70.246.64.70 14770 4.71.70.4 11970 70");
    std::cout << pi << std::endl;
}
