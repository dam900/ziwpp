#ifndef SOLVER_H
#define SOLVER_H

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace solver {

using ProcessTimes = std::vector<int>;
using Weights = std::vector<int>;
using DueDates = std::vector<int>;
using SetupTimes = std::vector<std::vector<int>>;

using ProblemInstance = std::tuple<ProcessTimes, Weights, DueDates, SetupTimes>;

struct Solution {
  std::vector<int> schedule;
  long long objectiveValue = std::numeric_limits<long long>::max();
};

long long calculateTWT(const std::vector<int> &schedule, const ProcessTimes &p,
                       const Weights &w, const DueDates &d,
                       const SetupTimes &s) {
  long long totalWeightedTardiness = 0;
  long long completionTime = 0;

  int prevJobId = -1;

  for (const int &jobId : schedule) {
    int setup = s[prevJobId + 1][jobId];

    completionTime += setup + p[jobId];
    long long tardiness = std::max(0LL, completionTime - d[jobId]);
    totalWeightedTardiness += (long long)w[jobId] * tardiness;
    prevJobId = jobId;
  }

  return totalWeightedTardiness;
}

std::vector<int> generateInitialSolution(int n, const DueDates &d) {
  std::vector<int> schedule(n);
  std::iota(schedule.begin(), schedule.end(), 0);

  std::sort(schedule.begin(), schedule.end(),
            [&](int a, int b) { return d[a] < d[b]; });

  return schedule;
}

Solution simulatedAnnealing(const ProblemInstance &instance, int maxIterations,
                            double initialTemp, double coolingRate,
                            std::function<void(Solution)> callback = nullptr) {
  ProcessTimes p = std::get<0>(instance);
  Weights w = std::get<1>(instance);
  DueDates d = std::get<2>(instance);
  SetupTimes s = std::get<3>(instance);

  int n = p.size();
  if (n == 0)
    return Solution();

  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> index_dist(0, n - 1);
  std::uniform_real_distribution<double> rand_01(0.0, 1.0);

  Solution currentSolution;
  currentSolution.schedule = generateInitialSolution(n, d);
  currentSolution.objectiveValue =
      calculateTWT(currentSolution.schedule, p, w, d, s);

  Solution bestSolution = currentSolution;

  std::cout << "Initial TWT (EDD): " << bestSolution.objectiveValue
            << std::endl;

  double currentTemp = initialTemp;

  for (int i = 0; i < maxIterations; ++i) {
    Solution neighborSolution = currentSolution;

    int idx1 = index_dist(rng);
    int idx2 = index_dist(rng);
    if (n > 1) {
      while (idx1 == idx2) {
        idx2 = index_dist(rng);
      }
    }

    std::swap(neighborSolution.schedule[idx1], neighborSolution.schedule[idx2]);

    neighborSolution.objectiveValue =
        calculateTWT(neighborSolution.schedule, p, w, d, s);

    long long deltaCost =
        neighborSolution.objectiveValue - currentSolution.objectiveValue;

    if (deltaCost < 0) {
      currentSolution = neighborSolution;
    } else {
      double acceptanceProb = std::exp(-(double)deltaCost / currentTemp);
      if (rand_01(rng) < acceptanceProb) {
        currentSolution = neighborSolution;
      }
    }

    if (currentSolution.objectiveValue < bestSolution.objectiveValue) {
      bestSolution = currentSolution;
      if (callback) {
        callback(bestSolution);
      }
    }
  }

  currentTemp *= coolingRate;

  std::cout << "Final TWT (SA): " << bestSolution.objectiveValue << std::endl;

  return bestSolution;
}

Solution TWT_Insertion_Heuristic(const ProblemInstance &instance) {
  ProcessTimes p = std::get<0>(instance);
  Weights w = std::get<1>(instance);
  DueDates d = std::get<2>(instance);
  SetupTimes s = std::get<3>(instance);

  int n = p.size();
  if (n == 0)
    return Solution();

  std::vector<int> job_indices(n);
  std::iota(job_indices.begin(), job_indices.end(), 0);

  std::sort(job_indices.begin(), job_indices.end(), [&](int a, int b) {
    if (w[b] == 0 && w[a] != 0)
      return true;
    if (w[a] == 0 && w[b] != 0)
      return false;
    if (w[a] == 0 && w[b] == 0)
      return a < b;

    return (double)p[a] / w[a] < (double)p[b] / w[b];
  });

  std::vector<int> currentSchedule;

  currentSchedule.push_back(job_indices[0]);

  for (int k = 1; k < n; ++k) {
    int jobToInsert = job_indices[k];

    long long bestTWT = std::numeric_limits<long long>::max();
    int bestPosition = -1;

    for (int j = 0; j <= currentSchedule.size(); ++j) {
      std::vector<int> tempSchedule = currentSchedule;

      tempSchedule.insert(tempSchedule.begin() + j, jobToInsert);

      long long tempTWT = calculateTWT(tempSchedule, p, w, d, s);

      if (tempTWT < bestTWT) {
        bestTWT = tempTWT;
        bestPosition = j;
      }
    }

    currentSchedule.insert(currentSchedule.begin() + bestPosition, jobToInsert);
  }

  Solution result;
  result.schedule = currentSchedule;
  result.objectiveValue = calculateTWT(result.schedule, p, w, d, s);

  std::cout << "Final TWT (TWT-Insertion): " << result.objectiveValue
            << std::endl;

  return result;
}

ProblemInstance readInstance(const std::string &filePath) {
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

  enum ParseState { HEADER, PTIMES, WEIGHTS, DATES, SETUPS };
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
        std::cerr << "Error: Could not read valid Problem Size from line: "
                  << line << std::endl;
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
          if (p_idx < n)
            pTimes[p_idx++] = std::stoi(line);
          break;
        case WEIGHTS:
          if (w_idx < n)
            weights[w_idx++] = std::stoi(line);
          break;
        case DATES:
          if (d_idx < n)
            dueDates[d_idx++] = std::stoi(line);
          break;
        case SETUPS: {
          std::stringstream ss(line);
          int i, j, time;
          if (ss >> i >> j >> time) {
            if (i >= -1 && i < n && j >= 0 && j < n) {
              setupTimes[i + 1][j] = time;
            } else {
              std::cerr << "Warning: Setup time index out of bounds: " << line
                        << std::endl;
            }
          }
        } break;
        case HEADER:
          break;
        }
      } catch (const std::invalid_argument &e) {
        std::cerr << "Warning: Skipping unparseable line: " << line
                  << std::endl;
      } catch (const std::out_of_range &e) {
        std::cerr << "Warning: Skipping line with out-of-range number: " << line
                  << std::endl;
      }
    }
  }

  if (p_idx != n || w_idx != n || d_idx != n) {
    std::cerr << "Warning: Data lists might be incomplete. Expected " << n
              << " items, but got (p:" << p_idx << ", w:" << w_idx
              << ", d:" << d_idx << ")." << std::endl;
  }

  std::cout << "Finished reading instance." << std::endl;
  return std::make_tuple(pTimes, weights, dueDates, setupTimes);
}

