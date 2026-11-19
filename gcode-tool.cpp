// gcode-tool: analyze a 3D printer G-code file.
//
// Reads a .gcode file and reports:
//   - filament used (mm and grams, PLA defaults)
//   - layer count and layer height
//   - estimated print time (from move distances and feedrates)
//   - print vs travel distance and the bounding box
//
// Usage: gcode-tool <file.gcode>
//
// Parsing notes:
//   - Comments (everything after ';') are stripped.
//   - G0/G1 moves are tracked in absolute (G90) or relative (G91) mode.
//   - Extrusion is tracked in absolute (M82, the default) or relative (M83)
//     mode, and G92 E0 resets are honoured.
//   - Retractions (E decreasing) are not counted as filament used; only
//     forward extrusion adds to the total.

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace {

// Filament defaults: standard 1.75 mm PLA.
constexpr double kFilamentDiameterMm = 1.75;
constexpr double kPlaDensityGPerCm3 = 1.24;
constexpr double kDefaultFeedrateMmMin = 1500.0;

// One parsed G-code word, e.g. X10.5 or F1800.
struct Word {
    char letter;
    double value;
    bool present = false;
};

// Split a line like "G1 X10.5 Y20 E0.8 F1800" into its words.
// Returns the command letter+number (e.g. "G1") and a map of parameter words.
bool parseLine(const std::string& raw, std::string& command,
               std::map<char, double>& params) {
    // Strip the comment.
    std::string line = raw.substr(0, raw.find(';'));
    std::istringstream in(line);
    std::string token;
    command.clear();
    params.clear();
    while (in >> token) {
        if (token.size() < 2) continue;
        char letter = token[0];
        try {
            double value = std::stod(token.substr(1));
            if (command.empty() && (letter == 'G' || letter == 'M')) {
                command = token;  // first G/M word is the command
            } else {
                params[letter] = value;
            }
        } catch (...) {
            // Not a number — ignore the token.
        }
    }
    return !command.empty();
}

double get(const std::map<char, double>& params, char letter, double fallback) {
    auto it = params.find(letter);
    return it == params.end() ? fallback : it->second;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file.gcode>\n";
        return 1;
    }
    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Cannot open " << argv[1] << "\n";
        return 1;
    }

    // --- Printer state -----------------------------------------------------
    double x = 0, y = 0, z = 0, e = 0;       // current position
    double feedrate = kDefaultFeedrateMmMin; // mm/min
    bool absolutePos = true;                 // G90 (true) vs G91 (false)
    bool absoluteExt = true;                 // M82 (true) vs M83 (false)

    // --- Totals ------------------------------------------------------------
    double filamentMm = 0;        // forward extrusion only
    double printDistMm = 0;       // distance moved while extruding
    double travelDistMm = 0;      // distance moved without extruding
    double timeSec = 0;           // feedrate-based time estimate
    long moveCount = 0, extrudeMoveCount = 0;
    std::set<int> layerZ;         // Z heights (in 0.01 mm steps) with extrusion
    double minX = 0, maxX = 0, minY = 0, maxY = 0, maxZ = 0;
    bool haveBounds = false;

    std::string raw;
    while (std::getline(file, raw)) {
        std::string cmd;
        std::map<char, double> p;
        if (!parseLine(raw, cmd, p)) continue;

        if (cmd == "G90") { absolutePos = true; continue; }
        if (cmd == "G91") { absolutePos = false; continue; }
        if (cmd == "M82") { absoluteExt = true; continue; }
        if (cmd == "M83") { absoluteExt = false; continue; }
        if (cmd == "G28") {
            // Homing: axes named (or all, if none) return to 0.
            if (p.empty()) { x = y = z = 0; }
            if (p.count('X')) x = 0;
            if (p.count('Y')) y = 0;
            if (p.count('Z')) z = 0;
            continue;
        }
        if (cmd == "G92") {
            // Set current position (commonly G92 E0 after a retraction).
            if (p.count('X')) x = p['X'];
            if (p.count('Y')) y = p['Y'];
            if (p.count('Z')) z = p['Z'];
            if (p.count('E')) e = p['E'];
            continue;
        }
        if (cmd != "G0" && cmd != "G1") continue;  // only moves matter

        double nx = absolutePos ? get(p, 'X', x) : x + get(p, 'X', 0);
        double ny = absolutePos ? get(p, 'Y', y) : y + get(p, 'Y', 0);
        double nz = absolutePos ? get(p, 'Z', z) : z + get(p, 'Z', 0);
        double ne = absoluteExt ? get(p, 'E', e) : e + get(p, 'E', 0);
        if (p.count('F')) feedrate = p['F'];

        double dx = nx - x, dy = ny - y, dz = nz - z;
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        double de = ne - e;

        if (dist > 0) {
            // Time = distance / speed. Feedrate is mm/min, so divide by 60.
            timeSec += dist / (feedrate / 60.0);
            moveCount++;
            if (de > 0) {
                printDistMm += dist;
                extrudeMoveCount++;
                layerZ.insert(static_cast<int>(std::lround(nz * 100)));
            } else {
                travelDistMm += dist;
            }
        }
        if (de > 0) filamentMm += de;  // retractions (de < 0) don't count

        // Bounding box over positions reached while extruding.
        if (de > 0 || dist == 0) {
            if (!haveBounds) {
                minX = maxX = nx; minY = maxY = ny; haveBounds = true;
            } else {
                minX = std::min(minX, nx); maxX = std::max(maxX, nx);
                minY = std::min(minY, ny); maxY = std::max(maxY, ny);
            }
        }
        maxZ = std::max(maxZ, nz);

        x = nx; y = ny; z = nz; e = ne;
    }

    // --- Report ------------------------------------------------------------
    double radius = kFilamentDiameterMm / 2.0;
    double volumeCm3 = filamentMm * 3.14159265 * radius * radius / 1000.0;
    double grams = volumeCm3 * kPlaDensityGPerCm3;

    int hours = static_cast<int>(timeSec) / 3600;
    int minutes = (static_cast<int>(timeSec) % 3600) / 60;
    int seconds = static_cast<int>(timeSec) % 60;

    double layerHeight = 0;
    if (layerZ.size() > 1) {
        auto it = layerZ.begin();
        int first = *it, prev = *it, sum = 0, n = 0;
        for (++it; it != layerZ.end(); ++it) { sum += (*it - prev); prev = *it; n++; }
        (void)first;
        layerHeight = n ? (sum / static_cast<double>(n)) / 100.0 : 0;
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "File:            " << argv[1] << "\n";
    std::cout << "Filament used:   " << filamentMm << " mm  (" << grams << " g PLA)\n";
    std::cout << "Layers:          " << layerZ.size();
    if (layerHeight > 0) std::cout << "  (avg layer height " << layerHeight << " mm)";
    std::cout << "\n";
    std::cout << "Print height:    " << maxZ << " mm\n";
    std::cout << "Est. print time: " << hours << "h " << minutes << "m " << seconds << "s\n";
    std::cout << "Print distance:  " << printDistMm << " mm (" << extrudeMoveCount << " extrusion moves)\n";
    std::cout << "Travel distance: " << travelDistMm << " mm\n";
    std::cout << "Total moves:     " << moveCount << "\n";
    if (haveBounds) {
        std::cout << "Bounding box:    X " << minX << " to " << maxX
                  << "  Y " << minY << " to " << maxY << " mm\n";
    }
    std::cout << "\nNote: time is a feedrate estimate — it ignores acceleration, "
              << "so real prints run longer.\n";
    return 0;
}
