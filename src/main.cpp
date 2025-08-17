#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

// -------------------- Domain Model --------------------
enum class IFF { Friend, Foe, Unknown };

struct Contact {
    std::string id;           // Track ID or callsign
    IFF iff;                  // Friend/Foe/Unknown
    double range_km;          // Slant range (km)
    double closing_mps;       // Positive means approaching (m/s)
    double altitude_m;        // Altitude (m)
    double rcs_m2;            // Radar cross-section (m^2)
};

// -------------------- Utilities --------------------
static inline std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::optional<IFF> parseIFF(const std::string& token) {
    std::string t = token;
    for (auto& c : t) c = std::toupper(static_cast<unsigned char>(c));
    if (t == "FRIEND" || t == "F") return IFF::Friend;
    if (t == "FOE"    || t == "HOSTILE" || t == "H") return IFF::Foe;
    if (t == "UNKNOWN"|| t == "U") return IFF::Unknown;
    return std::nullopt;
}

static std::string iffToStr(IFF iff) {
    switch (iff) {
        case IFF::Friend:  return "FRIEND";
        case IFF::Foe:     return "FOE";
        case IFF::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

// Safe stod with default
static double toDouble(const std::string& s, double def = 0.0) {
    try { return std::stod(s); }
    catch (...) { return def; }
}

// -------------------- CSV Ingest --------------------
// CSV columns (header optional):
// id, iff(Friend|Foe|Unknown), range_km, closing_mps, altitude_m, rcs_m2
static std::vector<Contact> loadCSV(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open CSV: " + path);
    }

    std::vector<Contact> out;
    std::string line;
    bool maybeHeader = true;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (line[0] == '#') continue; // comment

        std::stringstream ss(line);
        std::string tok;
        std::vector<std::string> cols;
        while (std::getline(ss, tok, ',')) cols.push_back(trim(tok));

        if (maybeHeader) {
            // Detect header if non-numeric where numeric expected and skip it
            if (cols.size() >= 6) {
                auto iffParsed = parseIFF(cols[1]);
                bool looksHeader =
                    !iffParsed.has_value() || cols[2] == "range_km" || cols[3] == "closing_mps";
                if (looksHeader) { maybeHeader = false; continue; }
            }
            maybeHeader = false;
        }

        if (cols.size() < 6) {
            std::cerr << "Skipping malformed row: " << line << "\n";
            continue;
        }

        auto iff = parseIFF(cols[1]);
        if (!iff) {
            std::cerr << "Skipping row with invalid IFF: " << line << "\n";
            continue;
        }

        Contact c {
            cols[0],
            *iff,
            toDouble(cols[2], 1e9),   // range
            toDouble(cols[3], 0.0),   // closing speed
            toDouble(cols[4], 0.0),   // altitude
            toDouble(cols[5], 1.0)    // rcs
        };
        out.push_back(c);
    }

    return out;
}

// -------------------- Scoring --------------------
// Simple, explainable weighting function.
// Larger score => higher priority.
struct Weights {
    double w_range_inv   = 60.0;   // closer = higher risk
    double w_closing     = 0.25;   // approaching faster = higher risk
    double w_rcs         = 0.4;    // bigger target = higher risk (proxy for aircraft size)
    double w_iff_friend  = -40.0;  // strong penalty for friend
    double w_iff_unknown = 15.0;   // mild boost for unknown
    double w_iff_foe     = 30.0;   // strong boost for foe
    double w_alt_low     = 0.004;  // lower altitude slightly more concerning
};

// Normalize helpers (to keep scores bounded-ish)
static double clamp(double x, double lo, double hi) {
    return std::max(lo, std::min(hi, x));
}

static double score(const Contact& c, const Weights& w) {
    // 1/range term (avoid div-by-zero)
    double inv_range = (c.range_km > 0.05) ? (1.0 / c.range_km) : 20.0; // cap when very close
    double s_range   = w.w_range_inv * inv_range;

    // Closing speed: positive = approaching. Scale ~0..400 m/s
    double closing_norm = clamp(c.closing_mps / 400.0, 0.0, 1.0);
    double s_closing    = w.w_closing * (closing_norm * 100.0); // scale into ~0..25

    // RCS: log-scale to compress (0.01..100 m^2 -> -2..2)
    double rcs_log = std::log10(std::max(0.01, c.rcs_m2));
    double s_rcs   = w.w_rcs * ((rcs_log + 2.0) * 25.0); // map -2..2 -> 0..100-ish then weight

    // Altitude: slightly prefer lower altitude (easier/closer to impact ground)
    double alt_term = (20000.0 - clamp(c.altitude_m, 0.0, 20000.0)) / 200.0; // 0..100
    double s_alt    = w.w_alt_low * alt_term;

    // IFF
    double s_iff = 0.0;
    switch (c.iff) {
        case IFF::Friend:  s_iff = w.w_iff_friend;  break;
        case IFF::Unknown: s_iff = w.w_iff_unknown; break;
        case IFF::Foe:     s_iff = w.w_iff_foe;     break;
    }

    return s_range + s_closing + s_rcs + s_alt + s_iff;
}

// -------------------- Engagement Suggestion --------------------
static std::string suggestion(const Contact& c, double riskScore) {
    // Very naive thresholds—tune freely
    if (c.iff == IFF::Friend) return "IGNORE (FRIEND)";
    if (riskScore > 120.0 && c.range_km < 25.0 && c.closing_mps > 100.0) return "INTERCEPT";
    if (riskScore > 80.0 && c.range_km < 50.0) return "ELEVATED MONITOR";
    return "MONITOR";
}

// -------------------- Output --------------------
static void printTable(const std::vector<std::pair<Contact, double>>& ranked) {
    std::cout << std::left
              << std::setw(10) << "RANK"
              << std::setw(12) << "ID"
              << std::setw(10) << "IFF"
              << std::setw(12) << "RANGE(km)"
              << std::setw(14) << "CLOSING(m/s)"
              << std::setw(12) << "ALT(m)"
              << std::setw(10) << "RCS(m^2)"
              << std::setw(12) << "SCORE"
              << "SUGGESTION"
              << "\n";

    std::cout << std::string(10+12+10+12+14+12+10+12+11, '-') << "\n";

    int rank = 1;
    for (const auto& [c, s] : ranked) {
        std::cout << std::left
                  << std::setw(10) << rank++
                  << std::setw(12) << c.id
                  << std::setw(10) << iffToStr(c.iff)
                  << std::setw(12) << std::fixed << std::setprecision(1) << c.range_km
                  << std::setw(14) << std::fixed << std::setprecision(0) << c.closing_mps
                  << std::setw(12) << std::fixed << std::setprecision(0) << c.altitude_m
                  << std::setw(10) << std::fixed << std::setprecision(2) << c.rcs_m2
                  << std::setw(12) << std::fixed << std::setprecision(1) << s
                  << suggestion(c, s)
                  << "\n";
    }
}

// -------------------- Main --------------------
int main(int argc, char** argv) {
    try {
        std::string csvPath = (argc > 1) ? argv[1] : "data/contacts.csv";
        auto contacts = loadCSV(csvPath);

        if (contacts.empty()) {
            std::cerr << "No contacts loaded from " << csvPath << "\n";
            return 1;
        }

        Weights w{}; // tweak if you like
        std::vector<std::pair<Contact, double>> ranked;
        ranked.reserve(contacts.size());

        for (const auto& c : contacts) {
            ranked.emplace_back(c, score(c, w));
        }

        std::sort(ranked.begin(), ranked.end(),
                  [](const auto& a, const auto& b){ return a.second > b.second; });

        printTable(ranked);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 2;
    }
}