ProblemInstance readInstance(std::istream &data) {

  ProcessTimes pTimes;
  Weights weights;
  DueDates dueDates;
  SetupTimes setupTimes;

  std::string line;
  if (!data.good()) {
    return std::make_tuple(pTimes, weights, dueDates, setupTimes);
  }

  enum ParseState { HEADER, PTIMES, WEIGHTS, DATES, SETUPS };
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
        std::cerr << "Error: Could not read valid Problem Size from line: "
                  << line << std::endl;
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
          if (p_idx < n)
            pTimes[p_idx++] = std::stoi(line);
          break;
        case WEIGHTS:
          if (w_idx < n)
            weights[w_idx++] = std::stoi(line);
          break;
        case DATES:
          if (d_idx < n)
            dueDates[d_idx++] = std::stoi(line);
          break;
        case SETUPS: {
          std::stringstream ss(line);
          int i, j, time;
          if (ss >> i >> j >> time) {
            if (i >= -1 && i < n && j >= 0 && j < n) {
              setupTimes[i + 1][j] = time;
            } else {
              std::cerr << "Warning: Setup time index out of bounds: " << line
                        << std::endl;
            }
          }
        } break;
        case HEADER:
          break;
        }
      } catch (const std::invalid_argument &e) {
        std::cerr << "Warning: Skipping unparseable line: " << line
                  << std::endl;
      } catch (const std::out_of_range &e) {
        std::cerr << "Warning: Skipping line with out-of-range number: " << line
                  << std::endl;
      }
    }
  }

  if (p_idx != n || w_idx != n || d_idx != n) {
    std::cerr << "Warning: Data lists might be incomplete. Expected " << n
              << " items, but got (p:" << p_idx << ", w:" << w_idx
              << ", d:" << d_idx << ")." << std::endl;
  }

  std::cout << "Finished reading instance." << std::endl;
  return std::make_tuple(pTimes, weights, dueDates, setupTimes);
}

} // namespace solver

#endif