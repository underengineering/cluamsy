#include "completions.hpp"

#include <array>

static const std::array<std::string_view, 11> IP_FIELDS = {
    "HdrLength", "Version",  "TOS",      "Length",  "Id",      "FragOff0",
    "TTL",       "Protocol", "Checksum", "SrcAddr", "DstAddr",
};

static const std::array<std::string_view, 10> IPV6_FIELDS = {
    "TrafficClass0", "Version", "FlowLabel0", "TrafficClass1", "FlowLabel1",
    "Length",        "NextHdr", "HopLimit",   "SrcAddr",       "DstAddr",
};

static const std::array<std::string_view, 4> ICMP_FIELDS = {
    "Type",
    "Code",
    "Checksum",
    "Body",
};

static const std::array<std::string_view, 16> TCP_FIELDS = {
    "SrcPort",   "DstPort", "SeqNum",   "AckNum", "Reserved1", "HdrLength",
    "Fin",       "Syn",     "Rst",      "Psh",    "Ack",       "Urg",
    "Reserved2", "Window",  "Checksum", "UrgPtr",
};

static const std::array<std::string_view, 4> UDP_FIELDS = {
    "SrcPort",
    "DstPort",
    "Length",
    "Checksum",
};

template <size_t N>
constexpr std::vector<Completion>
get_fields_for(std::string_view field, std::array<std::string_view, N> fields) {
    std::vector<Completion> completions;
    completions.reserve(fields.size());
    for (const auto& field1 : fields) {
        if (field1.starts_with(field))
            completions.emplace_back(field1);
    }

    return completions;
}

std::optional<std::vector<Completion>> get_fields_for(std::string_view header,
                                                      std::string_view field) {
    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RETIF(header_name, fields_var)                                         \
    if (header == header_name)                                                 \
    return get_fields_for(field, fields_var)

    RETIF("ip", IP_FIELDS);
    RETIF("ipv6", IPV6_FIELDS);
    RETIF("icmp", ICMP_FIELDS);
    RETIF("tcp", TCP_FIELDS);
    RETIF("udp", UDP_FIELDS);

    return std::nullopt;
#undef RETIF
}

std::optional<std::vector<Completion>>
get_filter_completions(std::string_view filter) {
    auto header_idx = filter.find_last_of(' ');
    if (header_idx == std::string_view::npos)
        header_idx = 0;

    const auto header_end_idx = filter.find_last_of('.');
    if (header_end_idx == std::string_view::npos)
        return std::nullopt;

    const auto header = filter.substr(header_idx, header_end_idx - header_idx);

    const auto field_idx = header_end_idx + 1;
    const auto field = filter.substr(field_idx);

    const auto available_fields = get_fields_for(header, field);
    if (!available_fields)
        return std::nullopt;

    return available_fields;
}