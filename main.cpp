#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using ProcessTimes = std::vector<int>;
using Weights = std::vector<int>;
using DueDates = std::vector<int>;
using SetupTimes = std::vector<std::vector<int>>;

using ProblemInstance = std::tuple<ProcessTimes, Weights, DueDates, SetupTimes>;

ProblemInstance readInstance(const std::string& filePath) {
    std::cout << "Reading instance from: " << filePath << std::endl;

    ProcessTimes pTimes;
    Weights weights;
    DueDates dueDates;
    SetupTimes setupTimes;

    std::ifstream data(filePath);
    std::string line;
    if (!data.is_open()) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return std::make_tuple(pTimes, weights, dueDates, setupTimes);
    }

    enum ParseState { HEADER,
                      PTIMES,
                      WEIGHTS,
                      DATES,
                      SETUPS };
    ParseState state = HEADER;

    int n = 0;
    int p_idx = 0, w_idx = 0, d_idx = 0;

    while (std::getline(data, line)) {
        if (line.empty() ||
            line.find("Begin Generator Parameters") != std::string::npos ||
            line.find("End Generator Parameters") != std::string::npos ||
            line.find("Begin Problem Specification") != std::string::npos ||
            line.find("Problem Instance:") != std::string::npos) {
            continue;
        }

        if (line.find("Problem Size:") != std::string::npos) {
            std::stringstream ss(line);
            std::string dummy;
            if (!(ss >> dummy >> dummy >> n) || n <= 0) {
                std::cerr << "Error: Could not read valid Problem Size from line: " << line << std::endl;
                return std::make_tuple(pTimes, weights, dueDates, setupTimes);
            }

            pTimes.resize(n);
            weights.resize(n);
            dueDates.resize(n);
            setupTimes.resize(n + 1, std::vector<int>(n, 0));

        } else if (line == "Process Times:") {
            state = PTIMES;
        } else if (line == "Weights:") {
            state = WEIGHTS;
        } else if (line == "Duedates:") {
            state = DATES;
        } else if (line == "Setup Times:") {
            state = SETUPS;
        } else {
            try {
                switch (state) {
                    case PTIMES:
                        if (p_idx < n) pTimes[p_idx++] = std::stoi(line);
                        break;
                    case WEIGHTS:
                        if (w_idx < n) weights[w_idx++] = std::stoi(line);
                        break;
                    case DATES:
                        if (d_idx < n) dueDates[d_idx++] = std::stoi(line);
                        break;
                    case SETUPS: {
                        std::stringstream ss(line);
                        int i, j, time;
                        if (ss >> i >> j >> time) {
                            if (i >= -1 && i < n && j >= 0 && j < n) {
                                setupTimes[i + 1][j] = time;
                            } else {
                                std::cerr << "Warning: Setup time index out of bounds: " << line << std::endl;
                            }
                        }
                    } break;
                    case HEADER:
                        break;
                }
            } catch (const std::invalid_argument& e) {
                std::cerr << "Warning: Skipping unparseable line: " << line << std::endl;
            } catch (const std::out_of_range& e) {
                std::cerr << "Warning: Skipping line with out-of-range number: " << line << std::endl;
            }
        }
    }

    if (p_idx != n || w_idx != n || d_idx != n) {
        std::cerr << "Warning: Data lists might be incomplete. Expected " << n
                  << " items, but got (p:" << p_idx << ", w:" << w_idx << ", d:" << d_idx << ")." << std::endl;
    }

    std::cout << "Finished reading instance." << std::endl;
    return std::make_tuple(pTimes, weights, dueDates, setupTimes);
}

int main() {
    std::string basePath = "/home/dam900/studia/drugi_stopien/ziwpp/data/scheduling-benchmarks/wtsds/";  // change according to directory
    std::string instanceName = "wt_sds_1.instance";                                                      // change according to directory

    std::string instancePath = basePath + instanceName;

    ProblemInstance instance = readInstance(instancePath);

    ProcessTimes p = std::get<0>(instance);
    Weights w = std::get<1>(instance);
    DueDates d = std::get<2>(instance);
    SetupTimes s = std::get<3>(instance);
    std::cout << "Process Times: ";
    for (const auto& pt : p) {
        std::cout << pt << " ";
    }

    return 0;
}